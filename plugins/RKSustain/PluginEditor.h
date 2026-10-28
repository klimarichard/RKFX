#pragma once

#include "plugin/EditorBase.h"

class RKSustainProcessor;

class RKSustainEditor : public rkfx::EditorBase
{
public:
    explicit RKSustainEditor (RKSustainProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> mode;
    std::unique_ptr<rkfx::ToggleSwitch> pedal, useCC;
    std::unique_ptr<rkfx::Knob> cc;
    std::unique_ptr<juce::ParameterAttachment> ccWatcher;

    juce::Rectangle<int> modeArea() const;
    juce::Rectangle<int> pedalArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKSustainEditor)
};
