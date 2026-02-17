#include "FuzzEngine.h"
#include "LookupTables.h"

namespace DSP
{

void FuzzEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = static_cast<int>(spec.maximumBlockSize);

    LookupTables::initialize();

    oversampling.initProcessing(static_cast<size_t>(maxBlockSize));
    oversampledRate = sampleRate * 2.0;  // 2x oversampling

    const double smoothingTime = 0.02;
    gain.reset(oversampledRate, smoothingTime);
    level.reset(oversampledRate, smoothingTime);
    shape.reset(oversampledRate, 0.02);

    juce::dsp::ProcessSpec osSpec;
    osSpec.sampleRate = oversampledRate;
    osSpec.maximumBlockSize = spec.maximumBlockSize * 2;
    osSpec.numChannels = spec.numChannels;

    // Prepare all filters at oversampled rate
    preEqHP.prepare(osSpec);
    preEqPeak.prepare(osSpec);
    preEqShelf.prepare(osSpec);
    postEqLP.prepare(osSpec);
    postEqPeak.prepare(osSpec);
    shapeMidEQ.prepare(osSpec);
    shapeLowEQ.prepare(osSpec);
    dcBlocker.prepare(osSpec);

    // DC blocker: 20Hz highpass
    dcBlocker.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    dcBlocker.setCutoffFrequency(20.0f);
    dcBlocker.setResonance(0.707f);

    // SHAPE filters initial config
    shapeMidEQ.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    shapeLowEQ.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    // Envelope coefficients
    attackCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * 0.001f));
    releaseCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * 0.05f));
    sagAttackCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * 0.2f));
    sagReleaseCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * 0.5f));

    compressionEnvelope = 0.0f;
    sagEnvelope = 0.0f;
    biasDriftPhase = 0.0f;
    impedanceLPFCutoff = 8000.0f;
    lastShapeValue = -1.0f;

    configureFiltersForMode(currentMode);
}

void FuzzEngine::reset()
{
    const double smoothingTime = 0.02;
    gain.reset(oversampledRate, smoothingTime);
    level.reset(oversampledRate, smoothingTime);
    shape.reset(oversampledRate, 0.02);

    oversampling.reset();
    preEqHP.reset();
    preEqPeak.reset();
    preEqShelf.reset();
    postEqLP.reset();
    postEqPeak.reset();
    shapeMidEQ.reset();
    shapeLowEQ.reset();
    dcBlocker.reset();

    compressionEnvelope = 0.0f;
    sagEnvelope = 0.0f;
    biasDriftPhase = 0.0f;
    lastShapeValue = -1.0f;
}

void FuzzEngine::configureFiltersForMode(int mode)
{
    // Pre-clip EQ
    preEqHP.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    preEqPeak.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    preEqShelf.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    // Post-clip EQ
    postEqLP.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    postEqPeak.setType(juce::dsp::StateVariableTPTFilterType::bandpass);

    switch (mode)
    {
        case ModeScreaming:
            // Pre: HPF 120Hz, peak at 2.5kHz Q=2
            preEqHP.setCutoffFrequency(120.0f);
            preEqHP.setResonance(0.707f);
            preEqPeak.setCutoffFrequency(2500.0f);
            preEqPeak.setResonance(2.0f);
            preEqShelf.setCutoffFrequency(20000.0f);  // Effectively bypassed
            preEqShelf.setResonance(0.707f);
            // Post: LPF 9kHz, presence at 3kHz (preserves pick attack for metal)
            postEqLP.setCutoffFrequency(9000.0f);
            postEqLP.setResonance(0.707f);
            postEqPeak.setCutoffFrequency(3000.0f);
            postEqPeak.setResonance(1.0f);
            // Tighter, harder clipping
            modeAsymmetry = 0.4f;
            modeDriveScale = 1.2f;
            break;

        case ModeDoom:
            // Pre: HPF 40Hz, peak at 400Hz Q=1.5, darken highs
            preEqHP.setCutoffFrequency(40.0f);
            preEqHP.setResonance(0.707f);
            preEqPeak.setCutoffFrequency(400.0f);
            preEqPeak.setResonance(1.5f);
            preEqShelf.setCutoffFrequency(3000.0f);  // Rolls off highs before clipping
            preEqShelf.setResonance(0.5f);
            // Post: LPF 4kHz, low presence at 200Hz
            postEqLP.setCutoffFrequency(4000.0f);
            postEqLP.setResonance(0.707f);
            postEqPeak.setCutoffFrequency(200.0f);
            postEqPeak.setResonance(0.8f);
            // Maximum headroom, slower compression
            modeAsymmetry = 0.25f;
            modeDriveScale = 0.85f;
            break;

        case ModeOverdrive:
        default:
            // Pre: HPF 80Hz, mild peak at 1kHz
            preEqHP.setCutoffFrequency(80.0f);
            preEqHP.setResonance(0.707f);
            preEqPeak.setCutoffFrequency(1000.0f);
            preEqPeak.setResonance(0.7f);
            preEqShelf.setCutoffFrequency(20000.0f);  // Bypassed
            preEqShelf.setResonance(0.707f);
            // Post: LPF 8kHz, mild presence
            postEqLP.setCutoffFrequency(8000.0f);
            postEqLP.setResonance(0.707f);
            postEqPeak.setCutoffFrequency(1500.0f);
            postEqPeak.setResonance(0.6f);
            // Softer saturation, more dynamic range
            modeAsymmetry = 0.2f;
            modeDriveScale = 0.6f;
            break;
    }
}

float FuzzEngine::germaniumWaveshape(float sample, float drive, float asymmetry) const
{
    const float driven = sample * drive;

    float shaped;
    if (driven >= 0.0f)
    {
        // Positive half: softer germanium onset — tanh with gradual saturation
        shaped = LookupTables::fastTanh(driven * 0.8f);
    }
    else
    {
        // Negative half: harder clipping, lower threshold (PNP germanium asymmetry)
        const float asymDrive = 1.0f + asymmetry * 2.5f;
        shaped = -LookupTables::fastTanh(-driven * asymDrive * 0.6f);
        // Add even harmonic content
        shaped += asymmetry * 0.08f * LookupTables::fastTanhPoly(driven * 2.0f);
    }

    return shaped;
}

void FuzzEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);

    // Upsample
    auto oversampledBlock = oversampling.processSamplesUp(block);

    const auto numSamples = oversampledBlock.getNumSamples();
    const auto numChannels = oversampledBlock.getNumChannels();

    // Pre-clip EQ (mode-dependent voicing)
    {
        juce::dsp::ProcessContextReplacing<float> ctx(oversampledBlock);
        preEqHP.process(ctx);
    }

    // Apply pre-EQ peak boost: read from bandpass, mix back into signal
    // StateVariableTPT bandpass outputs only the band — we add it back for a peak boost
    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        float* data = oversampledBlock.getChannelPointer(ch);
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float dry = data[i];
            const float band = preEqPeak.processSample(static_cast<int>(ch), dry);
            // Peak boost: original + scaled bandpass
            float boostAmount;
            if (currentMode == ModeScreaming)
                boostAmount = 2.0f;   // +6dB equivalent
            else if (currentMode == ModeDoom)
                boostAmount = 2.5f;   // +8dB equivalent
            else
                boostAmount = 1.0f;   // +3dB equivalent
            data[i] = dry + band * boostAmount;
        }
    }

    // Pre-clip shelf (doom mode darkening)
    if (currentMode == ModeDoom)
    {
        juce::dsp::ProcessContextReplacing<float> ctx(oversampledBlock);
        preEqShelf.process(ctx);
    }

    // Germanium gain stage — per-sample processing
    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        const float currentGain = gain.getNextValue();
        const float currentLevel = level.getNextValue();
        const float currentShapeVal = shape.getNextValue();

        // Gain curve: gain^1.5 for germanium-style gradual onset
        const float gainCurved = std::pow(currentGain, 1.5f);
        const float baseDrive = minDrive + gainCurved * (maxDrive - minDrive);
        const float drive = baseDrive * modeDriveScale;

        // Voltage sag: reduce clipping headroom under sustained signal
        const float sagFactor = 1.0f - sagEnvelope * 0.3f;

        // Bias drift: slow LFO modulated by envelope
        biasDriftPhase += 0.05f / static_cast<float>(oversampledRate);
        if (biasDriftPhase >= 1.0f) biasDriftPhase -= 1.0f;
        const float biasDrift = LookupTables::fastSin(biasDriftPhase) *
                                0.02f * (0.3f + compressionEnvelope * 0.7f);

        // Update SHAPE EQ periodically (every 64 samples)
        if ((sample & 63) == 0 && std::abs(currentShapeVal - lastShapeValue) > 0.005f)
        {
            lastShapeValue = currentShapeVal;

            // Mid sweep: 400Hz (shape=0) -> 800Hz (0.5) -> 2kHz (1.0)
            const float midFreq = 400.0f * std::pow(5.0f, currentShapeVal);
            // Q: 0.5 (shape=0) -> 0.7 (0.5) -> 3.0 (1.0)
            const float midQ = 0.5f + currentShapeVal * currentShapeVal * 2.5f;
            shapeMidEQ.setCutoffFrequency(midFreq);
            shapeMidEQ.setResonance(midQ);

            // Low shelf: rolls off more at shape=0, boosts at shape=1
            const float lowFreq = 200.0f;
            shapeLowEQ.setCutoffFrequency(lowFreq);
            shapeLowEQ.setResonance(0.5f + currentShapeVal * 0.3f);
        }

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            float* data = oversampledBlock.getChannelPointer(ch);
            float inputSample = data[sample];

            // Input impedance interaction: LPF cutoff tracks input level
            // (simulates pickup loading into low-Z germanium input)
            const float inputLevel = std::abs(inputSample);
            impedanceLPFCutoff += 0.001f * (juce::jmap(inputLevel, 0.0f, 0.5f, 2000.0f, 8000.0f) - impedanceLPFCutoff);

            // Envelope follower for compression
            const float envCoeff = inputLevel > compressionEnvelope ? attackCoeff : releaseCoeff;
            compressionEnvelope = compressionEnvelope * envCoeff + inputLevel * (1.0f - envCoeff);

            // Sag envelope (slower)
            const float sagCoeff = inputLevel > sagEnvelope ? sagAttackCoeff : sagReleaseCoeff;
            sagEnvelope = sagEnvelope * sagCoeff + inputLevel * (1.0f - sagCoeff);

            // Soft compression before clipping
            const float threshold = (0.3f + (1.0f - currentGain) * 0.5f) * sagFactor;
            const float absLevel = std::abs(inputSample);
            if (absLevel > threshold && absLevel > 0.0f)
            {
                constexpr float ratio = 4.0f;
                const float compGain = (threshold + (absLevel - threshold) / ratio) / absLevel;
                inputSample *= compGain;
            }

            // Add bias drift
            inputSample += biasDrift;

            // Germanium waveshaping
            float shaped = germaniumWaveshape(inputSample, drive, modeAsymmetry);

            // Hard limit safety
            shaped = juce::jlimit(-0.95f, 0.95f, shaped);

            // Makeup gain + level
            const float makeupGain = 1.0f / (1.0f + drive * 0.005f);

            // Level: map 0-1 to -inf..+24dB
            const float levelDb = currentLevel * 48.0f - 24.0f;  // 0->-24dB, 0.5->0dB, 1.0->+24dB
            const float levelGain = juce::Decibels::decibelsToGain(levelDb, -96.0f);

            data[sample] = shaped * makeupGain * levelGain;
        }
    }

    // DC blocker (removes bias drift residual)
    {
        juce::dsp::ProcessContextReplacing<float> ctx(oversampledBlock);
        dcBlocker.process(ctx);
    }

    // Post-clip EQ (mode-dependent)
    {
        juce::dsp::ProcessContextReplacing<float> ctx(oversampledBlock);
        postEqLP.process(ctx);
    }

    // Post-clip presence boost (additive bandpass)
    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        float* data = oversampledBlock.getChannelPointer(ch);
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float dry = data[i];
            const float band = postEqPeak.processSample(static_cast<int>(ch), dry);
            data[i] = dry + band * 0.5f;
        }
    }

    // SHAPE EQ: apply mid sweep as additive peak + low shelf blend
    {
        const float shapeVal = lastShapeValue >= 0.0f ? lastShapeValue : 0.5f;
        // Shape gain: +3dB at 0, 0dB at 0.5, +9dB at 1.0
        float shapeGain;
        if (shapeVal < 0.5f)
            shapeGain = 1.0f + (0.5f - shapeVal) * 0.8f;   // 1.4 -> 1.0
        else
            shapeGain = 1.0f + (shapeVal - 0.5f) * 4.6f;    // 1.0 -> 3.3 (~+9dB)

        // Low shelf influence: cuts low at shape=0, boosts at shape=1
        const float lowGain = (shapeVal - 0.5f) * 2.0f;  // -1 to +1

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            float* data = oversampledBlock.getChannelPointer(ch);
            for (size_t i = 0; i < numSamples; ++i)
            {
                const float dry = data[i];
                const float mid = shapeMidEQ.processSample(static_cast<int>(ch), dry);
                const float low = shapeLowEQ.processSample(static_cast<int>(ch), dry);
                data[i] = dry + mid * (shapeGain - 1.0f) + (low - dry) * lowGain * 0.3f;
            }
        }
    }

    // Downsample
    oversampling.processSamplesDown(block);
}

void FuzzEngine::setGain(float normalizedGain)
{
    gain.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedGain));
}

void FuzzEngine::setLevel(float normalizedLevel)
{
    level.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedLevel));
}

void FuzzEngine::setMode(int mode)
{
    mode = juce::jlimit(0, 2, mode);
    if (mode != currentMode)
    {
        currentMode = mode;
        configureFiltersForMode(mode);
    }
}

void FuzzEngine::setShape(float normalizedShape)
{
    shape.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedShape));
}

} // namespace DSP
