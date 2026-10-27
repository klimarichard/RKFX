#include <juce_audio_basics/juce_audio_basics.h>

#include "DistortEngine.h"

#include <cmath>

/** RKDistort DSP: clip curves, fuzz asymmetry and gating, tight, mid,
    cab curves, click-free type and cab changes, stability. */
class DistortTests : public juce::UnitTest
{
public:
    DistortTests() : juce::UnitTest ("RKDistort engine") {}

    void runTest() override
    {
        using Type = DistortEngine::Type;
        using Cab = DistortEngine::Cab;

        beginTest ("Clip curves: unity at low level, bounded");
        {
            for (const auto t : { Type::overdrive, Type::distortion, Type::fuzz })
            {
                expectWithinAbsoluteError (DistortEngine::clip (t, 0.001f) / 0.001f, 1.0f, 0.02f);
                expectWithinAbsoluteError (DistortEngine::clip (t, 0.0f), 0.0f, 1.0e-6f);
                expect (std::abs (DistortEngine::clip (t, 1000.0f)) < 2.0f);
                expect (std::abs (DistortEngine::clip (t, -1000.0f)) < 2.0f);
            }
        }

        beginTest ("Fuzz is asymmetric (2nd harmonic), overdrive is not");
        {
            const auto second = [] (Type t)
            {
                DistortEngine::Settings s;
                s.type = t;
                s.gainDb = 24.0f;
                const auto b = run (s, sine (220.0f, 0.2f));
                return juce::Decibels::gainToDecibels (tone (b, 440.0) / tone (b, 220.0));
            };
            expect (second (Type::fuzz) > -30.0f, "fuzz 2nd harmonic");
            expect (second (Type::overdrive) < -60.0f, "overdrive: no 2nd harmonic");
        }

        beginTest ("Fuzz gates very quiet signals; overdrive passes them");
        {
            const auto quietOut = [] (Type t)
            {
                DistortEngine::Settings s;
                s.type = t;
                s.gainDb = 30.0f;
                return run (s, sine (220.0f, 0.002f)).getRMSLevel (0, 24000, 24000);
            };
            expect (quietOut (Type::fuzz) < quietOut (Type::overdrive) * 0.05f);
        }

        beginTest ("Tight removes lows before the gain");
        {
            const auto low = [] (float tightHz)
            {
                DistortEngine::Settings s;
                s.type = Type::distortion;
                s.gainDb = 0.0f;
                s.tightHz = tightHz;
                s.oversampling = 1;
                return tone (run (s, sine (50.0f, 0.01f)), 50.0);
            };
            expect (low (400.0f) < low (0.0f) * 0.1f);
        }

        beginTest ("Mid boosts or cuts around 800 Hz");
        {
            const auto level = [] (float midDb)
            {
                DistortEngine::Settings s;
                s.type = Type::distortion;
                s.gainDb = 0.0f;
                s.midDb = midDb;
                s.oversampling = 1;
                return tone (run (s, sine (800.0f, 0.005f)), 800.0);
            };
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (level (12.0f) / level (0.0f)), 12.0, 0.3);
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (level (-12.0f) / level (0.0f)), -12.0, 0.3);
        }

        beginTest ("Cab curves: speaker-like band-pass shapes");
        {
            for (const auto c : { Cab::small, Cab::medium, Cab::large, Cab::bass })
            {
                DistortEngine::CabFilter cab;
                cab.configure (c, 48000.0);
                expect (cab.magnitudeDb (10000.0, 48000.0) < -15.0, "highs rolled off");
                expect (cab.magnitudeDb (20.0, 48000.0) < -6.0, "deep lows rolled off");
                expect (std::abs (cab.magnitudeDb (1000.0, 48000.0)) < 6.0, "mids about level");
            }

            DistortEngine::CabFilter bass, small;
            bass.configure (Cab::bass, 48000.0);
            small.configure (Cab::small, 48000.0);
            expect (bass.magnitudeDb (50.0, 48000.0) > small.magnitudeDb (50.0, 48000.0) + 6.0, "the bass cab keeps more low end");
        }

        beginTest ("Changing type and cab never clicks");
        {
            DistortEngine engine;
            DistortEngine::Settings s;
            s.gainDb = 20.0f;
            s.autoGain = true;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> b (1, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;
            for (int block = 0; block < 800; ++block)
            {
                if (block % 100 == 99)
                {
                    s.type = (Type) (((int) s.type + 1) % 3);
                    s.cab = (Cab) (((int) s.cab + 1) % 5);
                    engine.setSettings (s);
                }
                for (int i = 0; i < 64; ++i, ++n)
                    b.setSample (0, i, 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 110.0f * (float) n / 48000.0f));
                float* p = b.getWritePointer (0);
                engine.process (&p, 1, 64);
                for (int i = 0; i < 64; ++i)
                {
                    if (block > 10)
                        maxStep = std::max (maxStep, std::abs (b.getSample (0, i) - previous));
                    previous = b.getSample (0, i);
                }
            }
            expect (maxStep < 0.08f, "max step " + juce::String (maxStep));
        }

        beginTest ("Extremes stay finite and bounded");
        {
            DistortEngine engine;
            engine.prepare (44100.0);
            juce::Random rng (31);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;
            for (int block = 0; block < 5000; ++block)
            {
                if (block % 5 == 0)
                {
                    DistortEngine::Settings s;
                    s.type = (Type) rng.nextInt (3);
                    s.gainDb = rng.nextBool() ? 0.0f : 48.0f;
                    s.tightHz = rng.nextBool() ? 0.0f : 500.0f;
                    s.midDb = rng.nextBool() ? -12.0f : 12.0f;
                    s.tone = rng.nextFloat() * 2.0f - 1.0f;
                    s.cab = (Cab) rng.nextInt (5);
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
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 16.0f;
            }
            expect (ok);
        }
    }

private:
    static juce::AudioBuffer<float> sine (float hz, float amplitude)
    {
        juce::AudioBuffer<float> b (1, 48000);
        for (int i = 0; i < 48000; ++i)
            b.setSample (0, i, amplitude * std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f));
        return b;
    }

    static juce::AudioBuffer<float> run (const DistortEngine::Settings& s, juce::AudioBuffer<float> b)
    {
        DistortEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        float* p = b.getWritePointer (0);
        engine.process (&p, 1, b.getNumSamples());
        return b;
    }

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

static DistortTests distortTests;
