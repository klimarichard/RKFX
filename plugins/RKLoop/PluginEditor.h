#pragma once

#include "plugin/EditorBase.h"

class RKLoopProcessor;

class RKLoopEditor : public rkfx::EditorBase
{
public:
    explicit RKLoopEditor (RKLoopProcessor&);

private:
    RKLoopProcessor& loopProcessor;

    /** A footswitch: down while held (the processor acts on the press). */
    struct Footswitch : public juce::TextButton
    {
        Footswitch (juce::RangedAudioParameter&, const juce::String& text);
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        juce::RangedAudioParameter& param;
    };

    std::array<std::unique_ptr<Footswitch>, 4> footswitches;
    std::unique_ptr<rkfx::ToggleSwitch> reverse, halfSpeed;
    std::unique_ptr<rkfx::Knob> level;

    juce::Rectangle<int> loopArea() const;
    juce::Rectangle<int> statusArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;
    void refresh();
    juce::TimedCallback refresher { [this] { refresh(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKLoopEditor)
};
