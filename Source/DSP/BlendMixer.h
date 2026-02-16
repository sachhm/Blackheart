#pragma once

#include <JuceHeader.h>

namespace DSP
{

class BlendMixer
{
public:
    BlendMixer() = default;
    ~BlendMixer() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(const juce::AudioBuffer<float>& dryBuffer,
                 const juce::AudioBuffer<float>& wetBuffer,
                 juce::AudioBuffer<float>& outputBuffer);

    void setBlend(float normalizedBlend);

    float getCurrentBlend() const { return blend.getTargetValue(); }
    float getDryGain() const { return lastDryGain; }
    float getWetGain() const { return lastWetGain; }

private:
    void calculateGains(float blendValue, float& dryGain, float& wetGain) const;

    double sampleRate = 44100.0;
    int maxBlockSize = 512;

    juce::SmoothedValue<float> blend { 0.7f };

    float lastDryGain = 0.0f;
    float lastWetGain = 1.0f;

    static constexpr float compensationBoost = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlendMixer)
};

} // namespace DSP
