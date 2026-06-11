#include "FlatKnob.h"
#include "BlackheartPalette.h"
#include "BlackheartLookAndFeel.h"

namespace
{
    juce::Font makeNameFont()
    {
        auto f = BlackheartLookAndFeel::monoFont(13.0f);
        f.setExtraKerningFactor(0.12f);
        return f;
    }

    juce::Font makeValueFont()
    {
        return BlackheartLookAndFeel::monoFont(13.0f);
    }
}

FlatKnob::FlatKnob(const juce::String& name, const juce::String& suffix,
                     const juce::String& tooltip)
    : paramName(name), valueSuffix(suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled(false, false, nullptr);
    addAndMakeVisible(slider);

    nameLabel.setText(name.toUpperCase(), juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setFont(makeNameFont());
    nameLabel.setColour(juce::Label::textColourId, Blackheart::text());
    addAndMakeVisible(nameLabel);

    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setFont(makeValueFont());
    valueLabel.setColour(juce::Label::textColourId, Blackheart::dim());
    addAndMakeVisible(valueLabel);

    slider.onValueChange = [this]() { updateValueLabel(); };

    if (tooltip.isNotEmpty())
        setTooltip(tooltip);
}

void FlatKnob::resized()
{
    auto bounds = getLocalBounds();
    nameLabel.setBounds(bounds.removeFromTop(18));
    // Pull value readout up toward the knob arc; overlap is safe (transparent label).
    valueLabel.setBounds(bounds.removeFromBottom(17).translated(0, -8));
    slider.setBounds(bounds.withTrimmedBottom(-8).reduced(2));
}

void FlatKnob::updateValueLabel()
{
    const auto val = slider.getValue();
    juce::String text;

    if (valueSuffix == "%")
        text = juce::String(static_cast<int>(val * 100.0)) + "%";
    else if (valueSuffix == "ms")
        text = juce::String(static_cast<int>(val)) + "MS";
    else if (valueSuffix == "Hz")
        text = juce::String(val, 1) + "HZ";
    else
        text = juce::String(val, 2);

    valueLabel.setText(text, juce::dontSendNotification);
}
