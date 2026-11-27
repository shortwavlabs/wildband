// WILDBAND P0 standalone app: audio callback, MIDI, placeholder performance UI and patch bay.
// The UI is deliberately plain (docs/UI-AND-BLENDER-DIRECTION.md: interaction first). `--measure=FILE` runs the
// CoreAudio timing sweep used for docs/p0/EVIDENCE-LOG.md and quits.
#include <juce_audio_utils/juce_audio_utils.h>

#include <csignal>
#include <unistd.h>

#include "AudioIO.h"
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

bool roomAtStart = false;   // --room (test aid)
juce::String statusLogPath; // --log=FILE (test aid): one status line every 5 s
bool navTest = false;       // --nav-test (test aid): NEXT at 12 s, five quick NEXTs at 22 s, PREVIOUS at 32 s
bool switchTest = false;    // --switch-test (test aid): move the output to the built-in speakers and back while playing

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

// ---------------------------------------------------------------------------------------------------------------------
/** Everything the audio and MIDI threads touch. The UI talks to it through atomics only. */
class Instrument final : public juce::AudioIODeviceCallback, public juce::MidiInputCallback
{
public:
    Instrument()
    {
        for (int i = 0; i < kNumParams; ++i) params[(size_t) i] = paramDefaults[i];
        params[kTune] = dialCentre (0, kNumStations);
        for (auto& c : ccMap) c = -1;      // no controller is mapped until MIDI LEARN maps one: no guessed defaults
        for (auto& n : noteMap) n = -1;
        plans.publish (RoutingPlan::fromGraph (Graph::defaultPatch()));
    }

    // ---- shared state
    std::array<std::atomic<float>, kNumParams> params;
    std::atomic<int> mode { 0 };
    PlanMailbox plans;                                      // written by the patch bay (message thread), read by the callback
    std::atomic<bool> roomEnabled { false }, panic { false }, stallOnce { false };   // stallOnce: --measure positive control only
    // Station navigation can be mapped too. Targets from kNumParams up are actions, not knobs: PREVIOUS, NEXT, then one
    // per station slot. A controller fires an action when it rises through half way; a mapped note fires it on note-on
    // (and then no longer sets PITCH). The UI thread performs the action (navSteps / presetRequest).
    enum Action : int { kActionPrev = kNumParams, kActionNext, kActionPreset, kNumTargets = kActionPreset + kNumStations };
    std::array<std::atomic<int8_t>, 128> noteMap;           // note number -> Action, -1 = unmapped
    std::atomic<int> navSteps { 0 }, presetRequest { -1 };
    std::array<std::atomic<int8_t>, 128> ccMap;             // controller number -> Param or Action, -1 = unmapped
    std::atomic<int> learnTarget { -1 }, midiEvents { 0 };
    std::atomic<bool> mapDirty { false };
    std::atomic<float> roomLevel { 0 }, roomActivity { 0 }, roomBrightness { 0 };
    std::atomic<double> sampleRate { 0 };
    std::atomic<int> blockSize { 0 };

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
        local.prepare (fs);
        radio.setOutputRate (fs);
        sampleRate = fs;
        blockSize = device->getCurrentBufferSizeSamples();
        lastHostNs = 0;
        perf.reset();
    }
    void audioDeviceStopped() override { sampleRate = 0; }

    /** Message thread: the latest values from the WILDBAND Room helper. They go stale, and so neutral, by themselves. */
    void publishRoom (EnvFrame frame) { env.publish (kEnvMicrophone, frame, (int64_t) juce::Time::getMillisecondCounter()); }
    void withdrawRoom() { env.withdraw (kEnvMicrophone); }

    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int n, const juce::AudioIODeviceCallbackContext& context) override
    {
        const auto t0 = juce::Time::getHighResolutionTicks();
        juce::ScopedNoDenormals noDenormals;
        if (panic.exchange (false)) engine.panic();      // also silences tuner static for the rest of the current retune

        Controls c;
        for (int i = 0; i < kNumParams; ++i) c.p[i] = params[(size_t) i].load (std::memory_order_relaxed);
        c.mode = (Mode) mode.load (std::memory_order_relaxed);
        c.detune = dial (c.p[kTune], kNumStations).offCentre;
        c.gap = radio.tuneGap.load (std::memory_order_relaxed);   // written by radio.read in the previous block
        c.plan = plans.read();                              // whole plan or the previous whole plan, never a mixture
        const auto nowMs = (int64_t) juce::Time::getMillisecondCounter();

        for (int at = 0; at < n; at += kScratch)
        {
            const int m = juce::jmin (kScratch, n - at);
            radio.read (radioL, radioR, m);
            for (int i = 0; i < m; ++i) localBuf[i] = local.next();
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
            const int value = m.getControllerValue();
            if (const int target = learnTarget.exchange (-1); target >= 0)
            {
                unmap (target);
                ccMap[cc] = (int8_t) target;
                lastCc[cc] = 127;                                   // the move that taught it does not also fire it
                mapDirty = true;
            }
            const int target = ccMap[cc].load();
            if (target >= kNumParams) { if (value >= 64 && lastCc[cc] < 64) fire (target); }
            else if (target >= 0) params[(size_t) target] = (float) value / 127.0f;
            lastCc[cc] = (uint8_t) value;
            midiEvents.fetch_add (1);
        }
        else if (m.isNoteOn())
        {
            const auto note = (size_t) m.getNoteNumber();
            if (const int target = learnTarget.load(); target >= kNumParams)      // learning an action: a note may be its trigger
            {
                learnTarget = -1;
                unmap (target);
                noteMap[note] = (int8_t) target;
                mapDirty = true;
            }
            else if (const int action = noteMap[note].load(); action >= 0) fire (action);
            else params[kPitch] = clamp01 ((float) ((int) note - 36) / 48.0f);
            midiEvents.fetch_add (1);
        }
    }

    void unmap (int target) noexcept
    {
        for (auto& other : ccMap) if (other.load() == target) other = -1;
        for (auto& other : noteMap) if (other.load() == target) other = -1;
    }

private:
    void fire (int action) noexcept          // MIDI thread; the UI timer carries it out
    {
        if (action == kActionPrev) navSteps.fetch_sub (1);
        else if (action == kActionNext) navSteps.fetch_add (1);
        else if (action >= kActionPreset && action < kNumTargets) presetRequest = action - kActionPreset;
    }
    std::array<uint8_t, 128> lastCc {};      // MIDI thread only
    static constexpr int kScratch = 2048;
    EnvBus env;
    TestSource local;
    float radioL[kScratch], radioR[kScratch], localBuf[kScratch], spare[kScratch];
    uint64_t lastHostNs = 0;
    int lastN = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
struct Look final : juce::LookAndFeel_V4
{
    /** The DEPTH slider: a track with a centre mark, filled from the centre toward the thumb. */
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (! s.getProperties().contains ("bipolar")) return LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
        const float midY = (float) y + (float) h * 0.5f, centre = (float) x + (float) w * 0.5f;
        g.setColour (kPanel.brighter (0.5f));
        g.fillRoundedRectangle ((float) x, midY - 3.0f, (float) w, 6.0f, 3.0f);
        g.setColour (pos >= centre ? kOrange : kRadio);                       // also told apart by side and by the signed number
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (pos, centre), midY - 3.0f, juce::jmax (pos, centre), midY + 3.0f));
        g.setColour (kText);
        g.fillRect (centre - 1.0f, midY - 9.0f, 2.0f, 18.0f);
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ pos, midY }));
    }

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
    std::function<void (int)> onPick;        // a station's cell was clicked
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (onPick) onPick (juce::jlimit (0, kNumStations - 1, (int) ((float) e.x / (float) juce::jmax (1, getWidth()) * kNumStations)));
    }
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
    int selected = -1;                             // input port of the selected cable (an input has at most one), or -1
    std::function<void()> onChange, onSelect;
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
    /** The cable under `p` (as its input port), or -1. */
    int cableAt (juce::Point<float> p) const
    {
        for (int i = graph.size(); --i >= 0;)
        {
            juce::Point<float> nearest;
            cable (socket (graph[i].from), socket (graph[i].to)).getNearestPoint (p, nearest);
            if (nearest.getDistanceFrom (p) < 10.0f) return graph[i].to;
        }
        return -1;
    }
    void select (int input)
    {
        if (input >= 0 && graph.cableInto (input) < 0) input = -1;
        selected = input;
        if (onSelect) onSelect();
        repaint();
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
            const bool audio = ports[c.from].type == PortType::Audio;
            const float amount = std::abs (c.depth), v = valueOf (c.from) * amount;          // a cable at depth 0 carries nothing and looks it
            const auto path = cable (socket (c.from), socket (c.to));
            const juce::PathStrokeType stroke ((audio ? 5.0f : 2.0f + 1.5f * amount) + 3.0f * v, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            juce::Path drawn;
            if (c.depth < 0.0f) { const float dashes[] = { 9.0f, 7.0f }; stroke.createDashedStroke (drawn, path, dashes, 2); }   // inverted cables are dashed
            else stroke.createStrokedPath (drawn, path);
            if (c.to == selected)
            {
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.strokePath (path, juce::PathStrokeType (stroke.getStrokeThickness() + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour (moduleColour (ports[c.from].module).withAlpha (0.3f + 0.25f * amount + 0.45f * v));
            g.fillPath (drawn);
            if (! audio && (c.to == selected || std::abs (c.depth - 1.0f) > 0.005f))          // signed amount on any cable that is not at the default
            {
                const auto mid = path.getPointAlongPath (path.getLength() * 0.55f);
                const auto text = (c.depth > 0.0f ? "+" : "") + juce::String (juce::roundToInt (c.depth * 100.0f)) + " %";
                const auto badge = juce::Rectangle<float> (46.0f, 16.0f).withCentre (mid);
                g.setColour (kBack.withAlpha (0.85f));
                g.fillRoundedRectangle (badge, 4.0f);
                g.setColour (c.to == selected ? juce::Colours::white : kText.withAlpha (0.8f));
                g.setFont (11.0f);
                g.drawText (text, badge, juce::Justification::centred);
            }
        }
        if (dragFrom >= 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.strokePath (cable (socket (dragFrom), dragAt), juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Click a cable (or an occupied input) to select it for the DEPTH control. Drag from an output to patch. Drag a
    // cable off its input to move it - it keeps its depth - or drop it in space to pull it out.
    void mouseDown (const juce::MouseEvent& e) override
    {
        pressAt = dragAt = e.position;
        pendingInput = -1;
        const int port = socketAt (e.position);
        if (port < 0) return select (cableAt (e.position));                    // a cable, or empty space (deselects)
        if (port < kFirstInput) { dragFrom = port; dragDepth = 1.0f; }
        else if (graph.cableInto (port) >= 0) pendingInput = port;             // decided on release: click selects, drag picks up
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (pendingInput >= 0 && e.position.getDistanceFrom (pressAt) > 6.0f)  // it is a drag: pick the cable up by its input end
        {
            dragFrom = graph.cableInto (pendingInput);
            dragDepth = graph.depthInto (pendingInput);
            graph.disconnect (dragFrom, pendingInput);
            pendingInput = -1;
            select (-1);
            changed();
        }
        dragAt = e.position;
        hover = socketAt (e.position);
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (pendingInput >= 0) { const int input = pendingInput; pendingInput = -1; return select (input); }
        if (dragFrom < 0) return;
        if (const int to = socketAt (e.position); to >= 0 && to != dragFrom)
        {
            auto error = graph.check (dragFrom, to);
            if (error == ConnectError::FanIn)                                  // the input is taken: the new cable replaces the old one
            {
                graph.disconnect (graph.cableInto (to), to);
                error = graph.check (dragFrom, to);
            }
            if (error == ConnectError::None) graph.connect (dragFrom, to, dragDepth);
            if (onMessage) onMessage (error == ConnectError::None ? juce::String (ports[dragFrom].id) + " -> " + ports[to].id : juce::String ("Not patched: ") + describe (error));
            changed();
            dragFrom = hover = -1;
            return select (error == ConnectError::None ? to : -1);             // a new cable is selected, ready for DEPTH
        }
        dragFrom = hover = -1;
        if (selected >= 0 && graph.cableInto (selected) < 0) select (-1);
        repaint();
    }

private:
    void changed() { if (onChange) onChange(); }
    int dragFrom = -1, hover = -1, pendingInput = -1;
    float dragDepth = 1.0f;
    juce::Point<float> dragAt, pressAt;
};

// ---------------------------------------------------------------------------------------------------------------------
class MainComponent final : public juce::Component, private juce::Timer
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
        for (int i = 0; i < 2; ++i)
        {
            nav[(size_t) i].setTooltip (i == 0 ? "Previous station" : "Next station");
            nav[(size_t) i].onClick = [this, i] { navigate (Instrument::kActionPrev + i, ((tuned + (i == 0 ? -1 : 1)) % kNumStations + kNumStations) % kNumStations); };
            addAndMakeVisible (nav[(size_t) i]);
        }
        dialView.onPick = [this] (int slot) { navigate (Instrument::kActionPreset + slot, slot); };
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
        patch.onChange = [this] { instrument.plans.publish (RoutingPlan::fromGraph (patch.graph)); settingsDirty = true; };
        patch.onSelect = [this] { showDepthFor (patch.selected); };

        depth.setSliderStyle (juce::Slider::LinearHorizontal);
        depth.setRange (-100.0, 100.0, 1.0);
        depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 22);
        depth.setTextValueSuffix (" %");
        depth.setDoubleClickReturnValue (true, 0.0);          // centre: no influence
        depth.getProperties().set ("bipolar", true);
        depth.setTitle ("DEPTH");
        depth.setTooltip ("How strongly, and which way, the selected cable acts. Double-click for 0; +100 is the original behaviour.");
        depth.onValueChange = [this]
        {
            const int to = patch.selected, from = patch.graph.cableInto (to);
            if (from < 0) return;
            patch.graph.setDepth (from, to, (float) depth.getValue() / 100.0f);      // validated and clamped by Graph
            patch.onChange();
            patch.repaint();
        };
        addChildComponent (depth);
        addChildComponent (depthRoute);
        depthRoute.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        depthRoute.setJustificationType (juce::Justification::centredRight);
        patch.onMessage = [this] (const juce::String& m) { flash (m); };
        patch.onChange();

        openMidiInputs();
        midiList = juce::MidiDeviceListConnection::make ([this] { openMidiInputs(); });

        setSize (980, 640);
        startTimerHz (30);
    }

    void openAudio()
    {
        // Output only, always. This app declares no microphone use, so macOS never asks on its behalf; any input left
        // in a saved setup is dropped anyway.
        auto saved = settings.getUserSettings()->getXmlValue ("audio");
        if (saved != nullptr) { saved->removeAttribute ("audioInputDeviceName"); saved->removeAttribute ("audioDeviceInChans"); }
        juce::AudioDeviceManager::AudioDeviceSetup preferred;
        preferred.outputDeviceName = outputOverride;
        devices.initialise (0, 2, outputOverride.isEmpty() ? saved.get() : nullptr, true, {}, outputOverride.isEmpty() ? nullptr : &preferred);
        devices.addAudioCallback (&instrument);
        if (measurePath.isNotEmpty()) beginMeasure();
    }

    ~MainComponent() override
    {
        stopTimer();
        saveSettings();
        stopRoomHelper();
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
        auto strip = area.removeFromTop (44);
        nav[0].setBounds (strip.removeFromLeft (52).reduced (0, 4));
        nav[1].setBounds (strip.removeFromRight (52).reduced (0, 4));
        dialView.setBounds (strip.reduced (4, 0));
        area.removeFromTop (6);
        auto urlRow = area.removeFromTop (26);
        setUrl.setBounds (urlRow.removeFromRight (60));
        urlRow.removeFromRight (6);
        urlEditor.setBounds (urlRow);

        area.removeFromTop (8);
        auto buttons = area.removeFromTop (28);
        for (auto& m : modes) { m.setBounds (buttons.removeFromLeft (110)); buttons.removeFromLeft (4); }
        buttons.removeFromLeft (20);
        roomButton.setBounds (buttons.removeFromLeft (165));
        learn.setBounds (buttons.removeFromLeft (115));
        hint.setBounds (buttons);
        depthRoute.setBounds (buttons.removeFromLeft (juce::jmin (230, buttons.getWidth() / 2)));      // the DEPTH strip takes the hint's place while a cable is selected
        depth.setBounds (buttons.reduced (4, 0));

        area.removeFromTop (8);
        patch.setBounds (area);
    }

private:
    // ---- periodic UI work: follow MIDI, drive the tuner, show state. Nothing here touches the audio thread.
    void timerCallback() override
    {
        pollRoom();
        if (++ticks == 4 && snapshotPath.isNotEmpty())   // --snapshot=FILE (test aid): draw the UI to a PNG and quit; audio is never opened
        {
            juce::FileOutputStream out { juce::File (snapshotPath) };
            out.setPosition (0);
            out.truncate();
            juce::PNGImageFormat().writeImageToStream (createComponentSnapshot (getLocalBounds(), true, 2.0f), out);
            patch.graph.setDepth (patch.graph.cableInto (kResFeedbackIn), kResFeedbackIn, -0.35f);   // and once more with a cable selected and inverted
            patch.graph.setDepth (patch.graph.cableInto (kResDriftIn), kResDriftIn, 0.5f);
            patch.select (kResFeedbackIn);
            juce::FileOutputStream out3 { juce::File (snapshotPath).getSiblingFile (juce::File (snapshotPath).getFileNameWithoutExtension() + "-depth.png") };
            out3.setPosition (0);
            out3.truncate();
            juce::PNGImageFormat().writeImageToStream (createComponentSnapshot (getLocalBounds(), true, 2.0f), out3);
            AudioSettings dialog (*this);                    // and the audio settings dialog, beside it
            dialog.setLookAndFeel (&look);
            juce::FileOutputStream out2 { juce::File (snapshotPath).getSiblingFile (juce::File (snapshotPath).getFileNameWithoutExtension() + "-audio.png") };
            out2.setPosition (0);
            out2.truncate();
            juce::PNGImageFormat().writeImageToStream (dialog.createComponentSnapshot (dialog.getLocalBounds(), true, 2.0f), out2);
            dialog.setLookAndFeel (nullptr);
            juce::JUCEApplication::quit();
            return;
        }
        if (ticks == 8) openAudio();            // a quarter of a second after the window appears
        if (ticks == 150 && roomAtStart) { roomButton.setToggleState (true, juce::dontSendNotification); setRoom (true); }   // --room (test aid): press ROOM after 5 s
        if (roomAtStart && measurePath.isEmpty() && statusLogPath.isEmpty() && instrument.roomEnabled.load() && roomListeningSince != 0 && juce::Time::getMillisecondCounter() - roomListeningSince > 10000)
        {
            roomAtStart = false;                                                 // ...and switch it off again 10 s after it starts listening
            roomButton.setToggleState (false, juce::dontSendNotification);
            setRoom (false);
            roomMessage = "ROOM switched off by the --room test.";
        }

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

        // PREVIOUS / NEXT / presets from MIDI (the buttons and the dial strip call selectStation directly)
        if (const int steps = instrument.navSteps.exchange (0); steps != 0) selectStation (((tuned + steps) % kNumStations + kNumStations) % kNumStations);
        if (const int slot = instrument.presetRequest.exchange (-1); slot >= 0) selectStation (slot);
        for (int i = 0; i < 2; ++i)
        {
            const int target = Instrument::kActionPrev + i;
            int cc = -1, note = -1;
            for (int n = 0; n < 128; ++n) { if (instrument.ccMap[(size_t) n].load() == target) cc = n; if (instrument.noteMap[(size_t) n].load() == target) note = n; }
            nav[(size_t) i].setButtonText (juce::String (i == 0 ? "<" : ">") + (instrument.learnTarget.load() == target ? " ?" : cc >= 0 ? " cc" + juce::String (cc) : note >= 0 ? " n" + juce::String (note) : ""));
        }

        const auto state = instrument.radio.state();
        dialView.stations = stations;
        dialView.tune = instrument.params[kTune].load();
        dialView.tuned = tuned;
        dialView.state = state;
        dialView.repaint();
        patch.repaint();

        juce::String text = juce::String (wanted != tuned ? "TUNING" : Radio::name (state)) + "  " + stations[(size_t) (wanted != tuned ? wanted : tuned)].name;
        if (instrument.engine.meterStatic.load() > 0.01f) text << "  |  static (synthesized)";
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
            if (learn.getToggleState()) h = "Touch a knob on screen, then move a control on your controller.";
            else if (patch.graph.cableInto (kResAudioIn) < 0) h = "Nothing is patched into RESONATOR IN: drag a cable from an AUDIO output.";
            else if (patch.graph.cableInto (kResAudioIn) == kLocalAudioOut) h = "LOCAL TEST SOURCE is playing. It is not live radio.";
            else if (roomMessage.isNotEmpty()) h = roomMessage;
            else h = "MIDI: " + juce::String (midiInputs) + " input(s), " + juce::String (instrument.midiEvents.load()) + " events";
            hint.setText (h, juce::dontSendNotification);
        }

        if (instrument.mapDirty.exchange (false)) { settingsDirty = true; learn.setToggleState (false, juce::dontSendNotification); }
        if (settingsDirty && now - lastSave > 2000) saveSettings();
        if (measurePath.isNotEmpty() && ticks > 8) stepMeasure();
        if (navTest)
        {
            if (ticks == 360 || ticks == 960) instrument.navSteps.fetch_add (ticks == 360 ? 1 : -1);            // exactly what a mapped MIDI button does
            if (ticks >= 660 && ticks < 670 && ticks % 2 == 0) instrument.navSteps.fetch_add (1);                // five presses in a third of a second
        }
        if (statusLogPath.isNotEmpty() && (ticks % 150 == 0 || (navTest && ticks % 6 == 0 && instrument.radio.state() != Radio::State::Playing)))
        {
            auto* device = devices.getCurrentAudioDevice();
            juce::File (statusLogPath).appendText (juce::String (ticks / 30.0, 1) + " s | " + (device != nullptr ? device->getName() : juce::String ("no device")) + " | " + Radio::name (instrument.radio.state())
                + " | buffer " + juce::String (instrument.radio.bufferedSeconds(), 1) + " s | underruns " + juce::String (instrument.radio.underruns.load()) + " | reconnects " + juce::String (instrument.radio.reconnects.load())
                + " | callbacks " + juce::String ((juce::int64) instrument.perf.callbacks.load()) + " | late " + juce::String ((juce::int64) instrument.perf.lateCallbacks.load()) + " | max gap ms " + juce::String (instrument.perf.maxGapMs.load(), 1)
                + " | faults " + juce::String (instrument.engine.faults.load())
                + " | " + stations[(size_t) tuned].name + " | gap " + juce::String (instrument.radio.tuneGap.load(), 2) + " | static " + juce::String (instrument.engine.meterStatic.load(), 2) + " | connects " + juce::String (instrument.radio.connects.load()) + " | ROOM " + (instrument.roomEnabled.load() ? "on" : "off") + " | " + roomMessage.upToFirstOccurrenceOf (".", false, false) + "\n");
        }
        if (switchTest && (ticks == 450 || ticks == 900))          // at 15 s and 30 s: the same call the AUDIO dialog makes
        {
            auto setup = devices.getAudioDeviceSetup();
            const auto before = setup.outputDeviceName;
            if (ticks == 450) switchBack = before;
            setup.outputDeviceName = ticks == 450 ? juce::String ("MacBook Pro Speakers") : switchBack;
            const auto error = devices.setAudioDeviceSetup (setup, true);
            if (statusLogPath.isNotEmpty()) juce::File (statusLogPath).appendText ("-- output " + before + " -> " + setup.outputDeviceName + (error.isEmpty() ? " ok" : " FAILED: " + error) + "\n");
        }
    }

    /** Shows the DEPTH strip for the cable into `input` (a control input), or hides it. */
    void showDepthFor (int input)
    {
        const int from = input >= 0 ? patch.graph.cableInto (input) : -1;
        const bool control = from >= 0 && ports[input].type == PortType::CV;
        depth.setVisible (control);
        depthRoute.setVisible (control);
        hint.setVisible (! control);
        if (control)
        {
            depth.setValue (std::round (patch.graph.depthInto (input) * 100.0f), juce::dontSendNotification);
            depthRoute.setText (juce::String ("DEPTH  ") + moduleNames[ports[from].module] + " " + ports[from].label + " > " + ports[input].label, juce::dontSendNotification);
            depthRoute.setColour (juce::Label::textColourId, moduleColour (ports[from].module));
        }
        else if (from >= 0) flash ("Audio cables have no depth. Select a round (control) cable.");
    }

    /** NEXT / PREVIOUS / a preset: jump straight to a station. The knob follows; the request goes out at once (the
        receiver itself coalesces a quick run of them into the last one). In MIDI LEARN the control is armed instead. */
    void selectStation (int slot)
    {
        instrument.params[kTune] = dialCentre (slot, kNumStations);
        wanted = tuned = slot;
        instrument.radio.tune (stations[(size_t) slot].url);
        urlEditor.setText (stations[(size_t) slot].url, false);
        settingsDirty = true;
    }
    void navigate (int target, int slot)
    {
        if (learn.getToggleState()) { instrument.learnTarget = target; return flash ("Now press the button or pad on your controller for this."); }
        selectStation (slot);
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

    // ---- ROOM. The microphone belongs to a separate helper app ("WILDBAND Room", src/RoomHelper.mm) with its own
    // macOS permission; this app never touches an input. The button starts and stops that helper, and the audio output
    // is not involved either way. The helper reports three control values over loopback UDP.
    void setRoom (bool on)
    {
        stopRoomHelper();
        instrument.roomEnabled = false;
        instrument.withdrawRoom();
        roomMessage.clear();
        roomDevice.clear();
        if (! on) return;

        const auto helper = juce::File::getSpecialLocation (juce::File::currentApplicationFile).getSiblingFile ("WILDBAND Room.app");   // beside this app, not inside it
        roomSocket = std::make_unique<juce::DatagramSocket>();
        if (! helper.exists() || ! roomSocket->bindToPort (0, "127.0.0.1"))
        {
            roomSocket.reset();
            roomButton.setToggleState (false, juce::dontSendNotification);
            roomMessage = "ROOM is unavailable (WILDBAND Room.app must sit beside WILDBAND.app). Playback is unaffected.";
            return;
        }
        // Started through LaunchServices ("open"), not as a child process, so that macOS holds the helper - not this
        // app - responsible for the microphone. Not in the background ("-g"): macOS then denies without asking (observed).
        juce::ChildProcess().start (juce::StringArray { "/usr/bin/open", "-n", "-a", helper.getFullPathName(), "--args",
                                                        "--port", juce::String (roomSocket->getBoundPort()), "--parent", juce::String (getpid()),
                                                        "--device", roomInputUid() }, 0);
        roomStarted = roomHeard = juce::Time::getMillisecondCounter();
        roomMessage = "Starting ROOM...";
    }

    void stopRoomHelper()
    {
        if (roomPid > 0) ::kill (roomPid, SIGTERM);      // capture stops with the process
        roomPid = 0;
        roomSocket.reset();
    }

    void roomOff (const juce::String& why)
    {
        stopRoomHelper();
        instrument.roomEnabled = false;
        instrument.withdrawRoom();
        roomButton.setToggleState (false, juce::dontSendNotification);
        roomDevice.clear();
        roomMessage = why;
    }

    /** Timer: read what the helper has sent. ponytail: polled at 30 Hz on the message thread, so room values reach the
        engine up to ~35 ms late; move to a socket thread if claps feel late. */
    void pollRoom()
    {
        if (roomSocket == nullptr) return;
        const auto now = juce::Time::getMillisecondCounter();
        char data[512];
        while (roomSocket != nullptr && roomSocket->waitUntilReady (true, 0) == 1)
        {
            const int n = roomSocket->read (data, (int) sizeof (data) - 1, false);
            if (n <= 0) break;
            const auto words = juce::StringArray::fromTokens (juce::String::fromUTF8 (data, n), " ", "");
            roomHeard = now;
            if (words[0] == "room" && words.size() >= 4)
            {
                instrument.publishRoom ({ words[1].getFloatValue(), words[2].getFloatValue(), words[3].getFloatValue() });   // clamped by EnvBus
                if (roomDevice.isNotEmpty())
                    roomMessage = "ROOM is listening on " + roomDevice + " (never played or stored).  level " + words[1].substring (0, 4) + "  hits " + words[2].substring (0, 4) + "  bright " + words[3].substring (0, 4) + "  (" + words[4] + " dBFS" + (words[5] == "0" ? ", NO AUDIO ARRIVING" : "") + ")";
            }
            else if (words[0] == "asking")
            {
                roomPid = words[1].getIntValue();
                roomMessage = "macOS is asking whether WILDBAND Room may use the microphone. The radio keeps playing either way.";
            }
            else if (words[0] == "listening")
            {
                roomPid = words[1].getIntValue();
                instrument.roomEnabled = true;
                roomListeningSince = now;
                juce::Process::makeForegroundProcess();      // the helper was launched in front; take the keyboard back
                roomDevice = words.joinIntoString (" ", 2);
                roomMessage = "ROOM is listening on " + roomDevice + " (never played or stored).";
            }
            else if (words[0] == "denied")
            {
                juce::Process::makeForegroundProcess();
                roomOff ("Microphone access is off, so ROOM stays neutral. To allow it: System Settings > Privacy & Security > Microphone > WILDBAND Room.");
            }
            else if (words[0] == "error")
                roomOff ("ROOM could not open the microphone (" + words.joinIntoString (" ", 1) + "). Playback is unaffected.");
        }
        // no word from the helper: it never started, or it has gone (a pending macOS prompt keeps it silent, so only
        // give up once it had begun listening, or if it never even reported in)
        if (roomSocket != nullptr && ((instrument.roomEnabled.load() && now - roomHeard > 3000) || (roomPid == 0 && now - roomStarted > 8000)))
            roomOff ("ROOM stopped (the WILDBAND Room helper is not running). Playback is unaffected.");
    }

    void openMidiInputs()
    {
        const auto inputs = juce::MidiInput::getAvailableDevices();
        for (const auto& d : inputs) devices.setMidiInputDeviceEnabled (d.identifier, true);
        devices.removeMidiInputDeviceCallback ({}, &instrument);
        devices.addMidiInputDeviceCallback ({}, &instrument);
        midiInputs = inputs.size();
    }

    /** The input ROOM listens to: the saved choice if it is connected; otherwise the interface being played through, if
        it has inputs; otherwise the system default input (an empty UID). */
    juce::String roomInputUid()
    {
        const auto saved = settings.getUserSettings()->getValue ("roomInput", "auto");
        const auto all = audioDevices();
        if (saved == "default") return {};
        for (const auto& d : all) if (saved == juce::String (d.uid) && d.inputChannels > 0) return saved;
        if (auto* output = devices.getCurrentAudioDevice())
            for (const auto& d : all) if (output->getName() == juce::String (d.name) && d.inputChannels > 0) return d.uid;
        return {};
    }

    /** Audio settings dialog: the output device (JUCE's selector) and, below it, which input ROOM listens to. */
    struct AudioSettings final : juce::Component
    {
        AudioSettings (MainComponent& m) : owner (m), selector (m.devices, 0, 0, 2, 2, false, false, true, false)
        {
            addAndMakeVisible (selector);
            addAndMakeVisible (label);
            addAndMakeVisible (input);
            addAndMakeVisible (note);
            label.setText ("ROOM input", juce::dontSendNotification);
            note.setText ("Used only while ROOM is on. It is analysed, never played or recorded.", juce::dontSendNotification);
            note.setFont (juce::FontOptions (12.0f));
            const auto current = owner.roomInputUid();
            input.addItem ("System default input", 1);
            uids.add ({});
            for (const auto& d : audioDevices())
                if (d.inputChannels > 0)
                {
                    uids.add (d.uid);
                    input.addItem (juce::String (d.name) + "  (" + juce::String (d.inputChannels) + " in)", uids.size());
                    if (current == juce::String (d.uid)) input.setSelectedId (uids.size(), juce::dontSendNotification);
                }
            if (input.getSelectedId() == 0) input.setSelectedId (1, juce::dontSendNotification);
            input.onChange = [this]
            {
                const auto uid = uids[input.getSelectedId() - 1];
                owner.settings.getUserSettings()->setValue ("roomInput", uid.isEmpty() ? juce::String ("default") : uid);
                owner.settingsDirty = true;
                if (owner.roomButton.getToggleState()) owner.setRoom (true);      // restart the helper on the new input; the output is untouched
            };
            setSize (480, 440);
        }
        void paint (juce::Graphics& g) override { g.fillAll (kPanel); }
        void resized() override
        {
            auto area = getLocalBounds().reduced (10);
            note.setBounds (area.removeFromBottom (22));
            auto row = area.removeFromBottom (28);
            label.setBounds (row.removeFromLeft (110));
            input.setBounds (row);
            area.removeFromBottom (8);
            selector.setBounds (area);
        }
        MainComponent& owner;
        juce::AudioDeviceSelectorComponent selector;
        juce::Label label, note;
        juce::ComboBox input;
        juce::StringArray uids;
    };

    void showAudioSettings()
    {
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new AudioSettings (*this));
        o.dialogTitle = "Audio settings";
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
        for (int target = kNumParams; target < Instrument::kNumTargets; ++target)
        {
            if (const int cc = p.getIntValue ("ccAction" + juce::String (target - kNumParams), -1); cc >= 0 && cc < 128) instrument.ccMap[(size_t) cc] = (int8_t) target;
            if (const int note = p.getIntValue ("noteAction" + juce::String (target - kNumParams), -1); note >= 0 && note < 128) instrument.noteMap[(size_t) note] = (int8_t) target;
        }
        instrument.params[kLevel] = juce::jmin (instrument.params[kLevel].load(), paramDefaults[kLevel]);   // never start loud
        // "cables" stays in P0's format so both builds can share one settings file; depths live under their own key
        if (p.containsKey ("cables")) patch.graph = loadPatch (p.getValue ("cables").toStdString(), p.getValue ("cableDepths").toStdString());
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
        for (int target = kNumParams; target < Instrument::kNumTargets; ++target)
        {
            int cc = -1, note = -1;
            for (int n = 0; n < 128; ++n) { if (instrument.ccMap[(size_t) n].load() == target) cc = n; if (instrument.noteMap[(size_t) n].load() == target) note = n; }
            p.setValue ("ccAction" + juce::String (target - kNumParams), cc);
            p.setValue ("noteAction" + juce::String (target - kNumParams), note);
        }
        p.setValue ("cables", juce::String (savePatch (patch.graph)));
        p.setValue ("cableDepths", juce::String (savePatch (patch.graph, true)));
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
                   << "| rate | buffer (asked/got) | period ms | callbacks | mean us | p99.9 us | max us | max % of period | late callbacks | max host-time gap ms | radio underruns | reconnects | engine faults | radio LIVE % of window | mean radio level 0..1 | ROOM on % of window | mean room level 0..1 |\n|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
        measureOriginal = devices.getAudioDeviceSetup();
        measureIndex = -1;
        measureNext = juce::Time::getMillisecondCounter() + 6000;      // let the stream start first
    }
    void stepMeasure()
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (! measureSettling)      // what the engine was actually fed during the window, sampled 30 times a second
        {
            ++measureTicks;
            measureLive += instrument.radio.state() == Radio::State::Playing;
            measureRadioLevel += instrument.engine.meterRadioLevel.load();
            measureRoom += instrument.roomEnabled.load();
            measureRoomLevel += instrument.roomLevel.load();
        }
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
        if (measureSettling) { measureTicks = measureLive = measureRoom = 0; measureRadioLevel = measureRoomLevel = 0; instrument.perf.reset(); underruns0 = instrument.radio.underruns.load(); reconnects0 = instrument.radio.reconnects.load(); measureSettling = false; measureNext = now + 20000; return; }
        if (measureIndex >= 0)
        {
            const auto& step = measureSteps.getReference (measureIndex);
            const auto& perf = instrument.perf;
            const double fs = instrument.sampleRate.load(), period = fs > 0 ? 1000.0 * instrument.blockSize.load() / fs : 0, n = (double) juce::jmax<uint64_t> (1, perf.callbacks.load());
            measureLog << "| " << step.rate << " (" << fs << ") | " << step.buffer << " / " << instrument.blockSize.load() << " | " << juce::String (period, 3) << " | " << (juce::int64) perf.callbacks.load()
                       << " | " << juce::String (perf.sumUs.load() / n, 1) << " | " << juce::roundToInt (perf.percentileUs (0.999)) << " | " << juce::roundToInt (perf.maxUs.load())
                       << " | " << juce::String (period > 0 ? perf.maxUs.load() / (period * 10.0) : 0.0, 1) << " | " << (juce::int64) perf.lateCallbacks.load() << " | " << juce::String (perf.maxGapMs.load(), 2)
                       << " | " << instrument.radio.underruns.load() - underruns0 << " | " << instrument.radio.reconnects.load() - reconnects0 << " | " << instrument.engine.faults.load()
                       << " | " << juce::roundToInt (100.0 * measureLive / juce::jmax (1, measureTicks)) << " | " << juce::String (measureRadioLevel / juce::jmax (1, measureTicks), 2)
                       << " | " << juce::roundToInt (100.0 * measureRoom / juce::jmax (1, measureTicks)) << " | " << juce::String (measureRoomLevel / juce::jmax (1, measureTicks), 2) << " |\n";
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
    std::array<juce::TextButton, 2> nav;
    juce::TextEditor urlEditor;
    juce::Label title, status, hint, depthRoute;
    juce::Slider depth;
    StationDial dialView;
    PatchBay patch;

    std::array<Station, kNumStations> stations;
    int wanted = 0, tuned = 0, midiInputs = 0;
    juce::uint32 wantedSince = 0, flashUntil = 0, lastSave = 0;
    bool settingsDirty = false;
    int ticks = 0;
    juce::String roomMessage, roomDevice, outputOverride, snapshotPath, switchBack;
    std::unique_ptr<juce::DatagramSocket> roomSocket;
    int roomPid = 0;
    juce::uint32 roomStarted = 0, roomHeard = 0, roomListeningSince = 0;

    juce::String measurePath, measureLog;
    const bool transient;
    juce::Array<MeasureStep> measureSteps;
    juce::AudioDeviceManager::AudioDeviceSetup measureOriginal;
    int measureIndex = -1, underruns0 = 0, reconnects0 = 0;
    bool measureSettling = false, measureStalling = false;
    int measureTicks = 0, measureLive = 0, measureRoom = 0;
    double measureRadioLevel = 0, measureRoomLevel = 0;
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
        for (const auto& arg : juce::StringArray::fromTokens (commandLine, true))
            if (arg.startsWith ("--probe="))       // diagnostic: which CoreAudio operation makes macOS ask for the microphone
            {
                probeMicrophoneTrigger (arg.fromFirstOccurrenceOf ("=", false, false).unquoted().toStdString(), []
                {
                    const auto a = microphoneAccess();
                    return a == MicAccess::Granted ? "granted" : a == MicAccess::Denied ? "denied" : "undetermined";
                });
                return quit();
            }

        roomAtStart = commandLine.contains ("--room");
        switchTest = commandLine.contains ("--switch-test");
        navTest = commandLine.contains ("--nav-test");
        for (const auto& arg : juce::StringArray::fromTokens (commandLine, true))
            if (arg.startsWith ("--log=")) statusLogPath = arg.fromFirstOccurrenceOf ("=", false, false).unquoted();
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
