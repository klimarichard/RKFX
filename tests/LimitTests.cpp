#include <juce_audio_basics/juce_audio_basics.h>

#include "LimitEngine.h"

#include <cmath>

/** RKLimit DSP: the ceiling is never exceeded, quiet signals pass untouched,
    limiting level and release, stereo link, stability. */
class LimitTests : public juce::UnitTest
{
public:
    LimitTests() : juce::UnitTest ("RKLimit engine") {}

    void runTest() override
    {
        beginTest ("The clipper is linear below the knee and never reaches past the ceiling");
        {
            expectEquals (LimitEngine::clip (0.5f, 0.9f, 1.0f), 0.5f);
            expectEquals (LimitEngine::clip (-0.9f, 0.9f, 1.0f), -0.9f);
            for (const float x : { 0.95f, 1.0f, 2.0f, 100.0f })
            {
                const auto y = LimitEngine::clip (x, 0.9f, 1.0f);
                expect (y > 0.9f && y <= 1.0f);
                expectEquals (LimitEngine::clip (-x, 0.9f, 1.0f), -y);
            }
        }

        beginTest ("Nothing ever exceeds the ceiling, in any setting");
        {
            for (const int factor : { 1, 2, 4 })
                for (const float ceilingDb : { 0.0f, -1.0f, -12.0f })
                {
                    LimitEngine::Settings s;
                    s.inputDb = 24.0f;
                    s.ceilingDb = ceilingDb;
                    s.releaseMs = 1.0f;
                    s.link = 0.0f;
                    s.oversampling = factor;
                    LimitEngine engine;
                    engine.setSettings (s);
                    engine.prepare (44100.0);

                    juce::Random rng (factor * 7 + (int) ceilingDb);
                    juce::AudioBuffer<float> b (2, 64);
                    float peak = 0.0f;
                    for (int block = 0; block < 4000; ++block)
                    {
                        for (int ch = 0; ch < 2; ++ch)
                            for (int i = 0; i < 64; ++i)
                                b.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * (block % 3 == 0 ? 4.0f : 0.5f));
                        engine.process (b.getArrayOfWritePointers(), 2, 64);
                        peak = std::max (peak, b.getMagnitude (0, 64));
                    }
                    expect (peak <= juce::Decibels::decibelsToGain (ceilingDb) + 1.0e-6f,
                            juce::String (factor) + "x, ceiling " + juce::String (ceilingDb) + ": peak " + juce::String (peak));
                }
        }

        beginTest ("Below the threshold the signal passes untouched");
        {
            LimitEngine engine;
            engine.setSettings ({});
            engine.prepare (48000.0);

            juce::Random rng (2);
            juce::AudioBuffer<float> b (2, 512);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    b.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.5f);
            const auto input = b;
            engine.process (b.getArrayOfWritePointers(), 2, 512);

            bool identical = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    identical = identical && juce::exactlyEqual (input.getSample (ch, i), b.getSample (ch, i));
            expect (identical);
        }

        beginTest ("Loud input is held at the ceiling by the limiter; release recovers");
        {
            LimitEngine::Settings s;
            s.ceilingDb = -1.0f;
            s.releaseMs = 50.0f;
            LimitEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            // 0.5 s of a sine 6 dB over the ceiling: peaks sit just under it.
            juce::AudioBuffer<float> loud (1, 24000);
            for (int i = 0; i < 24000; ++i)
                loud.setSample (0, i, 1.78f * std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f * (float) i / 48000.0f));
            float* p = loud.getWritePointer (0);
            engine.process (&p, 1, 24000);
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (loud.getMagnitude (0, 12000, 12000)), -1.3f, 0.4f);
            expect (engine.takeGainReductionDb() < -5.0f);

            // Then a quiet signal: gain comes back up within a few release times.
            juce::AudioBuffer<float> quiet (1, 24000);
            for (int i = 0; i < 24000; ++i)
                quiet.setSample (0, i, 0.1f * std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f * (float) i / 48000.0f));
            p = quiet.getWritePointer (0);
            engine.process (&p, 1, 24000);
            expectWithinAbsoluteError (quiet.getMagnitude (0, 19200, 4800), 0.1f, 0.002f, "recovered after 400 ms");
        }

        beginTest ("Stereo link");
        {
            for (const float linkAmount : { 0.0f, 1.0f })
            {
                LimitEngine::Settings s;
                s.link = linkAmount;
                LimitEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                juce::AudioBuffer<float> b (2, 24000);
                for (int i = 0; i < 24000; ++i)
                {
                    const auto x = std::sin (2.0f * juce::MathConstants<float>::pi * 500.0f * (float) i / 48000.0f);
                    b.setSample (0, i, 2.0f * x);
                    b.setSample (1, i, 0.2f * x);
                }
                engine.process (b.getArrayOfWritePointers(), 2, 24000);
                const auto right = b.getMagnitude (1, 12000, 12000);
                if (linkAmount > 0.5f)
                    expect (right < 0.15f, "linked: the quiet side is turned down too");
                else
                    expectWithinAbsoluteError (right, 0.2f, 0.002f, "unlinked: quiet side untouched");
            }
        }
    }
};

static LimitTests limitTests;
