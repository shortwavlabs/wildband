// WILDBAND P0 radio receiver: one HTTP(S) Icecast/Shoutcast-style stream -> AudioToolbox decode and resample ->
// bounded PCM ring -> audio thread. Everything except read() runs on the receiver's own thread.
// What is and is not supported: docs/p0/RADIO-COMPATIBILITY.md
#pragma once

#include <juce_core/juce_core.h>

namespace wildband
{

class Radio final : private juce::Thread
{
public:
    enum class State { Idle, Connecting, Buffering, Playing, Reconnecting, Failed };

    Radio();
    ~Radio() override;

    /** Any non-audio thread. Switches to `url` (empty = stop); whatever is playing fades out at once. */
    void tune (const juce::String& url);
    /** Sample rate read() must deliver. A change restarts the stream; the audio thread is never involved. */
    void setOutputRate (double fs);

    /** Audio thread only: always fills n samples; silence while nothing is playing. Wait-free. */
    void read (float* left, float* right, int n) noexcept;

    State state() const noexcept;
    juce::String detail() const;                 // codec and station on success, the reason on failure
    double bufferedSeconds() const noexcept;
    std::atomic<int> underruns { 0 }, reconnects { 0 }, connects { 0 };   // connects: connection attempts started

    /** How much of the sound is missing because the receiver is fetching a station the player just asked for:
        0 while a station plays, rising to 1 as the old one fades, back to 0 as the new one fades in. Stays 0 for a
        spontaneous dropout of a station that was playing, and when idle or failed. The tuner static follows this,
        so it lasts exactly as long as the real wait. Written by read(). */
    std::atomic<float> tuneGap { 0.0f };
    /** Changes with every request (tune or sample-rate change). The engine restarts the static's time limit on it. */
    int request() const noexcept { return gen.load (std::memory_order_relaxed); }

    static const char* name (State) noexcept;

private:
    struct Session;
    enum class Phase { Idle, Connecting, Streaming, Reconnecting, Failed };
    enum class End { Superseded, Retry, Fatal };

    void run() override;
    End runSession (const juce::String& url, double fs, int myGen, int depth);
    bool push (const float* interleaved, int frames, int channels, int myGen);
    void setDetail (const juce::String&);
    bool current (int myGen) const noexcept { return ! threadShouldExit() && gen.load() == myGen; }

    static constexpr int kCapacity = 192000 * 8;     // ring size; 6 s of it is used at any rate
    juce::AbstractFifo fifo { kCapacity };
    juce::HeapBlock<float> ringL, ringR;

    juce::CriticalSection lock;                      // url, detailText, activeStream; never taken by read()
    juce::String url, detailText;
    juce::WebInputStream* activeStream = nullptr;

    std::atomic<int> gen { 0 };                      // bumped by tune() / setOutputRate()
    std::atomic<int> sessionGen { -1 };              // generation the receiver thread is now decoding
    std::atomic<int> ackGen { -1 };                  // generation the audio thread has flushed for
    std::atomic<double> rate { 48000.0 };
    std::atomic<Phase> phase { Phase::Idle };
    std::atomic<bool> readerPlaying { false }, gotAudio { false };

    static constexpr int kSettleMs = 150;            // requests closer together than this are coalesced into the last one
    std::atomic<bool> burst { false };
    std::atomic<juce::uint32> lastTuneMs { 0 };

    // audio thread state
    int readerGen = -1;
    bool playing = false, playedThisGen = false;
    float gain = 0.0f;
};

} // namespace wildband
