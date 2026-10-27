#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace rkfx
{

/** A vertical slider (graphic EQ bands), with the same handling as the knob:

    - drag vertically to adjust, hold Shift for fine adjustment;
    - mouse wheel adjusts in small steps;
    - double-click resets to the default value;
    - click the value text to type an exact number.

    The value sits above the track, the label below. For a bipolar parameter
    (range around 0) the track is filled from the centre. Repaints only when
    the parameter changes. */
class Fader : public juce::Component
{
public:
    Fader (juce::RangedAudioParameter& parameter, const juce::String& label);

    /** Narrow faders show whole numbers only. */
    void setCompactValue (bool compact);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::RangedAudioParameter& param;
    juce::String label;
    juce::ParameterAttachment attachment;

    float normalised = 0.5f, dragNormalised = 0.5f, lastDragY = 0.0f;
    bool dragging = false, compact = false;

    juce::TextEditor valueEditor;
    juce::Rectangle<int> valueArea, trackArea, labelArea;

    juce::String valueText() const;
    void setNormalised (float value, bool asCompleteGesture);
    void commitValueEditor();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fader)
};

} // namespace rkfx
