#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Scales.h"
#include "dsp/TempoSync.h"

#include <array>

/** The RKEcho MIDI processing: every note played schedules its repeats, one
    each echo time, transposed by Step (semitones or scale steps) and
    quieter by Feedback, on the played channel or the echo channel(s).

    The played notes pass through at once (or not, Original off); everything
    else passes untouched. A note number on a channel has one owner at a
    time: a repeat or played note that takes it ends the previous one first,
    and a note-off from a former owner is dropped, so no synth ever gets two
    note-ons for one note or loses a note early.

    At most kMaxEvents are scheduled; when full, the earliest pending repeat
    is dropped (never a note-off). Allocation-free after prepare(). */
class EchoEngine
{
public:
    static constexpr int kMaxEvents = 256;
    static constexpr int kMaxRepeats = 16;
    static constexpr int kMaxSources = 64;     // played notes remembered (for As played lengths)
    static constexpr float kAsPlayedCap = 0.9f; // an As played repeat lasts at most this share of the echo time

    struct Settings
    {
        float timeMs = 375.0f;
        bool sync = true;
        int division = rkfx::SyncDivision::index (8, 'd');
        int repeats = 4;
        float feedback = 0.7f;        // 0.1..1
        int velocityMin = 10;
        int step = 0;                 // -12..+12
        bool scaleSteps = false;
        int key = 0;                  // 0 = C
        rkfx::scales::Scale scale = rkfx::scales::Scale::major;
        bool asPlayed = true;         // Length: As played (else Gate)
        float gate = 0.5f;            // 0.1..1 of the echo time
        float swing = 0.0f;           // 0..0.5 of the echo time, on every second repeat
        bool original = true;
        int channel = 0;              // 0 = the played note's channel
        int alternate = 0;            // 0 = off
    };

    /** The note @p steps steps from @p note: semitones, or scale steps in the key. -1 outside 0..127. */
    static int transpose (int note, int steps, const Settings&);

    /** The repeats of a note (for the display and tests): note -1 marks the end. */
    struct Repeat { double offsetBeats; int note, velocity, channel; };
    static int planRepeats (const Settings&, int note, int velocity, int channel, double echoBeats,
                            std::array<Repeat, kMaxRepeats>& out);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings& s) { settings = s; }

    void process (juce::MidiBuffer&, int numSamples, const rkfx::Transport&) noexcept;

    /** Ends every sounding repeat and drops what's pending (bypass). */
    void releaseAll (juce::MidiBuffer&) noexcept;

    /** The last note played (for the display), -1 before any. */
    int getLastNote() const noexcept { return lastNote; }
    int getLastVelocity() const noexcept { return lastVelocity; }

private:
    struct Event
    {
        double time = 0.0;            // absolute samples
        bool on = false;
        int channel = 1, note = 60, velocity = 0;
        int source = -1;              // the played note it echoes (for As played)
        std::uint32_t owner = 0;      // note-off: the note-on it ends
        bool used = false;
    };

    struct Source { double start = 0.0, end = -1.0; bool used = false; };

    double sampleRate = 48000.0;
    Settings settings;
    std::array<Event, kMaxEvents> events {};
    int numEvents = 0;
    double nextDue = 1.0e300;
    std::array<Source, kMaxSources> sources {};
    int nextSource = 0;
    std::array<std::array<std::uint32_t, 128>, 16> owners {};   // per channel and note: who sounds it (0 = nobody)
    std::array<std::array<int, 128>, 16> heldSource {};         // played notes held down: their source index
    std::array<std::array<std::uint32_t, 128>, 16> heldOwner {}; // and the owner token of their pass-through note-on
    std::uint32_t nextOwner = 1;
    double now = 0.0;
    int lastNote = -1, lastVelocity = 100;
    juce::MidiBuffer out;

    bool schedule (const Event&) noexcept;
    void dropEarliestRepeat() noexcept;
    void noteOn (int channel, int note, int velocity, int sample, std::uint32_t& owner) noexcept;
    void emitDue (int sample, double echoSamples) noexcept;
};
