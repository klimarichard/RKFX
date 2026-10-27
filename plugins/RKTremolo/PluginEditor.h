#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

class RKTremoloProcessor;

class RKTremoloEditor : public rkfx::EditorBase
{
public:
    explicit RKTremoloEditor (RKTremoloProcessor&);

private:
    RKTremoloProcessor& trem;
    rkfx::ParameterWatcher watcher;

    std::unique_ptr<rkfx::SegmentedSwitch> mode, shape;
    std::unique_ptr<rkfx::ToggleSwitch> sync;
    std::unique_ptr<rkfx::Knob> depth, rate, division, smooth, phase, xover;

    void onParameterChange();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKTremoloEditor)
};
