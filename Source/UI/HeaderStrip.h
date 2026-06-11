#pragma once

#include <JuceHeader.h>

// DEAD CHANNEL header. Left: BLACKHEART wordmark.
// Right: IN/OUT segment meters + REC dot (pulses with audio activity) + rate·latency readout.
class HeaderStrip : public juce::Component
{
public:
    HeaderStrip();

    void paint(juce::Graphics&) override;

    void setSampleRate(double sampleRateHz);
    void setLatencyMs(float latencyMs);
    void setActivity(float level01); // drives REC dot brightness
    void setLevels(float in01, float out01);

private:
    double sampleRate = 44100.0;
    float  latencyMs  = 0.0f;
    float  activity   = 0.0f;
    float  smoothedIn = 0.0f, smoothedOut = 0.0f;

    void drawSegmentMeter(juce::Graphics& g, juce::Rectangle<float> r, float level01) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderStrip)
};
