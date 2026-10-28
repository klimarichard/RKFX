#pragma once

#include "dsp/Crossover.h"
#include "dsp/DelayLine.h"
#include "dsp/Oversampler.h"
#include "safety/DCBlocker.h"
#include "safety/Smoothing.h"

#include <array>
#include <vector>

/** The RKRotary DSP: a Leslie-style rotary speaker (mono in, stereo out).

        mono input -> drive (tube curve, oversampled; skipped at 0%)
        -> crossover at 800 Hz -> horn (highs) and drum (lows) rotors
        -> two virtual mics on opposite sides of the cabinet
        -> horn/drum balance -> stereo width

    Each rotor sweeps a delay line (Doppler) and its volume as it turns
    towards and away from each mic; the horn also gets darker facing away.
    Rotor positions are integrated per sample; their speeds ease towards the
    target with their own acceleration times (the drum is heavier and lags),
    and brake slows both to a stop. Mic distance softens the movement.

    The drive's oversampling sets a constant latency (latencySamples()):
    with drive at 0% the oversampler is skipped and replaced by a plain delay
    of the same length, so turning the drive up never changes the latency.
    GUI-free and allocation-free after prepare(). */
class RotaryEngine
{
public:
    enum class Speed { slow, fast, brake };

    static constexpr float kCrossoverHz = 800.0f;
    static constexpr float kDrumRatio = 0.85f;   // the drum turns a little slower than the horn

    struct Settings
    {
        Speed speed = Speed::slow;
        float slowHz = 0.8f;
        float fastHz = 6.5f;
        float hornAccelSeconds = 0.8f;
        float drumAccelSeconds = 4.0f;
        float balance = 0.0f;        // -1 drum .. +1 horn
        float distance = 0.5f;       // 0 close .. 1 distant
        float width = 0.8f;          // 0..1
        float drive = 0.0f;          // 0..1
        int oversampling = 2;        // 1, 2 or 4
    };

    void prepare (double sampleRate, int maxBlockSize = 4096);
    void reset();
    void setSettings (const Settings&);

    int latencySamples() const noexcept { return rkfx::Oversampler::latencySamples (factor); }

    /** Current rotor speeds (Hz), for tests and displays. */
    float getHornHz() const noexcept { return (float) hornHz; }
    float getDrumHz() const noexcept { return (float) drumHz; }

    /** Writes stereo into io[0] and io[1] (or mono into io[0]). */
    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    void processChunk (float* const* io, int numChannels, int numSamples) noexcept;

    struct Rotor
    {
        rkfx::DelayLine line;
        double angle = 0.0;            // double: tiny per-sample steps must never stall
        std::array<float, 2> lowPass {};   // per mic (horn directional darkening)
    };

    double sampleRate = 48000.0;
    Settings settings;

    rkfx::Crossover crossover;
    rkfx::Oversampler oversampler;
    rkfx::DelayLine cleanDelay;          // matches the oversampler's latency when drive is 0
    rkfx::DCBlocker driveDc;
    std::vector<float> scratch;
    int factor = 2;
    bool wasDriving = false;

    Rotor horn, drum;
    double hornHz = 0.8, drumHz = 0.8 * kDrumRatio;
    double hornAccelCoef = 0.0, drumAccelCoef = 0.0;
    float darkCoefFacing = 0.0f, darkCoefAway = 0.0f;

    rkfx::Smoothed drive, balance, distance, width;
};
