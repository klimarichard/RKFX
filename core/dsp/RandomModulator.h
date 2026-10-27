#pragma once

#include <cstdint>

namespace rkfx
{

/** Smooth random movement between -1 and +1, for drift, wow and flutter.

    Picks a new random target point about @c rate times per second and eases
    from one to the next (smoothstep), so the movement is smooth, never
    jumps, and never repeats in a recognisable cycle: the spacing of the
    points also varies randomly (70 to 130% of the nominal period). */
class RandomModulator
{
public:
    explicit RandomModulator (uint32_t seed = 0x2545f491u) : state (seed) {}

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        setRate (rateHz);
        reset();
    }

    void reset() noexcept
    {
        from = 0.0f;
        to = nextRandom();
        position = 0.0f;
        newSegment();
    }

    void setRate (float hz) noexcept
    {
        rateHz = hz;
        baseIncrement = (float) (hz / sampleRate);
    }

    float next() noexcept
    {
        position += increment;
        if (position >= 1.0f)
        {
            position -= 1.0f;
            from = to;
            to = nextRandom();
            newSegment();
        }

        const auto t = position;
        return from + (to - from) * t * t * (3.0f - 2.0f * t);
    }

private:
    double sampleRate = 48000.0;
    float rateHz = 0.5f, baseIncrement = 0.0f, increment = 0.0f;
    float from = 0.0f, to = 0.0f, position = 0.0f;
    uint32_t state;

    float nextRandom() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (state >> 8) / 8388608.0f - 1.0f;   // -1..1
    }

    void newSegment() noexcept
    {
        const auto jitter = 1.0f + 0.3f * nextRandom();     // 0.7..1.3
        increment = baseIncrement / jitter;
    }
};

} // namespace rkfx
