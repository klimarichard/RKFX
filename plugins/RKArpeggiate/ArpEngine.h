#pragma once

#include "dsp/TempoSync.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

/** The RKArpeggiate MIDI processing: the notes held down, played one after
    another (or together, as a repeated chord) at a tempo division.

    - Pattern: up, down, up and down (the ends not repeated), as played,
      random, or chord; over 1 to 4 octaves.
    - Timing: steps of a note value at the host's tempo. While the host
      plays they lock to its beat (a new arpeggio starts on the next step);
      when it doesn't, an arpeggio starts at once and runs at the tempo.
      Swing delays every second step (50% straight, 66% triplet feel).
    - Gate: how long each note sounds, as a share of its step.
    - Latch: the notes keep playing after the keys are released, until a new
      chord is played (after all keys were up).
    - Velocity: as played, or fixed.

    Played notes are taken in (not passed on); everything else passes
    through. Events are handled at their sample position, and steps start on
    the exact sample. GUI-free and allocation-free after prepare(). */
class ArpEngine
{
public:
    static constexpr int kMaxHeld = 32;
    static constexpr int kMaxSequence = kMaxHeld * 4;

    enum class Pattern { up, down, upDown, played, random, chord };

    struct Settings
    {
        Pattern pattern = Pattern::up;
        int division = rkfx::SyncDivision::index (16);
        int octaves = 1;                    // 1..4
        float gate = 0.75f;                 // 0.05..1
        float swing = 0.5f;                 // 0.5..0.75
        bool latch = false;
        bool fixedVelocity = false;
        int velocity = 100;                 // 1..127, when fixed
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&) noexcept;

    /** Transforms one block's MIDI in place. */
    void process (juce::MidiBuffer&, int numSamples, const rkfx::Transport&) noexcept;

    /** Note-offs (at position 0) for every note it is sounding; forgets the held notes. */
    void releaseAll (juce::MidiBuffer&) noexcept;

    /** Where step @p k starts, in steps (swing moves the odd ones). */
    static double stepStart (long long k, float swing) noexcept;

private:
    struct Note { int note = 0, channel = 1, velocity = 100; };

    double sampleRate = 48000.0;
    Settings settings;
    juce::MidiBuffer out;

    std::array<Note, kMaxHeld> held {};          // in the order played
    int numHeld = 0, keysDown = 0;
    std::array<Note, kMaxSequence> sequence {};
    int sequenceLength = 0;
    bool sequenceDirty = false;

    std::array<Note, kMaxSequence> soundingNotes {};
    int numSounding = 0;
    double offAt = 0.0;                          // in steps

    bool running = false;
    double phase = 0.0;                          // in steps
    long long nextStep = 0, stepCount = 0;
    int lastRandom = -1;
    juce::Random random { 0x5eed };

    void rebuildSequence() noexcept;
    void keyDown (const Note&) noexcept;
    void keyUp (int note, int channel) noexcept;
    void startStep (int position) noexcept;
    void releaseSounding (int position) noexcept;
};
