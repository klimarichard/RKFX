#pragma once

#include "dsp/DelayLine.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/PitchTracker.h"
#include "dsp/RandomModulator.h"
#include "dsp/Scales.h"
#include "safety/Smoothing.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

/** The RKVoice DSP: up to four harmony voices, pitch-shifted copies of a
    single voice or instrument, placed by a held MIDI chord or by a key and
    scale (docs/design/RKVoice.md).

    - Tracking: the input, summed to mono, goes to the shared zero-latency
      pitch tracker (core/dsp/PitchTracker.h). Its cycle reports are the
      PSOLA pitch marks; the period is the time between two marks, trusted
      once two in a row agree (within kStablePeriod), so the first cycle of
      a note and a missed or doubled crossing don't count; after silence a
      note needs kOnsetPeriods steady periods (its onset rings at the wrong
      pitch for a cycle or two). The sung note is that pitch rounded to the
      nearest semitone, with hysteresis: a move of up to
      kHoldRangeSemitones counts once the pitch has been more than
      kHysteresisCents from the note for kHoldMs (so vibrato and scoops
      don't make the voices jump); a bigger one, a new note, at once.
    - Harmony: in Chord mode each voice takes the Nth chord tone above or
      below the sung note (strictly above or below: a sung chord tone is
      skipped). In Scale mode, the RKHarmonize interval (core/dsp/Scales.h).
      The voice keeps the singer's expression: it plays the sung pitch times
      the interval's ratio, so vibrato and slides carry over.
    - Shifting: PSOLA. Each grain is two input periods with a Hann window,
      centred on a pitch mark, and laid out at the voice's own period. A
      grain starts one period after its mark, so it never needs input that
      hasn't arrived: the voices trail the dry signal by one to two periods
      of the sung note, and nothing looks ahead.
    - No pitch (below the threshold, no cycle for a while) or no target (no
      chord held, with No Chord on Silent): the voices fade out over 20 ms.

    The dry signal is the input at the Dry level; the voices are added,
    each with its level and pan. GUI-free and allocation-free after
    prepare(). */
class VoiceEngine
{
public:
    static constexpr int kNumVoices = 4;
    static constexpr int kMaxGrains = 10;           // overlapping grains per voice (4x up needs 8)
    static constexpr int kMaxShift = 24;            // semitones, either way
    static constexpr float kHysteresisCents = 70.0f;
    static constexpr float kHoldMs = 40.0f;
    static constexpr float kFadeMs = 20.0f;
    static constexpr float kMaxHumanizeMs = 10.0f;
    static constexpr float kStablePeriod = 0.06f;      // two periods in a row within 6%
    static constexpr int kOnsetPeriods = 3;            // steady periods before a note (after silence) counts
    static constexpr float kHoldRangeSemitones = 2.0f; // moves up to this wait kHoldMs; bigger ones switch at once

    using Range = rkfx::PitchTracker::Range;
    using Scale = rkfx::scales::Scale;
    using Interval = rkfx::scales::Interval;

    enum class Mode { chord, scale };
    enum class ChordTones { anyOctave, asVoiced };
    enum class NoChord { silent, latch, scale };

    struct VoiceSettings
    {
        int position = 0;                   // Chord mode: -3..-1 below, 0 off, 1..3 above
        Interval interval = Interval::off;  // Scale mode
        float levelDb = -3.0f;
        float pan = 0.0f;                   // -1 (left) .. +1 (right)
    };

    struct Settings
    {
        Mode mode = Mode::chord;
        Range range = Range::voice;
        ChordTones chordTones = ChordTones::anyOctave;
        NoChord noChord = NoChord::silent;
        int key = 0;                        // 0 = C .. 11 = B
        Scale scale = Scale::major;
        std::array<VoiceSettings, kNumVoices> voices { { { 1, Interval::thirdAbove, -3.0f, -0.3f },
                                                         { -1, Interval::off, -3.0f, 0.3f },
                                                         { 0, Interval::off, -3.0f, -0.6f },
                                                         { 0, Interval::off, -3.0f, 0.6f } } };
        float detuneCents = 5.0f;
        float humanizeMs = 3.0f;
        float glideMs = 30.0f;
        float dryDb = 0.0f;
        float thresholdDb = -50.0f;
    };

    /** A set of MIDI notes (the held chord). */
    using NoteSet = std::array<bool, 128>;

    /** The chord tone @p position places from @p sung: 1..3 the first to third
        above, -1..-3 below (strictly: a sung chord tone doesn't count), or -1
        (off, an empty chord, nothing there, outside 0..127). */
    static int chordNote (int sung, int position, const NoteSet& chord, ChordTones) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    // MIDI (the caller filters the channel).
    void noteOn (int note) noexcept;
    void noteOff (int note) noexcept;
    void setSustain (bool down) noexcept;
    void allNotesOff() noexcept;

    /** Mixes the dry signal and the voices in place (stereo or mono). */
    void process (float* const* io, int numChannels, int numSamples) noexcept;

    // For the window (any thread) and the tests.
    int getSungNote() const noexcept { return sungShown.load (std::memory_order_relaxed); }
    /** The note voice @p v is singing, or -1 when it's silent. */
    int getVoiceNote (int v) const noexcept { return voiceShown[(size_t) v].load (std::memory_order_relaxed); }
    /** The pitch classes of the chord in use (bit 0 = C), 0 when none. */
    int getChordPitchClasses() const noexcept { return chordShown.load (std::memory_order_relaxed); }
    /** The tracked pitch (Hz), 0 when there is none. */
    float getTrackedHz() const noexcept { return pitchValid ? markHz : 0.0f; }

private:
    struct Grain
    {
        double inCentre = 0.0, outCentre = 0.0;   // absolute sample times
        float half = 0.0f;                        // half the grain length: one input period
    };

    struct Voice
    {
        int target = -1;                          // the note it sings, or -1
        float shift = 0.0f;                       // semitones, gliding
        bool shiftSet = false;
        float fade = 0.0f;
        double nextOut = -1.0;                    // the next grain's centre (absolute output time)
        std::array<Grain, kMaxGrains> grains {};
        int numGrains = 0;
        rkfx::DelayLine humanize;
        rkfx::RandomModulator wander;
        rkfx::Smoothed gainL, gainR;
    };

    double sampleRate = 48000.0;
    Settings settings;

    // Input history (mono), for the grains.
    std::vector<float> history;
    int historyMask = 0;
    std::int64_t now = 0;

    rkfx::PitchTracker tracker;
    rkfx::EnvelopeFollower inputLevel;
    bool gateOpen = false, pitchValid = false;
    std::int64_t markTime = -1, sinceMark = 0, lastCycle = -1;
    float markPeriod = 0.0f, markHz = 0.0f, lastInterval = 0.0f;
    int steadyCount = 0;   // steady periods in a row

    // The sung note, with hysteresis.
    int sungNote = -1, pendingNote = -1;
    std::int64_t pendingSince = 0;

    // The chord.
    std::array<int, 128> held {};
    NoteSet sustained {}, lastChord {};
    bool sustainDown = false, targetsDirty = true;

    std::array<Voice, kNumVoices> voices;
    std::array<float, 1025> hann {};
    rkfx::Smoothed dryGain;
    float fadeStep = 0.0f, glideCoef = 0.0f;

    std::atomic<int> sungShown { -1 }, chordShown { 0 };
    std::array<std::atomic<int>, kNumVoices> voiceShown;

    void trackSample (float x) noexcept;
    void updateTargets() noexcept;
    NoteSet chordInUse (bool& hasChord) const noexcept;
    float voiceSample (Voice&, int index) noexcept;
    float readHistory (double t) const noexcept;
    float window (float position) const noexcept;   // -1..1 -> Hann
    static float shiftFor (int sung, int target) noexcept;
};
