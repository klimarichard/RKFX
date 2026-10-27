#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace rkfx
{

/** An on/off switch for a bool parameter: a flat rounded box, filled with the
    plugin's accent when on. Click toggles; repaints only on change. */
class ToggleSwitch : public juce::Component
{
public:
    ToggleSwitch (juce::RangedAudioParameter& parameter, juce::Colour accent, const juce::String& text = {});

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Colour accent;
    juce::String text;
    juce::ParameterAttachment attachment;
    bool on = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToggleSwitch)
};

/** A segmented switch for a choice parameter: one segment per choice, the
    selected one filled with the accent. Optional title above. */
class SegmentedSwitch : public juce::Component
{
public:
    SegmentedSwitch (juce::RangedAudioParameter& parameter, juce::Colour accent,
                     juce::StringArray segmentLabels = {}, const juce::String& title = {});

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Colour accent;
    juce::StringArray labels;
    juce::String title;
    juce::ParameterAttachment attachment;
    int selected = 0;

    juce::Rectangle<int> segmentArea() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedSwitch)
};

} // namespace rkfx
