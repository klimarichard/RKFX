#pragma once

#include <cmath>

namespace rkfx
{

/** Soft clipper used inside every feedback loop (and anywhere a signal must
    stay bounded). A rational tanh approximation: slope 1 and practically
    transparent for small signals, smoothly saturating, and never exceeding
    +/-1. No transcendental calls, so it is cheap per sample. */
inline float softClip (float x) noexcept
{
    if (x >= 3.0f)  return 1.0f;
    if (x <= -3.0f) return -1.0f;

    const auto x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** Soft clip around an arbitrary ceiling (linear amplitude, > 0). */
inline float softClip (float x, float ceiling) noexcept
{
    return ceiling * softClip (x / ceiling);
}

} // namespace rkfx
