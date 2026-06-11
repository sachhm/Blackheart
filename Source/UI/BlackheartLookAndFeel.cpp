#include "BlackheartLookAndFeel.h"
#include "BlackheartPalette.h"
#include "BinaryData.h"

// NDSP-style studio finish. Charcoal, dimensional knobs, one red accent.

BlackheartLookAndFeel::BlackheartLookAndFeel()
{
    using namespace Blackheart;

    setColour(juce::ResizableWindow::backgroundColourId,    bg());
    setColour(juce::Slider::backgroundColourId,             surface());
    setColour(juce::Slider::trackColourId,                  line());
    setColour(juce::Slider::thumbColourId,                  accent());
    setColour(juce::Slider::rotarySliderFillColourId,       accent());
    setColour(juce::Slider::rotarySliderOutlineColourId,    line());
    setColour(juce::Label::textColourId,                    dim());
    setColour(juce::TextButton::buttonColourId,             knob());
    setColour(juce::TextButton::buttonOnColourId,           accent());
    setColour(juce::TextButton::textColourOffId,            dim());
    setColour(juce::TextButton::textColourOnId,             text());
    setColour(juce::TooltipWindow::backgroundColourId,      surface());
    setColour(juce::TooltipWindow::textColourId,            text());
    setColour(juce::TooltipWindow::outlineColourId,         line());
}

void BlackheartLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
{
    juce::ignoreUnused(slider);

    const auto radius  = static_cast<float>(juce::jmin(width / 2, height / 2)) - 8.0f;
    const auto centreX = static_cast<float>(x) + static_cast<float>(width)  * 0.5f;
    const auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const auto angle   = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Drop shadow under the knob body.
    {
        juce::Path shadowDisc;
        shadowDisc.addEllipse(centreX - radius, centreY - radius + 2.0f, radius * 2.0f, radius * 2.0f);
        juce::DropShadow shadow (juce::Colours::black.withAlpha(0.55f), 10, { 0, 3 });
        shadow.drawForPath(g, shadowDisc);
    }

    // Machined body — subtle vertical sheen, darker at the base.
    {
        juce::ColourGradient grad (Blackheart::knob().brighter(0.18f), centreX, centreY - radius,
                                   Blackheart::knob().darker(0.35f),   centreX, centreY + radius, false);
        g.setGradientFill(grad);
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
    }

    // Bevel ring — light top edge, dark bottom edge.
    {
        juce::ColourGradient rim (Blackheart::textAt(0.20f), centreX, centreY - radius,
                                  juce::Colours::black.withAlpha(0.6f), centreX, centreY + radius, false);
        g.setGradientFill(rim);
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    }

    // Inner face — slightly recessed centre disc.
    {
        const float faceR = radius * 0.78f;
        juce::ColourGradient face (Blackheart::knob().darker(0.15f), centreX, centreY - faceR,
                                   Blackheart::knob().brighter(0.06f), centreX, centreY + faceR, false);
        g.setGradientFill(face);
        g.fillEllipse(centreX - faceR, centreY - faceR, faceR * 2.0f, faceR * 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawEllipse(centreX - faceR, centreY - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);
    }

    // Track arc — dark groove.
    const float arcRadius = radius + 5.0f;
    {
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                               0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(Blackheart::line());
        g.strokePath(trackArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // Accent value arc with soft glow.
    if (sliderPos > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                               0.0f, rotaryStartAngle, angle, true);
        g.setColour(Blackheart::accentAt(0.30f));
        g.strokePath(valueArc, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        g.setColour(Blackheart::accent());
        g.strokePath(valueArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // Pointer — bright white line, accent-lit at the tip.
    {
        const float p0 = radius * 0.42f;
        const float p1 = radius * 0.86f;
        const float sx = std::sin(angle), cy = std::cos(angle);
        g.setColour(Blackheart::text());
        g.drawLine(centreX + p0 * sx, centreY - p0 * cy,
                   centreX + p1 * sx, centreY - p1 * cy, 2.5f);
    }
}

void BlackheartLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool isMouseOverButton, bool isButtonDown)
{
    juce::ignoreUnused(backgroundColour);

    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool isOn = isButtonDown || button.getToggleState();
    const float corner = 4.0f;

    if (isOn)
    {
        g.setColour(Blackheart::accentAt(0.18f));
        g.fillRoundedRectangle(bounds.expanded(2.0f), corner + 2.0f);
        g.setColour(Blackheart::accent());
        g.fillRoundedRectangle(bounds, corner);
    }
    else
    {
        juce::ColourGradient grad (Blackheart::knob().brighter(0.10f), bounds.getX(), bounds.getY(),
                                   Blackheart::knob().darker(0.20f),   bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, corner);
    }

    g.setColour(isMouseOverButton && !isOn ? Blackheart::dimAt(0.8f) : Blackheart::line());
    g.drawRoundedRectangle(bounds, corner, 1.0f);
}

void BlackheartLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited())
    {
        const auto alpha = label.isEnabled() ? 1.0f : 0.5f;
        const auto font  = label.getFont();
        g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
        g.setFont(font);

        auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / font.getHeight())),
                         label.getMinimumHorizontalScale());
    }
}

//==============================================================================
// Font helpers — bundled OFL TTFs cached as static Typeface::Ptr.
//==============================================================================
namespace
{
    juce::Typeface::Ptr getDisplayTypeface()
    {
        static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::ArchivoBlackRegular_ttf,
            static_cast<size_t>(BinaryData::ArchivoBlackRegular_ttfSize));
        return tf;
    }

    juce::Typeface::Ptr getMonoTypeface()
    {
        static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::IBMPlexMonoRegular_ttf,
            static_cast<size_t>(BinaryData::IBMPlexMonoRegular_ttfSize));
        return tf;
    }
}

juce::Font BlackheartLookAndFeel::displayFont(float heightPx)
{
    juce::Font f { juce::FontOptions { getDisplayTypeface() } };
    f.setHeight(heightPx);
    return f;
}

juce::Font BlackheartLookAndFeel::monoFont(float heightPx)
{
    juce::Font f { juce::FontOptions { getMonoTypeface() } };
    f.setHeight(heightPx);
    return f;
}
