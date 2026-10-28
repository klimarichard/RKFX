#include "RoutingPanel.h"

namespace rkboard
{

using namespace rkfx;

RoutingPanel::RoutingPanel (BoardProcessor& b, int s, Node n) : board (b), split (s), node (n)
{
    auto& routing = board.getRouting (split);
    const auto accent = Palette::boardAccent;

    if (node == Node::split)
    {
        modeSwitch = std::make_unique<SegmentedSwitch> (routing.param ("split"), accent,
                                                        juce::StringArray { "Full", "Frequency", "Stereo" }, "Mode");
        crossoverKnob = std::make_unique<Knob> (routing.param ("crossover"), "Crossover");
        addAndMakeVisible (*modeSwitch);
        addAndMakeVisible (*crossoverKnob);
    }
    else
    {
        levelA = std::make_unique<Knob> (routing.param ("level_a"), "Level");
        panA   = std::make_unique<Knob> (routing.param ("pan_a"), "Pan");
        levelB = std::make_unique<Knob> (routing.param ("level_b"), "Level");
        panB   = std::make_unique<Knob> (routing.param ("pan_b"), "Pan");
        invertB = std::make_unique<ToggleSwitch> (routing.param ("invert_b"), accent, "Invert");
        for (auto* c : std::initializer_list<juce::Component*> { levelA.get(), panA.get(), levelB.get(), panB.get(), invertB.get() })
            addAndMakeVisible (c);
    }

    removeButton.setTooltip ("Its paths' pedals (A, then B) join the chain in its place.");
    removeButton.onClick = [this] { if (onRemove) onRemove(); };
    addAndMakeVisible (removeButton);

    timerCallback();
    startTimerHz (10);
}

juce::String RoutingPanel::title() const
{
    auto text = juce::String (node == Node::split ? "Split" : "Merge");
    if (board.getNumSplits() > 1)
        text << " " << (split + 1);
    return text;
}

juce::Rectangle<int> RoutingPanel::card() const
{
    return getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth(), 620), 230).withY (0);
}

juce::String RoutingPanel::explanation() const
{
    if (! board.isSplitActive (split))
        return "Not in use: put a pedal on path A or B (drag one between the split and the merge) to split the signal.";

    if (node == Node::merge)
        return "Paths A and B are added back together here. An empty path passes its signal through.";

    switch (board.getRouting (split).getSplitMode())
    {
        case SplitMode::full:      return "Both paths get the full signal.";
        case SplitMode::frequency: return "Lows go to path A, highs to path B (24 dB/octave, they add back flat).";
        case SplitMode::stereo:    return "The left channel goes to path A, the right to path B.";
    }
    return {};
}

void RoutingPanel::paint (juce::Graphics& g)
{
    const auto area = card();
    drawPanel (g, area, Palette::panel, title());

    if (node == Node::merge)
    {
        auto inner = area.reduced (16, 30).withTrimmedBottom (34);
        const auto half = inner.getWidth() / 2;
        g.setColour (Palette::textDim);
        g.setFont (Fonts::bold (12.5f));
        g.drawText ("PATH A", inner.removeFromLeft (half).removeFromTop (18), juce::Justification::centred);
        g.drawText ("PATH B", inner.removeFromTop (18), juce::Justification::centred);
    }

    g.setColour (board.isSplitActive (split) ? Palette::textDim : Palette::warning);
    g.setFont (Fonts::regular (13.0f));
    g.drawFittedText (explanation(), area.reduced (16, 0).removeFromBottom (34), juce::Justification::centred, 2);
}

void RoutingPanel::resized()
{
    removeButton.setBounds (card().removeFromTop (30).removeFromRight (140).reduced (4, 4));
    auto area = card().reduced (16, 30).withTrimmedBottom (34);

    if (node == Node::split)
    {
        auto row = area.withSizeKeepingCentre (340 + Knob::kDefaultWidth + 24, Knob::kDefaultHeight);
        modeSwitch->setBounds (row.removeFromLeft (340).withSizeKeepingCentre (340, 52));
        row.removeFromLeft (24);
        crossoverKnob->setBounds (row);
    }
    else
    {
        area.removeFromTop (18);
        const auto half = area.getWidth() / 2;
        auto a = area.removeFromLeft (half).withSizeKeepingCentre (2 * Knob::kDefaultWidth + 8, Knob::kDefaultHeight);
        levelA->setBounds (a.removeFromLeft (Knob::kDefaultWidth));
        panA->setBounds (a.removeFromRight (Knob::kDefaultWidth));

        auto b = area.withSizeKeepingCentre (2 * Knob::kDefaultWidth + 92, Knob::kDefaultHeight);
        levelB->setBounds (b.removeFromLeft (Knob::kDefaultWidth));
        b.removeFromLeft (4);
        panB->setBounds (b.removeFromLeft (Knob::kDefaultWidth));
        invertB->setBounds (b.withSizeKeepingCentre (76, 28).withX (b.getRight() - 76));
    }
}

void RoutingPanel::timerCallback()
{
    if (crossoverKnob != nullptr)
        crossoverKnob->setDimmed (board.getRouting (split).getSplitMode() != SplitMode::frequency);
    repaint (card().removeFromBottom (40));
}

} // namespace rkboard
