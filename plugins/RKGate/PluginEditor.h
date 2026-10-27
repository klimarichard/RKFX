#pragma once

#include "plugin/EditorBase.h"
#include "gui/GainReductionMeter.h"
#include "gui/ResponseCurve.h"

class RKGateProcessor;

class RKGateEditor : public rkfx::EditorBase
{
public:
    explicit RKGateEditor (RKGateProcessor&);

private:
    RKGateProcessor& gate;

    rkfx::GainReductionMeter meter;
    rkfx::ParameterWatcher watcher;

    std::unique_ptr<rkfx::Knob> threshold, ratio, range, hysteresis, attack, hold, release, keyHpf, keyLpf;
    std::unique_ptr<rkfx::SegmentedSwitch> mode, source;
    std::unique_ptr<rkfx::ToggleSwitch> listen, link;

    double lastMeterTime = 0.0;
    juce::TimedCallback meterTimer { [this] { updateMeter(); } };

    juce::Rectangle<int> meterSection() const;
    juce::Rectangle<int> gateSection() const;
    juce::Rectangle<int> timingSection() const;
    juce::Rectangle<int> keySection() const;

    void onParameterChange();
    void updateMeter();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKGateEditor)
};
