#pragma once

#include "plugin/EditorBase.h"

class RKHarmonizeProcessor;

class RKHarmonizeEditor : public rkfx::EditorBase
{
public:
    explicit RKHarmonizeEditor (RKHarmonizeProcessor&);

private:
    std::unique_ptr<rkfx::Knob> key, scale, voice1, voice2, velocity;
    std::unique_ptr<rkfx::ToggleSwitch> original;

    juce::Rectangle<int> keyArea() const;
    juce::Rectangle<int> harmonyArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKHarmonizeEditor)
};
