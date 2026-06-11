#pragma once

#include <JuceHeader.h>

// FlatKnob (file name FlatKnob.* preserved until step 10 cleanup).
// Wraps a rotary slider with two labels:
//   - upper:  uppercase tracked display label (knob NAME)
//   - lower:  monospaced value readout
// Knob body, track, arc, and pointer are drawn by BlackheartLookAndFeel.
class FlatKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatKnob(const juce::String& name, const juce::String& suffix = "",
              const juce::String& tooltip = "");

    void resized() override;
    juce::Slider& getSlider() { return slider; }
    void updateValueLabel();

private:
    juce::Slider slider;
    juce::Label nameLabel;
    juce::Label valueLabel;
    juce::String paramName;
    juce::String valueSuffix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlatKnob)
};
