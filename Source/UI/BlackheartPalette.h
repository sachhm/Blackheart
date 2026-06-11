#pragma once

#include <JuceHeader.h>

// Blackheart NDSP-style palette. Charcoal studio finish, one red accent.
// Gradients allowed but only subtle vertical/radial shading on controls.
namespace Blackheart
{
    namespace Palette
    {
        constexpr juce::uint32 kBg      = 0xff121316; // Window background
        constexpr juce::uint32 kSurface = 0xff1B1C20; // Section panels, scope bezel
        constexpr juce::uint32 kKnob    = 0xff26282D; // Knob faces, button bodies
        constexpr juce::uint32 kLine    = 0xff32343A; // Hairlines, borders, ticks
        constexpr juce::uint32 kText    = 0xffE8E9EB; // Primary text, knob pointers
        constexpr juce::uint32 kDim     = 0xff75787F; // Secondary text, idle labels
        constexpr juce::uint32 kAccent  = 0xffD63A32; // Blood red — live, active, value
    }

    inline juce::Colour bg()      { return juce::Colour (Palette::kBg); }
    inline juce::Colour surface() { return juce::Colour (Palette::kSurface); }
    inline juce::Colour knob()    { return juce::Colour (Palette::kKnob); }
    inline juce::Colour line()    { return juce::Colour (Palette::kLine); }
    inline juce::Colour text()    { return juce::Colour (Palette::kText); }
    inline juce::Colour dim()     { return juce::Colour (Palette::kDim); }
    inline juce::Colour accent()  { return juce::Colour (Palette::kAccent); }

    inline juce::Colour accentAt (float alpha) { return accent().withAlpha (alpha); }
    inline juce::Colour textAt   (float alpha) { return text().withAlpha   (alpha); }
    inline juce::Colour dimAt    (float alpha) { return dim().withAlpha    (alpha); }
    inline juce::Colour lineAt   (float alpha) { return line().withAlpha   (alpha); }
}
