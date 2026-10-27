#pragma once

#include "plugin/EditorBase.h"

class RKDistortProcessor;

class RKDistortEditor : public rkfx::EditorBase
{
public:
    explicit RKDistortEditor (RKDistortProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> type, cab, oversampling;
    std::unique_ptr<rkfx::ToggleSwitch> autoGain;
    std::unique_ptr<rkfx::Knob> gain, tight, mid, tone;

    juce::Rectangle<int> driveArea() const;
    juce::Rectangle<int> toneArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKDistortEditor)
};
