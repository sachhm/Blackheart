#include "BlendMixer.h"
#include "LookupTables.h"

namespace DSP
{

void BlendMixer::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = static_cast<int>(spec.maximumBlockSize);

    // Initialize lookup tables (thread-safe, only runs once)
    LookupTables::initialize();

    const double smoothingTime = 0.03;
    blend.reset(sampleRate, smoothingTime);

    lastDryGain = 1.0f;
    lastWetGain = 0.0f;
}

void BlendMixer::reset()
{
    blend.reset(sampleRate, 0.03);
    lastDryGain = 1.0f;
    lastWetGain = 0.0f;
}

void BlendMixer::calculateGains(float blendValue, float& dryGain, float& wetGain) const
{
    // Use lookup table for equal-power crossfade
    LookupTables::equalPowerGains(blendValue, dryGain, wetGain);
}

void BlendMixer::process(const juce::AudioBuffer<float>& dryBuffer,
                         const juce::AudioBuffer<float>& wetBuffer,
                         juce::AudioBuffer<float>& outputBuffer)
{
    const int numSamples = outputBuffer.getNumSamples();
    const int numChannels = outputBuffer.getNumChannels();

    jassert(dryBuffer.getNumSamples() >= numSamples);
    jassert(wetBuffer.getNumSamples() >= numSamples);
    jassert(dryBuffer.getNumChannels() >= numChannels);
    jassert(wetBuffer.getNumChannels() >= numChannels);

    // Check if blend is smoothing for optimized path
    if (!blend.isSmoothing())
    {
        const float currentBlend = blend.getTargetValue();
        float dryGain, wetGain;
        calculateGains(currentBlend, dryGain, wetGain);

        lastDryGain = dryGain;
        lastWetGain = wetGain;

        // SIMD-optimized block processing
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* output = outputBuffer.getWritePointer(channel);
            const float* dry = dryBuffer.getReadPointer(channel);
            const float* wet = wetBuffer.getReadPointer(channel);

            // Use SIMD operations for bulk processing
            juce::FloatVectorOperations::copyWithMultiply(output, dry, dryGain, numSamples);
            juce::FloatVectorOperations::addWithMultiply(output, wet, wetGain, numSamples);
        }
    }
    else
    {
        // Per-sample processing when smoothing
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float currentBlend = blend.getNextValue();

            float dryGain, wetGain;
            calculateGains(currentBlend, dryGain, wetGain);

            lastDryGain = dryGain;
            lastWetGain = wetGain;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float drySample = dryBuffer.getSample(channel, sample);
                const float wetSample = wetBuffer.getSample(channel, sample);
                const float mixed = drySample * dryGain + wetSample * wetGain;
                outputBuffer.setSample(channel, sample, mixed);
            }
        }
    }
}

void BlendMixer::setBlend(float normalizedBlend)
{
    blend.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedBlend));
}

} // namespace DSP
