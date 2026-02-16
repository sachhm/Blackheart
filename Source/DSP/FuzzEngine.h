#pragma once

#include <JuceHeader.h>

namespace DSP
{

class FuzzEngine
{
public:
    FuzzEngine() = default;
    ~FuzzEngine() = default;

    // Mode constants
    static constexpr int ModeScreaming = 0;
    static constexpr int ModeOverdrive = 1;
    static constexpr int ModeDoom      = 2;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    void setGain(float normalizedGain);
    void setLevel(float normalizedLevel);
    void setMode(int mode);
    void setShape(float normalizedShape);

    float getGain() const { return gain.getTargetValue(); }
    float getLevel() const { return level.getTargetValue(); }
    int getMode() const { return currentMode; }
    float getShape() const { return shape.getTargetValue(); }

private:
    // Germanium waveshaping
    float germaniumWaveshape(float sample, float drive, float asymmetry) const;

    // Mode-dependent filter configuration
    void configureFiltersForMode(int mode);

    double sampleRate = 44100.0;
    double oversampledRate = 88200.0;
    int maxBlockSize = 512;

    // Parameters
    juce::SmoothedValue<float> gain { 0.5f };
    juce::SmoothedValue<float> level { 0.7f };
    juce::SmoothedValue<float> shape { 0.5f };
    int currentMode = ModeOverdrive;

    // Oversampling: 2x with IIR for low latency
    juce::dsp::Oversampling<float> oversampling {
        2,  // numChannels
        1,  // order (2^1 = 2x)
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true
    };

    // Mode-dependent pre-clip EQ
    juce::dsp::StateVariableTPTFilter<float> preEqHP;     // Highpass (mode-dependent cutoff)
    juce::dsp::StateVariableTPTFilter<float> preEqPeak;   // Parametric peak (voicing)
    juce::dsp::StateVariableTPTFilter<float> preEqShelf;  // High shelf (doom mode darkening)

    // Mode-dependent post-clip EQ
    juce::dsp::StateVariableTPTFilter<float> postEqLP;    // Lowpass (tame harshness)
    juce::dsp::StateVariableTPTFilter<float> postEqPeak;  // Presence/character

    // SHAPE EQ (post-clip, user-controlled)
    juce::dsp::StateVariableTPTFilter<float> shapeMidEQ;  // Parametric mid sweep
    juce::dsp::StateVariableTPTFilter<float> shapeLowEQ;  // Low shelf

    // DC blocker (removes bias drift DC)
    juce::dsp::StateVariableTPTFilter<float> dcBlocker;

    // Germanium emulation state
    float compressionEnvelope = 0.0f;
    float sagEnvelope = 0.0f;          // Voltage sag tracking
    float biasDriftPhase = 0.0f;       // Slow LFO for bias drift
    float impedanceLPFCutoff = 8000.0f; // Input impedance interaction

    // Pre-calculated coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float sagAttackCoeff = 0.0f;
    float sagReleaseCoeff = 0.0f;

    // Mode-specific drive scaling
    float modeAsymmetry = 0.3f;       // Asymmetry amount per mode
    float modeDriveScale = 1.0f;      // Drive multiplier per mode

    // Drive range
    static constexpr float minDrive = 1.0f;
    static constexpr float maxDrive = 80.0f;

    // Cached filter params to avoid redundant updates
    float lastShapeValue = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FuzzEngine)
};

} // namespace DSP
