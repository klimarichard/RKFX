#pragma once

#include "dsp/EnvelopeFollower.h"
#include "dsp/LFO.h"
#include "dsp/PitchTracker.h"
#include "dsp/TempoSync.h"
#include "safety/DCBlocker.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKRingMod DSP: the input multiplied by a carrier, which shifts every
    partial to the sum and difference with the carrier (metallic, bell-like,
    clangorous; at low rates a choppy tremolo with polarity flips).

    - Carrier frequency, by mode:
        Free:    the Frequency knob, 1 Hz to 4 kHz;
        Tracked: the pitch being played (the shared pitch tracker) shifted
                 by Interval (-24 to +24 semitones), so the result stays in
                 key with the notes. A pitch counts once two cycles in a row
                 agree and the input is above -50 dBFS; between notes the
                 carrier holds the last one;
        Synced:  one carrier cycle per tempo division, locked to the beat
                 while the host plays.
    - Wave: sine (pure), triangle, or an anti-aliased square (harsher, more
      partials).
    - LFO: wobbles the carrier frequency by up to an octave either way.
    - The carrier frequency glides (log domain, about 20 ms), so knob moves
      and tracked notes never step; a DC blocker removes the offset a
      carrier at the input's own pitch leaves.

    The modulated signal is the wet signal (the base processor mixes the dry
    signal in with Dry/Wet). Zero latency; GUI-free and allocation-free
    after prepare(). */
class RingModEngine
{
public:
    static constexpr int kMaxChannels = 2;

    enum class Mode { free, tracked, synced };
    enum class Wave { sine, triangle, square };
    using Range = rkfx::PitchTracker::Range;

    struct Settings
    {
        Mode mode = Mode::free;
        Wave wave = Wave::sine;
        float frequencyHz = 440.0f;
        float intervalSemitones = 7.0f;
        int division = rkfx::SyncDivision::index (16);
        Range range = Range::guitar;
        float lfoDepth = 0.0f;        // 0..1 (up to +-1 octave)
        float lfoRateHz = 1.0f;
    };

    /** One carrier sample for a phase (0..1) and phase step. */
    static float carrier (Wave, float phase, float dt) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples, const rkfx::Transport&) noexcept;

    /** The carrier's frequency now (after the glide, before the LFO), for tests. */
    float getCarrierHz() const noexcept { return std::exp2 (logHz); }

    /** The carrier's last frequency with the LFO, for tests. */
    float getModulatedHz() const noexcept { return modulatedHz; }

private:
    double sampleRate = 48000.0;
    Settings settings;

    rkfx::PitchTracker tracker;
    rkfx::EnvelopeFollower inputLevel;
    rkfx::LFO lfo;
    rkfx::Smoothed lfoDepth;
    std::array<rkfx::DCBlocker, kMaxChannels> dc;

    double phase = 0.0;
    float logHz = 8.78f, glideCoef = 0.0f;     // log2 (440)
    float heldTrackedHz = 0.0f, lastCycleHz = 0.0f, modulatedHz = 0.0f;
};
