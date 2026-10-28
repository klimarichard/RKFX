#pragma once

#include "dsp/Dynamics.h"
#include "dsp/Oversampler.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKLimit DSP: a zero-latency peak limiter (latency only with the
    optional oversampling).

        input gain -> peak limiter (instant attack, smoothed release,
        stereo link) -> oversampled safety clipper -> hard ceiling

    No lookahead, so the two stages share the work: the limiter holds peaks
    at 0.3 dB under the ceiling; the clipper is exactly linear up to there
    and bends smoothly into the ceiling above it, catching what the limiter
    can't (with oversampling, also the peaks between samples). A final clamp
    at the ceiling makes the guarantee absolute, even after the
    oversampling filters. GUI-free and allocation-free. */
class LimitEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr float kHeadroomDb = 0.3f;   // limiter threshold below the ceiling

    struct Settings
    {
        float inputDb = 0.0f;
        float ceilingDb = -1.0f;
        float releaseMs = 50.0f;
        bool autoRelease = false;
        float link = 1.0f;          // 0..1
        int oversampling = 1;       // 1, 2 or 4
    };

    /** The safety clipper: linear up to @p knee, then smoothly up to (never past) @p ceiling. */
    static float clip (float x, float knee, float ceiling) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    int latencySamples() const noexcept { return rkfx::Oversampler::latencySamples (factor); }

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    /** Most gain reduction since the last call (dB, <= 0); resets. */
    float takeGainReductionDb() noexcept;

private:
    double sampleRate = 48000.0;
    Settings settings;
    std::array<rkfx::Dynamics::GainSmoother, kMaxChannels> smoothers;
    rkfx::Oversampler oversampler;
    int factor = 1;

    rkfx::SmoothedGain inputGain { 1.0f }, ceiling { 0.891f };
    rkfx::Smoothed link { 1.0f };
    float peakReduction = 0.0f;
};
