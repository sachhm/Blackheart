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

    // 2x oversampling for aliasing-free rectification
    juce::dsp::Oversampling<float> oversampling {
        2,  // numChannels
        1,  // order (2^1 = 2x)
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true
    };

    // Filters run at oversampled rate
    juce::dsp::StateVariableTPTFilter<float> preEmphasisHP;
    juce::dsp::StateVariableTPTFilter<float> preEmphasisLP;
    juce::dsp::StateVariableTPTFilter<float> dcBlockFilter;
    juce::dsp::StateVariableTPTFilter<float> octaveBandpass;
    juce::dsp::StateVariableTPTFilter<float> octaveHighShelf;

    // Pre-allocated buffer to avoid audio-thread allocation
    juce::AudioBuffer<float> octaveBuffer;
    float lastOctaveLevel = 0.0f;

    // Voicing: preHP at 40Hz keeps downtuned fundamentals feeding the rectifier
    // (rectification doubles f — kill 82Hz in and there is no 164Hz octave out).
    // Broad low-Q bandpass at 600Hz spans the doubled-fundamental range
    // (~160Hz-1.3kHz) instead of notching it out.
    static constexpr float preHPFreq = 40.0f;
    static constexpr float preLPFreq = 4000.0f;
    static constexpr float dcBlockFreq = 20.0f;
    static constexpr float bandpassFreq = 600.0f;
    static constexpr float bandpassQ = 0.5f;
    static constexpr float emphasisHPFreq = 150.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OctaveGenerator)
};

} // namespace DSP
