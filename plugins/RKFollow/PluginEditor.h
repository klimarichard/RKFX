#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "FollowEngine.h"

class RKFollowProcessor;

class RKFollowEditor : public rkfx::EditorBase
{
public:
    explicit RKFollowEditor (RKFollowProcessor&);

private:
    /** The note being sent, large, with the bend as a cents needle; the
        level against the Threshold and the expression range; the value sent. */
    class Display : public juce::Component
    {
    public:
        explicit Display (RKFollowEditor& o) : owner (o) { setInterceptsMouseClicks (false, false); }
        void setMeter (const FollowEngine::Meter&, float secondsSinceLast);
        void paint (juce::Graphics&) override;

    private:
        RKFollowEditor& owner;
        FollowEngine::Meter shown;
        float shownLevel = -70.0f;
    };

    RKFollowProcessor& follow;
    FollowEngine::Settings settings;
    Display display { *this };
    rkfx::ParameterWatcher watcher;

    std::unique_ptr<rkfx::ToggleSwitch> notes, expr;
    std::unique_ptr<rkfx::SegmentedSwitch> range, bend;
    std::unique_ptr<rkfx::Knob> threshold, transpose, velMin, velMax, retrigger, bendRange, glide, channel,
                                exprCc, exprFloor, exprCeiling, exprMin, exprMax, exprCurve, exprResponse;

    double lastMeterTime = 0.0;
    juce::TimedCallback meterTimer { [this] { updateMeters(); } };

    juce::Rectangle<int> displayArea() const;
    juce::Rectangle<int> trackingArea() const;
    juce::Rectangle<int> notesArea() const;
    juce::Rectangle<int> bendArea() const;
    juce::Rectangle<int> expressionArea() const;

    void onParameterChange();
    void updateMeters();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKFollowEditor)
};
