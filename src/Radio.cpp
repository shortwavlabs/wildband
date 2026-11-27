#include "Radio.h"

#include <AudioToolbox/AudioToolbox.h>

namespace wildband
{

namespace
{
constexpr OSStatus kNeedMoreInput = 'more';
constexpr double kPrebufferSeconds = 0.75, kFadeSeconds = 0.02, kMaxBufferedSeconds = 6.0;
}

/** One connection's decoder: AudioFileStream splits the bytes into packets, AudioConverter turns packets into
    float PCM at the device rate. */
struct Radio::Session
{
    Radio& radio;
    const double fs;
    const int myGen;
    AudioFileStreamID parser = nullptr;
    AudioConverterRef converter = nullptr;
    AudioStreamBasicDescription source {};
    int channels = 2;
    bool failed = false;
    juce::String error;
    std::vector<float> pcm = std::vector<float> (4096);

    const void* inBytes = nullptr;                    // packets handed to the converter by onPackets
    AudioStreamPacketDescription* inDescs = nullptr;
    UInt32 inCount = 0, inIndex = 0;
    AudioStreamPacketDescription currentDesc {};

    Session (Radio& r, double outputRate, int g) : radio (r), fs (outputRate), myGen (g) {}
    ~Session()
    {
        if (converter != nullptr) AudioConverterDispose (converter);
        if (parser != nullptr) AudioFileStreamClose (parser);
    }
    void fail (const juce::String& why) { if (! failed) { failed = true; error = why; } }

    static void onProperty (void* user, AudioFileStreamID stream, AudioFileStreamPropertyID id, AudioFileStreamPropertyFlags*)
    {
        auto& s = *static_cast<Session*> (user);
        if (id != kAudioFileStreamProperty_ReadyToProducePackets || s.converter != nullptr) return;

        UInt32 size = sizeof (s.source);
        if (AudioFileStreamGetProperty (stream, kAudioFileStreamProperty_DataFormat, &size, &s.source) != noErr) return s.fail ("unreadable stream format");
        // HE-AAC announces itself in the format list; its first entry is the most capable decode of the stream
        AudioFormatListItem formats[8];
        size = sizeof (formats);
        if (AudioFileStreamGetProperty (stream, kAudioFileStreamProperty_FormatList, &size, formats) == noErr && size >= sizeof (AudioFormatListItem))
            s.source = formats[0].mASBD;

        s.channels = s.source.mChannelsPerFrame >= 2 ? 2 : 1;
        AudioStreamBasicDescription out {};
        out.mSampleRate = s.fs;
        out.mFormatID = kAudioFormatLinearPCM;
        out.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        out.mChannelsPerFrame = (UInt32) s.channels;
        out.mBitsPerChannel = 32;
        out.mBytesPerFrame = out.mBytesPerPacket = 4 * (UInt32) s.channels;
        out.mFramesPerPacket = 1;
        if (AudioConverterNew (&s.source, &out, &s.converter) != noErr) return s.fail ("no system decoder for this stream");

        UInt32 cookieSize = 0;
        Boolean writable = false;
        if (AudioFileStreamGetPropertyInfo (stream, kAudioFileStreamProperty_MagicCookieData, &cookieSize, &writable) == noErr && cookieSize > 0)
        {
            std::vector<char> cookie (cookieSize);
            if (AudioFileStreamGetProperty (stream, kAudioFileStreamProperty_MagicCookieData, &cookieSize, cookie.data()) == noErr)
                AudioConverterSetProperty (s.converter, kAudioConverterDecompressionMagicCookie, cookieSize, cookie.data());
        }

        const char* codec = s.source.mFormatID == kAudioFormatMPEGLayer3 ? "MP3" : s.source.mFormatID == kAudioFormatMPEG4AAC ? "AAC"
                          : s.source.mFormatID == kAudioFormatMPEG4AAC_HE ? "HE-AAC" : s.source.mFormatID == kAudioFormatMPEG4AAC_HE_V2 ? "HE-AAC v2" : "audio";
        s.radio.setDetail (juce::String (codec) + " " + juce::String (s.source.mSampleRate / 1000.0, 1) + " kHz " + (s.source.mChannelsPerFrame >= 2 ? "stereo" : "mono"));
    }

    static OSStatus provideInput (AudioConverterRef, UInt32* numPackets, AudioBufferList* data, AudioStreamPacketDescription** desc, void* user)
    {
        auto& s = *static_cast<Session*> (user);
        if (s.inIndex >= s.inCount) { *numPackets = 0; return kNeedMoreInput; }
        s.currentDesc = s.inDescs[s.inIndex++];
        data->mNumberBuffers = 1;
        data->mBuffers[0].mNumberChannels = s.source.mChannelsPerFrame;
        data->mBuffers[0].mData = const_cast<char*> (static_cast<const char*> (s.inBytes) + s.currentDesc.mStartOffset);
        data->mBuffers[0].mDataByteSize = s.currentDesc.mDataByteSize;
        s.currentDesc.mStartOffset = 0;
        if (desc != nullptr) *desc = &s.currentDesc;
        *numPackets = 1;
        return noErr;
    }

    static void onPackets (void* user, UInt32, UInt32 numPackets, const void* bytes, AudioStreamPacketDescription* descs)
    {
        auto& s = *static_cast<Session*> (user);
        if (s.failed || s.converter == nullptr) return;
        if (descs == nullptr) return s.fail ("unsupported packet layout");
        s.inBytes = bytes; s.inDescs = descs; s.inCount = numPackets; s.inIndex = 0;
        for (;;)
        {
            UInt32 frames = (UInt32) (s.pcm.size() / (size_t) s.channels);
            AudioBufferList out;
            out.mNumberBuffers = 1;
            out.mBuffers[0].mNumberChannels = (UInt32) s.channels;
            out.mBuffers[0].mDataByteSize = (UInt32) (s.pcm.size() * sizeof (float));
            out.mBuffers[0].mData = s.pcm.data();
            const OSStatus status = AudioConverterFillComplexBuffer (s.converter, provideInput, &s, &frames, &out, nullptr);
            if (frames > 0 && ! s.radio.push (s.pcm.data(), (int) frames, s.channels, s.myGen)) return;   // superseded
            if (status == kNeedMoreInput || frames == 0) return;
            if (status != noErr) return s.fail ("decoder error " + juce::String ((int) status));
        }
    }
};

// ---------------------------------------------------------------------------------------------------------------------
Radio::Radio() : juce::Thread ("WILDBAND radio")
{
    ringL.calloc (kCapacity);
    ringR.calloc (kCapacity);
    startThread();
}

Radio::~Radio()
{
    signalThreadShouldExit();
    { const juce::ScopedLock sl (lock); if (activeStream != nullptr) activeStream->cancel(); }
    notify();
    stopThread (4000);
}

void Radio::tune (const juce::String& newUrl)
{
    {
        const juce::ScopedLock sl (lock);
        if (newUrl == url) return;
        url = newUrl;
        detailText.clear();
        ++gen;
        phase = url.isEmpty() ? Phase::Idle : Phase::Connecting;
        if (activeStream != nullptr) activeStream->cancel();
    }
    notify();
}

void Radio::setOutputRate (double fs)
{
    if (juce::approximatelyEqual (fs, rate.load()) || fs <= 0) return;
    {
        const juce::ScopedLock sl (lock);
        rate = fs;
        ++gen;
        if (activeStream != nullptr) activeStream->cancel();
    }
    notify();
}

void Radio::setDetail (const juce::String& text) { const juce::ScopedLock sl (lock); detailText = text; }
juce::String Radio::detail() const { const juce::ScopedLock sl (lock); return detailText; }
double Radio::bufferedSeconds() const noexcept { return fifo.getNumReady() / rate.load(); }

Radio::State Radio::state() const noexcept
{
    switch (phase.load())
    {
        case Phase::Idle:         return State::Idle;
        case Phase::Connecting:   return State::Connecting;
        case Phase::Streaming:    return readerPlaying.load() ? State::Playing : State::Buffering;
        case Phase::Reconnecting: return readerPlaying.load() ? State::Playing : State::Reconnecting;  // still playing out the buffer
        case Phase::Failed:       return State::Failed;
    }
    return State::Idle;
}

const char* Radio::name (State s) noexcept
{
    static constexpr const char* names[] = { "IDLE", "CONNECTING", "BUFFERING", "LIVE", "RECONNECTING", "FAILED" };
    return names[(int) s];
}

// ---- audio thread ---------------------------------------------------------------------------------------------------
void Radio::read (float* left, float* right, int n) noexcept
{
    const double fs = rate.load();
    const int fade = (int) (kFadeSeconds * fs), g = gen.load(), sg = sessionGen.load();

    if (readerGen != g) playing = false;                                  // retuned: fade out now
    if (readerGen != sg && sg == g && gain <= 0.0f)                       // the new session is up and the old audio has faded:
    {                                                                     // drop what is left of the old station
        fifo.finishedRead (fifo.getNumReady());
        readerGen = sg;
        ackGen = sg;
    }

    int ready = fifo.getNumReady();
    if (readerGen == g)
    {
        if (! playing && ready >= (int) (kPrebufferSeconds * fs)) playing = true;
        else if (playing && ready < n + fade) { playing = false; underruns.fetch_add (1); }   // fade before the ring runs dry
    }

    const int take = (playing || gain > 0.0f) ? juce::jmin (n, ready) : 0;
    if (take > 0)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (take, start1, size1, start2, size2);
        std::copy_n (ringL + start1, size1, left);  std::copy_n (ringL + start2, size2, left + size1);
        std::copy_n (ringR + start1, size1, right); std::copy_n (ringR + start2, size2, right + size1);
        fifo.finishedRead (take);
    }
    std::fill (left + take, left + n, 0.0f);
    std::fill (right + take, right + n, 0.0f);

    const float step = 1.0f / (float) juce::jmax (1, fade), target = playing ? 1.0f : 0.0f;
    for (int i = 0; i < n; ++i)
    {
        gain = target > gain ? juce::jmin (target, gain + step) : juce::jmax (target, gain - step);
        left[i] *= gain;
        right[i] *= gain;
    }
    readerPlaying = playing;
}

// ---- receiver thread ------------------------------------------------------------------------------------------------
bool Radio::push (const float* interleaved, int frames, int channels, int myGen)
{
    // wait for the audio thread to flush the previous station, then for room: this back-pressure is what bounds the ring
    const int limit = (int) (kMaxBufferedSeconds * rate.load());
    while (ackGen.load() != myGen || fifo.getNumReady() + frames > limit)
    {
        if (! current (myGen)) return false;
        wait (10);
    }
    int start1, size1, start2, size2;
    fifo.prepareToWrite (frames, start1, size1, start2, size2);
    for (int i = 0; i < size1 + size2; ++i)
    {
        const int at = i < size1 ? start1 + i : start2 + i - size1;
        ringL[at] = interleaved[i * channels];
        ringR[at] = interleaved[i * channels + channels - 1];
    }
    fifo.finishedWrite (size1 + size2);
    gotAudio = true;
    phase = Phase::Streaming;
    return true;
}

void Radio::run()
{
    int backoffMs = 500;
    while (! threadShouldExit())
    {
        juce::String u;
        int myGen;
        { const juce::ScopedLock sl (lock); u = url; myGen = gen.load(); }
        if (u.isEmpty()) { sessionGen = myGen; phase = Phase::Idle; wait (-1); continue; }

        sessionGen = myGen;
        gotAudio = false;
        if (phase.load() != Phase::Reconnecting) phase = Phase::Connecting;
        const End end = runSession (u, rate.load(), myGen, 0);
        if (! current (myGen)) { backoffMs = 500; phase = Phase::Connecting; continue; }

        if (end == End::Fatal) { phase = Phase::Failed; wait (-1); continue; }
        if (gotAudio.load()) backoffMs = 500;                // it worked for a while: start the back-off over
        phase = Phase::Reconnecting;
        reconnects.fetch_add (1);
        wait (backoffMs);
        backoffMs = juce::jmin (8000, backoffMs * 2);
    }
}

Radio::End Radio::runSession (const juce::String& address, double fs, int myGen, int depth)
{
    juce::WebInputStream stream (juce::URL (address), false);
    stream.withExtraHeaders ("Icy-MetaData: 0\r\nUser-Agent: WILDBAND/0.1 (P0 prototype)");   // no in-band metadata: nothing to strip
    stream.withConnectionTimeout (8000);
    stream.withNumRedirectsToFollow (5);

    struct Active
    {
        Radio& r;
        Active (Radio& radio, juce::WebInputStream* s) : r (radio) { const juce::ScopedLock sl (r.lock); r.activeStream = s; }
        ~Active() { const juce::ScopedLock sl (r.lock); r.activeStream = nullptr; }
    } active (*this, &stream);

    if (! current (myGen)) return End::Superseded;
    if (! stream.connect (nullptr)) { if (current (myGen)) setDetail ("could not connect"); return End::Retry; }
    const int status = stream.getStatusCode();
    if (status != 200)
    {
        setDetail ("HTTP " + juce::String (status));
        return status >= 400 && status < 500 ? End::Fatal : End::Retry;
    }

    const auto headers = stream.getResponseHeaders();
    juce::String type, station;
    int metaInterval = 0;
    for (const auto& key : headers.getAllKeys())
    {
        if (key.equalsIgnoreCase ("content-type")) type = headers[key].toLowerCase();
        if (key.equalsIgnoreCase ("icy-name")) station = headers[key];
        if (key.equalsIgnoreCase ("icy-metaint")) metaInterval = headers[key].getIntValue();   // sent although we asked for none
    }
    const auto path = juce::URL (address).getSubPath().toLowerCase();

    // ---- one level of playlist indirection (.m3u / .pls). HLS is a different transport and is not supported.
    if (type.contains ("mpegurl") || type.contains ("scpls") || path.endsWith (".m3u") || path.endsWith (".pls") || path.endsWith (".m3u8"))
    {
        juce::MemoryBlock text;
        stream.readIntoMemoryBlock (text, 16384);
        const auto body = text.toString();
        if (body.contains ("#EXT-X-")) { setDetail ("HLS playlists are not supported"); return End::Fatal; }
        for (auto line : juce::StringArray::fromLines (body))
        {
            line = line.trim();
            if (line.startsWithIgnoreCase ("File")) line = line.fromFirstOccurrenceOf ("=", false, false).trim();   // .pls
            if (line.startsWithIgnoreCase ("http") && depth < 2) return runSession (line, fs, myGen, depth + 1);
        }
        setDetail ("playlist has no stream address");
        return End::Fatal;
    }

    AudioFileTypeID hint = 0;
    if (type.contains ("mpeg") || type.contains ("mp3") || type.isEmpty()) hint = kAudioFileMP3Type;   // no type at all: legacy Shoutcast, assume MP3
    else if (type.contains ("aac")) hint = kAudioFileAAC_ADTSType;
    else if (type.contains ("ogg") || type.contains ("opus") || type.contains ("flac") || type.contains ("vorbis"))
    {
        setDetail ("unsupported stream type (" + type + ")");
        return End::Fatal;
    }

    Session session (*this, fs, myGen);
    if (AudioFileStreamOpen (&session, Session::onProperty, Session::onPackets, hint, &session.parser) != noErr)
    {
        setDetail ("could not open the system stream parser");
        return End::Fatal;
    }

    char buffer[4096];
    juce::int64 total = 0;
    int untilMeta = metaInterval, metaSkip = 0;
    while (current (myGen))
    {
        int n = stream.read (buffer, (int) sizeof (buffer));
        if (n <= 0) break;                                                    // closed, stalled past the timeout, or cancelled
        total += n;
        if (metaInterval > 0)                                                 // cut the interleaved metadata blocks out
        {
            int kept = 0;
            for (int i = 0; i < n; ++i)
            {
                if (metaSkip > 0) { --metaSkip; continue; }
                if (untilMeta == 0) { metaSkip = 16 * (int) (unsigned char) buffer[i]; untilMeta = metaInterval; continue; }
                buffer[kept++] = buffer[i];
                --untilMeta;
            }
            if ((n = kept) == 0) continue;
        }
        if (AudioFileStreamParseBytes (session.parser, (UInt32) n, buffer, 0) != noErr) session.fail ("not a decodable MP3/AAC stream");
        if (session.converter == nullptr && total > 512 * 1024) session.fail ("no audio found in the first 512 kB (" + type + ")");
        if (session.failed) { setDetail (session.error); return gotAudio.load() ? End::Retry : End::Fatal; }
        if (session.converter != nullptr && station.isNotEmpty()) { setDetail (detail().upToFirstOccurrenceOf (" - ", false, false) + " - " + station); station.clear(); }
    }
    if (! current (myGen)) return End::Superseded;
    if (! gotAudio.load()) setDetail ("stream ended before any audio (" + type + ")");
    return End::Retry;
}

} // namespace wildband
