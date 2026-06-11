#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Parameters/ParameterIDs.h"
#include "UI/BlackheartLookAndFeel.h"
#include "UI/FlatKnob.h"
#include "UI/PillButton.h"
#include "UI/Scope.h"
#include "UI/HeaderStrip.h"
#include "UI/StatusStrip.h"

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
    juce::TooltipWindow tooltipWindow{ this, 700 };

    // SIGNAL block (fuzz)
    FlatKnob gainKnob   { "GAIN", "%", "Controls distortion intensity" };
    FlatKnob glareKnob  { "GLARE", "%", "Octave-up amount and gating interaction" };
    FlatKnob shapeKnob  { "SHAPE", "%", "Active EQ sweep — tight to full destruction" };
    FlatKnob levelKnob  { "LEVEL", "%", "Output level of the fuzz block" };
    FlatKnob blendKnob  { "BLEND", "%", "Dry/wet mix (0% = clean, 100% = full fuzz)" };

    // Preset selector
    PillButton ch1Button { "SCREAM" };
    PillButton ch2Button { "OD" };
    PillButton ch3Button { "DOOM" };

    // INTERFERENCE block (pitch / chaos)
    FlatKnob speedKnob    { "SPEED", "%", "Low: chaos modulation rate. High: ring modulation" };
    FlatKnob chaosKnob    { "CHAOS", "%", "Modulation depth and randomness" };
    FlatKnob riseKnob     { "RISE", "ms", "Pitch shift attack/release time" };
    FlatKnob panicKnob    { "PANIC", "%", "Detuned pitch destruction — subtle chorus to full atonal chaos" };
    FlatKnob chaosMixKnob { "MIX", "%", "Dry/wet mix for pitch/chaos section" };

    // Octave momentaries
    PillButton octave1Button { "+1 OCT" };
    PillButton octave2Button { "+2 OCT" };

    // CRT scope (owns IN/OUT meters + OSD)
    Scope oscilloscope;

    HeaderStrip headerStrip;
    StatusStrip statusStrip;

    // Panel rects for paint() — populated by resized()
    juce::Rectangle<int> panelLeft;
    juce::Rectangle<int> panelRight;

    juce::Label signalSectionLabel;
    juce::Label interferenceSectionLabel;

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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chaosMixAttachment;

    void setupKnobAttachments();
    void setupOctaveButtons();
    void setupChannelButtons();
    void updateVisualizers();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackheartAudioProcessorEditor)
};
