#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

class RKUtilityProcessor;

class RKUtilityEditor : public rkfx::EditorBase
{
public:
    explicit RKUtilityEditor (RKUtilityProcessor&);

private:
    RKUtilityProcessor& utility;
    rkfx::ParameterWatcher watcher;

    std::unique_ptr<rkfx::Knob> gain, pan, width, bassMono, delayLeft, delayRight;
    std::unique_ptr<rkfx::ToggleSwitch> mute, swap, polLeft, polRight;
    std::unique_ptr<rkfx::SegmentedSwitch> channelMode;
    juce::Label delayInfo;

    juce::Rectangle<int> levelArea() const;
    juce::Rectangle<int> stereoArea() const;
    juce::Rectangle<int> alignArea() const;

    void updateDelayInfo();
    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKUtilityEditor)
};
