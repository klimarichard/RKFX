#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "Parameters.h"
#include "TriggerEngine.h"

#include <array>

class RKTriggerProcessor;

class RKTriggerEditor : public rkfx::EditorBase
{
public:
    explicit RKTriggerEditor (RKTriggerProcessor&);

private:
    /** Per trigger, the last few seconds scrolling right to left: the
        filtered level (shaded) against the threshold, and each hit as a bar
        as tall as its velocity. */
    class HitDisplay : public juce::Component
    {
    public:
        explicit HitDisplay (RKTriggerEditor& o) : owner (o) { setInterceptsMouseClicks (false, false); }
        void push (int trigger, float levelDb, int newHits, int velocity);
        void paint (juce::Graphics&) override;

        static constexpr int kPoints = 120;   // 4 s at 30 per second

    private:
        struct Point { float levelDb = -120.0f; int velocity = 0; };
        RKTriggerEditor& owner;
        std::array<std::array<Point, kPoints>, TriggerParams::kTriggers> history {};
        int next = 0;
        friend class RKTriggerEditor;
    };

    /** "Trigger 1 | Trigger 2": which trigger's controls are shown. */
    class Tabs : public juce::Component
    {
    public:
        explicit Tabs (RKTriggerEditor& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        RKTriggerEditor& owner;
    };

    struct TriggerControls
    {
        std::unique_ptr<rkfx::Knob> threshold, hpf, lpf, scan, mask, retrigger, dynamics, curve, velMin, velMax, note, channel, length;
    };

    RKTriggerProcessor& trigger;
    TriggerEngine::Settings settings;
    HitDisplay display { *this };
    Tabs tabs { *this };
    rkfx::ParameterWatcher watcher;

    std::array<TriggerControls, TriggerParams::kTriggers> controls;
    std::unique_ptr<rkfx::SegmentedSwitch> mode, audio;
    std::unique_ptr<rkfx::Knob> crosstalk;

    std::array<int, TriggerParams::kTriggers> lastHits {};
    juce::TimedCallback meterTimer { [this] { updateMeters(); } };

    int selected() const;
    void select (int trigger);

    juce::Rectangle<int> displayArea() const;
    juce::Rectangle<int> tabsArea() const;
    juce::Rectangle<int> detectionArea() const;
    juce::Rectangle<int> velocityArea() const;
    juce::Rectangle<int> noteArea() const;

    void onParameterChange();
    void updateMeters();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKTriggerEditor)
};
