#include "Knob.h"

#include "LookAndFeel.h"

namespace rkfx
{

namespace
{
    constexpr float kPixelsForFullRange = 200.0f;
    constexpr float kFineFactor = 0.1f;
    constexpr float kWheelStep = 0.01f;
    constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;   // 7 o'clock
    constexpr float kEndAngle   = juce::MathConstants<float>::pi * 2.75f;   // 5 o'clock
}

Knob::Knob (juce::RangedAudioParameter& p, const juce::String& labelText)
    : param (p),
      label (labelText.isNotEmpty() ? labelText : p.getName (32)),
      attachment (p, [this] (float denormalised)
                  {
                      normalised = param.convertTo0to1 (denormalised);
                      repaint();
                  })
{
    setRepaintsOnMouseActivity (false);

    valueEditor.setJustification (juce::Justification::centred);
    valueEditor.setFont (Fonts::regular (13.0f));
    valueEditor.setSelectAllWhenFocused (true);
    valueEditor.onReturnKey = [this] { commitValueEditor(); };
    valueEditor.onEscapeKey = [this] { valueEditor.setVisible (false); };
    valueEditor.onFocusLost = [this] { valueEditor.setVisible (false); };
    addChildComponent (valueEditor);

    attachment.sendInitialUpdate();
}

Knob::~Knob() = default;

void Knob::setDimmed (bool shouldBeDimmed)
{
    if (dimmed != shouldBeDimmed)
    {
        dimmed = shouldBeDimmed;
        repaint();
    }
}

void Knob::resized()
{
    auto bounds = getLocalBounds();
    labelArea = bounds.removeFromTop (18);
    valueArea = bounds.removeFromBottom (18);
    dialArea = bounds.reduced (4, 2);
    valueEditor.setBounds (valueArea.reduced (2, 0));
}

void Knob::paint (juce::Graphics& g)
{
    const auto alpha = dimmed ? 0.4f : 1.0f;

    g.setColour (Palette::textDim.withMultipliedAlpha (alpha));
    g.setFont (Fonts::regular (12.5f));
    g.drawFittedText (label, labelArea, juce::Justification::centred, 1);

    const auto size = (float) juce::jmin (dialArea.getWidth(), dialArea.getHeight());
    const auto dial = juce::Rectangle<float> (size, size).withCentre (dialArea.toFloat().getCentre());
    const auto centre = dial.getCentre();
    const auto radius = size * 0.5f;

    // Body
    g.setColour (Palette::knobBody.withMultipliedAlpha (alpha));
    g.fillEllipse (dial.reduced (3.0f));

    // Range track and indicator line
    const auto angle = kStartAngle + normalised * (kEndAngle - kStartAngle);
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f, kStartAngle, kEndAngle, true);
    g.setColour (Palette::knobTrack.withMultipliedAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto inner = juce::Point<float> (centre.x + (radius * 0.25f) * std::sin (angle),
                                           centre.y - (radius * 0.25f) * std::cos (angle));
    const auto outer = juce::Point<float> (centre.x + (radius - 6.0f) * std::sin (angle),
                                           centre.y - (radius - 6.0f) * std::cos (angle));
    g.setColour (Palette::text.withMultipliedAlpha (alpha));
    g.drawLine ({ inner, outer }, 2.5f);

    if (! valueEditor.isVisible())
    {
        g.setColour (Palette::text.withMultipliedAlpha (alpha));
        g.setFont (Fonts::regular (13.0f));
        g.drawFittedText (param.getCurrentValueAsText(), valueArea, juce::Justification::centred, 1);
    }
}

//==============================================================================
void Knob::setNormalised (float newNormalised, bool asCompleteGesture)
{
    const auto denormalised = param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, newNormalised));

    if (asCompleteGesture)
        attachment.setValueAsCompleteGesture (denormalised);
    else
        attachment.setValueAsPartOfGesture (denormalised);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (valueArea.contains (e.getPosition()) && e.getNumberOfClicks() == 1 && ! e.mods.isPopupMenu())
    {
        showValueEditor();
        return;
    }

    dragging = true;
    dragNormalised = normalised;
    lastDragY = e.position.y;
    attachment.beginGesture();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    // Incremental, so pressing or releasing Shift mid-drag never jumps.
    const auto delta = (lastDragY - e.position.y) / kPixelsForFullRange;
    lastDragY = e.position.y;

    dragNormalised = juce::jlimit (0.0f, 1.0f, dragNormalised + delta * (e.mods.isShiftDown() ? kFineFactor : 1.0f));
    setNormalised (dragNormalised, false);
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        dragging = false;
        attachment.endGesture();
    }
}

void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (valueArea.contains (e.getPosition()))
        return;

    setNormalised (param.getDefaultValue(), true);
}

void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto direction = (wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
    if (direction == 0.0f)
        return;

    auto step = kWheelStep * (e.mods.isShiftDown() ? kFineFactor : 1.0f);

    // Stepped parameters (choices, ints) move at least one step per notch.
    const auto numSteps = param.getNumSteps();
    if (numSteps > 1 && numSteps < 0x7fffffff)
        step = juce::jmax (step, 1.0f / (float) (numSteps - 1));

    setNormalised (normalised + (direction > 0.0f ? step : -step), true);
}

//==============================================================================
void Knob::showValueEditor()
{
    valueEditor.setText (param.getCurrentValueAsText(), false);
    valueEditor.setVisible (true);
    valueEditor.grabKeyboardFocus();
    repaint();
}

void Knob::commitValueEditor()
{
    const auto text = valueEditor.getText().trim();
    valueEditor.setVisible (false);

    if (text.isNotEmpty())
        setNormalised (param.getValueForText (text), true);

    repaint();
}

} // namespace rkfx
