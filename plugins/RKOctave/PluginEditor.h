#pragma once

#include "plugin/EditorBase.h"

class RKOctaveProcessor;

class RKOctaveEditor : public rkfx::EditorBase
{
public:
    explicit RKOctaveEditor (RKOctaveProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> range, voice;
    std::unique_ptr<rkfx::Knob> direct, octave1, octave2, tone, threshold;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKOctaveEditor)
};
