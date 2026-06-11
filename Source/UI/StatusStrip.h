#pragma once

#include <JuceHeader.h>

// Bottom-of-editor status strip.
// Left: CPU __% · LAT __MS · ____HZ.   Right: brand line.
class StatusStrip : public juce::Component
{
public:
    StatusStrip();

    void paint(juce::Graphics&) override;

    void setCpuLoad(float load01);
    void setLatencyMs(float ms);
    void setSampleRate(double sr);

private:
    float  cpuLoad    = 0.0f;
    float  latencyMs  = 0.0f;
    double sampleRate = 44100.0;

    static constexpr const char* kBrand = "BH\xC2\xB7""DEADCH\xC2\xB7""1.0";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusStrip)
};
