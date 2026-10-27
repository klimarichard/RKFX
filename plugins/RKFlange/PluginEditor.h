#pragma once

#include "plugin/EditorBase.h"

class RKFlangeProcessor;

class RKFlangeEditor : public rkfx::EditorBase
{
public:
    explicit RKFlangeEditor (RKFlangeProcessor&);

private:
    std::unique_ptr<rkfx::ToggleSwitch> sync;
    std::unique_ptr<rkfx::Knob> rate, division, depth, manual, feedback, phase;
    std::unique_ptr<juce::ParameterAttachment> syncWatcher, depthWatcher;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKFlangeEditor)
};
