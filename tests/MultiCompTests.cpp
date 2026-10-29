#include <juce_audio_basics/juce_audio_basics.h>

#include "CompressorEngine.h"
#include "MultiCompEngine.h"

#include <cmath>

namespace
{
    float dB (double gain) { return (float) (20.0 * std::log10 (std::max (gain, 1.0e-9))); }

    /** Steady-state gain (RMS out / RMS in) of a sine at @p hz. */
    float gainDbAt (const MultiCompEngine::Settings& s, double rate, double hz, float amplitude = 0.01f)
    {
        MultiCompEngine engine;
        engine.setSettings (s);
        engine.prepare (rate);

        const auto settle = (int) rate, measure = (int) rate / 4;
        juce::AudioBuffer<float> buffer (1, settle + measure);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / rate));

        const auto input = buffer;
        engine.process (buffer.getArrayOfWritePointers(), 1, buffer.getNumSamples());

        double in2 = 0.0, out2 = 0.0;
        for (int i = settle; i < buffer.getNumSamples(); ++i)
        {
            in2 += (double) input.getSample (0, i) * input.getSample (0, i);
            out2 += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        }
        return dB (std::sqrt (out2 / in2));
    }

    MultiCompEngine::Settings allBandsOff (bool fourBands)
    {
        MultiCompEngine::Settings s;
        s.fourBands = fourBands;
        for (auto& b : s.bands)
            b.on = false;
        return s;
    }

    juce::AudioBuffer<float> sine (double hz, float amplitude, int numSamples, double rate = 48000.0)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numSamples; ++i)
                b.setSample (ch, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / rate));
        return b;
    }
}

/** RKMultiComp DSP: the bands add up flat, the dry signal is phase-matched,
    bands compress independently and like RKCompress, crossovers keep their
    order, and nothing clicks. */
class MultiCompTests : public juce::UnitTest
{
public:
    MultiCompTests() : juce::UnitTest ("RKMultiComp engine") {}

    void runTest() override
    {
        beginTest ("Idle bands add up flat (3 and 4 bands, every sample rate)");
        for (const auto four : { false, true })
            for (const auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
                for (const auto hz : { 20.0, 60.0, 150.0, 400.0, 1000.0, 2000.0, 3500.0, 8000.0, 16000.0, 20000.0 })
                {
                    if (hz >= rate * 0.45)
                        continue;
                    expectWithinAbsoluteError (gainDbAt (allBandsOff (four), rate, hz), 0.0f, 0.01f,
                                               juce::String (four ? 4 : 3) + " bands, " + juce::String (rate) + " Hz rate, "
                                               + juce::String (hz) + " Hz");
                }

        beginTest ("Ratio 1:1 is flat too");
        {
            auto s = MultiCompEngine::Settings();
            s.fourBands = true;
            for (auto& b : s.bands)
                b.ratio = 1.0f;
            for (const auto hz : { 50.0, 700.0, 2500.0, 10000.0 })
                expectWithinAbsoluteError (gainDbAt (s, 48000.0, hz, 0.5f), 0.0f, 0.01f, juce::String (hz) + " Hz");
        }

        beginTest ("Dry/Wet is phase-matched: no comb dips at the crossovers");
        for (const auto four : { false, true })
        {
            auto s = allBandsOff (four);
            s.mix = 0.5f;
            // Equal-power 50% of two identical signals is +3 dB everywhere.
            for (const auto hz : { 40.0, 150.0, 300.0, 1000.0, 3500.0, 6000.0, 15000.0 })
                expectWithinAbsoluteError (gainDbAt (s, 48000.0, hz), 3.01f, 0.02f, juce::String (hz) + " Hz");
        }

        beginTest ("A tone compresses its own band only");
        {
            MultiCompEngine engine;
            MultiCompEngine::Settings s;
            s.fourBands = true;
            engine.setSettings (s);
            engine.prepare (48000.0);

            auto b = sine (60.0, 0.5f, 48000);
            engine.process (b.getArrayOfWritePointers(), 2, b.getNumSamples());

            expect (engine.takeMeter (MultiCompEngine::low).reductionDb < -6.0f, "low band compresses");
            for (const auto band : { MultiCompEngine::lowMid, MultiCompEngine::highMid, MultiCompEngine::high })
                expect (engine.takeMeter (band).reductionDb > -0.1f, "band " + juce::String ((int) band) + " untouched");
        }

        beginTest ("A band compresses like RKCompress");
        {
            CompressorEngine::Settings c;
            c.thresholdDb = -24.0f;
            c.ratio = 4.0f;
            c.attackMs = 10.0f;
            c.releaseMs = 150.0f;
            c.kneeDb = 6.0f;

            MultiCompEngine::Settings m;
            m.bands[MultiCompEngine::lowMid] = { -24.0f, 4.0f, 10.0f, 150.0f };
            m.kneeDb = 6.0f;

            CompressorEngine single;
            single.setSettings (c);
            single.prepare (48000.0);
            MultiCompEngine multi;
            multi.setSettings (m);
            multi.prepare (48000.0);

            // 700 Hz: in the middle of the Mid band (150 Hz to 3.5 kHz), where its filters pass it at 0 dB.
            auto a = sine (700.0, 0.5f, 48000), b = a;
            single.process (a.getArrayOfWritePointers(), 2, 47000, nullptr, 0);
            multi.process (b.getArrayOfWritePointers(), 2, 47000);
            single.takeGainReductionDb();
            multi.takeMeter (MultiCompEngine::lowMid);

            float* const ap[] = { a.getWritePointer (0, 47000), a.getWritePointer (1, 47000) };
            float* const bp[] = { b.getWritePointer (0, 47000), b.getWritePointer (1, 47000) };
            single.process (ap, 2, 1000, nullptr, 0);
            multi.process (bp, 2, 1000);

            const auto expected = single.takeGainReductionDb();
            const auto measured = multi.takeMeter (MultiCompEngine::lowMid).reductionDb;
            expect (expected < -5.0f, "compressing: " + juce::String (expected));
            expectWithinAbsoluteError (measured, expected, 0.3f);
        }

        beginTest ("Crossovers stay in order, half an octave apart");
        {
            MultiCompEngine::Settings s;
            s.lowHz = 500.0f;
            s.highHz = 600.0f;
            auto x = MultiCompEngine::crossoversInUse (s);
            expectWithinAbsoluteError (x.low, 500.0f, 0.01f);
            expectWithinAbsoluteError (x.high, 500.0f * std::sqrt (2.0f), 0.1f);
            expect (x.mid > x.low && x.mid < x.high);

            s.fourBands = true;
            s.lowHz = 400.0f;
            s.midHz = 300.0f;
            s.highHz = 350.0f;
            x = MultiCompEngine::crossoversInUse (s);
            expectWithinAbsoluteError (x.mid, 400.0f * std::sqrt (2.0f), 0.1f);
            expectWithinAbsoluteError (x.high, 800.0f, 0.1f);

            s.lowHz = 100.0f;
            s.midHz = 1000.0f;
            s.highHz = 5000.0f;
            x = MultiCompEngine::crossoversInUse (s);
            expect (juce::exactlyEqual (x.low, 100.0f) && juce::exactlyEqual (x.mid, 1000.0f) && juce::exactlyEqual (x.high, 5000.0f),
                    "well-spaced crossovers are used as set");
        }

        beginTest ("Offset moves every threshold; Auto gain adds half the reduction at 0 dBFS");
        {
            MultiCompEngine::Settings s;
            s.offsetDb = -6.0f;
            expectWithinAbsoluteError (MultiCompEngine::thresholdDb (s, MultiCompEngine::high), -30.0f, 0.001f);
            s.autoGain = true;
            s.kneeDb = 0.0f;
            s.bands[MultiCompEngine::low].ratio = 4.0f;
            s.bands[MultiCompEngine::low].gainDb = 1.0f;
            // -30 dB threshold at 4:1: 0 dBFS comes out at -22.5 dB, so 11.25 dB back, plus the 1 dB gain.
            expectWithinAbsoluteError (MultiCompEngine::makeupDb (s, MultiCompEngine::low), 12.25f, 0.001f);
        }

        beginTest ("Solo plays only the soloed band");
        {
            auto s = allBandsOff (false);
            s.bands[MultiCompEngine::high].solo = true;
            expect (gainDbAt (s, 48000.0, 60.0) < -40.0f, "low band muted");
            expectWithinAbsoluteError (gainDbAt (s, 48000.0, 12000.0), 0.0f, 0.05f, "high band plays");
        }

        beginTest ("Sweeps, band count, solo, band off and Dry/Wet never click");
        {
            MultiCompEngine engine;
            MultiCompEngine::Settings s;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (2, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;

            for (int block = 0; block < 1200; ++block)
            {
                if (block >= 100 && block < 300) { s.lowHz = 100.0f * std::pow (10.0f, (float) (block - 100) / 200.0f); engine.setSettings (s); }
                if (block == 300) { s.fourBands = true; engine.setSettings (s); }
                if (block == 400) { s.bands[MultiCompEngine::highMid].solo = true; engine.setSettings (s); }
                if (block == 500) { s.bands[MultiCompEngine::highMid].solo = false; engine.setSettings (s); }
                if (block == 600) { s.bands[MultiCompEngine::lowMid].on = false; engine.setSettings (s); }
                if (block == 700) { s.bands[MultiCompEngine::lowMid].on = true; engine.setSettings (s); }
                if (block == 800) { s.fourBands = false; engine.setSettings (s); }
                if (block == 900) { s.mix = 0.3f; engine.setSettings (s); }
                if (block == 1000) { s.offsetDb = -12.0f; engine.setSettings (s); }

                for (int i = 0; i < 64; ++i, ++n)
                    for (int ch = 0; ch < 2; ++ch)
                        buffer.setSample (ch, i, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) n / 48000.0f));

                engine.process (buffer.getArrayOfWritePointers(), 2, 64);

                for (int i = 0; i < 64; ++i)
                {
                    if (block > 10)
                        maxStep = std::max (maxStep, std::abs (buffer.getSample (0, i) - previous));
                    previous = buffer.getSample (0, i);
                }
            }

            // The signal itself (220 Hz at 0.25) moves at most ~0.0072 per sample.
            expect (maxStep < 0.011f, "max step " + juce::String (maxStep));
        }

        beginTest ("Extremes stay finite");
        {
            for (const double rate : { 44100.0, 192000.0 })
            {
                MultiCompEngine engine;
                MultiCompEngine::Settings s;
                s.fourBands = true;
                s.lowHz = 1000.0f;
                s.midHz = 5000.0f;
                s.highHz = 12000.0f;
                for (auto& b : s.bands)
                    b = { -60.0f, 20.0f, 0.1f, 10.0f, 24.0f };
                s.autoGain = true;
                engine.setSettings (s);
                engine.prepare (rate);

                juce::Random rng (3);
                juce::AudioBuffer<float> buffer (2, 33);
                bool finite = true;
                for (int block = 0; block < 2000; ++block)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 33; ++i)
                            buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);
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

static MultiCompTests multiCompTests;
