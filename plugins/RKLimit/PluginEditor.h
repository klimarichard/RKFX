#pragma once

#include "plugin/EditorBase.h"
#include "gui/GainReductionMeter.h"

class RKLimitProcessor;

class RKLimitEditor : public rkfx::EditorBase
{
public:
    explicit RKLimitEditor (RKLimitProcessor&);

private:
    RKLimitProcessor& limiter;
    rkfx::GainReductionMeter meter;

    std::unique_ptr<rkfx::Knob> input, ceiling, release, link;
    std::unique_ptr<rkfx::ToggleSwitch> releaseAuto;
    std::unique_ptr<rkfx::SegmentedSwitch> oversampling;
    std::unique_ptr<juce::ParameterAttachment> autoWatcher;

    double lastMeterTime = 0.0;
    juce::TimedCallback meterTimer { [this] { updateMeter(); } };

    juce::Rectangle<int> meterArea() const;
    juce::Rectangle<int> limiterArea() const;

    void updateMeter();
    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKLimitEditor)
};
