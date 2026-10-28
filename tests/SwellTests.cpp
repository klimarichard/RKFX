#include <juce_audio_basics/juce_audio_basics.h>

#include "SwellEngine.h"

#include <cmath>

/** RKSwell DSP: swell from silence over the set time, curve shapes,
    retrigger on new attacks (or not at 0%), silence resets, no clicks. */
class SwellTests : public juce::UnitTest
{
public:
    SwellTests() : juce::UnitTest ("RKSwell engine") {}

    void runTest() override
    {
        using Curve = SwellEngine::Curve;

        beginTest ("Curves");
        {
            for (const auto c : { Curve::linear, Curve::exponential, Curve::sCurve })
            {
                expectEquals (SwellEngine::curveGain (c, 0.0f), 0.0f);
                expectWithinAbsoluteError (SwellEngine::curveGain (c, 1.0f), 1.0f, 1.0e-6f);
            }
            expectWithinAbsoluteError (SwellEngine::curveGain (Curve::linear, 0.5f), 0.5f, 1.0e-6f);
            expectWithinAbsoluteError (SwellEngine::curveGain (Curve::sCurve, 0.5f), 0.5f, 1.0e-6f);
            expectWithinAbsoluteError (SwellEngine::curveGain (Curve::exponential, 0.5f), 0.0316f, 1.0e-3f, "-30 dB half way");
        }

        beginTest ("A note after silence swells in over the swell time");
        {
            SwellEngine::Settings s;
            s.curve = Curve::linear;
            s.timeMs = 400.0f;
            const auto env = envelopeOf (s, notes ({ 0 }, 48000));

            expect (env (5) < 0.05f, "starts from silence: " + juce::String (env (5)));
            expectWithinAbsoluteError (env (200), 0.5f, 0.08f, "half way at 200 ms");
            expectWithinAbsoluteError (env (450), 1.0f, 0.02f, "full after the swell time");
        }

        beginTest ("Retrigger: a new attack swells again; at 0% it doesn't");
        {
            for (const float retrigger : { 1.0f, 0.0f })
            {
                SwellEngine::Settings s;
                s.curve = Curve::linear;
                s.timeMs = 100.0f;
                s.retrigger = retrigger;
                // Two notes 500 ms apart, the first still sounding (no silence in between).
                const auto env = envelopeOf (s, notes ({ 0, 24000 }, 48000));
                const auto dip = env (520);   // 20 ms after the second attack
                if (retrigger > 0.5f)
                    expect (dip < 0.4f, "re-swelling: " + juce::String (dip));
                else
                    expect (dip > 0.95f, "no retrigger at 0%: " + juce::String (dip));
            }
        }

        beginTest ("Below the threshold counts as silence and resets the swell");
        {
            SwellEngine::Settings s;
            s.curve = Curve::linear;
            s.timeMs = 100.0f;
            s.retrigger = 0.0f;
            // Note, 300 ms of silence, note again: the second one swells from zero.
            auto input = notes ({ 0 }, 72000);
            for (int i = 24000; i < 38400; ++i)
                input.setSample (0, i, 0.0f);
            const auto env = envelopeOf (s, input);
            expect (env (810) < 0.15f, "second note starts quiet again: " + juce::String (env (810)));
            expect (env (1000) > 0.95f);
        }

        beginTest ("No clicks, even on fast retriggers");
        {
            SwellEngine::Settings s;
            s.timeMs = 10.0f;
            s.retrigger = 1.0f;
            SwellEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            // A sine whose level jumps every 100 ms (constant retriggering).
            juce::AudioBuffer<float> b (1, 48000);
            for (int i = 0; i < 48000; ++i)
                b.setSample (0, i, ((i / 4800) % 2 == 0 ? 0.1f : 0.8f) * std::sin (2.0f * juce::MathConstants<float>::pi * 200.0f * (float) i / 48000.0f));
            engine.process (b.getArrayOfWritePointers(), 1, 48000);
            float maxStep = 0.0f;
            for (int i = 1; i < 48000; ++i)
                maxStep = std::max (maxStep, std::abs (b.getSample (0, i) - b.getSample (0, i - 1)));
            // A 200 Hz sine at 0.8 moves at most ~0.021 per sample.
            expect (maxStep < 0.035f, "max step " + juce::String (maxStep));
        }
    }

private:
    /** Plucked notes (decaying 220 Hz) starting at the given sample positions. */
    static juce::AudioBuffer<float> notes (std::initializer_list<int> starts, int length)
    {
        juce::AudioBuffer<float> b (1, length);
        b.clear();
        for (const auto start : starts)
            for (int i = start; i < length; ++i)
            {
                const auto t = (float) (i - start) / 48000.0f;
                b.addSample (0, i, 0.5f * std::exp (-t / 1.5f) * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * t));
            }
        return b;
    }

    /** Output/input gain at a time (ms), measured over a few cycles. */
    static std::function<float (int)> envelopeOf (const SwellEngine::Settings& s, juce::AudioBuffer<float> input)
    {
        auto output = input;
        SwellEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        engine.process (output.getArrayOfWritePointers(), 1, output.getNumSamples());

        return [input, output] (int ms)
        {
            const auto start = juce::jmax (0, ms * 48 - 216);
            const auto in = input.getRMSLevel (0, start, 432);
            return in > 0.0f ? output.getRMSLevel (0, start, 432) / in : 0.0f;
        };
    }
};

static SwellTests swellTests;
