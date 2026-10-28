#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace rkfx
{

/** Controls tag themselves with the ID of the parameter they drive, so a
    host of the editor (RKBoard) can find it, e.g. to assign it on a
    right-click. */
inline void tagParameter (juce::Component& c, const juce::String& parameterId)
{
    c.getProperties().set ("rkfx.parameter", parameterId);
}

/** The parameter ID @p c (or the nearest of its parents) was tagged with,
    up to and excluding @p stopAt; empty if none. */
inline juce::String findTaggedParameter (juce::Component* c, const juce::Component* stopAt = nullptr)
{
    for (; c != nullptr && c != stopAt; c = c->getParentComponent())
        if (const auto* id = c->getProperties().getVarPointer ("rkfx.parameter"))
            return id->toString();
    return {};
}

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
