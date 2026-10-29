#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "PolyPitchEngine.h"

class RKPolyPitchProcessor;

class RKPolyPitchEditor : public rkfx::EditorBase
{
public:
    explicit RKPolyPitchEditor (RKPolyPitchProcessor&);

private:
    /** An on-screen footswitch for a momentary parameter (down = on). */
    class Footswitch : public juce::TextButton
    {
    public:
        Footswitch (juce::RangedAudioParameter&, const juce::String& text);
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        juce::RangedAudioParameter& param;
    };

    /** The treadle from heel to toe with the position now, the shift now
        (large), and what the mode does to your own sound (the delay). */
    class PedalDisplay : public juce::Component
    {
    public:
        explicit PedalDisplay (RKPolyPitchEditor& o) : owner (o) { setInterceptsMouseClicks (false, false); }
        void update();
        void paint (juce::Graphics&) override;

    private:
        RKPolyPitchEditor& owner;
        float shift = 0.0f, position = 0.0f;
    };

    RKPolyPitchProcessor& poly;
    PolyPitchEngine::Settings settings;
    PedalDisplay display { *this };
    Footswitch momentary;
    rkfx::ParameterWatcher watcher;

    std::unique_ptr<rkfx::SegmentedSwitch> mode, range;
    std::unique_ptr<rkfx::ToggleSwitch> lowLatency;
    std::unique_ptr<rkfx::Knob> heel, toe, pedal, glide, rise, fall, fine, detune, voiceLevel, width, tone;

    juce::TimedCallback meterTimer { [this] { display.update(); } };

    juce::Rectangle<int> displayArea() const;
    juce::Rectangle<int> modeArea() const;
    juce::Rectangle<int> pedalArea() const;
    juce::Rectangle<int> momentaryArea() const;
    juce::Rectangle<int> voiceArea() const;

    void onParameterChange();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKPolyPitchEditor)
};
