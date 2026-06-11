#include "PluginProcessor.h"
#include "PluginEditor.h"

BlackheartAudioProcessor::BlackheartAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         ),
#else
    :
#endif
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    gainParam    = apvts.getRawParameterValue(ParameterIDs::gain);
    glareParam   = apvts.getRawParameterValue(ParameterIDs::glare);
    blendParam   = apvts.getRawParameterValue(ParameterIDs::blend);
    levelParam   = apvts.getRawParameterValue(ParameterIDs::level);
    speedParam   = apvts.getRawParameterValue(ParameterIDs::speed);
    chaosParam   = apvts.getRawParameterValue(ParameterIDs::chaos);
    riseParam    = apvts.getRawParameterValue(ParameterIDs::rise);
    octave1Param = apvts.getRawParameterValue(ParameterIDs::octave1);
    octave2Param = apvts.getRawParameterValue(ParameterIDs::octave2);
    modeParam = apvts.getRawParameterValue(ParameterIDs::mode);
    shapeParam = apvts.getRawParameterValue(ParameterIDs::shape);
    panicParam = apvts.getRawParameterValue(ParameterIDs::panic);
    chaosMixParam = apvts.getRawParameterValue(ParameterIDs::chaosMix);
}

BlackheartAudioProcessor::~BlackheartAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BlackheartAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto percentFormat = [](float value, int)
    {
        return juce::String(static_cast<int>(value * 100.0f)) + " %";
    };

    auto percentParse = [](const juce::String& text)
    {
        return text.trimCharactersAtEnd(" %").getFloatValue() / 100.0f;
    };

    auto hzFormat = [](float value, int)
    {
        if (value < 1.0f)
            return juce::String(value, 2) + " Hz";
        else if (value < 10.0f)
            return juce::String(value, 1) + " Hz";
        else
            return juce::String(static_cast<int>(value)) + " Hz";
    };

    auto hzParse = [](const juce::String& text)
    {
        return text.trimCharactersAtEnd(" Hz").getFloatValue();
    };

    auto msFormat = [](float value, int)
    {
        if (value < 10.0f)
            return juce::String(value, 1) + " ms";
        else
            return juce::String(static_cast<int>(value)) + " ms";
    };

    auto msParse = [](const juce::String& text)
    {
        return text.trimCharactersAtEnd(" ms").getFloatValue();
    };

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::gain, 1 },
        ParameterIDs::Labels::gain,
        ParameterIDs::gainRange(),
        ParameterIDs::Defaults::gain,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(percentFormat)
            .withValueFromStringFunction(percentParse)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::glare, 1 },
        ParameterIDs::Labels::glare,
        ParameterIDs::glareRange(),
        ParameterIDs::Defaults::glare,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(percentFormat)
            .withValueFromStringFunction(percentParse)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::blend, 1 },
        ParameterIDs::Labels::blend,
        ParameterIDs::blendRange(),
        ParameterIDs::Defaults::blend,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(percentFormat)
            .withValueFromStringFunction(percentParse)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::level, 1 },
        ParameterIDs::Labels::level,
        ParameterIDs::levelRange(),
        ParameterIDs::Defaults::level,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(percentFormat)
            .withValueFromStringFunction(percentParse)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::speed, 1 },
        ParameterIDs::Labels::speed,
        ParameterIDs::speedRange(),
        ParameterIDs::Defaults::speed,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(percentFormat)
            .withValueFromStringFunction(percentParse)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::chaos, 1 },
        ParameterIDs::Labels::chaos,
        ParameterIDs::chaosRange(),
        ParameterIDs::Defaults::chaos,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(percentFormat)
            .withValueFromStringFunction(percentParse)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::rise, 1 },
        ParameterIDs::Labels::rise,
        ParameterIDs::riseRange(),
        ParameterIDs::Defaults::rise,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(msFormat)
            .withValueFromStringFunction(msParse)));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParameterIDs::octave1, 1 },
        ParameterIDs::Labels::octave1,
        ParameterIDs::Defaults::octave1,
        juce::AudioParameterBoolAttributes()
            .withStringFromValueFunction([](bool value, int) { return value ? "ON" : "OFF"; })));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParameterIDs::octave2, 1 },
        ParameterIDs::Labels::octave2,
        ParameterIDs::Defaults::octave2,
        juce::AudioParameterBoolAttributes()
            .withStringFromValueFunction([](bool value, int) { return value ? "ON" : "OFF"; })));


    // MODE: 3-position switch (0=Screaming, 1=Overdrive, 2=Doom)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::mode, 1 },
        ParameterIDs::Labels::mode,
        ParameterIDs::modeRange(),
        ParameterIDs::Defaults::mode,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int) {
                int v = static_cast<int>(value + 0.5f);
                if (v == 0) return juce::String("Screaming");
                if (v == 2) return juce::String("Doom");
                return juce::String("Overdrive");
            })
            .withValueFromStringFunction([](const juce::String& text) {
                if (text.containsIgnoreCase("scream")) return 0.0f;
                if (text.containsIgnoreCase("doom")) return 2.0f;
                return 1.0f;
            })));

    // SHAPE: Active EQ sweep
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::shape, 1 },
        ParameterIDs::Labels::shape,
        ParameterIDs::shapeRange(),
        ParameterIDs::Defaults::shape,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(percentFormat)
            .withValueFromStringFunction(percentParse)));

    // PANIC: Detuned pitch destruction
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::panic, 1 },
        ParameterIDs::Labels::panic,
        ParameterIDs::panicRange(),
        ParameterIDs::Defaults::panic,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(percentFormat)
            .withValueFromStringFunction(percentParse)));

    // CHAOS MIX: Dry/wet mix for chaos/pitch shifting section
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::chaosMix, 1 },
        ParameterIDs::Labels::chaosMix,
        ParameterIDs::chaosMixRange(),
        ParameterIDs::Defaults::chaosMix,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(percentFormat)
            .withValueFromStringFunction(percentParse)));

    return layout;
}

const juce::String BlackheartAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BlackheartAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool BlackheartAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool BlackheartAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double BlackheartAudioProcessor::getTailLengthSeconds() const
{
    return 0.1;
}

int BlackheartAudioProcessor::getNumPrograms()
{
    return 1;
}

int BlackheartAudioProcessor::getCurrentProgram()
{
    return 0;
}

void BlackheartAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String BlackheartAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void BlackheartAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void BlackheartAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    isFirstBlock = true;
    stabilityError = false;
    consecutiveHighLevelBlocks = 0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(
        std::max(getTotalNumInputChannels(), getTotalNumOutputChannels()));

    // Initialize parameter smoothing
    smoothedParams.prepare(sampleRate);

    //==========================================================================
    // SIGNAL CHAIN PREPARATION (in processing order)
    //==========================================================================

    // Stage 1: Input Conditioning
    inputConditioner.prepare(spec);
    inputConditioner.setDCBlockEnabled(true);
    inputConditioner.setAntiAliasingEnabled(true);

    // Stage 2: Fuzz Engine
    fuzzEngine.prepare(spec);

    // Stage 3: Octave Generator
    octaveGenerator.prepare(spec);

    // Stage 4: Dynamic Gate
    dynamicGate.prepare(spec);
    dynamicGate.setAttackTime(1.0f);
    dynamicGate.setReleaseTime(50.0f);
    dynamicGate.setHoldTime(10.0f);

    // Stage 5: Blend Mixer
    blendMixer.prepare(spec);

    // Stage 6: Pitch Shifter
    pitchShifter.prepare(spec);

    // Stage 7: Chaos Modulator
    chaosModulator.prepare(spec);
    chaosModulator.setResponseCurve(DSP::ChaosModulator::ResponseCurve::Exponential);
    chaosModulator.setEnvelopeSensitivity(2.0f);
    chaosModulator.setEnvelopeThreshold(0.02f);
    chaosModulator.setEnvelopeAttack(3.0f);
    chaosModulator.setEnvelopeRelease(100.0f);

    // Stage 8: Output Limiter
    outputLimiter.prepare(spec);
    outputLimiter.setCeiling(-0.3f);
    outputLimiter.setHeadroom(-1.0f);

    //==========================================================================
    // ENVELOPE FOLLOWERS
    //==========================================================================

    inputEnvelopeFollower.prepare(spec);
    inputEnvelopeFollower.setAttackTime(5.0f);
    inputEnvelopeFollower.setReleaseTime(100.0f);
    inputEnvelopeFollower.setDetectionMode(DSP::EnvelopeFollower::DetectionMode::Peak);

    chaosEnvelopeFollower.prepare(spec);
    chaosEnvelopeFollower.setAttackTime(10.0f);
    chaosEnvelopeFollower.setReleaseTime(150.0f);
    chaosEnvelopeFollower.setDetectionMode(DSP::EnvelopeFollower::DetectionMode::RMS);

    //==========================================================================
    // BUFFER ALLOCATION
    //==========================================================================

    const int numChannels = static_cast<int>(spec.numChannels);
    dryBuffer.setSize(numChannels, samplesPerBlock);
    stagingBuffer.setSize(numChannels, samplesPerBlock);
    prePitchDryBuffer.setSize(numChannels, samplesPerBlock);

    pitchModBuffer.assign(static_cast<size_t>(samplesPerBlock), 0.0f);
    grainModBuffer.assign(static_cast<size_t>(samplesPerBlock), 0.0f);
    timingModBuffer.assign(static_cast<size_t>(samplesPerBlock), 0.0f);
    pitchShifter.setModulationBuffers(pitchModBuffer.data(), grainModBuffer.data(), timingModBuffer.data());

    //==========================================================================
    // LATENCY CALCULATION
    //==========================================================================

    pitchShifterLatency = pitchShifter.getLatencySamples();
    totalLatencySamples = pitchShifterLatency;
    setLatencySamples(totalLatencySamples);

    // Reset meters
    signalMeters.reset();
    inputEnvelope = 0.0f;
    chaosEnvelope = 0.0f;
}

void BlackheartAudioProcessor::releaseResources()
{
    inputConditioner.reset();
    fuzzEngine.reset();
    octaveGenerator.reset();
    dynamicGate.reset();
    blendMixer.reset();
    pitchShifter.reset();
    chaosModulator.reset();
    inputEnvelopeFollower.reset();
    chaosEnvelopeFollower.reset();
    outputLimiter.reset();

    signalMeters.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BlackheartAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

//==============================================================================
// GAIN STAGING HELPERS
//==============================================================================

float BlackheartAudioProcessor::measurePeakLevel(const juce::AudioBuffer<float>& buffer) const
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float channelPeak = buffer.getMagnitude(ch, 0, buffer.getNumSamples());
        peak = std::max(peak, channelPeak);
    }
    return peak;
}

void BlackheartAudioProcessor::applyInterstageProtection(juce::AudioBuffer<float>& buffer)
{
    const float peak = measurePeakLevel(buffer);

    if (peak > safetyClipThreshold)
    {
        stabilityError = true;
        // Only increment if truly extreme (> 2x safety threshold)
        // This prevents normal octave processing from triggering volume cuts
        if (peak > safetyClipThreshold * 2.0f)
            consecutiveHighLevelBlocks++;

        // Apply soft saturation instead of hard gain reduction
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float sample = data[i];
                const float absSample = std::abs(sample);
                if (absSample > internalClipThreshold)
                {
                    const float sign = sample > 0.0f ? 1.0f : -1.0f;
                    const float excess = absSample - internalClipThreshold;
                    // Same curve as the moderate branch (caps at threshold+1) —
                    // the old tanh(x*0.3)*2 variant capped HIGHER (threshold+2)
                    // exactly when levels were most extreme
                    data[i] = sign * (internalClipThreshold + std::tanh(excess * 0.5f));
                }
            }
        }

        signalMeters.internalClipping.store(true);
    }
    else if (peak > internalClipThreshold)
    {
        signalMeters.internalClipping.store(true);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (std::abs(data[i]) > internalClipThreshold)
                {
                    const float sign = data[i] > 0.0f ? 1.0f : -1.0f;
                    const float excess = std::abs(data[i]) - internalClipThreshold;
                    data[i] = sign * (internalClipThreshold + std::tanh(excess * 0.5f));
                }
            }
        }
        // Reset counter when in normal clipping range (not extreme)
        consecutiveHighLevelBlocks = 0;
    }
    else
    {
        consecutiveHighLevelBlocks = 0;
    }
}

void BlackheartAudioProcessor::checkAndReportClipping(float level, bool isInput, bool isOutput)
{
    if (level > 1.0f)
    {
        if (isInput)
            signalMeters.inputClipping.store(true);
        if (isOutput)
            signalMeters.outputClipping.store(true);
    }
}

//==============================================================================
// PARAMETER HANDLING
//==============================================================================

void BlackheartAudioProcessor::fetchParameterValues()
{
    currentGain   = gainParam->load();
    currentGlare  = glareParam->load();
    currentBlend  = blendParam->load();
    currentLevel  = levelParam->load();
    currentSpeed  = speedParam->load();
    currentChaos  = chaosParam->load();
    currentRise   = riseParam->load();
    currentOctave1 = octave1Param->load() > 0.5f;
    currentOctave2 = octave2Param->load() > 0.5f;
    currentMode = static_cast<int>(modeParam->load() + 0.5f);
    currentShape = shapeParam->load();
    currentPanic = panicParam->load();
    currentChaosMix = chaosMixParam->load();
}

void BlackheartAudioProcessor::updateDSPParameters()
{
    // Raw block-rate values: every consumer below runs its own per-sample
    // SmoothedValue, so an outer smoothing layer only added a skipped-to-target
    // snapshot on top of the real ramp (double smoothing, no benefit)
    const float smoothedGain   = currentGain;
    const float smoothedGlare  = currentGlare;
    const float smoothedBlend  = currentBlend;
    const float smoothedLevel  = currentLevel;
    const float smoothedSpeed  = currentSpeed;
    const float smoothedChaos  = currentChaos;
    const float smoothedRise   = currentRise;
    const float smoothedShape  = currentShape;
    // Note: octave1/octave2 SmoothedValues intentionally not read here.
    // Octave buttons use raw booleans (currentOctave1/2) for instant response.

    // Fuzz Engine parameters
    fuzzEngine.setGain(smoothedGain);
    fuzzEngine.setLevel(smoothedLevel);
    fuzzEngine.setMode(currentMode);
    fuzzEngine.setShape(smoothedShape);

    // Octave Generator parameters
    octaveGenerator.setGlare(smoothedGlare);

    // Dynamic Gate parameters (influenced by gain and glare for spitty behavior)
    dynamicGate.setGainInfluence(smoothedGain);
    dynamicGate.setGlareInfluence(smoothedGlare);

    // Blend Mixer parameters
    blendMixer.setBlend(smoothedBlend);

    // Pitch Shifter parameters — use raw booleans, not smoothed values.
    // Octave buttons are momentary and need instant activation.
    // The PitchShifter handles its own Rise-based smoothing internally.
    pitchShifter.setOctaveOneActive(currentOctave1);
    pitchShifter.setOctaveTwoActive(currentOctave2);
    pitchShifter.setRiseTime(smoothedRise);
    pitchShifter.setChaosAmount(smoothedChaos);
    pitchShifter.setPanic(currentPanic);
    pitchShifter.setRingModSpeed(smoothedSpeed);

    // Chaos Modulator parameters
    chaosModulator.setSpeed(smoothedSpeed);
    chaosModulator.setChaos(smoothedChaos);
}

void BlackheartAudioProcessor::setOctave1(bool active)
{
    if (auto* param = apvts.getParameter(ParameterIDs::octave1))
        param->setValueNotifyingHost(active ? 1.0f : 0.0f);
}

void BlackheartAudioProcessor::setOctave2(bool active)
{
    if (auto* param = apvts.getParameter(ParameterIDs::octave2))
        param->setValueNotifyingHost(active ? 1.0f : 0.0f);
}

//==============================================================================
// MAIN PROCESSING
//==============================================================================

void BlackheartAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto cpuStartTicks = juce::Time::getHighResolutionTicks();

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Host violated prepareToPlay contract — never allocate on audio thread
    if (dryBuffer.getNumSamples() < numSamples || dryBuffer.getNumChannels() < numChannels
        || stagingBuffer.getNumSamples() < numSamples || stagingBuffer.getNumChannels() < numChannels
        || prePitchDryBuffer.getNumSamples() < numSamples || prePitchDryBuffer.getNumChannels() < numChannels)
    {
        buffer.clear();
        return;
    }

    //==========================================================================
    // PARAMETER FETCH AND SMOOTHING
    //==========================================================================

    fetchParameterValues();

    const float oct1Float = currentOctave1 ? 1.0f : 0.0f;
    const float oct2Float = currentOctave2 ? 1.0f : 0.0f;

    if (isFirstBlock)
    {
        smoothedParams.setCurrentAndTargetValue(
            currentGain, currentGlare, currentBlend, currentLevel,
            currentSpeed, currentChaos, currentRise, oct1Float, oct2Float, currentShape, currentPanic, currentChaosMix);
        isFirstBlock = false;
    }
    else
    {
        smoothedParams.updateTargets(
            currentGain, currentGlare, currentBlend, currentLevel,
            currentSpeed, currentChaos, currentRise, oct1Float, oct2Float, currentShape, currentPanic, currentChaosMix);
    }

    // Module parameters are smoothed inside each DSP class; only chaosMix is
    // consumed per-sample from smoothedParams (in the chaos mix loop below)
    updateDSPParameters();

    //==========================================================================
    // STAGE 0: INPUT METERING
    //==========================================================================

    const float inputLevel = measurePeakLevel(buffer);
    signalMeters.inputLevel.store(inputLevel);
    checkAndReportClipping(inputLevel, true, false);

    // Track input envelope for UI and internal use
    inputEnvelope = inputEnvelopeFollower.processBlock(buffer);

    //==========================================================================
    // TEST MODE: Early exit after input conditioning
    //==========================================================================

    if (testModeEnabled.load(std::memory_order_relaxed))
    {
        inputConditioner.process(buffer);
        smoothedParams.chaosMix.skip(numSamples);
        return;
    }

    //==========================================================================
    // STAGE 1: INPUT CONDITIONING
    // - DC blocking, anti-aliasing, level normalization
    //==========================================================================

    inputConditioner.process(buffer);

    //==========================================================================
    // STAGE 2: PRESERVE DRY SIGNAL FOR BLEND
    // Captured AFTER conditioning: blending a raw (DC-laden, unfiltered) dry
    // path against a conditioned wet path beats and skews the mix
    //==========================================================================

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Track chaos envelope from the conditioned, pre-fuzz signal — post-blend
    // the fuzz has compressed dynamics flat, leaving envelope-responsive chaos
    // with nothing to respond to at high gain
    chaosEnvelope = chaosEnvelopeFollower.processBlock(buffer);
    chaosModulator.setEnvelopeValue(chaosEnvelope);

    //==========================================================================
    // STAGE 3: FUZZ ENGINE
    // - Nonlinear waveshaping, compression, saturation
    // - Gain and Level parameters control intensity
    //==========================================================================

    fuzzEngine.process(buffer);

    //==========================================================================
    // STAGE 4: OCTAVE GENERATOR
    // - Full-wave rectification for octave-up harmonics
    // - Glare parameter controls octave blend
    //==========================================================================

    octaveGenerator.process(buffer);

    // Single interstage protection point: fuzz output is self-bounded (1.2x)
    // and the octave contribution is capped, so post-octave is the only spot
    // where summed level can still exceed the internal budget
    applyInterstageProtection(buffer);

    //==========================================================================
    // STAGE 5: DYNAMIC GATE
    // - Envelope-driven gating for spitty, broken-up textures
    // - Threshold influenced by Gain and Glare settings
    //==========================================================================

    dynamicGate.process(buffer);

    //==========================================================================
    // STAGE 6: BLEND MIXER
    // - Equal-power crossfade between dry and wet signals
    // - Blend: 0% = full dry, 100% = full wet
    //==========================================================================

    // Copy wet signal to staging buffer
    for (int ch = 0; ch < numChannels; ++ch)
        stagingBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    blendMixer.process(dryBuffer, stagingBuffer, buffer);

    //==========================================================================
    // STAGE 7: CHAOS MODULATOR + PITCH SHIFTER
    // - Envelope-responsive modulation system
    // - Granular pitch shifting with +1/+2 octave modes
    //==========================================================================

    // Generate per-sample modulation into pre-allocated buffers — the pitch
    // shifter reads these per sample (block-rate consumption aliased the LFO)
    chaosModulator.processToBuffers(pitchModBuffer.data(), grainModBuffer.data(),
                                    timingModBuffer.data(), numSamples);
    const auto chaosMod = chaosModulator.getModulation();

    // Store chaos modulation for visualization (lock-free)
    chaosModValue.store(chaosMod.combinedMod, std::memory_order_relaxed);

    // Save pre-pitch dry signal for chaos mix (pre-allocated in prepareToPlay)
    for (int ch = 0; ch < numChannels; ++ch)
        prePitchDryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Process pitch shifting
    pitchShifter.process(buffer);

    // Apply chaos mix (dry/wet blend for pitch section), ramped per sample
    {
        const int mixChannels = buffer.getNumChannels();
        float* wetPtrs[8] = {};
        const float* dryPtrs[8] = {};
        const int clampedChannels = std::min(mixChannels, 8);
        for (int ch = 0; ch < clampedChannels; ++ch)
        {
            wetPtrs[ch] = buffer.getWritePointer(ch);
            dryPtrs[ch] = prePitchDryBuffer.getReadPointer(ch);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            const float mix = smoothedParams.chaosMix.getNextValue();
            for (int ch = 0; ch < clampedChannels; ++ch)
                wetPtrs[ch][i] = dryPtrs[ch][i] + mix * (wetPtrs[ch][i] - dryPtrs[ch][i]);
        }
    }

    //==========================================================================
    // STAGE 8: OUTPUT LIMITER
    // - Soft clipping with tanh saturation
    // - DC blocking and headroom management
    //==========================================================================

    outputLimiter.process(buffer);

    float outputLevel = measurePeakLevel(buffer);
    signalMeters.outputLevel.store(outputLevel);
    checkAndReportClipping(outputLevel, false, true);

    // Push waveform data for visualization (lock-free, downsampled)
    if (buffer.getNumChannels() > 0)
        waveformFifo.pushBlock(buffer.getReadPointer(0), numSamples);

    // Safety check: if we've had too many consecutive high-level blocks, apply gradual reduction
    // instead of sudden halving which causes audible volume drops
    if (consecutiveHighLevelBlocks > maxConsecutiveHighLevelBlocks)
    {
        // Apply soft limiting instead of hard volume cut
        // The output limiter should handle this, so just reset the counter
        consecutiveHighLevelBlocks = 0;
        stabilityError = true;
    }

    // CPU load: elapsed processing time / block duration. EMA smoothed.
    {
        const auto cpuEndTicks = juce::Time::getHighResolutionTicks();
        const double elapsedSec = juce::Time::highResolutionTicksToSeconds(cpuEndTicks - cpuStartTicks);
        const double blockSec = currentSampleRate > 0.0
            ? static_cast<double>(numSamples) / currentSampleRate
            : 0.0;
        const float instant = blockSec > 0.0 ? juce::jlimit(0.0f, 1.0f, static_cast<float>(elapsedSec / blockSec)) : 0.0f;
        constexpr float alpha = 0.1f;
        const float prev = cpuLoad.load(std::memory_order_relaxed);
        cpuLoad.store(prev + alpha * (instant - prev), std::memory_order_relaxed);
    }
}

//==============================================================================
// EDITOR
//==============================================================================

bool BlackheartAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* BlackheartAudioProcessor::createEditor()
{
    return new BlackheartAudioProcessorEditor(*this);
}

//==============================================================================
// STATE PERSISTENCE
//==============================================================================

void BlackheartAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    if (xml != nullptr)
    {
        xml->setAttribute("pluginVersion", JucePlugin_VersionString);
        copyXmlToBinary(*xml, destData);
    }
}

void BlackheartAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

            fetchParameterValues();

            const float oct1Float = currentOctave1 ? 1.0f : 0.0f;
            const float oct2Float = currentOctave2 ? 1.0f : 0.0f;

        smoothedParams.setCurrentAndTargetValue(
            currentGain, currentGlare, currentBlend, currentLevel,
            currentSpeed, currentChaos, currentRise, oct1Float, oct2Float, currentShape, currentPanic, currentChaosMix);
        }
    }
}

//==============================================================================
// PLUGIN INSTANTIATION
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BlackheartAudioProcessor();
}
