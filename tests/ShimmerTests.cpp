#include <juce_audio_basics/juce_audio_basics.h>

#include "DelayEngine.h"
#include "ReverbEngine.h"
#include "dsp/PitchShifter.h"

#include <cmath>

namespace
{
    constexpr double kRate = 48000.0;
    constexpr double kTwoPi = 6.28318530717958647692;

    /** Energy of @p x[start, start + length) at @p hz (Goertzel), normalised by length. */
    double energyAt (const float* x, int start, int length, double hz)
    {
        const auto coef = 2.0 * std::cos (kTwoPi * hz / kRate);
        double s1 = 0.0, s2 = 0.0;
        for (int i = start; i < start + length; ++i)
        {
            const auto s0 = x[i] + coef * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return (s1 * s1 + s2 * s2 - coef * s1 * s2) / ((double) length * length);
    }

    double rms (const float* x, int start, int length)
    {
        double sum = 0.0;
        for (int i = start; i < start + length; ++i)
            sum += (double) x[i] * x[i];
        return std::sqrt (sum / length);
    }

    /** Frequency from upward zero crossings. */
    double frequencyOf (const float* x, int start, int length)
    {
        int crossings = 0;
        for (int i = start + 1; i < start + length; ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
                ++crossings;
        return crossings * kRate / length;
    }

    /** A Hann-windowed 440 Hz burst at the start of a mono buffer. */
    juce::AudioBuffer<float> burst (int totalSamples, double burstSeconds = 0.06)
    {
        juce::AudioBuffer<float> b (1, totalSamples);
        b.clear();
        const auto n = (int) (burstSeconds * kRate);
        for (int i = 0; i < n; ++i)
            b.setSample (0, i, 0.5f * (float) (std::sin (kTwoPi * 440.0 * i / kRate) * (0.5 - 0.5 * std::cos (kTwoPi * i / n))));
        return b;
    }

    void runDelay (DelayEngine& e, juce::AudioBuffer<float>& b)
    {
        for (int start = 0; start < b.getNumSamples(); start += 256)
        {
            const auto n = juce::jmin (256, b.getNumSamples() - start);
            float* ch[] = { b.getWritePointer (0, start) };
            e.process (ch, 1, n, rkfx::Transport {});
        }
    }

    void runVerb (ReverbEngine& e, juce::AudioBuffer<float>& b)
    {
        for (int start = 0; start < b.getNumSamples(); start += 256)
        {
            const auto n = juce::jmin (256, b.getNumSamples() - start);
            float* ch[] = { b.getWritePointer (0, start), b.getWritePointer (1, start) };
            e.process (ch, 2, n);
        }
    }

    DelayEngine::Settings cleanDelay (float shimmer, float ratio, float feedback)
    {
        DelayEngine::Settings s;
        s.mode = DelayEngine::Mode::digital;
        s.timeMs = 300.0f;
        s.feedback = feedback;
        s.stereo = DelayEngine::Stereo::mono;
        s.lowCutHz = 20.0f;
        s.highCutHz = 20000.0f;
        s.shimmer = shimmer;
        s.shimmerRatio = ratio;
        return s;
    }

    /** Energy centroid (in samples) of x around @p centre, +-@p halfWidth. */
    double centroid (const float* x, int centre, int halfWidth)
    {
        double sum = 0.0, weighted = 0.0;
        for (int i = centre - halfWidth; i < centre + halfWidth; ++i)
        {
            const auto e = (double) x[i] * x[i];
            sum += e;
            weighted += e * i;
        }
        return sum > 0.0 ? weighted / sum : 0.0;
    }
}

/** The shimmer pitch shifter, and shimmer in RKVerb and RKDelay: pitch,
    level, unchanged when off, repeats in time, bounded at the extremes,
    no clicks. */
class ShimmerTests : public juce::UnitTest
{
public:
    ShimmerTests() : juce::UnitTest ("Shimmer (pitch shifter, RKVerb, RKDelay)") {}

    void runTest() override
    {
        beginTest ("The shifter moves a tone by the interval and keeps its level");
        for (int interval = 0; interval < rkfx::ShimmerInterval::kCount; ++interval)
        {
            const auto ratio = rkfx::ShimmerInterval::ratio (interval);
            rkfx::PitchShifter shifter;
            shifter.prepare (kRate);
            shifter.setRatio (ratio);

            juce::AudioBuffer<float> b (1, (int) kRate * 2);
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (0, i, 0.5f * (float) std::sin (kTwoPi * 440.0 * i / kRate));
            const auto inputRms = rms (b.getReadPointer (0), 0, b.getNumSamples());
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (0, i, shifter.process (b.getSample (0, i)));

            const auto* y = b.getReadPointer (0);
            const auto measured = frequencyOf (y, (int) kRate, (int) kRate);
            expectWithinAbsoluteError (measured, 440.0 * ratio, 440.0 * ratio * 0.02, rkfx::ShimmerInterval::names[interval]);

            const auto levelDb = 20.0 * std::log10 (rms (y, (int) kRate, (int) kRate) / inputRms);
            expect (std::abs (levelDb) < 3.0, juce::String (rkfx::ShimmerInterval::names[interval]) + " level " + juce::String (levelDb, 2) + " dB");
        }

        beginTest ("RKDelay: off is unchanged, whatever the interval");
        {
            DelayEngine a, b;
            a.setSettings (cleanDelay (0.0f, 2.0f, 0.6f));
            b.setSettings (cleanDelay (0.0f, 0.5f, 0.6f));
            a.prepare (kRate);
            b.prepare (kRate);
            auto x = burst ((int) kRate * 2), y = x;
            runDelay (a, x);
            runDelay (b, y);
            bool identical = true;
            for (int i = 0; i < x.getNumSamples(); ++i)
                identical = identical && juce::exactlyEqual (x.getSample (0, i), y.getSample (0, i));
            expect (identical);
        }

        beginTest ("RKDelay: each repeat climbs an octave and stays on the beat");
        {
            DelayEngine e;
            e.setSettings (cleanDelay (1.0f, 2.0f, 0.9f));
            e.prepare (kRate);
            auto b = burst ((int) (kRate * 1.2));
            runDelay (e, b);
            const auto* y = b.getReadPointer (0);

            const auto period = (int) (0.3 * kRate), burstCentre = (int) (0.03 * kRate), half = (int) (0.1 * kRate);
            for (int repeat = 1; repeat <= 3; ++repeat)
            {
                const auto expected = repeat * period + burstCentre;
                const auto at = centroid (y, expected, half);
                // Within a few ms: an upward shift squeezes a note slightly, which pulls its weight a little earlier.
                expectWithinAbsoluteError (at / kRate * 1000.0, expected / kRate * 1000.0, 5.0,
                                           "repeat " + juce::String (repeat) + " (ms)");
                logMessage ("repeat " + juce::String (repeat) + ": " + juce::String (at / kRate * 1000.0, 2) + " ms, expected "
                            + juce::String (expected / kRate * 1000.0, 2));

                const auto hz = 440.0 * std::pow (2.0, repeat - 1);
                const auto own = energyAt (y, expected - half, 2 * half, hz);
                const auto below = energyAt (y, expected - half, 2 * half, hz / 2.0);
                if (repeat > 1)
                    expect (own > 10.0 * below, "repeat " + juce::String (repeat) + " is at " + juce::String (hz) + " Hz");
            }
        }

        beginTest ("RKDelay: 100% feedback with shimmer stays bounded");
        for (const auto ratio : { 2.0f, 0.5f })
        {
            DelayEngine e;
            auto s = cleanDelay (1.0f, ratio, 1.0f);
            s.timeMs = 120.0f;
            e.setSettings (s);
            e.prepare (kRate);
            juce::AudioBuffer<float> b (1, (int) kRate * 30);
            juce::Random rng (2);
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (0, i, i < (int) kRate ? rng.nextFloat() * 2.0f - 1.0f : 0.0f);
            runDelay (e, b);
            bool finite = true;
            for (int i = 0; i < b.getNumSamples(); ++i)
                finite = finite && std::isfinite (b.getSample (0, i)) && std::abs (b.getSample (0, i)) < 4.0f;
            expect (finite, "ratio " + juce::String (ratio));
        }

        beginTest ("RKVerb: off is unchanged, whatever the interval");
        {
            ReverbEngine a, b;
            ReverbEngine::Settings s;
            s.shimmerRatio = 2.0f;
            a.setSettings (s);
            s.shimmerRatio = 0.5f;
            b.setSettings (s);
            a.prepare (kRate);
            b.prepare (kRate);
            auto mono = burst ((int) kRate * 2);
            juce::AudioBuffer<float> x (2, mono.getNumSamples());
            x.copyFrom (0, 0, mono, 0, 0, mono.getNumSamples());
            x.copyFrom (1, 0, mono, 0, 0, mono.getNumSamples());
            auto y = x;
            runVerb (a, x);
            runVerb (b, y);
            bool identical = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < x.getNumSamples(); ++i)
                    identical = identical && juce::exactlyEqual (x.getSample (ch, i), y.getSample (ch, i));
            expect (identical);
        }

        beginTest ("RKVerb: the tail gains an octave above");
        {
            auto tailRatio = [] (float shimmer)
            {
                ReverbEngine e;
                ReverbEngine::Settings s;
                s.decaySeconds = 6.0f;
                s.damping = 0.3f;
                s.shimmer = shimmer;
                e.setSettings (s);
                e.prepare (kRate);
                auto mono = burst ((int) kRate * 3, 0.3);
                juce::AudioBuffer<float> b (2, mono.getNumSamples());
                b.copyFrom (0, 0, mono, 0, 0, mono.getNumSamples());
                b.copyFrom (1, 0, mono, 0, 0, mono.getNumSamples());
                runVerb (e, b);
                const auto* y = b.getReadPointer (0);
                const auto start = (int) (1.5 * kRate), length = (int) kRate;
                return energyAt (y, start, length, 880.0) / energyAt (y, start, length, 440.0);
            };
            const auto without = tailRatio (0.0f), with = tailRatio (0.6f);
            expect (with > 10.0 * without, "880/440 energy: " + juce::String (without) + " -> " + juce::String (with));
        }

        beginTest ("RKVerb: the longest decay with full shimmer, and freeze, stay bounded");
        for (const auto ratio : { 2.0f, 0.5f })
        {
            ReverbEngine e;
            ReverbEngine::Settings s;
            s.decaySeconds = 20.0f;
            s.damping = 0.0f;
            s.highCutHz = 20000.0f;
            s.shimmer = 1.0f;
            s.shimmerRatio = ratio;
            e.setSettings (s);
            e.prepare (kRate);

            juce::AudioBuffer<float> b (2, (int) kRate * 40);
            juce::Random rng (5);
            for (int i = 0; i < (int) kRate; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    b.setSample (ch, i, rng.nextFloat() - 0.5f);

            // 20 s free, then 20 s frozen.
            juce::AudioBuffer<float> first (2, (int) kRate * 20), second (2, (int) kRate * 20);
            first.copyFrom (0, 0, b, 0, 0, first.getNumSamples());
            first.copyFrom (1, 0, b, 1, 0, first.getNumSamples());
            runVerb (e, first);
            s.freeze = true;
            e.setSettings (s);
            second.clear();
            runVerb (e, second);

            const auto early = rms (first.getReadPointer (0), (int) kRate * 2, (int) kRate * 3);
            const auto late = rms (first.getReadPointer (0), (int) kRate * 15, (int) kRate * 5);
            expect (late < 1.5 * early, "ratio " + juce::String (ratio) + ": no build-up while free ("
                                         + juce::String (early, 3) + " -> " + juce::String (late, 3) + ")");

            const auto frozenEarly = rms (second.getReadPointer (0), (int) kRate * 2, (int) kRate * 3);
            const auto frozenLate = rms (second.getReadPointer (0), (int) kRate * 15, (int) kRate * 5);
            expect (frozenLate < 1.2 * frozenEarly + 1.0e-6, "ratio " + juce::String (ratio) + ": no growth while frozen ("
                                                            + juce::String (frozenEarly, 3) + " -> " + juce::String (frozenLate, 3) + ")");
        }

        beginTest ("Sweeping Shimmer and switching Interval never click");
        {
            DelayEngine e;
            auto s = cleanDelay (0.0f, 2.0f, 0.5f);
            e.setSettings (s);
            e.prepare (kRate);

            juce::AudioBuffer<float> b (1, 256);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;
            for (int block = 0; block < 1500; ++block)
            {
                if (block >= 300 && block < 700) { s.shimmer = (float) (block - 300) / 400.0f; e.setSettings (s); }
                if (block == 800)  { s.shimmerRatio = rkfx::ShimmerInterval::ratio (1); e.setSettings (s); }
                if (block == 1000) { s.shimmerRatio = rkfx::ShimmerInterval::ratio (4); e.setSettings (s); }
                if (block == 1200) { s.shimmer = 0.0f; e.setSettings (s); }

                for (int i = 0; i < 256; ++i, ++n)
                    b.setSample (0, i, 0.25f * (float) std::sin (kTwoPi * 220.0 * n / kRate));
                float* ch[] = { b.getWritePointer (0) };
                e.process (ch, 1, 256, rkfx::Transport {});

                for (int i = 0; i < 256; ++i)
                {
                    if (block > 10)
                        maxStep = std::max (maxStep, std::abs (b.getSample (0, i) - previous));
                    previous = b.getSample (0, i);
                }
            }

            // Repeats of the tone plus shifted copies up to about 1.3 kHz at that level move up to ~0.08 per sample.
            expect (maxStep < 0.12f, "max step " + juce::String (maxStep));
        }
    }
};

static ShimmerTests shimmerTests;
