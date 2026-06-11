#include "HeaderStrip.h"
#include "BlackheartPalette.h"
#include "BlackheartLookAndFeel.h"

HeaderStrip::HeaderStrip()
{
    setInterceptsMouseClicks(false, false);
}

void HeaderStrip::setSampleRate(double sr)
{
    if (std::abs(sr - sampleRate) > 0.5)
    {
        sampleRate = sr;
        repaint();
    }
}

void HeaderStrip::setLatencyMs(float ms)
{
    if (std::abs(ms - latencyMs) > 0.05f)
    {
        latencyMs = ms;
        repaint();
    }
}

void HeaderStrip::setActivity(float level01)
{
    const float v = juce::jlimit(0.0f, 1.0f, level01);
    if (std::abs(v - activity) > 0.02f)
    {
        activity = v;
        repaint();
    }
}

void HeaderStrip::setLevels(float in01, float out01)
{
    const float in  = juce::jlimit(0.0f, 1.0f, in01);
    const float out = juce::jlimit(0.0f, 1.0f, out01);
    smoothedIn  += (in  > smoothedIn  ? 0.5f : 0.15f) * (in  - smoothedIn);
    smoothedOut += (out > smoothedOut ? 0.5f : 0.15f) * (out - smoothedOut);
    repaint();
}

void HeaderStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.fillAll(Blackheart::bg());
    g.setColour(Blackheart::line());
    g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

    const float padX = 12.0f;
    auto inner = bounds.reduced(padX, 0.0f);

    // Wordmark — IBM Plex Mono, wide-tracked. Matches the instrument readouts.
    g.setColour(Blackheart::text());
    auto title = BlackheartLookAndFeel::monoFont(16.0f);
    title.setExtraKerningFactor(0.42f);
    g.setFont(title);
    g.drawText("BLACKHEART", inner, juce::Justification::centredLeft, false);

    // Right: REC ● + rate·latency. Mono, dim; dot glows amber with signal.
    const double srKhz = sampleRate / 1000.0;
    juce::String srStr;
    if (std::abs(srKhz - std::round(srKhz)) < 0.05)
        srStr = juce::String(static_cast<int>(std::round(srKhz))) + "K";
    else
        srStr = juce::String(srKhz, 1) + "K";

    const juce::String readout = srStr + " \xC2\xB7 " + juce::String(latencyMs, 1) + "MS";

    auto mono = BlackheartLookAndFeel::monoFont(11.0f);
    g.setFont(mono);
    g.setColour(Blackheart::dim());
    g.drawText(readout, inner, juce::Justification::centredRight, false);

    // REC label + dot to the left of the readout.
    const float readoutW = juce::GlyphArrangement::getStringWidth(mono, readout);
    const float dotD = 7.0f;
    const float dotX = inner.getRight() - readoutW - 16.0f - dotD;
    const float dotY = inner.getCentreY() - dotD * 0.5f;

    g.setColour(Blackheart::dim());
    g.drawText("REC", juce::Rectangle<float>(dotX - 36.0f, inner.getY(), 32.0f, inner.getHeight()),
               juce::Justification::centredRight, false);

    const float glow = 0.25f + 0.75f * activity;
    g.setColour(Blackheart::accentAt(glow * 0.3f));
    g.fillEllipse(dotX - 2.0f, dotY - 2.0f, dotD + 4.0f, dotD + 4.0f);
    g.setColour(Blackheart::accentAt(glow));
    g.fillEllipse(dotX, dotY, dotD, dotD);

    // IN / OUT segment meters — left of the REC group.
    {
        const float meterW = 6.0f, meterH = 18.0f, gap = 16.0f, labelH = 10.0f;
        const float groupRight = dotX - 36.0f - 24.0f;
        const float top = inner.getCentreY() - (meterH + labelH + 2.0f) * 0.5f;

        juce::Rectangle<float> outR (groupRight - meterW, top, meterW, meterH);
        juce::Rectangle<float> inR  (groupRight - meterW * 2.0f - gap, top, meterW, meterH);

        drawSegmentMeter(g, inR,  smoothedIn);
        drawSegmentMeter(g, outR, smoothedOut);

        g.setColour(Blackheart::dim());
        g.setFont(BlackheartLookAndFeel::monoFont(9.0f));
        g.drawText("IN",  inR.withY(inR.getBottom() + 2.0f).withHeight(labelH).expanded(9.0f, 0.0f),
                   juce::Justification::centredTop, false);
        g.drawText("OUT", outR.withY(outR.getBottom() + 2.0f).withHeight(labelH).expanded(9.0f, 0.0f),
                   juce::Justification::centredTop, false);
    }
}

void HeaderStrip::drawSegmentMeter(juce::Graphics& g, juce::Rectangle<float> r, float level01) const
{
    constexpr int segments = 6;
    const float segGap = 1.5f;
    const float segH = (r.getHeight() - segGap * (segments - 1)) / segments;
    const int lit = juce::jlimit(0, segments, static_cast<int>(std::ceil(level01 * segments)));

    for (int i = 0; i < segments; ++i)
    {
        const float y = r.getBottom() - segH - static_cast<float>(i) * (segH + segGap);
        g.setColour(i < lit ? Blackheart::accent() : Blackheart::lineAt(0.7f));
        g.fillRect(juce::Rectangle<float>(r.getX(), y, r.getWidth(), segH));
    }
}
