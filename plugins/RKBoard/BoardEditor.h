#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "BoardProcessor.h"
#include "gui/LookAndFeel.h"

namespace rkboard
{

/** The row of pedals: one tile per pedal in signal order, then a "+" tile
    while there is room. Painted as one component (no child per tile), so
    dragging a tile to a new place is simple.

    - click a tile: open its controls below;
    - click its footswitch: bypass on / off;
    - drag a tile: move the pedal;
    - right-click a tile: duplicate or remove;
    - click "+": add a pedal from a menu grouped by family. */
class BoardStrip : public juce::Component
{
public:
    explicit BoardStrip (BoardProcessor&);

    int getSelected() const noexcept { return selected; }
    void setSelected (int id);
    std::function<void (int id)> onSelect;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static constexpr int kTileWidth = 70;
    static constexpr int kTileGap = 6;
    static constexpr int kTileHeight = 112;

private:
    BoardProcessor& board;
    int selected = 0;
    int hoverId = 0;

    int pressedId = 0;          // tile under the mouse at mouseDown
    bool dragging = false;
    int dragX = 0;              // mouse x while dragging

    juce::Rectangle<int> tileBounds (int position) const;
    juce::Rectangle<int> footswitchBounds (juce::Rectangle<int> tile) const;
    int tileAt (juce::Point<int>) const;               // pedal ID, -1 for "+", 0 for none
    int insertionIndex (int x) const;

    void paintTile (juce::Graphics&, juce::Rectangle<int>, int id, bool ghost) const;
    void paintAddTile (juce::Graphics&, juce::Rectangle<int>) const;

    void showAddMenu (juce::Rectangle<int> target);
    void showTileMenu (int id);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardStrip)
};

/** The RKBoard window: a slim header (board preset, input and output level),
    the pedal row, and the selected pedal's own editor below. */
class BoardEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit BoardEditor (BoardProcessor&);
    ~BoardEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Opens pedal @p id's controls below the board (0 closes them). */
    void showPedal (int id);
    int getShownPedal() const noexcept { return shownPedal; }

    static constexpr int kWidth = 960;
    static constexpr int kHeight = 800;
    static constexpr int kHeaderHeight = 44;
    static constexpr int kMargin = 12;

private:
    BoardProcessor& board;

    // Declared before every component so it outlives them.
    rkfx::LookAndFeel lookAndFeel { rkfx::Palette::boardAccent };

    juce::TextButton presetButton, saveButton { "Save" }, saveAsButton { "Save as" };
    juce::Label noteLabel;
    juce::Label inputLabel, outputLabel;
    juce::Slider inputSlider, outputSlider;
    std::unique_ptr<juce::SliderParameterAttachment> inputAttachment, outputAttachment;
    std::unique_ptr<juce::FileChooser> chooser;

    BoardStrip strip { board };

    int shownPedal = 0;
    std::unique_ptr<juce::AudioProcessorEditor> pedalEditor;
    juce::Rectangle<int> pedalBaseSize;     // its own (unscaled) size

    int lastPresetCounter = -1;
    int lastBoardCounter = -1;

    juce::Rectangle<int> panelArea() const;
    void closePedalEditor();
    void layoutPedalEditor();

    void refreshPresetButton();
    void showNote (const juce::String& text, bool isError);
    void showPresetMenu();
    void save();
    void saveAs();

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardEditor)
};

} // namespace rkboard
