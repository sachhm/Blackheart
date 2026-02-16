#include "PitchShifter.h"
#include "LookupTables.h"
#include <cmath>

namespace DSP
{

void PitchShifter::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = static_cast<int>(spec.maximumBlockSize);

    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    LookupTables::initialize();

    chaos.reset(sampleRate, 0.02);
    panic.reset(sampleRate, 0.02);

    grainSizeSamples = static_cast<int>(defaultGrainMs * 0.001 * sampleRate);
    grainSizeSamples = juce::jlimit(256, delayBufferSize / 4, grainSizeSamples);

    setRiseTime(riseTimeMs);

    for (int ch = 0; ch < maxChannels; ++ch)
        for (int i = 0; i < delayBufferSize; ++i)
            delayBuffer[ch][i] = 0.0f;

    writePosition = 0;

    for (int g = 0; g < maxGrains; ++g)
    {
        grains[g].readPosition = static_cast<float>(grainSizeSamples * g / 2);
        grains[g].phase = static_cast<float>(g) / static_cast<float>(maxGrains);
        grains[g].active = (g < 2);
    }

    for (int g = 0; g < maxDetuneGrains; ++g)
    {
        detuneGrains[g].readPosition = static_cast<float>(grainSizeSamples * g / 2);
        detuneGrains[g].phase = static_cast<float>(g) * 0.5f;
        detuneGrains[g].active = false;
    }

    activeGrainCount = 2;

    transitionState = TransitionState::Idle;
    transitionActive = false;
    pitchSmoothState = 0.0f;
    mixSmoothState = 0.0f;
    currentPitchRatio = 1.0f;
    currentMix = 0.0f;
    feedbackSample = 0.0f;
    ringModPhase = 0.0f;
    ringModFreq = 0.0f;
    ringModMix = 0.0f;
    panicAmount = 0.0f;
    dryEnvelope = 0.0f;
    wetEnvelope = 0.0f;

    prevOctaveOneActive = false;
    prevOctaveTwoActive = false;
    octaveOneActive.store(false, std::memory_order_relaxed);
    octaveTwoActive.store(false, std::memory_order_relaxed);
}

void PitchShifter::reset()
{
    for (int ch = 0; ch < maxChannels; ++ch)
        for (int i = 0; i < delayBufferSize; ++i)
            delayBuffer[ch][i] = 0.0f;

    writePosition = 0;

    for (int g = 0; g < maxGrains; ++g)
    {
        grains[g].readPosition = static_cast<float>(grainSizeSamples * g / 2);
        grains[g].phase = static_cast<float>(g) / static_cast<float>(maxGrains);
        grains[g].active = (g < 2);
    }

    for (int g = 0; g < maxDetuneGrains; ++g)
    {
        detuneGrains[g].readPosition = 0.0f;
        detuneGrains[g].phase = static_cast<float>(g) * 0.5f;
        detuneGrains[g].active = false;
    }

    activeGrainCount = 2;

    transitionState = TransitionState::Idle;
    transitionActive = false;
    pitchSmoothState = 0.0f;
    mixSmoothState = 0.0f;
    currentPitchRatio = 1.0f;
    currentMix = 0.0f;
    feedbackSample = 0.0f;
    ringModPhase = 0.0f;
    dryEnvelope = 0.0f;
    wetEnvelope = 0.0f;

    prevOctaveOneActive = false;
    prevOctaveTwoActive = false;
}

float PitchShifter::getGrainWindow(float phase, float harshness) const
{
    const float hann = LookupTables::fastHann(phase);
    return hann * (1.0f - harshness) + harshness;
}

float PitchShifter::readFromBuffer(int channel, float position) const
{
    channel = juce::jlimit(0, maxChannels - 1, channel);

    // Wrap position to buffer size
    const float bufSize = static_cast<float>(delayBufferSize);
    while (position < 0.0f) position += bufSize;
    while (position >= bufSize) position -= bufSize;

    if (!std::isfinite(position))
        return 0.0f;

    // Linear interpolation
    const int idx0 = static_cast<int>(position);
    const int idx1 = (idx0 + 1) % delayBufferSize;
    const float frac = position - static_cast<float>(idx0);

    const float s0 = delayBuffer[channel][idx0];
    const float s1 = delayBuffer[channel][idx1];

    return s0 + frac * (s1 - s0);
}

void PitchShifter::updateGrain(Grain& grain, float pitchRatio, int grainSize)
{
    grainSize = juce::jlimit(256, delayBufferSize / 4, grainSize);
    pitchRatio = juce::jlimit(0.5f, 8.0f, pitchRatio);

    // Advance phase
    const float phaseInc = 1.0f / static_cast<float>(grainSize);
    grain.phase += phaseInc;

    // When grain completes, reset it
    if (grain.phase >= 1.0f)
    {
        grain.phase -= 1.0f;
        // Reset read position to trail behind write position
        grain.readPosition = static_cast<float>(writePosition) - static_cast<float>(grainSize);
        if (grain.readPosition < 0.0f)
            grain.readPosition += static_cast<float>(delayBufferSize);
    }

    // Advance read position by pitch ratio
    // pitchRatio > 1 = read faster = higher pitch
    grain.readPosition += pitchRatio;

    // Wrap
    const float bufSize = static_cast<float>(delayBufferSize);
    while (grain.readPosition >= bufSize) grain.readPosition -= bufSize;
    while (grain.readPosition < 0.0f) grain.readPosition += bufSize;
}

float PitchShifter::applyExpCurve(float linearValue, bool isAttack) const
{
    linearValue = juce::jlimit(0.0f, 1.0f, linearValue);
    if (isAttack)
        return 1.0f - std::exp(-linearValue * expCurveAmount);
    else
        return std::exp(-linearValue * expCurveAmount);
}

void PitchShifter::resetGrainsForTransition()
{
    const float bufSize = static_cast<float>(delayBufferSize);
    const float grainOffset = static_cast<float>(grainSizeSamples);

    for (int g = 0; g < activeGrainCount; ++g)
    {
        grains[g].readPosition = static_cast<float>(writePosition) - grainOffset * (1.0f - static_cast<float>(g) / static_cast<float>(activeGrainCount));
        if (grains[g].readPosition < 0.0f) grains[g].readPosition += bufSize;
        grains[g].phase = static_cast<float>(g) / static_cast<float>(activeGrainCount);
        grains[g].active = true;
    }

    for (int g = 0; g < maxDetuneGrains; ++g)
    {
        detuneGrains[g].readPosition = static_cast<float>(writePosition) - grainOffset * 0.5f;
        if (detuneGrains[g].readPosition < 0.0f) detuneGrains[g].readPosition += bufSize;
        detuneGrains[g].phase = static_cast<float>(g) * 0.5f;
        detuneGrains[g].active = true;
    }
}

void PitchShifter::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const int processChannels = std::min(numChannels, maxChannels);

    const bool oct1Active = octaveOneActive.load(std::memory_order_relaxed);
    const bool oct2Active = octaveTwoActive.load(std::memory_order_relaxed);
    const bool anyOctaveActive = oct1Active || oct2Active;

    float targetPitchRatio = 1.0f;
    if (oct2Active)
        targetPitchRatio = 4.0f;
    else if (oct1Active)
        targetPitchRatio = 2.0f;

    const float targetMix = anyOctaveActive ? 1.0f : 0.0f;

    const bool wasActive = prevOctaveOneActive || prevOctaveTwoActive;
    const bool stateChanged = (wasActive != anyOctaveActive) ||
                              (anyOctaveActive && (prevOctaveOneActive != oct1Active ||
                                                   prevOctaveTwoActive != oct2Active));

    if (stateChanged && anyOctaveActive)
    {
        resetGrainsForTransition();
        transitionActive = true;
    }

    prevOctaveOneActive = oct1Active;
    prevOctaveTwoActive = oct2Active;

    const float safeRiseCoeff = (riseCoeff > 0.0f && std::isfinite(riseCoeff)) ? riseCoeff : 0.002f;
    const float safeFallCoeff = (fallCoeff > 0.0f && std::isfinite(fallCoeff)) ? fallCoeff : 0.002f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float chaosVal = chaos.getNextValue();
        const float panicVal = panic.getNextValue();

        // Update active grain count based on chaos
        int targetGrainCount;
        if (chaosVal < 0.3f)
            targetGrainCount = 2;
        else if (chaosVal < 0.7f)
            targetGrainCount = 3;
        else
            targetGrainCount = 4;

        if (targetGrainCount > activeGrainCount)
        {
            grains[activeGrainCount].phase = 0.0f;
            grains[activeGrainCount].readPosition = static_cast<float>(writePosition) - static_cast<float>(grainSizeSamples);
            if (grains[activeGrainCount].readPosition < 0.0f)
                grains[activeGrainCount].readPosition += static_cast<float>(delayBufferSize);
            grains[activeGrainCount].active = true;
            activeGrainCount = targetGrainCount;
        }
        else if (targetGrainCount < activeGrainCount)
        {
            const float lastGrainPhase = grains[activeGrainCount - 1].phase;
            if (lastGrainPhase < 0.05f || lastGrainPhase > 0.95f)
            {
                grains[activeGrainCount - 1].active = false;
                activeGrainCount = targetGrainCount;
            }
        }

        // Write input to delay buffer with feedback
        for (int ch = 0; ch < processChannels; ++ch)
        {
            float inputSample = buffer.getSample(ch, sample);
            if (!std::isfinite(inputSample)) inputSample = 0.0f;

            const float feedbackAmount = chaosVal * 0.4f;
            const float feedbackInput = inputSample + std::tanh(feedbackSample * feedbackAmount) * feedbackAmount;
            delayBuffer[ch][writePosition] = feedbackInput;
        }

        const bool needsProcessing = anyOctaveActive || mixSmoothState > 0.0001f;

        if (!needsProcessing)
        {
            writePosition = (writePosition + 1) % delayBufferSize;
            continue;
        }

        const float smoothCoeff = (targetMix > mixSmoothState) ? safeRiseCoeff : safeFallCoeff;
        currentPitchRatio += smoothCoeff * (targetPitchRatio - currentPitchRatio);
        mixSmoothState += smoothCoeff * (targetMix - mixSmoothState);
        currentPitchRatio = juce::jlimit(1.0f, 8.0f, currentPitchRatio);
        mixSmoothState = juce::jlimit(0.0f, 1.0f, mixSmoothState);

        float effectiveMix;
        if (mixSmoothState < 0.5f)
            effectiveMix = 2.0f * mixSmoothState * mixSmoothState;
        else
        {
            const float t = mixSmoothState - 0.5f;
            effectiveMix = 0.5f + 2.0f * t * (1.0f - t) + t * t * 2.0f;
        }
        effectiveMix = juce::jlimit(0.0f, 1.0f, effectiveMix);

        // Wider modulation ranges
        const float pitchMod = pitchModulation * chaosVal * 0.4f;
        const float sizeMod = grainSizeModulation * chaosVal * 0.6f;
        const float timeMod = timingModulation * chaosVal * 12.0f;

        float modulatedPitch = currentPitchRatio * (1.0f + pitchMod);
        modulatedPitch = juce::jlimit(0.5f, 8.0f, modulatedPitch);

        int modulatedGrainSize = static_cast<int>(static_cast<float>(grainSizeSamples) * (1.0f + sizeMod));
        modulatedGrainSize = juce::jlimit(256, delayBufferSize / 4, modulatedGrainSize);

        // Window harshness: chaos^2 — stays clean until pushed hard
        const float harshness = chaosVal * chaosVal;

        // Detune ratios for PANIC grains
        const float detuneUp = 1.0f + panicVal * 0.15f;
        const float detuneDown = 1.0f - panicVal * 0.15f;
        const float detuneDrift = pitchModulation * chaosVal * 0.05f;

        float wetSumMono = 0.0f;

        for (int ch = 0; ch < processChannels; ++ch)
        {
            const float dryInput = buffer.getSample(ch, sample);

            float wetSum = 0.0f;
            float windowSum = 0.0f;

            for (int g = 0; g < activeGrainCount; ++g)
            {
                if (!grains[g].active) continue;
                const float window = getGrainWindow(grains[g].phase, harshness);
                float readPos = grains[g].readPosition;
                readPos += (g % 2 == 0) ? timeMod : -timeMod;

                const float bufSize = static_cast<float>(delayBufferSize);
                while (readPos < 0.0f) readPos += bufSize;
                while (readPos >= bufSize) readPos -= bufSize;

                wetSum += readFromBuffer(ch, readPos) * window;
                windowSum += window;
            }

            // PANIC detune grains
            if (panicVal > 0.001f)
            {
                for (int g = 0; g < maxDetuneGrains; ++g)
                {
                    const float window = getGrainWindow(detuneGrains[g].phase, harshness);
                    float readPos = detuneGrains[g].readPosition;

                    const float bufSize = static_cast<float>(delayBufferSize);
                    while (readPos < 0.0f) readPos += bufSize;
                    while (readPos >= bufSize) readPos -= bufSize;

                    wetSum += readFromBuffer(ch, readPos) * window * panicVal;
                    windowSum += window * panicVal;
                }
            }

            float wetOutput;
            if (windowSum > 0.001f)
                wetOutput = wetSum / windowSum;
            else
                wetOutput = dryInput;

            if (!std::isfinite(wetOutput))
                wetOutput = dryInput;

            // Ring modulation (post-grain, pre-mix)
            if (ringModMix > 0.001f)
            {
                const float ringModSine = std::sin(ringModPhase * juce::MathConstants<float>::twoPi);
                wetOutput = wetOutput * (1.0f - ringModMix) + wetOutput * ringModSine * ringModMix;
            }

            // Gain compensation: match wet level to dry level
            {
                const float dryAbs = std::abs(dryInput);
                const float wetAbs = std::abs(wetOutput);

                // Fast envelope followers for level tracking
                const float dryCoeff = (dryAbs > dryEnvelope) ? envelopeAttack : envelopeRelease;
                dryEnvelope += dryCoeff * (dryAbs - dryEnvelope);

                const float wetCoeff = (wetAbs > wetEnvelope) ? envelopeAttack : envelopeRelease;
                wetEnvelope += wetCoeff * (wetAbs - wetEnvelope);

                // Apply makeup gain when wet is quieter than dry
                if (wetEnvelope > 0.0001f && dryEnvelope > 0.0001f)
                {
                    const float gainComp = dryEnvelope / wetEnvelope;
                    wetOutput *= juce::jlimit(0.5f, 4.0f, gainComp);
                }
                else if (wetAbs < 0.0001f && dryAbs > 0.001f)
                {
                    wetOutput = dryInput;
                }
            }

            const float outputSample = dryInput * (1.0f - effectiveMix) + wetOutput * effectiveMix;
            float finalOutput = outputSample;
            if (!std::isfinite(finalOutput))
                finalOutput = dryInput;

            buffer.setSample(ch, sample, finalOutput);
            wetSumMono += wetOutput;
        }

        // Store feedback sample (mono average, tanh-clamped)
        feedbackSample = std::tanh(wetSumMono / static_cast<float>(processChannels));

        // Update main grain positions
        for (int g = 0; g < activeGrainCount; ++g)
        {
            if (grains[g].active)
                updateGrain(grains[g], modulatedPitch, modulatedGrainSize);
        }

        // Update detune grain positions
        if (panicVal > 0.001f)
        {
            updateGrain(detuneGrains[0], modulatedPitch * (detuneUp + detuneDrift), modulatedGrainSize);
            updateGrain(detuneGrains[1], modulatedPitch * (detuneDown - detuneDrift), modulatedGrainSize);
        }

        // Advance ring mod oscillator
        if (ringModFreq > 0.0f)
        {
            ringModPhase += ringModFreq / static_cast<float>(sampleRate);
            if (ringModPhase >= 1.0f) ringModPhase -= 1.0f;
        }

        writePosition = (writePosition + 1) % delayBufferSize;
        currentMix = effectiveMix;

        if (transitionActive && std::abs(targetMix - mixSmoothState) < 0.001f)
        {
            transitionActive = false;
            if (targetMix < 0.001f)
            {
                mixSmoothState = 0.0f;
                currentPitchRatio = 1.0f;
                feedbackSample = 0.0f;
            }
        }
    }
}

void PitchShifter::setOctaveOneActive(bool active)
{
    octaveOneActive.store(active, std::memory_order_relaxed);
}

void PitchShifter::setOctaveTwoActive(bool active)
{
    octaveTwoActive.store(active, std::memory_order_relaxed);
}

void PitchShifter::setRiseTime(float riseMs)
{
    riseMs = juce::jlimit(minRiseMs, maxRiseMs, riseMs);
    riseTimeMs = riseMs;
    fallTimeMs = std::max(minRiseMs, riseTimeMs * 0.6f);

    const double safeRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    const double riseSec = std::max(0.001, static_cast<double>(riseTimeMs) * 0.001);
    const double fallSec = std::max(0.001, static_cast<double>(fallTimeMs) * 0.001);

    // Coefficient for exponential smoothing
    riseCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (safeRate * riseSec)));
    fallCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (safeRate * fallSec)));

    riseCoeff = juce::jlimit(0.0001f, 0.1f, riseCoeff);
    fallCoeff = juce::jlimit(0.0001f, 0.1f, fallCoeff);

    if (!std::isfinite(riseCoeff)) riseCoeff = 0.002f;
    if (!std::isfinite(fallCoeff)) fallCoeff = 0.002f;
}

void PitchShifter::setChaosAmount(float normalizedChaos)
{
    chaos.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedChaos));
}

void PitchShifter::setPitchModulation(float mod)
{
    pitchModulation = std::isfinite(mod) ? juce::jlimit(-1.0f, 1.0f, mod) : 0.0f;
}

void PitchShifter::setGrainSizeModulation(float mod)
{
    grainSizeModulation = std::isfinite(mod) ? juce::jlimit(-1.0f, 1.0f, mod) : 0.0f;
}

void PitchShifter::setTimingModulation(float mod)
{
    timingModulation = std::isfinite(mod) ? juce::jlimit(-1.0f, 1.0f, mod) : 0.0f;
}

void PitchShifter::setPanic(float normalizedPanic)
{
    panic.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedPanic));
}

void PitchShifter::setRingModSpeed(float normalizedSpeed)
{
    ringModMix = juce::jlimit(0.0f, 1.0f, (normalizedSpeed - 0.5f) * 2.0f);
    if (ringModMix > 0.001f)
    {
        const float t = (normalizedSpeed - 0.5f) * 2.0f;
        ringModFreq = 20.0f * std::pow(100.0f, t);
    }
    else
    {
        ringModFreq = 0.0f;
    }
}

} // namespace DSP
