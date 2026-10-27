#pragma once

#include "plugin/EditorBase.h"

class RKDoubleProcessor;

class RKDoubleEditor : public rkfx::EditorBase
{
public:
    explicit RKDoubleEditor (RKDoubleProcessor&);

private:
    std::unique_ptr<rkfx::Knob> delay, drift, driftSpeed, tone, spread, level;
    std::unique_ptr<juce::ParameterAttachment> driftWatcher;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKDoubleEditor)
};
