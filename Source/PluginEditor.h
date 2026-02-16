#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Parameters/ParameterIDs.h"

//==============================================================================
// Custom dark look and feel for Blackheart
class BlackheartLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BlackheartLookAndFeel()
    {
        // Dark color scheme with red accents
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff121212));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1e1e1e));
        setColour(juce::Slider::trackColourId, juce::Colour(0xffcc3333));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xffdddddd));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffcc3333));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::Label::textColourId, juce::Colour(0xffb0b0b0));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e1e1e));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffcc3333));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xff888888));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));
        setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::TooltipWindow::textColourId, juce::Colour(0xffcccccc));
        setColour(juce::TooltipWindow::outlineColourId, juce::Colour(0xff444444));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        const auto radius = static_cast<float>(juce::jmin(width / 2, height / 2)) - 4.0f;
        const auto centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const auto rx = centreX - radius;
        const auto ry = centreY - radius;
        const auto rw = radius * 2.0f;
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Outer glow when hovering
        if (slider.isMouseOver())
        {
            g.setColour(juce::Colour(0x20cc3333));
            g.fillEllipse(rx - 3.0f, ry - 3.0f, rw + 6.0f, rw + 6.0f);
        }

        // Outer ring (dark background)
        g.setColour(juce::Colour(0xff1e1e1e));
        g.fillEllipse(rx, ry, rw, rw);

        // Inner dark circle
        g.setColour(juce::Colour(0xff0a0a0a));
        g.fillEllipse(rx + 3.0f, ry + 3.0f, rw - 6.0f, rw - 6.0f);

        // Background arc (track)
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, radius - 2.0f, radius - 2.0f,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff2a2a2a));
        g.strokePath(bgArc, juce::PathStrokeType(3.0f));

        // Value arc (red glow)
        juce::Path arcPath;
        arcPath.addCentredArc(centreX, centreY, radius - 2.0f, radius - 2.0f,
                              0.0f, rotaryStartAngle, angle, true);

        // Glow effect
        g.setColour(juce::Colour(0x40cc3333));
        g.strokePath(arcPath, juce::PathStrokeType(5.0f));
        g.setColour(juce::Colour(0xffcc3333));
        g.strokePath(arcPath, juce::PathStrokeType(2.5f));

        // Pointer line
        juce::Path pointer;
        const auto pointerLength = radius * 0.55f;
        const auto pointerThickness = 2.5f;
        pointer.addRectangle(-pointerThickness * 0.5f, -radius + 8.0f, pointerThickness, pointerLength);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

        g.setColour(juce::Colour(0xffcccccc));
        g.fillPath(pointer);

        // Center cap
        g.setColour(juce::Colour(0xff2a2a2a));
        g.fillEllipse(centreX - 5.0f, centreY - 5.0f, 10.0f, 10.0f);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool isOn = isButtonDown || button.getToggleState();

        // Glow effect when active
        if (isOn)
        {
            g.setColour(juce::Colour(0x30cc3333));
            g.fillRoundedRectangle(bounds.expanded(3.0f), 8.0f);
        }

        auto baseColour = isOn ? juce::Colour(0xffcc3333) : backgroundColour;
        if (isMouseOverButton && !isOn)
            baseColour = baseColour.brighter(0.15f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 6.0f);

        // Border
        g.setColour(isOn ? juce::Colour(0xffee5555) : juce::Colour(0xff3a3a3a));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }
};

//==============================================================================
// LED Indicator component
class LEDIndicator : public juce::Component, public juce::Timer
{
public:
    LEDIndicator(juce::Colour onColour = juce::Colour(0xffcc3333))
        : ledOnColour(onColour)
    {
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        const float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto ledBounds = juce::Rectangle<float>(size, size)
                            .withCentre(bounds.getCentre());

        // Outer glow when on
        if (smoothedBrightness > 0.1f)
        {
            g.setColour(ledOnColour.withAlpha(smoothedBrightness * 0.4f));
            g.fillEllipse(ledBounds.expanded(4.0f * smoothedBrightness));
        }

        // LED body
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillEllipse(ledBounds);

        // LED light
        const auto litColour = ledOnColour.interpolatedWith(juce::Colour(0xff331111), 1.0f - smoothedBrightness);
        g.setColour(litColour);
        g.fillEllipse(ledBounds.reduced(2.0f));

        // Highlight
        if (smoothedBrightness > 0.5f)
        {
            g.setColour(juce::Colours::white.withAlpha(smoothedBrightness * 0.3f));
            g.fillEllipse(ledBounds.reduced(4.0f).withTrimmedBottom(ledBounds.getHeight() * 0.5f));
        }
    }

    void setOn(bool shouldBeOn) { targetBrightness = shouldBeOn ? 1.0f : 0.0f; }
    void setBrightness(float b) { targetBrightness = juce::jlimit(0.0f, 1.0f, b); }

    void timerCallback() override
    {
        const float smoothing = 0.2f;
        smoothedBrightness += smoothing * (targetBrightness - smoothedBrightness);
        repaint();
    }

private:
    juce::Colour ledOnColour;
    float targetBrightness = 0.0f;
    float smoothedBrightness = 0.0f;
};

//==============================================================================
// Vertical Level Meter
class LevelMeter : public juce::Component, public juce::Timer
{
public:
    LevelMeter()
    {
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);

        // Background
        g.setColour(juce::Colour(0xff0a0a0a));
        g.fillRoundedRectangle(bounds, 3.0f);

        // Level bar
        const float meterHeight = bounds.getHeight() * smoothedLevel;
        auto levelBounds = bounds.removeFromBottom(meterHeight).reduced(2.0f, 1.0f);

        // Gradient from green to red
        juce::ColourGradient gradient(
            juce::Colour(0xff33cc33), levelBounds.getBottomLeft(),
            juce::Colour(0xffcc3333), levelBounds.getTopLeft(), false);
        gradient.addColour(0.6, juce::Colour(0xffcccc33));
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(levelBounds, 2.0f);

        // Peak indicator
        if (peakLevel > 0.01f)
        {
            const float peakY = bounds.getBottom() - bounds.getHeight() * smoothedPeak;
            g.setColour(juce::Colour(0xffeeeeee));
            g.fillRect(bounds.getX() + 2.0f, peakY - 1.0f, bounds.getWidth() - 4.0f, 2.0f);
        }

        // Clip indicator
        if (isClipping)
        {
            g.setColour(juce::Colour(0xffff0000));
            g.fillRoundedRectangle(bounds.removeFromTop(6.0f).reduced(2.0f, 1.0f), 2.0f);
        }

        // Border
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 3.0f, 1.0f);
    }

    void setLevel(float level)
    {
        targetLevel = juce::jlimit(0.0f, 1.0f, level);
        if (level > peakLevel)
        {
            peakLevel = level;
            peakHoldCounter = peakHoldTime;
        }
        isClipping = level > 0.99f;
    }

    void timerCallback() override
    {
        // Smooth level
        const float attackSmoothing = 0.4f;
        const float releaseSmoothing = 0.1f;
        const float smoothing = targetLevel > smoothedLevel ? attackSmoothing : releaseSmoothing;
        smoothedLevel += smoothing * (targetLevel - smoothedLevel);

        // Peak decay
        smoothedPeak += 0.3f * (peakLevel - smoothedPeak);
        if (peakHoldCounter > 0)
            peakHoldCounter--;
        else
            peakLevel *= 0.95f;

        repaint();
    }

private:
    float targetLevel = 0.0f;
    float smoothedLevel = 0.0f;
    float peakLevel = 0.0f;
    float smoothedPeak = 0.0f;
    int peakHoldCounter = 0;
    static constexpr int peakHoldTime = 30;
    bool isClipping = false;
};

//==============================================================================
// Chaos Visualization - animated waveform showing modulation
class ChaosVisualizer : public juce::Component, public juce::Timer
{
public:
    ChaosVisualizer()
    {
        startTimerHz(60);
        for (auto& v : waveformBuffer)
            v = 0.0f;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Dark background
        g.setColour(juce::Colour(0xff0a0a0a));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Center line
        g.setColour(juce::Colour(0xff1a1a1a));
        g.drawHorizontalLine(static_cast<int>(bounds.getCentreY()), bounds.getX(), bounds.getRight());

        // Waveform path
        juce::Path waveform;
        const float centreY = bounds.getCentreY();
        const float amplitude = bounds.getHeight() * 0.4f;
        const float xStep = bounds.getWidth() / static_cast<float>(bufferSize - 1);

        waveform.startNewSubPath(bounds.getX(), centreY + waveformBuffer[0] * amplitude);
        for (int i = 1; i < bufferSize; ++i)
        {
            const float x = bounds.getX() + static_cast<float>(i) * xStep;
            const float y = centreY + waveformBuffer[static_cast<size_t>(i)] * amplitude;
            waveform.lineTo(x, y);
        }

        // Glow
        g.setColour(juce::Colour(0x40cc3333));
        g.strokePath(waveform, juce::PathStrokeType(4.0f));

        // Main line
        g.setColour(juce::Colour(0xffcc3333));
        g.strokePath(waveform, juce::PathStrokeType(1.5f));

        // Border
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    void pushValue(float value)
    {
        nextValue = juce::jlimit(-1.0f, 1.0f, value);
    }

    void setChaosAmount(float amount)
    {
        chaosAmount = juce::jlimit(0.0f, 1.0f, amount);
    }

    void timerCallback() override
    {
        // Shift buffer left
        for (int i = 0; i < bufferSize - 1; ++i)
            waveformBuffer[static_cast<size_t>(i)] = waveformBuffer[static_cast<size_t>(i + 1)];

        // Add new value with some interpolation noise based on chaos
        const float noise = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * chaosAmount * 0.2f;
        waveformBuffer[bufferSize - 1] = nextValue + noise;

        repaint();
    }

private:
    static constexpr int bufferSize = 128;
    std::array<float, bufferSize> waveformBuffer{};
    float nextValue = 0.0f;
    float chaosAmount = 0.0f;
};

//==============================================================================
// Oscilloscope Display
class OscilloscopeDisplay : public juce::Component, public juce::Timer
{
public:
    OscilloscopeDisplay()
    {
        startTimerHz(30);
        for (auto& v : displayBuffer)
            v = 0.0f;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Background
        g.setColour(juce::Colour(0xff080808));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Grid lines
        g.setColour(juce::Colour(0xff1a1a1a));
        const int numGridLines = 4;
        for (int i = 1; i < numGridLines; ++i)
        {
            const float y = bounds.getY() + bounds.getHeight() * static_cast<float>(i) / numGridLines;
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX() + 4.0f, bounds.getRight() - 4.0f);
        }

        // Waveform
        juce::Path waveform;
        const float centreY = bounds.getCentreY();
        const float amplitude = bounds.getHeight() * 0.45f;
        const float xStep = bounds.getWidth() / static_cast<float>(displayBufferSize - 1);

        bool started = false;
        for (int i = 0; i < displayBufferSize; ++i)
        {
            const float x = bounds.getX() + static_cast<float>(i) * xStep;
            const float y = centreY - displayBuffer[static_cast<size_t>(i)] * amplitude;

            if (!started)
            {
                waveform.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                waveform.lineTo(x, y);
            }
        }

        // Draw waveform with glow
        g.setColour(juce::Colour(0x3033cc33));
        g.strokePath(waveform, juce::PathStrokeType(3.0f));
        g.setColour(juce::Colour(0xff33cc66));
        g.strokePath(waveform, juce::PathStrokeType(1.0f));

        // Border
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    void pushSamples(const float* samples, int numSamples)
    {
        for (int i = 0; i < numSamples && writeIndex < displayBufferSize; ++i)
        {
            displayBuffer[static_cast<size_t>(writeIndex++)] = samples[i];
        }
    }

    void timerCallback() override
    {
        writeIndex = 0;
        repaint();
    }

    void setProcessor(BlackheartAudioProcessor* p) { processor = p; }

private:
    static constexpr int displayBufferSize = 256;
    std::array<float, displayBufferSize> displayBuffer{};
    int writeIndex = 0;
    BlackheartAudioProcessor* processor = nullptr;
};

//==============================================================================
// Labeled rotary knob with tooltip
class LabeledKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    LabeledKnob(const juce::String& name, const juce::String& suffix = "",
                const juce::String& tooltip = "")
        : paramName(name), valueSuffix(suffix)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled(false, false, nullptr);
        addAndMakeVisible(slider);

        nameLabel.setText(name, juce::dontSendNotification);
        nameLabel.setJustificationType(juce::Justification::centred);
        nameLabel.setFont(juce::Font(11.0f, juce::Font::bold));
        addAndMakeVisible(nameLabel);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(10.0f));
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
        addAndMakeVisible(valueLabel);

        slider.onValueChange = [this]() { updateValueLabel(); };

        if (tooltip.isNotEmpty())
            setTooltip(tooltip);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        nameLabel.setBounds(bounds.removeFromTop(16));
        valueLabel.setBounds(bounds.removeFromBottom(14));
        slider.setBounds(bounds.reduced(2));
    }

    juce::Slider& getSlider() { return slider; }

    void updateValueLabel()
    {
        auto val = slider.getValue();
        juce::String text;

        if (valueSuffix == "%")
            text = juce::String(static_cast<int>(val * 100.0)) + "%";
        else if (valueSuffix == "ms")
            text = juce::String(static_cast<int>(val)) + " ms";
        else if (valueSuffix == "Hz")
            text = juce::String(val, 1) + " Hz";
        else
            text = juce::String(val, 2);

        valueLabel.setText(text, juce::dontSendNotification);
    }

private:
    juce::Slider slider;
    juce::Label nameLabel;
    juce::Label valueLabel;
    juce::String paramName;
    juce::String valueSuffix;
};

//==============================================================================
// Momentary button with enhanced visual feedback
class MomentaryButton : public juce::TextButton, public juce::Timer
{
public:
    MomentaryButton(const juce::String& name) : juce::TextButton(name)
    {
        setClickingTogglesState(false);
        startTimerHz(60);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const bool isActive = getToggleState() || isDown();

        // Pulsing glow when active
        if (glowAmount > 0.05f)
        {
            const float pulseGlow = glowAmount * (0.8f + 0.2f * std::sin(pulsePhase));
            g.setColour(juce::Colour(0xffcc3333).withAlpha(pulseGlow * 0.5f));
            g.fillRoundedRectangle(bounds.expanded(4.0f * pulseGlow), 10.0f);
        }

        // Button background
        auto baseColour = isActive ? juce::Colour(0xffcc3333) : juce::Colour(0xff1e1e1e);
        if (isMouseOver() && !isActive)
            baseColour = baseColour.brighter(0.1f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 6.0f);

        // Inner highlight when pressed
        if (isActive)
        {
            g.setColour(juce::Colour(0x30ffffff));
            g.fillRoundedRectangle(bounds.reduced(2.0f).withTrimmedBottom(bounds.getHeight() * 0.5f), 4.0f);
        }

        // Border
        g.setColour(isActive ? juce::Colour(0xffee5555) : juce::Colour(0xff3a3a3a));
        g.drawRoundedRectangle(bounds, 6.0f, 1.5f);

        // Text
        g.setColour(isActive ? juce::Colours::white : juce::Colour(0xff888888));
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(getButtonText(), bounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        juce::TextButton::mouseDown(e);
        if (onStateChange)
            onStateChange(true);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        juce::TextButton::mouseUp(e);
        if (onStateChange)
            onStateChange(false);
    }

    void timerCallback() override
    {
        // Smooth glow transition
        const float target = getToggleState() ? 1.0f : 0.0f;
        glowAmount += 0.15f * (target - glowAmount);

        // Pulse animation
        if (getToggleState())
            pulsePhase += 0.15f;

        repaint();
    }

    std::function<void(bool)> onStateChange;

private:
    float glowAmount = 0.0f;
    float pulsePhase = 0.0f;
};

//==============================================================================
class BlackheartAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        public juce::Timer
{
public:
    BlackheartAudioProcessorEditor(BlackheartAudioProcessor&);
    ~BlackheartAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    BlackheartAudioProcessor& audioProcessor;
    BlackheartLookAndFeel blackheartLookAndFeel;
    juce::TooltipWindow tooltipWindow{ this, 500 };

    // Knobs - Fuzz section
    LabeledKnob gainKnob   { "GAIN", "%", "Controls distortion intensity" };
    LabeledKnob glareKnob  { "GLARE", "%", "Octave-up amount and gating interaction" };
    LabeledKnob blendKnob  { "BLEND", "%", "Dry/wet mix (0% = clean, 100% = full fuzz)" };
    LabeledKnob levelKnob  { "LEVEL", "%", "Output level of the fuzz block" };

    // MODE switch (3-position)
    juce::TextButton modeScreamButton { "SCRM" };
    juce::TextButton modeODButton { "OD" };
    juce::TextButton modeDoomButton { "DOOM" };

    // SHAPE knob
    LabeledKnob shapeKnob { "SHAPE", "%", "Active EQ sweep — tight to full destruction" };

    // Knobs - Chaos section
    LabeledKnob speedKnob  { "SPEED", "%", "Low: chaos modulation rate. High: ring modulation" };
    LabeledKnob chaosKnob  { "CHAOS", "%", "Modulation depth and randomness" };
    LabeledKnob riseKnob   { "RISE", "ms", "Pitch shift attack/release time" };
    LabeledKnob panicKnob  { "PANIC", "%", "Detuned pitch destruction — subtle chorus to full atonal chaos" };

    // Octave buttons
    MomentaryButton octave1Button { "+1 OCT" };
    MomentaryButton octave2Button { "+2 OCT" };

    // LED indicators
    LEDIndicator octave1LED;
    LEDIndicator octave2LED;

    // Visualizers
    LevelMeter inputMeter;
    LevelMeter outputMeter;
    ChaosVisualizer chaosVisualizer;
    OscilloscopeDisplay oscilloscope;

    // Section labels
    juce::Label fuzzSectionLabel;
    juce::Label chaosSectionLabel;
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;

    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glareAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> blendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chaosAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> riseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shapeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panicAttachment;

    void setupKnobAttachments();
    void setupOctaveButtons();
    void setupModeButtons();
    void updateVisualizers();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackheartAudioProcessorEditor)
};
