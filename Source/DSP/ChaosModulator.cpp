#include "ChaosModulator.h"
#include "LookupTables.h"

namespace DSP
{

void ChaosModulator::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // Initialize lookup tables (thread-safe, only runs once)
    LookupTables::initialize();

    speed.reset(sampleRate, 0.05);
    chaos.reset(sampleRate, 0.03);

    setEnvelopeAttack(defaultAttackMs);
    setEnvelopeRelease(defaultReleaseMs);

    lfoPhase = 0.0f;
    lfoPhaseIncrement = 2.0f / static_cast<float>(sampleRate);

    sampleAndHoldValue = 0.0f;
    sampleAndHoldTarget = 0.0f;
    sampleAndHoldPhase = 0.0f;
    sampleAndHoldSmoothed = 0.0f;

    randomWalkValue = 0.0f;
    randomWalkTarget = 0.0f;
    randomWalkPhase = 0.0f;

    rawEnvelopeValue = 0.0f;
    smoothedEnvelopeInfluence = 0.0f;
    effectiveChaosAmount = 0.0f;

    deterministicRandom.setSeed(static_cast<juce::int64>(currentSeed));
    for (int i = 0; i < noiseTableSize; ++i)
    {
        noiseTable[static_cast<size_t>(i)] = deterministicRandom.nextFloat() * 2.0f - 1.0f;
    }
    noiseTableIndex = 0;
    noiseSmoothValue = 0.0f;

    currentOutput = ModulationOutput();
}

void ChaosModulator::reset()
{
    speed.reset(sampleRate, 0.05);
    chaos.reset(sampleRate, 0.03);

    lfoPhase = 0.0f;
    sampleAndHoldValue = 0.0f;
    sampleAndHoldTarget = 0.0f;
    sampleAndHoldPhase = 0.0f;
    sampleAndHoldSmoothed = 0.0f;
    randomWalkValue = 0.0f;
    randomWalkTarget = 0.0f;
    randomWalkPhase = 0.0f;
    noiseSmoothValue = 0.0f;

    rawEnvelopeValue = 0.0f;
    smoothedEnvelopeInfluence = 0.0f;
    effectiveChaosAmount = 0.0f;

    currentOutput = ModulationOutput();
}

float ChaosModulator::applyResponseCurve(float input) const
{
    input = juce::jlimit(0.0f, 1.0f, input);

    switch (responseCurve)
    {
        case ResponseCurve::Linear:
            return input;

        case ResponseCurve::Exponential:
            return input * input * input;

        case ResponseCurve::Logarithmic:
        {
            if (input <= 0.0f)
                return 0.0f;
            // Approximate log10(1 + x*9) using fast math
            const float scaled = 1.0f + input * 9.0f;
            // log10(x) ≈ (x-1)/(x+1) * 2/ln(10) for x near 1
            // For wider range, use a polynomial approximation
            return std::log10(scaled);
        }

        case ResponseCurve::SCurve:
        {
            const float x = input * 2.0f - 1.0f;
            const float curved = x * x * x;
            return (curved + 1.0f) * 0.5f;
        }

        default:
            return input;
    }
}

void ChaosModulator::updateEnvelopeSmoothing(float rawEnvelope)
{
    float scaledEnvelope = rawEnvelope;

    if (scaledEnvelope < envelopeThreshold)
    {
        scaledEnvelope = 0.0f;
    }
    else
    {
        scaledEnvelope = (scaledEnvelope - envelopeThreshold) / (1.0f - envelopeThreshold);
    }

    scaledEnvelope *= envelopeSensitivity;
    scaledEnvelope = juce::jlimit(0.0f, 1.0f, scaledEnvelope);

    const float curvedEnvelope = applyResponseCurve(scaledEnvelope);

    const float coeff = (curvedEnvelope > smoothedEnvelopeInfluence)
                        ? envelopeAttackCoeff
                        : envelopeReleaseCoeff;

    smoothedEnvelopeInfluence += coeff * (curvedEnvelope - smoothedEnvelopeInfluence);

    // Guard against NaN propagation from upstream envelope processing.
    // Once NaN enters smoothedEnvelopeInfluence, it permanently corrupts
    // all chaos modulation output.
    if (!std::isfinite(smoothedEnvelopeInfluence))
        smoothedEnvelopeInfluence = 0.0f;
}

float ChaosModulator::generateSineWave(float phase) const
{
    // Use lookup table instead of std::sin
    return LookupTables::fastSin(phase);
}

float ChaosModulator::generateTriangleWave(float phase) const
{
    // Branchless triangle wave
    phase = phase - std::floor(phase); // Ensure [0, 1)
    const float t = phase * 4.0f;
    const float tri = std::abs(std::fmod(t + 3.0f, 4.0f) - 2.0f) - 1.0f;
    return tri;
}

float ChaosModulator::interpolateHermite(float y0, float y1, float y2, float y3, float t) const
{
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * t + c2) * t + c1) * t + c0;
}

float ChaosModulator::generateSmoothNoise(float t)
{
    const float scaledT = t * 4.0f;
    const int index = static_cast<int>(scaledT) % noiseTableSize;
    const float frac = scaledT - std::floor(scaledT);

    const int i0 = (index - 1 + noiseTableSize) % noiseTableSize;
    const int i1 = index;
    const int i2 = (index + 1) % noiseTableSize;
    const int i3 = (index + 2) % noiseTableSize;

    return interpolateHermite(
        noiseTable[static_cast<size_t>(i0)],
        noiseTable[static_cast<size_t>(i1)],
        noiseTable[static_cast<size_t>(i2)],
        noiseTable[static_cast<size_t>(i3)],
        frac
    );
}

void ChaosModulator::updateSampleAndHold()
{
    if (sampleAndHoldPhase >= 1.0f)
    {
        sampleAndHoldPhase -= 1.0f;
        sampleAndHoldValue = sampleAndHoldTarget;
        sampleAndHoldTarget = random.nextFloat() * 2.0f - 1.0f;
    }

    sampleAndHoldSmoothed = sampleAndHoldSmoothed * sampleAndHoldSmoothCoeff +
                           sampleAndHoldValue * (1.0f - sampleAndHoldSmoothCoeff);
}

void ChaosModulator::updateRandomWalk()
{
    if (randomWalkPhase >= 1.0f)
    {
        randomWalkPhase -= 1.0f;

        const float step = (random.nextFloat() * 2.0f - 1.0f) * 0.3f;
        randomWalkTarget = juce::jlimit(-1.0f, 1.0f, randomWalkValue + step);
    }

    randomWalkValue = randomWalkValue * randomWalkSmoothCoeff +
                     randomWalkTarget * (1.0f - randomWalkSmoothCoeff);
}

void ChaosModulator::process(int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        getNextModulationValue();
    }
}

ChaosModulator::ModulationOutput ChaosModulator::getModulation() const
{
    return currentOutput;
}

float ChaosModulator::getNextModulationValue()
{
    currentSpeedHz = speed.getNextValue();
    const float baseChaos = chaos.getNextValue();

    updateEnvelopeSmoothing(rawEnvelopeValue);

    const float envelopeContribution = smoothedEnvelopeInfluence;

    // Reduced minimum chaos for more dynamic range (chaos responds more to playing intensity)
    const float minChaosAtLowEnvelope = 0.05f;
    effectiveChaosAmount = baseChaos * (minChaosAtLowEnvelope + envelopeContribution * (1.0f - minChaosAtLowEnvelope));

    const float speedModulation = 1.0f + envelopeContribution * 0.5f;
    const float effectiveSpeed = currentSpeedHz * speedModulation;

    lfoPhaseIncrement = effectiveSpeed / static_cast<float>(sampleRate);

    lfoPhase += lfoPhaseIncrement;
    if (lfoPhase >= 1.0f)
        lfoPhase -= 1.0f;
    if (!std::isfinite(lfoPhase))
        lfoPhase = 0.0f;

    const float sampleAndHoldRate = effectiveSpeed * (0.5f + effectiveChaosAmount * 1.5f);
    sampleAndHoldPhase += sampleAndHoldRate / static_cast<float>(sampleRate);
    updateSampleAndHold();

    const float randomWalkRate = effectiveSpeed * 0.25f;
    randomWalkPhase += randomWalkRate / static_cast<float>(sampleRate);
    updateRandomWalk();

    // Use lookup table-based waveform generators
    const float sineValue = generateSineWave(lfoPhase);
    const float triangleValue = generateTriangleWave(lfoPhase);
    const float smoothNoiseValue = generateSmoothNoise(lfoPhase);

    const float chaosSquared = effectiveChaosAmount * effectiveChaosAmount;

    const float smoothWeight = 1.0f - chaosSquared;
    const float noiseWeight = chaosSquared * 0.6f;
    const float sampleHoldWeight = chaosSquared * 0.25f;
    const float randomWalkWeight = chaosSquared * 0.15f;

    const float lfoBlend = sineValue * (1.0f - effectiveChaosAmount * 0.3f) +
                          triangleValue * (effectiveChaosAmount * 0.3f);

    float pitchMod = lfoBlend * smoothWeight +
                    smoothNoiseValue * noiseWeight +
                    sampleAndHoldSmoothed * sampleHoldWeight;

    float grainSizeMod = triangleValue * smoothWeight * 0.5f +
                        randomWalkValue * (noiseWeight + randomWalkWeight) +
                        smoothNoiseValue * sampleHoldWeight * 0.5f;

    float timingMod = sineValue * smoothWeight * 0.3f +
                     sampleAndHoldSmoothed * (sampleHoldWeight + noiseWeight * 0.5f) +
                     randomWalkValue * randomWalkWeight;

    const float dynamicDepth = 0.3f + envelopeContribution * 0.7f;
    pitchMod *= dynamicDepth;
    grainSizeMod *= dynamicDepth;
    timingMod *= dynamicDepth;

    pitchMod *= effectiveChaosAmount;
    grainSizeMod *= effectiveChaosAmount;
    timingMod *= effectiveChaosAmount;

    currentOutput.pitchMod = pitchMod;
    currentOutput.grainSizeMod = grainSizeMod;
    currentOutput.timingMod = timingMod;
    currentOutput.combinedMod = (pitchMod + grainSizeMod + timingMod) * 0.333333f;

    return currentOutput.combinedMod;
}

void ChaosModulator::setSpeed(float normalizedSpeed)
{
    normalizedSpeed = juce::jlimit(0.0f, 1.0f, normalizedSpeed);

    const float skewedSpeed = std::pow(normalizedSpeed, speedSkew);
    const float hz = minSpeedHz + skewedSpeed * (maxSpeedHz - minSpeedHz);

    speed.setTargetValue(hz);
}

void ChaosModulator::setChaos(float normalizedChaos)
{
    chaos.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedChaos));
}

void ChaosModulator::setEnvelopeValue(float envelopeLevel)
{
    rawEnvelopeValue = juce::jlimit(0.0f, 1.0f, envelopeLevel);
}

void ChaosModulator::setSeed(unsigned int seed)
{
    currentSeed = seed;
    deterministicRandom.setSeed(static_cast<juce::int64>(seed));

    for (int i = 0; i < noiseTableSize; ++i)
    {
        noiseTable[static_cast<size_t>(i)] = deterministicRandom.nextFloat() * 2.0f - 1.0f;
    }

    random.setSeed(static_cast<juce::int64>(seed + 1));
}

void ChaosModulator::setResponseCurve(ResponseCurve curve)
{
    responseCurve = curve;
}

void ChaosModulator::setEnvelopeSensitivity(float sensitivity)
{
    envelopeSensitivity = juce::jlimit(0.1f, 3.0f, sensitivity);
}

void ChaosModulator::setEnvelopeThreshold(float threshold)
{
    envelopeThreshold = juce::jlimit(0.0f, 0.5f, threshold);
}

void ChaosModulator::setEnvelopeAttack(float attackMs)
{
    attackMs = juce::jlimit(0.1f, 100.0f, attackMs);
    envelopeAttackCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * attackMs * 0.001f));
}

void ChaosModulator::setEnvelopeRelease(float releaseMs)
{
    releaseMs = juce::jlimit(10.0f, 500.0f, releaseMs);
    envelopeReleaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * releaseMs * 0.001f));
}

} // namespace DSP
