#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/MatchedBiquad.h"
#include "safety/Smoothing.h"

#include <array>

/** The filter building blocks RKMix's channel strips and bus chains share:
    sections switched with a crossfade, biquads run in a cascade with their
    state in registers, and the Butterworth cut filters. Allocation-free. */
namespace MixFilters
{
    constexpr double kSectionFadeSeconds = 0.02;
    constexpr int kCoefficientInterval = 32;    // filters redesign while a setting glides
    constexpr double kButterworth12 = 0.70710678;
    constexpr double kButterworth24[2] = { 0.54119610, 1.30656296 };

    /** A section switched on and off with a crossfade. */
    struct SectionFade
    {
        rkfx::Smoothed amount;
        bool isOff() const noexcept  { return ! amount.isSmoothing() && amount.getTargetValue() <= 0.0f; }
        bool isFull() const noexcept { return ! amount.isSmoothing() && amount.getTargetValue() >= 1.0f; }

        /** Returns true when the section wakes up from fully off (its state needs a reset). */
        bool set (bool on, bool snap) noexcept
        {
            const auto wakes = on && isOff();
            if (snap)
                amount.setCurrentAndTargetValue (on ? 1.0f : 0.0f);
            else
                amount.setTargetValue (on ? 1.0f : 0.0f);
            return wakes;
        }
    };

    /** Runs @p process (in place on @p io) inside a section's crossfade: skipped
        when off, plain when fully on, mixed with the dry signal while fading.
        @param scratch  2 channels of at least the block size */
    template <typename Process>
    void runSection (SectionFade& fade, float* const* io, int numSides, int numSamples,
                     juce::AudioBuffer<float>& scratch, Process&& process) noexcept
    {
        if (fade.isOff())
            return;

        if (fade.isFull())
        {
            process();
            return;
        }

        for (int side = 0; side < numSides; ++side)
            juce::FloatVectorOperations::copy (scratch.getWritePointer (side), io[side], numSamples);

        process();

        for (int i = 0; i < numSamples; ++i)
        {
            const auto amount = fade.amount.getNextValue();
            for (int side = 0; side < numSides; ++side)
            {
                const auto dry = scratch.getSample (side, i);
                io[side][i] = dry + amount * (io[side][i] - dry);
            }
        }
    }

    /** A transposed direct form II biquad for up to two channels. */
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        std::array<float, 2> z1 {}, z2 {};

        void set (const rkfx::BiquadCoefficients& c) noexcept
        {
            b0 = (float) c.b0; b1 = (float) c.b1; b2 = (float) c.b2; a1 = (float) c.a1; a2 = (float) c.a2;
        }
        void clear() noexcept { z1 = z2 = {}; }
    };

    /** @p N biquads in series on one side, coefficients and state in locals:
        the filters' recursions overlap instead of waiting on memory every sample. */
    template <int N>
    void runCascade (Biquad* const* filters, float* x, int side, int length) noexcept
    {
        float b0[N], b1[N], b2[N], a1[N], a2[N], z1[N], z2[N];
        for (int k = 0; k < N; ++k)
        {
            const auto& f = *filters[k];
            b0[k] = f.b0; b1[k] = f.b1; b2[k] = f.b2; a1[k] = f.a1; a2[k] = f.a2;
            z1[k] = f.z1[(size_t) side]; z2[k] = f.z2[(size_t) side];
        }

        for (int n = 0; n < length; ++n)
        {
            auto v = x[n];
            for (int k = 0; k < N; ++k)
            {
                const auto y = b0[k] * v + z1[k];
                z1[k] = b1[k] * v - a1[k] * y + z2[k];
                z2[k] = b2[k] * v - a2[k] * y;
                v = y;
            }
            x[n] = v;
        }

        for (int k = 0; k < N; ++k)
        {
            filters[k]->z1[(size_t) side] = z1[k];
            filters[k]->z2[(size_t) side] = z2[k];
        }
    }

    /** Up to 10 biquads in series, in place, from @p start for @p length samples. */
    inline void runFilters (Biquad* const* filters, int count, float* const* io, int numSides, int start, int length) noexcept
    {
        for (int side = 0; side < numSides; ++side)
        {
            auto* x = io[side] + start;
            // In chunks of up to 4 (the cascade's locals stay in registers).
            for (int first = 0; first < count; first += 4)
            {
                switch (juce::jmin (4, count - first))
                {
                    case 1: runCascade<1> (filters + first, x, side, length); break;
                    case 2: runCascade<2> (filters + first, x, side, length); break;
                    case 3: runCascade<3> (filters + first, x, side, length); break;
                    case 4: runCascade<4> (filters + first, x, side, length); break;
                    default: break;
                }
            }
        }
    }

    /** A bilinear (RBJ) high- or low-pass section; with the Butterworth Qs, a
        12 or 24 dB/oct Butterworth filter. */
    inline rkfx::BiquadCoefficients cutSection (bool highPass, double hz, double q, double rate)
    {
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (1.0, 0.49 * rate, hz) / rate;
        const auto cosW = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const auto a0 = 1.0 + alpha;
        rkfx::BiquadCoefficients c;
        c.b0 = (highPass ? 1.0 + cosW : 1.0 - cosW) * 0.5 / a0;
        c.b1 = (highPass ? -(1.0 + cosW) : 1.0 - cosW) / a0;
        c.b2 = c.b0;
        c.a1 = -2.0 * cosW / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

    /** A Butterworth cut filter of 1 (12 dB/oct) or 2 (24 dB/oct) sections. */
    struct CutFilter
    {
        std::array<Biquad, 2> stages;
        int numStages = 1;

        void design (bool highPass, double hz, int stagesWanted, double rate) noexcept
        {
            numStages = stagesWanted;
            if (numStages == 1)
                stages[0].set (cutSection (highPass, hz, kButterworth12, rate));
            else
                for (int s = 0; s < 2; ++s)
                    stages[(size_t) s].set (cutSection (highPass, hz, kButterworth24[s], rate));
        }

        void clear() noexcept
        {
            for (auto& s : stages)
                s.clear();
        }

        void process (float* const* io, int numSides, int start, int length) noexcept
        {
            Biquad* filters[2] = { &stages[0], &stages[1] };
            runFilters (filters, numStages, io, numSides, start, length);
        }

        /** The response (magnitude) at @p omega for a design. */
        static double magnitude (bool highPass, double hz, bool steep, double omega, double rate)
        {
            if (steep)
                return cutSection (highPass, hz, kButterworth24[0], rate).magnitude (omega)
                     * cutSection (highPass, hz, kButterworth24[1], rate).magnitude (omega);
            return cutSection (highPass, hz, kButterworth12, rate).magnitude (omega);
        }
    };
}
