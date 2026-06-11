#pragma once

#include <JuceHeader.h>

// Machined instrument button — same dimensional finish as the knobs.
// Drop shadow, vertical sheen, bevel rim; accent body when active.
class PillButton : public juce::TextButton
{
public:
    explicit PillButton(const juce::String& name);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    std::function<void(bool)> onStateChange;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PillButton)
};
