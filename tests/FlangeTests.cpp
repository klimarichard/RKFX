#include <juce_audio_basics/juce_audio_basics.h>

#include "FlangeEngine.h"

#include <cmath>

/** RKFlange DSP: delay time, comb notches, feedback stability (both
    polarities), stereo phase, stability. */
class FlangeTests : public juce::UnitTest
{
public:
    FlangeTests() : juce::UnitTest ("RKFlange engine") {}

    void runTest() override
    {
        beginTest ("Depth 0, no feedback: the wet signal is the input delayed by Manual");
        {
            FlangeEngine::Settings s;
            s.depth = 0.0f;
            s.feedback = 0.0f;
            s.manualMs = 2.0f;

            FlangeEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (1, 1000);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            float* p = buffer.getWritePointer (0);
            engine.process (&p, 1, 1000, {});

            expectWithinAbsoluteError (buffer.getSample (0, 96), 1.0f, 1.0e-6f, "2 ms = 96 samples");
            expect (buffer.getMagnitude (0, 0, 96) < 1.0e-6f);
        }

        beginTest ("With the dry signal it's a comb filter: notches at odd multiples of 1/(2 x delay)");
        {
            // 1 ms: notches at 500, 1500 Hz; peaks at 1000, 2000 Hz.
            expect (combLevel (500.0f) < 0.02f, "notch at 500 Hz: " + juce::String (combLevel (500.0f)));
            expect (combLevel (1500.0f) < 0.02f);
            expectWithinAbsoluteError (combLevel (1000.0f), 1.0f, 0.02f);
            expectWithinAbsoluteError (combLevel (2000.0f), 1.0f, 0.02f);
        }

        beginTest ("Maximum feedback stays bounded, positive and negative");
        {
            for (const float fb : { 0.95f, -0.95f })
            {
                FlangeEngine::Settings s;
                s.feedback = fb;
                s.depth = 1.0f;
                s.rateHz = 5.0f;
                s.manualMs = 0.1f;

                FlangeEngine engine;
                engine.setSettings (s);
                engine.prepare (44100.0);

                juce::Random rng (2);
                juce::AudioBuffer<float> buffer (2, 64);
                float peak = 0.0f;
                bool finite = true;
                for (int block = 0; block < 44100 * 10 / 64; ++block)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 64; ++i)
                            buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 2.0f);
                    engine.process (buffer.getArrayOfWritePointers(), 2, 64, {});
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 64; ++i)
                        {
                            finite = finite && std::isfinite (buffer.getSample (ch, i));
                            peak = std::max (peak, std::abs (buffer.getSample (ch, i)));
                        }
                }
                expect (finite);
                expect (peak < 4.0f, juce::String (fb) + ": peak " + juce::String (peak));
            }
        }

        beginTest ("Stereo phase makes the sides sweep differently");
        {
            for (const float phase : { 0.0f, 90.0f })
            {
                FlangeEngine::Settings s;
                s.phaseDeg = phase;
                FlangeEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                juce::Random rng (8);
                juce::AudioBuffer<float> buffer (2, 48000);
                for (int i = 0; i < 48000; ++i)
                {
                    const auto x = rng.nextFloat() * 2.0f - 1.0f;
                    buffer.setSample (0, i, x);
                    buffer.setSample (1, i, x);
                }
                engine.process (buffer.getArrayOfWritePointers(), 2, 48000, {});

                float diff = 0.0f;
                for (int i = 0; i < 48000; ++i)
                    diff = std::max (diff, std::abs (buffer.getSample (0, i) - buffer.getSample (1, i)));

                if (phase > 0.0f)
                    expect (diff > 0.1f);
                else
                    expect (diff < 1.0e-6f);
            }
        }

        beginTest ("Extremes stay finite");
        {
            FlangeEngine engine;
            engine.prepare (192000.0);
            juce::Random rng (19);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;

            for (int block = 0; block < 5000; ++block)
            {
                if (block % 5 == 0)
                {
                    FlangeEngine::Settings s;
                    s.rateHz = rng.nextBool() ? 0.02f : 10.0f;
                    s.depth = rng.nextFloat();
                    s.manualMs = rng.nextBool() ? 0.1f : 10.0f;
                    s.feedback = rng.nextBool() ? 0.95f : -0.95f;
                    s.sync = rng.nextBool();
                    s.division = rng.nextInt (rkfx::SyncDivision::kCount);
                    s.phaseDeg = rng.nextFloat() * 180.0f;
                    engine.setSettings (s);
                }

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

                engine.process (buffer.getArrayOfWritePointers(), 2, 64, {});

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 4.0f;
            }
            expect (ok);
        }
    }

private:
    /** Steady level of 0.5 x dry + 0.5 x wet for a sine (depth 0, no feedback, 1 ms). */
    static float combLevel (float hz)
    {
        FlangeEngine::Settings s;
        s.depth = 0.0f;
        s.feedback = 0.0f;
        s.manualMs = 1.0f;
        FlangeEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);

        juce::AudioBuffer<float> buffer (1, 9600);
        for (int i = 0; i < 9600; ++i)
            buffer.setSample (0, i, std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f));
        auto dry = buffer;
        float* p = buffer.getWritePointer (0);
        engine.process (&p, 1, 9600, {});

        float peak = 0.0f;
        for (int i = 4800; i < 9600; ++i)
            peak = std::max (peak, std::abs (0.5f * (dry.getSample (0, i) + buffer.getSample (0, i))));
        return peak;
    }
};

static FlangeTests flangeTests;
