#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BlackheartAudioProcessorEditor::BlackheartAudioProcessorEditor(BlackheartAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&blackheartLookAndFeel);

    // Add all knobs
    addAndMakeVisible(gainKnob);
    addAndMakeVisible(glareKnob);
    addAndMakeVisible(blendKnob);
    addAndMakeVisible(levelKnob);
    addAndMakeVisible(speedKnob);
    addAndMakeVisible(chaosKnob);
    addAndMakeVisible(riseKnob);

    // Mode buttons and shape knob
    addAndMakeVisible(modeScreamButton);
    addAndMakeVisible(modeODButton);
    addAndMakeVisible(modeDoomButton);
    addAndMakeVisible(shapeKnob);

    // Add octave buttons and LEDs
    addAndMakeVisible(octave1Button);
    addAndMakeVisible(octave2Button);
    addAndMakeVisible(octave1LED);
    addAndMakeVisible(octave2LED);

    // Add visualizers
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(chaosVisualizer);
    addAndMakeVisible(oscilloscope);

    // Setup section labels
    fuzzSectionLabel.setText("FUZZ", juce::dontSendNotification);
    fuzzSectionLabel.setJustificationType(juce::Justification::centred);
    fuzzSectionLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    fuzzSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcc3333));
    addAndMakeVisible(fuzzSectionLabel);

    chaosSectionLabel.setText("CHAOS", juce::dontSendNotification);
    chaosSectionLabel.setJustificationType(juce::Justification::centred);
    chaosSectionLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    chaosSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcc3333));
    addAndMakeVisible(chaosSectionLabel);

    inputMeterLabel.setText("IN", juce::dontSendNotification);
    inputMeterLabel.setJustificationType(juce::Justification::centred);
    inputMeterLabel.setFont(juce::Font(9.0f));
    inputMeterLabel.setColour(juce::Label::textColourId, juce::Colour(0xff666666));
    addAndMakeVisible(inputMeterLabel);

    outputMeterLabel.setText("OUT", juce::dontSendNotification);
    outputMeterLabel.setJustificationType(juce::Justification::centred);
    outputMeterLabel.setFont(juce::Font(9.0f));
    outputMeterLabel.setColour(juce::Label::textColourId, juce::Colour(0xff666666));
    addAndMakeVisible(outputMeterLabel);

    // Set tooltips for buttons
    octave1Button.setTooltip("Hold to shift pitch up one octave");
    octave2Button.setTooltip("Hold to shift pitch up two octaves");

    // Setup attachments and buttons
    setupKnobAttachments();
    setupOctaveButtons();
    setupModeButtons();

    // Connect oscilloscope to processor
    oscilloscope.setProcessor(&audioProcessor);

    // Start timer for UI updates (30 Hz is enough for smooth visuals without CPU impact)
    startTimerHz(30);

    // Set window size - wider to accommodate visualizers
    setSize(620, 500);
}

BlackheartAudioProcessorEditor::~BlackheartAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void BlackheartAudioProcessorEditor::setupKnobAttachments()
{
    auto& apvts = audioProcessor.getAPVTS();

    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::gain, gainKnob.getSlider());

    glareAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::glare, glareKnob.getSlider());

    blendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::blend, blendKnob.getSlider());

    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::level, levelKnob.getSlider());

    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::speed, speedKnob.getSlider());

    chaosAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::chaos, chaosKnob.getSlider());

    riseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::rise, riseKnob.getSlider());

    shapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::shape, shapeKnob.getSlider());

    // Update value labels after attachments are made
    gainKnob.updateValueLabel();
    glareKnob.updateValueLabel();
    blendKnob.updateValueLabel();
    levelKnob.updateValueLabel();
    speedKnob.updateValueLabel();
    chaosKnob.updateValueLabel();
    riseKnob.updateValueLabel();
    shapeKnob.updateValueLabel();
}

void BlackheartAudioProcessorEditor::setupOctaveButtons()
{
    // Momentary behavior - set octave active only while button is held
    octave1Button.onStateChange = [this](bool isDown)
    {
        audioProcessor.setOctave1(isDown);
        octave1Button.setToggleState(isDown, juce::dontSendNotification);
    };

    octave2Button.onStateChange = [this](bool isDown)
    {
        audioProcessor.setOctave2(isDown);
        octave2Button.setToggleState(isDown, juce::dontSendNotification);
    };
}

void BlackheartAudioProcessorEditor::setupModeButtons()
{
    auto updateModeButtons = [this]() {
        int mode = audioProcessor.getMode();
        modeScreamButton.setToggleState(mode == 0, juce::dontSendNotification);
        modeODButton.setToggleState(mode == 1, juce::dontSendNotification);
        modeDoomButton.setToggleState(mode == 2, juce::dontSendNotification);
    };

    modeScreamButton.setClickingTogglesState(false);
    modeODButton.setClickingTogglesState(false);
    modeDoomButton.setClickingTogglesState(false);

    modeScreamButton.onClick = [this, updateModeButtons]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter(ParameterIDs::mode))
            param->setValueNotifyingHost(0.0f);
        updateModeButtons();
    };
    modeODButton.onClick = [this, updateModeButtons]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter(ParameterIDs::mode))
            param->setValueNotifyingHost(0.5f);
        updateModeButtons();
    };
    modeDoomButton.onClick = [this, updateModeButtons]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter(ParameterIDs::mode))
            param->setValueNotifyingHost(1.0f);
        updateModeButtons();
    };

    // Style buttons
    for (auto* btn : { &modeScreamButton, &modeODButton, &modeDoomButton })
    {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e1e1e));
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffcc3333));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff888888));
        btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    // Set initial state
    updateModeButtons();
}

void BlackheartAudioProcessorEditor::updateVisualizers()
{
    // Get levels from processor (thread-safe atomic reads)
    const auto& meters = audioProcessor.getSignalMeters();
    inputMeter.setLevel(meters.inputLevel.load(std::memory_order_relaxed));
    outputMeter.setLevel(meters.outputLevel.load(std::memory_order_relaxed));

    // Update chaos visualizer
    float chaosModValue = audioProcessor.getChaosModulationValue();
    chaosVisualizer.pushValue(chaosModValue);
    chaosVisualizer.setChaosAmount(audioProcessor.getChaos());

    // Update octave LEDs based on processor state
    bool oct1Active = audioProcessor.getOctave1();
    bool oct2Active = audioProcessor.getOctave2();
    octave1LED.setOn(oct1Active);
    octave2LED.setOn(oct2Active);

    // Sync mode button state
    int mode = audioProcessor.getMode();
    modeScreamButton.setToggleState(mode == 0, juce::dontSendNotification);
    modeODButton.setToggleState(mode == 1, juce::dontSendNotification);
    modeDoomButton.setToggleState(mode == 2, juce::dontSendNotification);

    // NOTE: Do NOT sync button toggle states here!
    // The MomentaryButton manages its own state via mouseDown/mouseUp.
    // Syncing here at 30Hz can conflict with the mouse state and cause
    // the button to flicker or not respond properly.
    // The LED indicators show the actual processor state which is sufficient.

    // Pull waveform samples from processor FIFO (lock-free)
    auto& fifo = audioProcessor.getWaveformFifo();
    float sample;
    std::array<float, 64> samples;
    int sampleCount = 0;

    while (fifo.pull(sample) && sampleCount < 64)
    {
        samples[static_cast<size_t>(sampleCount++)] = sample;
    }

    if (sampleCount > 0)
        oscilloscope.pushSamples(samples.data(), sampleCount);
}

void BlackheartAudioProcessorEditor::timerCallback()
{
    updateVisualizers();
}

void BlackheartAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Dark gradient background
    juce::ColourGradient bgGradient(
        juce::Colour(0xff161616), 0.0f, 0.0f,
        juce::Colour(0xff0e0e0e), 0.0f, static_cast<float>(getHeight()),
        false);
    g.setGradientFill(bgGradient);
    g.fillAll();

    // Title with glow
    auto titleBounds = getLocalBounds().removeFromTop(45);

    // Title glow
    g.setColour(juce::Colour(0x30cc3333));
    g.setFont(juce::Font(28.0f, juce::Font::bold));
    g.drawText("BLACKHEART", titleBounds.translated(0, 1), juce::Justification::centred);

    // Title text
    g.setColour(juce::Colour(0xffcc3333));
    g.drawText("BLACKHEART", titleBounds, juce::Justification::centred);

    // Subtitle
    g.setColour(juce::Colour(0xff555555));
    g.setFont(juce::Font(9.0f));
    g.drawText("OCTAVE FUZZ + CHAOS ENGINE", titleBounds.translated(0, 22), juce::Justification::centred);

    // Section dividers
    g.setColour(juce::Colour(0xff2a2a2a));

    // Vertical divider between fuzz and chaos
    const int dividerX = 290;
    g.drawLine(static_cast<float>(dividerX), 50.0f, static_cast<float>(dividerX), 400.0f, 1.0f);

    // Horizontal divider above visualizers
    g.drawLine(20.0f, 405.0f, static_cast<float>(getWidth() - 20), 405.0f, 1.0f);

    // Panel backgrounds
    g.setColour(juce::Colour(0x10ffffff));
    g.fillRoundedRectangle(10.0f, 50.0f, 275.0f, 350.0f, 6.0f);  // Fuzz panel
    g.fillRoundedRectangle(295.0f, 50.0f, 315.0f, 350.0f, 6.0f); // Chaos panel

    // Outer border
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 4.0f, 1.0f);
}

void BlackheartAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Title area
    bounds.removeFromTop(50);

    // Bottom visualizer strip
    auto visualizerStrip = bounds.removeFromBottom(85);

    // Main content area
    auto contentArea = bounds.reduced(15, 5);

    // Split into left (fuzz) and right (chaos) sections
    auto fuzzSection = contentArea.removeFromLeft(260);
    contentArea.removeFromLeft(20); // Gap
    auto chaosSection = contentArea;

    //==========================================================================
    // FUZZ SECTION
    //==========================================================================
    {
        auto area = fuzzSection;

        // Section label
        fuzzSectionLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);

        const int knobWidth = area.getWidth() / 2;
        const int knobHeight = 85;

        // Row 1: Gain, Glare
        auto row1 = area.removeFromTop(knobHeight);
        gainKnob.setBounds(row1.removeFromLeft(knobWidth));
        glareKnob.setBounds(row1);

        area.removeFromTop(4);

        // Row 2: MODE switch (3 buttons in a row)
        auto modeRow = area.removeFromTop(28);
        modeRow = modeRow.reduced(10, 0);
        const int modeBtnW = modeRow.getWidth() / 3;
        modeScreamButton.setBounds(modeRow.removeFromLeft(modeBtnW).reduced(2, 0));
        modeODButton.setBounds(modeRow.removeFromLeft(modeBtnW).reduced(2, 0));
        modeDoomButton.setBounds(modeRow.reduced(2, 0));

        area.removeFromTop(4);

        // Row 3: Shape, Level
        auto row3 = area.removeFromTop(knobHeight);
        shapeKnob.setBounds(row3.removeFromLeft(knobWidth));
        levelKnob.setBounds(row3);

        area.removeFromTop(4);

        // Row 4: Blend (centered)
        auto row4 = area.removeFromTop(knobHeight);
        blendKnob.setBounds(row4.withSizeKeepingCentre(knobWidth, knobHeight));
    }
    //==========================================================================
    // CHAOS SECTION
    //==========================================================================
    {
        auto area = chaosSection;

        // Section label
        chaosSectionLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(8);

        // Top row: Speed, Chaos, Rise knobs
        const int knobWidth = area.getWidth() / 3;
        const int knobHeight = 95;

        auto topRow = area.removeFromTop(knobHeight);
        speedKnob.setBounds(topRow.removeFromLeft(knobWidth));
        chaosKnob.setBounds(topRow.removeFromLeft(knobWidth));
        riseKnob.setBounds(topRow);

        area.removeFromTop(15);

        // Octave buttons with LEDs
        auto buttonArea = area.removeFromTop(50);
        const int buttonWidth = 90;
        const int ledSize = 12;
        const int buttonGap = 30;

        // Center the buttons
        const int totalWidth = buttonWidth * 2 + buttonGap;
        const int startX = (buttonArea.getWidth() - totalWidth) / 2;

        // Octave 1 button and LED
        auto oct1Area = buttonArea.withX(buttonArea.getX() + startX).withWidth(buttonWidth);
        octave1Button.setBounds(oct1Area.withHeight(40));
        octave1LED.setBounds(oct1Area.getX() + (buttonWidth - ledSize) / 2,
                             oct1Area.getBottom() - 8, ledSize, ledSize);

        // Octave 2 button and LED
        auto oct2Area = buttonArea.withX(buttonArea.getX() + startX + buttonWidth + buttonGap).withWidth(buttonWidth);
        octave2Button.setBounds(oct2Area.withHeight(40));
        octave2LED.setBounds(oct2Area.getX() + (buttonWidth - ledSize) / 2,
                             oct2Area.getBottom() - 8, ledSize, ledSize);

        // Chaos visualizer below buttons
        area.removeFromTop(10);
        chaosVisualizer.setBounds(area.reduced(10, 0).withHeight(50));
    }

    //==========================================================================
    // VISUALIZER STRIP (bottom)
    //==========================================================================
    {
        auto area = visualizerStrip.reduced(15, 8);

        // Input meter on left
        auto inputArea = area.removeFromLeft(25);
        inputMeterLabel.setBounds(inputArea.removeFromBottom(12));
        inputMeter.setBounds(inputArea.reduced(2, 0));

        area.removeFromLeft(10);

        // Output meter on right
        auto outputArea = area.removeFromRight(25);
        outputMeterLabel.setBounds(outputArea.removeFromBottom(12));
        outputMeter.setBounds(outputArea.reduced(2, 0));

        area.removeFromRight(10);

        // Oscilloscope fills the remaining space
        oscilloscope.setBounds(area.reduced(0, 2));
    }
}
