#include "FuzzEngine.h"
#include "LookupTables.h"

namespace DSP
{

void FuzzEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = static_cast<int>(spec.maximumBlockSize);

    // Initialize lookup tables (thread-safe, only runs once)
    LookupTables::initialize();

    oversampling.initProcessing(static_cast<size_t>(maxBlockSize));
    oversampledRate = sampleRate * static_cast<double>(1 << oversamplingFactor);

    const double smoothingTime = 0.02;
    gain.reset(oversampledRate, smoothingTime);
    level.reset(oversampledRate, smoothingTime);
    asymmetryAmount.reset(oversampledRate, smoothingTime);
    toneControl.reset(oversampledRate, smoothingTime);
    biasAmount.reset(oversampledRate, smoothingTime);

    juce::dsp::ProcessSpec oversampledSpec;
    oversampledSpec.sampleRate = oversampledRate;
    oversampledSpec.maximumBlockSize = spec.maximumBlockSize * (1 << oversamplingFactor);
    oversampledSpec.numChannels = spec.numChannels;

    preFilterHP.prepare(oversampledSpec);
    preFilterHP.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    preFilterHP.setCutoffFrequency(preFilterHPFreq);
    preFilterHP.setResonance(0.707f);

    preFilterLP.prepare(oversampledSpec);
    preFilterLP.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    preFilterLP.setCutoffFrequency(preFilterLPFreq);
    preFilterLP.setResonance(0.707f);

    postFilterLP.prepare(oversampledSpec);
    postFilterLP.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    postFilterLP.setCutoffFrequency(postFilterLPFreq);
    postFilterLP.setResonance(0.707f);

    postFilterTone.prepare(oversampledSpec);
    postFilterTone.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    postFilterTone.setCutoffFrequency(4000.0f);
    postFilterTone.setResonance(0.707f);

    // Pre-calculate envelope coefficients
    attackCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * compressionAttack));
    releaseCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * compressionRelease));

    compressionEnvelope = 0.0f;
    lastToneFreq = 4000.0f;
}

void FuzzEngine::reset()
{
    const double smoothingTime = 0.02;
    gain.reset(oversampledRate, smoothingTime);
    level.reset(oversampledRate, smoothingTime);
    asymmetryAmount.reset(oversampledRate, smoothingTime);
    toneControl.reset(oversampledRate, smoothingTime);
    biasAmount.reset(oversampledRate, smoothingTime);

    oversampling.reset();
    preFilterHP.reset();
    preFilterLP.reset();
    postFilterLP.reset();
    postFilterTone.reset();

    compressionEnvelope = 0.0f;
}

void FuzzEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampling.processSamplesUp(block);

    {
        juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);
        preFilterHP.process(context);
    }

    {
        juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);
        preFilterLP.process(context);
    }

    const auto numSamples = oversampledBlock.getNumSamples();
    const auto numChannels = oversampledBlock.getNumChannels();

    // Check if any parameters are smoothing for optimized path selection
    const bool isSmoothing = gain.isSmoothing() || level.isSmoothing() ||
                             asymmetryAmount.isSmoothing() || toneControl.isSmoothing() ||
                             biasAmount.isSmoothing();

    if (!isSmoothing)
    {
        // Optimized path: block processing with constant parameters
        const float currentGain = gain.getTargetValue();
        const float currentLevel = level.getTargetValue();
        const float currentAsymmetry = asymmetryAmount.getTargetValue();
        const float currentTone = toneControl.getTargetValue();
        const float currentBias = biasAmount.getTargetValue();

        // Use gain^1.5 curve for more gradual saturation (less aggressive than gain²)
        const float gainCurved = std::pow(currentGain, 1.5f);
        const float drive = minDrive + gainCurved * (maxDrive - minDrive);
        const float makeupGain = 1.0f / (0.5f + drive * 0.02f);
        const float finalGain = makeupGain * currentLevel;

        // Update tone filter only if changed significantly
        const float toneFreq = 1000.0f + currentTone * 7000.0f;
        if (std::abs(toneFreq - lastToneFreq) > 10.0f)
        {
            postFilterTone.setCutoffFrequency(toneFreq);
            lastToneFreq = toneFreq;
        }

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            float* channelData = oversampledBlock.getChannelPointer(channel);

            for (size_t sample = 0; sample < numSamples; ++sample)
            {
                float inputSample = channelData[sample] + currentBias * 0.1f;

                // Envelope follower
                const float inputLevel = std::abs(inputSample);
                const float envCoeff = inputLevel > compressionEnvelope ? attackCoeff : releaseCoeff;
                compressionEnvelope = compressionEnvelope * envCoeff + inputLevel * (1.0f - envCoeff);

                // Compression
                float compressedSample = processCompressionFast(inputSample, 0.3f + (1.0f - currentGain) * 0.5f);

                // Drive
                float drivenSample = compressedSample * drive;

                // Waveshaping with lookup tables
                float shapedSample;
                if (drivenSample >= 0.0f)
                {
                    shapedSample = softClipFast(drivenSample);
                }
                else
                {
                    const float asymmetricDrive = 1.0f + currentAsymmetry * 2.0f;
                    shapedSample = -softClipFast(-drivenSample * asymmetricDrive) / asymmetricDrive;
                    shapedSample += currentAsymmetry * 0.1f * LookupTables::fastTanhPoly(drivenSample * 3.0f);
                }

                // Hard clip and output
                shapedSample = juce::jlimit(-0.95f, 0.95f, shapedSample);
                channelData[sample] = shapedSample * finalGain;
            }
        }
    }
    else
    {
        // Per-sample path when parameters are smoothing
        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            const float currentGain = gain.getNextValue();
            const float currentLevel = level.getNextValue();
            const float currentAsymmetry = asymmetryAmount.getNextValue();
            const float currentTone = toneControl.getNextValue();
            const float currentBias = biasAmount.getNextValue();

            // Use gain^1.5 curve for more gradual saturation (less aggressive than gain²)
        const float gainCurved = std::pow(currentGain, 1.5f);
        const float drive = minDrive + gainCurved * (maxDrive - minDrive);

            // Update tone filter less frequently (every 64 samples)
            if ((sample & 63) == 0)
            {
                const float toneFreq = 1000.0f + currentTone * 7000.0f;
                postFilterTone.setCutoffFrequency(toneFreq);
            }

            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                float* channelData = oversampledBlock.getChannelPointer(channel);
                float inputSample = channelData[sample] + currentBias * 0.1f;

                const float inputLevel = std::abs(inputSample);
                const float envCoeff = inputLevel > compressionEnvelope ? attackCoeff : releaseCoeff;
                compressionEnvelope = compressionEnvelope * envCoeff + inputLevel * (1.0f - envCoeff);

                float compressedSample = processCompressionFast(inputSample, 0.3f + (1.0f - currentGain) * 0.5f);
                float drivenSample = compressedSample * drive;

                float shapedSample;
                if (drivenSample >= 0.0f)
                {
                    shapedSample = softClipFast(drivenSample);
                }
                else
                {
                    const float asymmetricDrive = 1.0f + currentAsymmetry * 2.0f;
                    shapedSample = -softClipFast(-drivenSample * asymmetricDrive) / asymmetricDrive;
                    shapedSample += currentAsymmetry * 0.1f * LookupTables::fastTanhPoly(drivenSample * 3.0f);
                }

                shapedSample = juce::jlimit(-0.95f, 0.95f, shapedSample);
                const float makeupGain = 1.0f / (0.5f + drive * 0.02f);
                channelData[sample] = shapedSample * makeupGain * currentLevel;
            }
        }
    }

    {
        juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);
        postFilterLP.process(context);
    }

    {
        juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);
        postFilterTone.process(context);
    }

    oversampling.processSamplesDown(block);
}

float FuzzEngine::processCompressionFast(float sample, float threshold) const
{
    const float level = std::abs(sample);

    if (level <= threshold)
        return sample;

    const float overshoot = level - threshold;
    constexpr float ratio = 4.0f;
    constexpr float knee = 0.1f;

    float gain;
    if (overshoot < knee)
    {
        const float kneeRatio = overshoot / knee;
        gain = 1.0f - (1.0f - 1.0f / ratio) * kneeRatio * kneeRatio * 0.5f;
    }
    else
    {
        gain = (threshold + (level - threshold) / ratio) / level;
    }

    return sample * gain;
}

float FuzzEngine::processCompression(float sample, float threshold)
{
    return processCompressionFast(sample, threshold);
}

float FuzzEngine::processAsymmetricWaveshaper(float sample, float drive)
{
    const float driven = sample * drive;

    if (driven >= 0.0f)
    {
        return LookupTables::fastTanh(driven * 1.2f);
    }
    else
    {
        const float asymDrive = drive * 1.5f;
        return LookupTables::fastTanh(driven * 0.8f) + 0.15f * (1.0f - LookupTables::fastExpDecay(driven * asymDrive * 0.1f));
    }
}

float FuzzEngine::softClipFast(float sample) const
{
    // Optimized soft clip using polynomial for common range
    if (sample >= -1.0f && sample <= 1.0f)
    {
        const float x2 = sample * sample;
        return sample - (sample * x2) * 0.333333f;
    }
    else if (sample > 1.0f)
    {
        // Use lookup table for exp
        return 1.0f - LookupTables::fastExpDecay(-sample + 1.0f) * 0.333333f;
    }
    else
    {
        return -1.0f + LookupTables::fastExpDecay(sample + 1.0f) * 0.333333f;
    }
}

float FuzzEngine::softClip(float sample)
{
    return softClipFast(sample);
}

float FuzzEngine::hardClip(float sample, float threshold)
{
    return juce::jlimit(-threshold, threshold, sample);
}

void FuzzEngine::setGain(float normalizedGain)
{
    gain.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedGain));
}

void FuzzEngine::setLevel(float normalizedLevel)
{
    level.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedLevel));
}

void FuzzEngine::setAsymmetry(float asymmetry)
{
    asymmetryAmount.setTargetValue(juce::jlimit(0.0f, 1.0f, asymmetry));
}

void FuzzEngine::setTone(float normalizedTone)
{
    toneControl.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedTone));
}

void FuzzEngine::setBias(float bias)
{
    biasAmount.setTargetValue(juce::jlimit(-1.0f, 1.0f, bias));
}

} // namespace DSP
