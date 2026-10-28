#include "AssignPanel.h"

#include "gui/LookAndFeel.h"

namespace rkboard
{

using namespace rkfx;

AssignPanel::Row::Row (BoardProcessor& b, int s) : board (b), slot (s)
{
    target.setFont (Fonts::regular (13.0f));
    target.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (target);

    for (auto* slider : { &minSlider, &maxSlider })
    {
        slider->setSliderStyle (juce::Slider::LinearBar);
        slider->setRange (0.0, 1.0);
        slider->setTextBoxIsEditable (false);
        slider->setColour (juce::Slider::trackColourId, Palette::knobTrack);
        slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider->setDoubleClickReturnValue (true, slider == &minSlider ? 0.0 : 1.0);
        slider->onValueChange = [this] { rangeChanged(); };
        addChildComponent (*slider);
    }
    minSlider.setTooltip ("The knob's value with the parameter at 0 (heel).");
    maxSlider.setTooltip ("The knob's value with the parameter at 100% (toe).");

    invertButton.setTooltip ("Swap the two ends.");
    invertButton.onClick = [this] { rangeChanged(); };
    addChildComponent (invertButton);

    clearButton.setTooltip ("Remove this assignment.");
    clearButton.onClick = [this] { board.clearAssignment (slot); };
    addChildComponent (clearButton);

    refresh();
}

void AssignPanel::Row::refresh()
{
    const auto info = board.getAssignment (slot);
    assigned = info.pedal != 0;

    target.setText (assigned ? board.describeAssignment (slot) : juce::String::fromUTF8 ("\xe2\x80\x94"), juce::dontSendNotification);
    target.setColour (juce::Label::textColourId, assigned ? Palette::text : Palette::textDim);

    for (auto* c : std::initializer_list<juce::Component*> { &minSlider, &maxSlider, &invertButton, &clearButton })
        c->setVisible (assigned);

    if (! assigned)
        return;

    // Value text in the knob's own units.
    auto* param = board.getPedal (info.pedal)->findParam (info.paramId);
    for (auto* slider : { &minSlider, &maxSlider })
        slider->textFromValueFunction = [param] (double v) { return param->getText ((float) v, 12); };

    minSlider.setValue (info.min, juce::dontSendNotification);
    maxSlider.setValue (info.max, juce::dontSendNotification);
    minSlider.updateText();
    maxSlider.updateText();
    invertButton.setToggleState (info.invert, juce::dontSendNotification);
}

void AssignPanel::Row::rangeChanged()
{
    board.setAssignmentRange (slot, (float) minSlider.getValue(), (float) maxSlider.getValue(), invertButton.getToggleState());
}

void AssignPanel::Row::paint (juce::Graphics& g)
{
    g.setColour (assigned ? Palette::text : Palette::textDim);
    g.setFont (Fonts::bold (13.0f));
    g.drawText (juce::String (slot + 1), getLocalBounds().removeFromLeft (24), juce::Justification::centredRight);
}

void AssignPanel::Row::resized()
{
    auto r = getLocalBounds().reduced (0, 2);
    r.removeFromLeft (30);
    clearButton.setBounds (r.removeFromRight (22));
    r.removeFromRight (4);
    invertButton.setBounds (r.removeFromRight (50));
    r.removeFromRight (4);
    const auto sliderWidth = juce::jmax (60, (r.getWidth() - 150) / 2);
    maxSlider.setBounds (r.removeFromRight (sliderWidth));
    r.removeFromRight (4);
    minSlider.setBounds (r.removeFromRight (sliderWidth));
    r.removeFromRight (6);
    target.setBounds (r);
}

//==============================================================================
AssignPanel::AssignPanel (BoardProcessor& b) : board (b)
{
    for (int slot = 0; slot < BoardProcessor::kNumAssigns; ++slot)
    {
        rows.push_back (std::make_unique<Row> (board, slot));
        addAndMakeVisible (*rows.back());
    }
}

void AssignPanel::refresh()
{
    for (auto& row : rows)
        row->refresh();
}

void AssignPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds(), Palette::panel, "Assignable parameters");

    g.setColour (Palette::textDim);
    g.setFont (Fonts::regular (13.0f));
    g.drawText ("Right-click a knob in a pedal to assign it. Host parameters Assign 1 to 32 then move it between Min and Max.",
                getLocalBounds().reduced (12, 0).removeFromBottom (30), juce::Justification::centredLeft);

    // Column headings
    auto area = getLocalBounds().reduced (12, 0).withTrimmedTop (28);
    const auto columnWidth = (area.getWidth() - 16) / 2;
    for (int column = 0; column < 2; ++column)
    {
        auto heading = area.withWidth (columnWidth).withX (area.getX() + column * (columnWidth + 16)).removeFromTop (18);
        heading.removeFromLeft (30);
        heading.removeFromRight (80);
        const auto sliderWidth = juce::jmax (60, (heading.getWidth() - 150) / 2);
        g.setFont (Fonts::regular (11.5f));
        g.drawText ("Max", heading.removeFromRight (sliderWidth), juce::Justification::centred);
        heading.removeFromRight (4);
        g.drawText ("Min", heading.removeFromRight (sliderWidth), juce::Justification::centred);
        g.drawText ("Knob", heading.withTrimmedRight (6), juce::Justification::centredLeft);
    }
}

void AssignPanel::resized()
{
    auto area = getLocalBounds().reduced (12, 0).withTrimmedTop (48).withTrimmedBottom (30);
    const auto columnWidth = (area.getWidth() - 16) / 2;
    const auto perColumn = BoardProcessor::kNumAssigns / 2;
    const auto rowHeight = juce::jmin (kRowHeight, area.getHeight() / perColumn);

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto column = i / perColumn, line = i % perColumn;
        rows[(size_t) i]->setBounds (area.getX() + column * (columnWidth + 16), area.getY() + line * rowHeight,
                                     columnWidth, rowHeight);
    }
}

} // namespace rkboard
