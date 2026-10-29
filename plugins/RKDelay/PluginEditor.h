#pragma once

#include "plugin/EditorBase.h"

class RKDelayProcessor;

class RKDelayEditor : public rkfx::EditorBase
{
public:
    explicit RKDelayEditor (RKDelayProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> mode, stereo;
    std::unique_ptr<rkfx::ToggleSwitch> sync;
    std::unique_ptr<rkfx::Knob> time, division, feedback, offset, lowCut, highCut, modDepth, modRate, duck, shimmer, interval;
    std::unique_ptr<juce::ParameterAttachment> syncWatcher, stereoWatcher, modWatcher, shimmerWatcher;

    juce::Rectangle<int> delayArea() const;
    juce::Rectangle<int> toneArea() const;
    juce::Rectangle<int> modulationArea() const;
    juce::Rectangle<int> shimmerArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKDelayEditor)
};
