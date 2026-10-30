#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Loudness.h"

#include <cmath>
#include <vector>

/** The limiter that lifts a quiet song past its peak ceiling (see "Lift quiet
    songs" in docs/design/RKPlay.md). It runs on the disk thread, seconds
    ahead of playback, so it can look ahead without adding latency where it
    matters: its delay is dropped at the start of the song.

    Mastering style: the detector is the true peak (4x oversampled); the gain
    needed in the next 5 ms is held (a sliding minimum) and smoothed by a
    moving average as long as the lookahead, so the gain is already down when
    a peak arrives, with no sudden steps; it recovers over the release. A
    clip at the ceiling is the last guard. Allocates only in prepare(). */
class PlayLimiter
{
public:
    void prepare (double sampleRate, double ceilingDb, double lookaheadMs = 5.0, double releaseMs = 150.0)
    {
        length = juce::jmax (1, (int) std::lround (sampleRate * lookaheadMs * 0.001));
        ceiling = (float) juce::Decibels::decibelsToGain (ceilingDb);
        releaseCoef = std::exp (-1.0 / (sampleRate * releaseMs * 0.001));
        for (auto& d : delay)
            d.assign ((size_t) length, 0.0f);
        box.assign ((size_t) length, 1.0f);
        window.assign ((size_t) length + 16, 1.0f);   // + the true-peak filter's own delay
        reset();
    }

    void reset()
    {
        for (auto& d : delay)
            std::fill (d.begin(), d.end(), 0.0f);
        std::fill (box.begin(), box.end(), 1.0f);
        std::fill (window.begin(), window.end(), 1.0f);
        for (auto& t : truePeak)
            t.reset();
        boxSum = (double) length;
        position = windowPos = 0;
        state = 1.0;
        lowest = 1.0f;
    }

    /** The output's delay (samples): drop this many at the start, flush as many zeros at the end. */
    int getLatency() const noexcept { return length; }

    /** Most gain reduction so far (dB, <= 0). */
    float getMostReductionDb() const noexcept { return juce::Decibels::gainToDecibels (lowest); }

    void process (float* l, float* r, int numSamples) noexcept
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const auto peak = juce::jmax (truePeak[0].process (l[n]), truePeak[1].process (r[n]));
            const auto needed = peak > ceiling ? ceiling / peak : 1.0f;

            // The lowest gain needed over the lookahead and this sample.
            window[(size_t) windowPos] = needed;
            windowPos = (windowPos + 1) % (int) window.size();
            float held = 1.0f;
            for (const auto w : window)
                held = juce::jmin (held, w);

            state = held < state ? (double) held : held + releaseCoef * (state - held);
            boxSum += state - box[(size_t) position];
            box[(size_t) position] = (float) state;
            const auto gain = (float) (boxSum / length);
            lowest = juce::jmin (lowest, gain);

            // Out: the sample from a lookahead ago, at the gain that was ready for it.
            const auto dl = delay[0][(size_t) position], dr = delay[1][(size_t) position];
            delay[0][(size_t) position] = l[n];
            delay[1][(size_t) position] = r[n];
            position = (position + 1) % length;
            l[n] = juce::jlimit (-ceiling, ceiling, dl * gain);
            r[n] = juce::jlimit (-ceiling, ceiling, dr * gain);
        }
    }

private:
    int length = 1, position = 0, windowPos = 0;
    float ceiling = 1.0f, lowest = 1.0f;
    double releaseCoef = 0.0, state = 1.0, boxSum = 1.0;
    std::vector<float> delay[2], box, window;
    rkfx::TruePeakDetector truePeak[2];
};
