#include "LookAndFeel.h"

namespace rkfx
{

LookAndFeel::LookAndFeel (juce::Colour accentColour) : accent (accentColour)
{
    setColourScheme ({ Palette::background, Palette::panel, Palette::panel,
                       Palette::outline, Palette::text, accent,
                       Palette::onAccent (accent), Palette::knobBody, Palette::text });

    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);
    setColour (juce::TextButton::buttonColourId, Palette::knobBody);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, Palette::text);
    setColour (juce::TextButton::textColourOnId, Palette::onAccent (accent));
    setColour (juce::TextEditor::backgroundColourId, Palette::background);
    setColour (juce::TextEditor::textColourId, Palette::text);
    setColour (juce::TextEditor::outlineColourId, Palette::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, Palette::text);
    setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::onAccent (accent));
    setColour (juce::AlertWindow::backgroundColourId, Palette::panel);
    setColour (juce::AlertWindow::textColourId, Palette::text);
    setColour (juce::AlertWindow::outlineColourId, Palette::outline);
    setColour (juce::Label::textColourId, Palette::text);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::backgroundWhenEditingColourId, Palette::background);
    setColour (juce::Label::textWhenEditingColourId, Palette::text);
    setColour (juce::Label::outlineWhenEditingColourId, accent);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::textColourId, Palette::text);
    setColour (juce::ComboBox::backgroundColourId, Palette::knobBody);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, Palette::textDim);
    setColour (juce::ComboBox::focusedOutlineColourId, accent);
    setColour (juce::ToggleButton::textColourId, Palette::text);
    setColour (juce::ToggleButton::tickColourId, Palette::onAccent (accent));
    setColour (juce::ToggleButton::tickDisabledColourId, Palette::textDim);
    setColour (juce::Slider::backgroundColourId, Palette::knobTrack);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::thumbColourId, Palette::text);
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxBackgroundColourId, Palette::background);
    setColour (juce::Slider::textBoxOutlineColourId, Palette::outline);
    setColour (juce::ScrollBar::thumbColourId, Palette::knobTrack);
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                        bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = button.getToggleState() ? accent : backgroundColour;

    if (isButtonDown)
        fill = fill.brighter (0.15f);
    else if (isMouseOverButton)
        fill = fill.brighter (0.07f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, Style::kCornerRadius);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return Fonts::regular (juce::jmin (14.0f, (float) buttonHeight * 0.55f));
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return Fonts::regular (14.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isMouseOverButton, bool)
{
    // A small flat box (accent-filled with a tick when on) and the text beside it.
    const auto bounds = button.getLocalBounds();
    const auto boxSize = juce::jmin (16.0f, (float) bounds.getHeight() - 4.0f);
    const auto box = juce::Rectangle<float> (2.0f, ((float) bounds.getHeight() - boxSize) * 0.5f, boxSize, boxSize);
    const auto on = button.getToggleState();
    const auto alpha = button.isEnabled() ? 1.0f : 0.4f;

    auto fill = on ? accent : Palette::knobBody;
    if (isMouseOverButton)
        fill = fill.brighter (0.07f);

    g.setColour (fill.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (box, Style::kCornerRadius - 1.0f);

    if (on)
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + boxSize * 0.25f, box.getCentreY());
        tick.lineTo (box.getX() + boxSize * 0.43f, box.getY() + boxSize * 0.70f);
        tick.lineTo (box.getRight() - boxSize * 0.22f, box.getY() + boxSize * 0.30f);
        g.setColour (Palette::onAccent (accent).withMultipliedAlpha (alpha));
        g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (button.findColour (juce::ToggleButton::textColourId).withMultipliedAlpha (alpha));
    g.setFont (Fonts::regular (juce::jmin (14.0f, (float) bounds.getHeight() * 0.6f)));
    g.drawFittedText (button.getButtonText(),
                      bounds.withTrimmedLeft ((int) box.getRight() + 6).withTrimmedRight (2),
                      juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const auto alpha = box.isEnabled() ? 1.0f : 0.4f;

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (bounds, Style::kCornerRadius);

    if (box.hasKeyboardFocus (true))
    {
        g.setColour (box.findColour (juce::ComboBox::focusedOutlineColourId));
        g.drawRoundedRectangle (bounds, Style::kCornerRadius, Style::kLineWidth);
    }

    // Small down-pointing triangle at the right
    const auto arrowW = 8.0f, arrowH = 5.0f;
    const auto cx = (float) width - 11.0f, cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - arrowW * 0.5f, cy - arrowH * 0.5f, cx + arrowW * 0.5f, cy - arrowH * 0.5f, cx, cy + arrowH * 0.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withMultipliedAlpha (alpha));
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return Fonts::regular (juce::jmin (14.0f, (float) box.getHeight() * 0.6f));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (2, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                    float sliderPos, float minSliderPos, float maxSliderPos,
                                    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    // Thin flat track, accent fill up to the value, small round thumb.
    const auto horizontal = style == juce::Slider::LinearHorizontal;
    const auto alpha = slider.isEnabled() ? 1.0f : 0.4f;
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto trackW = 4.0f;

    const auto start = horizontal ? juce::Point<float> (bounds.getX(), bounds.getCentreY())
                                  : juce::Point<float> (bounds.getCentreX(), bounds.getBottom());
    const auto end   = horizontal ? juce::Point<float> (bounds.getRight(), bounds.getCentreY())
                                  : juce::Point<float> (bounds.getCentreX(), bounds.getY());
    const auto thumb = horizontal ? juce::Point<float> (sliderPos, bounds.getCentreY())
                                  : juce::Point<float> (bounds.getCentreX(), sliderPos);

    g.setColour (slider.findColour (juce::Slider::backgroundColourId).withMultipliedAlpha (alpha));
    g.drawLine ({ start, end }, trackW);
    g.setColour (slider.findColour (juce::Slider::trackColourId).withMultipliedAlpha (alpha));
    g.drawLine ({ start, thumb }, trackW);

    const auto thumbSize = 12.0f;
    g.setColour (slider.findColour (juce::Slider::thumbColourId).withMultipliedAlpha (alpha));
    g.fillEllipse (juce::Rectangle<float> (thumbSize, thumbSize).withCentre (thumb));
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int width, int height,
                                 bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                 bool isMouseOver, bool isMouseDown)
{
    auto thumb = isScrollbarVertical ? juce::Rectangle<int> (x, thumbStartPosition, width, thumbSize)
                                     : juce::Rectangle<int> (thumbStartPosition, y, thumbSize, height);
    if (thumb.isEmpty())
        return;

    auto colour = bar.findColour (juce::ScrollBar::thumbColourId);
    if (isMouseDown)
        colour = colour.brighter (0.25f);
    else if (isMouseOver)
        colour = colour.brighter (0.12f);

    const auto r = thumb.toFloat().reduced (2.5f);
    g.setColour (colour);
    g.fillRoundedRectangle (r, juce::jmin (r.getWidth(), r.getHeight()) * 0.5f);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour fill, const juce::String& title)
{
    g.setColour (fill);
    g.fillRoundedRectangle (area.toFloat(), Style::kPanelRadius);

    if (title.isNotEmpty())
    {
        g.setColour (Palette::textDim);
        g.setFont (Fonts::bold (12.5f));
        g.drawText (title.toUpperCase(), area.reduced (10, 6).removeFromTop (16), juce::Justification::centredLeft);
    }
}

} // namespace rkfx
