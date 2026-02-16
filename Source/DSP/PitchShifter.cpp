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

    // Initialize lookup tables (thread-safe, only runs once)
    LookupTables::initialize();

    chaos.reset(sampleRate, 0.02);

    // Calculate grain size in samples (30ms default)
    grainSizeSamples = static_cast<int>(defaultGrainMs * 0.001 * sampleRate);
    grainSizeSamples = juce::jlimit(256, delayBufferSize / 4, grainSizeSamples);

    setRiseTime(riseTimeMs);

    // Clear delay buffer
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        for (int i = 0; i < delayBufferSize; ++i)
        {
            delayBuffer[ch][i] = 0.0f;
        }
    }

    writePosition = 0;

    // Initialize grains - staggered by half a grain for overlap
    grains[0].readPosition = 0.0f;
    grains[0].phase = 0.0f;
    grains[0].active = true;

    grains[1].readPosition = static_cast<float>(grainSizeSamples / 2);
    grains[1].phase = 0.5f;
    grains[1].active = true;

    // Reset state
    transitionState = TransitionState::Idle;
    transitionActive = false;
    pitchSmoothState = 0.0f;
    mixSmoothState = 0.0f;
    currentPitchRatio = 1.0f;
    currentMix = 0.0f;

    prevOctaveOneActive = false;
    prevOctaveTwoActive = false;
    octaveOneActive.store(false, std::memory_order_relaxed);
    octaveTwoActive.store(false, std::memory_order_relaxed);
}

void PitchShifter::reset()
{
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        for (int i = 0; i < delayBufferSize; ++i)
        {
            delayBuffer[ch][i] = 0.0f;
        }
    }

    writePosition = 0;

    grains[0].readPosition = 0.0f;
    grains[0].phase = 0.0f;
    grains[0].active = true;

    grains[1].readPosition = static_cast<float>(grainSizeSamples / 2);
    grains[1].phase = 0.5f;
    grains[1].active = true;

    transitionState = TransitionState::Idle;
    transitionActive = false;
    pitchSmoothState = 0.0f;
    mixSmoothState = 0.0f;
    currentPitchRatio = 1.0f;
    currentMix = 0.0f;

    prevOctaveOneActive = false;
    prevOctaveTwoActive = false;
}

float PitchShifter::getGrainWindow(float phase) const
{
    // Use lookup table for Hann window - much faster than trig
    return LookupTables::fastHann(phase);
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

    // Position grain 0 one full grain behind write position
    // Start at phase 0.25 (Hann window = 0.5) for immediate contribution
    grains[0].readPosition = static_cast<float>(writePosition) - grainOffset * 0.75f;
    if (grains[0].readPosition < 0.0f) grains[0].readPosition += bufSize;
    grains[0].phase = 0.25f;  // Start mid-rise for immediate contribution

    // Position grain 1 half a grain behind (50% overlap from grain 0)
    // Start at phase 0.75 (Hann window = 0.5) for immediate contribution
    grains[1].readPosition = static_cast<float>(writePosition) - grainOffset * 0.25f;
    if (grains[1].readPosition < 0.0f) grains[1].readPosition += bufSize;
    grains[1].phase = 0.75f;  // Start mid-fall for immediate contribution
}

void PitchShifter::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const int processChannels = std::min(numChannels, maxChannels);

    // Load atomic octave states once per block for consistency
    const bool oct1Active = octaveOneActive.load(std::memory_order_relaxed);
    const bool oct2Active = octaveTwoActive.load(std::memory_order_relaxed);
    const bool anyOctaveActive = oct1Active || oct2Active;

    // Target pitch ratio: 2.0 for +1 octave (2x freq), 4.0 for +2 octaves (4x freq)
    float targetPitchRatio = 1.0f;  // 1.0 = no pitch change
    if (oct2Active)
        targetPitchRatio = 4.0f;    // +2 octaves
    else if (oct1Active)
        targetPitchRatio = 2.0f;    // +1 octave

    // Target mix: 1.0 when octave active, 0.0 when not
    const float targetMix = anyOctaveActive ? 1.0f : 0.0f;

    // Check for state change to reset grains
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

    // Get safe smoothing coefficients
    const float safeRiseCoeff = (riseCoeff > 0.0f && std::isfinite(riseCoeff)) ? riseCoeff : 0.002f;
    const float safeFallCoeff = (fallCoeff > 0.0f && std::isfinite(fallCoeff)) ? fallCoeff : 0.002f;

    // Process sample by sample
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // STEP 1: Write input to delay buffer
        for (int ch = 0; ch < processChannels; ++ch)
        {
            float inputSample = buffer.getSample(ch, sample);
            if (!std::isfinite(inputSample)) inputSample = 0.0f;
            delayBuffer[ch][writePosition] = inputSample;
        }

        // STEP 2: If not processing, pass through
        const bool needsProcessing = anyOctaveActive || mixSmoothState > 0.0001f;

        if (!needsProcessing)
        {
            // Just pass through - input already in buffer
            writePosition = (writePosition + 1) % delayBufferSize;
            continue;
        }

        // STEP 3: Smooth parameters
        const float smoothCoeff = (targetMix > mixSmoothState) ? safeRiseCoeff : safeFallCoeff;

        // Smooth pitch ratio directly (not an intermediate value)
        currentPitchRatio += smoothCoeff * (targetPitchRatio - currentPitchRatio);
        mixSmoothState += smoothCoeff * (targetMix - mixSmoothState);

        currentPitchRatio = juce::jlimit(1.0f, 8.0f, currentPitchRatio);
        mixSmoothState = juce::jlimit(0.0f, 1.0f, mixSmoothState);

        // Calculate effective mix with smooth curve
        // Use squared curve for smooth fade-in, but ensure we reach 1.0
        float effectiveMix;
        if (mixSmoothState < 0.5f)
        {
            // Quadratic curve for smooth start: 0->0, 0.5->0.5
            effectiveMix = 2.0f * mixSmoothState * mixSmoothState;
        }
        else
        {
            // Inverse quadratic for smooth approach to 1.0: 0.5->0.5, 1.0->1.0
            const float t = mixSmoothState - 0.5f;
            effectiveMix = 0.5f + 2.0f * t * (1.0f - t) + t * t * 2.0f;
        }

        effectiveMix = juce::jlimit(0.0f, 1.0f, effectiveMix);

        // STEP 4: Apply chaos modulation
        const float chaosVal = chaos.getNextValue();
        const float pitchMod = pitchModulation * chaosVal * 0.1f;
        const float sizeMod = grainSizeModulation * chaosVal * 0.3f;
        const float timeMod = timingModulation * chaosVal * 4.0f;

        float modulatedPitch = currentPitchRatio * (1.0f + pitchMod);
        modulatedPitch = juce::jlimit(0.5f, 8.0f, modulatedPitch);

        int modulatedGrainSize = static_cast<int>(static_cast<float>(grainSizeSamples) * (1.0f + sizeMod));
        modulatedGrainSize = juce::jlimit(256, delayBufferSize / 4, modulatedGrainSize);

        // STEP 5: Process each channel
        for (int ch = 0; ch < processChannels; ++ch)
        {
            const float dryInput = buffer.getSample(ch, sample);

            // Read from overlapping grains
            float wetSum = 0.0f;
            float windowSum = 0.0f;

            for (int g = 0; g < numGrains; ++g)
            {
                const Grain& grain = grains[g];
                const float window = getGrainWindow(grain.phase);

                // Calculate read position with timing modulation
                float readPos = grain.readPosition;
                readPos += (g == 0) ? timeMod : -timeMod;

                // Wrap position
                const float bufSize = static_cast<float>(delayBufferSize);
                while (readPos < 0.0f) readPos += bufSize;
                while (readPos >= bufSize) readPos -= bufSize;

                // Read from delay buffer
                const float grainSample = readFromBuffer(ch, readPos);

                // Accumulate with window
                wetSum += grainSample * window;
                windowSum += window;
            }

            // Normalize by window sum (Hann windows with 50% overlap sum to ~1.0)
            float wetOutput;
            if (windowSum > 0.001f)
                wetOutput = wetSum / windowSum;
            else
                wetOutput = dryInput;  // Fallback to dry if windows are zero

            // Safety checks to prevent silence
            if (!std::isfinite(wetOutput))
                wetOutput = dryInput;

            // If wet signal is suspiciously silent but dry isn't, blend in dry
            // This prevents complete silence during problematic transitions
            const float wetLevel = std::abs(wetOutput);
            const float dryLevel = std::abs(dryInput);
            if (wetLevel < 0.0001f && dryLevel > 0.001f)
            {
                // Wet is silent but dry has signal - use dry to prevent silence
                wetOutput = dryInput;
            }

            // Mix dry and wet
            // When effectiveMix = 0, output = dry
            // When effectiveMix = 1, output = wet (pitch shifted)
            const float outputSample = dryInput * (1.0f - effectiveMix) + wetOutput * effectiveMix;

            // Write to output - ensure we never output silence when there's input
            float finalOutput = outputSample;
            if (!std::isfinite(finalOutput))
                finalOutput = dryInput;

            buffer.setSample(ch, sample, finalOutput);
        }

        // STEP 6: Update grain positions for next sample
        for (int g = 0; g < numGrains; ++g)
        {
            updateGrain(grains[g], modulatedPitch, modulatedGrainSize);
        }

        // STEP 7: Advance write position
        writePosition = (writePosition + 1) % delayBufferSize;

        // Track current mix for state
        currentMix = effectiveMix;

        // Check if transition complete
        if (transitionActive && std::abs(targetMix - mixSmoothState) < 0.001f)
        {
            transitionActive = false;
            if (targetMix < 0.001f)
            {
                mixSmoothState = 0.0f;
                currentPitchRatio = 1.0f;
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

} // namespace DSP
