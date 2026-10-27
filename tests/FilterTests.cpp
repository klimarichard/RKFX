#include <juce_audio_basics/juce_audio_basics.h>

#include "FilterEngine.h"
#include "dsp/LFO.h"
#include "dsp/TempoSync.h"

#include <cmath>

/** RKFilter DSP and the shared LFO / tempo sync: slopes, resonance,
    band-pass, envelope, click-free mode changes, stability. */
class FilterTests : public juce::UnitTest
{
public:
    FilterTests() : juce::UnitTest ("RKFilter engine") {}

    void runTest() override
    {
        beginTest ("Low-pass and high-pass slopes");
        {
            FilterEngine::Settings s;
            s.cutoffHz = 1000.0f;
            s.resonance = 0.0f;

            s.mode = FilterEngine::Mode::lowpass;
            expectDb (gainAt (s, 100.0f), 0.0f, 0.3f, "LP12 passband");
            expectDb (gainAt (s, 1000.0f), -3.0f, 0.5f, "LP12 at cutoff");
            expectDb (gainAt (s, 4000.0f), -24.0f, 1.5f, "LP12 two octaves up");

            s.stages = 2;
            expectDb (gainAt (s, 4000.0f), -48.0f, 3.0f, "LP24 two octaves up");
            s.stages = 4;
            expect (gainAt (s, 4000.0f) < juce::Decibels::decibelsToGain (-85.0f), "LP48 two octaves up");
            expectDb (gainAt (s, 100.0f), 0.0f, 0.3f, "LP48 passband");

            s.mode = FilterEngine::Mode::highpass;
            s.stages = 1;
            expectDb (gainAt (s, 10000.0f), 0.0f, 0.3f, "HP12 passband");
            expectDb (gainAt (s, 250.0f), -24.0f, 1.5f, "HP12 two octaves down");
        }

        beginTest ("Band-pass is 0 dB at the centre; slope splits between sides");
        {
            FilterEngine::Settings s;
            s.mode = FilterEngine::Mode::bandpass;
            s.cutoffHz = 1000.0f;
            s.resonance = 0.0f;

            expectDb (gainAt (s, 1000.0f), 0.0f, 0.3f, "BP12 centre");
            const auto narrow = gainAt (s, 4000.0f);
            s.stages = 2;
            expectDb (gainAt (s, 1000.0f), 0.0f, 0.3f, "BP24 centre");
            expect (gainAt (s, 4000.0f) < narrow * 0.5f, "BP24 steeper than BP12");
        }

        beginTest ("Resonance peaks at the cutoff, the same at every slope");
        {
            FilterEngine::Settings s;
            s.cutoffHz = 1000.0f;
            s.resonance = 1.0f;

            const auto peak12 = gainAt (s, 1000.0f);
            expect (juce::Decibels::gainToDecibels (peak12) > 15.0f, "peak " + juce::String (peak12));

            s.stages = 4;
            const auto peak48 = gainAt (s, 1000.0f);
            // The flat extra stages add their -3 dB/section at the cutoff; the peak itself is the same.
            expect (std::abs (juce::Decibels::gainToDecibels (peak48 / peak12)) < 8.0f);
        }

        beginTest ("The envelope opens the filter");
        {
            FilterEngine::Settings s;
            s.cutoffHz = 200.0f;
            s.resonance = 0.0f;
            const auto closed = gainAt (s, 2000.0f, 0.9f);

            s.envAmount = 1.0f;
            const auto open = gainAt (s, 2000.0f, 0.9f);
            expect (open > closed * 10.0f, "open " + juce::String (open) + " closed " + juce::String (closed));
        }

        beginTest ("Mode and slope changes crossfade without clicks");
        {
            FilterEngine engine;
            FilterEngine::Settings s;
            s.cutoffHz = 5000.0f;
            engine.setSettings (s);
            engine.prepare (48000.0);

            juce::AudioBuffer<float> buffer (1, 64);
            float previous = 0.0f, maxStep = 0.0f;
            int n = 0;

            for (int block = 0; block < 300; ++block)
            {
                if (block == 100) { s.mode = FilterEngine::Mode::highpass; engine.setSettings (s); }
                if (block == 200) { s.stages = 4; s.mode = FilterEngine::Mode::bandpass; engine.setSettings (s); }

                for (int i = 0; i < 64; ++i, ++n)
                    buffer.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 200.0f * (float) n / 48000.0f));

                engine.process (buffer.getArrayOfWritePointers(), 1, 64, {});

                for (int i = 0; i < 64; ++i)
                {
                    if (block > 10)
                        maxStep = std::max (maxStep, std::abs (buffer.getSample (0, i) - previous));
                    previous = buffer.getSample (0, i);
                }
            }

            // A 200 Hz sine at 0.5 moves at most 0.013 per sample; a hard switch would jump ~0.5.
            expect (maxStep < 0.03f, "max step " + juce::String (maxStep));
        }

        beginTest ("Extremes and fast modulation stay finite and bounded");
        {
            for (const double rate : { 44100.0, 192000.0 })
            {
                FilterEngine engine;
                engine.prepare (rate);

                juce::Random rng (3);
                juce::AudioBuffer<float> buffer (2, 41);
                float peak = 0.0f;
                bool finite = true;

                for (int block = 0; block < 20000; ++block)
                {
                    if (block % 5 == 0)
                    {
                        FilterEngine::Settings s;
                        s.mode = (FilterEngine::Mode) rng.nextInt (3);
                        s.stages = 1 + rng.nextInt (4);
                        s.cutoffHz = rng.nextBool() ? 20.0f : 20000.0f;
                        s.resonance = rng.nextBool() ? 1.0f : rng.nextFloat();
                        s.envAmount = rng.nextBool() ? 1.0f : -1.0f;
                        s.envSensDb = 24.0f;
                        s.envAttackMs = 1.0f;
                        s.envReleaseMs = 20.0f;
                        s.lfoDepth = rng.nextBool() ? 1.0f : -1.0f;
                        s.lfoShape = (rkfx::LFO::Shape) rng.nextInt (6);
                        s.lfoRateHz = 20.0f;
                        s.lfoPhaseDeg = 180.0f;
                        engine.setSettings (s);
                    }

                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 41; ++i)
                            buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

                    engine.process (buffer.getArrayOfWritePointers(), 2, 41, {});

                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 41; ++i)
                        {
                            const auto v = buffer.getSample (ch, i);
                            finite = finite && std::isfinite (v);
                            peak = std::max (peak, std::abs (v));
                        }
                }

                expect (finite);
                expect (peak < 100.0f, "peak " + juce::String (peak));
            }
        }

        beginTest ("Tempo sync divisions and beat-locked LFO phase");
        {
            using namespace rkfx;
            expectEquals (SyncDivision::beats (SyncDivision::index (4)), 1.0);
            expectEquals (SyncDivision::beats (SyncDivision::index (1)), 4.0);
            expectEquals (SyncDivision::beats (SyncDivision::index (8, 'd')), 0.75);
            expectWithinAbsoluteError (SyncDivision::beats (SyncDivision::index (4, 't')), 2.0 / 3.0, 1.0e-12);
            expectEquals (SyncDivision::beats (SyncDivision::index (32)), 0.125);
            expectEquals (juce::String (SyncDivision::names[SyncDivision::index (16, 't')]), juce::String ("1/16 T"));

            LFO lfo;
            lfo.prepare (48000.0);
            lfo.syncTo (10.25, 1.0);
            expectWithinAbsoluteError (lfo.getPhase(), 0.25, 1.0e-9);
            lfo.syncTo (3.0, 2.0, 0.5);
            expectWithinAbsoluteError (lfo.getPhase(), 0.0, 1.0e-9);

            // Sine at phase 0.25 is the positive peak.
            lfo.setShape (LFO::Shape::sine);
            lfo.syncTo (0.25, 1.0);
            expectWithinAbsoluteError (lfo.next(), 1.0f, 1.0e-5f);
        }
    }

private:
    void expectDb (float gain, float expectedDb, float toleranceDb, const juce::String& what)
    {
        const auto db = juce::Decibels::gainToDecibels (gain, -200.0f);
        expect (std::abs (db - expectedDb) <= toleranceDb, what + ": " + juce::String (db, 2) + " dB");
    }

    /** Steady-state gain for a sine at @p hz (peak in / peak out). */
    static float gainAt (const FilterEngine::Settings& s, float hz, float amplitude = 0.25f)
    {
        constexpr double rate = 48000.0;
        FilterEngine engine;
        engine.setSettings (s);
        engine.prepare (rate);

        const int total = 48000;
        juce::AudioBuffer<float> buffer (1, total);
        for (int i = 0; i < total; ++i)
            buffer.setSample (0, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / rate));

        for (int start = 0; start < total; start += 256)
        {
            auto* p = buffer.getWritePointer (0, start);
            engine.process (&p, 1, std::min (256, total - start), {});
        }

        return buffer.getMagnitude (0, total / 2, total / 2) / amplitude;
    }
};

static FilterTests filterTests;
