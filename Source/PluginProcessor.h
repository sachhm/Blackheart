#pragma once

#include <JuceHeader.h>
#include "Parameters/ParameterIDs.h"
#include "DSP/InputConditioner.h"
#include "DSP/FuzzEngine.h"
#include "DSP/OctaveGenerator.h"
#include "DSP/DynamicGate.h"
#include "DSP/BlendMixer.h"
#include "DSP/PitchShifter.h"
#include "DSP/ChaosModulator.h"
#include "DSP/EnvelopeFollower.h"
#include "DSP/OutputLimiter.h"

//==============================================================================
// Lock-free FIFO for waveform visualization
template <typename T, int Size>
class LockFreeFifo
{
public:
    void push(T value) noexcept
    {
        const int currentWrite = writeIndex.load(std::memory_order_relaxed);
        buffer[static_cast<size_t>(currentWrite)] = value;
        writeIndex.store((currentWrite + 1) % Size, std::memory_order_release);
    }

    void pushBlock(const T* data, int numSamples) noexcept
    {
        // Downsample to avoid overwhelming the FIFO
        const int skip = std::max(1, numSamples / 64);
        for (int i = 0; i < numSamples; i += skip)
            push(data[i]);
    }

    bool pull(T& value) noexcept
    {
        const int currentRead = readIndex.load(std::memory_order_relaxed);
        const int currentWrite = writeIndex.load(std::memory_order_acquire);

        if (currentRead == currentWrite)
            return false;

        value = buffer[static_cast<size_t>(currentRead)];
        readIndex.store((currentRead + 1) % Size, std::memory_order_release);
        return true;
    }

    int getNumAvailable() const noexcept
    {
        const int w = writeIndex.load(std::memory_order_acquire);
        const int r = readIndex.load(std::memory_order_acquire);
        return (w - r + Size) % Size;
    }

    void reset() noexcept
    {
        readIndex.store(0, std::memory_order_relaxed);
        writeIndex.store(0, std::memory_order_relaxed);
    }

private:
    std::array<T, Size> buffer{};
    std::atomic<int> writeIndex{0};
    std::atomic<int> readIndex{0};
};

//==============================================================================
// Signal chain metering for gain staging verification
struct SignalMeters
{
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> postConditionerLevel { 0.0f };
    std::atomic<float> postFuzzLevel { 0.0f };
    std::atomic<float> postOctaveLevel { 0.0f };
    std::atomic<float> postGateLevel { 0.0f };
    std::atomic<float> postBlendLevel { 0.0f };
    std::atomic<float> postPitchLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };

    std::atomic<bool> inputClipping { false };
    std::atomic<bool> internalClipping { false };
    std::atomic<bool> outputClipping { false };

    void reset()
    {
        inputLevel.store(0.0f);
        postConditionerLevel.store(0.0f);
        postFuzzLevel.store(0.0f);
        postOctaveLevel.store(0.0f);
        postGateLevel.store(0.0f);
        postBlendLevel.store(0.0f);
        postPitchLevel.store(0.0f);
        outputLevel.store(0.0f);
        inputClipping.store(false);
        internalClipping.store(false);
        outputClipping.store(false);
    }
};

struct SmoothedParameters
{
    juce::SmoothedValue<float> gain;
    juce::SmoothedValue<float> glare;
    juce::SmoothedValue<float> blend;
    juce::SmoothedValue<float> level;
    juce::SmoothedValue<float> speed;
    juce::SmoothedValue<float> chaos;
    juce::SmoothedValue<float> rise;
    juce::SmoothedValue<float> octave1;
    juce::SmoothedValue<float> octave2;

    void prepare(double sampleRate)
    {
        gain.reset(sampleRate, ParameterIDs::Smoothing::gainRampSec);
        glare.reset(sampleRate, ParameterIDs::Smoothing::glareRampSec);
        blend.reset(sampleRate, ParameterIDs::Smoothing::blendRampSec);
        level.reset(sampleRate, ParameterIDs::Smoothing::levelRampSec);
        speed.reset(sampleRate, ParameterIDs::Smoothing::speedRampSec);
        chaos.reset(sampleRate, ParameterIDs::Smoothing::chaosRampSec);
        rise.reset(sampleRate, ParameterIDs::Smoothing::riseRampSec);
        octave1.reset(sampleRate, ParameterIDs::Smoothing::octaveRampSec);
        octave2.reset(sampleRate, ParameterIDs::Smoothing::octaveRampSec);
    }

    void setCurrentAndTargetValue(float gainVal, float glareVal, float blendVal,
                                   float levelVal, float speedVal, float chaosVal,
                                   float riseVal, float oct1Val, float oct2Val)
    {
        gain.setCurrentAndTargetValue(gainVal);
        glare.setCurrentAndTargetValue(glareVal);
        blend.setCurrentAndTargetValue(blendVal);
        level.setCurrentAndTargetValue(levelVal);
        speed.setCurrentAndTargetValue(speedVal);
        chaos.setCurrentAndTargetValue(chaosVal);
        rise.setCurrentAndTargetValue(riseVal);
        octave1.setCurrentAndTargetValue(oct1Val);
        octave2.setCurrentAndTargetValue(oct2Val);
    }

    void updateTargets(float gainVal, float glareVal, float blendVal,
                       float levelVal, float speedVal, float chaosVal,
                       float riseVal, float oct1Val, float oct2Val)
    {
        gain.setTargetValue(gainVal);
        glare.setTargetValue(glareVal);
        blend.setTargetValue(blendVal);
        level.setTargetValue(levelVal);
        speed.setTargetValue(speedVal);
        chaos.setTargetValue(chaosVal);
        rise.setTargetValue(riseVal);
        octave1.setTargetValue(oct1Val);
        octave2.setTargetValue(oct2Val);
    }

    void skip(int numSamples)
    {
        gain.skip(numSamples);
        glare.skip(numSamples);
        blend.skip(numSamples);
        level.skip(numSamples);
        speed.skip(numSamples);
        chaos.skip(numSamples);
        rise.skip(numSamples);
        octave1.skip(numSamples);
        octave2.skip(numSamples);
    }

    bool isSmoothing() const
    {
        return gain.isSmoothing() || glare.isSmoothing() || blend.isSmoothing() ||
               level.isSmoothing() || speed.isSmoothing() || chaos.isSmoothing() ||
               rise.isSmoothing() || octave1.isSmoothing() || octave2.isSmoothing();
    }
};

class BlackheartAudioProcessor : public juce::AudioProcessor
{
public:
    BlackheartAudioProcessor();
    ~BlackheartAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float getGain() const { return gainParam->load(); }
    float getGlare() const { return glareParam->load(); }
    float getBlend() const { return blendParam->load(); }
    float getLevel() const { return levelParam->load(); }
    float getSpeed() const { return speedParam->load(); }
    float getChaos() const { return chaosParam->load(); }
    float getRise() const { return riseParam->load(); }
    bool getOctave1() const { return octave1Param->load() > 0.5f; }
    bool getOctave2() const { return octave2Param->load() > 0.5f; }

    void setOctave1(bool active);
    void setOctave2(bool active);

    float getInputEnvelope() const { return inputEnvelope; }
    float getChaosEnvelope() const { return chaosEnvelope; }

    void setTestMode(bool enabled) { testModeEnabled = enabled; }
    bool isTestMode() const { return testModeEnabled; }

    // Signal metering for gain staging verification
    const SignalMeters& getSignalMeters() const { return signalMeters; }
    void resetClippingIndicators() { signalMeters.inputClipping.store(false);
                                     signalMeters.internalClipping.store(false);
                                     signalMeters.outputClipping.store(false); }

    // Latency reporting
    int getLatencyInSamples() const { return totalLatencySamples; }

    // Stability monitoring
    bool isStable() const { return !stabilityError; }
    void resetStabilityError() { stabilityError = false; }

    // Waveform visualization data (lock-free)
    static constexpr int waveformFifoSize = 2048;
    LockFreeFifo<float, waveformFifoSize>& getWaveformFifo() { return waveformFifo; }

    // Chaos modulation output for visualization
    float getChaosModulationValue() const { return chaosModValue.load(std::memory_order_relaxed); }
    float getGainReduction() const { return signalMeters.outputLevel.load() > 0.9f ? 0.1f : 0.0f; }

private:
    void updateDSPParameters();
    void fetchParameterValues();

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float>* gainParam    = nullptr;
    std::atomic<float>* glareParam   = nullptr;
    std::atomic<float>* blendParam   = nullptr;
    std::atomic<float>* levelParam   = nullptr;
    std::atomic<float>* speedParam   = nullptr;
    std::atomic<float>* chaosParam   = nullptr;
    std::atomic<float>* riseParam    = nullptr;
    std::atomic<float>* octave1Param = nullptr;
    std::atomic<float>* octave2Param = nullptr;

    SmoothedParameters smoothedParams;

    float currentGain   = ParameterIDs::Defaults::gain;
    float currentGlare  = ParameterIDs::Defaults::glare;
    float currentBlend  = ParameterIDs::Defaults::blend;
    float currentLevel  = ParameterIDs::Defaults::level;
    float currentSpeed  = ParameterIDs::Defaults::speed;
    float currentChaos  = ParameterIDs::Defaults::chaos;
    float currentRise   = ParameterIDs::Defaults::rise;
    bool  currentOctave1 = ParameterIDs::Defaults::octave1;
    bool  currentOctave2 = ParameterIDs::Defaults::octave2;

    DSP::InputConditioner inputConditioner;
    DSP::FuzzEngine fuzzEngine;
    DSP::OctaveGenerator octaveGenerator;
    DSP::DynamicGate dynamicGate;
    DSP::BlendMixer blendMixer;
    DSP::PitchShifter pitchShifter;
    DSP::ChaosModulator chaosModulator;
    DSP::EnvelopeFollower inputEnvelopeFollower;
    DSP::EnvelopeFollower chaosEnvelopeFollower;
    DSP::OutputLimiter outputLimiter;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> stagingBuffer;

    float inputEnvelope = 0.0f;
    float chaosEnvelope = 0.0f;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    bool isFirstBlock = true;
    bool testModeEnabled = false;

    // Signal metering
    SignalMeters signalMeters;

    // Latency tracking
    int totalLatencySamples = 0;
    static constexpr int pitchShifterLatency = 64;

    // Stability safeguards
    bool stabilityError = false;
    int consecutiveHighLevelBlocks = 0;
    static constexpr int maxConsecutiveHighLevelBlocks = 10;
    static constexpr float internalClipThreshold = 4.0f;
    static constexpr float safetyClipThreshold = 8.0f;

    // Visualization data (lock-free for GUI)
    LockFreeFifo<float, waveformFifoSize> waveformFifo;
    std::atomic<float> chaosModValue{0.0f};

    // Gain staging helpers
    float measurePeakLevel(const juce::AudioBuffer<float>& buffer) const;
    void applyInterstageProtection(juce::AudioBuffer<float>& buffer);
    void checkAndReportClipping(float level, bool isInput, bool isOutput);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackheartAudioProcessor)
};
