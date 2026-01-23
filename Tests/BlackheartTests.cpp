/**
 * Blackheart Plugin Test Suite
 *
 * Automated tests for parameter stability, latency verification,
 * and state persistence. Run via standalone app test mode.
 */

#include "../Source/PluginProcessor.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>

//==============================================================================
// Test Utilities
//==============================================================================

struct TestResult
{
    std::string name;
    bool passed;
    std::string details;
};

std::vector<TestResult> testResults;

void logTest(const std::string& name, bool passed, const std::string& details = "")
{
    testResults.push_back({ name, passed, details });
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << name;
    if (!details.empty())
        std::cout << " - " << details;
    std::cout << std::endl;
}

float calculateRMS(const juce::AudioBuffer<float>& buffer)
{
    float sum = 0.0f;
    int totalSamples = 0;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sum += data[i] * data[i];
            totalSamples++;
        }
    }

    return totalSamples > 0 ? std::sqrt(sum / totalSamples) : 0.0f;
}

float calculatePeak(const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float sample = std::abs(buffer.getSample(ch, i));
            if (sample > peak)
                peak = sample;
        }
    }

    return peak;
}

bool hasNaN(const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (std::isnan(data[i]) || std::isinf(data[i]))
                return true;
        }
    }
    return false;
}

void fillWithSineWave(juce::AudioBuffer<float>& buffer, float frequency, double sampleRate)
{
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float phaseIncrement = twoPi * frequency / static_cast<float>(sampleRate);
    float phase = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float sample = std::sin(phase) * 0.5f;  // -6dB level
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.setSample(ch, i, sample);
        }
        phase += phaseIncrement;
        if (phase > twoPi) phase -= twoPi;
    }
}

void fillWithImpulse(juce::AudioBuffer<float>& buffer)
{
    buffer.clear();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        buffer.setSample(ch, 0, 1.0f);
    }
}

//==============================================================================
// Test 1: Parameter Stability Tests
//==============================================================================

void testParameterStability()
{
    std::cout << "\n=== Parameter Stability Tests ===" << std::endl;

    BlackheartAudioProcessor processor;

    // Test at 48kHz with 256 sample buffer
    const double sampleRate = 48000.0;
    const int bufferSize = 256;

    processor.prepareToPlay(sampleRate, bufferSize);

    juce::AudioBuffer<float> buffer(2, bufferSize);
    juce::MidiBuffer midiBuffer;

    auto& apvts = processor.getAPVTS();

    // Test all parameters at extreme values
    struct ParamTest
    {
        const char* id;
        float minVal;
        float maxVal;
    };

    std::vector<ParamTest> paramTests = {
        { "gain", 0.0f, 1.0f },
        { "glare", 0.0f, 1.0f },
        { "blend", 0.0f, 1.0f },
        { "level", 0.0f, 1.0f },
        { "speed", 0.1f, 20.0f },
        { "chaos", 0.0f, 1.0f },
        { "rise", 1.0f, 500.0f }
    };

    bool allPassed = true;

    for (const auto& test : paramTests)
    {
        auto* param = apvts.getParameter(test.id);
        if (!param)
        {
            logTest(std::string("Parameter exists: ") + test.id, false, "Parameter not found");
            allPassed = false;
            continue;
        }

        // Test minimum value
        param->setValueNotifyingHost(param->convertTo0to1(test.minVal));
        fillWithSineWave(buffer, 440.0f, sampleRate);
        processor.processBlock(buffer, midiBuffer);

        bool minPassed = !hasNaN(buffer) && calculatePeak(buffer) < 10.0f;

        // Test maximum value
        param->setValueNotifyingHost(param->convertTo0to1(test.maxVal));
        fillWithSineWave(buffer, 440.0f, sampleRate);
        processor.processBlock(buffer, midiBuffer);

        bool maxPassed = !hasNaN(buffer) && calculatePeak(buffer) < 10.0f;

        bool testPassed = minPassed && maxPassed;
        logTest(std::string("Parameter stability: ") + test.id, testPassed,
                testPassed ? "" : (minPassed ? "Failed at max" : "Failed at min"));

        if (!testPassed) allPassed = false;
    }

    // Test all parameters at extreme values simultaneously
    for (const auto& test : paramTests)
    {
        auto* param = apvts.getParameter(test.id);
        if (param)
            param->setValueNotifyingHost(1.0f);  // All at max
    }

    // Enable both octaves
    processor.setOctave1(true);
    processor.setOctave2(true);

    // Process multiple blocks
    bool extremePassed = true;
    for (int block = 0; block < 100; ++block)
    {
        fillWithSineWave(buffer, 82.41f, sampleRate);  // Low E string
        processor.processBlock(buffer, midiBuffer);

        if (hasNaN(buffer) || calculatePeak(buffer) > 10.0f)
        {
            extremePassed = false;
            break;
        }
    }

    logTest("All parameters at extreme values", extremePassed);

    processor.setOctave1(false);
    processor.setOctave2(false);
    processor.releaseResources();
}

//==============================================================================
// Test 2: Octave Button Momentary Behavior
//==============================================================================

void testOctaveButtonBehavior()
{
    std::cout << "\n=== Octave Button Tests ===" << std::endl;

    BlackheartAudioProcessor processor;

    processor.prepareToPlay(48000.0, 256);

    // Initially off
    bool initialOct1 = processor.getOctave1();
    bool initialOct2 = processor.getOctave2();
    logTest("Octave 1 initially off", !initialOct1);
    logTest("Octave 2 initially off", !initialOct2);

    // Turn on
    processor.setOctave1(true);
    logTest("Octave 1 activates when set", processor.getOctave1());

    processor.setOctave2(true);
    logTest("Octave 2 activates when set", processor.getOctave2());

    // Turn off
    processor.setOctave1(false);
    logTest("Octave 1 deactivates when released", !processor.getOctave1());

    processor.setOctave2(false);
    logTest("Octave 2 deactivates when released", !processor.getOctave2());

    // Verify state doesn't latch after processing
    processor.setOctave1(true);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midiBuffer;
    fillWithSineWave(buffer, 440.0f, 48000.0);

    // Process several blocks while held
    for (int i = 0; i < 10; ++i)
        processor.processBlock(buffer, midiBuffer);

    logTest("Octave 1 stays on during processing", processor.getOctave1());

    // Release
    processor.setOctave1(false);

    // Process more blocks
    for (int i = 0; i < 10; ++i)
        processor.processBlock(buffer, midiBuffer);

    logTest("Octave 1 stays off after release", !processor.getOctave1());

    processor.releaseResources();
}

//==============================================================================
// Test 3: Latency Verification
//==============================================================================

void testLatencyVerification()
{
    std::cout << "\n=== Latency Tests ===" << std::endl;

    struct SampleRateTest
    {
        double sampleRate;
        int bufferSize;
        float maxLatencyMs;
    };

    std::vector<SampleRateTest> tests = {
        { 44100.0, 64, 10.0f },
        { 44100.0, 128, 10.0f },
        { 44100.0, 256, 10.0f },
        { 44100.0, 512, 10.0f },
        { 48000.0, 64, 10.0f },
        { 48000.0, 128, 10.0f },
        { 48000.0, 256, 10.0f },
        { 48000.0, 512, 10.0f },
        { 96000.0, 64, 10.0f },
        { 96000.0, 128, 10.0f },
        { 96000.0, 256, 10.0f },
        { 96000.0, 512, 10.0f },
    };

    for (const auto& test : tests)
    {
        BlackheartAudioProcessor processor;
        processor.prepareToPlay(test.sampleRate, test.bufferSize);

        int latencySamples = processor.getLatencyInSamples();
        float latencyMs = (latencySamples / static_cast<float>(test.sampleRate)) * 1000.0f;

        bool passed = latencyMs < test.maxLatencyMs;

        std::stringstream details;
        details << std::fixed << std::setprecision(2);
        details << latencyMs << "ms (" << latencySamples << " samples) @ "
                << static_cast<int>(test.sampleRate) << "Hz, " << test.bufferSize << " buf";

        logTest("Latency < 10ms", passed, details.str());

        processor.releaseResources();
    }
}

//==============================================================================
// Test 4: Sample Rate Compatibility
//==============================================================================

void testSampleRateCompatibility()
{
    std::cout << "\n=== Sample Rate Compatibility Tests ===" << std::endl;

    std::vector<double> sampleRates = { 44100.0, 48000.0, 88200.0, 96000.0 };

    for (double sampleRate : sampleRates)
    {
        BlackheartAudioProcessor processor;
        const int bufferSize = 256;

        processor.prepareToPlay(sampleRate, bufferSize);

        juce::AudioBuffer<float> buffer(2, bufferSize);
        juce::MidiBuffer midiBuffer;

        bool passed = true;
        std::string failReason;

        // Test with various input signals
        std::vector<float> testFrequencies = { 82.41f, 196.0f, 440.0f, 880.0f, 2000.0f };

        for (float freq : testFrequencies)
        {
            fillWithSineWave(buffer, freq, sampleRate);
            processor.processBlock(buffer, midiBuffer);

            if (hasNaN(buffer))
            {
                passed = false;
                failReason = "NaN at " + std::to_string(static_cast<int>(freq)) + "Hz";
                break;
            }

            if (calculatePeak(buffer) > 5.0f)
            {
                passed = false;
                failReason = "Peak overflow at " + std::to_string(static_cast<int>(freq)) + "Hz";
                break;
            }
        }

        // Test with silence
        buffer.clear();
        processor.processBlock(buffer, midiBuffer);
        float silenceLevel = calculateRMS(buffer);

        if (silenceLevel > 0.001f)
        {
            passed = false;
            failReason = "Self-oscillation detected (RMS: " + std::to_string(silenceLevel) + ")";
        }

        std::stringstream name;
        name << "Sample rate: " << static_cast<int>(sampleRate) << " Hz";
        logTest(name.str(), passed, failReason);

        processor.releaseResources();
    }
}

//==============================================================================
// Test 5: Buffer Size Compatibility
//==============================================================================

void testBufferSizeCompatibility()
{
    std::cout << "\n=== Buffer Size Compatibility Tests ===" << std::endl;

    std::vector<int> bufferSizes = { 32, 64, 128, 256, 512, 1024, 2048 };
    const double sampleRate = 48000.0;

    for (int bufferSize : bufferSizes)
    {
        BlackheartAudioProcessor processor;
        processor.prepareToPlay(sampleRate, bufferSize);

        juce::AudioBuffer<float> buffer(2, bufferSize);
        juce::MidiBuffer midiBuffer;

        bool passed = true;
        std::string failReason;

        // Process multiple blocks
        for (int block = 0; block < 50; ++block)
        {
            fillWithSineWave(buffer, 440.0f, sampleRate);

            // Randomly toggle octaves
            if (block % 10 == 0)
                processor.setOctave1(block % 20 == 0);
            if (block % 15 == 0)
                processor.setOctave2(block % 30 == 0);

            processor.processBlock(buffer, midiBuffer);

            if (hasNaN(buffer))
            {
                passed = false;
                failReason = "NaN at block " + std::to_string(block);
                break;
            }
        }

        std::stringstream name;
        name << "Buffer size: " << bufferSize << " samples";
        logTest(name.str(), passed, failReason);

        processor.setOctave1(false);
        processor.setOctave2(false);
        processor.releaseResources();
    }
}

//==============================================================================
// Test 6: State Save/Load
//==============================================================================

void testStatePersistence()
{
    std::cout << "\n=== State Persistence Tests ===" << std::endl;

    juce::MemoryBlock stateData;

    // Create processor and set specific state
    {
        BlackheartAudioProcessor processor;
        processor.prepareToPlay(48000.0, 256);

        auto& apvts = processor.getAPVTS();

        // Set specific parameter values
        if (auto* p = apvts.getParameter("gain"))
            p->setValueNotifyingHost(0.75f);
        if (auto* p = apvts.getParameter("glare"))
            p->setValueNotifyingHost(0.6f);
        if (auto* p = apvts.getParameter("blend"))
            p->setValueNotifyingHost(0.8f);
        if (auto* p = apvts.getParameter("level"))
            p->setValueNotifyingHost(0.5f);
        if (auto* p = apvts.getParameter("chaos"))
            p->setValueNotifyingHost(0.4f);

        // Save state
        processor.getStateInformation(stateData);

        logTest("State saved", stateData.getSize() > 0,
                "Size: " + std::to_string(stateData.getSize()) + " bytes");

        processor.releaseResources();
    }

    // Create new processor and restore state
    {
        BlackheartAudioProcessor processor;
        processor.prepareToPlay(48000.0, 256);

        // Restore state
        processor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

        auto& apvts = processor.getAPVTS();

        // Verify parameter values
        bool gainOk = false, glareOk = false, blendOk = false, levelOk = false, chaosOk = false;

        if (auto* p = apvts.getParameter("gain"))
            gainOk = std::abs(p->getValue() - 0.75f) < 0.01f;
        if (auto* p = apvts.getParameter("glare"))
            glareOk = std::abs(p->getValue() - 0.6f) < 0.01f;
        if (auto* p = apvts.getParameter("blend"))
            blendOk = std::abs(p->getValue() - 0.8f) < 0.01f;
        if (auto* p = apvts.getParameter("level"))
            levelOk = std::abs(p->getValue() - 0.5f) < 0.01f;
        if (auto* p = apvts.getParameter("chaos"))
            chaosOk = std::abs(p->getValue() - 0.4f) < 0.01f;

        logTest("State restored: gain", gainOk);
        logTest("State restored: glare", glareOk);
        logTest("State restored: blend", blendOk);
        logTest("State restored: level", levelOk);
        logTest("State restored: chaos", chaosOk);

        processor.releaseResources();
    }
}

//==============================================================================
// Test 7: Stability Under Stress
//==============================================================================

void testStressStability()
{
    std::cout << "\n=== Stress Stability Tests ===" << std::endl;

    BlackheartAudioProcessor processor;
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midiBuffer;
    auto& apvts = processor.getAPVTS();

    bool passed = true;
    int blocksProcessed = 0;

    // Process 1000 blocks with random parameter changes
    juce::Random random(12345);

    for (int block = 0; block < 1000; ++block)
    {
        // Random input (simulates various playing styles)
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < 256; ++i)
            {
                buffer.setSample(ch, i, random.nextFloat() * 2.0f - 1.0f);
            }
        }

        // Randomly change parameters every 10 blocks
        if (block % 10 == 0)
        {
            std::vector<const char*> params = { "gain", "glare", "blend", "level", "speed", "chaos", "rise" };
            const char* paramId = params[random.nextInt(static_cast<int>(params.size()))];
            if (auto* p = apvts.getParameter(paramId))
                p->setValueNotifyingHost(random.nextFloat());
        }

        // Randomly toggle octaves
        if (block % 25 == 0)
            processor.setOctave1(random.nextBool());
        if (block % 30 == 0)
            processor.setOctave2(random.nextBool());

        processor.processBlock(buffer, midiBuffer);
        blocksProcessed++;

        if (hasNaN(buffer))
        {
            passed = false;
            break;
        }

        // Check for runaway levels
        if (calculatePeak(buffer) > 20.0f)
        {
            passed = false;
            break;
        }
    }

    logTest("Stress test (1000 blocks)", passed,
            passed ? ("Processed " + std::to_string(blocksProcessed) + " blocks") :
                    ("Failed at block " + std::to_string(blocksProcessed)));

    // Check stability indicator
    logTest("Processor reports stable", processor.isStable());

    processor.setOctave1(false);
    processor.setOctave2(false);
    processor.releaseResources();
}

//==============================================================================
// Test 8: Input Signal Types
//==============================================================================

void testInputSignalTypes()
{
    std::cout << "\n=== Input Signal Type Tests ===" << std::endl;

    BlackheartAudioProcessor processor;
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midiBuffer;

    // Set moderate parameters
    auto& apvts = processor.getAPVTS();
    if (auto* p = apvts.getParameter("gain")) p->setValueNotifyingHost(0.5f);
    if (auto* p = apvts.getParameter("blend")) p->setValueNotifyingHost(0.7f);
    if (auto* p = apvts.getParameter("chaos")) p->setValueNotifyingHost(0.3f);

    // Test 1: Low frequency (Drop D tuning, low D)
    fillWithSineWave(buffer, 73.42f, 48000.0);
    processor.processBlock(buffer, midiBuffer);
    bool lowFreqPassed = !hasNaN(buffer) && calculatePeak(buffer) < 5.0f;
    logTest("Low frequency (73 Hz, Drop D)", lowFreqPassed);

    // Test 2: High gain chord simulation (multiple frequencies)
    buffer.clear();
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 256; ++i)
        {
            float t = i / 48000.0f;
            // Power chord: root + fifth
            float sample = std::sin(2.0f * juce::MathConstants<float>::pi * 82.41f * t) * 0.3f +
                          std::sin(2.0f * juce::MathConstants<float>::pi * 123.47f * t) * 0.3f;
            buffer.setSample(ch, i, sample);
        }
    }
    processor.processBlock(buffer, midiBuffer);
    bool chordPassed = !hasNaN(buffer) && calculatePeak(buffer) < 5.0f;
    logTest("Power chord (E + B)", chordPassed);

    // Test 3: High frequency
    fillWithSineWave(buffer, 4000.0f, 48000.0);
    processor.processBlock(buffer, midiBuffer);
    bool highFreqPassed = !hasNaN(buffer) && calculatePeak(buffer) < 5.0f;
    logTest("High frequency (4 kHz)", highFreqPassed);

    // Test 4: Impulse
    fillWithImpulse(buffer);
    processor.processBlock(buffer, midiBuffer);
    bool impulsePassed = !hasNaN(buffer) && calculatePeak(buffer) < 10.0f;
    logTest("Impulse response", impulsePassed);

    // Test 5: DC offset
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 256; ++i)
        {
            buffer.setSample(ch, i, 0.5f); // DC offset
        }
    }
    processor.processBlock(buffer, midiBuffer);
    // After DC blocking, output should be near zero
    bool dcPassed = !hasNaN(buffer) && calculateRMS(buffer) < 0.5f;
    logTest("DC blocking", dcPassed);

    processor.releaseResources();
}

//==============================================================================
// Main Test Runner
//==============================================================================

void runAllTests()
{
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║           BLACKHEART PLUGIN TEST SUITE                       ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;

    auto startTime = std::chrono::high_resolution_clock::now();

    testParameterStability();
    testOctaveButtonBehavior();
    testLatencyVerification();
    testSampleRateCompatibility();
    testBufferSizeCompatibility();
    testStatePersistence();
    testStressStability();
    testInputSignalTypes();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Summary
    int passed = 0, failed = 0;
    for (const auto& result : testResults)
    {
        if (result.passed) passed++;
        else failed++;
    }

    std::cout << "\n╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                      TEST SUMMARY                            ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "Total tests: " << testResults.size() << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Duration: " << duration.count() << " ms" << std::endl;
    std::cout << "Result: " << (failed == 0 ? "ALL TESTS PASSED ✓" : "SOME TESTS FAILED ✗") << std::endl;

    if (failed > 0)
    {
        std::cout << "\nFailed tests:" << std::endl;
        for (const auto& result : testResults)
        {
            if (!result.passed)
            {
                std::cout << "  - " << result.name;
                if (!result.details.empty())
                    std::cout << ": " << result.details;
                std::cout << std::endl;
            }
        }
    }
}

// Entry point when built as standalone test
#if JUCE_BUILD_STANDALONE_TEST
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    runAllTests();
    return 0;
}
#endif
