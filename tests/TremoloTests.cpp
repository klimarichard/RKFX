#include <juce_audio_basics/juce_audio_basics.h>

#include "TremoloEngine.h"
#include "dsp/Crossover.h"

#include <cmath>

/** RKTremolo DSP and the shared crossover: depth, rate, auto-pan, harmonic
    mode, tempo sync, click-free edges, stability. */
class TremoloTests : public juce::UnitTest
{
public:
    TremoloTests() : juce::UnitTest ("RKTremolo engine") {}

    void runTest() override
    {
        using Mode = TremoloEngine::Mode;
        using Shape = rkfx::LFO::Shape;

        beginTest ("Crossover bands sum to a flat allpass");
        {
            for (const float hz : { 50.0f, 600.0f, 2000.0f, 9000.0f })
            {
                rkfx::Crossover x;
                x.prepare (48000.0);
                x.setFrequency (600.0f);

                float peak = 0.0f;
                for (int i = 0; i < 48000; ++i)
                {
                    float low, high;
                    x.process (std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f), low, high);
                    if (i > 24000)
                        peak = std::max (peak, std::abs (low + high));
                }
                expectWithinAbsoluteError (peak, 1.0f, 0.01f, juce::String (hz) + " Hz");
            }
        }

        beginTest ("Depth 0 is exactly transparent (classic)");
        {
            TremoloEngine::Settings s;
            s.depth = 0.0f;
            const auto in = constant (2, 4800, 0.7f);
            auto out = in;
            run (s, out);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4800; ++i)
                    expect (juce::exactlyEqual (out.getSample (ch, i), 0.7f));
        }

        beginTest ("Depth sets the dip; rate sets the speed");
        {
            TremoloEngine::Settings s;
            s.depth = 1.0f;
            s.rateHz = 5.0f;
            auto out = constant (1, 48000, 1.0f);
            run (s, out);

            float lo = 1.0f, hi = 0.0f;
            int peaks = 0;
            for (int i = 1; i < 47999; ++i)
            {
                const auto v = out.getSample (0, i);
                lo = std::min (lo, v);
                hi = std::max (hi, v);
                if (v > out.getSample (0, i - 1) && v >= out.getSample (0, i + 1) && v > 0.9f)
                    ++peaks;
            }
            expect (lo < 0.02f, "min " + juce::String (lo));
            expect (hi > 0.98f, "max " + juce::String (hi));
            expectEquals (peaks, 5, "five cycles in one second");

            s.depth = 0.5f;
            auto half = constant (1, 48000, 1.0f);
            run (s, half);
            expectWithinAbsoluteError (half.getMagnitude (0, 0, 48000) - findMin (half), 0.5f, 0.02f, "50% depth: dips to half");
        }

        beginTest ("Stereo phase 180 degrees is an auto-panner");
        {
            TremoloEngine::Settings s;
            s.depth = 1.0f;
            s.phaseDeg = 180.0f;
            s.smooth = 0.0f;
            auto out = constant (2, 48000, 1.0f);
            run (s, out);

            // Left and right move in opposite directions: their sum stays near constant.
            float sumMin = 2.0f, sumMax = 0.0f;
            for (int i = 4800; i < 48000; ++i)
            {
                const auto sum = out.getSample (0, i) + out.getSample (1, i);
                sumMin = std::min (sumMin, sum);
                sumMax = std::max (sumMax, sum);
            }
            expectWithinAbsoluteError (sumMin, 1.0f, 0.05f);
            expectWithinAbsoluteError (sumMax, 1.0f, 0.05f);
        }

        beginTest ("Harmonic mode: lows and highs pulse in opposite directions");
        {
            TremoloEngine::Settings s;
            s.mode = Mode::harmonic;
            s.depth = 1.0f;
            s.rateHz = 2.0f;

            const auto envelope = [&] (float hz)
            {
                juce::AudioBuffer<float> b (1, 24000);
                for (int i = 0; i < 24000; ++i)
                    b.setSample (0, i, std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f));
                run (s, b);
                // Level in the first quarter of the LFO cycle (LFO rising from the middle).
                return b.getMagnitude (0, 5000, 1000);
            };

            const auto low = envelope (80.0f), high = envelope (6000.0f);
            expect (std::abs (low - high) > 0.3f, "low " + juce::String (low) + ", high " + juce::String (high));

            // Depth 0: flat level at every frequency (allpass).
            s.depth = 0.0f;
            for (const float hz : { 80.0f, 600.0f, 6000.0f })
            {
                juce::AudioBuffer<float> b (1, 24000);
                for (int i = 0; i < 24000; ++i)
                    b.setSample (0, i, std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f));
                run (s, b);
                // RMS, not peak: the allpass shifts the phase, so samples miss the true peak.
                expectWithinAbsoluteError (b.getRMSLevel (0, 12000, 12000) * std::sqrt (2.0f), 1.0f, 0.01f,
                                           juce::String (hz) + " Hz flat");
            }
        }

        beginTest ("Tempo sync follows the host tempo");
        {
            TremoloEngine::Settings s;
            s.depth = 1.0f;
            s.sync = true;
            s.division = rkfx::SyncDivision::index (8);   // 1/8 at 120 BPM = 4 Hz
            s.smooth = 0.0f;

            rkfx::Transport transport;
            transport.bpm = 120.0;

            TremoloEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);
            auto out = constant (1, 48000, 1.0f);
            for (int start = 0; start < 48000; start += 480)
            {
                auto* p = out.getWritePointer (0, start);
                engine.process (&p, 1, 480, transport);
            }

            int peaks = 0;
            for (int i = 1; i < 47999; ++i)
                if (out.getSample (0, i) > out.getSample (0, i - 1) && out.getSample (0, i) >= out.getSample (0, i + 1)
                    && out.getSample (0, i) > 0.9f)
                    ++peaks;
            expectEquals (peaks, 4);
        }

        beginTest ("Square at 100% depth with no smoothing still never clicks");
        {
            TremoloEngine::Settings s;
            s.depth = 1.0f;
            s.shape = Shape::square;
            s.smooth = 0.0f;
            s.rateHz = 20.0f;
            auto out = constant (1, 48000, 1.0f);
            run (s, out);

            float maxStep = 0.0f;
            for (int i = 1; i < 48000; ++i)
                maxStep = std::max (maxStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
            expect (maxStep < 0.05f, "max step " + juce::String (maxStep));
        }

        beginTest ("Extremes stay finite and bounded");
        {
            TremoloEngine engine;
            engine.prepare (44100.0);
            juce::Random rng (21);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;

            for (int block = 0; block < 5000; ++block)
            {
                if (block % 5 == 0)
                {
                    TremoloEngine::Settings s;
                    s.mode = rng.nextBool() ? Mode::classic : Mode::harmonic;
                    s.depth = rng.nextFloat();
                    s.rateHz = rng.nextBool() ? 0.1f : 20.0f;
                    s.sync = rng.nextBool();
                    s.division = rng.nextInt (rkfx::SyncDivision::kCount);
                    s.shape = (Shape) rng.nextInt (5);
                    s.smooth = rng.nextFloat();
                    s.phaseDeg = rng.nextFloat() * 180.0f;
                    s.crossoverHz = rng.nextBool() ? 200.0f : 2000.0f;
                    engine.setSettings (s);
                }

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

                engine.process (buffer.getArrayOfWritePointers(), 2, 64, {});

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 3.0f;
            }
            expect (ok);
        }
    }

private:
    static juce::AudioBuffer<float> constant (int channels, int length, float value)
    {
        juce::AudioBuffer<float> b (channels, length);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < length; ++i)
                b.setSample (ch, i, value);
        return b;
    }

    static float findMin (const juce::AudioBuffer<float>& b)
    {
        float m = 1.0e9f;
        for (int i = 4800; i < b.getNumSamples(); ++i)
            m = std::min (m, b.getSample (0, i));
        return m;
    }

    static void run (const TremoloEngine::Settings& s, juce::AudioBuffer<float>& buffer)
    {
        TremoloEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        for (int start = 0; start < buffer.getNumSamples(); start += 256)
        {
            const auto n = std::min (256, buffer.getNumSamples() - start);
            float* ptrs[2] = { buffer.getWritePointer (0, start),
                               buffer.getWritePointer (buffer.getNumChannels() - 1, start) };
            engine.process (ptrs, buffer.getNumChannels(), n, {});
        }
    }
};

static TremoloTests tremoloTests;
