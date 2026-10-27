#include <juce_audio_basics/juce_audio_basics.h>

#include "DoubleEngine.h"
#include "dsp/RandomModulator.h"

#include <cmath>

/** RKDouble DSP and the shared random modulator: smooth bounded drift, the
    double's delay and level, never leading, spread placement, stability. */
class DoubleTests : public juce::UnitTest
{
public:
    DoubleTests() : juce::UnitTest ("RKDouble engine") {}

    void runTest() override
    {
        beginTest ("Random modulator: bounded, smooth, not periodic");
        {
            rkfx::RandomModulator m;
            m.prepare (48000.0);
            m.setRate (2.0f);

            float lo = 1.0f, hi = -1.0f, maxStep = 0.0f, previous = 0.0f;
            std::vector<float> values;
            for (int i = 0; i < 48000 * 20; ++i)
            {
                const auto v = m.next();
                lo = std::min (lo, v);
                hi = std::max (hi, v);
                if (i > 0)
                    maxStep = std::max (maxStep, std::abs (v - previous));
                previous = v;
                if (i % 480 == 0)
                    values.push_back (v);
            }

            expect (lo >= -1.0f && hi <= 1.0f);
            expect (hi - lo > 1.0f, "uses its range");
            // Smooth: never jumps more than the steepest smoothstep allows (1.5 x 2 x rate / fs, with jitter).
            expect (maxStep < 0.0003f, "max step " + juce::String (maxStep));

            // Not periodic at the nominal rate: values half a second apart don't repeat.
            int matches = 0;
            for (size_t i = 0; i + 50 < values.size(); ++i)
                if (std::abs (values[i] - values[i + 50]) < 0.01f)
                    ++matches;
            expect (matches < (int) values.size() / 10);
        }

        beginTest ("Without drift: the double is the input delayed, at its level");
        {
            DoubleEngine::Settings s;
            s.drift = 0.0f;
            s.tone = 0.0f;
            s.spread = 0.0f;
            s.levelDb = -6.0f;
            s.delayMs = 25.0f;

            DoubleEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (1, 4800);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            float* p = buffer.getWritePointer (0);
            engine.process (&p, 1, 4800);

            expectEquals (buffer.getSample (0, 0), 1.0f, "dry untouched");
            expectWithinAbsoluteError (buffer.getSample (0, 1200), juce::Decibels::decibelsToGain (-6.0f), 1.0e-5f, "double at 25 ms, -6 dB");
            expectEquals (buffer.getMagnitude (0, 1, 1198), 0.0f, "nothing in between");
        }

        beginTest ("With full drift the double still always lags");
        {
            DoubleEngine::Settings s;
            s.drift = 1.0f;
            s.driftHz = 2.0f;
            s.delayMs = 10.0f;   // shortest delay, widest drift: 5 to 15 ms
            s.spread = 0.0f;

            DoubleEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            // Impulses every 100 ms: each echo lands 5 to 15 ms after its impulse.
            juce::AudioBuffer<float> buffer (1, 48000 * 4);
            buffer.clear();
            for (int i = 0; i < buffer.getNumSamples(); i += 4800)
                buffer.setSample (0, i, 1.0f);
            float* p = buffer.getWritePointer (0);
            engine.process (&p, 1, buffer.getNumSamples());

            bool lagging = true;
            for (int start = 4800; start + 4800 <= buffer.getNumSamples(); start += 4800)
            {
                expect (buffer.getMagnitude (0, start - 230, 230) < 1.0e-6f);   // nothing just before the impulse (tone filter tail only)
                lagging = lagging && buffer.getMagnitude (0, start + 1, 230) < 1.0e-6f    // nothing < ~4.8 ms after
                                  && buffer.getMagnitude (0, start + 240, 480) > 0.1f;  // the echo is 5..15 ms after
            }
            expect (lagging);
        }

        beginTest ("Spread 100%: dry left, double right");
        {
            DoubleEngine::Settings s;
            s.spread = 1.0f;
            s.drift = 0.0f;
            s.tone = 0.0f;

            DoubleEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (2, 4800);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            buffer.setSample (1, 0, 1.0f);
            engine.process (buffer.getArrayOfWritePointers(), 2, 4800);

            expectEquals (buffer.getSample (0, 0), 1.0f, "dry on the left");
            expectEquals (buffer.getSample (1, 0), 0.0f, "no dry on the right");
            expect (buffer.getMagnitude (0, 1, 4799) < 1.0e-6f, "no double on the left");
            expect (buffer.getMagnitude (1, 1, 4799) > 0.5f, "double on the right");
        }

        beginTest ("Extremes stay finite and bounded");
        {
            DoubleEngine engine;
            engine.prepare (192000.0);
            juce::Random rng (17);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;

            for (int block = 0; block < 5000; ++block)
            {
                if (block % 5 == 0)
                {
                    DoubleEngine::Settings s;
                    s.delayMs = rng.nextBool() ? 10.0f : 60.0f;
                    s.drift = rng.nextFloat();
                    s.driftHz = rng.nextBool() ? 0.1f : 2.0f;
                    s.tone = rng.nextFloat() * 2.0f - 1.0f;
                    s.spread = rng.nextFloat();
                    s.levelDb = rng.nextBool() ? -24.0f : 0.0f;
                    engine.setSettings (s);
                }

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

                engine.process (buffer.getArrayOfWritePointers(), 2, 64);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 5.0f;
            }
            expect (ok);
        }
    }
};

static DoubleTests doubleTests;
