#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "host/UndoHistory.h"

#include <memory>
#include <vector>

namespace rkboard
{

class BoardProcessor;

/** RKBoard's undo history (the suite's rkfx::UndoHistory, as RKMix's).

    Recorded:
    - values: every pedal's knobs and switches, every split's settings, each
      pedal's bypass on the board, In and Out (polled; changes less than half
      a second apart are one step);
    - settings: assignments (slot, range, invert), scene-controlled knobs,
      trails, scene names and what each scene stores;
    - structure: pedals added, removed, duplicated or moved, splits inserted
      or removed. Each step keeps the board's layout before and after and the
      full state of the pedals it added or removed: undoing re-creates only
      what's missing (with its same ID, so its assignments and scenes come
      back with it), keeps the pedals still there, and swaps the chain once;
    - scene recalls: undoing one brings back the board and the scene before.

    Not recorded: loading a board (a new history), the tuner, tempo and tap,
    MIDI learn, the Morph position. Message thread. */
class BoardUndo : private rkfx::UndoHistory::Client
{
public:
    explicit BoardUndo (BoardProcessor&);
    ~BoardUndo() override;

    rkfx::UndoHistory& getHistory() noexcept { return *history; }

    /** Around a structural edit: @p leaving are the pedals it removes. */
    void beginStructure (const std::vector<int>& leaving = {});
    void endStructure (const juce::String& description);

private:
    BoardProcessor& board;
    std::unique_ptr<rkfx::UndoHistory> history;

    // A structural edit in progress (BoardProcessor's edits don't nest).
    int depth = 0;
    juce::var layoutBefore;
    juce::String extraBefore;
    juce::DynamicObject::Ptr states;

    juce::var layoutVar() const;
    juce::var pedalState (int id) const;

    // rkfx::UndoHistory::Client
    void read (std::vector<std::pair<rkfx::UndoHistory::Key, float>>&) override;
    void write (rkfx::UndoHistory::Key, float) override;
    juce::String nameOf (rkfx::UndoHistory::Key) const override;
    int getScene() const override;
    void restoreScene (int) override;
    juce::String sceneName (int) const override;
    int getExtraVersion() const override;
    juce::String readExtra() override;
    void writeExtra (const juce::String&) override;
    juce::String describeExtra (const juce::String&, const juce::String&) const override;
    void applyStructure (const juce::var&, bool backwards) override;
    void beginApply() override;
    void endApply() override;
};

} // namespace rkboard
