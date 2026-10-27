#pragma once

#include "plugin/EditorBase.h"
#include "gui/GainReductionMeter.h"
#include "gui/ResponseCurve.h"

#include "TransferCurve.h"

class RKCompressProcessor;

class RKCompressEditor : public rkfx::EditorBase
{
public:
    explicit RKCompressEditor (RKCompressProcessor&);

private:
    RKCompressProcessor& comp;

    TransferCurve curve;
    rkfx::GainReductionMeter meter;
    rkfx::ParameterWatcher watcher;

    std::unique_ptr<rkfx::Knob> threshold, ratio, knee, attack, release, scHpf, link, makeup;
    std::unique_ptr<rkfx::ToggleSwitch> releaseAuto, makeupAuto, listen;
    std::unique_ptr<rkfx::SegmentedSwitch> detect, source;

    double lastMeterTime = 0.0;
    juce::TimedCallback meterTimer { [this] { updateMeters(); } };

    juce::Rectangle<int> curveSection() const;
    juce::Rectangle<int> compressorSection() const;
    juce::Rectangle<int> sidechainSection() const;

    void onParameterChange();
    void updateMeters();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKCompressEditor)
};
