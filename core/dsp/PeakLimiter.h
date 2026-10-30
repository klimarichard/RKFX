#pragma once

#include "Dynamics.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace rkfx
{

/** A zero-latency peak limiter: the gain falls at once to hold a peak at
    the ceiling (no lookahead) and recovers over the release; a clip at the
    ceiling is the last guard, so the output never exceeds it (in-ears). A
    stereo signal is limited by its louder side. */
class PeakLimiter
{
public:
    struct Settings
    {
        float ceilingDb = -1.0f, releaseMs = 100.0f;
    };

    void prepare (double rate)
    {
        sampleRate = rate;
        setSettings (settings);
        reset();
    }

    void reset() { gain = 1.0f; lowestGain = 1.0f; }

    void setSettings (const Settings& s)
    {
        settings = s;
        ceiling = rkfx::Dynamics::toGain (std::min (0.0f, s.ceilingDb));
        releaseCoef = rkfx::Dynamics::timeCoefficient (std::max (1.0f, s.releaseMs), sampleRate);
    }

    void process (float* const* io, int numSides, int numSamples) noexcept
    {
        for (int n = 0; n < numSamples; ++n)
        {
            float peak = 0.0f;
            for (int side = 0; side < numSides; ++side)
                peak = std::max (peak, std::abs (io[side][n]));

            const auto target = peak > ceiling ? ceiling / peak : 1.0f;
            gain = target < gain ? target : target + releaseCoef * (gain - target);
            lowestGain = std::min (lowestGain, gain);

            for (int side = 0; side < numSides; ++side)
                io[side][n] = std::clamp (io[side][n] * gain, -ceiling, ceiling);
        }
    }

    /** Most gain reduction since the last call (dB, <= 0); resets. */
    float takeGainReductionDb() noexcept
    {
        const auto db = rkfx::Dynamics::toDb (std::exchange (lowestGain, 1.0f));
        return std::min (0.0f, db);
    }

private:
    Settings settings;
    double sampleRate = 48000.0;
    float ceiling = 1.0f, releaseCoef = 0.0f, gain = 1.0f, lowestGain = 1.0f;
};

} // namespace rkfx
