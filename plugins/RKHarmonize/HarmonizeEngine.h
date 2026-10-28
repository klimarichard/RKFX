#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Scales.h"

#include <array>

/** The RKHarmonize MIDI processing: up to two harmony notes for every note
    played, in a key and scale, so they follow the melody the way a second
    and third player would.

    - Seven-note scales move diatonically: a "3rd above" is a major or minor
      third as the key requires. A note outside the scale keeps its distance
      from the scale note below it (chromatic passing notes stay parallel).
    - Pentatonic and blues scales: the interval is taken as in a major scale
      and the harmony moves to the nearest note of the scale.
    - Chromatic: fixed intervals (a 3rd is always a major third).
    - Octaves are always 12 semitones.

    The harmony notes are on the played note's channel, at a share of its
    velocity. Each sounding pitch is counted, so two notes landing on the
    same pitch don't cut each other off: a note-on goes out when a pitch
    starts sounding, a note-off when its last note ends. Everything that
    isn't a note passes through. GUI-free and allocation-free after
    prepare(). */
class HarmonizeEngine
{
public:
    // The key and scale logic lives in core/dsp/Scales.h, shared with RKVoice.
    using Scale = rkfx::scales::Scale;
    static constexpr int kNumScales = rkfx::scales::kNumScales;

    /** Harmony intervals: off, then below and above. */
    using Interval = rkfx::scales::Interval;
    static constexpr int kNumIntervals = rkfx::scales::kNumIntervals;

    struct Settings
    {
        int key = 0;                        // 0 = C .. 11 = B
        Scale scale = Scale::major;
        Interval voice1 = Interval::thirdAbove;
        Interval voice2 = Interval::off;
        float velocity = 0.8f;              // the harmony's share of the played velocity
        bool original = true;               // the played note itself
    };

    /** The harmony for @p note, or -1 (off, or outside 0..127). */
    static int harmonyNote (int note, int key, Scale, Interval) noexcept;

    void prepare();
    void reset();
    void setSettings (const Settings& s) noexcept { settings = s; }

    /** Transforms one block's MIDI in place. */
    void process (juce::MidiBuffer&) noexcept;

    /** Note-offs (at position 0) for every pitch this engine is sounding. */
    void releaseAll (juce::MidiBuffer&) noexcept;

private:
    static constexpr int kVoices = 3;     // the original, two harmonies

    Settings settings;
    juce::MidiBuffer out;
    std::array<std::array<uint8_t, 128>, 16> sounding {};                       // per channel and pitch
    std::array<std::array<std::array<int8_t, kVoices>, 128>, 16> started {};    // per channel and played note: the pitches it started

    void noteOn (int channel, int note, juce::uint8 velocity, int position) noexcept;
    void noteOff (int channel, int note, int position) noexcept;
    void clearChannel (int channel) noexcept;
};
