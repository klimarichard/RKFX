#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rkfx
{

/** The suite palette: flat, dark, high contrast for dim stages. The per-plugin
    accent is used for active toggles and switches only. */
namespace Palette
{
    const juce::Colour background  { 0xff1c1d20 };
    const juce::Colour panel       { 0xff26282c };
    const juce::Colour outline     { 0xff3a3d43 };
    const juce::Colour knobBody    { 0xff34373c };
    const juce::Colour knobTrack   { 0xff4a4e55 };
    const juce::Colour text        { 0xffe8e8ea };
    const juce::Colour textDim     { 0xff8d9198 };
    const juce::Colour warning     { 0xffe0a040 };

    /** Text drawn on top of an accent fill (dark on light tints, light on dark). */
    inline juce::Colour onAccent (juce::Colour accent)
    {
        return accent.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff15161a) : juce::Colours::white;
    }
}

namespace Fonts
{
    inline juce::Font regular (float height) { return juce::Font (juce::FontOptions (height)); }
    inline juce::Font bold (float height)    { return juce::Font (juce::FontOptions (height, juce::Font::bold)); }
}

/** Shared look and feel: colours for the stock widgets (popup menus, alert
    windows, text editors, buttons) and flat drawing for buttons. */
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (juce::Colour accent);

    juce::Colour getAccent() const noexcept { return accent; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;

private:
    juce::Colour accent;
};

} // namespace rkfx
