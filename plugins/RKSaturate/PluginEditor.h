#pragma once

#include "plugin/EditorBase.h"

class RKSaturateProcessor;

class RKSaturateEditor : public rkfx::EditorBase
{
public:
    explicit RKSaturateEditor (RKSaturateProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> character, oversampling;
    std::unique_ptr<rkfx::ToggleSwitch> autoGain;
    std::unique_ptr<rkfx::Knob> drive, tone;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKSaturateEditor)
};
