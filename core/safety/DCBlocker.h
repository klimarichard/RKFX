#pragma once

#include <cmath>

namespace rkfx
{

/** One-pole DC blocker (y = x - x1 + R * y1) for one channel. Used wherever
    feedback or asymmetric shaping can create a DC offset. The corner sits low
    enough (default 10 Hz) to leave bass untouched. */
class DCBlocker
{
public:
    void prepare (double sampleRate, double cornerHz = 10.0) noexcept
    {
        r = (float) std::exp (-2.0 * 3.14159265358979323846 * cornerHz / sampleRate);
        reset();
    }

    void reset() noexcept { x1 = y1 = 0.0f; }

    float process (float x) noexcept
    {
        const auto y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

} // namespace rkfx
