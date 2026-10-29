#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "EchoEngine.h"

class RKEchoProcessor;

class RKEchoEditor : public rkfx::EditorBase
{
public:
    explicit RKEchoEditor (RKEchoProcessor&);

private:
    /** The coming repeats of the last note played (or C3): a bar per repeat
        along the echo times, as tall as its velocity, labelled with its note. */
    class Preview : public juce::Component
    {
    public:
        explicit Preview (RKEchoEditor& o) : owner (o) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;

    private:
        RKEchoEditor& owner;
    };

    RKEchoProcessor& echo;
    EchoEngine::Settings settings;
    Preview preview { *this };
    rkfx::ParameterWatcher watcher;
    int shownNote = -2;

    std::unique_ptr<rkfx::ToggleSwitch> sync, scaleSteps, original;
    std::unique_ptr<rkfx::SegmentedSwitch> length;
    std::unique_ptr<rkfx::Knob> time, division, repeats, swing, feedback, velMin, gate, step, key, scale, channel, alternate;

    juce::TimedCallback noteTimer { [this] { checkNote(); } };

    juce::Rectangle<int> previewArea() const;
    juce::Rectangle<int> sectionArea (int index) const;

    void onParameterChange();
    void checkNote();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKEchoEditor)
};
