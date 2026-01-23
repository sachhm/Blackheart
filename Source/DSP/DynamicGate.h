#pragma once

#include <JuceHeader.h>
#include "EnvelopeFollower.h"

namespace DSP
{

class DynamicGate
{
public:
    DynamicGate() = default;
    ~DynamicGate() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    void setBaseThreshold(float thresholdDb);
    void setGainInfluence(float normalizedGain);
    void setGlareInfluence(float normalizedGlare);

    void setAttackTime(float attackMs);
    void setReleaseTime(float releaseMs);
    void setHoldTime(float holdMs);

    float getCurrentGateGain() const { return lastGateGain; }
    float getEffectiveThreshold() const;

private:
    float calculateDynamicThreshold() const;
    float calculateGateGain(float envelope, float threshold) const;

    double sampleRate = 44100.0;
    int maxBlockSize = 512;

    EnvelopeFollower envelopeFollower;

    juce::SmoothedValue<float> gateGain { 1.0f };

    float baseThresholdDb = -40.0f;
    float baseThresholdLinear = 0.01f;

    float gainInfluence = 0.5f;
    float glareInfluence = 0.3f;

    float lastGateGain = 1.0f;

    static constexpr float kneeWidth = 0.15f;
    static constexpr float hysteresisDb = 3.0f;

    bool gateOpen = true;
    float hysteresisThresholdHigh = 0.0f;
    float hysteresisThresholdLow = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicGate)
};

} // namespace DSP
