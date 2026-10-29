#include <juce_audio_basics/juce_audio_basics.h>

#include "ParaEQEngine.h"

#include <cmath>

namespace
{
    constexpr double kRate = 48000.0;

    float dB (double gain) { return (float) (20.0 * std::log10 (std::max (gain, 1.0e-9))); }

    /** Amplitude of a sine whose RMS is @p rmsDb. */
    float amplitudeForRms (float rmsDb) { return std::sqrt (2.0f) * std::pow (10.0f, rmsDb / 20.0f); }

    struct Result
    {
        float gainDb;           // steady-state output / input at the tone
        float dynamicDb;        // the band's dynamic gain at the end
    };

    /** Runs a stereo sine for a second and measures the last quarter. */
    Result run (const ParaEQEngine::Settings& s, int band, double hz, float amplitude)
    {
        ParaEQEngine engine;
        engine.setSettings (s);
        engine.prepare (kRate);

        const auto settle = (int) kRate, measure = (int) kRate / 4;
        juce::AudioBuffer<float> buffer (2, settle + measure);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / kRate));

        const auto input = buffer;
        engine.process (buffer.getArrayOfWritePointers(), 2, buffer.getNumSamples());

        double in2 = 0.0, out2 = 0.0;
        for (int i = settle; i < buffer.getNumSamples(); ++i)
        {
            in2 += (double) input.getSample (0, i) * input.getSample (0, i);
            out2 += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        }
        return { dB (std::sqrt (out2 / in2)), engine.takeMeter (band).gainDb };
    }

    ParaEQEngine::Settings dynamicBell (int band, float hz, float q, ParaEQEngine::Dynamic d)
    {
        auto s = ParaEQEngine::defaults();
        auto& b = s.bands[(size_t) band];
        b.bell = true;
        b.hz = hz;
        b.q = q;
        d.on = true;
        b.dynamic = d;
        return s;
    }
}

/** RKParaEQ dynamic bands: unchanged when off, cut above / boost below the
    threshold within Range, the boost window, frequency selectivity, shelves,
    the live response, no clicks. */
class ParaEQDynamicTests : public juce::UnitTest
{
public:
    ParaEQDynamicTests() : juce::UnitTest ("RKParaEQ dynamic bands") {}

    void runTest() override
    {
        using Engine = ParaEQEngine;

        beginTest ("Dynamic off: the output doesn't depend on the dynamics settings");
        {
            auto a = Engine::defaults();
            a.bands[Engine::low] = { true, false, 150.0f, -6.0f, 0.71f, {} };
            a.bands[Engine::mid] = { true, true, 1000.0f, 9.0f, 2.0f, {} };
            auto b = a;
            for (auto& band : b.bands)
                band.dynamic = { false, -60.0f, 24.0f, 10.0f, 1.0f, 2000.0f };

            juce::Random rng (11);
            juce::AudioBuffer<float> x (2, 4096);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4096; ++i)
                    x.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

            auto ya = x, yb = x;
            Engine ea, eb;
            ea.setSettings (a);
            ea.prepare (kRate);
            eb.setSettings (b);
            eb.prepare (kRate);
            ea.process (ya.getArrayOfWritePointers(), 2, 4096);
            eb.process (yb.getArrayOfWritePointers(), 2, 4096);

            bool identical = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4096; ++i)
                    identical = identical && juce::exactlyEqual (ya.getSample (ch, i), yb.getSample (ch, i));
            expect (identical);
        }

        beginTest ("The gain computer: cut above, boost below, within Range, boost window");
        {
            Engine::Dynamic cut { true, -30.0f, -24.0f, 3.0f, 10.0f, 150.0f };
            expectWithinAbsoluteError (Engine::dynamicGainDb (cut, -13.0f), -17.0f * 2.0f / 3.0f, 0.001f);
            expectWithinAbsoluteError (Engine::dynamicGainDb (cut, -50.0f), 0.0f, 0.001f);
            cut.rangeDb = -6.0f;
            expectWithinAbsoluteError (Engine::dynamicGainDb (cut, -13.0f), -6.0f, 0.001f);

            Engine::Dynamic boost { true, -30.0f, 6.0f, 2.0f, 10.0f, 150.0f };
            expectWithinAbsoluteError (Engine::dynamicGainDb (boost, -40.0f), 5.0f, 0.001f);
            expectWithinAbsoluteError (Engine::dynamicGainDb (boost, -20.0f), 0.0f, 0.001f);
            expectWithinAbsoluteError (Engine::dynamicGainDb (boost, -50.0f), 6.0f, 0.001f);   // capped by Range
            expectWithinAbsoluteError (Engine::dynamicGainDb (boost, -65.0f), 3.0f, 0.001f);   // half way through the fade
            expectWithinAbsoluteError (Engine::dynamicGainDb (boost, -75.0f), 0.0f, 0.001f);   // below the window
            expectWithinAbsoluteError (Engine::dynamicGainDb (boost, -120.0f), 0.0f, 0.001f);  // silence

            boost.on = false;
            expectWithinAbsoluteError (Engine::dynamicGainDb (boost, -40.0f), 0.0f, 0.001f);
        }

        beginTest ("A loud tone at a dynamic bell is cut by the ratio, never beyond Range");
        {
            const auto loud = amplitudeForRms (-13.0f);
            auto r = run (dynamicBell (Engine::mid, 1000.0f, 1.0f, { true, -30.0f, -24.0f, 3.0f, 10.0f, 150.0f }), Engine::mid, 1000.0, loud);
            expectWithinAbsoluteError (r.gainDb, -17.0f * 2.0f / 3.0f, 0.3f, "ratio 3:1");

            r = run (dynamicBell (Engine::mid, 1000.0f, 1.0f, { true, -30.0f, -6.0f, 3.0f, 10.0f, 150.0f }), Engine::mid, 1000.0, loud);
            expectWithinAbsoluteError (r.gainDb, -6.0f, 0.2f, "Range -6 dB");

            r = run (dynamicBell (Engine::mid, 1000.0f, 1.0f, { true, -30.0f, -6.0f, 3.0f, 10.0f, 150.0f }), Engine::mid, 1000.0,
                     amplitudeForRms (-50.0f));
            expectWithinAbsoluteError (r.gainDb, 0.0f, 0.05f, "below the threshold");
        }

        beginTest ("A quiet tone is lifted, a loud one isn't, silence-level isn't");
        {
            const auto s = dynamicBell (Engine::mid, 1000.0f, 1.0f, { true, -30.0f, 6.0f, 2.0f, 10.0f, 150.0f });
            expectWithinAbsoluteError (run (s, Engine::mid, 1000.0, amplitudeForRms (-40.0f)).gainDb, 5.0f, 0.3f, "10 dB under");
            expectWithinAbsoluteError (run (s, Engine::mid, 1000.0, amplitudeForRms (-20.0f)).gainDb, 0.0f, 0.05f, "above");
            expectWithinAbsoluteError (run (s, Engine::mid, 1000.0, amplitudeForRms (-75.0f)).gainDb, 0.0f, 0.05f, "45 dB under");
        }

        beginTest ("A bell only hears its own frequency");
        {
            const auto s = dynamicBell (Engine::mid, 1000.0f, 2.0f, { true, -20.0f, -12.0f, 3.0f, 10.0f, 150.0f });
            expect (run (s, Engine::mid, 250.0, 0.5f).dynamicDb > -0.1f, "two octaves away: untouched");
            expect (run (s, Engine::mid, 1000.0, 0.5f).dynamicDb < -5.0f, "at its frequency: cut");
        }

        beginTest ("Shelves hear their own side");
        {
            auto s = Engine::defaults();
            s.bands[Engine::low].hz = 200.0f;
            s.bands[Engine::low].dynamic = { true, -30.0f, -12.0f, 3.0f, 10.0f, 150.0f };
            expect (run (s, Engine::low, 2000.0, 0.5f).dynamicDb > -0.1f, "low shelf ignores 2 kHz");
            expect (run (s, Engine::low, 60.0, 0.5f).dynamicDb < -5.0f, "low shelf reacts to 60 Hz");

            s = Engine::defaults();
            s.bands[Engine::high].hz = 5000.0f;
            s.bands[Engine::high].dynamic = { true, -30.0f, -12.0f, 3.0f, 10.0f, 150.0f };
            expect (run (s, Engine::high, 500.0, 0.5f).dynamicDb > -0.1f, "high shelf ignores 500 Hz");
            expect (run (s, Engine::high, 12000.0, 0.5f).dynamicDb < -5.0f, "high shelf reacts to 12 kHz");
        }

        beginTest ("The live response matches what is heard");
        {
            auto s = dynamicBell (Engine::mid, 1000.0f, 1.0f, { true, -30.0f, -24.0f, 3.0f, 10.0f, 150.0f });
            s.bands[Engine::mid].gainDb = 3.0f;
            const auto r = run (s, Engine::mid, 1000.0, amplitudeForRms (-13.0f));
            std::array<float, Engine::kBands> gains {};
            gains[Engine::mid] = r.dynamicDb;
            const Engine::Response response (s, kRate, &gains);
            expectWithinAbsoluteError ((float) response.magnitudeDb (1000.0), r.gainDb, 0.2f);
        }

        beginTest ("Pumping, toggling, Range sweeps and shape changes never click");
        {
            Engine engine;
            auto s = Engine::defaults();
            s.bands[Engine::lowMid] = { true, true, 220.0f, 3.0f, 1.0f, { true, -24.0f, -6.0f, 3.0f, 1.0f, 50.0f } };
            s.bands[Engine::low] = { true, false, 150.0f, 2.0f, 0.71f, { true, -30.0f, -6.0f, 3.0f, 5.0f, 100.0f } };
            engine.setSettings (s);
            engine.prepare (kRate);

            juce::AudioBuffer<float> buffer (2, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;

            for (int block = 0; block < 3000; ++block)
            {
                if (block == 600)  { s.bands[Engine::lowMid].dynamic.on = false; engine.setSettings (s); }
                if (block == 900)  { s.bands[Engine::lowMid].dynamic.on = true; engine.setSettings (s); }
                if (block >= 1200 && block < 1800)
                {
                    s.bands[Engine::lowMid].dynamic.rangeDb = -24.0f + 48.0f * (float) (block - 1200) / 600.0f;
                    s.bands[Engine::lowMid].dynamic.thresholdDb = -45.0f;
                    engine.setSettings (s);
                }
                if (block == 2000) { s.bands[Engine::low].bell = true; engine.setSettings (s); }
                if (block == 2400) { s.bands[Engine::low].bell = false; engine.setSettings (s); }
                if (block == 2700) { s.bands[Engine::low].on = false; engine.setSettings (s); }

                for (int i = 0; i < 64; ++i, ++n)
                {
                    // 220 Hz, swelling between quiet and loud every half second.
                    const auto swell = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) n / 24000.0f);
                    const auto amplitude = 0.02f + 0.18f * swell;
                    for (int ch = 0; ch < 2; ++ch)
                        buffer.setSample (ch, i, amplitude * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) n / 48000.0f));
                }

                engine.process (buffer.getArrayOfWritePointers(), 2, 64);

                for (int i = 0; i < 64; ++i)
                {
                    if (block > 10)
                        maxStep = std::max (maxStep, std::abs (buffer.getSample (0, i) - previous));
                    previous = buffer.getSample (0, i);
                }
            }

            // The tone itself (220 Hz at up to 0.2, boosted up to about +8 dB) moves at most ~0.015 per sample.
            expect (maxStep < 0.02f, "max step " + juce::String (maxStep));
        }

        beginTest ("Extremes stay finite");
        {
            for (const double rate : { 44100.0, 192000.0 })
            {
                Engine engine;
                auto s = Engine::defaults();
                for (auto& b : s.bands)
                {
                    b.gainDb = 18.0f;
                    b.q = 10.0f;
                    b.dynamic = { true, -60.0f, 24.0f, 10.0f, 1.0f, 10.0f };
                }
                s.bands[Engine::high].hz = 20000.0f;
                engine.setSettings (s);
                engine.prepare (rate);

                juce::Random rng (4);
                juce::AudioBuffer<float> buffer (2, 33);
                bool finite = true;
                for (int block = 0; block < 2000; ++block)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 33; ++i)
                            buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * (block % 200 < 100 ? 1.0f : 1.0e-4f));
                    engine.process (buffer.getArrayOfWritePointers(), 2, 33);
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 33; ++i)
                            finite = finite && std::isfinite (buffer.getSample (ch, i));
                }
                expect (finite);
            }
        }
    }
};

static ParaEQDynamicTests paraEQDynamicTests;
