#pragma once

#include <JuceHeader.h>

namespace DSP
{

class FuzzEngine
{
public:
    FuzzEngine() = default;
    ~FuzzEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    void setGain(float normalizedGain);
    void setLevel(float normalizedLevel);

    void setAsymmetry(float asymmetry);
    void setTone(float normalizedTone);
    void setBias(float bias);

    float getGain() const { return gain.getTargetValue(); }
    float getLevel() const { return level.getTargetValue(); }

private:
    float processCompression(float sample, float threshold);
    float processCompressionFast(float sample, float threshold) const;
    float processAsymmetricWaveshaper(float sample, float drive);
    float softClip(float sample);
    float softClipFast(float sample) const;
    float hardClip(float sample, float threshold);

    double sampleRate = 44100.0;
    double oversampledRate = 88200.0;
    int maxBlockSize = 512;

    juce::SmoothedValue<float> gain { 0.5f };
    juce::SmoothedValue<float> level { 0.7f };
    juce::SmoothedValue<float> asymmetryAmount { 0.3f };
    juce::SmoothedValue<float> toneControl { 0.5f };
    juce::SmoothedValue<float> biasAmount { 0.0f };

    static constexpr int oversamplingFactor = 1;
    juce::dsp::Oversampling<float> oversampling {
        2,
        oversamplingFactor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true
    };

    juce::dsp::StateVariableTPTFilter<float> preFilterHP;
    juce::dsp::StateVariableTPTFilter<float> preFilterLP;
    juce::dsp::StateVariableTPTFilter<float> postFilterLP;
    juce::dsp::StateVariableTPTFilter<float> postFilterTone;

    float compressionEnvelope = 0.0f;
    static constexpr float compressionAttack = 0.001f;
    static constexpr float compressionRelease = 0.05f;

    // Pre-calculated envelope coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    // Cached tone frequency to avoid unnecessary filter updates
    float lastToneFreq = 4000.0f;

    static constexpr float minDrive = 1.0f;
    static constexpr float maxDrive = 80.0f;  // Reduced from 100 for less harsh saturation

    static constexpr float preFilterHPFreq = 40.0f;  // Lowered from 80Hz for better bass with downtuned guitars
    static constexpr float preFilterLPFreq = 8000.0f;
    static constexpr float postFilterLPFreq = 12000.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FuzzEngine)
};

} // namespace DSP
