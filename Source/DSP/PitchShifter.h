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
    // Dual-head delay line (Whammy/Noise-style pitch shifting)
    struct DelayHead
    {
        float readPosition = 0.0f;
        float ramp = 0.0f;       // Sawtooth ramp [0, 1) — sweep progress
    };

    float readFromBuffer(int channel, float position) const;
    void resetHeadsForTransition();

    double sampleRate = 44100.0;
    int maxBlockSize = 512;

    std::atomic<bool> octaveOneActive { false };
    std::atomic<bool> octaveTwoActive { false };
    bool prevOctaveOneActive = false;
    bool prevOctaveTwoActive = false;

    float currentPitchRatio = 1.0f;
    float currentMix = 0.0f;

    float mixSmoothState = 0.0f;

    float riseTimeMs = 50.0f;
    float fallTimeMs = 30.0f;
    float riseCoeff = 0.0f;
    float fallCoeff = 0.0f;

    static constexpr float minRiseMs = 1.0f;
    static constexpr float maxRiseMs = 500.0f;

    bool transitionActive = false;

    juce::SmoothedValue<float> chaos { 0.0f };
    juce::SmoothedValue<float> panic { 0.0f };

    float pitchModulation = 0.0f;
    float grainSizeModulation = 0.0f;  // Reused: modulates window size
    float timingModulation = 0.0f;     // Reused: jitters reset position

    // PANIC — detuned head pairs
    float panicAmount = 0.0f;

    // Ring modulation
    float ringModPhase = 0.0f;
    float ringModFreq = 0.0f;
    float ringModMix = 0.0f;

    // Feedback
    float feedbackSample = 0.0f;

    // Gain compensation envelopes
    float dryEnvelope = 0.0f;
    float wetEnvelope = 0.0f;
    static constexpr float envelopeAttack = 0.01f;
    static constexpr float envelopeRelease = 0.001f;

    // Dual-head delay line
    static constexpr int maxChannels = 2;
    static constexpr int delayBufferSize = 8192;
    static constexpr int numMainHeads = 2;
    static constexpr int numDetuneHeads = 2;  // For PANIC

    std::array<std::array<float, delayBufferSize>, maxChannels> delayBuffer {};
    int writePosition = 0;

    std::array<DelayHead, numMainHeads> mainHeads;
    std::array<DelayHead, numDetuneHeads> detuneHeads;

    // Window/crossfade parameters
    int windowSizeSamples = 1024;
    static constexpr float minWindowMs = 10.0f;
    static constexpr float maxWindowMs = 60.0f;
    static constexpr float defaultWindowMs = 30.0f;

    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchShifter)
};

} // namespace DSP
