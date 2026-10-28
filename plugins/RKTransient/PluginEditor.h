#pragma once

#include "plugin/EditorBase.h"

class RKTransientProcessor;

class RKTransientEditor : public rkfx::EditorBase
{
public:
    explicit RKTransientEditor (RKTransientProcessor&);

private:
    std::unique_ptr<rkfx::Knob> attack, sustain, attackTime, sustainTime, focusHpf, focusLpf;
    std::unique_ptr<rkfx::ToggleSwitch> listen, link;

    juce::Rectangle<int> shapeArea() const;
    juce::Rectangle<int> focusArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKTransientEditor)
};
