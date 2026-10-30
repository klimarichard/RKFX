#pragma once

#include <juce_core/juce_core.h>

#include "dsp/Dynamics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

/** RKMix's gate and compressor: RKGate's gate and RKCompress's compressor
    (the same curves, hysteresis, hold and log-domain timing from
    core/dsp/Dynamics.h), made cheap enough for 32 channels at once. The
    detector runs every sample in the linear domain; the gain computer
    (logarithms and powers) runs every kControlInterval samples, and the
    gain is interpolated in between. A stereo channel has one detector on
    the louder side. A key (another input) can replace the detector's input.
    Allocation-free; process() works in place on one or two channels. */
namespace MixDynamics
{
    constexpr int kControlInterval = 8;

    /** One linear gain step to the next control point. */
    struct GainRamp
    {
        float current = 1.0f;

        /** Applies a ramp from current to @p target over @p length samples. */
        void apply (float* const* io, int numSides, int start, int length, float target) noexcept
        {
            if (juce::exactlyEqual (current, target))
            {
                if (! juce::exactlyEqual (target, 1.0f))
                    for (int side = 0; side < numSides; ++side)
                        for (int n = 0; n < length; ++n)
                            io[side][start + n] *= target;
            }
            else
            {
                const auto step = (target - current) / (float) length;
                for (int side = 0; side < numSides; ++side)
                {
                    auto g = current;
                    for (int n = 0; n < length; ++n)
                    {
                        g += step;
                        io[side][start + n] *= g;
                    }
                }
            }
            current = target;
        }
    };

    class Gate
    {
    public:
        static constexpr float kClosedDb = -80.0f;      // Range at or below: fully closed
        static constexpr float kHysteresisDb = 3.0f;

        struct Settings
        {
            float thresholdDb = -50.0f, rangeDb = kClosedDb;
            float attackMs = 1.0f, holdMs = 50.0f, releaseMs = 150.0f;
        };

        void prepare (double rate)
        {
            sampleRate = rate;
            detectorRelease = rkfx::Dynamics::timeCoefficient (10.0f, sampleRate);   // as RKGate
            smoother.prepare (sampleRate / kControlInterval);
            setSettings (settings);
            reset();
        }

        void reset()
        {
            envelope = 0.0f;
            smoother.reset();
            open = false;
            holdRemaining = 0;
            ramp.current = 1.0f;
            phase = 0;
            peakReduction = 0.0f;
        }

        void setSettings (const Settings& s)
        {
            settings = s;
            threshold = rkfx::Dynamics::toGain (s.thresholdDb);
            closeLevel = rkfx::Dynamics::toGain (s.thresholdDb - kHysteresisDb);
            floorDb = s.rangeDb <= kClosedDb + 0.01f ? rkfx::Dynamics::kSilenceDb : std::min (0.0f, s.rangeDb);
            holdSteps = (int) std::ceil (s.holdMs * 0.001 * sampleRate / kControlInterval);
            // The smoother's attack is for falling gain: that is the gate's release.
            smoother.setTimes (s.releaseMs, s.attackMs);
        }

        /** @param key  the detector's input (nullptr = the channel itself) */
        void process (float* const* io, int numSides, int numSamples, const float* key) noexcept
        {
            for (int start = 0; start < numSamples;)
            {
                const auto length = std::min (kControlInterval - phase, numSamples - start);
                for (int n = start; n < start + length; ++n)
                {
                    float x = 0.0f;
                    if (key != nullptr)
                        x = std::abs (key[n]);
                    else
                        for (int side = 0; side < numSides; ++side)
                            x = std::max (x, std::abs (io[side][n]));
                    envelope = x > envelope ? x : x + detectorRelease * (envelope - x);
                }
                phase += length;
                if (phase == kControlInterval)
                {
                    phase = 0;
                    target = step();
                }
                ramp.apply (io, numSides, start, length, target);
                start += length;
            }
        }

        float takeGainReductionDb() noexcept { return std::exchange (peakReduction, 0.0f); }

    private:
        Settings settings;
        double sampleRate = 48000.0;
        float detectorRelease = 0.0f, envelope = 0.0f;
        float threshold = 0.0f, closeLevel = 0.0f, floorDb = rkfx::Dynamics::kSilenceDb;
        rkfx::Dynamics::GainSmoother smoother;
        bool open = false;
        int holdSteps = 0, holdRemaining = 0, phase = 0;
        float target = 1.0f, peakReduction = 0.0f;
        GainRamp ramp;

        /** One control step: the gate's state, hold and timing -> the next gain. */
        float step() noexcept
        {
            const auto wasOpen = open;
            if (envelope >= threshold)
                open = true;
            else if (envelope < closeLevel)
                open = false;

            auto wanted = open ? 0.0f : floorDb;
            if (open)
            {
                holdRemaining = holdSteps;
            }
            else if (wasOpen && holdRemaining > 0)
            {
                --holdRemaining;
                wanted = 0.0f;
                open = true;
            }

            const auto gainDb = smoother.process (wanted);
            peakReduction = std::min (peakReduction, gainDb);
            if (gainDb >= -1.0e-4f)
                return 1.0f;
            if (gainDb <= rkfx::Dynamics::kSilenceDb + 0.5f)
                return 0.0f;
            return rkfx::Dynamics::toGain (gainDb);
        }
    };

    class Compressor
    {
    public:
        struct Settings
        {
            float thresholdDb = -20.0f, ratio = 3.0f, kneeDb = 6.0f;
            float attackMs = 10.0f, releaseMs = 150.0f, makeupDb = 0.0f;
            bool autoRelease = false;           // program dependent, as RKCompress's Auto
        };

        /** The static curve: output level for an input level, with makeup (for the display). */
        static float transferDb (const Settings& s, float inputDb) noexcept
        {
            return rkfx::Dynamics::compressorCurve (inputDb, s.thresholdDb, std::max (1.0f, s.ratio), s.kneeDb) + s.makeupDb;
        }

        void prepare (double rate)
        {
            sampleRate = rate;
            rmsCoef = rkfx::Dynamics::timeCoefficient (10.0f, sampleRate);   // RMS over about 10 ms, as RKCompress
            smoother.prepare (sampleRate / kControlInterval);
            setSettings (settings);
            reset();
        }

        void reset()
        {
            meanSquare = 0.0f;
            smoother.reset();
            ramp.current = makeup;
            target = makeup;
            phase = 0;
            peakReduction = 0.0f;
        }

        void setSettings (const Settings& s)
        {
            settings = s;
            makeup = rkfx::Dynamics::toGain (s.makeupDb);
            smoother.setTimes (s.attackMs, s.releaseMs);
            smoother.setAutoRelease (s.autoRelease);
        }

        void process (float* const* io, int numSides, int numSamples, const float* key) noexcept
        {
            for (int start = 0; start < numSamples;)
            {
                const auto length = std::min (kControlInterval - phase, numSamples - start);
                for (int n = start; n < start + length; ++n)
                {
                    float x2 = 0.0f;
                    if (key != nullptr)
                        x2 = key[n] * key[n];
                    else
                        for (int side = 0; side < numSides; ++side)
                            x2 = std::max (x2, io[side][n] * io[side][n]);
                    meanSquare = x2 + rmsCoef * (meanSquare - x2);
                }
                phase += length;
                if (phase == kControlInterval)
                {
                    phase = 0;
                    target = step();
                }
                ramp.apply (io, numSides, start, length, target);
                start += length;
            }
        }

        float takeGainReductionDb() noexcept { return std::exchange (peakReduction, 0.0f); }

    private:
        Settings settings;
        double sampleRate = 48000.0;
        float rmsCoef = 0.0f, meanSquare = 0.0f, makeup = 1.0f;
        rkfx::Dynamics::GainSmoother smoother;
        int phase = 0;
        float target = 1.0f, peakReduction = 0.0f;
        GainRamp ramp;

        float step() noexcept
        {
            const auto levelDb = meanSquare > 1.0e-12f ? 10.0f * std::log10 (meanSquare) : rkfx::Dynamics::kSilenceDb;
            const auto wanted = std::min (0.0f, rkfx::Dynamics::compressorCurve (levelDb, settings.thresholdDb,
                                                                                 std::max (1.0f, settings.ratio), settings.kneeDb) - levelDb);
            const auto gainDb = smoother.process (wanted);
            peakReduction = std::min (peakReduction, gainDb);
            return gainDb >= -1.0e-4f ? makeup : makeup * rkfx::Dynamics::toGain (gainDb);
        }
    };

    /** A zero-latency peak limiter: the gain falls at once to hold a peak at
        the ceiling (no lookahead) and recovers over the release; a clip at the
        ceiling is the last guard, so the output never exceeds it (in-ears). A
        stereo signal is limited by its louder side. */
    class Limiter
    {
    public:
        struct Settings
        {
            float ceilingDb = -1.0f, releaseMs = 100.0f;
        };

        void prepare (double rate)
        {
            sampleRate = rate;
            setSettings (settings);
            reset();
        }

        void reset() { gain = 1.0f; lowestGain = 1.0f; }

        void setSettings (const Settings& s)
        {
            settings = s;
            ceiling = rkfx::Dynamics::toGain (std::min (0.0f, s.ceilingDb));
            releaseCoef = rkfx::Dynamics::timeCoefficient (std::max (1.0f, s.releaseMs), sampleRate);
        }

        void process (float* const* io, int numSides, int numSamples) noexcept
        {
            for (int n = 0; n < numSamples; ++n)
            {
                float peak = 0.0f;
                for (int side = 0; side < numSides; ++side)
                    peak = std::max (peak, std::abs (io[side][n]));

                const auto target = peak > ceiling ? ceiling / peak : 1.0f;
                gain = target < gain ? target : target + releaseCoef * (gain - target);
                lowestGain = std::min (lowestGain, gain);

                for (int side = 0; side < numSides; ++side)
                    io[side][n] = std::clamp (io[side][n] * gain, -ceiling, ceiling);
            }
        }

        /** Most gain reduction since the last call (dB, <= 0); resets. */
        float takeGainReductionDb() noexcept
        {
            const auto db = rkfx::Dynamics::toDb (std::exchange (lowestGain, 1.0f));
            return std::min (0.0f, db);
        }

    private:
        Settings settings;
        double sampleRate = 48000.0;
        float ceiling = 1.0f, releaseCoef = 0.0f, gain = 1.0f, lowestGain = 1.0f;
    };
}
