#pragma once

#include "plugin/EditorBase.h"

#include "PluginProcessor.h"

/** RKPlay's window, readable from a few steps away:

        +-------------------------------------------------------------+
        | RKPlay                                 [Show v] [Save] [As] |
        +-------------------------------------------------------------+
        | Title (large)                              elapsed  -remain |
        | Artist        [~~~~~~~~ waveform, click to seek ~~~~~~~~]   |
        | Next: ...                                                   |
        | [|<] [ PLAY ] [>|] [Fade out] [Stop]   End at ..  [Settings]|
        +-------------------------------------------------------------+
        | [Dinner >] [Party] [+]                        | Queue       |
        | [In order|Shuffle] At end [..] Stop after each| ...         |
        | songs                                         | End at      |
        | [Add files] [Add folder]              [Clear] |             |
        +-------------------------------------------------------------+
        | [Level]   note / status                            [Bypass] |
        +-------------------------------------------------------------+

    Settings (fade out, smart crossfade, loudness, the Music folder) open
    over the lower half. */
class RKPlayEditor : public rkfx::EditorBase,
                     public juce::FileDragAndDropTarget,
                     private juce::ListBoxModel
{
public:
    explicit RKPlayEditor (RKPlayProcessor&);
    ~RKPlayEditor() override;

    static constexpr int kWidth = 1100;
    static constexpr int kHeight = 860;

    // Files from the Finder
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHighlight = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHighlight = false; repaint(); }

    void mouseDown (const juce::MouseEvent&) override;   // right-click Stop: Panic

private:
    /** The now-playing area: names, times, the waveform (click to seek), what's next. */
    struct NowPlaying : public juce::Component
    {
        explicit NowPlaying (RKPlayEditor& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        juce::Rectangle<int> barArea() const;
        RKPlayEditor& owner;
    };

    /** The playlists as tabs: click to show one, double-click to rename, right-click for more. */
    struct Tabs : public juce::Component
    {
        explicit Tabs (RKPlayEditor& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        int tabAt (juce::Point<int>) const;            // -2: the + tab
        juce::Rectangle<int> tabBounds (int index) const;
        RKPlayEditor& owner;
    };

    /** The queue's list. */
    struct QueueModel : public juce::ListBoxModel
    {
        explicit QueueModel (RKPlayEditor& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void deleteKeyPressed (int row) override;
        RKPlayEditor& owner;
    };

    /** A pad: click to play (again to stop), right-click for its options; drop a file on it. */
    struct PadButton : public juce::Component
    {
        PadButton (RKPlayEditor& o, int i) : owner (o), index (i) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        RKPlayEditor& owner;
        const int index;
    };

    /** Pre-listen: the song on the cue output, its place (click to seek), Stop, its level. */
    struct PreListen : public juce::Component
    {
        explicit PreListen (RKPlayEditor&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        juce::Rectangle<int> barArea() const;
        RKPlayEditor& owner;
        juce::TextButton stop { "Stop" };
        juce::Slider level;
        int playlist = -1, index = -1;   // what it plays (to seek within it)
    };

    /** Settings for the whole show, over the lower half of the window. */
    struct Settings : public juce::Component
    {
        explicit Settings (RKPlayEditor&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

        RKPlayEditor& owner;
        juce::Slider fadeOutTime, target, depth, threshold, attack, hold, release;
        juce::ToggleButton autoDuck { "Duck from the input" };
        juce::ToggleButton smart { "Smart crossfade" }, match { "Level match" }, safety { "Safety limiter" };
        juce::ComboBox lift;
        juce::TextButton chooseFolder { "Choose..." }, clearFolder { "None" }, relink { "Relink missing songs..." },
                         close { "Close" };
        juce::Rectangle<int> transportArea, loudnessArea, duckArea, folderArea;
    };

    RKPlayProcessor& player;
    int viewed = 0;                       // the playlist the list shows
    NowPlaying nowPlaying { *this };
    juce::TextButton prevButton { "|<" }, playButton { "Play" }, nextButton { ">|" },
                     fadeButton { "Fade out" }, stopButton { "Stop" }, settingsButton { "Settings" }, duckButton { "Duck" };
    Tabs tabs { *this };
    juce::TextButton inOrderButton { "In order" }, shuffleButton { "Shuffle" };
    juce::ComboBox atEnd;
    juce::ToggleButton stopAfterEach { "Stop after each" };
    juce::Slider crossfade, listGain;
    juce::ListBox list { "Playlist", this };
    juce::TextButton addFilesButton { "Add files..." }, addFolderButton { "Add folder..." }, clearButton { "Clear" };
    QueueModel queueModel { *this };
    juce::ListBox queueList { "Queue", &queueModel };
    juce::TextButton clearQueueButton { "Clear queue" };
    PreListen preListen { *this };
    std::vector<std::unique_ptr<PadButton>> padButtons;
    juce::ToggleButton endAtOn { "End at" };
    juce::TextEditor endAtTime;
    juce::ComboBox endAtMode;
    std::unique_ptr<rkfx::Knob> level;
    juce::Label note, status;
    Settings settings { *this };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> infoArea, listArea, queueArea, preListenArea, endAtArea, messageArea;
    bool dropHighlight = false;
    int lastShowVersion = -1, lastNoteVersion = -1;
    double noteTime = 0.0;
    float limiterDb = 0.0f;
    double limiterTime = 0.0;
    RKPlayProcessor::Item nextItem;       // what plays next (refreshed with the show)
    juce::TimedCallback timer { [this] { tick(); } };

    const PlayPlaylist& viewedList() const { return player.getShow().playlists[(size_t) juce::jlimit (0, (int) player.getShow().playlists.size() - 1, viewed)]; }

    // juce::ListBoxModel (the songs)
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void deleteKeyPressed (int lastRowSelected) override;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;
    void tick();
    void refresh();
    void showSongMenu (int row);
    void showTabMenu (int playlist);
    void showSettings (bool);
    void renamePlaylist (int playlist);
    void chooseFiles (bool folders);
    void chooseLink (int playlist, bool folder);
    void exportPlaylist (int playlist);
    void applyEndAt();
    void showPadMenu (int pad);
    void choosePadFile (int pad);
    void renamePad (int pad);

    static juce::String timeText (double seconds);
};
