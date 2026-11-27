// "WILDBAND Room": the only WILDBAND process that may use the microphone.
//
// Why a separate app: macOS shows the microphone prompt to any app that declares microphone use as soon as it opens
// an audio device that has inputs - even for playback only (docs/p0/EVIDENCE-LOG.md, "Microphone prompt"). So the
// instrument itself declares no microphone use and can never cause that prompt, and this helper, which has its own
// identity and its own permission, is started by the ROOM button and by nothing else.
//
// It asks for permission, analyses the default input (level, onsets, brightness - src/Engine.h RoomAnalyser) and
// sends those three numbers to the instrument 50 times a second as text datagrams on the loopback interface. No
// audio leaves this process. It exits when told to (SIGTERM) or when the instrument is gone.
//
//   "WILDBAND Room" --port UDP_PORT --parent PID [--device UID]      (no --device: the system default input)
//   datagrams:  asking PID | denied | error TEXT | listening PID DEVICE NAME | room LEVEL HITS BRIGHT RAW_DBFS CALLBACKS
#include "AudioIO.h"
#include "Engine.h"
#include "MicPermission.h"

#include <CoreFoundation/CoreFoundation.h>
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

using namespace wildband;

namespace
{
int sock = -1;
sockaddr_in instrument {};
pid_t parent = 0;
std::string wantedUid;       // empty: follow the system default input
HalUnit microphone;
RoomAnalyser analyser;
std::atomic<float> level { 0 }, activity { 0 }, brightness { 0 }, rawDb { -200 };   // rawDb: block RMS, for the readout only
std::atomic<bool> listening { false };

void say (const char* format, ...)
{
    char text[512];
    va_list args;
    va_start (args, format);
    const int n = std::vsnprintf (text, sizeof (text), format, args);
    va_end (args);
    if (n > 0) sendto (sock, text, (size_t) std::min (n, (int) sizeof (text) - 1), 0, reinterpret_cast<const sockaddr*> (&instrument), sizeof (instrument));
}

[[noreturn]] void leave (const char* format, const char* detail = "")
{
    say (format, detail);
    std::exit (0);
}

void listen()
{
    listening = false;
    auto device = defaultAudioDevice (true);
    if (! wantedUid.empty())
    {
        device = 0;
        for (const auto& d : audioDevices()) if (d.uid == wantedUid && d.inputChannels > 0) device = d.id;
        if (device == 0) leave ("error the chosen input device is not connected");
    }
    if (device == 0) leave ("error no input device");
    analyser.prepare (deviceSampleRate (device));
    const auto error = microphone.open (device, true, [] (float* samples, float*, int n, uint64_t)
    {
        const auto frame = analyser.process (samples, n);       // the samples go no further than this line
        level = frame.level; activity = frame.activity; brightness = frame.brightness;
        double sum = 0;
        for (int i = 0; i < n; ++i) sum += (double) samples[i] * samples[i];
        rawDb = (float) (10.0 * std::log10 (sum / std::max (1, n) + 1.0e-20));
    });
    if (! error.empty()) leave ("error %s", error.c_str());
    std::string name = "the default input";
    for (const auto& d : audioDevices()) if (d.id == device) name = d.name;
    say ("listening %d %s", (int) getpid(), name.c_str());
    listening = true;
}

void tick (CFRunLoopTimerRef, void*)
{
    static int count = 0;
    if (kill (parent, 0) != 0 && errno == ESRCH) std::exit (0);                    // the instrument is gone
    if (! listening.load()) return;
    if (++count % 50 == 0 && ((wantedUid.empty() && defaultAudioDevice (true) != microphone.device())   // the input changed under us: follow it
                              || deviceSampleRate (microphone.device()) != microphone.sampleRate())) listen();
    say ("room %.4f %.4f %.4f %.0f %d", (double) level.load(), (double) activity.load(), (double) brightness.load(), (double) rawDb.load(), microphone.calls.load());
}
} // namespace

int main (int argc, char** argv)
{
    int port = 0;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::strcmp (argv[i], "--port") == 0) port = std::atoi (argv[i + 1]);
        if (std::strcmp (argv[i], "--parent") == 0) parent = (pid_t) std::atoi (argv[i + 1]);
        if (std::strcmp (argv[i], "--device") == 0) wantedUid = argv[i + 1];
    }
    if (port <= 0 || parent <= 0) return 2;        // only the instrument starts this

    sock = socket (AF_INET, SOCK_DGRAM, 0);
    instrument.sin_family = AF_INET;
    instrument.sin_port = htons ((uint16_t) port);
    instrument.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    say ("asking %d", (int) getpid());

    requestMicrophone ([] (bool granted)            // the one place WILDBAND asks macOS for the microphone
    {
        if (! granted) leave ("denied");
        listen();
    });

    CFRunLoopAddTimer (CFRunLoopGetMain(), CFRunLoopTimerCreate (nullptr, CFAbsoluteTimeGetCurrent() + 0.02, 0.02, 0, 0, tick, nullptr), kCFRunLoopCommonModes);
    CFRunLoopRun();
    return 0;
}
