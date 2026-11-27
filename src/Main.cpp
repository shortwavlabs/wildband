#include <juce_audio_utils/juce_audio_utils.h>
struct App : juce::JUCEApplication { const juce::String getApplicationName() override { return "WILDBAND"; } const juce::String getApplicationVersion() override { return "0"; } void initialise (const juce::String&) override { quit(); } void shutdown() override {} };
START_JUCE_APPLICATION (App)
