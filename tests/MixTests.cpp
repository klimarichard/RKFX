#include <juce_audio_basics/juce_audio_basics.h>

#include "MixEngine.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 256;
    constexpr int kChannels = MixEngine::kChannels;

    /** Each input n carries its own signal: a sine at 100 + 30 n Hz. */
    float inputSample (int input, int i)
    {
        return 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * (100.0 + 30.0 * input) * i / kRate);
    }

    struct Run
    {
        juce::AudioBuffer<float> in { kChannels, kBlock }, out { kChannels, kBlock };
        float inSample (int ch, int i) const { return in.getSample (ch, i); }
        float outSample (int ch, int i) const { return out.getSample (ch, i); }
    };

    /** One block from sample @p start, inputs as above (only @p liveInputs carry signal when given). */
    Run block (MixEngine& e, int start = 0, std::initializer_list<int> liveInputs = {})
    {
        Run r;
        r.in.clear();
        for (int ch = 0; ch < kChannels; ++ch)
        {
            const auto live = liveInputs.size() == 0 || std::find (liveInputs.begin(), liveInputs.end(), ch) != liveInputs.end();
            if (live)
                for (int i = 0; i < kBlock; ++i)
                    r.in.setSample (ch, i, inputSample (ch, start + i));
        }
        e.process (r.in.getArrayOfReadPointers(), r.out.getArrayOfWritePointers(), kBlock, true);
        return r;
    }

    /** Settings with every channel off the main except @p channels. */
    MixEngine::Settings only (std::initializer_list<int> channels)
    {
        MixEngine::Settings s;
        for (auto& c : s.channels)
            c.toMain = false;
        for (auto ch : channels)
            s.channels[(size_t) ch].toMain = true;
        return s;
    }

    MixEngine makeEngine (const MixEngine::Settings& s, const MixEngine::Levels& levels = {})
    {
        MixEngine e;
        e.prepare (kRate, kBlock);
        e.setLevels (levels);
        e.setSettings (s);
        e.reset();   // start at the targets (no glide)
        return e;
    }

    float maxError (const Run& r, int outCh, const std::function<float (int)>& expected)
    {
        float worst = 0.0f;
        for (int i = 0; i < kBlock; ++i)
            worst = juce::jmax (worst, std::abs (r.outSample (outCh, i) - expected (i)));
        return worst;
    }

    float peak (const Run& r, int ch)
    {
        return r.out.getMagnitude (ch, 0, kBlock);
    }
}

/** RKMix phase 1: channels to the main and mono sum, pan laws, fader, mute,
    trim, polarity, delay, links, output targets, and click-free changes. */
class MixTests : public juce::UnitTest
{
public:
    MixTests() : juce::UnitTest ("RKMix engine") {}

    void runTest() override
    {
        beginTest ("A linked pair at its defaults reaches the main bit-exactly");
        {
            auto s = only ({ 4 });
            s.links[2] = true;   // channels 5/6
            auto e = makeEngine (s);
            const auto r = block (e);
            bool exact = true;
            for (int i = 0; i < kBlock; ++i)
                exact = exact && juce::exactlyEqual (r.outSample (0, i), r.inSample (4, i))
                              && juce::exactlyEqual (r.outSample (1, i), r.inSample (5, i));
            expect (exact, "In 5/6 -> Out 1/2 unchanged");
            for (int ch = 2; ch < kChannels; ++ch)
                expectEquals (peak (r, ch), 0.0f, "only the main's outputs carry signal");
        }

        beginTest ("Mono pan law: -3 dB each side at the centre, exact at the ends");
        {
            auto s = only ({ 0 });
            auto e = makeEngine (s);
            auto r = block (e);
            const auto centre = std::sqrt (0.5f);
            expectLessThan (maxError (r, 0, [&] (int i) { return r.inSample (0, i) * centre; }), 1.0e-6f);
            expectLessThan (maxError (r, 1, [&] (int i) { return r.inSample (0, i) * centre; }), 1.0e-6f);

            s.channels[0].pan = -1.0f;
            e = makeEngine (s);
            r = block (e);
            expectLessThan (maxError (r, 0, [&] (int i) { return r.inSample (0, i); }), 1.0e-6f);
            expectEquals (peak (r, 1), 0.0f, "hard left: nothing on the right");
        }

        beginTest ("Balance turns the far side down, the near side stays at unity");
        {
            auto s = only ({ 0 });
            s.links[0] = true;
            s.channels[0].pan = 0.5f;
            auto e = makeEngine (s);
            const auto r = block (e);
            expectLessThan (maxError (r, 0, [&] (int i) { return r.inSample (0, i) * 0.5f; }), 1.0e-6f);
            expectLessThan (maxError (r, 1, [&] (int i) { return r.inSample (1, i); }), 1.0e-6f);
        }

        beginTest ("Fader, mute and the Main switch");
        {
            auto s = only ({ 0 });
            s.links[0] = true;
            MixEngine::Levels levels;
            levels.channelDb[0] = -6.0f;
            auto e = makeEngine (s, levels);
            auto r = block (e);
            const auto g = std::pow (10.0f, -6.0f / 20.0f);
            expectLessThan (maxError (r, 0, [&] (int i) { return r.inSample (0, i) * g; }), 1.0e-6f);

            levels.channelMute[0] = true;
            e = makeEngine (s, levels);
            expectEquals (peak (block (e), 0), 0.0f, "muted");

            levels.channelMute[0] = false;
            levels.channelDb[0] = mix::kFaderMinDb;
            e = makeEngine (s, levels);
            expectEquals (peak (block (e), 0), 0.0f, "fader at the bottom is silence");

            s.channels[0].toMain = false;
            e = makeEngine (s);
            expectEquals (peak (block (e), 0), 0.0f, "off the main");
        }

        beginTest ("Trim and polarity (per side on a pair)");
        {
            auto s = only ({ 0 });
            s.links[0] = true;
            s.channels[0].trimDb = 6.0f;
            s.channels[0].polarity = { false, true };
            auto e = makeEngine (s);
            const auto r = block (e);
            const auto g = std::pow (10.0f, 6.0f / 20.0f);
            expectLessThan (maxError (r, 0, [&] (int i) { return r.inSample (0, i) * g; }), 1.0e-5f);
            expectLessThan (maxError (r, 1, [&] (int i) { return -r.inSample (1, i) * g; }), 1.0e-5f);
        }

        beginTest ("A linked pair uses its left channel's settings");
        {
            auto s = only ({ 0 });
            s.links[0] = true;
            s.channels[1].trimDb = -24.0f;      // the right channel's own: unused
            s.channels[1].polarity = { true, true };
            auto e = makeEngine (s);
            const auto r = block (e);
            expectLessThan (maxError (r, 1, [&] (int i) { return r.inSample (1, i); }), 1.0e-7f);
        }

        beginTest ("Mono sum, single-output targets, and sources adding up on one output");
        {
            auto s = only ({ 0 });
            s.links[0] = true;
            s.main.monoOutput = 5;                      // Out 5
            auto e = makeEngine (s);
            auto r = block (e);
            expectLessThan (maxError (r, 4, [&] (int i) { return 0.5f * (r.inSample (0, i) + r.inSample (1, i)); }), 1.0e-6f);

            s.main.output = 3;                          // the main on Out 3 alone: summed
            s.main.monoOutput = 3;                      // and the mono sum too: they add
            e = makeEngine (s);
            r = block (e);
            expectLessThan (maxError (r, 2, [&] (int i) { return r.inSample (0, i) + r.inSample (1, i); }), 1.0e-6f);

            s.main.output = mix::pairTarget (30);       // Out 31/32
            s.main.monoOutput = mix::kTargetOff;
            e = makeEngine (s);
            r = block (e);
            expectLessThan (maxError (r, 30, [&] (int i) { return r.inSample (0, i); }), 1.0e-7f);
            expectEquals (peak (r, 0), 0.0f);
        }

        beginTest ("Delay: whole samples, and bit-exact again once switched off");
        {
            auto s = only ({ 0 });
            s.links[0] = true;
            s.channels[0].delayOn = true;
            s.channels[0].delayMs = 1.0f;               // 48 samples
            auto e = makeEngine (s);
            juce::AudioBuffer<float> in (kChannels, kBlock), out (kChannels, kBlock);
            in.clear();
            in.setSample (0, 10, 1.0f);
            e.process (in.getArrayOfReadPointers(), out.getArrayOfWritePointers(), kBlock, false);
            expectWithinAbsoluteError (out.getSample (0, 58), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (out.getSample (0, 10), 0.0f, 1.0e-6f);

            s.channels[0].delayOn = false;
            e.setSettings (s);
            for (int b = 0; b < 10; ++b)    // past the 20 ms crossfade
                block (e, b * kBlock);
            const auto r = block (e, 10 * kBlock);
            bool exact = true;
            for (int i = 0; i < kBlock; ++i)
                exact = exact && juce::exactlyEqual (r.outSample (0, i), r.inSample (0, i));
            expect (exact, "an off section is skipped");
        }

        beginTest ("No clicks: mute, fader, pan, polarity and the delay switching glide");
        {
            auto s = only ({ 0 });
            MixEngine::Levels levels;
            auto e = makeEngine (s, levels);
            // A 100 Hz sine moves at most about 0.007 per sample at 0.5 amplitude.
            float worstStep = 0.0f, last = 0.0f;
            const auto run = [&] (int blocks, int& start)
            {
                for (int b = 0; b < blocks; ++b, start += kBlock)
                {
                    const auto r = block (e, start);
                    for (int i = 0; i < kBlock; ++i)
                    {
                        worstStep = juce::jmax (worstStep, std::abs (r.outSample (0, i) - last));
                        last = r.outSample (0, i);
                    }
                }
            };
            int start = 0;
            run (2, start);
            levels.channelMute[0] = true;   e.setLevels (levels); run (3, start);
            levels.channelMute[0] = false;  e.setLevels (levels); run (3, start);
            levels.channelDb[0] = -40.0f;   e.setLevels (levels); run (3, start);
            levels.channelDb[0] = 0.0f;     e.setLevels (levels); run (3, start);
            s.channels[0].pan = -1.0f;      e.setSettings (s); run (3, start);
            s.channels[0].polarity[0] = true; e.setSettings (s); run (3, start);
            s.channels[0].delayOn = true; s.channels[0].delayMs = 7.0f; e.setSettings (s); run (6, start);
            s.channels[0].delayMs = 3.0f;   e.setSettings (s); run (6, start);
            s.channels[0].delayOn = false;  e.setSettings (s); run (6, start);
            s.channels[0].toMain = false;   e.setSettings (s); run (3, start);
            s.channels[0].toMain = true;    e.setSettings (s); run (3, start);
            expectLessThan (worstStep, 0.03f, "no step beyond the signal's own");
        }

        beginTest ("Main fader, mute and balance; mono fader and mute");
        {
            auto s = only ({ 0 });
            s.links[0] = true;
            s.main.balance = -0.5f;
            s.main.monoOutput = 3;
            MixEngine::Levels levels;
            levels.mainDb = -6.0f;
            levels.monoDb = -6.0f;
            auto e = makeEngine (s, levels);
            auto r = block (e);
            const auto g = std::pow (10.0f, -6.0f / 20.0f);
            expectLessThan (maxError (r, 0, [&] (int i) { return r.inSample (0, i) * g; }), 1.0e-6f);
            expectLessThan (maxError (r, 1, [&] (int i) { return r.inSample (1, i) * g * 0.5f; }), 1.0e-6f);
            expectLessThan (maxError (r, 2, [&] (int i) { return 0.5f * g * g * (r.inSample (0, i) + 0.5f * r.inSample (1, i)); }), 1.0e-6f);

            levels.mainMute = true;
            e = makeEngine (s, levels);
            r = block (e);
            expectEquals (peak (r, 0) + peak (r, 2), 0.0f, "main muted: the mono sum too");

            levels.mainMute = false;
            levels.monoMute = true;
            e = makeEngine (s, levels);
            r = block (e);
            expectEquals (peak (r, 2), 0.0f, "mono muted");
            expectGreaterThan (peak (r, 0), 0.1f, "the main still plays");
        }

        beginTest ("All 32 channels reach the main");
        {
            MixEngine::Settings s;   // every channel on the main, mono, centred
            auto e = makeEngine (s);
            for (int ch = 0; ch < kChannels; ++ch)
            {
                const auto r = block (e, 0, { ch });
                expectGreaterThan (peak (r, 0), 0.3f, "channel " + juce::String (ch + 1));
            }
        }

        beginTest ("The fader law: 0 dB at three quarters, the bottom is silence, positions invert");
        {
            expectWithinAbsoluteError (mix::faderDbAt (0.75f), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (mix::faderDbAt (1.0f), 10.0f, 1.0e-6f);
            expectEquals (mix::faderGain (mix::faderDbAt (0.0f)), 0.0f);
            expectEquals (mix::faderGain (0.0f), 1.0f);
            for (float db = -100.0f; db <= 10.0f; db += 2.5f)
                expectWithinAbsoluteError (mix::faderDbAt (mix::faderPositionOf (db)), db, 1.0e-3f);
        }
    }
};

static MixTests mixTests;
