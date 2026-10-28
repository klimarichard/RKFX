#pragma once

#include "plugin/EditorBase.h"

class RKArpeggiateProcessor;

class RKArpeggiateEditor : public rkfx::EditorBase
{
public:
    explicit RKArpeggiateEditor (RKArpeggiateProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> pattern, velocityMode;
    std::unique_ptr<rkfx::ToggleSwitch> latch;
    std::unique_ptr<rkfx::Knob> division, octaves, gate, swing, velocity;
    std::unique_ptr<juce::ParameterAttachment> velocityWatcher;

    juce::Rectangle<int> patternArea() const;
    juce::Rectangle<int> feelArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKArpeggiateEditor)
};
