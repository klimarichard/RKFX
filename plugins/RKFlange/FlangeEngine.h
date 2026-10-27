#pragma once

#include "dsp/DelayLine.h"
#include "dsp/LFO.h"
#include "dsp/TempoSync.h"
#include "safety/DCBlocker.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKFlange DSP (wet signal only; the base processor does dry/wet).

    A short delay line whose delay sweeps around the Manual time, following
    an LFO on an exponential curve (up to two octaves either way at 100%
    depth), with feedback (positive or negative) through a DC blocker and
    soft clipper back into the delay. The right channel's LFO runs offset by
    the stereo phase. With depth 0 it's a fixed, tunable comb filter.

    No through-zero mode (it would need the dry path delayed). GUI-free and
    allocation-free after prepare(). */
class FlangeEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr float kMaxDelayMs = 40.0f;
    static constexpr float kSweepOctaves = 2.0f;

    struct Settings
    {
        float rateHz = 0.3f;
        float depth = 0.6f;          // 0..1
        float manualMs = 2.0f;
        float feedback = 0.5f;       // -0.95..0.95
        bool sync = false;
        int division = rkfx::SyncDivision::index (2);
        float phaseDeg = 90.0f;
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples, const rkfx::Transport&) noexcept;

private:
    double sampleRate = 48000.0;
    Settings settings;

    std::array<rkfx::DelayLine, kMaxChannels> lines;
    std::array<rkfx::DCBlocker, kMaxChannels> dcBlockers;
    std::array<rkfx::LFO, kMaxChannels> lfos;

    rkfx::Smoothed manualSamples { 96.0f }, depth { 0.6f }, feedback { 0.5f };
};
