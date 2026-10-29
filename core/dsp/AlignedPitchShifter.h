#pragma once

#include "DelayLine.h"

#include <array>
#include <cmath>

namespace rkfx
{

/** A polyphonic pitch shifter that keeps the pitch exact with short windows
    (RKPolyPitch), for up to two channels.

    Two Hann-faded taps read the input half a window apart, gliding at the
    speed that gives the ratio. A plain granular shifter (PitchShifter)
    restarts a tap a fixed distance away, so its waveform meets the other
    tap's out of phase: with short windows the heard pitch drifts (an
    octave down ~100 cents sharp with a 14 ms window). Here each restart is
    aligned (as in WSOLA): the tap's new start is moved by up to the search
    span to where the input best matches what the other tap is reading
    (highest correlation, found coarse then fine, on both channels summed),
    so the two taps stay in phase. Measured within ~4 cents at +-24
    semitones, for single notes and chords.

    The search span must cover half a period of the lowest note; the window
    about one and a half periods. The delay is search + half the window.
    Allocation-free after prepare(). */
class AlignedPitchShifter
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double newSampleRate, float maxWindowMs, float maxSearchMs)
    {
        sampleRate = newSampleRate;
        const auto room = (int) std::ceil ((maxWindowMs + 3.0f * maxSearchMs) * 0.001 * sampleRate) + 32;
        for (auto& l : lines)
            l.prepare (room);
        setWindow (maxWindowMs, maxSearchMs);
        reset();
    }

    /** The window and search span (up to prepare()'s): the taps jump, so fade around a change. */
    void setWindow (float windowMs, float searchMs) noexcept
    {
        window = std::fmax (8.0f, std::round (windowMs * 0.001f * (float) sampleRate));
        search = std::fmax (1.0f, std::round (searchMs * 0.001f * (float) sampleRate));
    }

    void reset() noexcept
    {
        for (auto& l : lines)
            l.reset();
        phase = 0.0f;
        offsets = { 0.0f, 0.0f };
    }

    /** Output pitch / input pitch. */
    void setRatio (float newRatio) noexcept { ratio = newRatio; }

    /** The average delay (samples): the search span plus half the window. */
    int latencySamples() const noexcept { return (int) std::round (kMinDelay + search + 0.5f * window); }

    /** One sample frame: @p in to @p out, @p numChannels (1 or 2). */
    void process (const float* in, float* out, int numChannels) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            lines[(size_t) ch].write (in[ch]);
        channels = numChannels;

        const auto previous = phase;
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);

        // A tap that wrapped (at zero gain) starts again, aligned to the other.
        for (int k = 0; k < 2; ++k)
            if (std::abs (wrap (phase + 0.5f * (float) k) - wrap (previous + 0.5f * (float) k)) > 0.5f)
                realign (k);

        for (int ch = 0; ch < numChannels; ++ch)
            out[ch] = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            const auto p = wrap (phase + 0.5f * (float) k);
            const auto gain = 0.5f - 0.5f * std::cos (6.28318530717958647692f * p);   // the two taps sum to 1
            const auto delay = delayOf (k);
            for (int ch = 0; ch < numChannels; ++ch)
                out[ch] += gain * lines[(size_t) ch].read (delay);
        }
    }

private:
    static constexpr float kMinDelay = 2.0f;

    std::array<DelayLine, kMaxChannels> lines;
    double sampleRate = 48000.0;
    float window = 768.0f, search = 293.0f, ratio = 1.0f, phase = 0.0f;
    std::array<float, 2> offsets {};
    int channels = 1;

    static float wrap (float p) noexcept { return p - std::floor (p); }

    float nominalDelay (int k) const noexcept { return kMinDelay + search + wrap (phase + 0.5f * (float) k) * window; }
    float delayOf (int k) const noexcept { return nominalDelay (k) + offsets[(size_t) k]; }

    static constexpr int kMatchPoints = 96;     // points compared per candidate

    float sampleWhole (int delay) const noexcept
    {
        auto x = lines[0].readWhole (delay);
        if (channels > 1)
            x += lines[1].readWhole (delay);
        return x;
    }

    void realign (int k) noexcept
    {
        // What the other tap is reading (both channels summed), at whole samples.
        const auto length = (int) (0.5f * window);
        const auto pointStep = std::max (1, length / kMatchPoints);
        const auto other = (int) std::lround (delayOf (1 - k));
        std::array<float, kMatchPoints + 1> reference {};
        int points = 0;
        float xx = 0.0f;
        for (int j = 0; j < length && points <= kMatchPoints; j += pointStep)
        {
            reference[(size_t) points] = sampleWhole (other + j);
            xx += reference[(size_t) points] * reference[(size_t) points];
            ++points;
        }

        const auto nominal = (int) std::lround (nominalDelay (k));
        const auto span = (int) search;
        auto match = [&] (int offset)
        {
            float xy = 0.0f, yy = 0.0f;
            for (int p = 0; p < points; ++p)
            {
                const auto b = sampleWhole (nominal + offset + p * pointStep);
                xy += reference[(size_t) p] * b;
                yy += b * b;
            }
            return xy / std::sqrt (xx * yy + 1.0e-12f);
        };

        // Coarse (about 48 candidates across the span), then every sample around the best.
        const auto coarseStep = std::max (2, span / 24);
        auto best = 0;
        auto bestMatch = -2.0f;
        for (int offset = -span; offset <= span; offset += coarseStep)
            if (const auto m = match (offset); m > bestMatch)
            {
                bestMatch = m;
                best = offset;
            }
        const auto coarse = best;
        for (int offset = coarse - coarseStep + 1; offset < coarse + coarseStep; ++offset)
            if (std::abs (offset) <= span && offset != coarse)
                if (const auto m = match (offset); m > bestMatch)
                {
                    bestMatch = m;
                    best = offset;
                }

        // The fractional part keeps the tap on its exact (fractional) course.
        offsets[(size_t) k] = (float) best + ((float) nominal - nominalDelay (k));
    }
};

} // namespace rkfx
