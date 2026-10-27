#include <juce_audio_basics/juce_audio_basics.h>

#include "PhaseEngine.h"

#include <cmath>

/** RKPhase DSP: allpass chain is flat on its own, notches when mixed with
    dry, envelope moves the notch, click-free stage changes, feedback
    stability. */
class PhaseTests : public juce::UnitTest
{
public:
    PhaseTests() : juce::UnitTest ("RKPhase engine") {}

    void runTest() override
    {
        beginTest ("The wet signal alone is flat (allpass)");
        {
            for (const float hz : { 100.0f, 800.0f, 6000.0f })
                expectWithinAbsoluteError (level (still (4), hz, 0.25f, false), 1.0f, 0.01f, juce::String (hz) + " Hz");
        }

        beginTest ("Mixed with dry: 2 stages notch at the centre, 4 stages give two notches around it");
        {
            expect (level (still (2), 800.0f, 0.25f, true) < 0.02f, "2 stages: notch at 800 Hz");
            expect (level (still (2), 100.0f, 0.25f, true) > 0.9f);

            // 4 identical first-order stages: -180 degrees at f0 * tan(22.5) and -540 at f0 * tan(67.5).
            const auto low = 800.0f * std::tan (juce::MathConstants<float>::pi / 8.0f);
            const auto high = 800.0f * std::tan (3.0f * juce::MathConstants<float>::pi / 8.0f);
            expect (level (still (4), low, 0.25f, true) < 0.03f, "4 stages: notch at " + juce::String (low));
            expect (level (still (4), high, 0.25f, true) < 0.03f, "4 stages: notch at " + juce::String (high));
            expect (level (still (4), 800.0f, 0.25f, true) > 0.9f, "4 stages: peak at the centre");
        }

        beginTest ("The envelope moves the notch with the playing level");
        {
            auto s = still (2);
            s.envAmount = 1.0f;
            // Quiet: notch stays at the centre. Loud: the sweep moves it away, so 800 Hz comes through.
            expect (level (s, 800.0f, 0.001f, true) < 0.05f, "quiet: notch at the centre");
            expect (level (s, 800.0f, 0.9f, true) > 0.5f, "loud: notch moved");
        }

        beginTest ("Changing the stage count never clicks");
        {
            PhaseEngine engine;
            auto s = still (2);
            engine.setSettings (s);
            engine.prepare (48000.0);

            const int counts[] = { 2, 12, 4, 8, 6, 2 };
            juce::AudioBuffer<float> buffer (1, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;
            for (int block = 0; block < 600; ++block)
            {
                if (block % 100 == 99)
                {
                    s.stages = counts[(block / 100) % 6];
                    engine.setSettings (s);
                }
                for (int i = 0; i < 64; ++i, ++n)
                    buffer.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 200.0f * (float) n / 48000.0f));
                float* p = buffer.getWritePointer (0);
                engine.process (&p, 1, 64, {});
                for (int i = 0; i < 64; ++i)
                {
                    if (block > 10)
                        maxStep = std::max (maxStep, std::abs (buffer.getSample (0, i) - previous));
                    previous = buffer.getSample (0, i);
                }
            }
            // A 200 Hz sine at 0.5 moves at most ~0.013 per sample.
            expect (maxStep < 0.03f, "max step " + juce::String (maxStep));
        }

        beginTest ("Maximum feedback and fast sweeps stay bounded");
        {
            for (const float fb : { 0.95f, -0.95f })
            {
                PhaseEngine engine;
                PhaseEngine::Settings s;
                s.stages = 12;
                s.feedback = fb;
                s.depth = 1.0f;
                s.rateHz = 10.0f;
                s.envAmount = 1.0f;
                s.envSensDb = 24.0f;
                engine.setSettings (s);
                engine.prepare (192000.0);

                juce::Random rng (6);
                juce::AudioBuffer<float> buffer (2, 64);
                float peak = 0.0f;
                bool finite = true;
                for (int block = 0; block < 20000; ++block)
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
                expect (peak < 8.0f, juce::String (fb) + ": peak " + juce::String (peak));
            }
        }
    }

private:
    /** No movement and no feedback: a fixed allpass chain at 800 Hz. */
    static PhaseEngine::Settings still (int stages)
    {
        PhaseEngine::Settings s;
        s.stages = stages;
        s.centreHz = 800.0f;
        s.depth = 0.0f;
        s.feedback = 0.0f;
        return s;
    }

    /** RMS-based gain for a sine: the wet signal, or (dry + wet) / 2. */
    static float level (const PhaseEngine::Settings& s, float hz, float amplitude, bool withDry)
    {
        PhaseEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);

        juce::AudioBuffer<float> buffer (1, 48000);
        for (int i = 0; i < 48000; ++i)
            buffer.setSample (0, i, amplitude * std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f));
        auto dry = buffer;
        float* p = buffer.getWritePointer (0);
        engine.process (&p, 1, 48000, {});

        if (withDry)
            for (int i = 0; i < 48000; ++i)
                buffer.setSample (0, i, 0.5f * (buffer.getSample (0, i) + dry.getSample (0, i)));

        return buffer.getRMSLevel (0, 24000, 24000) / (amplitude / std::sqrt (2.0f));
    }
};

static PhaseTests phaseTests;
