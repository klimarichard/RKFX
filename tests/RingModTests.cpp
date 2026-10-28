#include <juce_audio_basics/juce_audio_basics.h>

#include "RingModEngine.h"

#include <cmath>

/** RKRingMod DSP: a sine carrier moves a tone to the sum and difference
    frequencies; the tracked carrier follows the played pitch at the set
    interval and holds it between notes; synced follows the tempo; the
    carrier glides, the LFO wobbles it, and no DC is left. */
class RingModTests : public juce::UnitTest
{
public:
    RingModTests() : juce::UnitTest ("RKRingMod engine") {}

    void runTest() override
    {
        using Mode = RingModEngine::Mode;

        beginTest ("Sum and difference, nothing of the original");
        {
            RingModEngine::Settings s;
            s.frequencyHz = 1000.0f;
            const auto out = run (s, sine (300.0, 0.5f, 48000));
            expectWithinAbsoluteError (amplitude (out, 700.0), 0.25, 0.02, "difference");
            expectWithinAbsoluteError (amplitude (out, 1300.0), 0.25, 0.02, "sum");
            expect (amplitude (out, 300.0) < 0.005, "the input is gone");
        }

        beginTest ("Tracked: the carrier follows the note at the interval");
        {
            RingModEngine::Settings s;
            s.mode = Mode::tracked;
            s.intervalSemitones = 7.0f;
            RingModEngine engine;
            auto in = sine (220.0, 0.3f, 48000);
            in.setSize (1, 72000, true, true);   // then silence
            const auto out = run (s, in, &engine);
            expectWithinAbsoluteError (engine.getCarrierHz(), 220.0f * std::exp2 (7.0f / 12.0f), 2.0f, "holds the fifth after the note");
            juce::ignoreUnused (out);
        }

        beginTest ("Synced: one cycle per division");
        {
            RingModEngine::Settings s;
            s.mode = Mode::synced;
            s.division = rkfx::SyncDivision::index (16);
            rkfx::Transport t;
            t.bpm = 120.0;
            RingModEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);
            auto buffer = sine (300.0, 0.5f, 4800);
            float* ch[] { buffer.getWritePointer (0) };
            engine.process (ch, 1, 4800, t);
            expectWithinAbsoluteError (engine.getCarrierHz(), 8.0f, 0.01f, "1/16 at 120 BPM");
        }

        beginTest ("The carrier glides; the LFO wobbles it");
        {
            RingModEngine::Settings s;
            s.frequencyHz = 200.0f;
            RingModEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);
            auto buffer = sine (300.0, 0.5f, 240);
            float* ch[] { buffer.getWritePointer (0) };
            s.frequencyHz = 800.0f;
            engine.setSettings (s);
            engine.process (ch, 1, 240, {});
            const auto hz = engine.getCarrierHz();
            expect (hz > 210.0f && hz < 790.0f, "on the way after 5 ms: " + juce::String (hz));

            s.lfoDepth = 1.0f;
            s.lfoRateHz = 2.0f;
            engine.setSettings (s);
            float lowest = 1.0e6f, highest = 0.0f;
            for (int block = 0; block < 100; ++block)
            {
                auto b = sine (300.0, 0.5f, 240);
                float* c[] { b.getWritePointer (0) };
                engine.process (c, 1, 240, {});
                if (block > 20)
                {
                    lowest = juce::jmin (lowest, engine.getModulatedHz());
                    highest = juce::jmax (highest, engine.getModulatedHz());
                }
            }
            expect (lowest < 450.0f && highest > 1400.0f, "an octave either way of 800 Hz: "
                                                           + juce::String (lowest) + " to " + juce::String (highest));
            expect (lowest > 390.0f && highest < 1610.0f, "but not more");
        }

        beginTest ("No DC when the carrier sits on the input's own pitch");
        {
            RingModEngine::Settings s;
            s.mode = Mode::tracked;
            s.intervalSemitones = 0.0f;
            const auto out = run (s, sine (220.0, 0.5f, 96000));
            double mean = 0.0;
            for (int i = 48000; i < 96000; ++i)
                mean += out.getSample (0, i);
            mean /= 48000.0;
            expect (std::abs (mean) < 0.01, "mean " + juce::String (mean));
            expect (out.getMagnitude (0, 0, 96000) < 1.0f, "bounded");
        }
    }

private:
    static juce::AudioBuffer<float> sine (double hz, float amplitude, int n)
    {
        juce::AudioBuffer<float> b (1, n);
        for (int i = 0; i < n; ++i)
            b.setSample (0, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / 48000.0));
        return b;
    }

    static juce::AudioBuffer<float> run (const RingModEngine::Settings& s, juce::AudioBuffer<float> buffer, RingModEngine* engine = nullptr)
    {
        RingModEngine local;
        auto& e = engine != nullptr ? *engine : local;
        e.setSettings (s);
        e.prepare (48000.0);
        for (int start = 0; start < buffer.getNumSamples(); start += 64)
        {
            float* ch[] { buffer.getWritePointer (0, start) };
            e.process (ch, 1, juce::jmin (64, buffer.getNumSamples() - start), {});
        }
        return buffer;
    }

    /** Amplitude at @p hz over the second half (Goertzel, Hann window). */
    static double amplitude (const juce::AudioBuffer<float>& b, double hz)
    {
        const auto n = b.getNumSamples() / 2;
        const auto* data = b.getReadPointer (0, n);
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / 48000.0;
        const auto coeff = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
            const auto s0 = data[i] * win + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return 2.0 * std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / (n * 0.5);
    }
};

static RingModTests ringModTests;
