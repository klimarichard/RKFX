#pragma once

#include "plugin/EditorBase.h"

#include "HoldEngine.h"

class RKHoldProcessor;

class RKHoldEditor : public rkfx::EditorBase
{
public:
    explicit RKHoldEditor (RKHoldProcessor&);

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

    /** The held layers: what each holds (a note or a texture) and its level as it fades. */
    class LayerDisplay : public juce::Component
    {
    public:
        explicit LayerDisplay (RKHoldEditor& o) : owner (o) { setInterceptsMouseClicks (false, false); }
        void update();
        void paint (juce::Graphics&) override;

    private:
        RKHoldEditor& owner;
        std::array<HoldEngine::LayerInfo, HoldEngine::kLayers> shown {};
    };

    RKHoldProcessor& holdProcessor;
    LayerDisplay display { *this };
    Footswitch holdButton, clearButton;

    std::unique_ptr<rkfx::SegmentedSwitch> mode, range, holdMode, layers, octave;
    std::unique_ptr<rkfx::Knob> movement, tone, width, attack, release, holdLevel, dryLevel;

    juce::TimedCallback meterTimer { [this] { display.update(); } };

    juce::Rectangle<int> holdArea() const;
    juce::Rectangle<int> captureArea() const;
    juce::Rectangle<int> soundArea() const;
    juce::Rectangle<int> envelopeArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKHoldEditor)
};
