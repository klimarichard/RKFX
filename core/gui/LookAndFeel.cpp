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
    g.fillRoundedRectangle (bounds, 4.0f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return Fonts::regular (juce::jmin (14.0f, (float) buttonHeight * 0.55f));
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return Fonts::regular (14.0f);
}

} // namespace rkfx
