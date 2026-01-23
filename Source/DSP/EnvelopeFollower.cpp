#include "EnvelopeFollower.h"

namespace DSP
{

void EnvelopeFollower::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    updateCoefficients();

    envelope = 0.0f;
    rmsSum = 0.0f;
    rmsIndex = 0;
    std::fill(rmsBuffer.begin(), rmsBuffer.end(), 0.0f);

    holdCounter = 0;
    peakHeldValue = 0.0f;
    setHoldTime(holdTimeMs);
}

void EnvelopeFollower::reset()
{
    envelope = 0.0f;
    rmsSum = 0.0f;
    rmsIndex = 0;
    std::fill(rmsBuffer.begin(), rmsBuffer.end(), 0.0f);
    holdCounter = 0;
    peakHeldValue = 0.0f;
}

void EnvelopeFollower::updateCoefficients()
{
    if (attackTimeMs > 0.0f)
        attackCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate) * attackTimeMs * 0.001f));
    else
        attackCoeff = 0.0f;

    if (releaseTimeMs > 0.0f)
        releaseCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate) * releaseTimeMs * 0.001f));
    else
        releaseCoeff = 0.0f;
}

float EnvelopeFollower::processSample(float inputSample)
{
    const float inputLevel = std::abs(inputSample) * sensitivity;

    switch (detectionMode)
    {
        case DetectionMode::Peak:
            return processPeak(inputLevel);

        case DetectionMode::RMS:
            return processRMS(inputLevel);

        case DetectionMode::PeakHold:
            return processPeakHold(inputLevel);

        default:
            return processPeak(inputLevel);
    }
}

float EnvelopeFollower::processPeak(float inputLevel)
{
    if (inputLevel > envelope)
        envelope = attackCoeff * envelope + (1.0f - attackCoeff) * inputLevel;
    else
        envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * inputLevel;

    return envelope;
}

float EnvelopeFollower::processRMS(float inputLevel)
{
    const float squared = inputLevel * inputLevel;

    rmsSum -= rmsBuffer[static_cast<size_t>(rmsIndex)];
    rmsBuffer[static_cast<size_t>(rmsIndex)] = squared;
    rmsSum += squared;

    rmsIndex = (rmsIndex + 1) % rmsWindowSize;

    const float rmsValue = std::sqrt(rmsSum / static_cast<float>(rmsWindowSize));

    if (rmsValue > envelope)
        envelope = attackCoeff * envelope + (1.0f - attackCoeff) * rmsValue;
    else
        envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * rmsValue;

    return envelope;
}

float EnvelopeFollower::processPeakHold(float inputLevel)
{
    if (inputLevel > peakHeldValue)
    {
        peakHeldValue = inputLevel;
        holdCounter = holdSamples;
    }
    else if (holdCounter > 0)
    {
        --holdCounter;
    }
    else
    {
        peakHeldValue = releaseCoeff * peakHeldValue;
    }

    if (peakHeldValue > envelope)
        envelope = attackCoeff * envelope + (1.0f - attackCoeff) * peakHeldValue;
    else
        envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * peakHeldValue;

    return envelope;
}

float EnvelopeFollower::processBlock(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float maxLevel = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            maxLevel = std::max(maxLevel, std::abs(buffer.getSample(channel, sample)));
        }
        processSample(maxLevel);
    }

    return envelope;
}

float EnvelopeFollower::processBlockRMS(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float sumSquared = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float sampleSum = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float s = buffer.getSample(channel, sample);
            sampleSum += s * s;
        }
        sumSquared += sampleSum / static_cast<float>(numChannels);
    }

    const float blockRMS = std::sqrt(sumSquared / static_cast<float>(numSamples));

    if (blockRMS > envelope)
        envelope = attackCoeff * envelope + (1.0f - attackCoeff) * blockRMS;
    else
        envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * blockRMS;

    return envelope;
}

void EnvelopeFollower::setAttackTime(float attackMs)
{
    attackTimeMs = std::max(0.1f, attackMs);
    updateCoefficients();
}

void EnvelopeFollower::setReleaseTime(float releaseMs)
{
    releaseTimeMs = std::max(0.1f, releaseMs);
    updateCoefficients();
}

void EnvelopeFollower::setAttackTimeSeconds(double attackSec)
{
    setAttackTime(static_cast<float>(attackSec * 1000.0));
}

void EnvelopeFollower::setReleaseTimeSeconds(double releaseSec)
{
    setReleaseTime(static_cast<float>(releaseSec * 1000.0));
}

void EnvelopeFollower::setHoldTime(float holdMs)
{
    holdTimeMs = std::max(0.0f, holdMs);
    holdSamples = static_cast<int>(holdTimeMs * 0.001f * static_cast<float>(sampleRate));
}

} // namespace DSP
