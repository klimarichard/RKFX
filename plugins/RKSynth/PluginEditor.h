#pragma once

#include "plugin/EditorBase.h"

class RKSynthProcessor;

class RKSynthEditor : public rkfx::EditorBase
{
public:
    explicit RKSynthEditor (RKSynthProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> range, wave, octave;
    std::unique_ptr<rkfx::Knob> sub, glide, dynamics, threshold, cutoff, resonance, envAmount, decay;

    juce::Rectangle<int> oscArea() const;
    juce::Rectangle<int> filterArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKSynthEditor)
};
