#include "Fader.h"

#include "LookAndFeel.h"

namespace rkfx
{

namespace
{
    constexpr float kFineFactor = 0.1f;
    constexpr float kWheelStep = 0.01f;
    constexpr int kTextHeight = 16;
}

Fader::Fader (juce::RangedAudioParameter& p, const juce::String& labelText)
    : param (p),
      label (labelText),
      attachment (p, [this] (float denormalised)
                  {
                      normalised = param.convertTo0to1 (denormalised);
                      repaint();
                  })
{
    valueEditor.setJustification (juce::Justification::centred);
    valueEditor.setFont (Fonts::regular (11.0f));
    valueEditor.setSelectAllWhenFocused (true);
    valueEditor.onReturnKey = [this] { commitValueEditor(); };
    valueEditor.onEscapeKey = [this] { valueEditor.setVisible (false); repaint(); };
    valueEditor.onFocusLost = [this] { valueEditor.setVisible (false); repaint(); };
    addChildComponent (valueEditor);

    attachment.sendInitialUpdate();
}

void Fader::setCompactValue (bool shouldBeCompact)
{
    compact = shouldBeCompact;
    repaint();
}

void Fader::resized()
{
    auto bounds = getLocalBounds();
    valueArea = bounds.removeFromTop (kTextHeight);
    labelArea = bounds.removeFromBottom (kTextHeight);
    trackArea = bounds.reduced (0, 6);
    valueEditor.setBounds (valueArea.expanded (8, 0).constrainedWithin (getLocalBounds()));
}

juce::String Fader::valueText() const
{
    const auto value = param.convertFrom0to1 (normalised);
    if (! compact)
        return param.getCurrentValueAsText();

    const auto whole = juce::roundToInt (value);
    return (whole > 0 ? "+" : "") + juce::String (whole);
}

void Fader::paint (juce::Graphics& g)
{
    const auto track = trackArea.toFloat();
    const auto centreX = track.getCentreX();
    const auto yFor = [&] (float n) { return track.getBottom() - n * track.getHeight(); };

    // Track, with a centre mark for bipolar ranges
    g.setColour (Palette::knobBody);
    g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, track.getHeight()).withCentre (track.getCentre()), 2.0f);

    const auto& range = param.getNormalisableRange();
    const auto bipolar = range.start < 0.0f && range.end > 0.0f;
    const auto zero = bipolar ? range.convertTo0to1 (0.0f) : 0.0f;

    g.setColour (Palette::outline);
    if (bipolar)
        g.drawHorizontalLine ((int) yFor (zero), track.getX() + 2.0f, track.getRight() - 2.0f);

    // Fill from zero to the value
    const auto top = yFor (juce::jmax (zero, normalised));
    const auto bottom = yFor (juce::jmin (zero, normalised));
    g.setColour (Palette::knobTrack);
    g.fillRect (juce::Rectangle<float> (centreX - 2.0f, top, 4.0f, bottom - top));

    // Thumb
    const auto thumbWidth = juce::jmin (track.getWidth() - 2.0f, 26.0f);
    g.setColour (Palette::text);
    g.fillRoundedRectangle (juce::Rectangle<float> (thumbWidth, 6.0f).withCentre ({ centreX, yFor (normalised) }), 2.0f);

    g.setFont (Fonts::regular (compact ? 10.5f : 11.5f));
    if (! valueEditor.isVisible())
    {
        g.setColour (Palette::text);
        g.drawFittedText (valueText(), valueArea, juce::Justification::centred, 1, 0.8f);
    }

    g.setColour (Palette::textDim);
    g.drawFittedText (label, labelArea, juce::Justification::centred, 1, 0.7f);
}

//==============================================================================
void Fader::setNormalised (float value, bool asCompleteGesture)
{
    const auto denormalised = param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, value));
    if (asCompleteGesture)
        attachment.setValueAsCompleteGesture (denormalised);
    else
        attachment.setValueAsPartOfGesture (denormalised);
}

void Fader::mouseDown (const juce::MouseEvent& e)
{
    if (valueArea.contains (e.getPosition()) && e.getNumberOfClicks() == 1)
    {
        valueEditor.setText (param.getCurrentValueAsText(), false);
        valueEditor.setVisible (true);
        valueEditor.grabKeyboardFocus();
        repaint();
        return;
    }

    dragging = true;
    dragNormalised = normalised;
    lastDragY = e.position.y;
    attachment.beginGesture();
}

void Fader::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    // Incremental, so pressing or releasing Shift mid-drag never jumps.
    const auto delta = (lastDragY - e.position.y) / juce::jmax (40.0f, (float) trackArea.getHeight());
    lastDragY = e.position.y;

    dragNormalised = juce::jlimit (0.0f, 1.0f, dragNormalised + delta * (e.mods.isShiftDown() ? kFineFactor : 1.0f));
    setNormalised (dragNormalised, false);
}

void Fader::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        dragging = false;
        attachment.endGesture();
    }
}

void Fader::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! valueArea.contains (e.getPosition()))
        setNormalised (param.getDefaultValue(), true);
}

void Fader::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto direction = (wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
    if (std::abs (direction) > 0.0f)
        setNormalised (normalised + (direction > 0.0f ? 1.0f : -1.0f) * kWheelStep * (e.mods.isShiftDown() ? kFineFactor : 1.0f), true);
}

void Fader::commitValueEditor()
{
    const auto text = valueEditor.getText().trim();
    valueEditor.setVisible (false);
    if (text.isNotEmpty())
        setNormalised (param.getValueForText (text), true);
    repaint();
}

} // namespace rkfx
