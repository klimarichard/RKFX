#pragma once

#include "plugin/EditorBase.h"
#include "gui/Fader.h"
#include "gui/ResponseCurve.h"

class RKGraphEQProcessor;

class RKGraphEQEditor : public rkfx::EditorBase
{
public:
    explicit RKGraphEQEditor (RKGraphEQProcessor&);

private:
    RKGraphEQProcessor& eq;
    rkfx::ResponseCurve curve;
    rkfx::ParameterWatcher watcher;

    std::array<std::vector<std::unique_ptr<rkfx::Fader>>, 3> faders;   // one set per band count
    std::unique_ptr<rkfx::SegmentedSwitch> bandsSwitch;
    juce::TextButton flatButton { "Flat" };
    std::unique_ptr<juce::ParameterAttachment> modeWatcher;

    juce::Rectangle<int> curveArea() const;
    juce::Rectangle<int> faderArea() const;

    void showMode (int modeIndex);
    void flatten();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKGraphEQEditor)
};
