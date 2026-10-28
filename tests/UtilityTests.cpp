#include <juce_audio_basics/juce_audio_basics.h>

#include "UtilityEngine.h"

#include <cmath>

/** RKUtility DSP: bit-exact defaults, gain and -inf, balance, width,
    channel modes, swap, polarity, mid/side round trip, channel delay,
    bass mono, click-free switching and mute. */
class UtilityTests : public juce::UnitTest
{
public:
    UtilityTests() : juce::UnitTest ("RKUtility engine") {}

    void runTest() override
    {
        using Mode = UtilityEngine::ChannelMode;

        beginTest ("Defaults pass the signal bit-for-bit");
        {
            const auto in = stereoNoise();
            auto out = in;
            run ({}, out);
            expect (identical (in, out));
        }

        beginTest ("Gain, -inf, balance");
        {
            UtilityEngine::Settings s;
            s.gainDb = -6.0f;
            auto out = constant (0.5f, 0.5f);
            run (s, out);
            expectWithinAbsoluteError (out.getSample (0, 4000), 0.5f * juce::Decibels::decibelsToGain (-6.0f), 1.0e-5f);

            s.gainDb = UtilityEngine::kSilentDb;
            out = constant (0.5f, 0.5f);
            run (s, out);
            expectEquals (out.getSample (0, 4000), 0.0f, "-inf is silence");

            UtilityEngine::Settings b;
            b.pan = 1.0f;
            out = constant (0.5f, 0.5f);
            run (b, out);
            expectEquals (out.getSample (0, 4000), 0.0f, "full right: left silent");
            expectEquals (out.getSample (1, 4000), 0.5f, "full right: right at unity");
        }

        beginTest ("Channel modes, swap and polarity");
        {
            const auto check = [this] (UtilityEngine::Settings s, float expectL, float expectR, const juce::String& what)
            {
                auto out = constant (0.6f, 0.2f);
                run (s, out);
                expectWithinAbsoluteError (out.getSample (0, 4000), expectL, 1.0e-5f, what + " L");
                expectWithinAbsoluteError (out.getSample (1, 4000), expectR, 1.0e-5f, what + " R");
            };

            UtilityEngine::Settings s;
            s.channelMode = Mode::leftOnly;      check (s, 0.6f, 0.6f, "left only");
            s.channelMode = Mode::rightOnly;     check (s, 0.2f, 0.2f, "right only");
            s.channelMode = Mode::midSideEncode; check (s, 0.4f, 0.2f, "M/S encode");
            s.channelMode = Mode::midSideDecode; check (s, 0.8f, 0.4f, "M/S decode");
            s.channelMode = Mode::stereo;
            s.swap = true;                       check (s, 0.2f, 0.6f, "swap");
            s.swap = false;
            s.invertRight = true;                check (s, 0.6f, -0.2f, "invert right");
        }

        beginTest ("Mid/side encode then decode gives the original back");
        {
            const auto in = stereoNoise();
            auto out = in;
            UtilityEngine::Settings enc, dec;
            enc.channelMode = Mode::midSideEncode;
            dec.channelMode = Mode::midSideDecode;
            run (enc, out);
            run (dec, out);
            float maxError = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < out.getNumSamples(); ++i)
                    maxError = std::max (maxError, std::abs (out.getSample (ch, i) - in.getSample (ch, i)));
            expect (maxError < 1.0e-6f);
        }

        beginTest ("Width: 0% mono, 200% doubles the side");
        {
            UtilityEngine::Settings s;
            s.width = 0.0f;
            auto out = constant (0.6f, 0.2f);
            run (s, out);
            expectWithinAbsoluteError (out.getSample (0, 4000), 0.4f, 1.0e-5f);
            expectWithinAbsoluteError (out.getSample (1, 4000), 0.4f, 1.0e-5f);

            s.width = 2.0f;
            out = constant (0.6f, 0.2f);
            run (s, out);
            expectWithinAbsoluteError (out.getSample (0, 4000), 0.8f, 1.0e-5f);
            expectWithinAbsoluteError (out.getSample (1, 4000), 0.0f, 1.0e-5f);
        }

        beginTest ("Channel delay in 0.01 ms steps");
        {
            UtilityEngine::Settings s;
            s.delayLeftMs = 1.0f;   // 48 samples
            UtilityEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);
            juce::AudioBuffer<float> b (2, 200);
            b.clear();
            b.setSample (0, 0, 1.0f);
            b.setSample (1, 0, 1.0f);
            engine.process (b.getArrayOfWritePointers(), 2, 200);
            expectWithinAbsoluteError (b.getSample (0, 48), 1.0f, 1.0e-5f, "left delayed by 48 samples");
            expectEquals (b.getSample (1, 0), 1.0f, "right untouched");
        }

        beginTest ("Bass mono: lows to the centre, highs keep their stereo");
        {
            UtilityEngine::Settings s;
            s.bassMonoHz = 200.0f;
            const auto sideLevel = [&s] (float hz)
            {
                // Opposite-polarity sine on the two sides: pure "side" signal.
                juce::AudioBuffer<float> b (2, 48000);
                for (int i = 0; i < 48000; ++i)
                {
                    const auto x = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f);
                    b.setSample (0, i, x);
                    b.setSample (1, i, -x);
                }
                UtilityEngine engine;
                engine.setSettings (s);
                engine.prepare (48000.0);
                engine.process (b.getArrayOfWritePointers(), 2, 48000);
                double side = 0.0;
                for (int i = 24000; i < 48000; ++i)
                    side += std::pow (0.5 * (b.getSample (0, i) - b.getSample (1, i)), 2.0);
                return std::sqrt (side / 24000.0) / (0.5 / std::sqrt (2.0));
            };
            expect (sideLevel (40.0f) < 0.15, "40 Hz side removed: " + juce::String (sideLevel (40.0f)));
            expect (sideLevel (3000.0f) > 0.95, "3 kHz side kept");
        }

        beginTest ("Switching modes and polarity, and muting, never click");
        {
            UtilityEngine engine;
            UtilityEngine::Settings s;
            engine.setSettings (s);
            engine.prepare (48000.0);
            juce::AudioBuffer<float> b (2, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;
            for (int block = 0; block < 700; ++block)
            {
                if (block % 100 == 99)
                {
                    s.channelMode = (Mode) ((int) s.channelMode == 4 ? 0 : (int) s.channelMode + 1);
                    s.invertLeft = ! s.invertLeft;
                    s.mute = block % 200 == 99;
                    engine.setSettings (s);
                }
                for (int i = 0; i < 64; ++i, ++n)
                {
                    const auto x = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 100.0f * (float) n / 48000.0f);
                    b.setSample (0, i, x);
                    b.setSample (1, i, 0.3f * x);
                }
                engine.process (b.getArrayOfWritePointers(), 2, 64);
                for (int i = 0; i < 64; ++i)
                {
                    if (block > 10)
                        maxStep = std::max (maxStep, std::abs (b.getSample (0, i) - previous));
                    previous = b.getSample (0, i);
                }
            }
            expect (maxStep < 0.03f, "max step " + juce::String (maxStep));
        }
    }

private:
    static juce::AudioBuffer<float> stereoNoise()
    {
        juce::Random rng (61);
        juce::AudioBuffer<float> b (2, 4800);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4800; ++i)
                b.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);
        return b;
    }

    static juce::AudioBuffer<float> constant (float l, float r)
    {
        juce::AudioBuffer<float> b (2, 4800);
        for (int i = 0; i < 4800; ++i)
        {
            b.setSample (0, i, l);
            b.setSample (1, i, r);
        }
        return b;
    }

    static void run (const UtilityEngine::Settings& s, juce::AudioBuffer<float>& b)
    {
        UtilityEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        engine.process (b.getArrayOfWritePointers(), b.getNumChannels(), b.getNumSamples());
    }

    static bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (! juce::exactlyEqual (a.getSample (ch, i), b.getSample (ch, i)))
                    return false;
        return true;
    }
};

static UtilityTests utilityTests;
