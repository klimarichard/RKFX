#pragma once

#include "plugin/EditorBase.h"

class RKSwellProcessor;

class RKSwellEditor : public rkfx::EditorBase
{
public:
    explicit RKSwellEditor (RKSwellProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> curve;
    std::unique_ptr<rkfx::ToggleSwitch> link;
    std::unique_ptr<rkfx::Knob> threshold, time, retrigger;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKSwellEditor)
};
