#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "RideGraph.h"

class RKRideProcessor;

class RKRideEditor : public rkfx::EditorBase
{
public:
    explicit RKRideEditor (RKRideProcessor&);

private:
    RKRideProcessor& ride;

    RideGraph graph;
    RideFader fader;
    rkfx::ParameterWatcher watcher;

    std::unique_ptr<rkfx::Knob> target, maxBoost, maxCut, speed, floor, returnAfter, above;
    std::unique_ptr<rkfx::SegmentedSwitch> mode;

    juce::TimedCallback meterTimer { [this] { updateMeters(); } };

    juce::Rectangle<int> graphSection() const;
    juce::Rectangle<int> riderSection() const;

    void onParameterChange();
    void updateMeters();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKRideEditor)
};
