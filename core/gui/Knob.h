#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace rkfx
{

/** The suite knob: a flat circle with an indicator line, the parameter name
    above and its value below. Behaves the same in every plugin:

    - drag vertically to adjust, hold Shift for fine adjustment;
    - mouse wheel adjusts in small steps;
    - double-click resets to the default value;
    - click the value text to type an exact number.

    Repaints only when the parameter changes. */
class Knob : public juce::Component
{
public:
    Knob (juce::RangedAudioParameter& parameter, const juce::String& label = {});
    ~Knob() override;

    juce::RangedAudioParameter& getParameter() const noexcept { return param; }

    /** Dims the knob (for controls inactive in the current mode). It stays usable. */
    void setDimmed (bool shouldBeDimmed);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int kDefaultWidth = 72;
    static constexpr int kDefaultHeight = 96;

private:
    juce::RangedAudioParameter& param;
    juce::String label;
    juce::ParameterAttachment attachment;

    float normalised = 0.0f;      // what is drawn
    float dragNormalised = 0.0f;  // unsnapped, so stepped parameters drag smoothly
    float lastDragY = 0.0f;
    bool dragging = false;
    bool dimmed = false;

    juce::TextEditor valueEditor;

    juce::Rectangle<int> labelArea, dialArea, valueArea;

    void setNormalised (float newNormalised, bool asCompleteGesture);
    void showValueEditor();
    void commitValueEditor();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

} // namespace rkfx
