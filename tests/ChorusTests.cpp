#include <juce_audio_basics/juce_audio_basics.h>

#include "ChorusEngine.h"
#include "dsp/DelayLine.h"
#include "dsp/Tilt.h"

#include <cmath>

/** RKChorus DSP and the shared delay line and tilt: interpolation accuracy,
    delay time, stereo image, steady loudness over voice counts, click-free
    voice changes, tone, width, stability. */
class ChorusTests : public juce::UnitTest
{
public:
    ChorusTests() : juce::UnitTest ("RKChorus engine") {}

    void runTest() override
    {
        beginTest ("Delay line: integer delays are exact, fractional reads are accurate");
        {
            rkfx::DelayLine line;
            line.prepare (1000);

            juce::Random rng (3);
            std::vector<float> input;
            for (int i = 0; i < 2000; ++i)
            {
                input.push_back (rng.nextFloat() * 2.0f - 1.0f);
                line.write (input.back());
                if (i > 600)
                    expect (juce::exactlyEqual (line.read (576.0f), input[(size_t) i - 576]));
            }

            // A 1 kHz sine read at a fractional delay matches the ideal value closely.
            rkfx::DelayLine sine;
            sine.prepare (1000);
            float maxError = 0.0f;
            for (int i = 0; i < 4800; ++i)
            {
                const auto w = 2.0 * juce::MathConstants<double>::pi * 1000.0 / 48000.0;
                sine.write ((float) std::sin (w * i));
                if (i > 200)
                    maxError = std::max (maxError, std::abs (sine.read (100.37f) - (float) std::sin (w * (i - 100.37))));
            }
            expect (maxError < 0.002f, "error " + juce::String (maxError));

            // Below one sample, reads are clamped to one sample (never wrap to old data).
            rkfx::DelayLine shortLine;
            shortLine.prepare (100);
            for (int i = 1; i <= 10; ++i)
                shortLine.write ((float) i);
            expect (juce::exactlyEqual (shortLine.read (0.0f), 9.0f));
            expect (juce::exactlyEqual (shortLine.read (0.5f), 9.0f));
        }

        beginTest ("Tilt: 0 passes exactly, positive brightens, negative darkens");
        {
            const auto level = [] (float tone, float hz)
            {
                rkfx::Tilt t;
                t.prepare (48000.0);
                t.setTone (tone);
                float peak = 0.0f;
                for (int i = 0; i < 48000; ++i)
                {
                    const auto y = t.process (std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f));
                    if (i > 24000)
                        peak = std::max (peak, std::abs (y));
                }
                return juce::Decibels::gainToDecibels (peak);
            };

            expectWithinAbsoluteError (level (0.0f, 5000.0f), 0.0f, 0.01f);
            expect (level (1.0f, 10000.0f) > 4.5f);
            expect (level (1.0f, 50.0f) < -4.5f);
            expect (level (-1.0f, 10000.0f) < -4.0f);
        }

        beginTest ("Depth 0, one voice: the wet signal is the input delayed by Delay");
        {
            ChorusEngine::Settings s;
            s.depth = 0.0f;
            s.delayMs = 12.0f;
            ChorusEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (1, 2000);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            float* p = buffer.getWritePointer (0);
            engine.process (&p, 1, 2000, {});

            const auto peakIndex = findPeak (buffer, 0);
            expectEquals (peakIndex, 576, "12 ms at 48 kHz");
        }

        beginTest ("A mono source becomes stereo; width 0 folds it back to mono");
        {
            for (const float w : { 1.0f, 0.0f })
            {
                ChorusEngine::Settings s;
                s.width = w;
                s.depth = 0.8f;
                ChorusEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                auto buffer = noise (2, 48000, 4, true);
                engine.process (buffer.getArrayOfWritePointers(), 2, 48000, {});

                float diff = 0.0f;
                for (int i = 0; i < 48000; ++i)
                    diff = std::max (diff, std::abs (buffer.getSample (0, i) - buffer.getSample (1, i)));

                if (w > 0.5f)
                    expect (diff > 0.05f, "left and right differ");
                else
                    expect (diff < 1.0e-6f, "width 0: identical");
            }
        }

        beginTest ("Loudness stays steady from 1 to 4 voices");
        {
            float levels[5] {};
            for (int v = 1; v <= 4; ++v)
            {
                ChorusEngine::Settings s;
                s.voices = v;
                ChorusEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);

                auto buffer = noise (2, 96000, 9, false);
                engine.process (buffer.getArrayOfWritePointers(), 2, 96000, {});
                levels[v] = juce::Decibels::gainToDecibels (buffer.getRMSLevel (0, 4800, 91200));
            }

            for (int v = 2; v <= 4; ++v)
                expectWithinAbsoluteError (levels[v], levels[1], 1.5f, juce::String (v) + " voices");
        }

        beginTest ("Changing the voice count never clicks");
        {
            ChorusEngine engine;
            ChorusEngine::Settings s;
            s.depth = 1.0f;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (1, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;
            for (int block = 0; block < 400; ++block)
            {
                if (block % 50 == 49)
                {
                    s.voices = 1 + (s.voices % 4);
                    engine.setSettings (s);
                }

                for (int i = 0; i < 64; ++i, ++n)
                    buffer.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 150.0f * (float) n / 48000.0f));
                float* p = buffer.getWritePointer (0);
                engine.process (&p, 1, 64, {});

                for (int i = 0; i < 64; ++i)
                {
                    if (block > 20)
                        maxStep = std::max (maxStep, std::abs (buffer.getSample (0, i) - previous));
                    previous = buffer.getSample (0, i);
                }
            }

            // Up to four 150 Hz voices at 0.5 move at most ~0.02 per sample each.
            expect (maxStep < 0.06f, "max step " + juce::String (maxStep));
        }

        beginTest ("Extremes stay finite and bounded");
        {
            ChorusEngine engine;
            engine.prepare (192000.0);
            juce::Random rng (14);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;

            for (int block = 0; block < 5000; ++block)
            {
                if (block % 5 == 0)
                {
                    ChorusEngine::Settings s;
                    s.voices = 1 + rng.nextInt (4);
                    s.rateHz = rng.nextBool() ? 0.05f : 10.0f;
                    s.depth = rng.nextFloat();
                    s.delayMs = rng.nextBool() ? 5.0f : 30.0f;
                    s.width = rng.nextFloat();
                    s.tone = rng.nextFloat() * 2.0f - 1.0f;
                    s.sync = rng.nextBool();
                    s.division = rng.nextInt (rkfx::SyncDivision::kCount);
                    engine.setSettings (s);
                }

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

                engine.process (buffer.getArrayOfWritePointers(), 2, 64, {});

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 8.0f;
            }
            expect (ok);
        }
    }

private:
    static juce::AudioBuffer<float> noise (int channels, int length, int seed, bool sameInBoth)
    {
        juce::Random rng (seed);
        juce::AudioBuffer<float> b (channels, length);
        for (int i = 0; i < length; ++i)
        {
            const auto x = rng.nextFloat() * 2.0f - 1.0f;
            for (int ch = 0; ch < channels; ++ch)
                b.setSample (ch, i, sameInBoth ? x : (ch == 0 ? x : rng.nextFloat() * 2.0f - 1.0f));
        }
        return b;
    }

    static int findPeak (const juce::AudioBuffer<float>& b, int channel)
    {
        int index = 0;
        for (int i = 1; i < b.getNumSamples(); ++i)
            if (std::abs (b.getSample (channel, i)) > std::abs (b.getSample (channel, index)))
                index = i;
        return index;
    }
};

static ChorusTests chorusTests;
