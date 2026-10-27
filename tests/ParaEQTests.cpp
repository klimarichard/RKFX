#include <juce_audio_basics/juce_audio_basics.h>

#include "ParaEQEngine.h"
#include "dsp/MatchedBiquad.h"

#include <cmath>

/** RKParaEQ DSP and the matched bell/shelf design: transparency, accuracy
    (including near Nyquist), click-free switching, stability. */
class ParaEQTests : public juce::UnitTest
{
public:
    ParaEQTests() : juce::UnitTest ("RKParaEQ engine") {}

    void runTest() override
    {
        beginTest ("Defaults are exactly transparent");
        {
            ParaEQEngine engine;
            engine.setSettings (ParaEQEngine::defaults());
            engine.prepare (48000.0);

            juce::Random rng (5);
            juce::AudioBuffer<float> input (2, 512);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    input.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

            auto output = input;
            engine.process (output.getArrayOfWritePointers(), 2, 512);

            bool identical = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    identical = identical && juce::exactlyEqual (input.getSample (ch, i), output.getSample (ch, i));
            expect (identical);
        }

        beginTest ("Bells and shelves hit their gain; cut is the mirror of boost");
        {
            using namespace rkfx::MatchedDesign;

            for (const double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            {
                for (const double hz : { 30.0, 250.0, 1000.0, 5000.0, 12000.0 })
                {
                    for (const double gain : { -18.0, -6.0, 6.0, 18.0 })
                    {
                        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / rate;
                        expectWithinAbsoluteError (toDb (bell (hz, gain, 2.0, rate).magnitude (w)), gain, 0.05,
                                                   "bell " + juce::String (hz) + " Hz @" + juce::String (rate));

                        const auto boost = bell (hz, std::abs (gain), 1.0, rate);
                        const auto cut = bell (hz, -std::abs (gain), 1.0, rate);
                        for (const double f : { 50.0, 800.0, 9000.0 })
                        {
                            const auto wf = 2.0 * juce::MathConstants<double>::pi * f / rate;
                            expectWithinAbsoluteError (boost.magnitude (wf) * cut.magnitude (wf), 1.0, 1.0e-9);
                        }
                    }
                }

                const auto lowShelfAt = [&] (double f) { return toDb (lowShelf (200.0, -12.0, rate).magnitude (2.0 * juce::MathConstants<double>::pi * f / rate)); };
                expectWithinAbsoluteError (lowShelfAt (20.0), -12.0, 0.3);
                expectWithinAbsoluteError (lowShelfAt (5000.0), 0.0, 0.2);

                const auto highShelfAt = [&] (double f) { return toDb (highShelf (3000.0, 9.0, rate).magnitude (2.0 * juce::MathConstants<double>::pi * f / rate)); };
                expectWithinAbsoluteError (highShelfAt (18000.0), 9.0, 0.5);
                expectWithinAbsoluteError (highShelfAt (100.0), 0.0, 0.2);
            }
        }

        beginTest ("No cramping: a 16 kHz bell keeps its shape at 44.1 kHz");
        {
            using namespace rkfx::MatchedDesign;
            const auto c = bell (16000.0, 12.0, 1.0, 44100.0);
            const auto at = [&] (double f) { return toDb (c.magnitude (2.0 * juce::MathConstants<double>::pi * f / 44100.0)); };

            expectWithinAbsoluteError (at (16000.0), 12.0, 0.05);
            // One octave below, within 1.5 dB of the analog bell (a plain bilinear
            // bell is much narrower here). The design is within 1 dB up to 12 kHz.
            expectWithinAbsoluteError (at (8000.0), analogBellDb (8000.0, 16000.0, 12.0, 1.0), 1.5);
            expect (at (20000.0) > 8.0, "still boosting near Nyquist: " + juce::String (at (20000.0)));
        }

        beginTest ("The engine's sound matches the displayed curve");
        {
            auto s = ParaEQEngine::defaults();
            s.highPass = { true, 120.0f, 2 };
            s.lowPass = { true, 9000.0f, 4 };
            s.bands[ParaEQEngine::low] = { true, false, 150.0f, -6.0f, 0.71f };
            s.bands[ParaEQEngine::mid] = { true, true, 1000.0f, 9.0f, 2.0f };
            s.bands[ParaEQEngine::high] = { true, true, 6000.0f, -4.0f, 1.5f };

            for (const float hz : { 60.0f, 150.0f, 400.0f, 1000.0f, 2500.0f, 6000.0f, 12000.0f })
            {
                const auto shown = ParaEQEngine::magnitudeDb (s, 48000.0, hz);
                const auto measured = toDb (gainAt (s, hz));
                expectWithinAbsoluteError (measured, shown, 0.2, juce::String (hz) + " Hz");
            }
        }

        beginTest ("Switching shape, bands and cut filters never clicks");
        {
            ParaEQEngine engine;
            auto s = ParaEQEngine::defaults();
            s.bands[ParaEQEngine::low] = { true, false, 200.0f, 12.0f, 0.71f };
            s.bands[ParaEQEngine::mid] = { true, true, 300.0f, 12.0f, 1.0f };
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (1, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;

            for (int block = 0; block < 600; ++block)
            {
                if (block == 100) { s.bands[ParaEQEngine::low].bell = true; engine.setSettings (s); }
                if (block == 200) { s.bands[ParaEQEngine::mid].on = false; engine.setSettings (s); }
                if (block == 300) { s.highPass = { true, 150.0f, 4 }; engine.setSettings (s); }
                if (block == 400) { s.highPass.stages = 1; engine.setSettings (s); }
                if (block == 500) { s.lowPass = { true, 1000.0f, 2 }; engine.setSettings (s); }

                for (int i = 0; i < 64; ++i, ++n)
                    buffer.setSample (0, i, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) n / 48000.0f));

                engine.process (buffer.getArrayOfWritePointers(), 1, 64);

                for (int i = 0; i < 64; ++i)
                {
                    if (block > 10)
                        maxStep = std::max (maxStep, std::abs (buffer.getSample (0, i) - previous));
                    previous = buffer.getSample (0, i);
                }
            }

            // The signal itself (220 Hz, up to about +18 dB) moves at most ~0.06 per sample.
            expect (maxStep < 0.09f, "max step " + juce::String (maxStep));
        }

        beginTest ("Extremes and fast changes stay finite");
        {
            for (const double rate : { 44100.0, 192000.0 })
            {
                ParaEQEngine engine;
                engine.prepare (rate);
                juce::Random rng (9);
                juce::AudioBuffer<float> buffer (2, 33);
                bool finite = true;

                for (int block = 0; block < 10000; ++block)
                {
                    if (block % 4 == 0)
                    {
                        ParaEQEngine::Settings s;
                        s.highPass = { rng.nextBool(), rng.nextBool() ? 20.0f : 1000.0f, 1 + rng.nextInt (4) };
                        s.lowPass = { rng.nextBool(), rng.nextBool() ? 1000.0f : 20000.0f, 1 + rng.nextInt (4) };
                        for (auto& b : s.bands)
                            b = { rng.nextBool(), rng.nextBool(), rng.nextBool() ? 20.0f : 20000.0f,
                                  rng.nextBool() ? 18.0f : -18.0f, rng.nextBool() ? 0.3f : 10.0f };
                        engine.setSettings (s);
                    }

                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 33; ++i)
                            buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

                    engine.process (buffer.getArrayOfWritePointers(), 2, 33);

                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 33; ++i)
                            finite = finite && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 1000.0f;
                }

                expect (finite);
            }
        }
    }

private:
    static double toDb (double gain) { return 20.0 * std::log10 (std::max (gain, 1.0e-12)); }

    static double analogBellDb (double f, double f0, double gainDb, double q)
    {
        const auto a = std::pow (10.0, gainDb / 40.0);
        const std::complex<double> s (0.0, f / f0);
        return toDb (std::abs ((s * s + s * (a / q) + 1.0) / (s * s + s / (a * q) + 1.0)));
    }

    static double gainAt (const ParaEQEngine::Settings& s, float hz)
    {
        constexpr double rate = 48000.0;
        ParaEQEngine engine;
        engine.setSettings (s);
        engine.prepare (rate);

        const int total = 48000;
        juce::AudioBuffer<float> buffer (1, total);
        for (int i = 0; i < total; ++i)
            buffer.setSample (0, i, 0.1f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / rate));

        for (int start = 0; start < total; start += 256)
        {
            auto* p = buffer.getWritePointer (0, start);
            engine.process (&p, 1, std::min (256, total - start));
        }

        // RMS, not peak: a sampled high-frequency sine rarely hits its true peak.
        return buffer.getRMSLevel (0, total / 2, total / 2) / (0.1 / std::sqrt (2.0));
    }
};

static ParaEQTests paraEQTests;
