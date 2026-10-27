#pragma once

#include <cmath>

namespace rkfx
{

/** True when every sample in the given channels is a finite number.
    Uses the fact that (x - x) is 0 for finite x but NaN for NaN or infinity,
    so the loop is a plain branch-free sum the compiler can vectorise. */
inline bool allFinite (const float* const* channels, int numChannels, int numSamples) noexcept
{
    float acc = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* d = channels[ch];
        for (int i = 0; i < numSamples; ++i)
            acc += d[i] - d[i];
    }

    return acc == 0.0f;
}

} // namespace rkfx
