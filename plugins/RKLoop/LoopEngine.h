#pragma once

#include "safety/Smoothing.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>

/** The RKLoop DSP: a live looper with eight loops, zero latency.

    Each loop is a Part or a Layer:
    - Parts are song sections: one plays at a time. Choosing another part
      while one plays switches at the end of the playing part's loop.
    - Layers play on top of the part (and of each other), each on or off.

    The footswitches act on the selected loop, like a looper pedal:

        empty --Rec--> recording --Rec--> playing <--Rec--> overdubbing
                                   |                          |
                                   +---Stop--> stopped <------+   (Rec in stopped: play from the start)

    Stop on a part stops everything (the song); on a layer, only the layer.

    Sync (on by default): Loop 1 sets the length. While anything plays,
    other loops start recording or playing on Loop 1's next downbeat, and a
    recording ends at the nearest 1/4, 1/2, 1, 2, 3... times Loop 1's
    length (it keeps recording until then, or drops what went past it), so
    every loop stays in step. Without sync, or before Loop 1 exists, loops
    are free: they start at once and are as long as played.

    Per loop, as in a single looper: 5 ms fades at the seam of a new
    recording and on play, stop and clear; overdubbing saves each changed
    sample once per session, so Undo swaps the session out again (and Redo
    back in), a few hundred samples per audio sample. Reverse plays and
    overdubs backwards; Half Speed plays an octave down (no overdubbing).
    The input always passes through; the loops are added at Loop Level.

    Memory for every loop at its maximum length is allocated in prepare()
    (never while processing); a prepare() at the same rate keeps the loops.
    The loops' audio is not part of the saved state. */
class LoopEngine
{
public:
    static constexpr int kLoops = 8;
    static constexpr int kMaxChannels = 2;
    static constexpr double kMaxSeconds = 30.0;
    static constexpr double kFadeMs = 5.0;
    static constexpr int kSwapPerSample = 256;

    enum class State { empty, recording, playing, overdubbing, stopped };
    enum class Role { part, layer };
    enum class Button { record, stop, undo, clear, next, previous };

    /** The synced length nearest to @p recorded samples: 1/4, 1/2 or a
        whole multiple of @p master, at most @p maxLength. */
    static int syncedLength (int recorded, int master, int maxLength) noexcept;

    void prepare (double sampleRate);
    void reset();                         // stops playback (keeps the loops)

    // Audio thread, before process().
    void press (Button) noexcept;         // acts on the selected loop
    void select (int index) noexcept;     // 0..7; choosing a part while one plays queues the switch
    void setRole (int index, Role) noexcept;
    void setSync (bool on) noexcept       { sync = on; }
    void setReverse (bool on) noexcept    { reverse = on; }
    void setHalfSpeed (bool on) noexcept  { halfSpeed = on; }
    void setLevel (float gain) noexcept   { level.setTargetValue (gain); }

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    // For the display and tests (any thread).
    int getSelected() const noexcept                 { return selected.load(); }
    State getState (int i) const noexcept            { return loops[(size_t) i].state.load(); }
    State getState() const noexcept                  { return getState (getSelected()); }
    int getLength (int i) const noexcept             { return loops[(size_t) i].length.load(); }
    int getLength() const noexcept                   { return getLength (getSelected()); }
    int getRecordedLength (int i) const noexcept     { return getState (i) == State::recording ? loops[(size_t) i].written.load() : getLength (i); }
    int getRecordedLength() const noexcept           { return getRecordedLength (getSelected()); }
    double getPosition (int i) const noexcept        { return loops[(size_t) i].shownPosition.load(); }
    double getPosition() const noexcept              { return getPosition (getSelected()); }
    bool isQueued (int i) const noexcept             { return loops[(size_t) i].queued.load(); }
    bool canUndo (int i) const noexcept              { return loops[(size_t) i].undoCount.load() > 0 && ! loops[(size_t) i].undone.load(); }
    bool canRedo (int i) const noexcept              { return loops[(size_t) i].undoCount.load() > 0 && loops[(size_t) i].undone.load(); }
    bool canUndo() const noexcept                    { return canUndo (getSelected()); }
    bool canRedo() const noexcept                    { return canRedo (getSelected()); }
    Role getRole (int i) const noexcept              { return loops[(size_t) i].role.load(); }
    int getMaxLength() const noexcept                { return maxLength; }

    /** Loop 1's length while sync is on (0 = no sync reference). */
    int getMasterLength() const noexcept;

private:
    enum class AfterFade { nothing, stop, clear };

    struct Loop
    {
        juce::AudioBuffer<float> audio, undo;
        std::atomic<State> state { State::empty };
        std::atomic<Role> role { Role::part };
        std::atomic<int> length { 0 }, written { 0 };
        std::atomic<double> shownPosition { 0.0 };
        std::atomic<bool> queued { false };      // waiting to start (record or play)
        bool queuedRecord = false;
        int recordTarget = -1;                   // synced recording: closes here...
        bool stopAfterRecording = false;         // ...and then stops (else plays)
        double position = 0.0;
        rkfx::Smoothed gain;
        AfterFade afterFade = AfterFade::nothing;

        int undoStart = 0, sessionSaved = 0, swapCursor = -1;
        std::atomic<int> undoCount { 0 };
        std::atomic<bool> undone { false };
    };

    double sampleRate = 0.0;
    int maxLength = 0, fadeLength = 240;
    std::array<Loop, kLoops> loops;

    std::atomic<int> selected { 0 };
    bool sync = true, reverse = false, halfSpeed = false;
    rkfx::Smoothed level { 1.0f };

    // Sync clock: Loop 1's position while anything runs; it wraps on the downbeat.
    double clock = 0.0;
    bool clockRunning = false;
    int currentPart = -1, pendingPart = -1;

    bool isRunning (int i) const noexcept;
    bool anyRunning() const noexcept;
    bool startsOnDownbeat (int i) const noexcept;

    void beginRecording (int i) noexcept;
    void endRecording (int i, bool thenPlay) noexcept;
    void closeRecording (int i, int newLength) noexcept;
    void playFrom (int i, int offset) noexcept;   // plays, @p offset samples into the loop
    void startPlaying (int i) noexcept;
    void requestRecording (int i) noexcept;
    void requestPlaying (int i) noexcept;
    void startOverdub (int i) noexcept;
    void fadeOut (int i, AfterFade) noexcept;
    void stopOtherParts (int except) noexcept;
    void continueSwap (int i) noexcept;
    float read (const Loop&, int channel, double pos) const noexcept;
};
