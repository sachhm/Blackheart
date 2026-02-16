#include "DynamicGate.h"

namespace DSP
{

void DynamicGate::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = static_cast<int>(spec.maximumBlockSize);

    envelopeFollower.prepare(spec);
    envelopeFollower.setAttackTime(2.0f);
    envelopeFollower.setReleaseTime(80.0f);
    envelopeFollower.setDetectionMode(EnvelopeFollower::DetectionMode::PeakHold);
    envelopeFollower.setHoldTime(10.0f);
    envelopeFollower.setSensitivity(1.0f);

    gateGain.reset(sampleRate, 0.015);

    gateOpen = true;
    lastGateGain = 1.0f;

    const float threshold = calculateDynamicThreshold();
    hysteresisThresholdHigh = threshold;
    hysteresisThresholdLow = threshold * juce::Decibels::decibelsToGain(-hysteresisDb);
}

void DynamicGate::reset()
{
    envelopeFollower.reset();
    gateGain.reset(sampleRate, 0.015);
    gateOpen = true;
    lastGateGain = 1.0f;
}

void DynamicGate::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const float dynamicThreshold = calculateDynamicThreshold();

    hysteresisThresholdHigh = dynamicThreshold;
    hysteresisThresholdLow = dynamicThreshold * juce::Decibels::decibelsToGain(-hysteresisDb);

    // Glare-dependent release: faster at high glare for spitty texture
    const float glareRelease = 80.0f - glareInfluence * 50.0f;  // 80ms → 30ms
    envelopeFollower.setReleaseTime(std::max(20.0f, glareRelease));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float maxLevel = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            maxLevel = std::max(maxLevel, std::abs(buffer.getSample(channel, sample)));
        }

        const float envelope = envelopeFollower.processSample(maxLevel);

        if (gateOpen)
        {
            if (envelope < hysteresisThresholdLow)
                gateOpen = false;
        }
        else
        {
            if (envelope > hysteresisThresholdHigh)
                gateOpen = true;
        }

        const float targetGain = calculateGateGain(envelope, dynamicThreshold);

        gateGain.setTargetValue(targetGain);
        const float smoothedGain = gateGain.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float inputSample = buffer.getSample(channel, sample);
            buffer.setSample(channel, sample, inputSample * smoothedGain);
        }

        lastGateGain = smoothedGain;
    }
}

float DynamicGate::calculateDynamicThreshold() const
{
    // Use linear scaling instead of squared for more predictable gate response
    // Reduced multipliers for less aggressive gating (more playable at high gain)
    const float gainBoost = gainInfluence * 12.0f;   // Was influence² * 20

    // Exponential glare coupling: gentle at low glare, aggressive at high
    const float glareCurved = glareInfluence * glareInfluence * glareInfluence;  // cubic
    const float glareBoost = glareCurved * 18.0f;

    const float effectiveThresholdDb = baseThresholdDb + gainBoost + glareBoost;

    return juce::Decibels::decibelsToGain(effectiveThresholdDb, -96.0f);
}

float DynamicGate::calculateGateGain(float envelope, float threshold) const
{
    if (envelope <= 0.0f)
        return 0.0f;

    const float kneeStart = threshold * (1.0f - kneeWidth);
    const float kneeEnd = threshold * (1.0f + kneeWidth);

    if (envelope >= kneeEnd)
    {
        return 1.0f;
    }
    else if (envelope <= kneeStart)
    {
        const float ratio = envelope / kneeStart;

        const float shaped = ratio * ratio * (3.0f - 2.0f * ratio);

        const float minGain = 0.0f;
        return minGain + shaped * (0.3f - minGain);
    }
    else
    {
        const float kneePosition = (envelope - kneeStart) / (kneeEnd - kneeStart);

        const float smoothKnee = kneePosition * kneePosition * (3.0f - 2.0f * kneePosition);

        return 0.3f + smoothKnee * 0.7f;
    }
}

void DynamicGate::setBaseThreshold(float thresholdDb)
{
    baseThresholdDb = juce::jlimit(-60.0f, 0.0f, thresholdDb);
    baseThresholdLinear = juce::Decibels::decibelsToGain(baseThresholdDb, -96.0f);
}

void DynamicGate::setGainInfluence(float normalizedGain)
{
    gainInfluence = juce::jlimit(0.0f, 1.0f, normalizedGain);
}

void DynamicGate::setGlareInfluence(float normalizedGlare)
{
    glareInfluence = juce::jlimit(0.0f, 1.0f, normalizedGlare);
}

void DynamicGate::setAttackTime(float attackMs)
{
    envelopeFollower.setAttackTime(attackMs);
}

void DynamicGate::setReleaseTime(float releaseMs)
{
    envelopeFollower.setReleaseTime(releaseMs);
}

void DynamicGate::setHoldTime(float holdMs)
{
    envelopeFollower.setHoldTime(holdMs);
}

float DynamicGate::getEffectiveThreshold() const
{
    return juce::Decibels::gainToDecibels(calculateDynamicThreshold(), -96.0f);
}

} // namespace DSP
