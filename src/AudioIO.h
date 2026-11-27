// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
// Small CoreAudio layer used by the WILDBAND Room helper (microphone capture) and by the --probe diagnostic.
// The instrument's own output goes through juce::AudioDeviceManager. Input here is a plain device IO proc that reads
// the device's first input stream and mixes its channels to mono; output is Apple's HAL output unit (probe only).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace wildband
{

struct AudioDeviceInfo { uint32_t id = 0; std::string name, uid; int inputChannels = 0, outputChannels = 0; };
std::vector<AudioDeviceInfo> audioDevices();
uint32_t defaultAudioDevice (bool input);          // 0 = none
double deviceSampleRate (uint32_t device);
int deviceBufferSize (uint32_t device);
bool setDeviceSampleRate (uint32_t device, double rate);
bool setDeviceBufferSize (uint32_t device, int frames);

class HalUnit
{
public:
    /** Output: fill left/right (right is never null). Input: `left` holds n mono samples to read, `right` is null.
        Called on the unit's real-time thread. */
    using Callback = std::function<void (float* left, float* right, int n, uint64_t hostTimeNs)>;

    ~HalUnit() { close(); }
    /** Opens and starts. Returns an empty string on success, otherwise what failed. */
    std::string open (uint32_t device, bool input, Callback);
    void close();
    bool isOpen() const noexcept { return unit != nullptr || ioProc != nullptr; }
    std::atomic<int> calls { 0 };       // callbacks delivered since open (diagnostic)
    uint32_t device() const noexcept { return deviceId; }
    double sampleRate() const noexcept { return rate; }

private:
    struct Procs;
    void* unit = nullptr;                // AudioUnit (output)
    void* ioProc = nullptr;              // AudioDeviceIOProcID (input)
    uint32_t deviceId = 0;
    double rate = 0;
    int channels = 2;
    bool isInput = false;
    Callback callback;
    std::vector<float> scratch;
};

/** Diagnostic (--probe=FILE): performs CoreAudio operations one at a time and appends the microphone authorization
    status and elapsed time after each to `file`, flushing as it goes, so that a step that blocks or prompts is the
    last "BEGIN" without an "END". `micStatus` returns a short status word. */
void probeMicrophoneTrigger (const std::string& file, const std::function<const char*()>& micStatus);

} // namespace wildband
