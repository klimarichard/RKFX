#pragma once

#include "plugin/EditorBase.h"

class RKStrumProcessor;

class RKStrumEditor : public rkfx::EditorBase
{
public:
    explicit RKStrumEditor (RKStrumProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> direction;
    std::unique_ptr<rkfx::Knob> speed, window, ramp, humanize;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKStrumEditor)
};
