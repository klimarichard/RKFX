#pragma once

#include "plugin/EditorBase.h"

class RKAcousticProcessor;

class RKAcousticEditor : public rkfx::EditorBase
{
public:
    explicit RKAcousticEditor (RKAcousticProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> voicing;
    std::unique_ptr<rkfx::ToggleSwitch> phase;
    std::unique_ptr<rkfx::Knob> body, quack, highPass, notch, compress, low, mid, midFreq, treble, air;

    juce::Rectangle<int> pickupArea() const;
    juce::Rectangle<int> eqArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKAcousticEditor)
};
