#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/BlackheartPalette.h"

//==============================================================================
BlackheartAudioProcessorEditor::BlackheartAudioProcessorEditor(BlackheartAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&blackheartLookAndFeel);

    for (auto* knob : { &gainKnob, &glareKnob, &shapeKnob, &levelKnob, &blendKnob,
                        &speedKnob, &chaosKnob, &riseKnob, &panicKnob, &chaosMixKnob })
        addAndMakeVisible(*knob);

    for (auto* btn : { &ch1Button, &ch2Button, &ch3Button, &octave1Button, &octave2Button })
        addAndMakeVisible(*btn);

    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(headerStrip);
    addAndMakeVisible(statusStrip);
    headerStrip.setSampleRate(audioProcessor.getSampleRate());
    statusStrip.setSampleRate(audioProcessor.getSampleRate());

    auto sectionFont = BlackheartLookAndFeel::monoFont(13.0f);
    sectionFont.setExtraKerningFactor(0.20f);

    signalSectionLabel.setText("SIGNAL", juce::dontSendNotification);
    signalSectionLabel.setJustificationType(juce::Justification::centredLeft);
    signalSectionLabel.setFont(sectionFont);
    signalSectionLabel.setColour(juce::Label::textColourId, Blackheart::dim());
    addAndMakeVisible(signalSectionLabel);

    interferenceSectionLabel.setText("INTERFERENCE", juce::dontSendNotification);
    interferenceSectionLabel.setJustificationType(juce::Justification::centredLeft);
    interferenceSectionLabel.setFont(sectionFont);
    interferenceSectionLabel.setColour(juce::Label::textColourId, Blackheart::dim());
    addAndMakeVisible(interferenceSectionLabel);

    octave1Button.setTooltip("Hold to shift pitch up one octave");
    octave2Button.setTooltip("Hold to shift pitch up two octaves");

    setupKnobAttachments();
    setupOctaveButtons();
    setupChannelButtons();

    oscilloscope.setProcessor(&audioProcessor);

    startTimerHz(30);

    // Resizable, aspect-locked to default 760x580.
    setResizable(true, true);
    setResizeLimits(640, 490, 1280, 980);
    getConstrainer()->setFixedAspectRatio(760.0 / 580.0);
    setSize(760, 580);
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

    for (auto* knob : { &gainKnob, &glareKnob, &shapeKnob, &levelKnob, &blendKnob,
                        &speedKnob, &chaosKnob, &riseKnob, &panicKnob, &chaosMixKnob })
        knob->updateValueLabel();
}

void BlackheartAudioProcessorEditor::setupOctaveButtons()
{
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

void BlackheartAudioProcessorEditor::setupChannelButtons()
{
    auto updateChannelButtons = [this]() {
        const int mode = audioProcessor.getMode();
        ch1Button.setToggleState(mode == 0, juce::dontSendNotification);
        ch2Button.setToggleState(mode == 1, juce::dontSendNotification);
        ch3Button.setToggleState(mode == 2, juce::dontSendNotification);
    };

    ch1Button.onClick = [this, updateChannelButtons]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter(ParameterIDs::mode))
            param->setValueNotifyingHost(0.0f);
        updateChannelButtons();
    };
    ch2Button.onClick = [this, updateChannelButtons]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter(ParameterIDs::mode))
            param->setValueNotifyingHost(0.5f);
        updateChannelButtons();
    };
    ch3Button.onClick = [this, updateChannelButtons]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter(ParameterIDs::mode))
            param->setValueNotifyingHost(1.0f);
        updateChannelButtons();
    };

    updateChannelButtons();
}

void BlackheartAudioProcessorEditor::updateVisualizers()
{
    const auto& meters = audioProcessor.getSignalMeters();
    const float inputLevel  = meters.inputLevel.load(std::memory_order_relaxed);
    const float outputLevel = meters.outputLevel.load(std::memory_order_relaxed);
    headerStrip.setLevels(inputLevel, outputLevel);
    headerStrip.setActivity(juce::jmax(inputLevel, outputLevel));

    // Channel buttons track processor state.
    const int mode = audioProcessor.getMode();
    ch1Button.setToggleState(mode == 0, juce::dontSendNotification);
    ch2Button.setToggleState(mode == 1, juce::dontSendNotification);
    ch3Button.setToggleState(mode == 2, juce::dontSendNotification);

    // OSD readouts.
    static const char* modeNames[] = { "SCREAM", "OD", "DOOM" };
    oscilloscope.setOsdChannel(modeNames[juce::jlimit(0, 2, mode)]);

    const bool oct1 = audioProcessor.getOctave1();
    const bool oct2 = audioProcessor.getOctave2();
    juce::String momentary;
    if (oct1) momentary = juce::String::fromUTF8("+1OCT\xC2\xB7HOLD");
    if (oct2) momentary = juce::String::fromUTF8("+2OCT\xC2\xB7HOLD");
    if (oct1 && oct2) momentary = juce::String::fromUTF8("+1+2OCT\xC2\xB7HOLD");
    oscilloscope.setOsdMomentary(momentary);

    // Waveform samples from processor FIFO (lock-free).
    auto& fifo = audioProcessor.getWaveformFifo();
    float sample;
    std::array<float, 64> samples;
    int sampleCount = 0;

    while (fifo.pull(sample) && sampleCount < 64)
        samples[static_cast<size_t>(sampleCount++)] = sample;

    if (sampleCount > 0)
        oscilloscope.pushSamples(samples.data(), sampleCount);
}

void BlackheartAudioProcessorEditor::timerCallback()
{
    updateVisualizers();

    const double sr = audioProcessor.getSampleRate();
    const float latMs = sr > 0.0 ? static_cast<float>(audioProcessor.getLatencyInSamples() * 1000.0 / sr) : 0.0f;
    headerStrip.setSampleRate(sr);
    headerStrip.setLatencyMs(latMs);
    statusStrip.setSampleRate(sr);
    statusStrip.setLatencyMs(latMs);
    statusStrip.setCpuLoad(audioProcessor.getCpuLoad());
}

void BlackheartAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(Blackheart::bg());

    // Open NDSP-style face — hairline dividers, no boxed panels.
    g.setColour(Blackheart::line());
    if (! panelLeft.isEmpty())
        g.drawHorizontalLine(panelLeft.getY(), static_cast<float>(panelLeft.getX()),
                             static_cast<float>(panelLeft.getRight()));
    if (! panelRight.isEmpty())
        g.drawHorizontalLine(panelRight.getY(), static_cast<float>(panelRight.getX()),
                             static_cast<float>(panelRight.getRight()));
}

void BlackheartAudioProcessorEditor::resized()
{
    constexpr int kGrid    = 8;
    constexpr int kHeaderH = 40;
    constexpr int kStatusH = 24;
    constexpr int kMargin  = 24;
    constexpr int kBtnH    = 72;

    auto area = getLocalBounds();

    headerStrip.setBounds(area.removeFromTop(kHeaderH));
    statusStrip.setBounds(area.removeFromBottom(kStatusH));

    area.reduce(kMargin, kGrid);

    // Slim scope strip under the header.
    const int scopeH = juce::jmax(64, area.getHeight() * 14 / 100);
    oscilloscope.setBounds(area.removeFromTop(scopeH));
    area.removeFromTop(kGrid * 2);

    // Footer rail — channel selector left, octave momentaries right.
    auto rail = area.removeFromBottom(kBtnH);
    area.removeFromBottom(kGrid * 2);
    {
        const int btnW = 92;
        auto left = rail;
        for (auto* b : { &ch1Button, &ch2Button, &ch3Button })
        {
            b->setBounds(left.removeFromLeft(btnW));
            left.removeFromLeft(kGrid);
        }
        auto right = rail;
        for (auto* b : { &octave2Button, &octave1Button })
        {
            b->setBounds(right.removeFromRight(btnW));
            right.removeFromRight(kGrid);
        }
    }

    // Two full-width knob rows: SIGNAL (5) over INTERFERENCE (5).
    constexpr int kLabelH = 18;
    const int rowH = (area.getHeight() - kGrid) / 2;

    auto layoutRow = [&](juce::Rectangle<int> row,
                         juce::Label& sectionLabel,
                         std::array<FlatKnob*, 5> knobs) -> juce::Rectangle<int>
    {
        const auto full = row;
        sectionLabel.setBounds(row.removeFromTop(kLabelH));
        const int knobW = row.getWidth() / 5;
        for (auto* k : knobs)
            k->setBounds(row.removeFromLeft(knobW));
        return full;
    };

    panelLeft = layoutRow(area.removeFromTop(rowH), signalSectionLabel,
                          { &gainKnob, &glareKnob, &shapeKnob, &levelKnob, &blendKnob });
    area.removeFromTop(kGrid);
    panelRight = layoutRow(area, interferenceSectionLabel,
                           { &speedKnob, &chaosKnob, &riseKnob, &panicKnob, &chaosMixKnob });
}
