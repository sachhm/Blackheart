#pragma once

#include <JuceHeader.h>

namespace DSP
{

class EnvelopeFollower
{
public:
    enum class DetectionMode
    {
        Peak,
        RMS,
        PeakHold
    };

    EnvelopeFollower() = default;
    ~EnvelopeFollower() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    float processSample(float inputSample);

    template<typename ProcessContext>
    float process(const ProcessContext& context)
    {
        auto& inputBlock = context.getInputBlock();
        const auto numSamples = inputBlock.getNumSamples();
        const auto numChannels = inputBlock.getNumChannels();

        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            float maxLevel = 0.0f;
            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                maxLevel = std::max(maxLevel, std::abs(inputBlock.getSample(static_cast<int>(channel),
                                                                            static_cast<int>(sample))));
            }
            processSample(maxLevel);
        }

        return envelope;
    }

    float processBlock(const juce::AudioBuffer<float>& buffer);

    float processBlockRMS(const juce::AudioBuffer<float>& buffer);

    void setAttackTime(float attackMs);
    void setReleaseTime(float releaseMs);
    void setAttackTimeSeconds(double attackSec);
    void setReleaseTimeSeconds(double releaseSec);
    void setDetectionMode(DetectionMode mode) { detectionMode = mode; }
    void setHoldTime(float holdMs);
    void setSensitivity(float sens) { sensitivity = juce::jlimit(0.0f, 1.0f, sens); }

    float getCurrentEnvelope() const { return envelope; }
    float getEnvelopeDb() const { return juce::Decibels::gainToDecibels(envelope, -96.0f); }
    DetectionMode getDetectionMode() const { return detectionMode; }
    double getSampleRate() const { return sampleRate; }

private:
    void updateCoefficients();
    float processPeak(float inputLevel);
    float processRMS(float inputLevel);
    float processPeakHold(float inputLevel);

    double sampleRate = 44100.0;

    float envelope = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    float attackTimeMs = 5.0f;
    float releaseTimeMs = 100.0f;

    DetectionMode detectionMode = DetectionMode::Peak;

    float rmsSum = 0.0f;
    static constexpr int rmsWindowSize = 64;
    std::array<float, rmsWindowSize> rmsBuffer {};
    int rmsIndex = 0;

    float holdTimeMs = 0.0f;
    int holdSamples = 0;
    int holdCounter = 0;
    float peakHeldValue = 0.0f;

    float sensitivity = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeFollower)
};

} // namespace DSP
