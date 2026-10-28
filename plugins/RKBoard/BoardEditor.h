#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "BoardProcessor.h"
#include "AssignPanel.h"
#include "RoutingPanel.h"
#include "TunerPanel.h"
#include "gui/LookAndFeel.h"

namespace rkboard
{

/** The board, flowing left to right:

        [pre] -> SPLIT -+- [path A] -+- MERGE -> [post]
                        +- [path B] -+

    One tile per pedal, a "+" at the end of each section, and the split and
    merge as narrow nodes. Like text, the board wraps onto further rows when
    it doesn't fit, with a cable from the end of one row to the start of the
    next; the split with both paths and the merge stay together on one row.
    Painted as one component (no child per tile), so dragging a tile
    anywhere is simple.

    - click a tile: open its controls below; click a node: its settings;
    - click a tile's footswitch: bypass on / off;
    - drag a tile: move the pedal, also to another section or row;
    - right-click a tile: duplicate, remove, trails, assign its switch;
    - click a "+": add a pedal there, from a menu grouped by family. */
class BoardStrip : public juce::Component
{
public:
    explicit BoardStrip (BoardProcessor&);

    /** What can be selected: a pedal ID, or one of the nodes. */
    static constexpr int kSplitNode = -2;
    static constexpr int kMergeNode = -3;
    static constexpr int kAssignView = -4;      // not on the strip: the Assign button
    static constexpr int kTunerView = -5;       // not on the strip: the tuner (standalone)

    int getSelected() const noexcept { return selected; }
    void setSelected (int item);
    std::function<void (int item)> onSelect;

    /** Extra items for a tile's right-click menu (MIDI learn, standalone),
        with IDs from 100; the result goes to onTileMenuResult. */
    std::function<void (juce::PopupMenu&, int id)> addTileMenuItems;
    std::function<void (int result, int id)> onTileMenuResult;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** The height the board needs at @p width (it grows a row at a time). */
    int getPreferredHeight (int width) const;

    static constexpr int kTileWidth = 66;
    static constexpr int kTileHeight = 84;
    static constexpr int kLaneGap = 8;
    static constexpr int kRowGap = 22;     // between rows, for the cable
    static constexpr int kPadding = 10;

private:
    struct Item
    {
        enum class Kind { pedal, add, split, merge } kind;
        int id = 0;                        // pedals
        Section section = Section::pre;    // pedals and "+"
        int index = 0;                     // position in its section
        juce::Rectangle<int> bounds;
    };

    /** One row of the wrapped board. */
    struct Row { int top = 0, height = 0; };

    struct Layout
    {
        std::vector<Item> items;
        std::vector<int> flow;            // indices into items, in signal order, without the paths' items
        std::vector<Row> rows;
        std::vector<int> rowOf;           // per item
        int height = 0;
    };

    BoardProcessor& board;
    int selected = 0;
    int hoverKey = 0;                     // keyOf() the item under the mouse
    Layout layout;                        // rebuilt by layoutItems()

    Layout computeLayout (int width) const;

    int pressedId = 0;          // pedal under the mouse at mouseDown
    bool dragging = false;
    juce::Point<int> dragPos;

    void layoutItems();
    static int keyOf (const Item&);       // pedal ID, node constant, or 100 + section for a "+"
    const Item* itemAt (juce::Point<int>) const;
    const Item* find (Item::Kind, int id = 0) const;
    juce::Rectangle<int> footswitchBounds (juce::Rectangle<int> tile) const;

    /** Where a dragged pedal would land. */
    struct Drop { Section section; int index; juce::Rectangle<int> marker; };
    Drop dropAt (juce::Point<int>) const;

    void paintTile (juce::Graphics&, juce::Rectangle<int>, int id, bool ghost) const;
    void paintAddTile (juce::Graphics&, const Item&) const;
    void paintNode (juce::Graphics&, const Item&) const;

    void showAddMenu (Section, juce::Rectangle<int> target);
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

    /** Right-click on a knob in the pedal's controls: the assign menu. */
    void mouseDown (const juce::MouseEvent&) override;

    /** Opens a pedal's controls below the board, the split's or merge's
        settings (BoardStrip::kSplitNode / kMergeNode), or the assignable
        parameters (BoardStrip::kAssignView); 0 closes them. */
    void showPedal (int item);
    int getShownPedal() const noexcept { return shownPedal; }

    static constexpr int kWidth = 1100;
    static constexpr int kPanelHeight = 560;    // the pedal's controls (scaled down only when the screen is too small)
    static constexpr int kHeaderHeight = 44;
    static constexpr int kSceneBarHeight = 36;
    static constexpr int kMargin = 12;

private:
    BoardProcessor& board;

    // Declared before every component so it outlives them.
    rkfx::LookAndFeel lookAndFeel { rkfx::Palette::boardAccent };

    juce::TextButton presetButton, saveButton { "Save" }, saveAsButton { "Save as" };
    juce::TextButton assignButton { "Assign" };
    juce::Label sceneLabel;
    std::array<juce::TextButton, BoardProcessor::kNumScenes> sceneButtons;

    // Standalone: tempo and tuner
    juce::Label tempoField, tempoUnit;
    juce::TextButton tapButton { "Tap" }, tunerButton { "Tuner" };
    double shownTempo = -1.0;
    int lastLearning = -1;
    int viewBeforeTuner = 0;
    juce::Label noteLabel;
    juce::Label inputLabel, outputLabel;
    juce::Slider inputSlider, outputSlider;
    std::unique_ptr<juce::SliderParameterAttachment> inputAttachment, outputAttachment;
    std::unique_ptr<juce::FileChooser> chooser;

    BoardStrip strip { board };

    int shownPedal = 0;
    std::unique_ptr<juce::AudioProcessorEditor> pedalEditor;
    std::unique_ptr<RoutingPanel> routingPanel;
    std::unique_ptr<AssignPanel> assignPanel;
    std::unique_ptr<TunerPanel> tunerPanel;
    juce::Rectangle<int> pedalBaseSize;     // its own (unscaled) size

    int lastPresetCounter = -1;
    int lastBoardCounter = -1;

    juce::Rectangle<int> panelArea() const;
    int stripHeight() const;
    void updateSize();
    void closePedalEditor();
    void showAssignMenu (int pedalId, juce::RangedAudioParameter&);
    void showSceneMenu (int scene);

    /** MIDI learn items (standalone) for @p target, added to @p menu with IDs from @p base. */
    void addLearnItems (juce::PopupMenu& menu, int target, int base) const;
    /** Handles a result of addLearnItems(); false when it wasn't one of them. */
    bool handleLearnResult (int result, int target, int base);
    void showLearnMenu (int target, juce::Component& near);
    void refreshTempo();
    void syncTunerView();
    void refreshSceneButtons();
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
