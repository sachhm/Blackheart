#include "OutputLimiter.h"
#include "LookupTables.h"

namespace DSP
{

void OutputLimiter::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = static_cast<int>(spec.maximumBlockSize);

    // Initialize lookup tables (thread-safe, only runs once)
    LookupTables::initialize();

    const double smoothingTime = 0.02;
    outputLevel.reset(sampleRate, smoothingTime);
    gainReduction.reset(sampleRate, 0.005);

    attackCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate) * attackTimeMs * 0.001f));
    releaseCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate) * releaseTimeMs * 0.001f));

    envelope = 0.0f;
    lastGainReduction = 0.0f;

    dcBlockFilter.prepare(spec);
    dcBlockFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    dcBlockFilter.setCutoffFrequency(dcBlockFreq);
    dcBlockFilter.setResonance(0.707f);
}

void OutputLimiter::reset()
{
    outputLevel.reset(sampleRate, 0.02);
    gainReduction.reset(sampleRate, 0.005);
    envelope = 0.0f;
    lastGainReduction = 0.0f;
    dcBlockFilter.reset();
}

float OutputLimiter::softClip(float sample) const
{
    // Use fast polynomial tanh approximation
    return LookupTables::fastTanhPoly(sample);
}

float OutputLimiter::processSaturation(float sample, float drive) const
{
    const float absInput = std::abs(sample);
    const float sign = sample >= 0.0f ? 1.0f : -1.0f;

    if (absInput <= saturationKnee)
    {
        return sample;
    }
    else
    {
        const float overKnee = absInput - saturationKnee;
        const float headroomAboveKnee = 1.0f - saturationKnee;

        // Use fast tanh approximation
        const float saturated = saturationKnee + headroomAboveKnee * LookupTables::fastTanhPoly(overKnee * drive / headroomAboveKnee);

        return sign * saturated;
    }
}

void OutputLimiter::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // DC block using JUCE DSP
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        dcBlockFilter.process(context);
    }

    // Check if level is smoothing for optimized path
    if (!outputLevel.isSmoothing())
    {
        const float level = outputLevel.getTargetValue();

        // Apply gain to entire buffer using SIMD
        if (std::abs(level - 1.0f) > 0.0001f)
        {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                juce::FloatVectorOperations::multiply(buffer.getWritePointer(channel), level, numSamples);
            }
        }

        // Process limiting
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float maxAbsLevel = 0.0f;
            for (int channel = 0; channel < numChannels; ++channel)
            {
                maxAbsLevel = std::max(maxAbsLevel, std::abs(buffer.getSample(channel, sample)));
            }

            const float coeff = maxAbsLevel > envelope ? attackCoeff : releaseCoeff;
            envelope = envelope * coeff + maxAbsLevel * (1.0f - coeff);

            float targetGainReduction = 1.0f;

            if (envelope > headroom)
            {
                const float overHeadroom = envelope - headroom;
                const float maxOver = ceiling - headroom;

                if (maxOver > 0.0f)
                {
                    const float compressionAmount = std::min(1.0f, overHeadroom / maxOver);
                    targetGainReduction = 1.0f - compressionAmount * 0.3f;
                }
            }

            if (envelope > ceiling)
            {
                targetGainReduction = ceiling / envelope;
            }

            gainReduction.setTargetValue(targetGainReduction);
            const float currentGainReduction = gainReduction.getNextValue();
            lastGainReduction = 1.0f - currentGainReduction;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                float inputSample = buffer.getSample(channel, sample);
                inputSample *= currentGainReduction;
                const float saturated = processSaturation(inputSample, 1.5f);
                const float limited = juce::jlimit(-ceiling, ceiling, saturated);
                buffer.setSample(channel, sample, limited);
            }
        }
    }
    else
    {
        // Per-sample processing when level is smoothing
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float level = outputLevel.getNextValue();

            float maxAbsLevel = 0.0f;
            for (int channel = 0; channel < numChannels; ++channel)
            {
                float inputSample = buffer.getSample(channel, sample);
                inputSample *= level;
                buffer.setSample(channel, sample, inputSample);
                maxAbsLevel = std::max(maxAbsLevel, std::abs(inputSample));
            }

            const float coeff = maxAbsLevel > envelope ? attackCoeff : releaseCoeff;
            envelope = envelope * coeff + maxAbsLevel * (1.0f - coeff);

            float targetGainReduction = 1.0f;

            if (envelope > headroom)
            {
                const float overHeadroom = envelope - headroom;
                const float maxOver = ceiling - headroom;

                if (maxOver > 0.0f)
                {
                    const float compressionAmount = std::min(1.0f, overHeadroom / maxOver);
                    targetGainReduction = 1.0f - compressionAmount * 0.3f;
                }
            }

            if (envelope > ceiling)
            {
                targetGainReduction = ceiling / envelope;
            }

            gainReduction.setTargetValue(targetGainReduction);
            const float currentGainReduction = gainReduction.getNextValue();
            lastGainReduction = 1.0f - currentGainReduction;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                float inputSample = buffer.getSample(channel, sample);
                inputSample *= currentGainReduction;
                const float saturated = processSaturation(inputSample, 1.5f);
                const float limited = juce::jlimit(-ceiling, ceiling, saturated);
                buffer.setSample(channel, sample, limited);
            }
        }
    }
}

void OutputLimiter::setOutputLevel(float normalizedLevel)
{
    outputLevel.setTargetValue(juce::jlimit(0.0f, 1.5f, normalizedLevel));
}

void OutputLimiter::setCeiling(float ceilingDb)
{
    ceiling = juce::jlimit(0.5f, 1.0f, juce::Decibels::decibelsToGain(ceilingDb));
    headroom = ceiling * 0.9f;
}

void OutputLimiter::setHeadroom(float headroomDb)
{
    const float headroomLinear = juce::Decibels::decibelsToGain(headroomDb);
    headroom = juce::jlimit(0.3f, ceiling * 0.95f, headroomLinear);
}

} // namespace DSP
