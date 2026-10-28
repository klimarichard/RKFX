#pragma once

#include "plugin/EditorBase.h"
#include "gui/GainReductionMeter.h"

class RKDeEssProcessor;

class RKDeEssEditor : public rkfx::EditorBase
{
public:
    explicit RKDeEssEditor (RKDeEssProcessor&);

private:
    RKDeEssProcessor& deEsser;
    rkfx::GainReductionMeter meter;

    std::unique_ptr<rkfx::SegmentedSwitch> detect, mode;
    std::unique_ptr<rkfx::Knob> freq, threshold, thresholdRel, range;
    std::unique_ptr<rkfx::ToggleSwitch> listen, link;
    std::unique_ptr<juce::ParameterAttachment> detectWatcher;

    double lastMeterTime = 0.0;
    juce::TimedCallback meterTimer { [this] { updateMeter(); } };

    juce::Rectangle<int> meterArea() const;
    juce::Rectangle<int> mainArea() const;

    void updateMeter();
    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKDeEssEditor)
};
