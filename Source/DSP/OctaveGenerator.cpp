#include "OctaveGenerator.h"

namespace DSP
{

void OctaveGenerator::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = static_cast<int>(spec.maximumBlockSize);
    numChannels = static_cast<int>(spec.numChannels);

    const double smoothingTime = 0.02;
    glare.reset(sampleRate, smoothingTime);

    // Initialize 2x oversampling
    oversampling.initProcessing(static_cast<size_t>(maxBlockSize));

    // Filters run at oversampled rate
    const double osRate = sampleRate * 2.0;
    juce::dsp::ProcessSpec osSpec;
    osSpec.sampleRate = osRate;
    osSpec.maximumBlockSize = spec.maximumBlockSize * 2;
    osSpec.numChannels = spec.numChannels;

    preEmphasisHP.prepare(osSpec);
    preEmphasisHP.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    preEmphasisHP.setCutoffFrequency(preHPFreq);
    preEmphasisHP.setResonance(0.707f);

    preEmphasisLP.prepare(osSpec);
    preEmphasisLP.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    preEmphasisLP.setCutoffFrequency(preLPFreq);
    preEmphasisLP.setResonance(0.707f);

    dcBlockFilter.prepare(osSpec);
    dcBlockFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    dcBlockFilter.setCutoffFrequency(dcBlockFreq);
    dcBlockFilter.setResonance(0.707f);

    octaveBandpass.prepare(osSpec);
    octaveBandpass.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    octaveBandpass.setCutoffFrequency(bandpassFreq);
    octaveBandpass.setResonance(bandpassQ);

    octaveHighShelf.prepare(osSpec);
    octaveHighShelf.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    // 150Hz: trims sub-octave rumble while passing the doubled low-E (164Hz).
    // The old 800Hz corner removed the octave fundamental for most of the neck.
    octaveHighShelf.setCutoffFrequency(emphasisHPFreq);
    octaveHighShelf.setResonance(0.5f);

    // Pre-allocate buffer to avoid audio thread allocation
    octaveBuffer.setSize(numChannels, maxBlockSize, false, true, true);

    lastOctaveLevel = 0.0f;
}

void OctaveGenerator::reset()
{
    glare.reset(sampleRate, 0.02);

    oversampling.reset();
    preEmphasisHP.reset();
    preEmphasisLP.reset();
    dcBlockFilter.reset();
    octaveBandpass.reset();
    octaveHighShelf.reset();

    lastOctaveLevel = 0.0f;
}

void OctaveGenerator::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    // Buffer pre-allocated in prepare(); never allocate on the audio thread.
    // Undersized means the host violated the prepare contract — pass through dry.
    if (octaveBuffer.getNumSamples() < numSamples || octaveBuffer.getNumChannels() < channels)
        return;

    // Copy input to octave buffer
    for (int ch = 0; ch < channels; ++ch)
    {
        juce::FloatVectorOperations::copy(octaveBuffer.getWritePointer(ch),
                                          buffer.getReadPointer(ch),
                                          numSamples);
    }

    // Create AudioBlock for oversampling
    juce::dsp::AudioBlock<float> octaveBlock(octaveBuffer.getArrayOfWritePointers(),
                                              static_cast<size_t>(channels),
                                              static_cast<size_t>(numSamples));

    // Upsample to 2x rate
    auto oversampledBlock = oversampling.processSamplesUp(octaveBlock);
    const auto osNumSamples = oversampledBlock.getNumSamples();
    const auto osNumChannels = oversampledBlock.getNumChannels();

    // Pre-emphasis filters at oversampled rate
    {
        juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);
        preEmphasisHP.process(context);
    }
    {
        juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);
        preEmphasisLP.process(context);
    }

    // Full-wave rectification at oversampled rate. No smoothing: a lowpass here
    // turns the rectified wave into an amplitude envelope and strips the doubled
    // frequency the whole stage exists to produce — DC block below handles offset
    for (size_t channel = 0; channel < osNumChannels; ++channel)
    {
        float* octaveData = oversampledBlock.getChannelPointer(channel);

        for (size_t sample = 0; sample < osNumSamples; ++sample)
            octaveData[sample] = std::abs(octaveData[sample]);
    }

    // Post-rectification filtering at oversampled rate
    {
        juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);
        dcBlockFilter.process(context);
    }
    {
        juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);
        octaveBandpass.process(context);
    }
    {
        juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);
        octaveHighShelf.process(context);
    }

    // Downsample back to native rate
    oversampling.processSamplesDown(octaveBlock);

    // Mix octave into output - optimized with reduced branching
    float octaveLevelSum = 0.0f;

    // Check if glare is smoothing for optimized path
    if (!glare.isSmoothing())
    {
        const float currentGlare = glare.getTargetValue();
        // Use glare^1.5 curve for smoother blend progression (less abrupt than glare^2)
        const float glareCurved = std::pow(currentGlare, 1.5f);
        const float octaveGain = glareCurved * 1.2f;

        if (octaveGain > 0.0001f)
        {
            for (int channel = 0; channel < channels; ++channel)
            {
                float* outputData = buffer.getWritePointer(channel);
                const float* octaveData = octaveBuffer.getReadPointer(channel);

                // SIMD-optimized add with gain
                juce::FloatVectorOperations::addWithMultiply(outputData, octaveData, octaveGain, numSamples);

                // Calculate level for metering
                octaveLevelSum += juce::FloatVectorOperations::findMaximum(octaveData, numSamples) * octaveGain;
            }
        }
    }
    else
    {
        // Per-sample processing when smoothing
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float currentGlare = glare.getNextValue();
            const float glareCurved = std::pow(currentGlare, 1.5f);
            const float octaveGain = glareCurved * 1.2f;

            for (int channel = 0; channel < channels; ++channel)
            {
                const float dry = buffer.getSample(channel, sample);
                const float octave = octaveBuffer.getSample(channel, sample);
                const float octaveContribution = octave * octaveGain;

                buffer.setSample(channel, sample, dry + octaveContribution);
                octaveLevelSum += std::abs(octaveContribution);
            }
        }
    }

    lastOctaveLevel = octaveLevelSum / static_cast<float>(numSamples * channels + 1);
}

void OctaveGenerator::setGlare(float normalizedGlare)
{
    glare.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedGlare));
}

} // namespace DSP
