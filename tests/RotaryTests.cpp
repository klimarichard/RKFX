#include <juce_audio_basics/juce_audio_basics.h>

#include "RotaryEngine.h"

#include <cmath>

/** RKRotary DSP: rotor speeds and acceleration (drum lags the horn), brake,
    rotating sound in stereo, constant latency with or without drive, stability. */
class RotaryTests : public juce::UnitTest
{
public:
    RotaryTests() : juce::UnitTest ("RKRotary engine") {}

    void runTest() override
    {
        using Speed = RotaryEngine::Speed;

        beginTest ("Slow to fast: each rotor follows its own acceleration; the drum lags");
        {
            RotaryEngine::Settings s;
            RotaryEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);
            expectWithinAbsoluteError (engine.getHornHz(), 0.8f, 1.0e-4f);

            s.speed = Speed::fast;
            engine.setSettings (s);
            run (engine, (int) (0.8 * 48000));   // one horn time constant

            const auto hornShare = (engine.getHornHz() - 0.8f) / (6.5f - 0.8f);
            expectWithinAbsoluteError (hornShare, 0.632f, 0.02f, "horn 63% of the way after its acceleration time");
            const auto drumShare = (engine.getDrumHz() - 0.8f * 0.85f) / ((6.5f - 0.8f) * 0.85f);
            expect (drumShare < hornShare * 0.4f, "the heavier drum lags: " + juce::String (drumShare));

            run (engine, 48000 * 30);
            expectWithinAbsoluteError (engine.getHornHz(), 6.5f, 0.01f);
            expectWithinAbsoluteError (engine.getDrumHz(), 6.5f * 0.85f, 0.01f);
        }

        beginTest ("Brake slows both rotors to a stop");
        {
            RotaryEngine::Settings s;
            s.speed = Speed::fast;
            RotaryEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);
            s.speed = Speed::brake;
            engine.setSettings (s);
            run (engine, 48000 * 40);
            expect (engine.getHornHz() < 0.01f && engine.getDrumHz() < 0.01f);
        }

        beginTest ("The sound rotates: level moves, and left and right differ");
        {
            for (const float w : { 1.0f, 0.0f })
            {
                RotaryEngine::Settings s;
                s.speed = Speed::fast;
                s.width = w;
                s.distance = 0.0f;
                RotaryEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                juce::AudioBuffer<float> b (2, 48000);
                for (int i = 0; i < 48000; ++i)
                {
                    const auto x = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 3000.0f * (float) i / 48000.0f);
                    b.setSample (0, i, x);
                    b.setSample (1, i, x);
                }
                engine.process (b.getArrayOfWritePointers(), 2, 48000);

                // Level in 10 ms windows over the second half.
                float lo = 1.0f, hi = 0.0f, diff = 0.0f;
                for (int start = 24000; start + 480 <= 48000; start += 480)
                {
                    const auto level = b.getRMSLevel (0, start, 480);
                    lo = std::min (lo, level);
                    hi = std::max (hi, level);
                }
                for (int i = 24000; i < 48000; ++i)
                    diff = std::max (diff, std::abs (b.getSample (0, i) - b.getSample (1, i)));

                expect (hi > lo * 1.5f, "amplitude modulation: " + juce::String (lo) + " .. " + juce::String (hi));
                if (w > 0.5f)
                    expect (diff > 0.05f, "stereo");
                else
                    expect (diff < 1.0e-6f, "width 0: mono");
            }
        }

        beginTest ("Latency follows the oversampling setting, with or without drive");
        {
            for (const float driveAmount : { 0.0f, 0.5f })
            {
                RotaryEngine::Settings s;
                s.drive = driveAmount;
                s.oversampling = 2;
                RotaryEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);
                expectEquals (engine.latencySamples(), 4);

                // A step arrives at the same time either way (rotor delays aside).
                juce::AudioBuffer<float> b (1, 4800);
                b.clear();
                for (int i = 100; i < 4800; ++i)
                    b.setSample (0, i, 0.1f);
                engine.process (b.getArrayOfWritePointers(), 1, 4800);
                int first = 0;
                while (first < 4800 && std::abs (b.getSample (0, first)) < 1.0e-4f)
                    ++first;
                expect (first > 100 + 4 + 40 && first < 100 + 4 + 120,
                        "drive " + juce::String (driveAmount) + ": first output at " + juce::String (first));
            }
        }

        beginTest ("Extremes stay finite and bounded");
        {
            RotaryEngine engine;
            engine.prepare (192000.0);
            juce::Random rng (47);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;
            for (int block = 0; block < 6000; ++block)
            {
                if (block % 5 == 0)
                {
                    RotaryEngine::Settings s;
                    s.speed = (Speed) rng.nextInt (3);
                    s.slowHz = rng.nextBool() ? 0.3f : 1.5f;
                    s.fastHz = rng.nextBool() ? 4.0f : 8.0f;
                    s.hornAccelSeconds = rng.nextBool() ? 0.2f : 3.0f;
                    s.drumAccelSeconds = rng.nextBool() ? 1.0f : 8.0f;
                    s.balance = rng.nextFloat() * 2.0f - 1.0f;
                    s.distance = rng.nextFloat();
                    s.width = rng.nextFloat();
                    s.drive = rng.nextBool() ? 0.0f : 1.0f;
                    s.oversampling = 1 << rng.nextInt (3);
                    engine.setSettings (s);
                }
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 2.0f);
                engine.process (buffer.getArrayOfWritePointers(), 2, 64);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 8.0f;
            }
            expect (ok);
        }
    }

private:
    static void run (RotaryEngine& engine, int samples)
    {
        juce::AudioBuffer<float> b (2, 512);
        for (int done = 0; done < samples; done += 512)
        {
            b.clear();
            engine.process (b.getArrayOfWritePointers(), 2, juce::jmin (512, samples - done));
        }
    }
};

static RotaryTests rotaryTests;
