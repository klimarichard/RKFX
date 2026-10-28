#include <juce_audio_basics/juce_audio_basics.h>

#include "ReverbEngine.h"

#include <cmath>

/** RKVerb DSP: decay time accuracy, pre-delay, stereo, damping, freeze,
    click-free type changes, stability. */
class VerbTests : public juce::UnitTest
{
public:
    VerbTests() : juce::UnitTest ("RKVerb engine") {}

    void runTest() override
    {
        using Type = ReverbEngine::Type;

        beginTest ("Decay time matches the setting (RT60)");
        {
            for (const auto type : { Type::room, Type::hall, Type::plate })
                for (const float decay : { 1.0f, 3.0f })
                    for (const float size : { 0.0f, 1.0f })
                    {
                        auto s = neutral();
                        s.type = type;
                        s.decaySeconds = decay;
                        s.size = size;
                        const auto measured = rt60 (s);
                        logMessage ("RT60 " + juce::String (decay) + " s set, type " + juce::String ((int) type)
                                    + ", size " + juce::String (size) + ": measured " + juce::String (measured, 2) + " s");
                        expectWithinAbsoluteError (measured, (double) decay, decay * 0.05,
                                                   "type " + juce::String ((int) type) + ", size " + juce::String (size)
                                                       + ": " + juce::String (measured, 2) + " s");
                    }
        }

        beginTest ("Nothing comes out before the pre-delay");
        {
            auto s = neutral();
            s.type = Type::plate;
            s.predelayMs = 100.0f;
            const auto ir = impulse (s, 48000);
            expect (ir.getMagnitude (0, 0, 4790) < 1.0e-6f, "silent during the pre-delay");
            expect (ir.getMagnitude (0, 4800, 9600) > 1.0e-3f, "reverb after it");
        }

        beginTest ("Mono in gives stereo out; width 0 folds it back");
        {
            for (const float w : { 1.0f, 0.0f })
            {
                auto s = neutral();
                s.width = w;
                const auto ir = impulse (s, 24000);
                float diff = 0.0f;
                for (int i = 0; i < 24000; ++i)
                    diff = std::max (diff, std::abs (ir.getSample (0, i) - ir.getSample (1, i)));
                if (w > 0.5f)
                    expect (diff > 1.0e-3f);
                else
                    expect (diff < 1.0e-6f);
            }
        }

        beginTest ("Damping makes the highs die away faster");
        {
            auto s = neutral();
            s.damping = 1.0f;
            s.decaySeconds = 3.0f;
            const auto lows = toneLevelAfter (s, 300.0f), highs = toneLevelAfter (s, 8000.0f);
            expect (highs < lows * 0.1f, "lows " + juce::String (lows) + ", highs " + juce::String (highs));
        }

        beginTest ("Freeze holds the tail and ignores new input; releasing it decays");
        {
            auto s = neutral();
            s.decaySeconds = 1.0f;
            ReverbEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::Random rng (3);
            juce::AudioBuffer<float> b (2, 4800);
            const auto feed = [&] (float amplitude)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 4800; ++i)
                        b.setSample (ch, i, amplitude * (rng.nextFloat() * 2.0f - 1.0f));
                engine.process (b.getArrayOfWritePointers(), 2, 4800);
                return b.getRMSLevel (0, 0, 4800);
            };

            feed (0.3f);                    // 100 ms of noise
            s.freeze = true;
            engine.setSettings (s);
            feed (0.0f);
            const auto start = feed (0.0f);
            for (int k = 0; k < 40; ++k)    // 4 s frozen, with loud input that must be ignored
                feed (k % 2 == 0 ? 1.0f : 0.0f);
            const auto later = feed (0.0f);
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (later / start), 0.0f, 3.0f, "tail held");

            s.freeze = false;
            engine.setSettings (s);
            for (int k = 0; k < 20; ++k)    // 2 s after release at a 1 s decay
                feed (0.0f);
            expect (feed (0.0f) < start * 0.01f, "decays after release");
        }

        beginTest ("Changing type glides without clicks");
        {
            ReverbEngine engine;
            auto s = neutral();
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> b (2, 64);
            float previous = 0.0f, maxStep = 0.0f, peak = 0.0f;
            int n = 0;
            for (int block = 0; block < 1200; ++block)
            {
                if (block % 300 == 299)
                {
                    s.type = (Type) (((int) s.type + 1) % 3);
                    engine.setSettings (s);
                }
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        b.setSample (ch, i, 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 150.0f * (float) (n + i) / 48000.0f));
                n += 64;
                engine.process (b.getArrayOfWritePointers(), 2, 64);
                for (int i = 0; i < 64; ++i)
                {
                    if (block > 200)
                    {
                        maxStep = std::max (maxStep, std::abs (b.getSample (0, i) - previous));
                        peak = std::max (peak, std::abs (b.getSample (0, i)));
                    }
                    previous = b.getSample (0, i);
                }
            }
            expect (maxStep < peak * 0.15f, "max step " + juce::String (maxStep) + " at peak " + juce::String (peak));
        }

        beginTest ("Extremes stay finite and bounded");
        {
            ReverbEngine engine;
            engine.prepare (44100.0);
            juce::Random rng (37);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;
            for (int block = 0; block < 8000; ++block)
            {
                if (block % 7 == 0)
                {
                    ReverbEngine::Settings s;
                    s.type = (Type) rng.nextInt (3);
                    s.decaySeconds = rng.nextBool() ? 0.2f : 20.0f;
                    s.size = rng.nextFloat();
                    s.predelayMs = rng.nextBool() ? 0.0f : 1000.0f;
                    s.damping = rng.nextFloat();
                    s.lowCutHz = rng.nextBool() ? 20.0f : 1000.0f;
                    s.highCutHz = rng.nextBool() ? 1000.0f : 20000.0f;
                    s.modulation = rng.nextBool() ? 1.0f : 0.0f;
                    s.width = rng.nextFloat();
                    s.freeze = rng.nextInt (4) == 0;
                    engine.setSettings (s);
                }
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 2.0f);
                engine.process (buffer.getArrayOfWritePointers(), 2, 64);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 20.0f;
            }
            expect (ok);
        }
    }

private:
    /** Wide-open filters, no damping or modulation: the cleanest decay. */
    static ReverbEngine::Settings neutral()
    {
        ReverbEngine::Settings s;
        s.damping = 0.0f;
        s.lowCutHz = 20.0f;
        s.highCutHz = 20000.0f;
        s.modulation = 0.0f;
        s.predelayMs = 0.0f;
        return s;
    }

    static juce::AudioBuffer<float> impulse (const ReverbEngine::Settings& s, int length)
    {
        ReverbEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        juce::AudioBuffer<float> b (2, length);
        b.clear();
        b.setSample (0, 0, 1.0f);
        b.setSample (1, 0, 1.0f);
        engine.process (b.getArrayOfWritePointers(), 2, length);
        return b;
    }

    /** Schroeder backward integration, T20 fit (-5 to -25 dB) extrapolated to 60 dB. */
    static double rt60 (const ReverbEngine::Settings& s)
    {
        const int length = (int) (48000.0 * s.decaySeconds * 1.5);
        const auto ir = impulse (s, length);

        std::vector<double> edc ((size_t) length);
        double acc = 0.0;
        for (int i = length - 1; i >= 0; --i)
        {
            const double l = ir.getSample (0, i), r = ir.getSample (1, i);
            acc += l * l + r * r;
            edc[(size_t) i] = acc;
        }

        const auto db = [&] (int i) { return 10.0 * std::log10 (edc[(size_t) i] / edc[0]); };
        int t5 = 0, t25 = 0;
        while (t5 < length - 1 && db (t5) > -5.0)  ++t5;
        while (t25 < length - 1 && db (t25) > -25.0) ++t25;
        return 3.0 * (t25 - t5) / 48000.0;
    }

    /** Level of a tone burst's tail 0.5 s after it stops. */
    static float toneLevelAfter (const ReverbEngine::Settings& s, float hz)
    {
        ReverbEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        juce::AudioBuffer<float> b (2, 48000);
        b.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4800; ++i)
                b.setSample (ch, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f));
        engine.process (b.getArrayOfWritePointers(), 2, 48000);
        return b.getRMSLevel (0, 28800, 4800);
    }
};

static VerbTests verbTests;
