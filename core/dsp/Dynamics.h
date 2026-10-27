#pragma once

#include <algorithm>
#include <cmath>

namespace rkfx
{

/** Shared building blocks for the dynamics plugins (compressor, gate,
    limiter, de-esser): level detection, gain computers and log-domain gain
    smoothing. All work per sample, with no lookahead. */
namespace Dynamics
{
    inline constexpr float kSilenceDb = -120.0f;

    inline float toDb (float linear) noexcept
    {
        return linear > 1.0e-6f ? 20.0f * std::log10 (linear) : kSilenceDb;
    }

    inline float toGain (float db) noexcept
    {
        return std::pow (10.0f, db * 0.05f);
    }

    inline float timeCoefficient (float ms, double sampleRate) noexcept
    {
        return ms <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (0.001 * (double) ms * sampleRate));
    }

    /** Peak or RMS level of one channel (linear). RMS averages over about
        10 ms, which follows syllables and notes without riding the waveform. */
    class LevelDetector
    {
    public:
        enum class Mode { peak, rms };

        void prepare (double sampleRate) noexcept
        {
            rmsCoef = timeCoefficient (10.0f, sampleRate);
            reset();
        }

        void reset() noexcept { meanSquare = 0.0f; }
        void setMode (Mode m) noexcept { mode = m; }

        float process (float x) noexcept
        {
            if (mode == Mode::peak)
                return std::abs (x);

            meanSquare = x * x + rmsCoef * (meanSquare - x * x);
            return std::sqrt (meanSquare);
        }

    private:
        Mode mode = Mode::rms;
        float rmsCoef = 0.0f, meanSquare = 0.0f;
    };

    /** Static compressor curve with a soft knee (Giannoulis, Massberg, Reiss):
        output level in dB for an input level in dB. */
    inline float compressorCurve (float inDb, float thresholdDb, float ratio, float kneeDb) noexcept
    {
        const auto over = inDb - thresholdDb;

        if (kneeDb > 0.0f && std::abs (over) <= kneeDb * 0.5f)
        {
            const auto t = over + kneeDb * 0.5f;
            return inDb + (1.0f / ratio - 1.0f) * t * t / (2.0f * kneeDb);
        }

        return over > 0.0f ? thresholdDb + over / ratio : inDb;
    }

    /** Smooths a gain change in dB (0 or negative) with attack and release,
        in the log domain, so the timing sounds the same at every depth.

        Auto release is program dependent: after a short peak the gain
        recovers quickly (about 50 ms); during sustained gain reduction the
        recovery slows (up to about 1.5 s), avoiding pumping. */
    class GainSmoother
    {
    public:
        void prepare (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate;
            averageCoef = timeCoefficient (800.0f, sampleRate);
            setTimes (attackMs, releaseMs);
            reset();
        }

        void reset() noexcept { state = 0.0f; average = 0.0f; }

        void setTimes (float newAttackMs, float newReleaseMs) noexcept
        {
            attackMs = newAttackMs;
            releaseMs = newReleaseMs;
            attackCoef = timeCoefficient (attackMs, sampleRate);
            releaseCoef = timeCoefficient (releaseMs, sampleRate);
        }

        void setAutoRelease (bool shouldBeAuto) noexcept { autoRelease = shouldBeAuto; }

        /** @param targetDb  the gain the gain computer asks for (<= 0)
            @returns         the smoothed gain in dB */
        float process (float targetDb) noexcept
        {
            float coef;
            if (targetDb < state)
            {
                coef = attackCoef;
            }
            else if (autoRelease)
            {
                // How much reduction has been going on lately: 0 = none, 1 = 6 dB or more.
                const auto sustained = std::clamp (-average / 6.0f, 0.0f, 1.0f);
                coef = std::exp (-1.0f / (0.001f * (50.0f + 1450.0f * sustained) * (float) sampleRate));
            }
            else
            {
                coef = releaseCoef;
            }

            state = targetDb + coef * (state - targetDb);

            // Tracks how much reduction is being asked for (not how much is
            // still applied), so the release speeds up as soon as a peak ends.
            average = targetDb + averageCoef * (average - targetDb);
            return state;
        }

        float getGainDb() const noexcept { return state; }

    private:
        double sampleRate = 48000.0;
        float attackMs = 10.0f, releaseMs = 150.0f;
        float attackCoef = 0.0f, releaseCoef = 0.0f, averageCoef = 0.0f;
        float state = 0.0f, average = 0.0f;
        bool autoRelease = false;
    };
}

} // namespace rkfx
