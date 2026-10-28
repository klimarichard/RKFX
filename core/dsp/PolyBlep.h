#pragma once

namespace rkfx
{

/** PolyBLEP residual for a step at phase 0: add it to a naive waveform's
    jump to remove most of the aliasing (t = phase 0..1, dt = phase step per
    sample). Used by audio-rate oscillators (RKSynth, RKRingMod). */
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/** An anti-aliased square, +1 for the first half of the cycle. */
inline float polyBlepSquare (float t, float dt) noexcept
{
    const auto shifted = t + 0.5f >= 1.0f ? t - 0.5f : t + 0.5f;
    return (t < 0.5f ? 1.0f : -1.0f) + polyBlep (t, dt) - polyBlep (shifted, dt);
}

} // namespace rkfx
