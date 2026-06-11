#pragma once

#include <JuceHeader.h>

class BlackheartAudioProcessor;

// DEAD CHANNEL CRT scope. Waveform with phosphor bloom on void black.
// Minimal OSD: top-left active preset, bottom-left momentary states.
class Scope : public juce::Component, public juce::Timer
{
public:
    Scope();

    void paint(juce::Graphics& g) override;
    void pushSamples(const float* samples, int numSamples);
    void timerCallback() override;
    void setProcessor(BlackheartAudioProcessor* p) { processor = p; }

    void setOsdChannel(const juce::String& text);   // e.g. "SCREAM"
    void setOsdMomentary(const juce::String& text); // e.g. "+1OCT\xC2\xB7HOLD" or ""

private:
    static constexpr int displayBufferSize = 512;
    std::array<float, displayBufferSize> displayBuffer{};
    int writeIndex = 0;
    BlackheartAudioProcessor* processor = nullptr;

    juce::String osdChannel, osdMomentary;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Scope)
};
