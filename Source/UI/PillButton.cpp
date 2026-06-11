#include "PillButton.h"
#include "BlackheartPalette.h"
#include "BlackheartLookAndFeel.h"

PillButton::PillButton(const juce::String& name) : juce::TextButton(name)
{
    setClickingTogglesState(false);
}

void PillButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const bool isActive = getToggleState() || isDown();

    auto labelArea  = bounds.removeFromBottom(18.0f);
    auto switchArea = bounds;

    const float d       = juce::jmin(switchArea.getWidth(), switchArea.getHeight()) - 4.0f;
    const float radius  = d * 0.5f;
    const float centreX = switchArea.getCentreX();
    const float centreY = switchArea.getCentreY();

    // Drop shadow under the switch.
    {
        juce::Path disc;
        disc.addEllipse(centreX - radius, centreY - radius + 2.0f, d, d);
        juce::DropShadow shadow (juce::Colours::black.withAlpha(0.6f), 10, { 0, 3 });
        shadow.drawForPath(g, disc);
    }

    // Active halo — pedal LED glow around the switch.
    if (isActive)
    {
        g.setColour(Blackheart::accentAt(0.25f));
        g.fillEllipse(centreX - radius - 4.0f, centreY - radius - 4.0f, d + 8.0f, d + 8.0f);
    }

    // Outer bezel — the hex-nut collar of a footswitch.
    {
        juce::ColourGradient bezel (Blackheart::line().brighter(0.35f), centreX, centreY - radius,
                                    juce::Colours::black, centreX, centreY + radius, false);
        g.setGradientFill(bezel);
        g.fillEllipse(centreX - radius, centreY - radius, d, d);
    }

    // Cap — raised machined button, smaller than the bezel.
    {
        const float capR = radius * 0.80f;
        if (isActive)
        {
            juce::ColourGradient cap (Blackheart::accent().brighter(0.20f), centreX, centreY - capR,
                                      Blackheart::accent().darker(0.35f),   centreX, centreY + capR, false);
            g.setGradientFill(cap);
        }
        else
        {
            juce::ColourGradient cap (Blackheart::knob().brighter(0.25f), centreX, centreY - capR,
                                      Blackheart::knob().darker(0.40f),   centreX, centreY + capR, false);
            g.setGradientFill(cap);
        }
        g.fillEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

        // Cap rim — light top, dark bottom.
        juce::ColourGradient rim (Blackheart::textAt(isMouseOver() && !isActive ? 0.40f : 0.25f),
                                  centreX, centreY - capR,
                                  juce::Colours::black.withAlpha(0.7f), centreX, centreY + capR, false);
        g.setGradientFill(rim);
        g.drawEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f, 1.5f);

        // Specular highlight on the upper face of the cap.
        const float hiR = capR * 0.55f;
        juce::ColourGradient hi (juce::Colours::white.withAlpha(isActive ? 0.18f : 0.10f),
                                 centreX, centreY - capR,
                                 juce::Colours::transparentWhite, centreX, centreY, false);
        g.setGradientFill(hi);
        g.fillEllipse(centreX - hiR, centreY - capR * 0.9f, hiR * 2.0f, capR * 0.9f);
    }

    // Label beneath the switch.
    auto main = BlackheartLookAndFeel::monoFont(13.0f);
    main.setExtraKerningFactor(0.10f);
    g.setColour(isActive ? Blackheart::text() : Blackheart::dim());
    g.setFont(main);
    g.drawText(getButtonText().toUpperCase(), labelArea, juce::Justification::centred);
}

void PillButton::mouseDown(const juce::MouseEvent& e)
{
    juce::TextButton::mouseDown(e);
    if (onStateChange)
        onStateChange(true);
}

void PillButton::mouseUp(const juce::MouseEvent& e)
{
    juce::TextButton::mouseUp(e);
    if (onStateChange)
        onStateChange(false);
}
