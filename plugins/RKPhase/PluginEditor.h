#pragma once

#include "plugin/EditorBase.h"

class RKPhaseProcessor;

class RKPhaseEditor : public rkfx::EditorBase
{
public:
    explicit RKPhaseEditor (RKPhaseProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> mode, stages;
    std::unique_ptr<rkfx::ToggleSwitch> sync;
    std::unique_ptr<rkfx::Knob> centre, feedback, depth, rate, division, phase,
                                envAmount, envSens, envAttack, envRelease;
    std::unique_ptr<juce::ParameterAttachment> syncWatcher, depthWatcher, envWatcher, modeWatcher;

    juce::Rectangle<int> phaserArea() const;
    juce::Rectangle<int> lfoArea() const;
    juce::Rectangle<int> envelopeArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKPhaseEditor)
};
