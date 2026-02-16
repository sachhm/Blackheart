#pragma once

#include <JuceHeader.h>

namespace ParameterIDs
{

inline constexpr auto gain     { "gain" };
inline constexpr auto glare    { "glare" };
inline constexpr auto blend    { "blend" };
inline constexpr auto level    { "level" };
inline constexpr auto speed    { "speed" };
inline constexpr auto chaos    { "chaos" };
inline constexpr auto rise     { "rise" };
inline constexpr auto octave1  { "octave1" };
inline constexpr auto octave2  { "octave2" };
inline constexpr auto mode     { "mode" };
inline constexpr auto shape    { "shape" };
inline constexpr auto panic    { "panic" };

namespace Defaults
{
    inline constexpr float gain    = 0.5f;
    inline constexpr float glare   = 0.3f;
    inline constexpr float blend   = 0.7f;
    inline constexpr float level   = 0.7f;
    inline constexpr float speed   = 0.3f;
    inline constexpr float chaos   = 0.5f;
    inline constexpr float rise    = 50.0f;
    inline constexpr bool  octave1 = false;
    inline constexpr bool  octave2 = false;
    inline constexpr float mode    = 1.0f;   // 0=Up(Screaming), 1=Center(Overdrive), 2=Down(Doom)
    inline constexpr float shape   = 0.5f;
    inline constexpr float panic   = 0.0f;
}

namespace Ranges
{
    inline constexpr float gainMin   = 0.0f;
    inline constexpr float gainMax   = 1.0f;
    inline constexpr float gainStep  = 0.01f;
    inline constexpr float gainSkew  = 0.7f;  // Skewed for more resolution at lower gain (where subtlety matters)

    inline constexpr float glareMin  = 0.0f;
    inline constexpr float glareMax  = 1.0f;
    inline constexpr float glareStep = 0.01f;
    inline constexpr float glareSkew = 1.0f;

    inline constexpr float blendMin  = 0.0f;
    inline constexpr float blendMax  = 1.0f;
    inline constexpr float blendStep = 0.01f;
    inline constexpr float blendSkew = 1.0f;

    inline constexpr float levelMin  = 0.0f;
    inline constexpr float levelMax  = 1.0f;
    inline constexpr float levelStep = 0.01f;
    inline constexpr float levelSkew = 1.0f;

    inline constexpr float speedMin  = 0.0f;
    inline constexpr float speedMax  = 1.0f;
    inline constexpr float speedStep = 0.01f;
    inline constexpr float speedSkew = 1.0f;

    inline constexpr float chaosMin  = 0.0f;
    inline constexpr float chaosMax  = 1.0f;
    inline constexpr float chaosStep = 0.01f;
    inline constexpr float chaosSkew = 0.8f;  // Slight skew for more control in subtle chaos range

    inline constexpr float riseMin   = 1.0f;
    inline constexpr float riseMax   = 500.0f;
    inline constexpr float riseStep  = 1.0f;
    inline constexpr float riseSkew  = 0.35f;

    inline constexpr float modeMin   = 0.0f;
    inline constexpr float modeMax   = 2.0f;
    inline constexpr float modeStep  = 1.0f;
    inline constexpr float modeSkew  = 1.0f;

    inline constexpr float shapeMin  = 0.0f;
    inline constexpr float shapeMax  = 1.0f;
    inline constexpr float shapeStep = 0.01f;
    inline constexpr float shapeSkew = 1.0f;

    inline constexpr float panicMin  = 0.0f;
    inline constexpr float panicMax  = 1.0f;
    inline constexpr float panicStep = 0.01f;
    inline constexpr float panicSkew = 1.0f;
}

namespace Smoothing
{
    inline constexpr double gainRampSec   = 0.015;  // Faster for more responsive distortion
    inline constexpr double glareRampSec  = 0.015;  // Match gain for consistent feel
    inline constexpr double blendRampSec  = 0.03;   // Faster blend changes
    inline constexpr double levelRampSec  = 0.02;   // Keep smooth for output
    inline constexpr double speedRampSec  = 0.04;   // Slightly faster modulation response
    inline constexpr double chaosRampSec  = 0.03;   // More responsive chaos
    inline constexpr double riseRampSec   = 0.01;   // Keep fast for pitch transitions
    inline constexpr double octaveRampSec = 0.008;  // Slightly slower for smoother octave transitions
    inline constexpr double shapeRampSec  = 0.02;   // Smooth shape transitions
    inline constexpr double panicRampSec  = 0.02;
    // MODE has no smoothing — discrete switch, instant change
}

namespace Labels
{
    inline const juce::String gain    { "Gain" };
    inline const juce::String glare   { "Glare" };
    inline const juce::String blend   { "Blend" };
    inline const juce::String level   { "Level" };
    inline const juce::String speed   { "Speed" };
    inline const juce::String chaos   { "Chaos" };
    inline const juce::String rise    { "Rise" };
    inline const juce::String octave1 { "Octave +1" };
    inline const juce::String octave2 { "Octave +2" };
    inline const juce::String mode    { "Mode" };
    inline const juce::String shape   { "Shape" };
    inline const juce::String panic   { "Panic" };
}

namespace Units
{
    inline const juce::String percent { "%" };
    inline const juce::String hz      { "Hz" };
    inline const juce::String ms      { "ms" };
    inline const juce::String none    { "" };
}

inline juce::NormalisableRange<float> makeRange(float min, float max, float step, float skew)
{
    return juce::NormalisableRange<float>(min, max, step, skew);
}

inline juce::NormalisableRange<float> gainRange()
{
    return makeRange(Ranges::gainMin, Ranges::gainMax, Ranges::gainStep, Ranges::gainSkew);
}

inline juce::NormalisableRange<float> glareRange()
{
    return makeRange(Ranges::glareMin, Ranges::glareMax, Ranges::glareStep, Ranges::glareSkew);
}

inline juce::NormalisableRange<float> blendRange()
{
    return makeRange(Ranges::blendMin, Ranges::blendMax, Ranges::blendStep, Ranges::blendSkew);
}

inline juce::NormalisableRange<float> levelRange()
{
    return makeRange(Ranges::levelMin, Ranges::levelMax, Ranges::levelStep, Ranges::levelSkew);
}

inline juce::NormalisableRange<float> speedRange()
{
    return makeRange(Ranges::speedMin, Ranges::speedMax, Ranges::speedStep, Ranges::speedSkew);
}

inline juce::NormalisableRange<float> chaosRange()
{
    return makeRange(Ranges::chaosMin, Ranges::chaosMax, Ranges::chaosStep, Ranges::chaosSkew);
}

inline juce::NormalisableRange<float> riseRange()
{
    return makeRange(Ranges::riseMin, Ranges::riseMax, Ranges::riseStep, Ranges::riseSkew);
}

inline juce::NormalisableRange<float> modeRange()
{
    return makeRange(Ranges::modeMin, Ranges::modeMax, Ranges::modeStep, Ranges::modeSkew);
}

inline juce::NormalisableRange<float> shapeRange()
{
    return makeRange(Ranges::shapeMin, Ranges::shapeMax, Ranges::shapeStep, Ranges::shapeSkew);
}

inline juce::NormalisableRange<float> panicRange()
{
    return makeRange(Ranges::panicMin, Ranges::panicMax, Ranges::panicStep, Ranges::panicSkew);
}

} // namespace ParameterIDs
