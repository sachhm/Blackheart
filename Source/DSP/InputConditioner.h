#pragma once

#include <JuceHeader.h>

namespace DSP
{

class InputConditioner
{
public:
    InputConditioner() = default;
    ~InputConditioner() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    void setInputGain(float gainDb);
    void setInputGainLinear(float gainLinear);
    void setDCBlockEnabled(bool enabled) { dcBlockEnabled = enabled; }
    void setAntiAliasingEnabled(bool enabled) { antiAliasingEnabled = enabled; }
    void setMonoSumEnabled(bool enabled) { monoSumEnabled = enabled; }

    float getInputGainDb() const { return juce::Decibels::gainToDecibels(inputGain.getTargetValue()); }
    float getInputGainLinear() const { return inputGain.getTargetValue(); }

private:
    void processDCBlock(juce::AudioBuffer<float>& buffer);
    void processGain(juce::AudioBuffer<float>& buffer);
    void processAntiAliasing(juce::AudioBuffer<float>& buffer);
    void processMonoSum(juce::AudioBuffer<float>& buffer);

    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxBlockSize = 512;

    juce::SmoothedValue<float> inputGain { 1.0f };

    static constexpr float dcBlockCutoffHz = 10.0f;
    std::array<juce::dsp::IIR::Filter<float>, 2> dcBlockFilters;
    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockCoeffs;

    juce::dsp::StateVariableTPTFilter<float> antiAliasingFilter;

    bool dcBlockEnabled = true;
    bool antiAliasingEnabled = true;
    bool monoSumEnabled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputConditioner)
};

} // namespace DSP
