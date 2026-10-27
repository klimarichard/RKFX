#include <juce_audio_basics/juce_audio_basics.h>

#include "DelayEngine.h"

#include <cmath>

/** RKDelay DSP: timing, feedback decay, ping-pong, offset, sync, ducking,
    analog darkening, time glides, stability at 100% feedback. */
class DelayTests : public juce::UnitTest
{
public:
    DelayTests() : juce::UnitTest ("RKDelay engine") {}

    void runTest() override
    {
        using Mode = DelayEngine::Mode;
        using Stereo = DelayEngine::Stereo;

        beginTest ("Echoes arrive on time and decay by the feedback");
        {
            auto s = clean();
            s.timeMs = 100.0f;
            s.feedback = 0.5f;
            const auto out = impulseResponse (s, 1, 48000);

            const auto first = peakAround (out, 0, 4800);
            const auto second = peakAround (out, 0, 9600);
            const auto third = peakAround (out, 0, 14400);
            expect (first > 0.9f, "first echo " + juce::String (first));
            expectWithinAbsoluteError (second / first, 0.5f, 0.05f);
            expectWithinAbsoluteError (third / second, 0.5f, 0.05f);
            expect (out.getMagnitude (0, kBurst + 10, 4780 - kBurst) < 1.0e-3f, "nothing between the burst and the first echo");
        }

        beginTest ("Ping-pong alternates left and right");
        {
            auto s = clean();
            s.timeMs = 100.0f;
            s.feedback = 0.8f;
            s.stereo = Stereo::pingPong;
            const auto out = impulseResponse (s, 2, 48000);

            expect (peakAround (out, 0, 4800) > 0.4f, "1st echo left");
            expect (peakAround (out, 1, 4800) < 1.0e-3f, "...not right");
            expect (peakAround (out, 1, 9600) > 0.2f, "2nd echo right");
            expect (peakAround (out, 0, 9600) < 1.0e-3f, "...not left");
        }

        beginTest ("Right time offset");
        {
            auto s = clean();
            s.timeMs = 200.0f;
            s.offset = 0.25f;   // right at 250 ms
            const auto out = impulseResponse (s, 2, 24000);
            expect (peakAround (out, 0, 9600) > 0.9f);
            expect (peakAround (out, 1, 12000) > 0.9f);
            expect (peakAround (out, 1, 9600) < 1.0e-3f);
        }

        beginTest ("Sync follows the tempo, long values are clamped to the buffer");
        {
            DelayEngine::Settings s;
            s.sync = true;
            s.division = rkfx::SyncDivision::index (4);
            expectWithinAbsoluteError (DelayEngine::leftTimeMs (s, 120.0), 500.0, 1.0e-9);
            s.division = rkfx::SyncDivision::index (8, 'd');
            expectWithinAbsoluteError (DelayEngine::leftTimeMs (s, 120.0), 375.0, 1.0e-9);
            s.division = rkfx::SyncDivision::index (1, 'd');
            expectWithinAbsoluteError (DelayEngine::leftTimeMs (s, 40.0), 4000.0, 1.0e-9);
        }

        beginTest ("Ducking turns the repeats down while playing");
        {
            const auto wetLevel = [] (float duck)
            {
                auto s = clean();
                s.timeMs = 50.0f;
                s.feedback = 0.5f;
                s.duck = duck;
                DelayEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);
                juce::AudioBuffer<float> b (1, 48000);
                for (int i = 0; i < 48000; ++i)
                    b.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / 48000.0f));
                float* p = b.getWritePointer (0);
                engine.process (&p, 1, 48000, {});
                return b.getRMSLevel (0, 24000, 24000);
            };
            expect (wetLevel (1.0f) < wetLevel (0.0f) * 0.3f);
        }

        beginTest ("Analog repeats get darker with each echo");
        {
            auto s = clean();
            s.mode = Mode::analog;
            s.timeMs = 50.0f;
            s.feedback = 0.9f;

            // Feed one burst of a 6 kHz tone; compare its level in the 1st and 4th echo.
            DelayEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);
            juce::AudioBuffer<float> b (1, 24000);
            b.clear();
            for (int i = 0; i < 480; ++i)
                b.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 6000.0f * (float) i / 48000.0f));
            float* p = b.getWritePointer (0);
            engine.process (&p, 1, 24000, {});

            const auto echo1 = b.getRMSLevel (0, 2400, 480);
            const auto echo4 = b.getRMSLevel (0, 9600, 480);
            expect (echo4 < echo1 * 0.9f * 0.9f * 0.9f * 0.3f, "highs fade faster than the feedback alone");
        }

        beginTest ("Changing the time glides without clicks");
        {
            auto s = clean();
            s.timeMs = 300.0f;
            DelayEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> b (1, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;
            for (int block = 0; block < 1500; ++block)
            {
                if (block == 750) { s.timeMs = 50.0f; engine.setSettings (s); }
                for (int i = 0; i < 64; ++i, ++n)
                    b.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) n / 48000.0f));
                float* p = b.getWritePointer (0);
                engine.process (&p, 1, 64, {});
                for (int i = 0; i < 64; ++i)
                {
                    if (block > 300)
                        maxStep = std::max (maxStep, std::abs (b.getSample (0, i) - previous));
                    previous = b.getSample (0, i);
                }
            }
            // A 220 Hz sine at 0.5 moves ~0.014 per sample; the glide pitches it up a few times at most.
            expect (maxStep < 0.12f, "max step " + juce::String (maxStep));
        }

        beginTest ("100% feedback sustains but never runs away, in every mode");
        {
            for (int m = 0; m < 3; ++m)
            {
                DelayEngine::Settings s;
                s.mode = (Mode) m;
                s.feedback = 1.0f;
                s.timeMs = 20.0f;
                s.lowCutHz = 20.0f;
                s.highCutHz = 20000.0f;
                s.modDepth = 1.0f;
                s.modRateHz = 5.0f;
                s.stereo = (Stereo) (m % 3);

                DelayEngine engine;
                engine.setSettings (s);
                engine.prepare (44100.0);

                juce::Random rng (m + 1);
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
                expect (peak < 6.0f, "mode " + juce::String (m) + ": peak " + juce::String (peak));
            }
        }

        beginTest ("Extremes stay finite");
        {
            DelayEngine engine;
            engine.prepare (192000.0);
            juce::Random rng (23);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;
            rkfx::Transport transport;
            transport.bpm = 30.0;

            for (int block = 0; block < 5000; ++block)
            {
                if (block % 5 == 0)
                {
                    DelayEngine::Settings s;
                    s.mode = (Mode) rng.nextInt (3);
                    s.timeMs = rng.nextBool() ? 1.0f : 2000.0f;
                    s.sync = rng.nextBool();
                    s.division = rng.nextInt (rkfx::SyncDivision::kCount);
                    s.feedback = rng.nextFloat();
                    s.stereo = (Stereo) rng.nextInt (3);
                    s.offset = rng.nextFloat() - 0.5f;
                    s.lowCutHz = rng.nextBool() ? 20.0f : 1000.0f;
                    s.highCutHz = rng.nextBool() ? 1000.0f : 20000.0f;
                    s.modDepth = rng.nextFloat();
                    s.modRateHz = rng.nextBool() ? 0.1f : 5.0f;
                    s.duck = rng.nextFloat();
                    engine.setSettings (s);
                }

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

                engine.process (buffer.getArrayOfWritePointers(), 2, 64, transport);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 6.0f;
            }
            expect (ok);
        }
    }

private:
    /** Digital, wide-open loop filters, no modulation. */
    static DelayEngine::Settings clean()
    {
        DelayEngine::Settings s;
        s.lowCutHz = 20.0f;
        s.highCutHz = 20000.0f;
        s.feedback = 0.0f;
        return s;
    }

    static constexpr int kBurst = 480;   // 10 ms

    /** Response to a smooth 10 ms, 1 kHz burst at the start (a single-sample
        click would be smeared by the loop's high cut). */
    static juce::AudioBuffer<float> impulseResponse (const DelayEngine::Settings& s, int channels, int length)
    {
        DelayEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        juce::AudioBuffer<float> b (channels, length);
        b.clear();
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < kBurst; ++i)
            {
                const auto window = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) kBurst);
                b.setSample (ch, i, window * std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f * (float) i / 48000.0f));
            }
        engine.process (b.getArrayOfWritePointers(), channels, length, {});
        return b;
    }

    /** Peak of the echo that starts at @p index. */
    static float peakAround (const juce::AudioBuffer<float>& b, int channel, int index)
    {
        return b.getMagnitude (channel, index, juce::jmin (kBurst + 20, b.getNumSamples() - index));
    }
};

static DelayTests delayTests;
