#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "host/UndoHistory.h"

#include <functional>
#include <memory>
#include <vector>

class MixScenes;

/** RKMix's undo history: the console's values (every mix setting and the
    host's faders and mutes, DCAs and mute groups; not the assignables, the
    scene number, talk or bypass) and which scene is current.

    The steps and their grouping are the suite's shared history
    (rkfx::UndoHistory, polled ten times a second; changes less than half a
    second apart are one step). Undoing a recall also makes the scene before
    it current again, without storing anything into either scene. Not
    recorded: the inserts' own knobs, inserts added or removed, names and
    colours. Loading a mix clears the history. */
class MixUndo : private rkfx::UndoHistory::Client
{
public:
    MixUndo (std::vector<juce::RangedAudioParameter*> tracked, MixScenes&,
             std::function<void (int scene)> setSceneParameter);
    ~MixUndo() override;

    bool canUndo() const                      { return history->canUndo(); }
    bool canRedo() const noexcept             { return history->canRedo(); }
    void undo()                               { history->undo(); }
    void redo()                               { history->redo(); }

    /** "Undo Ch 3 Fader", "Undo Scene 2 recall", "Undo 12 changes" (empty: nothing). */
    juce::String undoDescription() const      { return history->undoDescription(); }
    juce::String redoDescription() const      { return history->redoDescription(); }

    /** Forgets every step and takes the mix as it is now (after a load). */
    void clear()                              { history->clear(); }

    /** Records what changed since the last poll now (tests; the timer does it otherwise). */
    void poll()                               { history->poll(); }

    /** Closes the step being recorded (tests: no waiting for the half second). */
    void closeStep()                          { history->closeStep(); }

    /** Bumped when the history changes (the window polls it). */
    int getVersion() const noexcept           { return history->getVersion(); }

    static constexpr int kMaxSteps = rkfx::UndoHistory::kMaxSteps;
    static constexpr double kStepGapMs = rkfx::UndoHistory::kStepGapMs;

private:
    std::vector<juce::RangedAudioParameter*> params;
    MixScenes& scenes;
    std::function<void (int)> setScene;
    std::unique_ptr<rkfx::UndoHistory> history;

    // rkfx::UndoHistory::Client: a control's key is its place in params.
    void read (std::vector<std::pair<rkfx::UndoHistory::Key, float>>&) override;
    void write (rkfx::UndoHistory::Key, float) override;
    juce::String nameOf (rkfx::UndoHistory::Key) const override;
    int getScene() const override;
    void restoreScene (int) override;
    void beginApply() override;
};
