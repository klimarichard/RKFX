#pragma once

#include "plugin/EditorBase.h"

class RKVerbProcessor;

class RKVerbEditor : public rkfx::EditorBase
{
public:
    explicit RKVerbEditor (RKVerbProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> type;
    std::unique_ptr<rkfx::ToggleSwitch> sync, freeze;
    std::unique_ptr<rkfx::Knob> decay, size, predelay, division, damping, lowCut, highCut, mod, width, shimmer, interval;
    std::unique_ptr<juce::ParameterAttachment> syncWatcher, shimmerWatcher;

    juce::Rectangle<int> reverbArea() const;
    juce::Rectangle<int> toneArea() const;
    juce::Rectangle<int> spaceArea() const;
    juce::Rectangle<int> shimmerArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKVerbEditor)
};
