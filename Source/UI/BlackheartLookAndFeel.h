#pragma once

#include <JuceHeader.h>

class BlackheartLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BlackheartLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    // Bundled fonts. Display = Archivo Black (heavy condensed display).
    // Mono = IBM Plex Mono (technical readouts).
    static juce::Font displayFont(float heightPx);
    static juce::Font monoFont   (float heightPx);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackheartLookAndFeel)
};
