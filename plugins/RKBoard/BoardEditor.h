#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "BoardProcessor.h"
#include "AssignPanel.h"
#include "RemotePanel.h"
#include "RoutingPanel.h"
#include "TunerPanel.h"
#include "gui/LookAndFeel.h"

namespace rkboard
{

/** The board, flowing left to right:

        [chain] -> SPLIT -+- [path A] -+- MERGE -> [chain] -> SPLIT ... -> [chain]
                          +- [path B] -+

    One tile per pedal, a "+" at the end of each section, and each split and
    merge as narrow nodes (numbered when there are several). Like text, the board wraps onto further rows when
    it doesn't fit, with a cable from the end of one row to the start of the
    next; the split with both paths and the merge stay together on one row.
    Painted as one component (no child per tile), so dragging a tile
    anywhere is simple.

    - click a tile: open its controls below; click a node: its settings;
    - click a tile's footswitch: bypass on / off;
    - drag a tile: move the pedal, also to another section or row;
    - right-click a tile: duplicate, remove, trails, assign its switch,
      insert a split after it;
    - click a "+": add a pedal there, from a menu grouped by family (on a
      chain also: insert a split there);
    - right-click a split or merge node: remove that split. */
class BoardStrip : public juce::Component
{
public:
    explicit BoardStrip (BoardProcessor&);

    /** What can be selected: a pedal ID, a split's or merge's node, or a view. */
    static constexpr int splitNode (int split) noexcept { return -100 - 2 * split; }
    static constexpr int mergeNode (int split) noexcept { return -101 - 2 * split; }
    static constexpr bool isNode (int key) noexcept     { return key <= -100 && key > -100 - 2 * kMaxSplits; }
    static constexpr int nodeSplit (int key) noexcept   { return (-100 - key) / 2; }
    static constexpr bool isMergeNode (int key) noexcept { return isNode (key) && (-100 - key) % 2 == 1; }
    static constexpr int kSplitNode = -100;      // splitNode (0)
    static constexpr int kMergeNode = -101;      // mergeNode (0)
    static constexpr int kAssignView = -4;      // not on the strip: the Assign button
    static constexpr int kTunerView = -5;       // not on the strip: the tuner (standalone)
    static constexpr int kRemoteView = -6;      // not on the strip: the remote's settings

    int getSelected() const noexcept { return selected; }

    /** The window's lock: with footswitches, a click selects a pedal or
        switches it; with everything, it only selects. No dragging, adding or
        menus either way. */
    void setLock (BoardProcessor::Lock l) { lock = l; dragging = false; pressedId = 0; repaint(); }
    void setSelected (int item);
    std::function<void (int item)> onSelect;

    /** Extra items for a tile's right-click menu (MIDI learn, standalone),
        with IDs from 100; the result goes to onTileMenuResult. */
    std::function<void (juce::PopupMenu&, int id)> addTileMenuItems;
    std::function<void (int result, int id)> onTileMenuResult;

    /** "Remove this split" on a node's menu (the editor closes its panel first). */
    std::function<void (int split)> onRemoveSplit;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** Where pedal @p id's tile is, and its footswitch (empty when it isn't on the board). */
    juce::Rectangle<int> tileBounds (int id);
    juce::Rectangle<int> footswitchOf (int id) { return tileBounds (id).isEmpty() ? juce::Rectangle<int>() : footswitchBounds (tileBounds (id)); }

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
        int id = 0;                        // pedals; the split's index for nodes
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
    BoardProcessor::Lock lock = BoardProcessor::Lock::off;
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
    void showNodeMenu (int split);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardStrip)
};

/** A scene button: its number, and its name under it (shortened to fit;
    whole in the tooltip). Double-click to rename it. */
class SceneButton : public juce::TextButton
{
public:
    void setScene (int number, const juce::String& name);
    std::function<void()> onDoubleClick;
    /** Locked: it recalls its scene, but isn't renamed. */
    void setLocked (bool l) { locked = l; }

    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    int scene = 0;
    juce::String sceneName;
    bool locked = false;
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
    /** Cmd+Z undo, Cmd+Shift+Z redo. */
    bool keyPressed (const juce::KeyPress&) override;

    /** Opens a pedal's controls below the board, the split's or merge's
        settings (BoardStrip::kSplitNode / kMergeNode), or the assignable
        parameters (BoardStrip::kAssignView); 0 closes them. */
    void showPedal (int item);
    int getShownPedal() const noexcept { return shownPedal; }

    /** The pedal row (tests). */
    BoardStrip& getStrip() noexcept { return strip; }
    /** Follows the board now, as the window's timer does (tests). */
    void update() { timerCallback(); }

    static constexpr int kWidth = 1180;
    static constexpr int kPanelHeight = 560;    // the pedal's controls (scaled down only when the screen is too small)
    static constexpr int kHeaderHeight = 44;
    static constexpr int kSceneBarHeight = 46;
    static constexpr int kMargin = 12;

private:
    BoardProcessor& board;

    // Declared before every component so it outlives them.
    rkfx::LookAndFeel lookAndFeel { rkfx::Palette::boardAccent };

    juce::TextButton presetButton, saveButton { "Save" }, saveAsButton { "Save as" };
    juce::TextButton assignButton { "Assign" }, remoteButton { "Remote" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    int lastUndoVersion = -1;

    /** Lies over what the lock protects: takes the clicks, does nothing. */
    struct LockShield : public juce::Component,
                        public juce::SettableTooltipClient
    {
        LockShield() { setTooltip ("Locked: hold Lock to unlock"); }
        void paint (juce::Graphics&) override;
    };

    /** Lock: click for the lock's menu; while locked, hold it a second to unlock. */
    struct LockButton : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
    {
        explicit LockButton (BoardEditor& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void timerCallback() override;

        BoardEditor& owner;
        double holdStart = 0.0;
        bool holding = false;
    };
    LockButton lockButton { *this };
    std::array<LockShield, 4> shields;   // the header's left and right, the scene bar (or its tempo field), the panel
    BoardProcessor::Lock shownLock = BoardProcessor::Lock::off;
    bool isLocked() const noexcept { return board.getLock() != BoardProcessor::Lock::off; }
    void applyLock();
    std::array<SceneButton, BoardProcessor::kNumScenes> sceneButtons;

    // Scene morph: From, To, and the Morph position (the host parameter).
    juce::Label morphLabel;
    juce::TextButton morphFromButton, morphToButton;
    juce::Label morphArrow;
    juce::Slider morphSlider;
    std::unique_ptr<juce::SliderParameterAttachment> morphAttachment;
    void showMorphSceneMenu (bool to);
    void refreshMorph();

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
    std::unique_ptr<RemotePanel> remotePanel;
    juce::Rectangle<int> pedalBaseSize;     // its own (unscaled) size

    int lastPresetCounter = -1;
    int lastNumSplits = -1;
    int lastBoardCounter = -1;

    juce::Rectangle<int> panelArea() const;
    int stripHeight() const;
    void updateSize();
    void closePedalEditor();
    void showAssignMenu (int pedalId, juce::RangedAudioParameter&);
    void removeSplit (int split);
    void showSceneMenu (int scene);
    void renameScene (int scene);

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
    void refreshUndo();
    void undoOrRedo (bool redo);
    void showNote (const juce::String& text, bool isError);
    void showPresetMenu();
    void save();
    void saveAs();

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardEditor)
};

} // namespace rkboard
