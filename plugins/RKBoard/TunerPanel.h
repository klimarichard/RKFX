#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "BoardProcessor.h"
#include "PitchDetector.h"

namespace rkboard
{

/** The tuner (standalone): note, octave and how many cents off, read from a
    copy of the input 20 times a second. The audio path never waits for it. */
class TunerPanel : public juce::Component,
                   private juce::Timer
{
public:
    explicit TunerPanel (BoardProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BoardProcessor& board;
    PitchDetector detector;
    std::vector<float> window;

    float hz = 0.0f;              // 0 = no clear pitch
    float shownCents = 0.0f;      // smoothed for a calm needle
    int quietTicks = 0;

    juce::Label referenceLabel;
    juce::Slider referenceSlider;
    juce::ToggleButton muteButton { "Mute output while tuning" };
    juce::TextButton closeButton { "Close" };

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerPanel)
};

} // namespace rkboard
