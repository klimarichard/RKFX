#pragma once

#include "plugin/EditorBase.h"

class RKChorusProcessor;

class RKChorusEditor : public rkfx::EditorBase
{
public:
    explicit RKChorusEditor (RKChorusProcessor&);

private:
    std::unique_ptr<rkfx::ToggleSwitch> sync;
    std::unique_ptr<rkfx::Knob> voices, rate, division, depth, delay, width, tone;
    std::unique_ptr<juce::ParameterAttachment> syncWatcher;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKChorusEditor)
};
