#pragma once

#include "plugin/EditorBase.h"

class RKVibratoProcessor;

class RKVibratoEditor : public rkfx::EditorBase
{
public:
    explicit RKVibratoEditor (RKVibratoProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> shape;
    std::unique_ptr<rkfx::ToggleSwitch> sync;
    std::unique_ptr<rkfx::Knob> rate, division, depth, phase, rise;
    std::unique_ptr<juce::ParameterAttachment> syncWatcher;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKVibratoEditor)
};
