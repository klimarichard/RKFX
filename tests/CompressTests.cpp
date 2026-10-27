#include <juce_audio_basics/juce_audio_basics.h>

#include "CompressorEngine.h"
#include "dsp/Dynamics.h"

#include <cmath>

/** RKCompress DSP and the shared dynamics blocks: static curve, timing,
    stereo link, external sidechain, listen, auto makeup, auto release. */
class CompressTests : public juce::UnitTest
{
public:
    CompressTests() : juce::UnitTest ("RKCompress engine") {}

    void runTest() override
    {
        using Mode = rkfx::Dynamics::LevelDetector::Mode;
        using rkfx::Dynamics::compressorCurve;

        beginTest ("Gain computer: hard and soft knee");
        {
            expectWithinAbsoluteError (compressorCurve (-30.0f, -18.0f, 4.0f, 0.0f), -30.0f, 1.0e-5f);
            expectWithinAbsoluteError (compressorCurve (-6.0f, -18.0f, 4.0f, 0.0f), -15.0f, 1.0e-5f);
            expectWithinAbsoluteError (compressorCurve (0.0f, -20.0f, 20.0f, 0.0f), -19.0f, 1.0e-5f);

            // Soft knee meets the straight parts at both edges and stays below them in between.
            expectWithinAbsoluteError (compressorCurve (-21.0f, -18.0f, 4.0f, 6.0f), -21.0f, 1.0e-4f);
            expectWithinAbsoluteError (compressorCurve (-15.0f, -18.0f, 4.0f, 6.0f), -18.0f + 3.0f / 4.0f, 1.0e-4f);
            expect (compressorCurve (-18.0f, -18.0f, 4.0f, 6.0f) < -18.0f);
        }

        beginTest ("Steady level settles on the static curve");
        {
            CompressorEngine::Settings s;
            s.kneeDb = 0.0f;
            s.detection = Mode::peak;
            const auto out = steadyOutputDb (s, -6.0f);
            expectWithinAbsoluteError (out, -15.0f, 0.1f);
        }

        beginTest ("Below the threshold the signal is untouched");
        {
            CompressorEngine engine;
            engine.setSettings ({});
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::Random rng (1);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    buffer.setSample (ch, i, (rng.nextFloat() - 0.5f) * 0.01f);   // about -46 dB
            auto input = buffer;

            engine.process (buffer.getArrayOfWritePointers(), 2, 512, nullptr, 0);

            bool identical = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    identical = identical && juce::exactlyEqual (input.getSample (ch, i), buffer.getSample (ch, i));
            expect (identical);
        }

        beginTest ("Attack time sets how fast the gain comes down");
        {
            for (const float attackMs : { 1.0f, 10.0f, 50.0f })
            {
                CompressorEngine::Settings s;
                s.kneeDb = 0.0f;
                s.detection = Mode::peak;
                s.attackMs = attackMs;
                s.ratio = 20.0f;

                CompressorEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                // Constant 0 dBFS: target reduction = -18 + 18/20 = -17.1 dB.
                const int n = (int) (attackMs * 48.0f);
                juce::AudioBuffer<float> buffer (1, n);
                for (int i = 0; i < n; ++i)
                    buffer.setSample (0, i, 1.0f);
                engine.process (buffer.getArrayOfWritePointers(), 1, n, nullptr, 0);

                // After one time constant, 63% of the way.
                const auto reachedDb = rkfx::Dynamics::toDb (buffer.getSample (0, n - 1));
                expectWithinAbsoluteError (reachedDb, -17.1f * 0.632f, 0.6f, juce::String (attackMs) + " ms");
            }
        }

        beginTest ("Stereo link");
        {
            for (const float linkAmount : { 0.0f, 1.0f })
            {
                CompressorEngine::Settings s;
                s.kneeDb = 0.0f;
                s.detection = Mode::peak;
                s.link = linkAmount;

                CompressorEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                juce::AudioBuffer<float> buffer (2, 48000);
                for (int i = 0; i < 48000; ++i)
                {
                    buffer.setSample (0, i, 0.5f);    // loud left
                    buffer.setSample (1, i, 0.01f);   // quiet right
                }
                engine.process (buffer.getArrayOfWritePointers(), 2, 48000, nullptr, 0);

                const auto rightGainDb = rkfx::Dynamics::toDb (buffer.getSample (1, 47999) / 0.01f);
                if (linkAmount > 0.5f)
                    expectWithinAbsoluteError (rightGainDb, -9.0f, 0.1f, "linked: right follows left");
                else
                    expectWithinAbsoluteError (rightGainDb, 0.0f, 0.01f, "unlinked: right untouched");
            }
        }

        beginTest ("External sidechain ducks; missing sidechain does nothing");
        {
            CompressorEngine::Settings s;
            s.external = true;
            s.kneeDb = 0.0f;
            s.detection = Mode::peak;

            CompressorEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> main (2, 48000), side (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                {
                    main.setSample (ch, i, 0.05f);   // quiet backing track
                    side.setSample (ch, i, 0.5f);    // loud vocal
                }

            engine.process (main.getArrayOfWritePointers(), 2, 48000, side.getArrayOfReadPointers(), 2);
            expectWithinAbsoluteError (rkfx::Dynamics::toDb (main.getSample (0, 47999) / 0.05f), -9.0f, 0.1f);

            CompressorEngine silentSide;
            silentSide.setSettings (s);
            silentSide.prepare (48000.0);
            juce::AudioBuffer<float> loud (2, 4800);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4800; ++i)
                    loud.setSample (ch, i, 0.9f);
            silentSide.process (loud.getArrayOfWritePointers(), 2, 4800, nullptr, 0);
            expectEquals (loud.getSample (0, 4799), 0.9f);
        }

        beginTest ("Sidechain listen outputs the filtered detector");
        {
            CompressorEngine::Settings s;
            s.listen = true;
            s.sidechainHpfHz = 200.0f;

            CompressorEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (1, 48000);
            for (int i = 0; i < 48000; ++i)
                buffer.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 30.0f * (float) i / 48000.0f));
            engine.process (buffer.getArrayOfWritePointers(), 1, 48000, nullptr, 0);

            // 30 Hz through a 200 Hz high-pass: strongly attenuated.
            expect (buffer.getMagnitude (0, 24000, 24000) < 0.05f);
        }

        beginTest ("Auto makeup");
        {
            CompressorEngine::Settings s;
            s.kneeDb = 0.0f;
            expectWithinAbsoluteError (CompressorEngine::autoMakeupDb (s), 6.75f, 1.0e-4f);
            s.autoMakeup = true;
            s.makeupDb = 3.0f;
            expectWithinAbsoluteError (CompressorEngine::effectiveMakeupDb (s), 6.75f, 1.0e-4f);
            expectWithinAbsoluteError (CompressorEngine::transferDb (s, -40.0f), -40.0f + 6.75f, 1.0e-4f);
        }

        beginTest ("Auto release: fast after a short peak, slower after sustained compression");
        {
            const auto recovery = [] (int loudSamples)
            {
                rkfx::Dynamics::GainSmoother smoother;
                smoother.prepare (48000.0);
                smoother.setTimes (1.0f, 150.0f);
                smoother.setAutoRelease (true);

                for (int i = 0; i < loudSamples; ++i)
                    smoother.process (-12.0f);

                // Samples to recover from the reduction to within 3 dB.
                int n = 0;
                while (smoother.process (0.0f) < -3.0f && n < 480000)
                    ++n;
                return n;
            };

            const auto afterPeak = recovery (480);        // 10 ms burst
            const auto afterSustained = recovery (96000); // 2 s of compression
            expect (afterSustained > afterPeak * 4, juce::String (afterPeak) + " vs " + juce::String (afterSustained));
        }

        beginTest ("Extremes stay finite");
        {
            CompressorEngine engine;
            engine.prepare (44100.0);
            juce::Random rng (6);
            juce::AudioBuffer<float> buffer (2, 64), side (2, 64);
            bool finite = true;

            for (int block = 0; block < 5000; ++block)
            {
                if (block % 7 == 0)
                {
                    CompressorEngine::Settings s;
                    s.thresholdDb = rng.nextBool() ? -60.0f : 0.0f;
                    s.ratio = rng.nextBool() ? 1.0f : 20.0f;
                    s.attackMs = rng.nextBool() ? 0.1f : 100.0f;
                    s.releaseMs = rng.nextBool() ? 10.0f : 2000.0f;
                    s.autoRelease = rng.nextBool();
                    s.kneeDb = rng.nextBool() ? 0.0f : 12.0f;
                    s.detection = rng.nextBool() ? Mode::peak : Mode::rms;
                    s.external = rng.nextBool();
                    s.sidechainHpfHz = rng.nextBool() ? 0.0f : 300.0f;
                    s.listen = rng.nextBool();
                    s.link = rng.nextFloat();
                    s.makeupDb = 24.0f;
                    s.autoMakeup = rng.nextBool();
                    engine.setSettings (s);
                }

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                    {
                        buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 4.0f);
                        side.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);
                    }

                engine.process (buffer.getArrayOfWritePointers(), 2, 64, side.getArrayOfReadPointers(), 2);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        finite = finite && std::isfinite (buffer.getSample (ch, i));
            }
            expect (finite);
        }
    }

private:
    static float steadyOutputDb (const CompressorEngine::Settings& s, float inputDb)
    {
        CompressorEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);

        const auto level = rkfx::Dynamics::toGain (inputDb);
        juce::AudioBuffer<float> buffer (1, 48000);
        for (int i = 0; i < 48000; ++i)
            buffer.setSample (0, i, level);
        engine.process (buffer.getArrayOfWritePointers(), 1, 48000, nullptr, 0);
        return rkfx::Dynamics::toDb (buffer.getSample (0, 47999));
    }
};

static CompressTests compressTests;
