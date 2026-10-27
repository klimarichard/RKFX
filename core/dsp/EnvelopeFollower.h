#pragma once

#include <cmath>

namespace rkfx
{

/** Peak envelope follower: rectifies the input and smooths it with separate
    attack and release times (one-pole each). Feed it one detector sample per
    call; for stereo-linked detection feed max(|L|, |R|). */
class EnvelopeFollower
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        setTimes (attackMs, releaseMs);
        reset();
    }

    void reset() noexcept { envelope = 0.0f; }

    void setTimes (float newAttackMs, float newReleaseMs) noexcept
    {
        attackMs = newAttackMs;
        releaseMs = newReleaseMs;
        attackCoef = coefficient (attackMs);
        releaseCoef = coefficient (releaseMs);
    }

    float process (float detector) noexcept
    {
        const auto x = std::abs (detector);
        const auto coef = x > envelope ? attackCoef : releaseCoef;
        envelope = x + coef * (envelope - x);
        return envelope;
    }

    float getEnvelope() const noexcept { return envelope; }

    /** Maps a level to 0..1 on a dB scale: @p floorDb (and below) -> 0,
        0 dBFS -> 1. Used to drive filter sweeps (auto-wah, envelope phaser),
        so quiet and loud playing spread evenly over the sweep. */
    static float toUnit (float level, float floorDb = -48.0f) noexcept
    {
        const auto db = 20.0f * std::log10 (level + 1.0e-9f);
        return std::fmin (1.0f, std::fmax (0.0f, (db - floorDb) / -floorDb));
    }

private:
    double sampleRate = 48000.0;
    float attackMs = 5.0f, releaseMs = 150.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f, envelope = 0.0f;

    float coefficient (float ms) const noexcept
    {
        return ms <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (0.001 * (double) ms * sampleRate));
    }
};

} // namespace rkfx
