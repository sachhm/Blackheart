#include "Scope.h"
#include "BlackheartPalette.h"
#include "BlackheartLookAndFeel.h"

// Phosphor bloom = stacked strokes with alpha falloff. No shaders, no
// scanlines, no curvature. Glow is the only effect on this face.

Scope::Scope()
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(30);
    for (auto& v : displayBuffer)
        v = 0.0f;
}

void Scope::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(Blackheart::bg());
    g.fillRect(bounds);

    auto inner = bounds.reduced(1.0f);

    // Faint centre reference line.
    g.setColour(Blackheart::lineAt(0.8f));
    g.fillRect(juce::Rectangle<float>(inner.getX(), inner.getCentreY() - 0.5f, inner.getWidth(), 1.0f));

    // Waveform path.
    const float centreY   = inner.getCentreY();
    const float amplitude = inner.getHeight() * 0.42f;
    const float xStep     = inner.getWidth() / static_cast<float>(displayBufferSize - 1);

    juce::Path waveform;
    waveform.preallocateSpace(displayBufferSize * 3);

    for (int i = 0; i < displayBufferSize; ++i)
    {
        const float x = inner.getX() + static_cast<float>(i) * xStep;
        const float y = centreY - displayBuffer[static_cast<size_t>(i)] * amplitude;
        if (i == 0)
            waveform.startNewSubPath(x, y);
        else
            waveform.lineTo(x, y);
    }

    // Phosphor bloom: wide faint → narrow bright.
    g.setColour(Blackheart::accentAt(0.10f));
    g.strokePath(waveform, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved));
    g.setColour(Blackheart::accentAt(0.28f));
    g.strokePath(waveform, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved));
    g.setColour(Blackheart::accent());
    g.strokePath(waveform, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));

    // --- OSD chrome, burned into corners ---
    const float pad = 10.0f;
    auto osd = inner.reduced(pad);
    auto osdFont = BlackheartLookAndFeel::monoFont(13.0f);
    osdFont.setExtraKerningFactor(0.08f);
    g.setFont(osdFont);

    g.setColour(Blackheart::dim());
    g.drawText(osdChannel, osd, juce::Justification::topLeft, false);

    if (osdMomentary.isNotEmpty())
    {
        g.setColour(Blackheart::accent());
        g.drawText(osdMomentary, osd, juce::Justification::bottomLeft, false);
    }

    g.setColour(Blackheart::line());
    g.drawRect(bounds, 1.0f);
}

void Scope::pushSamples(const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        displayBuffer[static_cast<size_t>(writeIndex)] = samples[i];
        writeIndex = (writeIndex + 1) % displayBufferSize;
    }
}

void Scope::timerCallback()
{
    repaint();
}

void Scope::setOsdChannel(const juce::String& text)
{
    if (text != osdChannel) { osdChannel = text; }
}

void Scope::setOsdMomentary(const juce::String& text)
{
    if (text != osdMomentary) { osdMomentary = text; }
}
