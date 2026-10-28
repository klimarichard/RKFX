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

    /** The suite's effect families; each has a main colour (the plugins' own
        accents are tints of it). */
    enum class Family { filterEQ, dynamics, drive, modulation, delay, reverb, utility };

    inline juce::Colour familyColour (Family f)
    {
        switch (f)
        {
            case Family::filterEQ:   return juce::Colour (0xff8e5bd9);   // violet
            case Family::dynamics:   return juce::Colour (0xffe8b923);   // yellow
            case Family::drive:      return juce::Colour (0xffd4403a);   // red
            case Family::modulation: return juce::Colour (0xff3a9ad9);   // light blue
            case Family::delay:      return juce::Colour (0xff3dae5b);   // green
            case Family::reverb:     return juce::Colour (0xfff08a24);   // orange
            case Family::utility:    return juce::Colour (0xffb4b4b4);   // light gray
        }
        return juce::Colour (0xffb4b4b4);
    }

    /** How strongly the family colour tints the background and the panels. */
    inline constexpr float kBackgroundTint = 0.06f;
    inline constexpr float kPanelTint = 0.09f;

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
