#pragma once

#include "dsp/EnvelopeFollower.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKOctave DSP: an analog-style octave divider, with zero latency.

    - Tracking: the input (summed to mono) is band-limited to the range of
      the instrument: a high-pass below its lowest note and a 24 dB/oct
      low-pass that follows the detected pitch (1.25 x the fundamental), so
      the fundamental dominates and the harmonics don't add zero crossings.
      A Schmitt trigger (hysteresis at 30% of the filtered level) finds one
      rising crossing per cycle.
    - Dividing: a flip-flop toggles once per cycle, a square at half the
      input frequency (Octave 1); a second flip-flop toggles on the first
      one's rising edges (Octave 2, two octaves down). It toggles at the
      input's own next rising zero crossing (the tracking filter lags behind
      it), so Natural flips the input's sign where it is zero: no buzz.
      Each edge ramps over an eighth of a cycle (at most 2 ms).
    - Voices: Natural multiplies the input by the squares (as classic
      octave pedals do), keeping the instrument's own timbre an octave
      lower. Synth outputs the squares themselves at the input's level, a
      pure sub-oscillator tone.
    - Below the threshold the octave voices fade out and the tracker
      resets, so noise and tails never produce garbled low notes.
    - Tone: a 24 dB/oct low-pass on the octave voices.

    Monophonic: chords and double stops confuse the tracker, as on any
    analog octave pedal. GUI-free and allocation-free after prepare(). */
class OctaveEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr float kHysteresisDb = 3.0f;

    enum class Range { bass, guitar, violin };
    enum class Voice { natural, synth };

    struct Settings
    {
        Range range = Range::guitar;
        Voice voice = Voice::natural;
        float direct = 1.0f;          // 0..1, the dry signal
        float octave1 = 0.8f;         // 0..1, one octave down
        float octave2 = 0.0f;         // 0..1, two octaves down
        float toneHz = 1500.0f;
        float thresholdDb = -50.0f;
    };

    /** Lowest and highest fundamental a range tracks, in Hz. */
    struct Limits { float lowHz, highHz; };
    static Limits limitsFor (Range) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    /** The tracked fundamental in Hz (0 while below the threshold), for tests. */
    float getTrackedHz() const noexcept { return gateOpen ? estimateHz : 0.0f; }

private:
    static constexpr int kUpdateInterval = 16;   // samples between coefficient updates

    double sampleRate = 48000.0;
    Settings settings;
    Limits limits = limitsFor (Range::guitar);

    // Tracker (mono).
    rkfx::StateVariableFilter highPass, trackLow1, trackLow2;
    rkfx::EnvelopeFollower trackLevel, inputLevel, gateLevel;
    bool high = false, gateOpen = false, togglePending = false;
    int samplesSinceRise = 0, pendingFor = 0;
    float lastBand = 0.0f;
    float estimateHz = 0.0f;
    float square1 = 1.0f, square2 = 1.0f;        // flip-flop targets
    float edge1 = 1.0f, edge2 = 1.0f;            // ramped squares
    float step1 = 1.0f, step2 = 1.0f;
    float gateGain = 0.0f, gateAttack = 0.0f, gateRelease = 0.0f;
    int untilUpdate = 0;

    // Output, per channel.
    std::array<rkfx::StateVariableFilter, kMaxChannels> tone1, tone2;
    rkfx::Smoothed direct, octave1, octave2, toneHz;

    void resetTracker() noexcept;
    void updateCoefficients() noexcept;
    void track (float mono) noexcept;
    void toggle() noexcept;
    float rampSamples() const noexcept;
};
