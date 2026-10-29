#include <juce_audio_basics/juce_audio_basics.h>

#include "RemapEngine.h"

#include <cmath>

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 256;
    constexpr int kChannels = RemapEngine::kChannels;

    /** Each input n carries its own signal: a sine at 100 + 20 n Hz (distinct, never silent). */
    float inputSample (int input, int i)
    {
        return 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * (100.0 + 20.0 * input) * i / kRate);
    }

    struct Run
    {
        juce::AudioBuffer<float> in { kChannels, kBlock }, out { kChannels, kBlock };
    };

    /** One block from sample @p start, inputs as above; returns the inputs and the outputs. */
    Run block (RemapEngine& e, int start = 0)
    {
        Run r;
        for (int ch = 0; ch < kChannels; ++ch)
            for (int i = 0; i < kBlock; ++i)
                r.in.setSample (ch, i, inputSample (ch, start + i));
        r.out.makeCopyOf (r.in);
        e.process (r.out.getArrayOfWritePointers(), kChannels, kBlock);
        return r;
    }

    bool equal (const juce::AudioBuffer<float>& a, int chA, const juce::AudioBuffer<float>& b, int chB, float scale = 1.0f)
    {
        for (int i = 0; i < kBlock; ++i)
            if (! juce::exactlyEqual (a.getSample (chA, i), scale * b.getSample (chB, i)))
                return false;
        return true;
    }

    bool isAverage (const Run& r, int output, int a, int b)
    {
        for (int i = 0; i < kBlock; ++i)
            if (std::abs (r.out.getSample (output, i) - 0.5f * (r.in.getSample (a, i) + r.in.getSample (b, i))) > 1.0e-6f)
                return false;
        return true;
    }

    RemapEngine makeEngine (const RemapEngine::Settings& s)
    {
        RemapEngine e;
        e.setSettings (s);
        e.prepare (kRate, kBlock);
        return e;
    }

    int pairSource (int pair) { return RemapEngine::kFirstPairSource + pair; }   // pair 0 = In 1/2
}

/** RKRemap: bit-exact routes and fan-out, linked pairs, mono sums, trim,
    mute and polarity, every input reachable from every output, and
    click-free switching. */
class RemapTests : public juce::UnitTest
{
public:
    RemapTests() : juce::UnitTest ("RKRemap engine") {}

    void runTest() override
    {
        beginTest ("Routes are bit-exact copies; one input feeds several outputs; Off is silent");
        {
            RemapEngine::Settings s;
            s.outputs[0].source = 5;    // Out 1 <- In 5
            s.outputs[9].source = 5;    // Out 10 <- In 5
            s.outputs[63].source = 64;  // Out 64 <- In 64
            auto e = makeEngine (s);
            const auto r = block (e);
            expect (equal (r.out, 0, r.in, 4), "Out 1 = In 5");
            expect (equal (r.out, 9, r.in, 4), "Out 10 = In 5");
            expect (equal (r.out, 63, r.in, 63), "Out 64 = In 64");
            expect (r.out.getMagnitude (1, 0, kBlock) == 0.0f, "an output without a source is silent");
        }

        beginTest ("Every input reaches every output");
        {
            bool all = true;
            for (int input = 0; input < kChannels; input += 7)
            {
                RemapEngine::Settings s;
                for (auto& o : s.outputs)
                    o.source = input + 1;
                auto e = makeEngine (s);
                const auto r = block (e);
                for (int k = 0; k < kChannels; ++k)
                    all = all && equal (r.out, k, r.in, input);
            }
            expect (all);
        }

        beginTest ("Linked pairs: left to left, right to right; a single input to both sides");
        {
            RemapEngine::Settings s;
            s.inputLinks[2] = true;                  // In 5/6
            s.outputLinks[0] = true;                 // Out 1/2
            s.outputs[0].source = pairSource (2);
            s.outputLinks[1] = true;                 // Out 3/4
            s.outputs[2].source = 9;                 // In 9 to both
            s.outputs[3].source = 1;                 // ignored: Out 4 follows Out 3
            auto e = makeEngine (s);
            const auto r = block (e);
            expect (equal (r.out, 0, r.in, 4) && equal (r.out, 1, r.in, 5), "In 5/6 -> Out 1/2");
            expect (equal (r.out, 2, r.in, 8) && equal (r.out, 3, r.in, 8), "In 9 -> Out 3 and 4");
        }

        beginTest ("Sum to mono: a pair on a single output, and Mono on a linked pair");
        {
            RemapEngine::Settings s;
            s.inputLinks[2] = true;                  // In 5/6 (the synth bass)
            s.outputLinks[0] = true;
            s.outputs[0].source = pairSource (2);    // stereo to the in-ears
            s.outputs[6].source = pairSource (2);    // Out 7 (single): mono to FOH
            s.outputLinks[4] = true;
            s.outputs[8].source = pairSource (2);    // Out 9/10, Mono on
            s.outputs[8].mono = true;
            auto e = makeEngine (s);
            const auto r = block (e);
            expect (equal (r.out, 0, r.in, 4) && equal (r.out, 1, r.in, 5), "stereo stays stereo");
            expect (isAverage (r, 6, 4, 5), "single output: (L + R) / 2");
            expect (isAverage (r, 8, 4, 5) && isAverage (r, 9, 4, 5), "Mono: (L + R) / 2 on both sides");

            RemapEngine::Settings unlinked;
            unlinked.outputs[0].source = pairSource (2);   // In 5/6 not linked: In 5 alone
            auto u = makeEngine (unlinked);
            const auto ur = block (u);
            expect (equal (ur.out, 0, ur.in, 4), "an unlinked pair acts as its left input");
        }

        beginTest ("Mid/side pairs: decoded on a stereo output, the mid on a mono one");
        {
            RemapEngine::Settings s;
            s.inputLinks[0] = true;                  // In 1/2: mid, side
            s.inputMidSide[0] = true;
            s.outputLinks[0] = true;                 // Out 1/2: decoded
            s.outputs[0].source = pairSource (0);
            s.outputs[2].source = pairSource (0);    // Out 3 (single): the mid
            s.outputLinks[2] = true;                 // Out 5/6 with Mono: the mid on both
            s.outputs[4].source = pairSource (0);
            s.outputs[4].mono = true;
            s.outputs[6].source = 2;                 // Out 7: "In 2 Side" alone, raw
            auto e = makeEngine (s);
            const auto r = block (e);

            bool decoded = true;
            for (int i = 0; i < kBlock; ++i)
            {
                const auto m = r.in.getSample (0, i), side = r.in.getSample (1, i);
                decoded = decoded && std::abs (r.out.getSample (0, i) - (m + side)) < 1.0e-6f
                                  && std::abs (r.out.getSample (1, i) - (m - side)) < 1.0e-6f;
            }
            expect (decoded, "left = M + S, right = M - S");
            expect (equal (r.out, 2, r.in, 0), "a single output: the mid, exactly");
            expect (equal (r.out, 4, r.in, 0) && equal (r.out, 5, r.in, 0), "Mono on a linked pair: the mid on both");
            expect (equal (r.out, 6, r.in, 1), "the side channel alone, raw");
        }

        beginTest ("Trim, mute and polarity; a linked pair follows its left output");
        {
            RemapEngine::Settings s;
            s.outputs[0] = { 1, -6.0f, false, false, false };
            s.outputs[1] = { 2, 0.0f, true, false, false };
            s.outputs[2] = { 3, 0.0f, false, true, false };
            s.outputLinks[2] = true;
            s.outputs[4] = { 5, 0.0f, false, true, false };
            s.outputs[5] = { 6, 0.0f, true, false, false };   // ignored while linked
            auto e = makeEngine (s);
            const auto r = block (e);
            const auto g = juce::Decibels::decibelsToGain (-6.0f);
            bool trimmed = true;
            for (int i = 0; i < kBlock; ++i)
                trimmed = trimmed && std::abs (r.out.getSample (0, i) - g * r.in.getSample (0, i)) < 1.0e-6f;
            expect (trimmed, "trim -6 dB");
            expect (r.out.getMagnitude (1, 0, kBlock) == 0.0f, "mute");
            expect (equal (r.out, 2, r.in, 2, -1.0f), "polarity");
            expect (equal (r.out, 5, r.in, 4, -1.0f), "Out 6 follows Out 5: In 5, inverted, not muted");
        }

        beginTest ("Switching never clicks, and a steady route is exact again afterwards");
        {
            RemapEngine::Settings s;
            s.outputs[0].source = 1;
            auto e = makeEngine (s);
            float previous = 0.0f, maxStep = 0.0f;
            int start = 0;
            juce::AudioBuffer<float> last;
            for (int b = 0; b < 60; ++b, start += kBlock)
            {
                if (b == 10) { s.outputs[0].source = 2; e.setSettings (s); }
                if (b == 20) { s.outputs[0].mute = true; e.setSettings (s); }
                if (b == 30) { s.outputs[0].mute = false; s.outputs[0].polarity = true; e.setSettings (s); }
                if (b == 40) { s.outputs[0].trimDb = -24.0f; e.setSettings (s); }
                if (b == 50) { s.outputs[0].trimDb = 0.0f; s.outputs[0].polarity = false; s.outputs[0].source = 3; e.setSettings (s); }
                const auto r = block (e, start);
                for (int i = 0; i < kBlock; ++i)
                {
                    if (b > 0)
                        maxStep = std::max (maxStep, std::abs (r.out.getSample (0, i) - previous));
                    previous = r.out.getSample (0, i);
                }
                if (b == 59)
                    expect (equal (r.out, 0, r.in, 2), "In 3 exact once the change is done");
            }
            // The inputs (0.5 at up to 180 Hz) move up to about 0.012 per sample.
            expect (maxStep < 0.03f, "max step " + juce::String (maxStep));
        }
    }
};

static RemapTests remapTests;
