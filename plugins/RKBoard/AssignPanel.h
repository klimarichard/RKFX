#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "BoardProcessor.h"

namespace rkboard
{

/** The 32 assignable parameters in two columns: what each drives, its range
    (in the knob's own units) and invert. Knobs are assigned by right-clicking
    them in a pedal's controls. */
class AssignPanel : public juce::Component
{
public:
    explicit AssignPanel (BoardProcessor&);

    /** Shows the board's assignments (after any change). */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row : public juce::Component
    {
        Row (BoardProcessor&, int slot);
        void refresh();
        void paint (juce::Graphics&) override;
        void resized() override;

        BoardProcessor& board;
        const int slot;
        juce::Label target;
        juce::Slider minSlider, maxSlider;
        juce::ToggleButton invertButton { "Inv" };
        juce::TextButton clearButton { juce::String::fromUTF8 ("\xc3\x97") };
        bool assigned = false;

        void rangeChanged();
    };

    BoardProcessor& board;
    std::vector<std::unique_ptr<Row>> rows;

    static constexpr int kRowHeight = 26;
};

} // namespace rkboard
