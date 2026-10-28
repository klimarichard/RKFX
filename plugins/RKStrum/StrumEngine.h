#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

/** The RKStrum MIDI processing: chords played on a keyboard come out spread
    in time, one note after another, like a strummed guitar.

    - Notes starting within the Window count as one chord. When the window
      closes, the chord plays in strum order: down (low to high, like a
      downstroke), up (high to low), or alternating between chords; each
      note Speed after the one before, with an optional velocity ramp and a
      little random timing (Humanize).
    - The first note waits for the window to close (the chord's order isn't
      known before): 10 ms by default, so a single note is that late too.
    - A note released before it has sounded still sounds briefly (20 ms), so
      a quick chord is never lost.

    Everything that isn't a note passes through at once. GUI-free and
    allocation-free after prepare(). */
class StrumEngine
{
public:
    static constexpr int kMaxChord = 16;
    static constexpr int kMaxQueue = 256;
    static constexpr double kMinLengthMs = 20.0;

    enum class Direction { down, up, alternate };

    struct Settings
    {
        Direction direction = Direction::down;
        float speedMs = 20.0f;          // between notes
        float windowMs = 10.0f;         // notes this close together are one chord
        float ramp = 0.0f;              // -0.5..0.5: velocity change from the first note to the last
        float humanize = 0.0f;          // 0..1: random timing, up to half the speed
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings& s) noexcept { settings = s; }

    void process (juce::MidiBuffer&, int numSamples) noexcept;
    void releaseAll (juce::MidiBuffer&) noexcept;

private:
    struct Pending { int note, channel, velocity; bool released = false; };
    struct Scheduled { long long time; juce::MidiMessage message; };

    double sampleRate = 48000.0;
    Settings settings;
    juce::MidiBuffer out;
    long long now = 0;                       // samples since prepare

    std::array<Pending, kMaxChord> chord {};
    int chordSize = 0;
    long long windowEnd = -1;
    bool nextUp = false;                     // alternate: the next chord goes up

    std::array<Scheduled, kMaxQueue> queue {};
    int queueSize = 0;
    std::array<std::array<long long, 128>, 16> startsAt {};   // when a note's (queued) note-on plays, -1 = not queued
    std::array<std::array<bool, 128>, 16> sounding {};
    juce::Random random { 0x57a1 };

    void schedule (long long time, const juce::MidiMessage&) noexcept;
    void strum() noexcept;
    void emitDue (int position) noexcept;
};
