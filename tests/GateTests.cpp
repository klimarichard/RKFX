#include <juce_audio_basics/juce_audio_basics.h>

#include "GateEngine.h"
#include "dsp/Dynamics.h"

#include <cmath>

/** RKGate DSP: open/closed, range, hysteresis, hold, expander curve,
    external key and key filters, stereo link, stability. */
class GateTests : public juce::UnitTest
{
public:
    GateTests() : juce::UnitTest ("RKGate engine") {}

    void runTest() override
    {
        using Mode = GateEngine::Mode;

        beginTest ("Gain computer: gate with hysteresis, expander with range");
        {
            GateEngine::Settings s;
            s.thresholdDb = -40.0f;
            s.hysteresisDb = 6.0f;
            s.rangeDb = -30.0f;

            bool open = false;
            expectEquals (GateEngine::targetGainDb (s, -50.0f, open), -30.0f);
            expect (! open);
            expectEquals (GateEngine::targetGainDb (s, -38.0f, open), 0.0f);
            expect (open);
            expectEquals (GateEngine::targetGainDb (s, -44.0f, open), 0.0f, "inside the hysteresis: stays open");
            expectEquals (GateEngine::targetGainDb (s, -47.0f, open), -30.0f, "below threshold - hysteresis: closes");
            expectEquals (GateEngine::targetGainDb (s, -44.0f, open), -30.0f, "inside the hysteresis: stays closed");

            s.mode = Mode::expander;
            s.ratio = 3.0f;
            expectEquals (GateEngine::targetGainDb (s, -45.0f, open), -10.0f, "5 dB below at 1:3 = 10 dB of reduction");
            expectEquals (GateEngine::targetGainDb (s, -70.0f, open), -30.0f, "limited by the range");
            expectEquals (GateEngine::targetGainDb (s, -20.0f, open), 0.0f);
        }

        beginTest ("Open passes the signal; fully closed is silence");
        {
            GateEngine::Settings s;   // threshold -50 dB, fully closed, hold 50 ms, release 100 ms
            GateEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            auto loud = tone (0.5f, 9600);
            engine.process (loud.getArrayOfWritePointers(), 1, 9600, nullptr, 0);
            expectWithinAbsoluteError (loud.getMagnitude (0, 4800, 4800), 0.5f, 0.001f, "open");

            auto quiet = tone (0.0005f, 96000);   // -66 dB, 2 s
            engine.process (quiet.getArrayOfWritePointers(), 1, 96000, nullptr, 0);
            expectEquals (quiet.getMagnitude (0, 48000, 48000), 0.0f, "fully closed = exact silence");
        }

        beginTest ("Range limits the reduction");
        {
            GateEngine::Settings s;
            s.rangeDb = -20.0f;
            GateEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            auto quiet = tone (0.001f, 96000);   // -60 dB, below the -50 dB threshold
            engine.process (quiet.getArrayOfWritePointers(), 1, 96000, nullptr, 0);
            expectWithinAbsoluteError (rkfx::Dynamics::toDb (quiet.getMagnitude (0, 48000, 48000) / 0.001f), -20.0f, 0.2f);
        }

        beginTest ("Hold keeps the gate open after the level drops");
        {
            for (const float holdMs : { 0.0f, 200.0f })
            {
                GateEngine::Settings s;
                s.holdMs = holdMs;
                s.releaseMs = 5.0f;
                GateEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                auto loud = tone (0.5f, 4800);
                engine.process (loud.getArrayOfWritePointers(), 1, 4800, nullptr, 0);

                // The detector needs about 50 ms to see a 47 dB drop; look at 150..200 ms.
                auto quiet = tone (0.0005f, 9600);
                engine.process (quiet.getArrayOfWritePointers(), 1, 9600, nullptr, 0);
                const auto level = quiet.getMagnitude (0, 7200, 2400) / 0.0005f;

                if (holdMs > 0.0f)
                    expectWithinAbsoluteError (level, 1.0f, 0.01f, "still held open");
                else
                    expect (level < 0.01f, "closed without hold: " + juce::String (level));
            }
        }

        beginTest ("External key and key filters decide when the gate opens");
        {
            GateEngine::Settings s;
            s.external = true;
            GateEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            auto main = tone (0.0005f, 9600);      // would keep the gate closed on its own
            auto key = tone (0.5f, 9600);
            engine.process (main.getArrayOfWritePointers(), 1, 9600, key.getArrayOfReadPointers(), 1);
            expectWithinAbsoluteError (main.getMagnitude (0, 4800, 4800), 0.0005f, 0.00002f, "opened by the key input");

            // A low-pass on the key ignores a loud 8 kHz hi-hat-like trigger.
            GateEngine::Settings filtered;
            filtered.keyLowPassHz = 500.0f;
            filtered.thresholdDb = -20.0f;
            GateEngine engine2;
            engine2.setSettings (filtered);
            engine2.prepare (48000.0);

            auto hat = tone (0.2f, 48000, 8000.0f);
            engine2.process (hat.getArrayOfWritePointers(), 1, 48000, nullptr, 0);
            expect (hat.getMagnitude (0, 24000, 24000) < 0.001f, "the key low-pass keeps it closed");
        }

        beginTest ("Stereo link");
        {
            for (const bool linked : { true, false })
            {
                GateEngine::Settings s;
                s.link = linked;
                GateEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                juce::AudioBuffer<float> buffer (2, 48000);
                for (int i = 0; i < 48000; ++i)
                {
                    const auto x = std::sin (2.0f * juce::MathConstants<float>::pi * 200.0f * (float) i / 48000.0f);
                    buffer.setSample (0, i, 0.5f * x);
                    buffer.setSample (1, i, 0.0005f * x);
                }
                engine.process (buffer.getArrayOfWritePointers(), 2, 48000, nullptr, 0);

                const auto right = buffer.getMagnitude (1, 24000, 24000);
                if (linked)
                    expectWithinAbsoluteError (right, 0.0005f, 0.00002f, "linked: opens with the left");
                else
                    expectEquals (right, 0.0f, "unlinked: right stays closed");
            }
        }

        beginTest ("Extremes stay finite");
        {
            GateEngine engine;
            engine.prepare (192000.0);
            juce::Random rng (12);
            juce::AudioBuffer<float> buffer (2, 64), side (2, 64);
            bool finite = true;

            for (int block = 0; block < 5000; ++block)
            {
                if (block % 5 == 0)
                {
                    GateEngine::Settings s;
                    s.mode = rng.nextBool() ? Mode::gate : Mode::expander;
                    s.thresholdDb = rng.nextBool() ? -80.0f : 0.0f;
                    s.ratio = rng.nextBool() ? 1.5f : 10.0f;
                    s.rangeDb = rng.nextBool() ? GateEngine::kClosedDb : 0.0f;
                    s.attackMs = rng.nextBool() ? 0.05f : 50.0f;
                    s.holdMs = rng.nextBool() ? 0.0f : 500.0f;
                    s.releaseMs = rng.nextBool() ? 5.0f : 2000.0f;
                    s.hysteresisDb = rng.nextBool() ? 0.0f : 12.0f;
                    s.external = rng.nextBool();
                    s.keyHighPassHz = rng.nextBool() ? 0.0f : 5000.0f;
                    s.keyLowPassHz = rng.nextBool() ? 0.0f : 500.0f;
                    s.listen = rng.nextBool();
                    s.link = rng.nextBool();
                    engine.setSettings (s);
                }

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                    {
                        buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * (rng.nextBool() ? 1.0f : 0.0001f));
                        side.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);
                    }

                engine.process (buffer.getArrayOfWritePointers(), 2, 64, side.getArrayOfReadPointers(), 2);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        finite = finite && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 4.0f;
            }
            expect (finite);
        }
    }

private:
    static juce::AudioBuffer<float> tone (float amplitude, int length, float hz = 200.0f)
    {
        juce::AudioBuffer<float> b (1, length);
        for (int i = 0; i < length; ++i)
            b.setSample (0, i, amplitude * std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f));
        return b;
    }
};

static GateTests gateTests;
