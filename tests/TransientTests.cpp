#include <juce_audio_basics/juce_audio_basics.h>

#include "TransientEngine.h"

#include <cmath>

/** RKTransient DSP: transparency at 0, attack and sustain boost/cut, level
    independence, steady tones untouched, listen, stability. */
class TransientTests : public juce::UnitTest
{
public:
    TransientTests() : juce::UnitTest ("RKTransient engine") {}

    void runTest() override
    {
        beginTest ("At 0% attack and sustain the signal passes exactly");
        {
            TransientEngine engine;
            engine.setSettings ({});
            engine.prepare (48000.0);
            auto b = hits (1, 0.5f);
            const auto input = b;
            engine.process (b.getArrayOfWritePointers(), 1, b.getNumSamples());
            bool identical = true;
            for (int i = 0; i < b.getNumSamples(); ++i)
                identical = identical && juce::exactlyEqual (input.getSample (0, i), b.getSample (0, i));
            expect (identical);
        }

        beginTest ("Attack boosts or softens the start of each hit");
        {
            const auto attackRatio = [] (float amount)
            {
                TransientEngine::Settings s;
                s.attack = amount;
                return ratios (s, 0.5f).first;
            };
            logMessage ("attack +100%: x" + juce::String (attackRatio (1.0f), 2) + ", -100%: x" + juce::String (attackRatio (-1.0f), 2));
            expect (attackRatio (1.0f) > 2.0f, "boost: " + juce::String (attackRatio (1.0f)));
            expect (attackRatio (-1.0f) < 0.6f, "cut: " + juce::String (attackRatio (-1.0f)));
        }

        beginTest ("Sustain lengthens or shortens the tail");
        {
            const auto tailRatio = [] (float amount)
            {
                TransientEngine::Settings s;
                s.sustain = amount;
                return ratios (s, 0.5f).second;
            };
            logMessage ("sustain +100%: x" + juce::String (tailRatio (1.0f), 2) + ", -100%: x" + juce::String (tailRatio (-1.0f), 2));
            expect (tailRatio (1.0f) > 2.0f, "boost: " + juce::String (tailRatio (1.0f)));
            expect (tailRatio (-1.0f) < 0.7f, "cut: " + juce::String (tailRatio (-1.0f)));
        }

        beginTest ("The effect is the same at any level (no threshold)");
        {
            TransientEngine::Settings s;
            s.attack = 0.7f;
            s.sustain = -0.5f;
            const auto loud = ratios (s, 0.5f), quiet = ratios (s, 0.005f);   // 40 dB apart
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (quiet.first / loud.first), 0.0f, 0.3f, "attack");
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (quiet.second / loud.second), 0.0f, 0.3f, "sustain");
        }

        beginTest ("A steady tone is left alone once it settles");
        {
            TransientEngine::Settings s;
            s.attack = 1.0f;
            s.sustain = 1.0f;
            TransientEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);
            juce::AudioBuffer<float> b (1, 48000);
            for (int i = 0; i < 48000; ++i)
                b.setSample (0, i, 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) i / 48000.0f));
            engine.process (b.getArrayOfWritePointers(), 1, 48000);
            const auto steady = b.getRMSLevel (0, 36000, 12000) / (0.3f / std::sqrt (2.0f));
            logMessage ("steady 220 Hz at +100/+100%: " + juce::String (juce::Decibels::gainToDecibels (steady), 2) + " dB");
            // A little envelope ripple remains on low steady tones (about 0.5 dB at full boost).
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (steady), 0.0f, 1.0f);
        }

        beginTest ("Extremes stay finite and bounded");
        {
            TransientEngine engine;
            engine.prepare (192000.0);
            juce::Random rng (41);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;
            for (int block = 0; block < 6000; ++block)
            {
                if (block % 5 == 0)
                {
                    TransientEngine::Settings s;
                    s.attack = rng.nextBool() ? 1.0f : -1.0f;
                    s.sustain = rng.nextBool() ? 1.0f : -1.0f;
                    s.attackTimeMs = rng.nextBool() ? 1.0f : 50.0f;
                    s.sustainTimeMs = rng.nextBool() ? 20.0f : 500.0f;
                    s.focusHighPassHz = rng.nextBool() ? 0.0f : 5000.0f;
                    s.focusLowPassHz = rng.nextBool() ? 0.0f : 100.0f;
                    s.listen = rng.nextBool();
                    s.link = rng.nextBool();
                    engine.setSettings (s);
                }
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * (rng.nextInt (50) == 0 ? 2.0f : 0.01f));
                engine.process (buffer.getArrayOfWritePointers(), 2, 64);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 40.0f;
            }
            expect (ok);
        }
    }

private:
    /** Drum-like hits every 250 ms: a noise burst decaying over ~150 ms. */
    static juce::AudioBuffer<float> hits (int channels, float level)
    {
        juce::Random rng (5);
        juce::AudioBuffer<float> b (channels, 48000);
        for (int i = 0; i < 48000; ++i)
        {
            const auto t = (float) (i % 12000) / 48000.0f;
            const auto x = level * std::exp (-t / 0.04f) * (rng.nextFloat() * 2.0f - 1.0f);
            for (int ch = 0; ch < channels; ++ch)
                b.setSample (ch, i, x);
        }
        return b;
    }

    /** Output/input level ratios in the attack (first 3 ms of a hit) and the tail (60 to 150 ms). */
    static std::pair<float, float> ratios (const TransientEngine::Settings& s, float level)
    {
        TransientEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        auto b = hits (1, level);
        const auto input = b;
        engine.process (b.getArrayOfWritePointers(), 1, b.getNumSamples());

        const int hit = 36000;   // the 4th hit, well settled
        const auto attackRatio = b.getRMSLevel (0, hit, 144) / input.getRMSLevel (0, hit, 144);
        const auto tailRatio = b.getRMSLevel (0, hit + 2880, 4320) / input.getRMSLevel (0, hit + 2880, 4320);
        return { attackRatio, tailRatio };
    }
};

static TransientTests transientTests;
