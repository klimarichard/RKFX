#pragma once

#include "dsp/DelayLine.h"
#include "dsp/RandomModulator.h"
#include "dsp/Tilt.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKDouble DSP: artificial double tracking.

    The dry signal passes untouched (zero latency); a delayed copy, the
    "double", drifts randomly in time around the base delay, so it sounds
    like a second take. The drift's small pitch variations come naturally
    from the changing delay. Then tone tilt and level on the double, and
    stereo placement: spread pans the dry towards the left and the double
    towards the right.

    The double always lags the original (never leads: that would need
    lookahead). Output = dry + double. GUI-free, allocation-free. */
class DoubleEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr float kMaxDelayMs = 60.0f;
    static constexpr float kMaxDriftMs = 5.0f;

    struct Settings
    {
        float delayMs = 25.0f;
        float drift = 0.4f;          // 0..1 (up to +/-5 ms)
        float driftHz = 0.5f;
        float tone = -0.2f;          // -1..1
        float spread = 0.6f;         // 0 = both centred, 1 = dry left, double right
        float levelDb = -3.0f;
    };

    /** Balance-style pan gains (centre = unity on both sides). */
    static void panGains (float pan, float& left, float& right) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    double sampleRate = 48000.0;
    Settings settings;

    std::array<rkfx::DelayLine, kMaxChannels> lines;
    std::array<rkfx::Tilt, kMaxChannels> tilts;
    rkfx::RandomModulator drift;

    rkfx::Smoothed delaySamples { 1200.0f }, driftSamples, spread { 0.6f };
    rkfx::SmoothedGain level { 0.7f };
};
