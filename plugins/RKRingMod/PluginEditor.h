#pragma once

#include "plugin/EditorBase.h"

class RKRingModProcessor;

class RKRingModEditor : public rkfx::EditorBase
{
public:
    explicit RKRingModEditor (RKRingModProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> mode, wave, range;
    std::unique_ptr<rkfx::Knob> frequency, interval, division, lfoDepth, lfoRate;
    std::unique_ptr<juce::ParameterAttachment> modeWatcher, depthWatcher;

    juce::Rectangle<int> carrierArea() const;
    juce::Rectangle<int> lfoArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKRingModEditor)
};
