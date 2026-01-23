#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace DSP
{

/**
 * High-performance lookup tables for common DSP functions.
 * All tables are thread-safe (read-only after initialization).
 * Uses linear interpolation for smooth output.
 */
class LookupTables
{
public:
    static constexpr int tableSize = 4096;
    static constexpr int tableMask = tableSize - 1;

    // Initialize all tables - call once at startup
    static void initialize()
    {
        static bool initialized = false;
        if (initialized) return;

        // Sine table: full period [0, 2π)
        for (int i = 0; i < tableSize; ++i)
        {
            const float phase = static_cast<float>(i) / static_cast<float>(tableSize);
            sineTable[i] = std::sin(phase * juce::MathConstants<float>::twoPi);
        }

        // Cosine table: full period [0, 2π)
        for (int i = 0; i < tableSize; ++i)
        {
            const float phase = static_cast<float>(i) / static_cast<float>(tableSize);
            cosineTable[i] = std::cos(phase * juce::MathConstants<float>::twoPi);
        }

        // Tanh table: range [-4, 4] mapped to [0, tableSize)
        for (int i = 0; i < tableSize; ++i)
        {
            const float x = (static_cast<float>(i) / static_cast<float>(tableSize - 1)) * 8.0f - 4.0f;
            tanhTable[i] = std::tanh(x);
        }

        // Exp table for envelope followers: range [-8, 0] mapped to exp(x)
        for (int i = 0; i < tableSize; ++i)
        {
            const float x = (static_cast<float>(i) / static_cast<float>(tableSize - 1)) * -8.0f;
            expDecayTable[i] = std::exp(x);
        }

        // Hann window table: one full window [0, 1]
        for (int i = 0; i < tableSize; ++i)
        {
            const float phase = static_cast<float>(i) / static_cast<float>(tableSize);
            hannTable[i] = 0.5f * (1.0f - std::cos(phase * juce::MathConstants<float>::twoPi));
        }

        // Soft clip table: x - x^3/3 for [-1, 1], asymptotic beyond
        for (int i = 0; i < tableSize; ++i)
        {
            const float x = (static_cast<float>(i) / static_cast<float>(tableSize - 1)) * 4.0f - 2.0f;
            if (x >= -1.0f && x <= 1.0f)
            {
                const float x2 = x * x;
                softClipTable[i] = x - (x * x2) / 3.0f;
            }
            else if (x > 1.0f)
            {
                softClipTable[i] = 1.0f - std::exp(-x + 1.0f) * (1.0f - 2.0f/3.0f);
            }
            else
            {
                softClipTable[i] = -1.0f + std::exp(x + 1.0f) * (1.0f - 2.0f/3.0f);
            }
        }

        initialized = true;
    }

    // Fast sine lookup with linear interpolation
    // Input: normalized phase [0, 1)
    static float fastSin(float phase) noexcept
    {
        phase = phase - std::floor(phase); // Wrap to [0, 1)
        const float index = phase * static_cast<float>(tableSize);
        const int idx0 = static_cast<int>(index) & tableMask;
        const int idx1 = (idx0 + 1) & tableMask;
        const float frac = index - std::floor(index);
        return sineTable[idx0] + frac * (sineTable[idx1] - sineTable[idx0]);
    }

    // Fast cosine lookup with linear interpolation
    // Input: normalized phase [0, 1)
    static float fastCos(float phase) noexcept
    {
        phase = phase - std::floor(phase);
        const float index = phase * static_cast<float>(tableSize);
        const int idx0 = static_cast<int>(index) & tableMask;
        const int idx1 = (idx0 + 1) & tableMask;
        const float frac = index - std::floor(index);
        return cosineTable[idx0] + frac * (cosineTable[idx1] - cosineTable[idx0]);
    }

    // Fast tanh approximation
    // Input: any float (clamped to [-4, 4] range)
    static float fastTanh(float x) noexcept
    {
        // Clamp to table range
        x = juce::jlimit(-4.0f, 4.0f, x);
        // Map [-4, 4] to [0, tableSize-1]
        const float index = (x + 4.0f) * (static_cast<float>(tableSize - 1) / 8.0f);
        const int idx0 = static_cast<int>(index);
        const int idx1 = std::min(idx0 + 1, tableSize - 1);
        const float frac = index - static_cast<float>(idx0);
        return tanhTable[idx0] + frac * (tanhTable[idx1] - tanhTable[idx0]);
    }

    // Very fast tanh for values likely in [-2, 2] range
    // Uses polynomial approximation for speed
    static float fastTanhPoly(float x) noexcept
    {
        // Clamp to prevent overflow
        x = juce::jlimit(-3.0f, 3.0f, x);
        const float x2 = x * x;
        // Padé approximant: tanh(x) ≈ x(27 + x²) / (27 + 9x²)
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // Fast exp for decay envelopes (only valid for negative inputs)
    // Input: value in range [-8, 0]
    static float fastExpDecay(float x) noexcept
    {
        x = juce::jlimit(-8.0f, 0.0f, x);
        const float index = (x + 8.0f) * (static_cast<float>(tableSize - 1) / 8.0f);
        const int idx0 = static_cast<int>(index);
        const int idx1 = std::min(idx0 + 1, tableSize - 1);
        const float frac = index - static_cast<float>(idx0);
        return expDecayTable[idx0] + frac * (expDecayTable[idx1] - expDecayTable[idx0]);
    }

    // Fast Hann window lookup
    // Input: phase [0, 1]
    static float fastHann(float phase) noexcept
    {
        phase = juce::jlimit(0.0f, 1.0f, phase);
        const float index = phase * static_cast<float>(tableSize - 1);
        const int idx0 = static_cast<int>(index);
        const int idx1 = std::min(idx0 + 1, tableSize - 1);
        const float frac = index - static_cast<float>(idx0);
        return hannTable[idx0] + frac * (hannTable[idx1] - hannTable[idx0]);
    }

    // Fast soft clip lookup
    // Input: value in range [-2, 2]
    static float fastSoftClip(float x) noexcept
    {
        x = juce::jlimit(-2.0f, 2.0f, x);
        const float index = (x + 2.0f) * (static_cast<float>(tableSize - 1) / 4.0f);
        const int idx0 = static_cast<int>(index);
        const int idx1 = std::min(idx0 + 1, tableSize - 1);
        const float frac = index - static_cast<float>(idx0);
        return softClipTable[idx0] + frac * (softClipTable[idx1] - softClipTable[idx0]);
    }

    // Calculate equal-power crossfade gains
    // Input: blend [0, 1], outputs dry and wet gains
    static void equalPowerGains(float blend, float& dryGain, float& wetGain) noexcept
    {
        // blend=0 -> dry=1, wet=0
        // blend=1 -> dry=0, wet=1
        // Using quarter-period sine/cosine
        const float phase = blend * 0.25f; // Map [0,1] to [0, π/2] normalized
        dryGain = fastCos(phase);
        wetGain = fastSin(phase);
    }

private:
    static inline std::array<float, tableSize> sineTable;
    static inline std::array<float, tableSize> cosineTable;
    static inline std::array<float, tableSize> tanhTable;
    static inline std::array<float, tableSize> expDecayTable;
    static inline std::array<float, tableSize> hannTable;
    static inline std::array<float, tableSize> softClipTable;
};

} // namespace DSP
