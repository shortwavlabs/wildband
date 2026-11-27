#include "AudioIO.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mach/mach_time.h>
#include <thread>

namespace wildband
{

namespace
{
template <typename T>
bool getProperty (AudioObjectID object, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope, T& value)
{
    const AudioObjectPropertyAddress address { selector, scope, kAudioObjectPropertyElementMain };
    UInt32 size = sizeof (T);
    return AudioObjectGetPropertyData (object, &address, 0, nullptr, &size, &value) == noErr;
}

template <typename T>
bool setProperty (AudioObjectID object, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope, const T& value)
{
    const AudioObjectPropertyAddress address { selector, scope, kAudioObjectPropertyElementMain };
    return AudioObjectSetPropertyData (object, &address, 0, nullptr, sizeof (T), &value) == noErr;
}

std::string toString (CFStringRef s)
{
    if (s == nullptr) return {};
    char buffer[512] = {};
    CFStringGetCString (s, buffer, sizeof (buffer), kCFStringEncodingUTF8);
    CFRelease (s);
    return buffer;
}

int channelCount (AudioObjectID device, AudioObjectPropertyScope scope)
{
    const AudioObjectPropertyAddress address { kAudioDevicePropertyStreamConfiguration, scope, kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize (device, &address, 0, nullptr, &size) != noErr || size == 0) return 0;
    std::vector<char> storage (size);
    auto* list = reinterpret_cast<AudioBufferList*> (storage.data());
    if (AudioObjectGetPropertyData (device, &address, 0, nullptr, &size, list) != noErr) return 0;
    int n = 0;
    for (UInt32 i = 0; i < list->mNumberBuffers; ++i) n += (int) list->mBuffers[i].mNumberChannels;
    return n;
}

uint64_t hostNs (uint64_t hostTime)
{
    static const mach_timebase_info_data_t base = [] { mach_timebase_info_data_t b; mach_timebase_info (&b); return b; }();
    return hostTime * base.numer / base.denom;
}
} // namespace

std::vector<AudioDeviceInfo> audioDevices()
{
    std::vector<AudioDeviceInfo> result;
    const AudioObjectPropertyAddress address { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize (kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr) return result;
    std::vector<AudioObjectID> ids (size / sizeof (AudioObjectID));
    if (AudioObjectGetPropertyData (kAudioObjectSystemObject, &address, 0, nullptr, &size, ids.data()) != noErr) return result;
    for (const auto id : ids)
    {
        AudioDeviceInfo info;
        info.id = id;
        CFStringRef name = nullptr, uid = nullptr;
        if (getProperty (id, kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, name)) info.name = toString (name);
        if (getProperty (id, kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, uid)) info.uid = toString (uid);
        info.inputChannels = channelCount (id, kAudioObjectPropertyScopeInput);
        info.outputChannels = channelCount (id, kAudioObjectPropertyScopeOutput);
        result.push_back (info);
    }
    return result;
}

uint32_t defaultAudioDevice (bool input)
{
    AudioObjectID id = 0;
    getProperty (kAudioObjectSystemObject, input ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, id);
    return id;
}

double deviceSampleRate (uint32_t device) { Float64 r = 0; getProperty (device, kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, r); return r; }
int deviceBufferSize (uint32_t device) { UInt32 n = 0; getProperty (device, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, n); return (int) n; }
bool setDeviceSampleRate (uint32_t device, double r) { return setProperty (device, kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, (Float64) r); }
bool setDeviceBufferSize (uint32_t device, int frames) { return setProperty (device, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, (UInt32) frames); }

// ---------------------------------------------------------------------------------------------------------------------
struct HalUnit::Procs
{
    static OSStatus render (void* user, AudioUnitRenderActionFlags*, const AudioTimeStamp* time, UInt32, UInt32 frames, AudioBufferList* data)
    {
        auto& u = *static_cast<HalUnit*> (user);
        auto* left = static_cast<float*> (data->mBuffers[0].mData);
        auto* right = data->mNumberBuffers > 1 ? static_cast<float*> (data->mBuffers[1].mData) : u.scratch.data();
        if (left == nullptr || frames > u.scratch.size()) return noErr;
        u.callback (left, right, (int) frames, hostNs (time->mHostTime));
        return noErr;
    }
};

std::string HalUnit::open (uint32_t dev, bool input, Callback cb)
{
    close();
    if (dev == 0) return "no audio device";
    callback = std::move (cb);
    isInput = input;
    scratch.assign (8192, 0.0f);
    calls = 0;

    if (input)      // a plain IO proc on the device: first input stream, channels summed to mono
    {
        rate = deviceSampleRate (dev);
        if (rate <= 0 || channelCount (dev, kAudioObjectPropertyScopeInput) <= 0) return "the device has no inputs";
        AudioDeviceIOProcID proc = nullptr;
        const OSStatus created = AudioDeviceCreateIOProcIDWithBlock (&proc, dev, nullptr,
            ^(const AudioTimeStamp* now, const AudioBufferList* in, const AudioTimeStamp*, AudioBufferList* out, const AudioTimeStamp*)
            {
                if (out != nullptr)                                     // this proc plays nothing: silence on any output streams
                    for (UInt32 b = 0; b < out->mNumberBuffers; ++b)
                        if (out->mBuffers[b].mData != nullptr) std::memset (out->mBuffers[b].mData, 0, out->mBuffers[b].mDataByteSize);
                if (in == nullptr || in->mNumberBuffers == 0 || in->mBuffers[0].mData == nullptr) return;
                const auto& buffer = in->mBuffers[0];
                const UInt32 chans = buffer.mNumberChannels > 0 ? buffer.mNumberChannels : 1;
                const size_t frames = std::min (scratch.size(), (size_t) buffer.mDataByteSize / (sizeof (float) * chans));
                const auto* samples = static_cast<const float*> (buffer.mData);
                for (size_t i = 0; i < frames; ++i)
                {
                    float sum = 0.0f;
                    for (UInt32 c = 0; c < chans; ++c) sum += samples[i * chans + c];
                    scratch[i] = sum;
                }
                calls.fetch_add (1, std::memory_order_relaxed);
                callback (scratch.data(), nullptr, (int) frames, hostNs (now->mHostTime));
            });
        if (created != noErr) return "could not attach to the device (" + std::to_string ((int) created) + ")";
        ioProc = reinterpret_cast<void*> (proc);
        deviceId = dev;
        if (const OSStatus started = AudioDeviceStart (dev, proc); started != noErr) { close(); return "the device could not be started (" + std::to_string ((int) started) + ")"; }
        return {};
    }

    const AudioComponentDescription description { kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0 };
    AudioUnit au = nullptr;
    auto* component = AudioComponentFindNext (nullptr, &description);
    if (component == nullptr || AudioComponentInstanceNew (component, &au) != noErr) return "the system output unit is unavailable";
    unit = au;

    auto set = [au] (AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, const void* data, UInt32 size)
    { return AudioUnitSetProperty (au, id, scope, element, data, size) == noErr; };
    auto fail = [this] (const char* why) { close(); return std::string (why); };

    // The point of this class: exactly one direction is enabled. Element 1 is the device's input, element 0 its output.
    const UInt32 on = 1, off = 0;
    if (! set (kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, input ? &on : &off, sizeof (UInt32))
        || ! set (kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, input ? &off : &on, sizeof (UInt32))) return fail ("could not select the I/O direction");
    const AudioObjectID id = dev;
    if (! set (kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &id, sizeof (id))) return fail ("could not select the device");

    deviceId = dev;
    rate = deviceSampleRate (dev);
    const int available = channelCount (dev, input ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput);
    if (rate <= 0 || available <= 0) return fail (input ? "the device has no inputs" : "the device has no outputs");
    channels = input ? 1 : (available >= 2 ? 2 : 1);

    AudioStreamBasicDescription format {};
    format.mSampleRate = rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
    format.mChannelsPerFrame = (UInt32) channels;
    format.mBitsPerChannel = 32;
    format.mBytesPerFrame = format.mBytesPerPacket = 4;
    format.mFramesPerPacket = 1;
    if (! set (kAudioUnitProperty_StreamFormat, input ? kAudioUnitScope_Output : kAudioUnitScope_Input, input ? 1 : 0, &format, sizeof (format))) return fail ("the device refused the sample format");

    const UInt32 maxFrames = 4096;
    set (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFrames, sizeof (maxFrames));
    const AURenderCallbackStruct proc { Procs::render, this };
    if (! (input ? set (kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, &proc, sizeof (proc))
                 : set (kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &proc, sizeof (proc)))) return fail ("could not install the callback");

    if (AudioUnitInitialize (au) != noErr) return fail ("the device could not be initialised");
    if (AudioOutputUnitStart (au) != noErr) return fail ("the device could not be started");
    return {};
}

void HalUnit::close()
{
    if (ioProc != nullptr)
    {
        AudioDeviceStop (deviceId, reinterpret_cast<AudioDeviceIOProcID> (ioProc));      // returns once no callback is running
        AudioDeviceDestroyIOProcID (deviceId, reinterpret_cast<AudioDeviceIOProcID> (ioProc));
        ioProc = nullptr;
        deviceId = 0;
    }
    if (unit == nullptr) return;
    auto au = static_cast<AudioUnit> (unit);
    AudioOutputUnitStop (au);              // returns once no callback is running
    AudioUnitUninitialize (au);
    AudioComponentInstanceDispose (au);
    unit = nullptr;
    deviceId = 0;
}

// ---------------------------------------------------------------------------------------------------------------------
void probeMicrophoneTrigger (const std::string& file, const std::function<const char*()>& micStatus)
{
    FILE* out = std::fopen (file.c_str(), "w");
    if (out == nullptr) return;
    const auto began = std::chrono::steady_clock::now();
    auto ms = [&] { return (long long) std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now() - began).count(); };
    auto log = [&] (const char* tag, const std::string& what) { std::fprintf (out, "%7lld ms  %-5s %s  [microphone: %s]\n", ms(), tag, what.c_str(), micStatus()); std::fflush (out); };
    auto step = [&] (const std::string& what, const std::function<std::string()>& run)
    {
        log ("BEGIN", what);
        const auto t0 = ms();
        const auto result = run();
        log ("END", what + " -> " + result + " (" + std::to_string (ms() - t0) + " ms)");
    };

    const auto devices = audioDevices();
    const auto defaultOut = defaultAudioDevice (false);
    AudioDeviceInfo duplex, outputOnly;
    for (const auto& d : devices)
    {
        std::fprintf (out, "device %u \"%s\" in %d out %d%s\n", d.id, d.name.c_str(), d.inputChannels, d.outputChannels, d.id == defaultOut ? "  <- default output" : "");
        if (d.outputChannels > 0 && d.inputChannels > 0 && (duplex.id == 0 || d.id == defaultOut)) duplex = d;
        if (d.outputChannels > 0 && d.inputChannels == 0 && d.name.find ("Speakers") != std::string::npos) outputOnly = d;
    }
    log ("START", "probe");

    auto auhal = [] (const AudioDeviceInfo& d)
    {
        HalUnit unit;
        std::atomic<int> calls { 0 };
        const auto error = unit.open (d.id, false, [&calls] (float* l, float* r, int n, uint64_t) { std::fill (l, l + n, 0.0f); std::fill (r, r + n, 0.0f); ++calls; });   // silence
        if (! error.empty()) return "FAILED: " + error;
        std::this_thread::sleep_for (std::chrono::milliseconds (1000));
        unit.close();
        return "ok, " + std::to_string (calls.load()) + " output callbacks in 1 s";
    };
    auto rawIOProc = [] (const AudioDeviceInfo& d, bool start)       // what JUCE 9.0.5's CoreAudio device does
    {
        AudioDeviceIOProcID proc = nullptr;
        __block int calls = 0;
        const OSStatus created = AudioDeviceCreateIOProcIDWithBlock (&proc, d.id, nullptr,
            ^(const AudioTimeStamp*, const AudioBufferList*, const AudioTimeStamp*, AudioBufferList* output, const AudioTimeStamp*)
            { for (UInt32 i = 0; i < output->mNumberBuffers; ++i) std::memset (output->mBuffers[i].mData, 0, output->mBuffers[i].mDataByteSize); ++calls; });
        if (created != noErr) return std::string ("FAILED: create ") + std::to_string ((int) created);
        std::string result = "ok, created";
        if (start)
        {
            if (const OSStatus started = AudioDeviceStart (d.id, proc); started != noErr) result = "FAILED: start " + std::to_string ((int) started);
            else { std::this_thread::sleep_for (std::chrono::milliseconds (1000)); AudioDeviceStop (d.id, proc); result = "ok, " + std::to_string (calls) + " output callbacks in 1 s"; }
        }
        AudioDeviceDestroyIOProcID (d.id, proc);
        return result;
    };

    // Ordered from least to most likely to prompt, so that everything before the trigger still gets logged.
    if (outputOnly.id != 0) step ("AUHAL, input disabled, on output-only \"" + outputOnly.name + "\": open, run 1 s, close", [&] { return auhal (outputOnly); });
    if (duplex.id != 0)     step ("AUHAL, input disabled, on duplex \"" + duplex.name + "\": open, run 1 s, close", [&] { return auhal (duplex); });
    if (outputOnly.id != 0) step ("raw IOProc on output-only \"" + outputOnly.name + "\": create, start, run 1 s, stop, destroy", [&] { return rawIOProc (outputOnly, true); });
    if (duplex.id != 0)     step ("raw IOProc on duplex \"" + duplex.name + "\": create only, destroy", [&] { return rawIOProc (duplex, false); });
    if (duplex.id != 0)     step ("raw IOProc on duplex \"" + duplex.name + "\": create, start, run 1 s, stop, destroy", [&] { return rawIOProc (duplex, true); });
    log ("DONE", "probe");
    std::fclose (out);
}

} // namespace wildband
