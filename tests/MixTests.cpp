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
            s.channels[0].strip.trimDb = 6.0f;
            s.channels[0].strip.polarity = { false, true };
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
            s.channels[1].strip.trimDb = -24.0f;      // the right channel's own: unused
            s.channels[1].strip.polarity = { true, true };
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
            s.channels[0].strip.delayOn = true;
            s.channels[0].strip.delayMs = 1.0f;               // 48 samples
            auto e = makeEngine (s);
            juce::AudioBuffer<float> in (kChannels, kBlock), out (kChannels, kBlock);
            in.clear();
            in.setSample (0, 10, 1.0f);
            e.process (in.getArrayOfReadPointers(), out.getArrayOfWritePointers(), kBlock, false);
            expectWithinAbsoluteError (out.getSample (0, 58), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (out.getSample (0, 10), 0.0f, 1.0e-6f);

            s.channels[0].strip.delayOn = false;
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
            s.channels[0].strip.polarity[0] = true; e.setSettings (s); run (3, start);
            s.channels[0].strip.delayOn = true; s.channels[0].strip.delayMs = 7.0f; e.setSettings (s); run (6, start);
            s.channels[0].strip.delayMs = 3.0f;   e.setSettings (s); run (6, start);
            s.channels[0].strip.delayOn = false;  e.setSettings (s); run (6, start);
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

        beginTest ("Strip: the HPF cuts lows, more at 24 dB/oct; its response matches the display");
        {
            const auto level = [] (MixEngine::Settings s, float hz)
            {
                auto e = makeEngine (s);
                juce::AudioBuffer<float> in (kChannels, kBlock), out (kChannels, kBlock);
                float loudest = 0.0f;
                for (int b = 0; b < 40; ++b)
                {
                    in.clear();
                    for (int i = 0; i < kBlock; ++i)
                    {
                        const auto x = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (b * kBlock + i) / kRate);
                        in.setSample (0, i, x);
                        in.setSample (1, i, x);
                    }
                    e.process (in.getArrayOfReadPointers(), out.getArrayOfWritePointers(), kBlock, false);
                    if (b >= 30)
                        loudest = juce::jmax (loudest, out.getMagnitude (0, 0, kBlock));
                }
                return juce::Decibels::gainToDecibels (loudest / 0.5f);
            };

            auto s = only ({ 0 });
            s.links[0] = true;
            s.channels[0].strip.hpf = { true, 100.0f, false };
            const auto at30 = level (s, 30.0f);
            expectWithinAbsoluteError (at30, (float) StripDsp::responseDb (s.channels[0].strip, 30.0, kRate), 0.5f);
            expectLessThan (at30, -15.0f, "12 dB/oct: 30 Hz well down");
            expectWithinAbsoluteError (level (s, 1000.0f), 0.0f, 0.2f);
            s.channels[0].strip.hpf.steep = true;
            expectLessThan (level (s, 30.0f), at30 - 15.0f, "24 dB/oct: steeper");
        }

        beginTest ("Strip: an EQ bell boosts its band by its gain; flat bands are skipped");
        {
            auto s = only ({ 0 });
            s.links[0] = true;
            s.channels[0].strip.eq.on = true;
            auto e = makeEngine (s);
            auto r = block (e, 0, { 0, 1 });
            bool exact = true;
            for (int i = 0; i < kBlock; ++i)
                exact = exact && juce::exactlyEqual (r.outSample (0, i), r.inSample (0, i));
            expect (exact, "EQ on with every band flat: bit-exact");

            s.channels[0].strip.eq.bands[2] = { true, 1000.0f, 6.0f, 1.0f };
            expectWithinAbsoluteError ((float) StripDsp::responseDb (s.channels[0].strip, 1000.0, kRate), 6.0f, 0.01f);
            expectWithinAbsoluteError ((float) StripDsp::responseDb (s.channels[0].strip, 50.0, kRate), 0.0f, 0.2f);
        }

        beginTest ("Strip: the gate closes on a quiet signal and opens for a loud one; a key opens it from another input");
        {
            const auto run = [] (MixEngine& e, float level0, float level5)
            {
                juce::AudioBuffer<float> in (kChannels, kBlock), out (kChannels, kBlock);
                float loudest = 0.0f;
                for (int b = 0; b < 40; ++b)
                {
                    in.clear();
                    for (int i = 0; i < kBlock; ++i)
                    {
                        const auto x = (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (b * kBlock + i) / kRate);
                        in.setSample (0, i, level0 * x);
                        in.setSample (5, i, level5 * x);
                    }
                    e.process (in.getArrayOfReadPointers(), out.getArrayOfWritePointers(), kBlock, false);
                    if (b >= 30)
                        loudest = juce::jmax (loudest, out.getMagnitude (0, 0, kBlock));
                }
                return loudest;
            };

            auto s = only ({ 0 });
            s.channels[0].strip.gate.on = true;
            s.channels[0].strip.gate.thresholdDb = -30.0f;
            auto e = makeEngine (s);
            expectLessThan (run (e, 0.01f, 0.0f), 1.0e-5f, "-40 dBFS: closed");
            e = makeEngine (s);
            expectGreaterThan (run (e, 0.3f, 0.0f), 0.1f, "-10 dBFS: open");

            s.channels[0].strip.gate.key = 6;          // keyed by input 6
            e = makeEngine (s);
            expectGreaterThan (run (e, 0.01f, 0.3f), 0.004f, "the key opens it for the quiet channel");
            e = makeEngine (s);
            expectLessThan (run (e, 0.3f, 0.0f), 3.0e-4f, "a silent key keeps it closed (below -60 dB)");
        }

        beginTest ("Strip: the compressor reduces a loud signal by its ratio; makeup adds back");
        {
            auto s = only ({ 0 });
            s.links[0] = true;
            auto& comp = s.channels[0].strip.comp;
            comp = { true, -20.0f, 4.0f, 0.0f, 1.0f, 50.0f, 0.0f, 0 };
            expectWithinAbsoluteError (StripDsp::compTransferDb (comp, 0.0f), -15.0f, 0.01f);
            comp.makeupDb = 6.0f;
            expectWithinAbsoluteError (StripDsp::compTransferDb (comp, 0.0f), -9.0f, 0.01f);

            auto e = makeEngine (s);
            juce::AudioBuffer<float> in (kChannels, kBlock), out (kChannels, kBlock);
            float loudest = 0.0f;
            for (int b = 0; b < 60; ++b)
            {
                in.clear();
                for (int i = 0; i < kBlock; ++i)
                    in.setSample (0, i, 0.9f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (b * kBlock + i) / kRate));
                e.process (in.getArrayOfReadPointers(), out.getArrayOfWritePointers(), kBlock, false);
                if (b >= 50)
                    loudest = juce::jmax (loudest, out.getMagnitude (0, 0, kBlock));
            }
            expectLessThan (loudest, 0.9f * 0.85f, "compressed");
            expectLessThan (e.takeCompReductionDb (0), -3.0f, "gain reduction reported");
        }

        beginTest ("Strip: every section on and off again is bit-exact; switching doesn't click");
        {
            auto s = only ({ 0 });
            s.links[0] = true;
            auto e = makeEngine (s);
            auto& strip = s.channels[0].strip;
            strip.hpf.on = strip.gate.on = strip.eq.on = strip.comp.on = true;
            strip.gate.thresholdDb = -80.0f;
            strip.eq.bands[1].gainDb = 4.0f;
            float worstStep = 0.0f, last = 0.0f;
            int start = 0;
            const auto run = [&] (int blocks)
            {
                for (int b = 0; b < blocks; ++b, start += kBlock)
                {
                    const auto r = block (e, start, { 0, 1 });
                    for (int i = 0; i < kBlock; ++i)
                    {
                        worstStep = juce::jmax (worstStep, std::abs (r.outSample (0, i) - last));
                        last = r.outSample (0, i);
                    }
                }
            };
            run (2);
            e.setSettings (s); run (6);
            strip.eq.bands[1].gainDb = -6.0f; e.setSettings (s); run (4);
            strip.hpf.steep = true; e.setSettings (s); run (4);
            strip.hpf.on = strip.gate.on = strip.eq.on = strip.comp.on = false;
            e.setSettings (s); run (6);
            expectLessThan (worstStep, 0.03f, "no clicks");

            const auto r = block (e, start, { 0, 1 });
            bool exact = true;
            for (int i = 0; i < kBlock; ++i)
                exact = exact && juce::exactlyEqual (r.outSample (0, i), r.inSample (0, i));
            expect (exact, "all off: bit-exact again");
        }

        //==============================================================================
        // Buses

        const auto busLevel = [] (MixEngine& e, int output, int inputCh, float amplitude = 0.5f, int blocks = 30)
        {
            juce::AudioBuffer<float> in (kChannels, kBlock), out (kChannels, kBlock);
            float loudest = 0.0f;
            for (int b = 0; b < blocks; ++b)
            {
                in.clear();
                for (int i = 0; i < kBlock; ++i)
                    in.setSample (inputCh, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (b * kBlock + i) / kRate));
                e.process (in.getArrayOfReadPointers(), out.getArrayOfWritePointers(), kBlock, false);
                if (b >= blocks - 5)
                    loudest = juce::jmax (loudest, out.getMagnitude (output, 0, kBlock));
            }
            return loudest;
        };

        beginTest ("Aux: a pre-fader send ignores the fader, a post-fader one follows it; mute silences both");
        {
            auto s = only ({});
            s.buses[0] = {};
            s.buses[0].stereo = false;
            s.buses[0].output = 3;                           // Out 3
            s.buses[1] = {};
            s.buses[1].stereo = false;
            s.buses[1].output = 4;
            s.buses[1].defaultTap = mix::Tap::postFader;
            s.channels[0].sendDb[0] = 0.0f;
            s.channels[0].sendDb[1] = 0.0f;
            MixEngine::Levels levels;
            levels.channelDb[0] = mix::kFaderMinDb;          // fader down
            auto e = makeEngine (s, levels);
            expectWithinAbsoluteError (busLevel (e, 2, 0), 0.5f, 1.0e-4f, "pre-fader: full level, unpanned into a mono bus");
            expectEquals (busLevel (e, 3, 0), 0.0f, "post-fader: down with the fader");

            levels.channelDb[0] = -6.0f;
            e = makeEngine (s, levels);
            expectWithinAbsoluteError (busLevel (e, 3, 0), 0.5f * std::pow (10.0f, -0.3f), 1.0e-4f);

            levels.channelMute[0] = true;
            e = makeEngine (s, levels);
            expectEquals (busLevel (e, 2, 0) + busLevel (e, 3, 0), 0.0f, "mute applies at every tap");
        }

        beginTest ("Tap per send: the singer's in-ear before the compressor, another mix after it");
        {
            auto s = only ({});
            auto& comp = s.channels[0].strip.comp;
            comp = { true, -30.0f, 20.0f, 0.0f, 0.5f, 50.0f, 0.0f, 0 };
            for (int bus = 0; bus < 2; ++bus)
            {
                s.buses[(size_t) bus] = {};
                s.buses[(size_t) bus].stereo = false;
                s.buses[(size_t) bus].output = bus + 1;
                s.channels[0].sendDb[(size_t) bus] = 0.0f;
            }
            s.channels[0].tap[0] = (int) mix::Tap::preComp;  // bus 1 overrides; bus 2 at its default, Pre-Fader
            auto e = makeEngine (s);
            expectWithinAbsoluteError (busLevel (e, 0, 0), 0.5f, 1.0e-3f, "Pre-Comp: uncompressed");
            e = makeEngine (s);
            expectLessThan (busLevel (e, 1, 0), 0.2f, "Pre-Fader: compressed");
        }

        beginTest ("Group: post-fader assigns, its fader and pan into the main; output adds to what's there");
        {
            auto s = only ({});
            s.buses[3] = {};
            s.buses[3].type = mix::BusType::group;
            s.buses[3].output = mix::pairTarget (4);          // Out 5/6
            s.channels[0].assign[3] = true;
            s.links[0] = true;
            MixEngine::Levels levels;
            levels.channelDb[0] = -6.0f;
            levels.busDb[3] = -6.0f;
            auto e = makeEngine (s, levels);
            const auto expected = 0.5f * std::pow (10.0f, -0.6f);
            expectWithinAbsoluteError (busLevel (e, 0, 0), expected, 1.0e-4f, "group -> main");
            expectWithinAbsoluteError (busLevel (e, 4, 0), expected, 1.0e-4f, "group -> its own output");

            s.buses[3].toMain = false;
            e = makeEngine (s, levels);
            expectEquals (busLevel (e, 0, 0), 0.0f, "group off the main");
        }

        beginTest ("Groups feed only later groups; FX returns to the main and feeds Aux buses");
        {
            MixEngine::Settings s;
            s.buses[2].type = mix::BusType::group;
            s.buses[5].type = mix::BusType::group;
            expect (MixEngine::canAssign (s, 2, 5));
            expect (! MixEngine::canAssign (s, 5, 2), "no loops: never to an earlier group");
            s.buses[7].type = mix::BusType::fx;
            s.buses[8].type = mix::BusType::aux;
            expect (MixEngine::canSend (s, 7, 8), "FX -> Aux");
            expect (! MixEngine::canSend (s, 8, 7), "Aux feeds nothing");
            expect (! MixEngine::canSend (s, 7, 2), "FX never feeds a group");
            expect (MixEngine::canSend (s, 2, 7) && MixEngine::canSend (s, 2, 8), "Group -> FX and Aux");

            // Channel -> group 3 -> group 6 -> main; FX 8 fed post-fader, back into the main and on to Aux 9.
            auto t = only ({});
            t.buses[2] = {}; t.buses[2].type = mix::BusType::group; t.buses[2].toMain = false;
            t.buses[2].assign[5] = true;
            t.buses[5] = {}; t.buses[5].type = mix::BusType::group;
            t.channels[0].assign[2] = true;
            t.links[0] = true;
            auto e = makeEngine (t);
            expectWithinAbsoluteError (busLevel (e, 0, 0), 0.5f, 1.0e-4f, "through two groups to the main");

            auto f = only ({});
            f.links[0] = true;
            f.buses[7] = {}; f.buses[7].type = mix::BusType::fx;
            f.buses[7].sendDb[8] = 0.0f;
            f.buses[8] = {}; f.buses[8].output = mix::pairTarget (10);   // Out 11/12
            f.channels[0].sendDb[7] = 0.0f;
            e = makeEngine (f);
            expectWithinAbsoluteError (busLevel (e, 0, 0), 0.5f, 1.0e-4f, "FX return in the main");
            e = makeEngine (f);
            expectWithinAbsoluteError (busLevel (e, 10, 0), 0.5f, 1.0e-4f, "FX -> Aux (reverb in the in-ears)");
        }

        beginTest ("Pan law into buses: a mono channel into a stereo bus pans; pan follows can be off");
        {
            auto s = only ({});
            s.buses[0] = {};
            s.buses[0].output = mix::pairTarget (0);
            s.channels[0].sendDb[0] = 0.0f;
            s.channels[0].pan = -1.0f;
            auto e = makeEngine (s);
            expectWithinAbsoluteError (busLevel (e, 0, 0), 0.5f, 1.0e-4f);
            e = makeEngine (s);
            expectEquals (busLevel (e, 1, 0), 0.0f, "follows the channel's pan");

            s.buses[0].panFollows = false;
            e = makeEngine (s);
            expectWithinAbsoluteError (busLevel (e, 1, 0), 0.5f * std::sqrt (0.5f), 1.0e-4f, "centred, -3 dB");
        }

        beginTest ("No clicks: sends, assigns and bus faders glide");
        {
            auto s = only ({ 0 });
            s.buses[0] = {}; s.buses[0].output = mix::pairTarget (0);
            s.buses[1] = {}; s.buses[1].type = mix::BusType::group;
            MixEngine::Levels levels;
            auto e = makeEngine (s, levels);
            float worstStep = 0.0f, last = 0.0f;
            int start = 0;
            const auto run = [&] (int blocks)
            {
                for (int b = 0; b < blocks; ++b, start += kBlock)
                {
                    const auto r = block (e, start, { 0 });
                    for (int i = 0; i < kBlock; ++i)
                    {
                        worstStep = juce::jmax (worstStep, std::abs (r.outSample (0, i) - last));
                        last = r.outSample (0, i);
                    }
                }
            };
            run (2);
            s.channels[0].sendDb[0] = 0.0f;   e.setSettings (s); run (3);
            s.channels[0].assign[1] = true;   e.setSettings (s); run (3);
            levels.busDb[0] = -30.0f;         e.setLevels (levels); run (3);
            levels.busMute[1] = true;         e.setLevels (levels); run (3);
            s.channels[0].sendDb[0] = mix::kFaderMinDb; e.setSettings (s); run (3);
            expectLessThan (worstStep, 0.03f);
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
