#pragma once

#include "plugin/EditorBase.h"

class RKPreampProcessor;

class RKPreampEditor : public rkfx::EditorBase
{
public:
    explicit RKPreampEditor (RKPreampProcessor&);

private:
    std::unique_ptr<rkfx::SegmentedSwitch> voicing, cab, oversampling;
    std::unique_ptr<rkfx::ToggleSwitch> bright;
    std::unique_ptr<rkfx::Knob> gain, bass, middle, treble, presence;

    juce::Rectangle<int> ampArea() const;
    juce::Rectangle<int> cabArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKPreampEditor)
};
