#pragma once

#include "safety/Smoothing.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

/** The RKLoop DSP: a live looper, one loop, zero latency.

    One footswitch runs it like a classic looper pedal:

        empty --Rec--> recording --Rec--> playing <--Rec--> overdubbing
                                   |                          |
                                   +---Stop--> stopped <------+   (Rec in stopped: play from the start)

    - Recording sets the loop's length; the first loop gets 5 ms fades at
      both ends, so its seam never clicks. Reaching the maximum length
      closes the loop and plays it.
    - Overdubbing adds the input to the loop. Before a sample is first
      changed in an overdub session, its old value is saved, so Undo can
      swap the session out again (and Redo back in). The swap runs a few
      hundred samples per audio sample (about 0.2 s for a full minute), so
      no block ever stalls.
    - Reverse plays (and overdubs) backwards; Half Speed plays an octave
      down at half speed (overdubbing is off while it is on).
    - Stop and Clear fade out over 5 ms; playing fades in.
    - The input always passes through; the loop is added at Loop Level.

    Memory for the maximum length is allocated in prepare() (never while
    processing); a prepare() at the same sample rate keeps the loop. The
    loop's audio is not part of the saved state. */
class LoopEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr double kMaxSeconds = 60.0;
    static constexpr double kFadeMs = 5.0;
    static constexpr int kSwapPerSample = 256;

    enum class State { empty, recording, playing, overdubbing, stopped };
    enum class Button { record, stop, undo, clear };

    void prepare (double sampleRate);
    void reset();                         // stops playback (keeps the loop)

    /** A footswitch press (call from the audio thread, before process()). */
    void press (Button) noexcept;

    void setReverse (bool on) noexcept    { reverse = on; }
    void setHalfSpeed (bool on) noexcept  { halfSpeed = on; }
    void setLevel (float gain) noexcept   { level.setTargetValue (gain); }

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    // For the display and tests (any thread).
    State getState() const noexcept            { return state.load(); }
    int getLength() const noexcept             { return length.load(); }
    double getPosition() const noexcept        { return displayPosition.load(); }
    int getRecordedLength() const noexcept     { return state == State::recording ? writeIndex.load() : length.load(); }
    bool canUndo() const noexcept              { return undoCount.load() > 0 && ! undone.load(); }
    bool canRedo() const noexcept              { return undoCount.load() > 0 && undone.load(); }
    int getMaxLength() const noexcept          { return maxLength; }

private:
    double sampleRate = 0.0;
    int maxLength = 0, fadeLength = 240;
    juce::AudioBuffer<float> loop, undoBuffer;

    std::atomic<State> state { State::empty };
    std::atomic<int> length { 0 }, writeIndex { 0 };
    std::atomic<double> displayPosition { 0.0 };
    double position = 0.0;
    bool reverse = false, halfSpeed = false;

    // Fades: playback gain (5 ms), and what to do once it is silent.
    rkfx::Smoothed playGain, level { 1.0f };
    enum class AfterFade { nothing, stop, clear } afterFade = AfterFade::nothing;

    // Undo: the samples an overdub session changed, by position.
    int undoStart = 0, sessionSaved = 0;
    std::atomic<int> undoCount { 0 };
    std::atomic<bool> undone { false };
    int swapCursor = -1;                  // >= 0 while swapping

    void closeRecording() noexcept;
    void startPlaying (bool fromStart) noexcept;
    void startOverdub() noexcept;
    void continueSwap() noexcept;
    float read (int channel, double pos) const noexcept;
    int wrap (int index) const noexcept { return (index % length + length) % length; }
};
