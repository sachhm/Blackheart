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

    preEmphasisHP.prepare(spec);
    preEmphasisHP.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    preEmphasisHP.setCutoffFrequency(preHPFreq);
    preEmphasisHP.setResonance(0.707f);

    preEmphasisLP.prepare(spec);
    preEmphasisLP.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    preEmphasisLP.setCutoffFrequency(preLPFreq);
    preEmphasisLP.setResonance(0.707f);

    dcBlockFilter.prepare(spec);
    dcBlockFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    dcBlockFilter.setCutoffFrequency(dcBlockFreq);
    dcBlockFilter.setResonance(0.707f);

    octaveBandpass.prepare(spec);
    octaveBandpass.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    octaveBandpass.setCutoffFrequency(bandpassFreq);
    octaveBandpass.setResonance(bandpassQ);

    octaveHighShelf.prepare(spec);
    octaveHighShelf.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    octaveHighShelf.setCutoffFrequency(800.0f);
    octaveHighShelf.setResonance(0.5f);

    // Pre-allocate buffer to avoid audio thread allocation
    octaveBuffer.setSize(numChannels, maxBlockSize, false, true, true);

    previousSample[0] = 0.0f;
    previousSample[1] = 0.0f;
    lastOctaveLevel = 0.0f;
}

void OctaveGenerator::reset()
{
    glare.reset(sampleRate, 0.02);

    preEmphasisHP.reset();
    preEmphasisLP.reset();
    dcBlockFilter.reset();
    octaveBandpass.reset();
    octaveHighShelf.reset();

    previousSample[0] = 0.0f;
    previousSample[1] = 0.0f;
    lastOctaveLevel = 0.0f;
}

void OctaveGenerator::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    // Ensure octave buffer is large enough (no allocation if already sized)
    if (octaveBuffer.getNumSamples() < numSamples || octaveBuffer.getNumChannels() < channels)
        octaveBuffer.setSize(channels, numSamples, false, false, true);

    // Copy input to octave buffer using SIMD-optimized copy
    for (int ch = 0; ch < channels; ++ch)
    {
        juce::FloatVectorOperations::copy(octaveBuffer.getWritePointer(ch),
                                          buffer.getReadPointer(ch),
                                          numSamples);
    }

    // Process filters on octave buffer
    {
        juce::dsp::AudioBlock<float> octaveBlock(octaveBuffer.getArrayOfWritePointers(),
                                                  static_cast<size_t>(channels),
                                                  static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> context(octaveBlock);
        preEmphasisHP.process(context);
    }

    {
        juce::dsp::AudioBlock<float> octaveBlock(octaveBuffer.getArrayOfWritePointers(),
                                                  static_cast<size_t>(channels),
                                                  static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> context(octaveBlock);
        preEmphasisLP.process(context);
    }

    // Full-wave rectification with smoothing - optimized loop
    for (int channel = 0; channel < channels; ++channel)
    {
        float* octaveData = octaveBuffer.getWritePointer(channel);
        float prevSample = previousSample[static_cast<size_t>(channel)];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Branchless abs using SIMD intrinsic
            const float rectified = std::abs(octaveData[sample]);
            // One-pole lowpass smoothing (0.6/0.4 for slightly looser tracking, more organic feel)
            const float smoothed = rectified * 0.6f + prevSample * 0.4f;
            prevSample = smoothed;
            octaveData[sample] = smoothed;
        }

        previousSample[static_cast<size_t>(channel)] = prevSample;
    }

    // Post-rectification filtering
    {
        juce::dsp::AudioBlock<float> octaveBlock(octaveBuffer.getArrayOfWritePointers(),
                                                  static_cast<size_t>(channels),
                                                  static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> context(octaveBlock);
        dcBlockFilter.process(context);
    }

    {
        juce::dsp::AudioBlock<float> octaveBlock(octaveBuffer.getArrayOfWritePointers(),
                                                  static_cast<size_t>(channels),
                                                  static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> context(octaveBlock);
        octaveBandpass.process(context);
    }

    {
        juce::dsp::AudioBlock<float> octaveBlock(octaveBuffer.getArrayOfWritePointers(),
                                                  static_cast<size_t>(channels),
                                                  static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> context(octaveBlock);
        octaveHighShelf.process(context);
    }

    // Mix octave into output - optimized with reduced branching
    float octaveLevelSum = 0.0f;

    // Check if glare is smoothing for optimized path
    if (!glare.isSmoothing())
    {
        const float currentGlare = glare.getTargetValue();
        // Use glare^1.5 curve for smoother blend progression (less abrupt than glare²)
        const float glareCurved = std::pow(currentGlare, 1.5f);
        const float octaveGain = glareCurved * 1.6f;  // Slightly reduced from 2.0 for better mix balance

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
            // Use glare^1.5 curve for smoother blend progression
            const float glareCurved = std::pow(currentGlare, 1.5f);
            const float octaveGain = glareCurved * 1.6f;

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
