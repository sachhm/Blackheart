#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

//==============================================================================
BlackheartAudioProcessorEditor::BlackheartAudioProcessorEditor(BlackheartAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&blackheartLookAndFeel);

    // Load blackletter font from binary data
    blackletterTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::UnifrakturMaguntiaRegular_ttf,
        BinaryData::UnifrakturMaguntiaRegular_ttfSize);

    // Add all knobs
    addAndMakeVisible(gainKnob);
    addAndMakeVisible(glareKnob);
    addAndMakeVisible(blendKnob);
    addAndMakeVisible(levelKnob);
    addAndMakeVisible(speedKnob);
    addAndMakeVisible(chaosKnob);
    addAndMakeVisible(riseKnob);
    addAndMakeVisible(panicKnob);
    addAndMakeVisible(chaosMixKnob);

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

    // Mode LEDs
    addAndMakeVisible(modeScreamLED);
    addAndMakeVisible(modeODLED);
    addAndMakeVisible(modeDoomLED);

    // Add visualizers
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(oscilloscope);
    // chaosVisualizer kept alive for internal updates but not shown in section

    // Setup section labels
    fuzzSectionLabel.setText("VIOLENCE", juce::dontSendNotification);
    fuzzSectionLabel.setJustificationType(juce::Justification::centred);
    fuzzSectionLabel.setFont(juce::Font(15.0f, juce::Font::bold));
    fuzzSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffdddddd));
    addAndMakeVisible(fuzzSectionLabel);

    chaosSectionLabel.setText("FRACTURE", juce::dontSendNotification);
    chaosSectionLabel.setJustificationType(juce::Justification::centred);
    chaosSectionLabel.setFont(juce::Font(15.0f, juce::Font::bold));
    chaosSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffdddddd));
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

    // Set window size
    setSize(700, 600);
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

    panicAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::panic, panicKnob.getSlider());

    chaosMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::chaosMix, chaosMixKnob.getSlider());

    // Update value labels after attachments are made
    gainKnob.updateValueLabel();
    glareKnob.updateValueLabel();
    blendKnob.updateValueLabel();
    levelKnob.updateValueLabel();
    speedKnob.updateValueLabel();
    chaosKnob.updateValueLabel();
    riseKnob.updateValueLabel();
    shapeKnob.updateValueLabel();
    panicKnob.updateValueLabel();
    chaosMixKnob.updateValueLabel();
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

    // Style buttons — individual colors per mode
    for (auto* btn : { &modeScreamButton, &modeODButton, &modeDoomButton })
    {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff888888));
    }

    // SCRM = Red
    modeScreamButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffcc2222));
    modeScreamButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));

    // OD = Blue
    modeODButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2255cc));
    modeODButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));

    // DOOM = White
    modeDoomButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffdddddd));
    modeDoomButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff000000));

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

    // Sync mode button and LED state
    int mode = audioProcessor.getMode();
    modeScreamButton.setToggleState(mode == 0, juce::dontSendNotification);
    modeODButton.setToggleState(mode == 1, juce::dontSendNotification);
    modeDoomButton.setToggleState(mode == 2, juce::dontSendNotification);
    modeScreamLED.setOn(mode == 0);
    modeODLED.setOn(mode == 1);
    modeDoomLED.setOn(mode == 2);

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
    // Pure black background
    g.fillAll(juce::Colour(0xff000000));

    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());

    //==========================================================================
    // HEADER
    //==========================================================================
    auto titleBounds = getLocalBounds().removeFromTop(65);

    // "blackheart" logo — top left
    if (blackletterTypeface != nullptr)
    {
        juce::Font blackletterFont(blackletterTypeface);
        blackletterFont.setHeight(38.0f);

        // Subtle white glow
        g.setColour(juce::Colour(0x18ffffff));
        g.setFont(blackletterFont);
        g.drawText("blackheart", titleBounds.withLeft(18).translated(0, 1),
                   juce::Justification::centredLeft);

        // Main title
        g.setColour(juce::Colour(0xffffffff));
        g.drawText("blackheart", titleBounds.withLeft(18),
                   juce::Justification::centredLeft);
    }
    else
    {
        g.setColour(juce::Colour(0xffffffff));
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText("BLVCKHEART", titleBounds.withLeft(18),
                   juce::Justification::centredLeft);
    }

    // "NVGN X Sachh Moka" — top right branding
    g.setColour(juce::Colour(0xffffffff));
    g.setFont(juce::Font(14.0f));
    g.drawText("NVGN X Sachh Moka", titleBounds.withRight(static_cast<int>(w) - 20),
               juce::Justification::centredRight);

    //==========================================================================
    // SECTION PANELS — exact coordinates matching resized() layout
    //==========================================================================
    // Shared constants (must match resized())
    const float margin = 12.0f;
    const float headerH = 68.0f;
    const float vizH = 95.0f;
    const float gap = 10.0f;
    const float panelRadius = 6.0f;

    const float panelTop = headerH;
    const float panelBottom = h - vizH;
    const float panelH = panelBottom - panelTop - gap;
    const float vizTop = panelBottom;
    const float vizBottom = h - margin;

    // Two equal-width section panels with gap between
    const float totalPanelW = w - margin * 2.0f;
    const float sectionW = (totalPanelW - gap) * 0.5f;
    const float leftX = margin;
    const float rightX = margin + sectionW + gap;

    // Violence panel — fill + border
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRoundedRectangle(leftX, panelTop, sectionW, panelH, panelRadius);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawRoundedRectangle(leftX, panelTop, sectionW, panelH, panelRadius, 1.0f);

    // Fracture panel — fill + border
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRoundedRectangle(rightX, panelTop, sectionW, panelH, panelRadius);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawRoundedRectangle(rightX, panelTop, sectionW, panelH, panelRadius, 1.0f);

    // Visualizer strip panel — fill + border
    g.setColour(juce::Colour(0xff080808));
    g.fillRoundedRectangle(leftX, vizTop, totalPanelW, vizBottom - vizTop, panelRadius);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawRoundedRectangle(leftX, vizTop, totalPanelW, vizBottom - vizTop, panelRadius, 1.0f);
}

void BlackheartAudioProcessorEditor::resized()
{
    const int W = getWidth();
    const int H = getHeight();

    // Shared constants (must match paint())
    const int margin = 12;
    const int headerH = 68;
    const int vizH = 95;
    const int gap = 10;
    const int panelPad = 10;  // inner padding inside panels

    const int panelTop = headerH;
    const int panelBottom = H - vizH;
    const int panelH = panelBottom - panelTop - gap;
    const int vizTop = panelBottom;

    const int totalPanelW = W - margin * 2;
    const int sectionW = (totalPanelW - gap) / 2;
    const int leftX = margin;
    const int rightX = margin + sectionW + gap;

    // Panel inner bounds
    auto fuzzPanel = juce::Rectangle<int>(leftX, panelTop, sectionW, panelH).reduced(panelPad);
    auto chaosPanel = juce::Rectangle<int>(rightX, panelTop, sectionW, panelH).reduced(panelPad);
    auto vizPanel = juce::Rectangle<int>(leftX, vizTop, totalPanelW, H - margin - vizTop).reduced(panelPad);

    // Shared row layout constants — both sections use identical vertical positions
    const int labelH = 22;
    const int labelGap = 8;
    const int knobHeight = 90;
    const int rowGap = 8;
    const int blendRowH = 90;  // blend/mix knob row
    const int blendGap = 8;
    const int buttonRowH = 50; // buttons + LEDs row
    const int ledSize = 10;
    const int ledGapBelow = 4; // gap between button bottom and LED

    //==========================================================================
    // FUZZ SECTION (VIOLENCE)
    //==========================================================================
    {
        auto area = fuzzPanel;

        // Row 0: Section label
        fuzzSectionLabel.setBounds(area.removeFromTop(labelH));
        area.removeFromTop(labelGap);

        const int knobWidth = area.getWidth() / 2;

        // Row 1: Gain, Glare
        auto row1 = area.removeFromTop(knobHeight);
        gainKnob.setBounds(row1.removeFromLeft(knobWidth));
        glareKnob.setBounds(row1);
        area.removeFromTop(rowGap);

        // Row 2: Shape, Level
        auto row2 = area.removeFromTop(knobHeight);
        shapeKnob.setBounds(row2.removeFromLeft(knobWidth));
        levelKnob.setBounds(row2);
        area.removeFromTop(rowGap);

        // Row 3: Blend (centered)
        auto row3 = area.removeFromTop(blendRowH);
        blendKnob.setBounds(row3.withSizeKeepingCentre(knobWidth, blendRowH));
        area.removeFromTop(blendGap);

        // Row 4: MODE buttons with LEDs underneath
        auto row4 = area.removeFromTop(buttonRowH);
        {
            auto btnRow = row4.reduced(6, 0);
            const int btnH = 32;
            const int modeBtnW = btnRow.getWidth() / 3;

            auto scrArea = btnRow.removeFromLeft(modeBtnW).reduced(4, 0);
            modeScreamButton.setBounds(scrArea.withHeight(btnH));
            modeScreamLED.setBounds(scrArea.getCentreX() - ledSize / 2,
                                    scrArea.getY() + btnH + ledGapBelow, ledSize, ledSize);

            auto odArea = btnRow.removeFromLeft(modeBtnW).reduced(4, 0);
            modeODButton.setBounds(odArea.withHeight(btnH));
            modeODLED.setBounds(odArea.getCentreX() - ledSize / 2,
                                odArea.getY() + btnH + ledGapBelow, ledSize, ledSize);

            auto doomArea = btnRow.reduced(4, 0);
            modeDoomButton.setBounds(doomArea.withHeight(btnH));
            modeDoomLED.setBounds(doomArea.getCentreX() - ledSize / 2,
                                  doomArea.getY() + btnH + ledGapBelow, ledSize, ledSize);
        }
    }

    //==========================================================================
    // CHAOS SECTION (FRACTURE)
    //==========================================================================
    {
        auto area = chaosPanel;

        // Row 0: Section label
        chaosSectionLabel.setBounds(area.removeFromTop(labelH));
        area.removeFromTop(labelGap);

        const int knobWidth = area.getWidth() / 2;

        // Row 1: Speed, Chaos
        auto row1 = area.removeFromTop(knobHeight);
        speedKnob.setBounds(row1.removeFromLeft(knobWidth));
        chaosKnob.setBounds(row1);
        area.removeFromTop(rowGap);

        // Row 2: Rise, Panic
        auto row2 = area.removeFromTop(knobHeight);
        riseKnob.setBounds(row2.removeFromLeft(knobWidth));
        panicKnob.setBounds(row2);
        area.removeFromTop(rowGap);

        // Row 3: Mix (centered)
        auto row3 = area.removeFromTop(blendRowH);
        chaosMixKnob.setBounds(row3.withSizeKeepingCentre(knobWidth, blendRowH));
        area.removeFromTop(blendGap);

        // Row 4: Octave buttons with LEDs underneath (same layout as mode buttons)
        auto row4 = area.removeFromTop(buttonRowH);
        {
            auto btnRow = row4.reduced(6, 0);
            const int btnH = 32;
            // Two buttons centered, same width as mode buttons
            const int octBtnW = btnRow.getWidth() / 3;
            const int totalW = octBtnW * 2;
            const int offsetX = (btnRow.getWidth() - totalW) / 2;

            auto oct1Area = btnRow.withX(btnRow.getX() + offsetX).withWidth(octBtnW).reduced(4, 0);
            octave1Button.setBounds(oct1Area.withHeight(btnH));
            octave1LED.setBounds(oct1Area.getCentreX() - ledSize / 2,
                                 oct1Area.getY() + btnH + ledGapBelow, ledSize, ledSize);

            auto oct2Area = btnRow.withX(btnRow.getX() + offsetX + octBtnW).withWidth(octBtnW).reduced(4, 0);
            octave2Button.setBounds(oct2Area.withHeight(btnH));
            octave2LED.setBounds(oct2Area.getCentreX() - ledSize / 2,
                                 oct2Area.getY() + btnH + ledGapBelow, ledSize, ledSize);
        }

        // Hide chaos visualizer from this section (consolidated to bottom)
        chaosVisualizer.setBounds(0, 0, 0, 0);
    }

    //==========================================================================
    // VISUALIZER STRIP (bottom)
    //==========================================================================
    {
        auto area = vizPanel;

        // Input meter on left
        auto inputArea = area.removeFromLeft(28);
        inputMeterLabel.setBounds(inputArea.removeFromBottom(14));
        inputMeter.setBounds(inputArea.reduced(3, 0));

        area.removeFromLeft(10);

        // Output meter on right
        auto outputArea = area.removeFromRight(28);
        outputMeterLabel.setBounds(outputArea.removeFromBottom(14));
        outputMeter.setBounds(outputArea.reduced(3, 0));

        area.removeFromRight(10);

        // Oscilloscope fills the remaining space
        oscilloscope.setBounds(area.reduced(0, 2));
    }
}
