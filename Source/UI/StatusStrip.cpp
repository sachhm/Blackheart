#include "StatusStrip.h"
#include "BlackheartPalette.h"
#include "BlackheartLookAndFeel.h"

StatusStrip::StatusStrip()
{
    setInterceptsMouseClicks(false, false);
}

void StatusStrip::setCpuLoad(float v)
{
    if (std::abs(v - cpuLoad) > 0.005f)
    {
        cpuLoad = v;
        repaint();
    }
}

void StatusStrip::setLatencyMs(float ms)
{
    if (std::abs(ms - latencyMs) > 0.05f)
    {
        latencyMs = ms;
        repaint();
    }
}

void StatusStrip::setSampleRate(double sr)
{
    if (std::abs(sr - sampleRate) > 0.5)
    {
        sampleRate = sr;
        repaint();
    }
}

void StatusStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.fillAll(Blackheart::bg());
    g.setColour(Blackheart::line());
    g.drawHorizontalLine(0, 0.0f, bounds.getWidth());

    const float padX = 12.0f;
    auto inner = bounds.reduced(padX, 0.0f);

    const int cpuPct = juce::jlimit(0, 999, static_cast<int>(std::round(cpuLoad * 100.0f)));
    const juce::String left =
        "CPU " + juce::String(cpuPct) + "% \xC2\xB7 "
      + "LAT " + juce::String(latencyMs, 1) + "MS \xC2\xB7 "
      + juce::String(static_cast<int>(std::round(sampleRate))) + "HZ";

    g.setColour(Blackheart::dim());
    g.setFont(BlackheartLookAndFeel::monoFont(12.0f));
    g.drawText(left, inner, juce::Justification::centredLeft, false);
    g.drawText(juce::String::fromUTF8(kBrand), inner, juce::Justification::centredRight, false);
}
