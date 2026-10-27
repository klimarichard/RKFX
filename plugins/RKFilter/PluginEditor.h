#pragma once

#include "plugin/EditorBase.h"

class RKFilterProcessor;

class RKFilterEditor : public rkfx::EditorBase
{
public:
    explicit RKFilterEditor (RKFilterProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> mode, slope, lfoShape;
    std::unique_ptr<rkfx::ToggleSwitch> lfoSync;
    std::unique_ptr<rkfx::Knob> cutoff, resonance,
                                envAmount, envSens, envAttack, envRelease,
                                lfoDepth, lfoRate, lfoDivision, lfoPhase;

    // Rate shows as a note division while synced; envelope and LFO controls
    // dim while their amount/depth is 0.
    std::unique_ptr<juce::ParameterAttachment> syncWatcher, envWatcher, lfoWatcher;

    juce::Rectangle<int> filterArea() const;
    juce::Rectangle<int> envelopeArea() const;
    juce::Rectangle<int> lfoArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKFilterEditor)
};
