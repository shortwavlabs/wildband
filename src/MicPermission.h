// macOS microphone authorisation (AVFoundation). JUCE's RuntimePermissions does nothing on macOS.
#pragma once
#include <functional>

namespace wildband
{
enum class MicAccess { Undetermined, Granted, Denied };
MicAccess microphoneAccess();
/** Shows the system prompt if the user has not decided yet. `done` runs on the main thread. */
void requestMicrophone (std::function<void (bool granted)> done);
}
