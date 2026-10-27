#include <juce_audio_basics/juce_audio_basics.h>

#include "SaturateEngine.h"
#include "dsp/Oversampler.h"

#include <cmath>

/** RKSaturate DSP and the shared oversampler: latency, alias rejection,
    curve shapes, transformer low-end drive, auto gain, click-free character
    changes, stability. */
class SaturateTests : public juce::UnitTest
{
public:
    SaturateTests() : juce::UnitTest ("RKSaturate engine") {}

    void runTest() override
    {
        using Character = SaturateEngine::Character;

        beginTest ("Oversampler: unity gain, delay matches the reported latency");
        {
            for (const int factor : { 2, 4 })
            {
                rkfx::Oversampler os;
                os.reset();
                const double hz = 500.0, rate = 48000.0;
                juce::AudioBuffer<float> b (1, 48000);
                for (int i = 0; i < 48000; ++i)
                    b.setSample (0, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / rate));
                float* p = b.getWritePointer (0);
                os.process (&p, 1, 48000, factor, [] (int, int, float x) { return x; });

                // Fit the delay of the output sine (least squares on sin/cos components).
                double s = 0.0, c = 0.0;
                for (int i = 24000; i < 48000; ++i)
                {
                    const auto w = 2.0 * juce::MathConstants<double>::pi * hz * i / rate;
                    s += b.getSample (0, i) * std::sin (w);
                    c += b.getSample (0, i) * std::cos (w);
                }
                const auto amplitude = 2.0 * std::sqrt (s * s + c * c) / 24000.0;
                const auto delay = -std::atan2 (c, s) / (2.0 * juce::MathConstants<double>::pi * hz) * rate;

                expectWithinAbsoluteError (amplitude, 1.0, 0.002, juce::String (factor) + "x gain");
                expectWithinAbsoluteError (delay, (double) rkfx::Oversampler::latencySamples (factor), 0.5,
                                           juce::String (factor) + "x delay " + juce::String (delay, 2));
            }
        }

        beginTest ("Oversampling removes aliasing from hard saturation");
        {
            // 5 kHz driven at 48 kHz: its 9th harmonic (45 kHz) folds back to 3 kHz without
            // oversampling; at 2x and 4x it stays below the oversampled Nyquist and is filtered.
            // (A sine clipped into a near-square has harmonics no practical oversampling removes.)
            const auto aliasDb = [] (int factor)
            {
                SaturateEngine::Settings s;
                s.driveDb = 18.0f;
                s.autoGain = false;
                s.oversampling = factor;
                SaturateEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                juce::AudioBuffer<float> b (1, 48000);
                for (int i = 0; i < 48000; ++i)
                    b.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 5000.0f * (float) i / 48000.0f));
                float* p = b.getWritePointer (0);
                engine.process (&p, 1, 48000);
                return juce::Decibels::gainToDecibels (tone (b, 3000.0) / tone (b, 5000.0));
            };

            const auto off = aliasDb (1), x2 = aliasDb (2), x4 = aliasDb (4);
            logMessage ("alias at 3 kHz: off " + juce::String (off, 1) + " dB, 2x " + juce::String (x2, 1) + " dB, 4x " + juce::String (x4, 1) + " dB");
            expect (off > -40.0f, "off: alias at " + juce::String (off, 1) + " dB (audible)");
            expect (x2 < -70.0f, "2x: alias at " + juce::String (x2, 1) + " dB");
            expect (x4 < -70.0f, "4x: alias at " + juce::String (x4, 1) + " dB");
        }

        beginTest ("Curves: unity at low level, bounded, tube adds even harmonics");
        {
            for (const auto c : { Character::tape, Character::tube, Character::transformer })
            {
                expectWithinAbsoluteError (SaturateEngine::shape (c, 0.001f) / 0.001f, 1.0f, 0.01f);
                expect (std::abs (SaturateEngine::shape (c, 100.0f)) < 2.0f);
                expect (std::abs (SaturateEngine::shape (c, -100.0f)) < 2.0f);
            }

            const auto secondHarmonicDb = [] (Character c)
            {
                SaturateEngine::Settings s;
                s.character = c;
                s.driveDb = 12.0f;
                s.oversampling = 4;
                SaturateEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);
                juce::AudioBuffer<float> b (1, 48000);
                for (int i = 0; i < 48000; ++i)
                    b.setSample (0, i, 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 500.0f * (float) i / 48000.0f));
                float* p = b.getWritePointer (0);
                engine.process (&p, 1, 48000);
                return juce::Decibels::gainToDecibels (tone (b, 1000.0) / tone (b, 500.0));
            };

            expect (secondHarmonicDb (Character::tube) > -40.0f, "tube: 2nd harmonic");
            expect (secondHarmonicDb (Character::tape) < -80.0f, "tape: symmetric, no 2nd harmonic");
        }

        beginTest ("Transformer saturates the lows more than the highs");
        {
            const auto thirdHarmonicDb = [] (Character c, float hz)
            {
                SaturateEngine::Settings s;
                s.character = c;
                s.driveDb = 6.0f;
                s.oversampling = 4;
                SaturateEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);
                juce::AudioBuffer<float> b (1, 48000);
                for (int i = 0; i < 48000; ++i)
                    b.setSample (0, i, 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f));
                float* p = b.getWritePointer (0);
                engine.process (&p, 1, 48000);
                return juce::Decibels::gainToDecibels (tone (b, 3.0 * hz) / tone (b, hz));
            };

            expect (thirdHarmonicDb (Character::transformer, 60.0f) > thirdHarmonicDb (Character::transformer, 2000.0f) + 6.0f);
        }

        beginTest ("Auto gain keeps the level roughly steady across the drive range");
        {
            float levels[3] {};
            const float drives[3] = { 0.0f, 12.0f, 24.0f };
            for (int d = 0; d < 3; ++d)
            {
                SaturateEngine::Settings s;
                s.driveDb = drives[d];
                SaturateEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);
                juce::AudioBuffer<float> b (1, 48000);
                for (int i = 0; i < 48000; ++i)
                    b.setSample (0, i, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / 48000.0f));
                float* p = b.getWritePointer (0);
                engine.process (&p, 1, 48000);
                levels[d] = juce::Decibels::gainToDecibels (b.getRMSLevel (0, 24000, 24000));
            }
            expectWithinAbsoluteError (levels[1], levels[0], 4.0f, "12 dB drive");
            expectWithinAbsoluteError (levels[2], levels[0], 6.0f, "24 dB drive");
        }

        beginTest ("Changing character never clicks");
        {
            SaturateEngine engine;
            SaturateEngine::Settings s;
            s.driveDb = 12.0f;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> b (1, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;
            for (int block = 0; block < 400; ++block)
            {
                if (block % 100 == 99)
                {
                    s.character = (Character) (((int) s.character + 1) % 3);
                    engine.setSettings (s);
                }
                for (int i = 0; i < 64; ++i, ++n)
                    b.setSample (0, i, 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 100.0f * (float) n / 48000.0f));
                float* p = b.getWritePointer (0);
                engine.process (&p, 1, 64);
                for (int i = 0; i < 64; ++i)
                {
                    if (block > 10)
                        maxStep = std::max (maxStep, std::abs (b.getSample (0, i) - previous));
                    previous = b.getSample (0, i);
                }
            }
            expect (maxStep < 0.03f, "max step " + juce::String (maxStep));
        }

        beginTest ("Extremes stay finite and bounded");
        {
            SaturateEngine engine;
            engine.prepare (192000.0);
            juce::Random rng (29);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;
            for (int block = 0; block < 5000; ++block)
            {
                if (block % 5 == 0)
                {
                    SaturateEngine::Settings s;
                    s.driveDb = rng.nextBool() ? 0.0f : 24.0f;
                    s.character = (Character) rng.nextInt (3);
                    s.tone = rng.nextFloat() * 2.0f - 1.0f;
                    s.autoGain = rng.nextBool();
                    s.oversampling = 1 << rng.nextInt (3);
                    engine.setSettings (s);
                }
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 4.0f);
                engine.process (buffer.getArrayOfWritePointers(), 2, 64);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 8.0f;
            }
            expect (ok);
        }
    }

private:
    /** Amplitude of a sine component (Goertzel with a Hann window over the second half). */
    static double tone (const juce::AudioBuffer<float>& b, double hz)
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

static SaturateTests saturateTests;
