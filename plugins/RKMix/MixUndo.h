#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <deque>
#include <functional>
#include <vector>

class MixScenes;

/** RKMix's undo history: the console's values (every mix setting and the
    host's faders and mutes, DCAs and mute groups; not the assignables, the
    scene number, talk or bypass) and which scene is current.

    Polled ten times a second on the message thread, so a change from
    anywhere (the window, a Gig Performer widget, a scene recall, the remote)
    is recorded and no audio-thread listener is needed. Changes that follow
    each other with less than half a second between them are one step: a
    fader drag, a scene recall with its glide. Undoing a recall also makes
    the scene before it current again, without storing anything into either
    scene. Not recorded: the inserts' own knobs, inserts added or removed,
    names and colours. Loading a mix clears the history. */
class MixUndo : private juce::Timer
{
public:
    MixUndo (std::vector<juce::RangedAudioParameter*> tracked, MixScenes&,
             std::function<void (int scene)> setSceneParameter);
    ~MixUndo() override;

    bool canUndo() const;
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    void undo();
    void redo();

    /** "Undo Ch 3 Fader", "Undo Scene 2 recall", "Undo 12 changes" (empty: nothing). */
    juce::String undoDescription() const;
    juce::String redoDescription() const;

    /** Forgets every step and takes the mix as it is now (after a load). */
    void clear();

    /** Records what changed since the last poll now (tests; the timer does it otherwise). */
    void poll();

    /** Closes the step being recorded (tests: no waiting for the half second). */
    void closeStep();

    /** Bumped when the history changes (the window polls it). */
    int getVersion() const noexcept { return version; }

    static constexpr int kMaxSteps = 200;
    static constexpr double kStepGapMs = 500.0;

private:
    struct Change
    {
        int index;              // into tracked
        float before, after;    // normalised
    };
    struct Step
    {
        std::vector<Change> changes;
        int sceneBefore = -1, sceneAfter = -1;
        bool empty() const noexcept { return changes.empty() && sceneBefore == sceneAfter; }
    };

    std::vector<juce::RangedAudioParameter*> params;
    std::vector<float> last;
    std::vector<int> openSlot;    // by tracked control: its place in open.changes, or -1
    MixScenes& scenes;
    std::function<void (int)> setScene;
    int lastScene = 0;

    std::deque<Step> undoStack, redoStack;
    Step open;
    bool isOpen = false;
    double lastChangeMs = 0.0;
    int version = 0;

    void timerCallback() override { poll(); }
    void apply (const Step&, bool backwards);
    juce::String describe (const Step&) const;
};
