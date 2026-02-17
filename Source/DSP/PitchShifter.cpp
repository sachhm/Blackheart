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

    windowSizeSamples = static_cast<int>(defaultWindowMs * 0.001 * sampleRate);
    windowSizeSamples = juce::jlimit(256, delayBufferSize / 4, windowSizeSamples);

    setRiseTime(riseTimeMs);

    for (int ch = 0; ch < maxChannels; ++ch)
        for (int i = 0; i < delayBufferSize; ++i)
            delayBuffer[ch][i] = 0.0f;

    writePosition = 0;

    // Initialize heads 180° out of phase
    mainHeads[0].readPosition = 0.0f;
    mainHeads[0].ramp = 0.0f;
    mainHeads[1].readPosition = 0.0f;
    mainHeads[1].ramp = 0.5f;

    detuneHeads[0].readPosition = 0.0f;
    detuneHeads[0].ramp = 0.0f;
    detuneHeads[1].readPosition = 0.0f;
    detuneHeads[1].ramp = 0.5f;

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
    transitionActive = false;

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

    mainHeads[0] = { 0.0f, 0.0f };
    mainHeads[1] = { 0.0f, 0.5f };
    detuneHeads[0] = { 0.0f, 0.0f };
    detuneHeads[1] = { 0.0f, 0.5f };

    mixSmoothState = 0.0f;
    currentPitchRatio = 1.0f;
    currentMix = 0.0f;
    feedbackSample = 0.0f;
    ringModPhase = 0.0f;
    dryEnvelope = 0.0f;
    wetEnvelope = 0.0f;
    transitionActive = false;

    prevOctaveOneActive = false;
    prevOctaveTwoActive = false;
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

    // 4-point Hermite cubic interpolation
    const int idx0 = static_cast<int>(position);
    const int idxM1 = (idx0 - 1 + delayBufferSize) % delayBufferSize;
    const int idx1 = (idx0 + 1) % delayBufferSize;
    const int idx2 = (idx0 + 2) % delayBufferSize;
    const float frac = position - static_cast<float>(idx0);

    const float y0 = delayBuffer[channel][idxM1];
    const float y1 = delayBuffer[channel][idx0];
    const float y2 = delayBuffer[channel][idx1];
    const float y3 = delayBuffer[channel][idx2];

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void PitchShifter::resetHeadsForTransition()
{
    const float bufSize = static_cast<float>(delayBufferSize);
    const float startPos = static_cast<float>(writePosition) - static_cast<float>(windowSizeSamples);

    float pos = startPos;
    if (pos < 0.0f) pos += bufSize;

    mainHeads[0].readPosition = pos;
    mainHeads[0].ramp = 0.0f;
    mainHeads[1].readPosition = pos;
    mainHeads[1].ramp = 0.5f;

    detuneHeads[0].readPosition = pos;
    detuneHeads[0].ramp = 0.25f;
    detuneHeads[1].readPosition = pos;
    detuneHeads[1].ramp = 0.75f;
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
        resetHeadsForTransition();
        transitionActive = true;
    }

    prevOctaveOneActive = oct1Active;
    prevOctaveTwoActive = oct2Active;

    const float safeRiseCoeff = (riseCoeff > 0.0f && std::isfinite(riseCoeff)) ? riseCoeff : 0.002f;
    const float safeFallCoeff = (fallCoeff > 0.0f && std::isfinite(fallCoeff)) ? fallCoeff : 0.002f;

    const float bufSize = static_cast<float>(delayBufferSize);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float chaosVal = chaos.getNextValue();
        const float panicVal = panic.getNextValue();

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

        // Smooth pitch ratio and mix
        const float smoothCoeff = (targetMix > mixSmoothState) ? safeRiseCoeff : safeFallCoeff;
        currentPitchRatio += smoothCoeff * (targetPitchRatio - currentPitchRatio);
        mixSmoothState += smoothCoeff * (targetMix - mixSmoothState);
        currentPitchRatio = juce::jlimit(1.0f, 8.0f, currentPitchRatio);
        mixSmoothState = juce::jlimit(0.0f, 1.0f, mixSmoothState);

        // Smoothstep S-curve for mix
        const float mx = mixSmoothState;
        const float effectiveMix = mx * mx * (3.0f - 2.0f * mx);

        // Apply chaos modulation to pitch ratio
        const float pitchMod = pitchModulation * chaosVal * 0.4f;
        float modulatedPitch = currentPitchRatio * (1.0f + pitchMod);
        modulatedPitch = juce::jlimit(0.5f, 8.0f, modulatedPitch);

        // Chaos modulates window size: 30ms at chaos=0, down to 10ms at chaos=1
        const float windowMod = grainSizeModulation * chaosVal * 0.6f;
        const float chaosWindowMs = defaultWindowMs - chaosVal * (defaultWindowMs - minWindowMs);
        int modWindowSize = static_cast<int>(chaosWindowMs * 0.001f * static_cast<float>(sampleRate) * (1.0f + windowMod));
        modWindowSize = juce::jlimit(256, delayBufferSize / 4, modWindowSize);

        // Crossfade harshness: at high chaos, crossfade becomes sharper
        // harshness 0 = smooth cosine, 1 = nearly rectangular (hard glitch)
        const float harshness = chaosVal * chaosVal;

        // Timing jitter for reset position (chaos-driven)
        const float resetJitter = timingModulation * chaosVal * static_cast<float>(modWindowSize) * 0.3f;

        // Ramp increment: how fast each head sweeps through its window
        // At pitchRatio=2, read advances 2x faster than write, so the head
        // pulls ahead by (pitchRatio-1) samples per sample
        const float rampInc = (modulatedPitch - 1.0f) / static_cast<float>(modWindowSize);

        // Detune ratios for PANIC
        const float detuneUp = 1.0f + panicVal * 0.15f;
        const float detuneDown = 1.0f - panicVal * 0.15f;

        float wetSumMono = 0.0f;

        for (int ch = 0; ch < processChannels; ++ch)
        {
            const float dryInput = buffer.getSample(ch, sample);
            float wetSum = 0.0f;

            // Process main heads
            for (int h = 0; h < numMainHeads; ++h)
            {
                // Compute crossfade gain from ramp position
                // Hann-like envelope: gain = 0.5 - 0.5 * cos(2*PI*ramp)
                float gain;
                const float ramp = mainHeads[h].ramp;
                if (harshness < 0.001f)
                {
                    // Pure cosine crossfade
                    gain = 0.5f - 0.5f * LookupTables::fastCos(ramp);
                }
                else
                {
                    // Blend between cosine and rectangular based on harshness
                    const float cosGain = 0.5f - 0.5f * LookupTables::fastCos(ramp);
                    // Rectangular: 1.0 in middle, 0.0 at edges
                    const float edgeFade = 0.05f * (1.0f - harshness) + 0.001f;
                    float rectGain = 1.0f;
                    if (ramp < edgeFade)
                        rectGain = ramp / edgeFade;
                    else if (ramp > 1.0f - edgeFade)
                        rectGain = (1.0f - ramp) / edgeFade;
                    gain = cosGain * (1.0f - harshness) + rectGain * harshness;
                }

                wetSum += readFromBuffer(ch, mainHeads[h].readPosition) * gain;
            }

            // PANIC detuned heads
            if (panicVal > 0.001f)
            {
                for (int h = 0; h < numDetuneHeads; ++h)
                {
                    const float ramp = detuneHeads[h].ramp;
                    const float gain = 0.5f - 0.5f * LookupTables::fastCos(ramp);
                    wetSum += readFromBuffer(ch, detuneHeads[h].readPosition) * gain * panicVal;
                }
            }

            // Normalize: main heads sum to ~1.0 with cosine crossfade (two 180° offset Hann = constant)
            // PANIC adds on top, scaled by panicVal
            float wetOutput = wetSum;

            if (!std::isfinite(wetOutput))
                wetOutput = dryInput;

            // Ring modulation (post-pitch, pre-mix)
            if (ringModMix > 0.001f)
            {
                const float ringModSine = std::sin(ringModPhase * juce::MathConstants<float>::twoPi);
                wetOutput = wetOutput * (1.0f - ringModMix) + wetOutput * ringModSine * ringModMix;
            }

            // Gain compensation: match wet level to dry level
            {
                const float dryAbs = std::abs(dryInput);
                const float wetAbs = std::abs(wetOutput);

                const float dryCoeff = (dryAbs > dryEnvelope) ? envelopeAttack : envelopeRelease;
                dryEnvelope += dryCoeff * (dryAbs - dryEnvelope);

                const float wetCoeff = (wetAbs > wetEnvelope) ? envelopeAttack : envelopeRelease;
                wetEnvelope += wetCoeff * (wetAbs - wetEnvelope);

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

        // Store feedback sample
        feedbackSample = std::tanh(wetSumMono / static_cast<float>(processChannels));

        // Advance main heads
        for (int h = 0; h < numMainHeads; ++h)
        {
            mainHeads[h].ramp += rampInc;
            mainHeads[h].readPosition += modulatedPitch;

            // Wrap read position
            while (mainHeads[h].readPosition >= bufSize)
                mainHeads[h].readPosition -= bufSize;
            while (mainHeads[h].readPosition < 0.0f)
                mainHeads[h].readPosition += bufSize;

            // When ramp completes, reset head back near the write position
            if (mainHeads[h].ramp >= 1.0f)
            {
                mainHeads[h].ramp -= 1.0f;
                float resetPos = static_cast<float>(writePosition) - static_cast<float>(modWindowSize);
                resetPos += resetJitter * (random.nextFloat() * 2.0f - 1.0f);
                while (resetPos < 0.0f) resetPos += bufSize;
                while (resetPos >= bufSize) resetPos -= bufSize;
                mainHeads[h].readPosition = resetPos;
            }
        }

        // Advance detune heads (PANIC)
        if (panicVal > 0.001f)
        {
            const float detuneRampIncUp = (modulatedPitch * detuneUp - 1.0f) / static_cast<float>(modWindowSize);
            const float detuneRampIncDown = (modulatedPitch * detuneDown - 1.0f) / static_cast<float>(modWindowSize);

            detuneHeads[0].ramp += detuneRampIncUp;
            detuneHeads[0].readPosition += modulatedPitch * detuneUp;
            while (detuneHeads[0].readPosition >= bufSize) detuneHeads[0].readPosition -= bufSize;
            while (detuneHeads[0].readPosition < 0.0f) detuneHeads[0].readPosition += bufSize;
            if (detuneHeads[0].ramp >= 1.0f)
            {
                detuneHeads[0].ramp -= 1.0f;
                float resetPos = static_cast<float>(writePosition) - static_cast<float>(modWindowSize);
                while (resetPos < 0.0f) resetPos += bufSize;
                detuneHeads[0].readPosition = resetPos;
            }

            detuneHeads[1].ramp += detuneRampIncDown;
            detuneHeads[1].readPosition += modulatedPitch * detuneDown;
            while (detuneHeads[1].readPosition >= bufSize) detuneHeads[1].readPosition -= bufSize;
            while (detuneHeads[1].readPosition < 0.0f) detuneHeads[1].readPosition += bufSize;
            if (detuneHeads[1].ramp >= 1.0f)
            {
                detuneHeads[1].ramp -= 1.0f;
                float resetPos = static_cast<float>(writePosition) - static_cast<float>(modWindowSize);
                while (resetPos < 0.0f) resetPos += bufSize;
                detuneHeads[1].readPosition = resetPos;
            }
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
