#pragma once

#include <JuceHeader.h>

namespace DSP
{

class OutputLimiter
{
public:
    OutputLimiter() = default;
    ~OutputLimiter() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    void setOutputLevel(float normalizedLevel);
    void setCeiling(float ceilingDb);
    void setHeadroom(float headroomDb);

    float getCurrentLevel() const { return outputLevel.getTargetValue(); }
    float getCeilingDb() const { return juce::Decibels::gainToDecibels(ceiling); }
    float getGainReduction() const { return lastGainReduction; }

private:
    float softClip(float sample) const;
    float processSaturation(float sample, float drive) const;

    double sampleRate = 44100.0;
    int maxBlockSize = 512;

    juce::SmoothedValue<float> outputLevel { 0.7f };
    juce::SmoothedValue<float> gainReduction { 1.0f };

    float ceiling = 0.95f;
    float headroom = 0.9f;

    float envelope = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    float lastGainReduction = 0.0f;

    juce::dsp::StateVariableTPTFilter<float> dcBlockFilter;

    static constexpr float dcBlockFreq = 5.0f;
    static constexpr float attackTimeMs = 0.5f;
    static constexpr float releaseTimeMs = 100.0f;
    static constexpr float saturationKnee = 0.7f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputLimiter)
};

} // namespace DSP
