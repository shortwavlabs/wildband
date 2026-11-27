// WILDBAND P0 standalone app: audio callback, MIDI, placeholder performance UI and patch bay.
// The UI is deliberately plain (docs/UI-AND-BLENDER-DIRECTION.md: interaction first). `--measure=FILE` runs the
// CoreAudio timing sweep used for docs/p0/EVIDENCE-LOG.md and quits.
#include <juce_audio_utils/juce_audio_utils.h>

#include "Engine.h"
#include "MicPermission.h"
#include "Radio.h"

#if WILDBAND_HAS_KNOB
 #include <BinaryData.h>
#endif

namespace
{
using namespace wildband;

const juce::Colour kOrange (0xffF59E0B), kRadio (0xff38BDF8), kRoom (0xff34D399), kLocal (0xff9CA3AF), kPanel (0xff1C1917), kBack (0xff0C0A09), kText (0xffE7E5E4);
juce::Colour moduleColour (int module) { return module == kModRadio ? kRadio : module == kModRoom ? kRoom : module == kModLocal ? kLocal : kOrange; }

struct Station { juce::String name, url; };
// Starting points only: public stream addresses of independent broadcasters. Nothing of theirs is bundled, and a
// public stream is not a licence to rebroadcast (docs/p0/RADIO-COMPATIBILITY.md).
const Station kDefaultStations[] = {
    { "SomaFM Groove Salad", "https://ice2.somafm.com/groovesalad-128-mp3" },
    { "KEXP Seattle", "https://kexp.streamguys1.com/kexp160.aac" },
    { "WFMU Freeform", "http://stream0.wfmu.org/freeform-128k" },
    { "BBC World Service", "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service" },
    { "Radio Paradise", "https://stream.radioparadise.com/aac-128" },
    { "SomaFM SF 10-33", "https://ice2.somafm.com/sf1033-128-mp3" } };
constexpr int kNumStations = (int) std::size (kDefaultStations);

// Default controller numbers: an UNVERIFIED guess at a Launch Control XL factory template's top knob row.
// MIDI learn is the supported way to map a controller; nothing depends on these.
constexpr int kDefaultCc[kNumParams] = { 13, 14, 15, 16, 17, 18, 19, 20, 77 };

// ---------------------------------------------------------------------------------------------------------------------
/** Everything the audio and MIDI threads touch. The UI talks to it through atomics only. */
class Instrument final : public juce::AudioIODeviceCallback, public juce::MidiInputCallback
{
public:
    Instrument()
    {
        for (int i = 0; i < kNumParams; ++i) params[(size_t) i] = paramDefaults[i];
        params[kTune] = dialCentre (0, kNumStations);
        for (auto& c : ccMap) c = -1;
        for (int i = 0; i < kNumParams; ++i) ccMap[(size_t) kDefaultCc[i]] = (int8_t) i;
        plan = RoutingPlan::fromGraph (Graph::defaultPatch()).pack();
    }

    // ---- shared state
    std::array<std::atomic<float>, kNumParams> params;
    std::atomic<int> mode { 0 };
    std::atomic<uint64_t> plan { 0 };                       // RoutingPlan::pack, written by the patch bay
    std::atomic<bool> roomEnabled { false }, panic { false }, stallOnce { false };   // stallOnce: --measure positive control only
    std::array<std::atomic<int8_t>, 128> ccMap;             // controller number -> Param, -1 = unmapped
    std::atomic<int> learnTarget { -1 }, midiEvents { 0 };
    std::atomic<bool> mapDirty { false };
    std::atomic<float> roomLevel { 0 }, roomActivity { 0 }, roomBrightness { 0 };
    std::atomic<double> sampleRate { 0 };
    std::atomic<int> blockSize { 0 }, inputChannels { 0 };

    Radio radio;
    Engine engine;

    // ---- callback timing (the instrument itself; host-time gaps are the dropout evidence, as AFTERIMAGE found
    // JUCE's xrun counter does not move on this macOS)
    struct Perf
    {
        static constexpr int kBins = 400;                   // 5 us each: 0 .. 2 ms, last bin = everything above
        std::array<std::atomic<uint32_t>, kBins> histogram;
        std::atomic<uint64_t> callbacks { 0 }, lateCallbacks { 0 };
        std::atomic<double> maxUs { 0 }, sumUs { 0 }, maxGapMs { 0 };
        void reset() { for (auto& b : histogram) b = 0; callbacks = 0; lateCallbacks = 0; maxUs = 0; sumUs = 0; maxGapMs = 0; }
        double percentileUs (double p) const
        {
            uint64_t total = 0, run = 0;
            for (auto& b : histogram) total += b.load();
            for (int i = 0; i < kBins; ++i) { run += histogram[(size_t) i].load(); if ((double) run >= p * (double) total) return (i + 1) * 5.0; }
            return kBins * 5.0;
        }
    } perf;

    // ---- audio device
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        const double fs = device->getCurrentSampleRate();
        engine.prepare (fs);
        room.prepare (fs);
        local.prepare (fs);
        radio.setOutputRate (fs);
        sampleRate = fs;
        blockSize = device->getCurrentBufferSizeSamples();
        inputChannels = device->getActiveInputChannels().countNumberOfSetBits();
        lastHostNs = 0;
        perf.reset();
    }
    void audioDeviceStopped() override { sampleRate = 0; env.withdraw (kEnvMicrophone); }

    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n, const juce::AudioIODeviceCallbackContext& context) override
    {
        const auto t0 = juce::Time::getHighResolutionTicks();
        juce::ScopedNoDenormals noDenormals;
        if (panic.exchange (false)) engine.reset();

        Controls c;
        for (int i = 0; i < kNumParams; ++i) c.p[i] = params[(size_t) i].load (std::memory_order_relaxed);
        c.mode = (Mode) mode.load (std::memory_order_relaxed);
        c.detune = dial (c.p[kTune], kNumStations).offCentre;
        c.plan = RoutingPlan::unpack (plan.load (std::memory_order_relaxed));
        const auto nowMs = (int64_t) juce::Time::getMillisecondCounter();
        const bool mic = roomEnabled.load (std::memory_order_relaxed) && numIn > 0 && in[0] != nullptr;

        for (int at = 0; at < n; at += kScratch)
        {
            const int m = juce::jmin (kScratch, n - at);
            radio.read (radioL, radioR, m);
            for (int i = 0; i < m; ++i) localBuf[i] = local.next();
            if (mic) env.publish (kEnvMicrophone, room.process (in[0] + at, m), nowMs);   // analysed here; never routed to the output
            c.room = env.read (nowMs);
            float* l = numOut > 0 && out[0] != nullptr ? out[0] + at : spare;
            float* r = numOut > 1 && out[1] != nullptr ? out[1] + at : spare;
            engine.process (radioL, radioR, localBuf, l, r, m, c);
        }
        for (int ch = 2; ch < numOut; ++ch) if (out[ch] != nullptr) juce::FloatVectorOperations::clear (out[ch], n);
        roomLevel = c.room.level; roomActivity = c.room.activity; roomBrightness = c.room.brightness;

        if (stallOnce.exchange (false))   // --measure positive control: overrun this callback by three periods, on purpose
            while (juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) < 3.0 * n / sampleRate.load()) {}
        const double us = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1.0e6;
        perf.histogram[(size_t) juce::jlimit (0, Perf::kBins - 1, (int) (us / 5.0))].fetch_add (1, std::memory_order_relaxed);
        perf.callbacks.fetch_add (1, std::memory_order_relaxed);
        perf.sumUs = perf.sumUs.load (std::memory_order_relaxed) + us;
        if (us > perf.maxUs.load (std::memory_order_relaxed)) perf.maxUs = us;
        if (context.hostTimeNs != nullptr)
        {
            const uint64_t host = *context.hostTimeNs;
            if (lastHostNs != 0 && lastN > 0)
            {
                const double gapMs = (double) (host - lastHostNs) * 1.0e-6, expectedMs = 1000.0 * lastN / sampleRate.load (std::memory_order_relaxed);
                if (gapMs > perf.maxGapMs.load (std::memory_order_relaxed)) perf.maxGapMs = gapMs;
                if (gapMs > 1.5 * expectedMs) perf.lateCallbacks.fetch_add (1, std::memory_order_relaxed);
            }
            lastHostNs = host;
            lastN = n;
        }
    }

    // ---- MIDI (its own thread): controllers move knobs, notes set PITCH
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override
    {
        if (m.isController())
        {
            const auto cc = (size_t) m.getControllerNumber();
            if (const int target = learnTarget.exchange (-1); target >= 0)
            {
                for (auto& other : ccMap) if (other.load() == target) other = -1;
                ccMap[cc] = (int8_t) target;
                mapDirty = true;
            }
            if (const int p = ccMap[cc].load(); p >= 0) params[(size_t) p] = (float) m.getControllerValue() / 127.0f;
            midiEvents.fetch_add (1);
        }
        else if (m.isNoteOn())
        {
            params[kPitch] = clamp01 ((float) (m.getNoteNumber() - 36) / 48.0f);
            midiEvents.fetch_add (1);
        }
    }

private:
    static constexpr int kScratch = 2048;
    EnvBus env;
    RoomAnalyser room;
    TestSource local;
    float radioL[kScratch], radioR[kScratch], localBuf[kScratch], spare[kScratch];
    uint64_t lastHostNs = 0;
    int lastN = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
struct Look final : juce::LookAndFeel_V4
{
    juce::Image knobStrip;   // optional Blender-rendered filmstrip (vertical, square frames), used by the TUNE knob

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto area = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
        const float size = juce::jmin (area.getWidth(), area.getHeight()), radius = size * 0.5f, angle = a0 + pos * (a1 - a0);
        const auto centre = area.getCentre();
        if (knobStrip.isValid() && s.getProperties().contains ("rendered"))
        {
            const int frame = knobStrip.getWidth(), frames = knobStrip.getHeight() / frame;
            const int index = juce::jlimit (0, frames - 1, juce::roundToInt (pos * (float) (frames - 1)));
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (knobStrip, juce::roundToInt (centre.x - radius), juce::roundToInt (centre.y - radius), juce::roundToInt (size), juce::roundToInt (size),
                         0, index * frame, frame, frame);
            return;
        }
        g.setColour (kPanel.brighter (0.25f));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (centre).reduced (5.0f));
        juce::Path track, value;
        track.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, a0, a1, true);
        value.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, a0, angle, true);
        g.setColour (kPanel.brighter (0.5f));
        g.strokePath (track, juce::PathStrokeType (3.0f));
        g.setColour (s.hasKeyboardFocus (false) || s.getProperties().contains ("learn") ? juce::Colours::white : kOrange);
        g.strokePath (value, juce::PathStrokeType (3.0f));
        g.drawLine ({ centre.getPointOnCircumference (radius * 0.3f, angle), centre.getPointOnCircumference (radius - 8.0f, angle) }, 3.0f);
    }
};

/** The TUNE dial: station arcs, clean zones and the needle. It shows where the knob is, not a radio frequency. */
struct StationDial final : juce::Component
{
    std::array<Station, kNumStations> stations;
    float tune = 0;
    int tuned = 0;
    Radio::State state = Radio::State::Idle;

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (kPanel);
        g.fillRoundedRectangle (area, 6.0f);
        const float slot = area.getWidth() / kNumStations;
        for (int i = 0; i < kNumStations; ++i)
        {
            const auto cell = juce::Rectangle<float> (area.getX() + slot * (float) i, area.getY(), slot, area.getHeight());
            const auto clean = cell.withSizeKeepingCentre (slot * 0.4f, cell.getHeight()).removeFromBottom (8.0f);
            g.setColour (i == tuned ? kRadio.withAlpha (state == Radio::State::Playing ? 1.0f : 0.45f) : kPanel.brighter (0.5f));
            g.fillRect (clean);                                             // the clean-reception zone of each arc
            g.setColour (i == tuned ? kText : kText.withAlpha (0.5f));
            g.setFont (12.0f);
            g.drawFittedText (stations[(size_t) i].name, cell.reduced (4.0f, 2.0f).withTrimmedBottom (10.0f).toNearestInt(), juce::Justification::centred, 2);
            g.setColour (kBack);
            g.drawVerticalLine (juce::roundToInt (cell.getRight()), area.getY(), area.getBottom());
        }
        g.setColour (kOrange);
        g.fillRect (area.getX() + tune * area.getWidth() - 1.5f, area.getY(), 3.0f, area.getHeight());
    }
};

/** Outputs along the top, resonator inputs along the bottom, cables in between. Drag from an output to an input to
    patch; drag a cable off its input to move it, or drop it in space to pull it out. All edits go through Graph. */
struct PatchBay final : juce::Component, juce::SettableTooltipClient
{
    Graph graph = Graph::defaultPatch();
    std::function<void()> onChange;
    std::function<void (const juce::String&)> onMessage;
    std::function<float (int port)> valueOf;      // live 0..1 value of an output port

    juce::Point<float> socket (int port) const
    {
        const bool out = port < kFirstInput;
        const int index = out ? port : port - kFirstInput, count = out ? kFirstInput : kNumInputs;
        return { (float) getWidth() * ((float) index + 0.5f) / (float) count, out ? 52.0f : (float) getHeight() - 44.0f };
    }
    int socketAt (juce::Point<float> p) const
    {
        for (int i = 0; i < kNumPorts; ++i) if (socket (i).getDistanceFrom (p) < 22.0f) return i;
        return -1;
    }
    static juce::Path cable (juce::Point<float> a, juce::Point<float> b)
    {
        juce::Path p;
        const float sag = 30.0f + 0.12f * std::abs (a.x - b.x);
        p.startNewSubPath (a);
        p.cubicTo (a.x, a.y + sag, b.x, b.y - sag * 0.4f, b.x, b.y);
        return p;
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setFont (11.0f);
        for (int i = 0; i < kNumPorts; ++i)
        {
            const auto& spec = ports[i];
            const auto p = socket (i);
            const auto colour = moduleColour (spec.module);
            const bool out = i < kFirstInput;
            ConnectError verdict = ConnectError::None;
            if (dragFrom >= 0 && ! out) verdict = graph.check (dragFrom, i);
            const bool offered = dragFrom >= 0 && ! out && (verdict == ConnectError::None || verdict == ConnectError::FanIn);
            const float dim = dragFrom >= 0 && ! out && ! offered ? 0.25f : 1.0f;
            const auto ring = juce::Rectangle<float> (26.0f, 26.0f).withCentre (p);
            if (out) { g.setColour (colour.withAlpha (0.15f + 0.6f * valueOf (i))); spec.type == PortType::Audio ? g.fillRect (ring.expanded (4.0f)) : g.fillEllipse (ring.expanded (4.0f)); }
            g.setColour (kBack);
            spec.type == PortType::Audio ? g.fillRect (ring) : g.fillEllipse (ring);                   // audio sockets are square, control round
            g.setColour ((offered ? juce::Colours::white : colour).withAlpha (dim));
            spec.type == PortType::Audio ? g.drawRect (ring, offered && i == hover ? 4.0f : 2.0f) : g.drawEllipse (ring, offered && i == hover ? 4.0f : 2.0f);
            g.setColour (kText.withAlpha (dim));
            g.drawText (spec.label, juce::Rectangle<float> (80.0f, 14.0f).withCentre ({ p.x, out ? p.y - 24.0f : p.y + 24.0f }), juce::Justification::centred);
            if (i == 0 || ports[i - 1].module != spec.module)
            {
                g.setColour (colour);
                g.drawText (moduleNames[spec.module], juce::Rectangle<float> (120.0f, 14.0f).withPosition (p.x - 14.0f, out ? 4.0f : (float) getHeight() - 16.0f), juce::Justification::left);
            }
        }
        for (int i = 0; i < graph.size(); ++i)
        {
            const auto c = graph[i];
            const float v = valueOf (c.from);
            g.setColour (moduleColour (ports[c.from].module).withAlpha (0.45f + 0.55f * v));
            g.strokePath (cable (socket (c.from), socket (c.to)), juce::PathStrokeType ((ports[c.from].type == PortType::Audio ? 5.0f : 3.0f) + 3.0f * v, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        if (dragFrom >= 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.strokePath (cable (socket (dragFrom), dragAt), juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int port = socketAt (e.position);
        if (port < 0) return;
        if (port < kFirstInput) dragFrom = port;
        else if (const int source = graph.cableInto (port); source >= 0)      // pick the cable up by its input end
        {
            graph.disconnect (source, port);
            dragFrom = source;
            changed();
        }
        dragAt = e.position;
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override { dragAt = e.position; hover = socketAt (e.position); repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragFrom < 0) return;
        if (const int to = socketAt (e.position); to >= 0 && to != dragFrom)
        {
            auto error = graph.check (dragFrom, to);
            if (error == ConnectError::FanIn)                                  // the input is taken: the new cable replaces the old one
            {
                graph.disconnect (graph.cableInto (to), to);
                error = graph.check (dragFrom, to);
            }
            if (error == ConnectError::None) graph.connect (dragFrom, to);
            if (onMessage) onMessage (error == ConnectError::None ? juce::String (ports[dragFrom].id) + " -> " + ports[to].id : juce::String ("Not patched: ") + describe (error));
            changed();
        }
        dragFrom = hover = -1;
        repaint();
    }

private:
    void changed() { if (onChange) onChange(); }
    int dragFrom = -1, hover = -1;
    juce::Point<float> dragAt;
};

// ---------------------------------------------------------------------------------------------------------------------
class MainComponent final : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    MainComponent (const juce::String& measureFile, bool muted, int station, const juce::String& output) : measurePath (measureFile), transient (muted || measureFile.isNotEmpty())
    {
        outputOverride = output.startsWith ("snapshot:") ? juce::String() : output;
        snapshotPath = output.startsWith ("snapshot:") ? output.fromFirstOccurrenceOf (":", false, false) : juce::String();
        juce::PropertiesFile::Options options;
        options.applicationName = "WILDBAND";
        options.filenameSuffix = "settings";
        options.osxLibrarySubFolder = "Application Support";
        settings.setStorageParameters (options);
        setLookAndFeel (&look);
       #if WILDBAND_HAS_KNOB
        look.knobStrip = juce::ImageCache::getFromMemory (BinaryData::knob_png, BinaryData::knob_pngSize);
       #endif

        loadSettings();
        if (transient) instrument.params[kLevel] = 0.0f;      // --mute / --measure: everything runs, nothing is heard
        if (station >= 0 && station < kNumStations)           // --station=N (test aid): start on slot N
        {
            instrument.params[kTune] = dialCentre (station, kNumStations);
            wanted = tuned = station;
            instrument.radio.tune (stations[(size_t) tuned].url);
            urlEditor.setText (stations[(size_t) tuned].url, false);
        }
        for (int i = 0; i < kNumParams; ++i)
        {
            auto& s = knobs[(size_t) i];
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.setRange (0.0, 1.0);
            s.setDoubleClickReturnValue (true, i == kTune ? (double) dialCentre (0, kNumStations) : (double) paramDefaults[i]);
            s.setValue (instrument.params[(size_t) i].load(), juce::dontSendNotification);
            s.onValueChange = [this, i] { instrument.params[(size_t) i] = (float) knobs[(size_t) i].getValue(); };
            s.onDragStart = [this, i] { if (learn.getToggleState()) instrument.learnTarget = i; };
            s.setTitle (paramNames[i]);
            addAndMakeVisible (s);
            names[(size_t) i].setJustificationType (juce::Justification::centred);
            names[(size_t) i].setFont (juce::FontOptions (i == kTune ? 15.0f : 12.0f, juce::Font::bold));
            addAndMakeVisible (names[(size_t) i]);
        }
        knobs[kTune].getProperties().set ("rendered", true);

        static const char* modeNames[] = { "ENGINE", "DRY", "REFERENCE" };
        for (int m = 0; m < 3; ++m)
        {
            modes[(size_t) m].setButtonText (modeNames[m]);
            modes[(size_t) m].setClickingTogglesState (true);
            modes[(size_t) m].setRadioGroupId (1);
            modes[(size_t) m].setColour (juce::TextButton::buttonOnColourId, kOrange);
            modes[(size_t) m].setColour (juce::TextButton::textColourOnId, kBack);
            modes[(size_t) m].onClick = [this, m] { if (modes[(size_t) m].getToggleState()) instrument.mode = m; };
            addAndMakeVisible (modes[(size_t) m]);
        }
        modes[0].setToggleState (true, juce::dontSendNotification);
        modes[1].setTooltip ("The source alone, for comparison");
        modes[2].setTooltip ("An ordinary resonant filter and feedback delay on the same knobs, for comparison");

        roomButton.setButtonText ("ROOM (microphone)");
        roomButton.onClick = [this] { setRoom (roomButton.getToggleState()); };
        learn.setButtonText ("MIDI LEARN");
        learn.onClick = [this] { if (! learn.getToggleState()) instrument.learnTarget = -1; };
        audioButton.setButtonText ("AUDIO...");
        audioButton.onClick = [this] { showAudioSettings(); };
        panicButton.setButtonText ("PANIC");
        panicButton.setTooltip ("Silences the resonators at once");
        panicButton.onClick = [this] { instrument.panic = true; };
        setUrl.setButtonText ("SET");
        setUrl.onClick = [this] { applyUrl(); };
        urlEditor.onReturnKey = [this] { applyUrl(); };
        urlEditor.setTextToShowWhenEmpty ("stream address (http/https, MP3 or AAC)", kText.withAlpha (0.4f));
        for (auto* c : std::initializer_list<juce::Component*> { &roomButton, &learn, &audioButton, &panicButton, &setUrl, &urlEditor, &dialView, &status, &hint, &patch, &title })
            addAndMakeVisible (c);
        title.setText ("WILDBAND  P0", juce::dontSendNotification);
        title.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, kOrange);
        status.setFont (juce::FontOptions (13.0f));
        hint.setFont (juce::FontOptions (12.0f));
        hint.setColour (juce::Label::textColourId, kText.withAlpha (0.7f));

        patch.valueOf = [this] (int port)
        {
            const float couple = instrument.params[kCouple].load();
            switch (port)
            {
                case kRadioAudioOut:     return instrument.radio.state() == Radio::State::Playing ? 0.3f + 0.7f * instrument.engine.meterRadioLevel.load() : 0.0f;
                case kLocalAudioOut:     return 0.5f;
                case kRadioLevelOut:     return couple * instrument.engine.meterRadioLevel.load();
                case kRadioActivityOut:  return couple * instrument.engine.meterRadioActivity.load();
                case kRoomLevelOut:      return couple * instrument.roomLevel.load();
                case kRoomActivityOut:   return couple * instrument.roomActivity.load();
                case kRoomBrightnessOut: return couple * instrument.roomBrightness.load();
                default:                 return 0.0f;
            }
        };
        patch.onChange = [this] { instrument.plan = RoutingPlan::fromGraph (patch.graph).pack(); settingsDirty = true; };
        patch.onMessage = [this] (const juce::String& m) { flash (m); };
        patch.onChange();

        // KNOWN DEFECT (docs/p0/EVIDENCE-LOG.md): when the output device also has inputs (most USB interfaces), macOS
        // shows its microphone prompt as soon as CoreAudio opens the device, and blocks this thread until it is
        // answered, although no input is requested. So the window goes up first, with an explanation, and the device
        // is opened a moment later (openAudio, from the timer).
        if (microphoneAccess() == MicAccess::Undetermined)
            roomMessage = "If macOS asks about the microphone now, it is because your audio interface has inputs. WILDBAND listens only while ROOM is on; either answer works.";
        openMidiInputs();
        midiList = juce::MidiDeviceListConnection::make ([this] { openMidiInputs(); });

        setSize (980, 640);
        startTimerHz (30);
    }

    void openAudio()
    {
        // output only at launch: the microphone is opened by the ROOM button, never automatically
        auto saved = settings.getUserSettings()->getXmlValue ("audio");
        if (saved != nullptr) saved->removeAttribute ("audioInputDeviceName");
        juce::AudioDeviceManager::AudioDeviceSetup preferred;
        preferred.outputDeviceName = outputOverride;
        devices.initialise (0, 2, outputOverride.isEmpty() ? saved.get() : nullptr, true, {}, outputOverride.isEmpty() ? nullptr : &preferred);
        closeInputs();
        devices.addAudioCallback (&instrument);
        devices.addChangeListener (this);
        if (roomMessage.startsWith ("If macOS asks")) roomMessage.clear();
        if (measurePath.isNotEmpty()) beginMeasure();
    }

    ~MainComponent() override
    {
        stopTimer();
        saveSettings();
        devices.removeChangeListener (this);
        devices.removeAudioCallback (&instrument);
        devices.removeMidiInputDeviceCallback ({}, &instrument);
        devices.closeAudioDevice();
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override { g.fillAll (kBack); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        auto top = area.removeFromTop (30);
        title.setBounds (top.removeFromLeft (170));
        panicButton.setBounds (top.removeFromRight (70));
        top.removeFromRight (6);
        audioButton.setBounds (top.removeFromRight (80));
        status.setBounds (top.reduced (8, 0));

        area.removeFromTop (8);
        auto row = area.removeFromTop (190);
        auto tuneArea = row.removeFromLeft (190);
        names[kTune].setBounds (tuneArea.removeFromBottom (20));
        knobs[kTune].setBounds (tuneArea);
        const int w = row.getWidth() / (kNumParams - 1);
        auto small = row.withTrimmedTop (30).withTrimmedBottom (30);
        for (int i = 1; i < kNumParams; ++i)
        {
            auto cell = small.removeFromLeft (w);
            names[(size_t) i].setBounds (cell.removeFromBottom (34));
            knobs[(size_t) i].setBounds (cell);
        }

        area.removeFromTop (4);
        dialView.setBounds (area.removeFromTop (44));
        area.removeFromTop (6);
        auto urlRow = area.removeFromTop (26);
        setUrl.setBounds (urlRow.removeFromRight (60));
        urlRow.removeFromRight (6);
        urlEditor.setBounds (urlRow);

        area.removeFromTop (8);
        auto buttons = area.removeFromTop (28);
        for (auto& m : modes) { m.setBounds (buttons.removeFromLeft (110)); buttons.removeFromLeft (4); }
        buttons.removeFromLeft (20);
        roomButton.setBounds (buttons.removeFromLeft (170));
        learn.setBounds (buttons.removeFromLeft (120));
        hint.setBounds (buttons);

        area.removeFromTop (8);
        patch.setBounds (area);
    }

private:
    // ---- periodic UI work: follow MIDI, drive the tuner, show state. Nothing here touches the audio thread.
    void timerCallback() override
    {
        if (++ticks == 4 && snapshotPath.isNotEmpty())   // --snapshot=FILE (test aid): draw the UI to a PNG and quit; audio is never opened
        {
            juce::FileOutputStream out { juce::File (snapshotPath) };
            out.setPosition (0);
            out.truncate();
            juce::PNGImageFormat().writeImageToStream (createComponentSnapshot (getLocalBounds(), true, 2.0f), out);
            juce::JUCEApplication::quit();
            return;
        }
        if (ticks == 8) openAudio();            // a quarter of a second after the window appears

        for (int i = 0; i < kNumParams; ++i)
        {
            auto& s = knobs[(size_t) i];
            const float v = instrument.params[(size_t) i].load();
            if (! s.isMouseButtonDown() && std::abs ((float) s.getValue() - v) > 1.0e-4f) { s.setValue (v, juce::dontSendNotification); settingsDirty = true; }
            int cc = -1;
            for (int n = 0; n < 128; ++n) if (instrument.ccMap[(size_t) n].load() == i) cc = n;
            const bool waiting = instrument.learnTarget.load() == i;
            waiting ? (void) s.getProperties().set ("learn", true) : (void) s.getProperties().remove ("learn");
            names[(size_t) i].setText (juce::String (paramNames[i]) + (i == kTune ? "" : "\n") + (waiting ? " move a control" : cc >= 0 ? " cc " + juce::String (cc) : ""), juce::dontSendNotification);
        }

        // TUNE: the knob lands in a station's arc; once it has rested there for a moment, that station is fetched.
        const auto d = dial (instrument.params[kTune].load(), kNumStations);
        const auto now = juce::Time::getMillisecondCounter();
        if (d.station != wanted) { wanted = d.station; wantedSince = now; }
        if (wanted != tuned && now - wantedSince > 250) { tuned = wanted; instrument.radio.tune (stations[(size_t) tuned].url); urlEditor.setText (stations[(size_t) tuned].url, false); settingsDirty = true; }

        const auto state = instrument.radio.state();
        dialView.stations = stations;
        dialView.tune = instrument.params[kTune].load();
        dialView.tuned = tuned;
        dialView.state = state;
        dialView.repaint();
        patch.repaint();

        juce::String text = juce::String (Radio::name (state)) + "  " + stations[(size_t) tuned].name;
        if (state == Radio::State::Playing || state == Radio::State::Buffering) text << "  |  " << instrument.radio.detail().upToFirstOccurrenceOf (" - ", false, false) << "  |  buffer " << juce::String (instrument.radio.bufferedSeconds(), 1) << " s";
        else if (instrument.radio.detail().isNotEmpty()) text << "  |  " << instrument.radio.detail();
        if (d.offCentre > 0.0f) text << "  |  off-centre " << juce::roundToInt (d.offCentre * 100.0f) << " %";
        if (const double fs = instrument.sampleRate.load(); fs > 0) text << "  |  " << juce::String (fs / 1000.0, 1) << " kHz / " << instrument.blockSize.load();
        else text << "  |  NO AUDIO DEVICE";
        if (instrument.engine.faults.load() > 0) text << "  |  ENGINE FAULTS " << instrument.engine.faults.load();
        status.setText (text, juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, state == Radio::State::Playing ? kRadio : state == Radio::State::Failed ? juce::Colours::orangered : kText);

        if (now > flashUntil)
        {
            juce::String h;
            if (ticks < 8 && roomMessage.isNotEmpty()) h = roomMessage;
            else if (learn.getToggleState()) h = "Touch a knob on screen, then move a control on your controller.";
            else if (patch.graph.cableInto (kResAudioIn) < 0) h = "Nothing is patched into RESONATOR IN: drag a cable from an AUDIO output.";
            else if (patch.graph.cableInto (kResAudioIn) == kLocalAudioOut) h = "LOCAL TEST SOURCE is playing. It is not live radio.";
            else if (roomMessage.isNotEmpty()) h = roomMessage;
            else h = "MIDI: " + juce::String (midiInputs) + " input(s), " + juce::String (instrument.midiEvents.load()) + " events";
            hint.setText (h, juce::dontSendNotification);
        }

        if (instrument.mapDirty.exchange (false)) { settingsDirty = true; learn.setToggleState (false, juce::dontSendNotification); }
        if (settingsDirty && now - lastSave > 2000) saveSettings();
        if (measurePath.isNotEmpty() && ticks > 8) stepMeasure();
    }

    void flash (const juce::String& m) { hint.setText (m, juce::dontSendNotification); flashUntil = juce::Time::getMillisecondCounter() + 3000; }

    void applyUrl()
    {
        const auto url = urlEditor.getText().trim();
        if (! (url.startsWithIgnoreCase ("http://") || url.startsWithIgnoreCase ("https://"))) return flash ("A stream address starts with http:// or https://");
        stations[(size_t) tuned] = { juce::URL (url).getDomain(), url };
        instrument.radio.tune (url);
        settingsDirty = true;
    }

    // ---- ROOM: explicit permission, then the default input device; analysis only
    void setRoom (bool on)
    {
        if (! on)
        {
            instrument.roomEnabled = false;
            closeInputs();
            roomMessage.clear();
            return;
        }
        requestMicrophone ([safe = juce::Component::SafePointer<MainComponent> (this)] (bool granted)
        {
            if (safe == nullptr) return;
            if (! granted)
            {
                safe->roomButton.setToggleState (false, juce::dontSendNotification);
                safe->roomMessage = "Microphone access is off, so ROOM stays neutral. Allow WILDBAND in System Settings > Privacy & Security > Microphone.";
                return;
            }
            auto setup = safe->devices.getAudioDeviceSetup();
            setup.inputDeviceName = {};
            if (auto* type = safe->devices.getCurrentDeviceTypeObject())
                setup.inputDeviceName = type->getDeviceNames (true)[type->getDefaultDeviceIndex (true)];
            setup.useDefaultInputChannels = true;
            const auto error = setup.inputDeviceName.isEmpty() ? juce::String ("no input device") : safe->devices.setAudioDeviceSetup (setup, true);
            if (error.isNotEmpty() || safe->instrument.inputChannels.load() == 0)
            {
                safe->roomButton.setToggleState (false, juce::dontSendNotification);
                safe->roomMessage = "Could not open the microphone (" + (error.isNotEmpty() ? error : juce::String ("no input channels")) + "). ROOM stays neutral.";
                safe->closeInputs();
                return;
            }
            safe->instrument.roomEnabled = true;
            safe->roomMessage = "ROOM is listening on " + setup.inputDeviceName + " (analysed here; never played or stored).";
        });
    }

    void closeInputs()
    {
        auto setup = devices.getAudioDeviceSetup();
        if (setup.inputDeviceName.isEmpty() && setup.inputChannels.isZero()) return;
        setup.inputDeviceName = {};
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        devices.setAudioDeviceSetup (setup, true);
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (instrument.roomEnabled.load() && instrument.inputChannels.load() == 0)      // the input went away under us
        {
            instrument.roomEnabled = false;
            roomButton.setToggleState (false, juce::dontSendNotification);
            roomMessage = "The microphone is no longer available. ROOM is neutral.";
        }
        settingsDirty = true;
    }

    void openMidiInputs()
    {
        const auto inputs = juce::MidiInput::getAvailableDevices();
        for (const auto& d : inputs) devices.setMidiInputDeviceEnabled (d.identifier, true);
        devices.removeMidiInputDeviceCallback ({}, &instrument);
        devices.addMidiInputDeviceCallback ({}, &instrument);
        midiInputs = inputs.size();
    }

    void showAudioSettings()
    {
        auto* selector = new juce::AudioDeviceSelectorComponent (devices, 0, 0, 2, 2, false, false, true, false);
        selector->setSize (480, 360);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (selector);
        o.dialogTitle = "Audio output";
        o.dialogBackgroundColour = kPanel;
        o.launchAsync();
    }

    // ---- settings: knob positions, stations, patch, controller map, output device. No audio, no observations.
    void loadSettings()
    {
        auto& p = *settings.getUserSettings();
        for (int i = 0; i < kNumStations; ++i)
            stations[(size_t) i] = { p.getValue ("station" + juce::String (i) + "name", kDefaultStations[i].name), p.getValue ("station" + juce::String (i) + "url", kDefaultStations[i].url) };
        for (int i = 0; i < kNumParams; ++i)
        {
            instrument.params[(size_t) i] = clamp01 ((float) p.getDoubleValue (juce::String ("param") + paramNames[i], instrument.params[(size_t) i].load()));
            if (p.containsKey (juce::String ("cc") + paramNames[i]))
            {
                for (auto& c : instrument.ccMap) if (c.load() == i) c = -1;
                if (const int cc = p.getIntValue (juce::String ("cc") + paramNames[i]); cc >= 0 && cc < 128) instrument.ccMap[(size_t) cc] = (int8_t) i;
            }
        }
        instrument.params[kLevel] = juce::jmin (instrument.params[kLevel].load(), paramDefaults[kLevel]);   // never start loud
        if (p.containsKey ("cables"))
        {
            patch.graph.clear();
            for (const auto& cable : juce::StringArray::fromTokens (p.getValue ("cables"), ";", ""))     // "from|to", TALORA's cable id format
                patch.graph.connect (findPort (cable.upToFirstOccurrenceOf ("|", false, false).toRawUTF8()), findPort (cable.fromFirstOccurrenceOf ("|", false, false).toRawUTF8()));
        }
        wanted = tuned = dial (instrument.params[kTune].load(), kNumStations).station;
        instrument.radio.tune (stations[(size_t) tuned].url);
        urlEditor.setText (stations[(size_t) tuned].url, false);
    }

    void saveSettings()
    {
        if (transient) return;                                        // a muted or measurement run leaves the owner's settings alone
        auto& p = *settings.getUserSettings();
        for (int i = 0; i < kNumStations; ++i) { p.setValue ("station" + juce::String (i) + "name", stations[(size_t) i].name); p.setValue ("station" + juce::String (i) + "url", stations[(size_t) i].url); }
        for (int i = 0; i < kNumParams; ++i)
        {
            p.setValue (juce::String ("param") + paramNames[i], (double) instrument.params[(size_t) i].load());
            int cc = -1;
            for (int n = 0; n < 128; ++n) if (instrument.ccMap[(size_t) n].load() == i) cc = n;
            p.setValue (juce::String ("cc") + paramNames[i], cc);
        }
        juce::StringArray cables;
        for (int i = 0; i < patch.graph.size(); ++i) cables.add (juce::String (ports[patch.graph[i].from].id) + "|" + ports[patch.graph[i].to].id);
        p.setValue ("cables", cables.joinIntoString (";"));
        if (auto xml = devices.createStateXml()) p.setValue ("audio", xml.get());
        p.saveIfNeeded();
        settingsDirty = false;
        lastSave = juce::Time::getMillisecondCounter();
    }

    // ---- --measure: real CoreAudio callback timing at several rates and buffer sizes, engine working hard
    struct MeasureStep { double rate; int buffer; };
    void beginMeasure()
    {
        instrument.params[kFeedback] = 0.85f; instrument.params[kDrift] = 0.7f; instrument.params[kCouple] = 0.8f;
        for (double rate : { 44100.0, 48000.0, 96000.0 }) for (int buffer : { 32, 64, 128, 512 }) measureSteps.add ({ rate, buffer });
        auto* device = devices.getCurrentAudioDevice();
        measureLog << "Device: " << (device != nullptr ? device->getName() : "none") << ", " << juce::SystemStats::getOperatingSystemName() << ", " << juce::SystemStats::getDeviceDescription()
                   << ", " << juce::Time::getCurrentTime().toISO8601 (true) << "\n\n"
                   << "| rate | buffer (asked/got) | period ms | callbacks | mean us | p99.9 us | max us | max % of period | late callbacks | max host-time gap ms | radio underruns | reconnects | engine faults |\n|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
        measureOriginal = devices.getAudioDeviceSetup();
        measureIndex = -1;
        measureNext = juce::Time::getMillisecondCounter() + 6000;      // let the stream start first
    }
    void stepMeasure()
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now < measureNext) return;
        if (measureStalling)
        {
            measureLog << "\nPositive control (one callback deliberately held for three buffer periods at the last setting): late callbacks counted = "
                       << (juce::int64) instrument.perf.lateCallbacks.load() << ", max host-time gap " << juce::String (instrument.perf.maxGapMs.load(), 2) << " ms. "
                       << (instrument.perf.lateCallbacks.load() > 0 ? "The late-callback counter responds to an overrun." : "The late-callback counter did NOT respond: the zeros above are NOT evidence of no dropouts.") << "\n";
            devices.setAudioDeviceSetup (measureOriginal, true);         // put the interface back as it was found
            juce::File (measurePath).replaceWithText (measureLog);
            measurePath.clear();
            juce::JUCEApplication::quit();
            return;
        }
        if (measureSettling) { instrument.perf.reset(); underruns0 = instrument.radio.underruns.load(); reconnects0 = instrument.radio.reconnects.load(); measureSettling = false; measureNext = now + 20000; return; }
        if (measureIndex >= 0)
        {
            const auto& step = measureSteps.getReference (measureIndex);
            const auto& perf = instrument.perf;
            const double fs = instrument.sampleRate.load(), period = fs > 0 ? 1000.0 * instrument.blockSize.load() / fs : 0, n = (double) juce::jmax<uint64_t> (1, perf.callbacks.load());
            measureLog << "| " << step.rate << " (" << fs << ") | " << step.buffer << " / " << instrument.blockSize.load() << " | " << juce::String (period, 3) << " | " << (juce::int64) perf.callbacks.load()
                       << " | " << juce::String (perf.sumUs.load() / n, 1) << " | " << juce::roundToInt (perf.percentileUs (0.999)) << " | " << juce::roundToInt (perf.maxUs.load())
                       << " | " << juce::String (period > 0 ? perf.maxUs.load() / (period * 10.0) : 0.0, 1) << " | " << (juce::int64) perf.lateCallbacks.load() << " | " << juce::String (perf.maxGapMs.load(), 2)
                       << " | " << instrument.radio.underruns.load() - underruns0 << " | " << instrument.radio.reconnects.load() - reconnects0 << " | " << instrument.engine.faults.load() << " |\n";
        }
        if (++measureIndex >= measureSteps.size())
        {
            instrument.perf.reset();
            instrument.stallOnce = true;
            measureStalling = true;
            measureNext = now + 2000;
            return;
        }
        auto setup = devices.getAudioDeviceSetup();
        setup.sampleRate = measureSteps.getReference (measureIndex).rate;
        setup.bufferSize = measureSteps.getReference (measureIndex).buffer;
        devices.setAudioDeviceSetup (setup, true);
        measureSettling = true;
        measureNext = now + 5000;                                       // the stream restarts at the new rate; let it settle
    }

    Instrument instrument;
    juce::AudioDeviceManager devices;
    juce::ApplicationProperties settings;
    juce::MidiDeviceListConnection midiList;
    Look look;
    juce::TooltipWindow tooltips { this };

    std::array<juce::Slider, kNumParams> knobs;
    std::array<juce::Label, kNumParams> names;
    std::array<juce::TextButton, 3> modes;
    juce::ToggleButton roomButton, learn;
    juce::TextButton audioButton, panicButton, setUrl;
    juce::TextEditor urlEditor;
    juce::Label title, status, hint;
    StationDial dialView;
    PatchBay patch;

    std::array<Station, kNumStations> stations;
    int wanted = 0, tuned = 0, midiInputs = 0;
    juce::uint32 wantedSince = 0, flashUntil = 0, lastSave = 0;
    bool settingsDirty = false;
    int ticks = 0;
    juce::String roomMessage, outputOverride, snapshotPath;

    juce::String measurePath, measureLog;
    const bool transient;
    juce::Array<MeasureStep> measureSteps;
    juce::AudioDeviceManager::AudioDeviceSetup measureOriginal;
    int measureIndex = -1, underruns0 = 0, reconnects0 = 0;
    bool measureSettling = false, measureStalling = false;
    juce::uint32 measureNext = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

class App final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "WILDBAND"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::String measure;
        for (const auto& arg : juce::StringArray::fromTokens (commandLine, true))
            if (arg.startsWith ("--measure=")) measure = arg.fromFirstOccurrenceOf ("=", false, false).unquoted();
        int station = -1;
        for (const auto& arg : juce::StringArray::fromTokens (commandLine, true))
            if (arg.startsWith ("--station=")) station = arg.fromFirstOccurrenceOf ("=", false, false).getIntValue();
        juce::String output;                                  // --output=NAME (test aid): use this output device, ignore the saved one
        for (const auto& arg : juce::StringArray::fromTokens (commandLine, true))
            if (arg.startsWith ("--output=")) output = arg.fromFirstOccurrenceOf ("=", false, false).unquoted();
            else if (arg.startsWith ("--snapshot=")) output = "snapshot:" + arg.fromFirstOccurrenceOf ("=", false, false).unquoted();
        window = std::make_unique<Window> (measure, commandLine.contains ("--mute") || output.startsWith ("snapshot:"), station, output);
    }
    void shutdown() override { window = nullptr; }
    void systemRequestedQuit() override { quit(); }

private:
    struct Window final : juce::DocumentWindow
    {
        Window (const juce::String& measure, bool muted, int station, const juce::String& output) : DocumentWindow ("WILDBAND", kBack, DocumentWindow::closeButton | DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (measure, muted, station, output), true);
            setResizable (true, false);
            setResizeLimits (860, 600, 1600, 1000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};
} // namespace

START_JUCE_APPLICATION (App)
