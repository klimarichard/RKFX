#include <juce_audio_basics/juce_audio_basics.h>

#include "DeEssEngine.h"
#include "dsp/StateVariableFilter.h"

#include <cmath>

/** RKDeEss DSP: vowels untouched, sibilance reduced up to the range, relative
    mode independent of level, absolute threshold, split vs wide, listen. */
class DeEssTests : public juce::UnitTest
{
public:
    DeEssTests() : juce::UnitTest ("RKDeEss engine") {}

    void runTest() override
    {
        using Detection = DeEssEngine::Detection;
        using Mode = DeEssEngine::Mode;

        beginTest ("A steady vowel passes untouched (bit-exact)");
        {
            DeEssEngine engine;
            engine.setSettings ({});
            engine.prepare (48000.0);
            auto b = vowel (0.5f, 48000);
            const auto input = b;
            engine.process (b.getArrayOfWritePointers(), 1, 48000);
            // The first moments of a sound starting from silence carry a brief
            // high-frequency onset (which a de-esser may touch); from 0.5 s it's steady.
            bool identical = true;
            for (int i = 24000; i < 48000; ++i)
                identical = identical && juce::exactlyEqual (input.getSample (0, i), b.getSample (0, i));
            expect (identical);
        }

        beginTest ("Sibilance is turned down, by at most the range");
        {
            for (const float range : { 4.0f, 8.0f, 16.0f })
            {
                DeEssEngine::Settings s;
                s.rangeDb = range;
                s.relativeThresholdDb = -12.0f;   // plenty of sibilance over the threshold
                s.mode = Mode::wide;              // the whole signal gets the gain, so it can be measured exactly
                const auto reduction = sibilanceReductionDb (s, 0.5f);
                expect (reduction < -range + 1.0f && reduction > -range - 0.5f,
                        "range " + juce::String (range) + ": " + juce::String (reduction, 1) + " dB");
            }
        }

        beginTest ("Relative detection works the same at any level");
        {
            DeEssEngine::Settings s;
            s.rangeDb = 24.0f;
            s.relativeThresholdDb = -6.0f;
            const auto loud = sibilanceReductionDb (s, 0.5f), quiet = sibilanceReductionDb (s, 0.005f);
            expectWithinAbsoluteError (quiet, loud, 0.5f, "loud " + juce::String (loud, 1) + ", quiet " + juce::String (quiet, 1));
        }

        beginTest ("Absolute detection uses a fixed threshold");
        {
            DeEssEngine::Settings s;
            s.detection = Detection::absolute;
            s.thresholdDb = -24.0f;
            expect (sibilanceReductionDb (s, 0.5f) < -4.0f, "loud sibilance reduced");
            expectWithinAbsoluteError (sibilanceReductionDb (s, 0.01f), 0.0f, 0.1f, "quiet sibilance (below the threshold) untouched");
        }

        beginTest ("Split-band keeps the lows; wideband turns everything down");
        {
            for (const auto mode : { Mode::split, Mode::wide })
            {
                DeEssEngine::Settings s;
                s.mode = mode;
                s.rangeDb = 12.0f;
                DeEssEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                // A 150 Hz tone under constant loud sibilance.
                auto b = sibilance (0.5f, 48000);
                for (int i = 0; i < 48000; ++i)
                    b.setSample (0, i, b.getSample (0, i) + 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 150.0f * (float) i / 48000.0f));
                engine.process (b.getArrayOfWritePointers(), 1, 48000);

                const auto low = toneLevel (b, 150.0) / 0.3;
                if (mode == Mode::split)
                    expectWithinAbsoluteError (low, 1.0, 0.03, "split: lows kept");
                else
                    expect (low < 0.5, "wide: lows reduced too (" + juce::String (low) + ")");
            }
        }

        beginTest ("Extremes stay finite");
        {
            DeEssEngine engine;
            engine.prepare (44100.0);
            juce::Random rng (43);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;
            for (int block = 0; block < 5000; ++block)
            {
                if (block % 5 == 0)
                {
                    DeEssEngine::Settings s;
                    s.frequencyHz = rng.nextBool() ? 2000.0f : 12000.0f;
                    s.detection = rng.nextBool() ? Detection::relative : Detection::absolute;
                    s.thresholdDb = rng.nextBool() ? -60.0f : 0.0f;
                    s.relativeThresholdDb = rng.nextBool() ? -24.0f : 12.0f;
                    s.rangeDb = rng.nextBool() ? 0.0f : 24.0f;
                    s.mode = rng.nextBool() ? Mode::split : Mode::wide;
                    s.listen = rng.nextBool();
                    s.link = rng.nextBool();
                    engine.setSettings (s);
                }
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 2.0f);
                engine.process (buffer.getArrayOfWritePointers(), 2, 64);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 8.0f;
            }
            expect (ok);
        }
    }

private:
    /** Harmonics of 220 Hz rolling off: a vowel-like sound with little energy up high. */
    static juce::AudioBuffer<float> vowel (float level, int length)
    {
        juce::AudioBuffer<float> b (1, length);
        for (int i = 0; i < length; ++i)
        {
            float x = 0.0f;
            for (int h = 1; h <= 8; ++h)
                x += std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) (h * i) / 48000.0f) / (float) (h * h);
            b.setSample (0, i, level * x / 1.6f);
        }
        return b;
    }

    /** "Sss": noise high-passed at 5 kHz. */
    static juce::AudioBuffer<float> sibilance (float level, int length)
    {
        juce::Random rng (11);
        rkfx::StateVariableFilter hp;
        hp.prepare (48000.0);
        hp.setCutoff (5000.0f);
        juce::AudioBuffer<float> b (1, length);
        for (int i = 0; i < length; ++i)
            b.setSample (0, i, level * hp.process (rng.nextFloat() * 2.0f - 1.0f, rkfx::StateVariableFilter::Type::highpass));
        return b;
    }

    /** How much a steady "sss" at @p level is reduced (dB), measured after settling. */
    static float sibilanceReductionDb (const DeEssEngine::Settings& s, float level)
    {
        DeEssEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        auto b = sibilance (level, 48000);
        const auto input = b;
        engine.process (b.getArrayOfWritePointers(), 1, 48000);
        return juce::Decibels::gainToDecibels (b.getRMSLevel (0, 24000, 24000) / input.getRMSLevel (0, 24000, 24000));
    }

    static double toneLevel (const juce::AudioBuffer<float>& b, double hz)
    {
        const int start = b.getNumSamples() / 2, n = b.getNumSamples() - start;
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / 48000.0;
        const auto coeff = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
            const auto s0 = b.getSample (0, start + i) * win + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return 4.0 * std::sqrt (std::max (1.0e-30, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / n;
    }
};

static DeEssTests deEssTests;
