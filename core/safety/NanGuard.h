#pragma once

#include <cmath>

namespace rkfx
{

/** True when every sample in the given channels is a finite number.
    Uses the fact that (x - x) is 0 for finite x but NaN for NaN or infinity.
    Eight independent sums (one per lane) let the compiler vectorise the loop:
    a single running sum can't be, as float addition must keep its order. */
inline bool allFinite (const float* const* channels, int numChannels, int numSamples) noexcept
{
    float lanes[8] {};

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* d = channels[ch];
        int i = 0;
        for (; i + 8 <= numSamples; i += 8)
            for (int j = 0; j < 8; ++j)
                lanes[j] += d[i + j] - d[i + j];
        for (; i < numSamples; ++i)
            lanes[0] += d[i] - d[i];
    }

    float acc = 0.0f;
    for (const auto lane : lanes)
        acc += lane;
    return acc == 0.0f;
}

} // namespace rkfx
