#include "InputConditioner.h"

namespace DSP
{

void InputConditioner::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = static_cast<int>(spec.numChannels);
    maxBlockSize = static_cast<int>(spec.maximumBlockSize);

    inputGain.reset(sampleRate, 0.02);

    dcBlockCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, dcBlockCutoffHz);

    for (auto& filter : dcBlockFilters)
    {
        filter.coefficients = dcBlockCoeffs;
        filter.reset();
    }

    antiAliasingFilter.prepare(spec);
    antiAliasingFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    const float nyquist = static_cast<float>(sampleRate) * 0.5f;
    const float cutoff = std::min(nyquist * 0.9f, 20000.0f);
    antiAliasingFilter.setCutoffFrequency(cutoff);
    antiAliasingFilter.setResonance(0.707f);
}

void InputConditioner::reset()
{
    inputGain.reset(sampleRate, 0.02);

    for (auto& filter : dcBlockFilters)
        filter.reset();

    antiAliasingFilter.reset();
}

void InputConditioner::process(juce::AudioBuffer<float>& buffer)
{
    if (monoSumEnabled && buffer.getNumChannels() > 1)
        processMonoSum(buffer);

    if (dcBlockEnabled)
        processDCBlock(buffer);

    processGain(buffer);

    if (antiAliasingEnabled)
        processAntiAliasing(buffer);
}

void InputConditioner::processDCBlock(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    for (int channel = 0; channel < std::min(channels, 2); ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            channelData[sample] = dcBlockFilters[static_cast<size_t>(channel)]
                                      .processSample(channelData[sample]);
        }
    }
}

void InputConditioner::processGain(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    if (inputGain.isSmoothing())
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float gain = inputGain.getNextValue();

            for (int channel = 0; channel < channels; ++channel)
            {
                buffer.setSample(channel, sample,
                                 buffer.getSample(channel, sample) * gain);
            }
        }
    }
    else
    {
        const float gain = inputGain.getTargetValue();
        if (std::abs(gain - 1.0f) > 0.0001f)
        {
            for (int channel = 0; channel < channels; ++channel)
                buffer.applyGain(channel, 0, numSamples, gain);
        }
    }
}

void InputConditioner::processAntiAliasing(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    antiAliasingFilter.process(context);
}

void InputConditioner::processMonoSum(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    if (channels < 2)
        return;

    float* leftChannel = buffer.getWritePointer(0);
    const float* rightChannel = buffer.getReadPointer(1);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float mono = (leftChannel[sample] + rightChannel[sample]) * 0.5f;
        leftChannel[sample] = mono;
    }

    for (int channel = 1; channel < channels; ++channel)
        buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);
}

void InputConditioner::setInputGain(float gainDb)
{
    const float linear = juce::Decibels::decibelsToGain(gainDb);
    inputGain.setTargetValue(linear);
}

void InputConditioner::setInputGainLinear(float gainLinear)
{
    inputGain.setTargetValue(std::max(0.0f, gainLinear));
}

} // namespace DSP
