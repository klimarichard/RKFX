#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "BoardProcessor.h"
#include "gui/Knob.h"
#include "gui/Switches.h"

namespace rkboard
{

/** A split's or merge's settings, shown below the board when its node is
    clicked. Same knobs and switches as the pedals (right-click one to assign
    it or make it scene-controlled), and a button to remove the split. */
class RoutingPanel : public juce::Component,
                     private juce::Timer
{
public:
    enum class Node { split, merge };

    RoutingPanel (BoardProcessor&, int split, Node);

    int getSplit() const noexcept { return split; }

    /** "Remove this split" was clicked. */
    std::function<void()> onRemove;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BoardProcessor& board;
    const int split;
    const Node node;
    juce::TextButton removeButton { "Remove this split" };

    // Split
    std::unique_ptr<rkfx::SegmentedSwitch> modeSwitch;
    std::unique_ptr<rkfx::Knob> crossoverKnob;

    // Merge
    std::unique_ptr<rkfx::Knob> levelA, panA, levelB, panB;
    std::unique_ptr<rkfx::ToggleSwitch> invertB;

    juce::Rectangle<int> card() const;
    juce::String explanation() const;
    juce::String title() const;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingPanel)
};

} // namespace rkboard
