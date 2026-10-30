#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

/** RKMix's own controls: the console fader, the level meter and the strip
    colours. Everything else is the suite's (rk_gui). */
namespace MixGui
{
    /** The 8 strip colours (0 = none, a neutral gray). */
    juce::Colour stripColour (int index);
    juce::String stripColourName (int index);
    constexpr int kNumStripColours = 8;

    /** A console fader on a fader-law parameter: a long track with a dB
        scale, a cap, the value below. Drag the cap (Shift for fine), wheel
        for small steps, double-click for 0 dB, click the value to type one.
        @p capColour tints the cap (sends on fader). Repaints only on change. */
    class Fader : public juce::Component
    {
    public:
        explicit Fader (juce::RangedAudioParameter&);

        juce::RangedAudioParameter& getParameter() const noexcept { return param; }
        void setCapColour (juce::Colour c) { capColour = c; repaint(); }
        void setShowScale (bool show) { showScale = show; repaint(); }

        /** The track's vertical extent (for a meter beside it). */
        juce::Range<int> getTrackRange() const noexcept { return { trackArea.getY(), trackArea.getBottom() }; }

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    private:
        juce::RangedAudioParameter& param;
        juce::ParameterAttachment attachment;
        juce::Colour capColour;
        float normalised = 0.0f, dragNormalised = 0.0f, lastDragY = 0.0f;
        bool dragging = false, showScale = true;
        juce::Rectangle<int> trackArea, valueArea;
        juce::TextEditor valueEditor;

        float yFor (float n) const noexcept;
        void setNormalised (float value, bool asCompleteGesture);
        void commitValueEditor();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fader)
    };

    /** A peak meter, one or two bars, -60 to +6 dBFS on the fader law's scale
        (so it lines up with a fader beside it), with a hold line. The editor
        feeds it peaks from a timer; it falls back at about 24 dB/s. */
    class Meter : public juce::Component
    {
    public:
        explicit Meter (juce::Colour accentColour) : accent (accentColour) { setInterceptsMouseClicks (false, false); }

        void setNumBars (int bars) { numBars = juce::jlimit (1, 2, bars); repaint(); }
        /** @param secondsSinceLast  time since the previous call (fall back and hold) */
        void setPeaks (float left, float right, float secondsSinceLast);
        /** Draws only along this vertical range (the fader's track). */
        void setScaleRange (juce::Range<int> range) { scaleRange = range; repaint(); }

        void paint (juce::Graphics&) override;

    private:
        juce::Colour accent;
        int numBars = 1;
        std::array<float, 2> shownDb { -100.0f, -100.0f }, holdDb { -100.0f, -100.0f };
        std::array<float, 2> holdSeconds {};
        juce::Range<int> scaleRange;
    };
}
