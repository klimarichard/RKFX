#include <juce_audio_basics/juce_audio_basics.h>

#include "FreqShiftEngine.h"

#include <cmath>

/** RKFreqShift DSP: shift direction and sideband rejection, feedback
    stability, parameter extremes and stereo spread. */
class FreqShiftTests : public juce::UnitTest
{
public:
    FreqShiftTests() : juce::UnitTest ("RKFreqShift engine") {}

    void runTest() override
    {
        beginTest ("Shifts up and down with the unwanted sideband below -40 dB");
        {
            for (const double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            {
                for (const float input : { 100.0f, 1000.0f, 8000.0f })
                {
                    for (const bool down : { false, true })
                    {
                        const float shift = 250.0f;
                        const auto r = measure (rate, input, shift, down);
                        const auto wanted   = down ? r.below : r.above;
                        const auto unwanted = down ? r.above : r.below;
                        const auto rejection = juce::Decibels::gainToDecibels (unwanted / wanted);

                        expect (std::abs (wanted - 0.5f) < 0.03f,
                                juce::String (rate) + " Hz, " + juce::String (input) + " Hz in: level " + juce::String (wanted));
                        expect (rejection < -40.0f,
                                juce::String (rate) + " Hz, " + juce::String (input) + " Hz in, "
                                    + (down ? "down" : "up") + ": sideband " + juce::String (rejection, 1) + " dB");
                    }
                }
            }
        }

        beginTest ("Up/down at 50% gives both sidebands equally");
        {
            const auto r = measure (48000.0, 1000.0f, 100.0f, false, 0.5f);
            expectWithinAbsoluteError (r.above, r.below, 0.02f);
        }

        beginTest ("Maximum feedback stays bounded");
        {
            for (const double rate : { 44100.0, 192000.0 })
            {
                FreqShiftEngine engine;
                const float freqs[] = { 0.0f, 3.0f, 200.0f, 5000.0f };
                for (int k = 0; k < 4; ++k)
                    engine.setShift (k, { true, freqs[k], k % 2 == 0 ? 0.0f : 1.0f, 0.9f, 1.0f });
                engine.prepare (rate);

                juce::Random rng (7);
                juce::AudioBuffer<float> buffer (2, 64);
                float peak = 0.0f;
                bool finite = true;

                for (int block = 0; block < (int) (rate * 10.0) / 64; ++block)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 64; ++i)
                            buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 4.0f);

                    engine.process (buffer.getArrayOfWritePointers(), 2, 64);

                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 64; ++i)
                        {
                            const auto v = buffer.getSample (ch, i);
                            finite = finite && std::isfinite (v);
                            peak = std::max (peak, std::abs (v));
                        }
                }

                expect (finite);
                expect (peak < 64.0f, "peak " + juce::String (peak));
            }
        }

        beginTest ("Parameter extremes and fast changes never produce NaN or infinity");
        {
            FreqShiftEngine engine;
            engine.prepare (44100.0);

            juce::Random rng (11);
            juce::AudioBuffer<float> buffer (2, 37);   // odd block size on purpose
            bool finite = true;

            for (int block = 0; block < 20000; ++block)
            {
                if (block % 3 == 0)
                {
                    for (int k = 0; k < 4; ++k)
                        engine.setShift (k, { rng.nextBool(), rng.nextBool() ? 0.0f : 5000.0f * rng.nextFloat(),
                                              rng.nextFloat(), rng.nextBool() ? 0.9f : 0.0f, rng.nextFloat() });
                    engine.setSpread (rng.nextFloat());
                    engine.setLowCut (rng.nextBool() ? 20.0f : 1000.0f);
                    engine.setHighCut (rng.nextBool() ? 1000.0f : 20000.0f);
                }

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 37; ++i)
                        buffer.setSample (ch, i, rng.nextBool() ? 1.0f : -1.0f);

                engine.process (buffer.getArrayOfWritePointers(), 2, 37);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 37; ++i)
                        finite = finite && std::isfinite (buffer.getSample (ch, i));
            }

            expect (finite);
        }

        beginTest ("Stereo spread separates the channels; 0% keeps them identical");
        {
            for (const float spreadAmount : { 0.0f, 1.0f })
            {
                FreqShiftEngine engine;
                engine.setShift (0, { true, 100.0f, 0.0f, 0.0f, 1.0f });
                engine.setSpread (spreadAmount);
                engine.prepare (48000.0);

                juce::AudioBuffer<float> buffer (2, 4800);
                for (int i = 0; i < 4800; ++i)
                {
                    const auto x = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / 48000.0f);
                    buffer.setSample (0, i, x);
                    buffer.setSample (1, i, x);
                }
                engine.process (buffer.getArrayOfWritePointers(), 2, 4800);

                float maxDiff = 0.0f;
                for (int i = 0; i < 4800; ++i)
                    maxDiff = std::max (maxDiff, std::abs (buffer.getSample (0, i) - buffer.getSample (1, i)));

                if (spreadAmount > 0.0f)
                    expect (maxDiff > 0.1f);
                else
                    expectEquals (maxDiff, 0.0f);
            }
        }

        beginTest ("All shifts off gives silence");
        {
            FreqShiftEngine engine;
            engine.setShift (0, { false, 100.0f, 0.0f, 0.0f, 1.0f });
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (1, 512);
            for (int i = 0; i < 512; ++i)
                buffer.setSample (0, i, 0.5f);
            engine.process (buffer.getArrayOfWritePointers(), 1, 512);
            expectEquals (buffer.getMagnitude (0, 512), 0.0f);
        }
    }

private:
    struct Sidebands { float above = 0.0f, below = 0.0f; };

    /** Amplitude of a sine at @p hz in @p data (Goertzel, whole-cycle window). */
    static float amplitude (const float* data, int n, double hz, double rate)
    {
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / rate;
        const auto coeff = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            // Hann window to keep leakage from the wanted tone out of the other bin.
            const auto win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
            const auto s0 = data[i] * win + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const auto power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        return (float) (2.0 * std::sqrt (std::max (0.0, power)) / (n * 0.5));
    }

    static Sidebands measure (double rate, float inputHz, float shiftHz, bool down, float updown = -1.0f)
    {
        FreqShiftEngine engine;
        engine.setShift (0, { true, shiftHz, updown >= 0.0f ? updown : (down ? 1.0f : 0.0f), 0.0f, 1.0f });
        engine.prepare (rate);

        const auto total = (int) rate;          // 1 s: settle, then analyse the second half
        juce::AudioBuffer<float> buffer (1, total);
        for (int i = 0; i < total; ++i)
            buffer.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * inputHz * i / rate));

        for (int start = 0; start < total; start += 256)
        {
            auto* p = buffer.getWritePointer (0, start);
            engine.process (&p, 1, std::min (256, total - start));
        }

        const auto* tail = buffer.getReadPointer (0, total / 2);
        return { amplitude (tail, total / 2, inputHz + shiftHz, rate),
                 amplitude (tail, total / 2, std::abs (inputHz - shiftHz), rate) };
    }
};

static FreqShiftTests freqShiftTests;
