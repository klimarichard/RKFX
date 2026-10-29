#pragma once

#include "DelayLine.h"

#include <cmath>

namespace rkfx
{

/** A granular pitch shifter for any material (chords, reverb tails): no
    pitch tracking, so it never fails, at the cost of about 20 ms of delay.
    For feedback paths (shimmer), where that delay is absorbed.

    Two pairs of taps read one short delay line. Each tap glides through its
    pair's window at the speed that gives the ratio (towards the present to
    shift up, away from it to shift down) and jumps back at the end; the two
    taps of a pair are half a window apart and Hann-faded, so a jump is never
    heard and each pair keeps a steady level.

    Taps a fixed distance apart cancel some frequencies (a comb). The pairs'
    windows have different lengths (50 and 35 ms), so their cancellations
    fall at different frequencies and move over time: averaged, no frequency
    is ever lost. (Four taps evenly spaced in one window would be worse:
    they cancel every odd multiple of 40 Hz completely.)

    One channel; allocation-free after prepare(). */
class PitchShifter
{
public:
    /** @param windowMs, secondWindowMs  the two pairs' windows (shimmer: 50 and 35 ms)
        @param maxWindowMs               room for setWindows() later (at least windowMs) */
    void prepare (double newSampleRate, float windowMs = kWindowMs[0], float secondWindowMs = kWindowMs[1],
                  float maxWindowMs = 0.0f)
    {
        sampleRate = newSampleRate;
        windows[0] = (float) std::round (windowMs * 0.001 * sampleRate);
        windows[1] = (float) std::round (secondWindowMs * 0.001 * sampleRate);
        const auto room = (float) std::round (larger (maxWindowMs, larger (windowMs, secondWindowMs)) * 0.001 * sampleRate);
        line.prepare ((int) room + 8);
        reset();
    }

    /** Changes the windows (no larger than prepare()'s maxWindowMs). The taps
        jump: fade the output around a change. */
    void setWindows (float windowMs, float secondWindowMs) noexcept
    {
        const auto limit = (float) (line.getMaxDelay() - 4);
        windows[0] = std::fmin ((float) std::round (windowMs * 0.001 * sampleRate), limit);
        windows[1] = std::fmin ((float) std::round (secondWindowMs * 0.001 * sampleRate), limit);
    }

    void reset() noexcept
    {
        line.reset();
        phases[0] = 0.0f;
        phases[1] = 0.37f;   // the pairs never jump together
    }

    /** Output pitch / input pitch (2 = an octave up, 0.5 = an octave down). */
    void setRatio (float newRatio) noexcept { ratio = newRatio; }

    /** The average delay through the shifter (half of each pair's window). */
    int latencySamples() const noexcept
    {
        return (int) std::round (kMinDelay + 0.25f * (windows[0] + windows[1]));
    }

    float process (float x) noexcept
    {
        line.write (x);

        constexpr float twoPi = 6.28318530717958647692f;
        auto y = 0.0f;

        for (int pair = 0; pair < 2; ++pair)
        {
            // The taps' delay changes by (1 - ratio) samples per sample.
            auto& phase = phases[pair];
            phase += (1.0f - ratio) / windows[pair];
            phase -= std::floor (phase);

            const auto other = phase < 0.5f ? phase + 0.5f : phase - 0.5f;
            const auto gain = 0.5f - 0.5f * std::cos (twoPi * phase);   // the other tap gets 1 - gain

            y += gain * line.read (kMinDelay + phase * windows[pair])
               + (1.0f - gain) * line.read (kMinDelay + other * windows[pair]);
        }

        // The taps only partly add up (about -4 dB on any material, tones or
        // noise): the average of the pairs, made up to unity.
        return kMakeup * 0.5f * y;
    }

private:
    static constexpr float kMinDelay = 2.0f;
    static constexpr float kMakeup = 1.6f;
    static constexpr float kWindowMs[2] = { 50.0f, 35.0f };

    static float larger (float a, float b) noexcept { return a > b ? a : b; }

    DelayLine line;
    double sampleRate = 48000.0;
    float windows[2] { 2400.0f, 1680.0f };
    float phases[2] { 0.0f, 0.37f };
    float ratio = 1.0f;
};

/** The shimmer intervals RKVerb and RKDelay offer (parameter order). */
namespace ShimmerInterval
{
    inline constexpr int kCount = 5;
    inline const char* const names[kCount] = { "Octave up", "Fifth up", "Octave + fifth up", "Two octaves up", "Octave down" };
    inline const char* const keys[kCount]  = { "octave_up", "fifth_up", "octave_fifth_up", "two_octaves_up", "octave_down" };

    /** Pitch ratio of interval @p index. */
    inline float ratio (int index) noexcept
    {
        constexpr float ratios[kCount] = { 2.0f, 1.49830708f, 2.99661415f, 4.0f, 0.5f };   // fifths: 2^(7/12), 2^(19/12)
        return ratios[index < 0 ? 0 : (index >= kCount ? kCount - 1 : index)];
    }
}

} // namespace rkfx
