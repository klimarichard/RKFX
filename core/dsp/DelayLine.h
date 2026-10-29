#pragma once

#include <cmath>
#include <vector>

namespace rkfx
{

/** A delay line for one channel with smooth fractional reads, for modulated
    effects (chorus, flanger, doubler, delay, reverb, vibrato).

    Power-of-two ring buffer, allocated in prepare() only. read() uses cubic
    Hermite interpolation: smooth under fast modulation and clean down to
    sub-millisecond delays (flanging). Write first, then read: read (1) is the
    sample written just before the latest one. Delays are clamped to at least
    one sample, because the interpolation needs one newer sample to work with. */
class DelayLine
{
public:
    /** Allocates for delays up to @p maxDelaySamples (plus interpolation room). */
    void prepare (int maxDelaySamples)
    {
        int size = 4;
        while (size < maxDelaySamples + 4)
            size *= 2;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writeIndex = 0;
    }

    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    int getMaxDelay() const noexcept { return mask - 3; }

    void write (float x) noexcept
    {
        writeIndex = (writeIndex + 1) & mask;
        buffer[(size_t) writeIndex] = x;
    }

    /** Adds @p x to the sample written @p samplesAgo ago (0 = the latest),
        so it is read that much sooner: lets a feedback path that adds its own
        delay (a pitch shifter) write early and keep the loop's timing. */
    void addAt (int samplesAgo, float x) noexcept
    {
        buffer[(size_t) ((writeIndex - samplesAgo) & mask)] += x;
    }

    /** Sample from @p delaySamples ago (fractional, clamped to 1..the line's length). */
    float read (float delaySamples) const noexcept
    {
        delaySamples = std::fmin (std::fmax (delaySamples, 1.0f), (float) getMaxDelay());

        const auto whole = (int) delaySamples;
        const auto t = delaySamples - (float) whole;
        const auto i = writeIndex - whole;

        const auto xm1 = at (i + 1);   // newer
        const auto x0  = at (i);
        const auto x1  = at (i - 1);   // older
        const auto x2  = at (i - 2);

        // Hermite (Catmull-Rom) between x0 and x1.
        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

private:
    std::vector<float> buffer;
    int mask = 0, writeIndex = 0;

    float at (int index) const noexcept { return buffer[(size_t) (index & mask)]; }
};

} // namespace rkfx
