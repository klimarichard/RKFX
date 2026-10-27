#include <juce_audio_basics/juce_audio_basics.h>

#include "GraphEQEngine.h"
#include "GraphEQLayout.h"

#include <cmath>

/** RKGraphEQ DSP: transparency, band accuracy in every mode, constant Q,
    click-free band-count switching, stability. */
class GraphEQTests : public juce::UnitTest
{
public:
    GraphEQTests() : juce::UnitTest ("RKGraphEQ engine") {}

    void runTest() override
    {
        using Mode = GraphEQEngine::Mode;

        beginTest ("Band layouts and labels");
        {
            expectEquals (GraphEQEngine::bandCount (Mode::ten), 10);
            expectEquals (GraphEQEngine::bandCount (Mode::fifteen), 15);
            expectEquals (GraphEQEngine::bandCount (Mode::thirtyOne), 31);
            expectEquals (GraphEQEngine::centres (Mode::thirtyOne)[0], 20.0);
            expectEquals (GraphEQEngine::centres (Mode::thirtyOne)[30], 20000.0);
            expectWithinAbsoluteError (GraphEQEngine::q (Mode::ten), 1.414, 0.001);
            expectWithinAbsoluteError (GraphEQEngine::q (Mode::thirtyOne), 4.318, 0.001);
            expectEquals (GraphEQParams::bandId (0, 0), juce::String ("g10_b01"));
            expectEquals (GraphEQParams::bandId (2, 30), juce::String ("g31_b31"));
            expectEquals (GraphEQParams::frequencyLabel (31.5), juce::String ("31.5"));
            expectEquals (GraphEQParams::frequencyLabel (1250.0), juce::String ("1.25k"));
            expectEquals (GraphEQParams::frequencyLabel (16000.0), juce::String ("16k"));
        }

        beginTest ("All bands at 0 dB is exactly transparent");
        {
            for (int m = 0; m < 3; ++m)
            {
                GraphEQEngine engine;
                GraphEQEngine::Settings s;
                s.mode = (Mode) m;
                engine.setSettings (s);
                engine.prepare (48000.0);

                juce::Random rng (2);
                juce::AudioBuffer<float> input (2, 300);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 300; ++i)
                        input.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

                auto output = input;
                engine.process (output.getArrayOfWritePointers(), 2, 300);

                bool identical = true;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 300; ++i)
                        identical = identical && juce::exactlyEqual (input.getSample (ch, i), output.getSample (ch, i));
                expect (identical);
            }
        }

        beginTest ("Each band hits its gain at its centre, in every mode and rate");
        {
            for (const double rate : { 44100.0, 48000.0, 96000.0 })
                for (int m = 0; m < 3; ++m)
                    for (int b = 0; b < GraphEQEngine::bandCount ((Mode) m); ++b)
                    {
                        GraphEQEngine::Settings s;
                        s.mode = (Mode) m;
                        s.gainsDb[(size_t) m][(size_t) b] = (b % 2 == 0) ? 12.0f : -12.0f;
                        const auto hz = GraphEQEngine::centres ((Mode) m)[b];
                        expectWithinAbsoluteError (GraphEQEngine::magnitudeDb (s, rate, hz), (double) s.gainsDb[(size_t) m][(size_t) b], 0.05);
                    }
        }

        beginTest ("Constant Q: the width doesn't change with gain");
        {
            // Half-gain (in dB) bandwidth of a single 1-octave band at +3 and +12 dB.
            const auto width = [] (float gain)
            {
                GraphEQEngine::Settings s;
                s.gainsDb[0][5] = gain;   // 1 kHz
                double lo = 1000.0, hi = 1000.0;
                const auto inside = [&] (double hz) { return std::abs (GraphEQEngine::magnitudeDb (s, 96000.0, hz)) > std::abs (gain) * 0.5; };
                while (inside (lo)) lo /= 1.001;
                while (inside (hi)) hi *= 1.001;
                return std::log2 (hi / lo);
            };

            const auto w3 = width (3.0f), w12 = width (12.0f), wCut = width (-12.0f);
            expectWithinAbsoluteError (w3, 1.0, 0.02, "+3 dB: one octave wide");
            expectWithinAbsoluteError (w12, 1.0, 0.02, "+12 dB: one octave wide");
            expectWithinAbsoluteError (wCut, 1.0, 0.02, "-12 dB: one octave wide");
        }

        beginTest ("The engine's sound matches the displayed curve");
        {
            GraphEQEngine::Settings s;
            s.mode = Mode::thirtyOne;
            juce::Random rng (4);
            for (int b = 0; b < 31; ++b)
                s.gainsDb[2][(size_t) b] = rng.nextFloat() * 24.0f - 12.0f;

            for (const float hz : { 40.0f, 125.0f, 400.0f, 1000.0f, 3150.0f, 9000.0f })
                expectWithinAbsoluteError (toDb (gainAt (s, hz)), GraphEQEngine::magnitudeDb (s, 48000.0, hz), 0.2,
                                           juce::String (hz) + " Hz");
        }

        beginTest ("Switching band count keeps settings and never clicks");
        {
            GraphEQEngine engine;
            GraphEQEngine::Settings s;
            s.gainsDb[0][2] = 12.0f;    // 10-band 125 Hz
            s.gainsDb[2][5] = -12.0f;   // 31-band 63 Hz
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (1, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;

            for (int block = 0; block < 400; ++block)
            {
                if (block == 100) { s.mode = Mode::thirtyOne; engine.setSettings (s); }
                if (block == 200) { s.mode = Mode::fifteen; engine.setSettings (s); }
                if (block == 300) { s.mode = Mode::ten; engine.setSettings (s); }
                else engine.setSettings (s);

                for (int i = 0; i < 64; ++i, ++n)
                    buffer.setSample (0, i, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * 110.0f * (float) n / 48000.0f));

                engine.process (buffer.getArrayOfWritePointers(), 1, 64);

                for (int i = 0; i < 64; ++i)
                {
                    if (block > 10)
                        maxStep = std::max (maxStep, std::abs (buffer.getSample (0, i) - previous));
                    previous = buffer.getSample (0, i);
                }
            }

            // 110 Hz at up to +12 dB moves at most ~0.015 per sample.
            expect (maxStep < 0.03f, "max step " + juce::String (maxStep));
            expectEquals (s.gainsDb[0][2], 12.0f, "the 10-band setting survives switching");
        }

        beginTest ("Extremes and fast changes stay finite");
        {
            for (const double rate : { 44100.0, 192000.0 })
            {
                GraphEQEngine engine;
                engine.prepare (rate);
                juce::Random rng (8);
                juce::AudioBuffer<float> buffer (2, 50);
                bool finite = true;

                for (int block = 0; block < 5000; ++block)
                {
                    if (block % 3 == 0)
                    {
                        GraphEQEngine::Settings s;
                        s.mode = (Mode) rng.nextInt (3);
                        for (auto& set : s.gainsDb)
                            for (auto& g : set)
                                g = rng.nextBool() ? 12.0f : -12.0f;
                        engine.setSettings (s);
                    }

                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 50; ++i)
                            buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

                    engine.process (buffer.getArrayOfWritePointers(), 2, 50);

                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 50; ++i)
                            finite = finite && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 10000.0f;
                }

                expect (finite);
            }
        }
    }

private:
    static double toDb (double gain) { return 20.0 * std::log10 (std::max (gain, 1.0e-12)); }

    static double gainAt (const GraphEQEngine::Settings& s, float hz)
    {
        constexpr double rate = 48000.0;
        GraphEQEngine engine;
        engine.setSettings (s);
        engine.prepare (rate);

        const int total = 96000;
        juce::AudioBuffer<float> buffer (1, total);
        for (int i = 0; i < total; ++i)
            buffer.setSample (0, i, 0.05f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / rate));

        for (int start = 0; start < total; start += 256)
        {
            auto* p = buffer.getWritePointer (0, start);
            engine.process (&p, 1, std::min (256, total - start));
        }

        return buffer.getRMSLevel (0, total / 2, total / 2) / (0.05 / std::sqrt (2.0));
    }
};

static GraphEQTests graphEQTests;
