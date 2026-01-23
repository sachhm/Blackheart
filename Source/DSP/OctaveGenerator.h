#pragma once

#include <JuceHeader.h>
#include <array>

namespace DSP
{

class OctaveGenerator
{
public:
    OctaveGenerator() = default;
    ~OctaveGenerator() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    void setGlare(float normalizedGlare);

    float getCurrentGlare() const { return glare.getTargetValue(); }
    float getOctaveLevel() const { return lastOctaveLevel; }

private:
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    int numChannels = 2;

    juce::SmoothedValue<float> glare { 0.3f };

    juce::dsp::StateVariableTPTFilter<float> preEmphasisHP;
    juce::dsp::StateVariableTPTFilter<float> preEmphasisLP;
    juce::dsp::StateVariableTPTFilter<float> dcBlockFilter;
    juce::dsp::StateVariableTPTFilter<float> octaveBandpass;
    juce::dsp::StateVariableTPTFilter<float> octaveHighShelf;

    // Pre-allocated buffer to avoid audio-thread allocation
    juce::AudioBuffer<float> octaveBuffer;

    std::array<float, 2> previousSample = { 0.0f, 0.0f };
    float lastOctaveLevel = 0.0f;

    static constexpr float preHPFreq = 150.0f;
    static constexpr float preLPFreq = 4000.0f;
    static constexpr float dcBlockFreq = 20.0f;
    static constexpr float bandpassFreq = 1200.0f;
    static constexpr float bandpassQ = 0.8f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OctaveGenerator)
};

} // namespace DSP
