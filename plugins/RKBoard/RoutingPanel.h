#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "BoardProcessor.h"
#include "gui/Knob.h"
#include "gui/Switches.h"

namespace rkboard
{

/** The split's or the merge's settings, shown below the board when its node
    is clicked. Same knobs and switches as the pedals. */
class RoutingPanel : public juce::Component,
                     private juce::Timer
{
public:
    enum class Node { split, merge };

    RoutingPanel (BoardProcessor&, Node);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BoardProcessor& board;
    const Node node;

    // Split
    std::unique_ptr<rkfx::SegmentedSwitch> modeSwitch;
    std::unique_ptr<rkfx::Knob> crossoverKnob;

    // Merge
    std::unique_ptr<rkfx::Knob> levelA, panA, levelB, panB;
    std::unique_ptr<rkfx::ToggleSwitch> invertB;

    juce::Rectangle<int> card() const;
    juce::String explanation() const;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingPanel)
};

} // namespace rkboard
