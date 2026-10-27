#pragma once

#include <cmath>
#include <cstdint>

namespace rkfx
{

/** The shared low-frequency oscillator. Output is bipolar, -1 to +1.

    Free-running at a rate in Hz, or locked to the host: syncTo() sets the
    phase from the host's beat position, so a synced LFO lands on the beat.
    Square and sample-and-hold are lightly smoothed (about 1.5 ms) so their
    jumps never click. */
class LFO
{
public:
    enum class Shape { sine, triangle, square, sawUp, sawDown, sampleHold };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        smoothCoef = (float) std::exp (-1.0 / (0.0015 * sampleRate));
        setRate (rateHz);
        reset();
    }

    void reset (double startPhase = 0.0) noexcept
    {
        phase = wrap (startPhase);
        held = nextRandom();
        smoothed = raw();
    }

    void setShape (Shape s) noexcept { shape = s; }

    void setRate (double hz) noexcept
    {
        rateHz = hz;
        increment = hz / sampleRate;
    }

    /** Locks the phase to the host: one cycle lasts @p beatsPerCycle quarter
        notes, counted from the start of the song, plus @p offset cycles. */
    void syncTo (double ppq, double beatsPerCycle, double offset = 0.0) noexcept
    {
        const auto newPhase = wrap (ppq / beatsPerCycle + offset);
        if (phase - newPhase > 0.5)
            held = nextRandom();   // crossed a cycle boundary: next sample-and-hold step
        phase = newPhase;
    }

    double getPhase() const noexcept { return phase; }

    /** Current value, then advances one sample. */
    float next() noexcept
    {
        const auto value = raw();
        const auto smooth = shape == Shape::square || shape == Shape::sampleHold;
        smoothed = smooth ? value + smoothCoef * (smoothed - value) : value;

        phase += increment;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = nextRandom();
        }

        return smoothed;
    }

private:
    double sampleRate = 48000.0, rateHz = 1.0, increment = 0.0, phase = 0.0;
    Shape shape = Shape::sine;
    float held = 0.0f, smoothed = 0.0f, smoothCoef = 0.0f;
    uint32_t seed = 0x12345678u;

    static double wrap (double p) noexcept { return p - std::floor (p); }

    float nextRandom() noexcept
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) (seed >> 8) / 8388608.0f - 1.0f;   // -1..1
    }

    float raw() const noexcept
    {
        const auto p = (float) phase;
        switch (shape)
        {
            case Shape::sine:       return std::sin (6.28318530718f * p);
            case Shape::triangle:   return p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f;
            case Shape::square:     return p < 0.5f ? 1.0f : -1.0f;
            case Shape::sawUp:      return 2.0f * p - 1.0f;
            case Shape::sawDown:    return 1.0f - 2.0f * p;
            case Shape::sampleHold: return held;
        }
        return 0.0f;
    }
};

} // namespace rkfx
