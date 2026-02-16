#pragma once

#include <JuceHeader.h>

namespace DSP
{

class ChaosModulator
{
public:
    ChaosModulator() = default;
    ~ChaosModulator() = default;

    struct ModulationOutput
    {
        float pitchMod = 0.0f;
        float grainSizeMod = 0.0f;
        float timingMod = 0.0f;
        float combinedMod = 0.0f;
    };

    enum class ResponseCurve
    {
        Linear,
        Exponential,
        Logarithmic,
        SCurve
    };

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void process(int numSamples);
    ModulationOutput getModulation() const;
    float getNextModulationValue();

    void setSpeed(float normalizedSpeed);
    void setChaos(float normalizedChaos);
    void setEnvelopeValue(float envelopeLevel);
    void setSeed(unsigned int seed);

    void setResponseCurve(ResponseCurve curve);
    void setEnvelopeSensitivity(float sensitivity);
    void setEnvelopeThreshold(float threshold);
    void setEnvelopeAttack(float attackMs);
    void setEnvelopeRelease(float releaseMs);

    float getSpeed() const { return currentSpeedHz; }
    float getChaos() const { return chaos.getTargetValue(); }
    float getLFOPhase() const { return lfoPhase; }
    float getEnvelopeInfluence() const { return smoothedEnvelopeInfluence; }
    float getEffectiveChaos() const { return effectiveChaosAmount; }
    ResponseCurve getResponseCurve() const { return responseCurve; }

private:
    float generateSineWave(float phase) const;
    float generateTriangleWave(float phase) const;
    float generateSmoothNoise(float t);
    float interpolateHermite(float y0, float y1, float y2, float y3, float t) const;
    void updateSampleAndHold();
    void updateRandomWalk();
    float applyResponseCurve(float input) const;
    void updateEnvelopeSmoothing(float rawEnvelope);

    double sampleRate = 44100.0;

    juce::SmoothedValue<float> speed { 2.0f };
    juce::SmoothedValue<float> chaos { 0.5f };

    float rawEnvelopeValue = 0.0f;
    float smoothedEnvelopeInfluence = 0.0f;
    float effectiveChaosAmount = 0.0f;

    float envelopeAttackCoeff = 0.0f;
    float envelopeReleaseCoeff = 0.0f;

    ResponseCurve responseCurve = ResponseCurve::Exponential;
    float envelopeSensitivity = 1.0f;
    float envelopeThreshold = 0.05f;

    static constexpr float defaultAttackMs = 3.0f;
    static constexpr float defaultReleaseMs = 100.0f;

    float currentSpeedHz = 2.0f;

    float lfoPhase = 0.0f;
    float lfoPhaseIncrement = 0.0f;

    float sampleAndHoldValue = 0.0f;
    float sampleAndHoldTarget = 0.0f;
    float sampleAndHoldPhase = 0.0f;
    float sampleAndHoldSmoothed = 0.0f;
    static constexpr float sampleAndHoldSmoothCoeff = 0.995f;
    float dynamicSHSmoothCoeff = sampleAndHoldSmoothCoeff;

    float randomWalkValue = 0.0f;
    float randomWalkTarget = 0.0f;
    float randomWalkPhase = 0.0f;
    static constexpr float randomWalkSmoothCoeff = 0.999f;

    static constexpr int noiseTableSize = 256;
    std::array<float, noiseTableSize> noiseTable {};
    int noiseTableIndex = 0;
    float noiseSmoothValue = 0.0f;

    ModulationOutput currentOutput;

    unsigned int currentSeed = 12345;
    juce::Random random;
    juce::Random deterministicRandom;

    static constexpr float minSpeedHz = 0.1f;
    static constexpr float maxSpeedHz = 20.0f;
    static constexpr float speedSkew = 0.4f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaosModulator)
};

} // namespace DSP
