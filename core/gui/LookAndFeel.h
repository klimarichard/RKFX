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

    /** Accent for playback tools (RKTracks): a neutral gray outside the effect
        families, so a tool never looks like an effect. */
    const juce::Colour toolAccent { 0xff8a8a8a };

    /** How strongly the family colour tints the background and the panels. */
    inline constexpr float kBackgroundTint = 0.06f;
    inline constexpr float kPanelTint = 0.09f;

    inline juce::Colour tintedBackground (juce::Colour tint) { return background.interpolatedWith (tint, kBackgroundTint); }
    inline juce::Colour tintedPanel (juce::Colour tint)      { return panel.interpolatedWith (tint, kPanelTint); }

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

/** Corner radii and line widths, so hand-painted components match the widgets. */
namespace Style
{
    inline constexpr float kCornerRadius = 4.0f;   // buttons, switches, fields
    inline constexpr float kPanelRadius = 6.0f;    // sections and panels
    inline constexpr float kLineWidth = 1.0f;      // outlines and separators
}

/** A section panel: rounded fill, optional dim uppercase title at the top left. */
void drawPanel (juce::Graphics&, juce::Rectangle<int> area, juce::Colour fill, const juce::String& title = {});

/** Shared look and feel: colours for the stock widgets (popup menus, alert
    windows, text editors, labels, lists, combo boxes, sliders, scroll bars)
    and flat drawing for buttons, toggle buttons, combo boxes and sliders. */
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (juce::Colour accent);

    juce::Colour getAccent() const noexcept { return accent; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool isMouseOverButton, bool isButtonDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override { return 10; }

private:
    juce::Colour accent;
};

} // namespace rkfx
