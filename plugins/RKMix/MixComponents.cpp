#include "MixComponents.h"

#include "MixLayout.h"
#include "gui/LookAndFeel.h"
#include "gui/Switches.h"

namespace MixGui
{

namespace
{
    constexpr float kPixelsForFullRange = 300.0f;
    constexpr float kFineFactor = 0.1f;
    constexpr float kWheelStep = 0.01f;
    constexpr int kValueHeight = 18;
    constexpr int kCapHeight = 26;
    constexpr float kMeterFloorDb = -60.0f;
    constexpr float kMeterFallDbPerSecond = 24.0f;
    constexpr float kHoldSeconds = 1.5f;

    const std::array<juce::Colour, kNumStripColours> kColours {
        juce::Colour (0xff6e737b),   // none
        juce::Colour (0xffd4524a),   // red
        juce::Colour (0xffe8913a),   // orange
        juce::Colour (0xffe3c341),   // yellow
        juce::Colour (0xff5cb85c),   // green
        juce::Colour (0xff45b5c4),   // cyan
        juce::Colour (0xff5b8ee0),   // blue
        juce::Colour (0xffb06ad9),   // purple
    };
    const std::array<const char*, kNumStripColours> kColourNames { "None", "Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple" };

    /** The fader law's position of a level, for meters on the same scale. */
    float meterPosition (float db) { return mix::faderPositionOf (juce::jmax (db, kMeterFloorDb)); }
}

juce::Colour stripColour (int index)    { return kColours[(size_t) juce::jlimit (0, kNumStripColours - 1, index)]; }
juce::String stripColourName (int index) { return kColourNames[(size_t) juce::jlimit (0, kNumStripColours - 1, index)]; }

//==============================================================================
Fader::Fader (juce::RangedAudioParameter& p)
    : param (p),
      attachment (p, [this] (float denormalised)
                  {
                      normalised = param.convertTo0to1 (denormalised);
                      repaint();
                  }),
      capColour (rkfx::Palette::knobTrack)
{
    rkfx::tagParameter (*this, p.getParameterID());
    setRepaintsOnMouseActivity (false);

    valueEditor.setJustification (juce::Justification::centred);
    valueEditor.setFont (rkfx::Fonts::regular (11.0f));
    valueEditor.setSelectAllWhenFocused (true);
    valueEditor.onReturnKey = [this] { commitValueEditor(); };
    valueEditor.onEscapeKey = [this] { valueEditor.setVisible (false); repaint(); };
    valueEditor.onFocusLost = [this] { valueEditor.setVisible (false); repaint(); };
    addChildComponent (valueEditor);

    attachment.sendInitialUpdate();
}

void Fader::resized()
{
    auto bounds = getLocalBounds();
    valueArea = bounds.removeFromBottom (kValueHeight);
    trackArea = bounds.reduced (0, kCapHeight / 2 + 2);
    valueEditor.setBounds (valueArea.expanded (6, 0).constrainedWithin (getLocalBounds()));
}

float Fader::yFor (float n) const noexcept
{
    return (float) trackArea.getBottom() - n * (float) trackArea.getHeight();
}

void Fader::paint (juce::Graphics& g)
{
    const auto trackX = showScale ? (float) getWidth() * 0.62f : (float) getWidth() * 0.5f;

    // Scale
    if (showScale)
    {
        g.setFont (rkfx::Fonts::regular (9.5f));
        const auto bottomY = yFor (mix::faderPositionOf (-100.0f));
        auto lastY = -100.0f;
        for (const auto db : { 10.0f, 0.0f, -10.0f, -20.0f, -40.0f, -60.0f, -100.0f })
        {
            // A short fader skips the marks that would crowd the one above, or -inf below.
            const auto y = yFor (mix::faderPositionOf (db));
            if (db > -100.0f && (y - lastY < 14.0f || bottomY - y < 11.0f))
                continue;
            lastY = y;
            g.setColour (juce::exactlyEqual (db, 0.0f) ? rkfx::Palette::text : rkfx::Palette::textDim);
            g.drawText (db <= -100.0f ? juce::String ("-inf") : juce::String ((int) db), 0, (int) y - 6, (int) trackX - 9, 12,
                        juce::Justification::centredRight);
            g.setColour (rkfx::Palette::outline);
            g.drawHorizontalLine ((int) y, trackX - 7.0f, trackX - 3.0f);
        }
    }

    // Track
    g.setColour (rkfx::Palette::knobBody);
    g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, (float) trackArea.getHeight()).withCentre ({ trackX, (float) trackArea.getCentreY() }), 2.0f);

    // Cap
    const auto capWidth = juce::jmin (28.0f, (float) getWidth() * (showScale ? 0.62f : 0.8f));
    const auto cap = juce::Rectangle<float> (capWidth, (float) kCapHeight).withCentre ({ trackX, yFor (normalised) });
    g.setColour (capColour.darker (0.2f));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (capColour.brighter (0.3f));
    g.drawRoundedRectangle (cap.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (rkfx::Palette::text);
    g.fillRect (cap.withSizeKeepingCentre (capWidth - 6.0f, 2.0f));

    // Value
    if (! valueEditor.isVisible())
    {
        const auto db = param.convertFrom0to1 (normalised);
        const auto text = db <= mix::kFaderMinDb + 0.001f ? juce::String ("-inf")
                                                          : (db > 0.05f ? "+" : "") + juce::String (std::abs (db) < 0.05f ? 0.0f : db, 1);
        g.setColour (rkfx::Palette::text);
        g.setFont (rkfx::Fonts::regular (11.5f));
        g.drawText (text, valueArea, juce::Justification::centred);
    }
}

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
    if (e.mods.isPopupMenu())
        return;   // right-click is for context menus (assign)

    if (valueArea.contains (e.getPosition()))
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

    const auto delta = (lastDragY - e.position.y) / juce::jmax (kPixelsForFullRange * 0.5f, (float) trackArea.getHeight());
    lastDragY = e.position.y;
    dragNormalised = juce::jlimit (0.0f, 1.0f, dragNormalised + delta * (e.mods.isShiftDown() ? kFineFactor : 1.0f));
    setNormalised (dragNormalised, false);
}

void Fader::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        attachment.endGesture();
    dragging = false;
}

void Fader::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! valueArea.contains (e.getPosition()))
        setNormalised (param.getDefaultValue(), true);
}

void Fader::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto direction = (wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
    if (direction != 0.0f)
        setNormalised (normalised + (direction > 0.0f ? kWheelStep : -kWheelStep), true);
}

void Fader::commitValueEditor()
{
    valueEditor.setVisible (false);
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getValueForText (valueEditor.getText())));
    repaint();
}

//==============================================================================
OptionList::OptionList (juce::RangedAudioParameter& p, juce::StringArray optionLabels, juce::Colour accentColour)
    : labels (std::move (optionLabels)),
      accent (accentColour),
      attachment (p, [this] (float value)
                  {
                      selected = juce::roundToInt (value);
                      repaint();
                  })
{
    rkfx::tagParameter (*this, p.getParameterID());
    attachment.sendInitialUpdate();
}

void OptionList::paint (juce::Graphics& g)
{
    const auto h = rowHeight();
    g.setFont (rkfx::Fonts::regular (10.5f));
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto row = juce::Rectangle<int> (0, i * h, getWidth(), h).reduced (0, 1);
        if (i == selected)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (row.toFloat(), rkfx::Style::kCornerRadius);
        }
        g.setColour (i == selected ? rkfx::Palette::onAccent (accent) : rkfx::Palette::textDim);
        g.drawFittedText (labels[i], row.reduced (3, 0), juce::Justification::centred, 1);
        if (i == marked)
        {
            g.setColour (i == selected ? rkfx::Palette::onAccent (accent) : accent);
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ (float) row.getRight() - 4.0f, (float) row.getCentreY() }));
        }
    }
}

void OptionList::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    const auto option = juce::jlimit (0, labels.size() - 1, e.y / rowHeight());
    attachment.setValueAsCompleteGesture ((float) option);
}

//==============================================================================
void Meter::setPeaks (float left, float right, float secondsSinceLast)
{
    const float peaks[2] = { left, right };
    bool changed = false;
    for (int bar = 0; bar < numBars; ++bar)
    {
        const auto db = juce::Decibels::gainToDecibels (peaks[bar], -100.0f);
        auto shown = juce::jmax (db, shownDb[(size_t) bar] - kMeterFallDbPerSecond * secondsSinceLast);
        if (db >= holdDb[(size_t) bar])
        {
            holdDb[(size_t) bar] = db;
            holdSeconds[(size_t) bar] = 0.0f;
        }
        else if ((holdSeconds[(size_t) bar] += secondsSinceLast) > kHoldSeconds)
        {
            holdDb[(size_t) bar] = shown;
        }

        if (std::abs (shown - shownDb[(size_t) bar]) > 0.2f)
            changed = true;
        shownDb[(size_t) bar] = shown;
    }
    if (changed)
        repaint();
}

void Meter::paint (juce::Graphics& g)
{
    const auto range = scaleRange.isEmpty() ? juce::Range<int> (0, getHeight()) : scaleRange;
    const auto height = (float) range.getLength();
    const auto barWidth = numBars == 2 ? ((float) getWidth() - 1.0f) * 0.5f : (float) getWidth();
    const auto yFor = [&] (float db) { return (float) range.getEnd() - meterPosition (db) * height; };

    for (int bar = 0; bar < numBars; ++bar)
    {
        const auto x = (float) bar * (barWidth + 1.0f);
        const auto column = juce::Rectangle<float> (x, (float) range.getStart(), barWidth, height);
        g.setColour (rkfx::Palette::knobBody);
        g.fillRect (column);

        const auto db = shownDb[(size_t) bar];
        if (db > kMeterFloorDb)
        {
            const auto top = yFor (db);
            const auto zero = yFor (0.0f);
            g.setColour (accent);
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x, juce::jmax (top, zero), x + barWidth, column.getBottom()));
            if (top < zero)
            {
                g.setColour (rkfx::Palette::warning);
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x, top, x + barWidth, zero));
            }
        }

        const auto hold = holdDb[(size_t) bar];
        if (hold > kMeterFloorDb)
        {
            g.setColour (hold > 0.0f ? rkfx::Palette::warning : rkfx::Palette::text);
            g.fillRect (juce::Rectangle<float> (x, yFor (hold) - 1.0f, barWidth, 2.0f));
        }
    }
}

} // namespace MixGui
