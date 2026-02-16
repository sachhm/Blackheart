#pragma once

#include <JuceHeader.h>

namespace DSP
{

class PitchShifter
{
public:
    PitchShifter() = default;
    ~PitchShifter() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    void setOctaveOneActive(bool active);
    void setOctaveTwoActive(bool active);
    void setRiseTime(float riseTimeMs);
    void setChaosAmount(float normalizedChaos);
    void setPanic(float normalizedPanic);
    void setRingModSpeed(float normalizedSpeed);

    void setPitchModulation(float mod);
    void setGrainSizeModulation(float mod);
    void setTimingModulation(float mod);

    bool isOctaveOneActive() const { return octaveOneActive.load(std::memory_order_relaxed); }
    bool isOctaveTwoActive() const { return octaveTwoActive.load(std::memory_order_relaxed); }
    float getCurrentPitchRatio() const { return currentPitchRatio; }
    float getCurrentMix() const { return currentMix; }
    bool isTransitioning() const { return transitionActive; }

private:
    struct Grain
    {
        float readPosition = 0.0f;
        float phase = 0.0f;
        bool active = true;
    };

    enum class TransitionState
    {
        Idle,
        Engaging,
        Engaged,
        Disengaging
    };

    float getGrainWindow(float phase, float harshness) const;
    float readFromBuffer(int channel, float position) const;
    void updateGrain(Grain& grain, float pitchRatio, int grainSizeSamples);
    void updateTransition();
    float applyExpCurve(float linearValue, bool isAttack) const;
    void resetGrainsForTransition();

    double sampleRate = 44100.0;
    int maxBlockSize = 512;

    std::atomic<bool> octaveOneActive { false };
    std::atomic<bool> octaveTwoActive { false };
    bool prevOctaveOneActive = false;
    bool prevOctaveTwoActive = false;

    float targetPitchAmount = 0.0f;
    float currentPitchRatio = 1.0f;
    float currentMix = 0.0f;

    float pitchSmoothState = 0.0f;
    float mixSmoothState = 0.0f;

    float riseTimeMs = 50.0f;
    float fallTimeMs = 30.0f;
    float riseCoeff = 0.0f;
    float fallCoeff = 0.0f;

    static constexpr float minRiseMs = 1.0f;
    static constexpr float maxRiseMs = 500.0f;
    static constexpr float expCurveAmount = 3.0f;

    TransitionState transitionState = TransitionState::Idle;
    bool transitionActive = false;
    int transitionSampleCounter = 0;
    int antiClickFadeSamples = 64;
    float antiClickFadeGain = 1.0f;

    juce::SmoothedValue<float> chaos { 0.0f };
    juce::SmoothedValue<float> panic { 0.0f };

    float pitchModulation = 0.0f;
    float grainSizeModulation = 0.0f;
    float timingModulation = 0.0f;

    // PANIC — detune engine
    float panicAmount = 0.0f;

    // Ring modulation
    float ringModPhase = 0.0f;
    float ringModFreq = 0.0f;
    float ringModMix = 0.0f;

    // Feedback
    float feedbackSample = 0.0f;

    // Variable grain count
    int activeGrainCount = 2;

    static constexpr int maxChannels = 2;
    static constexpr int delayBufferSize = 8192;
    static constexpr int maxGrains = 4;
    static constexpr int maxDetuneGrains = 2;

    std::array<std::array<float, delayBufferSize>, maxChannels> delayBuffer {};
    int writePosition = 0;

    std::array<Grain, maxGrains> grains;
    std::array<Grain, maxDetuneGrains> detuneGrains;

    int grainSizeSamples = 1024;
    static constexpr float minGrainMs = 20.0f;
    static constexpr float maxGrainMs = 40.0f;
    static constexpr float defaultGrainMs = 30.0f;

    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchShifter)
};

} // namespace DSP
