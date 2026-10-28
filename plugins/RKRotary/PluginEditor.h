#pragma once

#include "plugin/EditorBase.h"

class RKRotaryProcessor;

class RKRotaryEditor : public rkfx::EditorBase
{
public:
    explicit RKRotaryEditor (RKRotaryProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> speed, oversampling;
    std::unique_ptr<rkfx::Knob> slowRate, fastRate, hornAccel, drumAccel, balance, distance, width, drive;

    juce::Rectangle<int> rotorArea() const;
    juce::Rectangle<int> soundArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKRotaryEditor)
};
