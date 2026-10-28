#pragma once

#include "dsp/EnvelopeFollower.h"
#include "dsp/PitchTracker.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

/** The RKSynth DSP: a monophonic analog-style synth played by the instrument
    (guitar synth, bass synth, violin synth), with zero latency.

    - Tracking: the input, summed to mono, goes to the shared pitch tracker
      (core/dsp/PitchTracker.h). A note sounds once its pitch is known: two
      cycles into it (about 50 ms for a bass's low E, under 10 ms from the
      middle of a guitar up), as with any tracking synth. Between notes, the
      synth glides from the old pitch to the new one (95% of the way in the
      Glide time). The threshold releases over about 2.5 periods of the range's
      lowest note, so the synth stops soon after the note.
    - Oscillator: saw, square or triangle at the played pitch shifted by
      -2 to +1 octaves, anti-aliased (PolyBLEP), plus a square sub-octave.
    - Level: Dynamics blends a steady level with the input's level, so the
      synth plays as evenly or as dynamically as wanted. Below the
      threshold it fades out and the tracker starts over for the next note.
    - Filter: a 24 dB/oct resonant low-pass whose cutoff an envelope opens
      on every new note (after silence, or a fresh attack during a phrase)
      and lets fall over the Decay time: the classic synth "bwow".

    The synth is the wet signal (the base processor mixes the dry signal in
    with Dry/Wet), the same on every channel. GUI-free and allocation-free
    after prepare(). */
class SynthEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr float kHysteresisDb = 3.0f;
    static constexpr float kMaxEnvOctaves = 4.0f;

    using Range = rkfx::PitchTracker::Range;
    enum class Wave { saw, square, triangle };

    struct Settings
    {
        Range range = Range::guitar;
        Wave wave = Wave::saw;
        int octave = 0;               // -2..+1
        float sub = 0.0f;             // 0..1
        float glideMs = 10.0f;
        float cutoffHz = 1200.0f;
        float resonance = 0.3f;       // 0..1
        float envAmount = 0.5f;       // 0..1 (up to 4 octaves)
        float decayMs = 300.0f;
        float dynamics = 0.7f;        // 0 = steady level, 1 = follows the playing
        float thresholdDb = -50.0f;
    };

    /** The threshold detector's release for a range (ms). */
    static float gateReleaseMs (Range) noexcept;

    /** PolyBLEP residual for a discontinuity at phase 0 (t in 0..1, dt = step). */
    static float polyBlep (float t, float dt) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    /** Writes the synth into every channel (the input is read first). */
    void process (float* const* io, int numChannels, int numSamples) noexcept;

    /** The pitch the oscillator plays (before the octave shift), 0 when silent; for tests. */
    float getPlayingHz() const noexcept { return amp > 1.0e-3f ? std::exp2 (logHz) : 0.0f; }

private:
    static constexpr int kUpdateInterval = 16;
    static constexpr float kSteadyLevel = 0.35f;     // about -9 dBFS at Dynamics 0

    double sampleRate = 48000.0;
    Settings settings;

    rkfx::PitchTracker tracker;
    rkfx::EnvelopeFollower gateLevel, inputLevel, onsetFast, onsetSlow;
    bool gateOpen = false, fresh = true, armed = true;

    float logHz = 7.0f;               // log2 of the playing pitch
    double phase = 0.0, subPhase = 0.0;
    float amp = 0.0f, ampAttack = 0.0f, ampRelease = 0.0f;
    float env = 0.0f, envSmooth = 0.0f, envDecay = 0.0f, envAttack = 0.0f, glideCoef = 0.0f;

    rkfx::StateVariableFilter filter1, filter2;
    rkfx::Smoothed cutoff, resonance, envAmount, sub;
    int untilUpdate = 0;

    void trigger() noexcept { env = 1.0f; }
    void updateFilter() noexcept;
    float oscillator (double hz) noexcept;
};
