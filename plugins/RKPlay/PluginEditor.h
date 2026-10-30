#pragma once

#include "plugin/EditorBase.h"

#include "PluginProcessor.h"

/** RKPlay's window, readable from a few steps away:

        +-------------------------------------------------------------+
        | RKPlay                                 [Show v] [Save] [As] |
        +-------------------------------------------------------------+
        | Title (large)                              elapsed  -remain |
        | Artist                                                      |
        | [============ progress (click to seek) ===============]     |
        | Next: ...                                                   |
        | [ |< ] [ PLAY ] [ >| ] [ Fade out ] [ Stop ]                |
        +-------------------------------------------------------------+
        | Playlist (drop files and folders here)       Crossfade, ... |
        +-------------------------------------------------------------+
        | [Level]   note                                     [Bypass] |
        +-------------------------------------------------------------+ */
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
    /** The now-playing area: names, times, the progress bar (click to seek), what's next. */
    struct NowPlaying : public juce::Component
    {
        explicit NowPlaying (RKPlayEditor& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        juce::Rectangle<int> barArea() const;
        RKPlayEditor& owner;
    };

    RKPlayProcessor& player;
    NowPlaying nowPlaying { *this };
    juce::TextButton prevButton { "|<" }, playButton { "Play" }, nextButton { ">|" },
                     fadeButton { "Fade out" }, stopButton { "Stop" };
    juce::ListBox list { "Playlist", this };
    juce::TextButton addFilesButton { "Add files..." }, addFolderButton { "Add folder..." }, clearButton { "Clear" };
    juce::Slider crossfade, fadeOutTime, target;
    juce::ToggleButton smart { "Smart crossfade" }, match { "Level match" }, safety { "Safety limiter" };
    juce::ComboBox lift;
    std::unique_ptr<rkfx::Knob> level;
    juce::Label note, status;
    float limiterDb = 0.0f;
    double limiterTime = 0.0;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> listArea, playlistArea, loudnessArea;
    bool dropHighlight = false;
    int lastShowVersion = -1, lastNoteVersion = -1;
    double noteTime = 0.0;
    juce::TimedCallback timer { [this] { tick(); } };

    // juce::ListBoxModel
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
    void chooseFiles (bool folders);

    static juce::String timeText (double seconds);
};
