#pragma once

#include <cmath>

namespace rkfx
{

/** Tone tilt for one channel: one knob from dark to bright, pivoting around
    about 1 kHz (lows and highs move in opposite directions, approaching
    +/-6 dB well away from the pivot; about 4 dB at 10 kHz). A one-pole split,
    so it is cheap and adds no latency.
    tone = 0 passes the signal exactly. */
class Tilt
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        coef = (float) std::exp (-2.0 * 3.14159265358979323846 * kPivotHz / sampleRate);
        reset();
    }

    void reset() noexcept { low = 0.0f; }

    /** @param tone  -1 (dark) to +1 (bright) */
    void setTone (float tone) noexcept
    {
        highGain = std::pow (10.0f, tone * kMaxDb / 20.0f);
        lowGain = 1.0f / highGain;
        flat = std::fabs (tone) < 1.0e-6f;
    }

    float process (float x) noexcept
    {
        low = x + coef * (low - x);
        return flat ? x : low * lowGain + (x - low) * highGain;
    }

    static constexpr double kPivotHz = 1000.0;
    static constexpr float kMaxDb = 6.0f;

private:
    double sampleRate = 48000.0;
    float coef = 0.0f, low = 0.0f, lowGain = 1.0f, highGain = 1.0f;
    bool flat = true;
};

} // namespace rkfx
