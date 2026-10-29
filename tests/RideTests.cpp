#include <juce_audio_basics/juce_audio_basics.h>

#include "RideEngine.h"
#include "dsp/MatchedBiquad.h"

#include <cmath>

namespace
{
    constexpr double kRate = 48000.0;

    /** How much the weighting (mostly its 2 kHz shelf) raises a 1 kHz tone, in dB. */
    float weightingAt1k()
    {
        const auto w = 2.0 * juce::MathConstants<double>::pi * 1000.0 / kRate;
        return (float) (20.0 * std::log10 (rkfx::MatchedDesign::highShelf (2000.0, 4.0, kRate).magnitude (w)));
    }

    float amplitudeForRms (float rmsDb) { return std::sqrt (2.0f) * std::pow (10.0f, rmsDb / 20.0f); }

    /** Feeds a stereo sine (amplitude 0 = silence) for @p seconds, with an
        optional sidechain sine. */
    void feed (RideEngine& e, double seconds, double hz, float amplitude, float sidechainAmplitude = -1.0f)
    {
        static int phase = 0;
        const auto total = (int) (seconds * kRate);
        juce::AudioBuffer<float> io (2, 64), sc (2, 64);

        for (int done = 0; done < total; done += 64)
        {
            for (int i = 0; i < 64; ++i, ++phase)
            {
                const auto s = (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * phase / kRate);
                const auto m = (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * phase / kRate);
                for (int ch = 0; ch < 2; ++ch)
                {
                    io.setSample (ch, i, amplitude * s);
                    sc.setSample (ch, i, juce::jmax (0.0f, sidechainAmplitude) * m);
                }
            }

            e.process (io.getArrayOfWritePointers(), 2, 64,
                       sidechainAmplitude >= 0.0f ? sc.getArrayOfReadPointers() : nullptr, sidechainAmplitude >= 0.0f ? 2 : 0);
        }
    }

    RideEngine makeEngine (const RideEngine::Settings& s)
    {
        RideEngine e;
        e.setSettings (s);
        e.prepare (kRate);
        return e;
    }
}

/** RKRide DSP: converges to the target within Max Boost / Max Cut, glides at
    the Speed (down twice as fast), holds in short pauses and returns to 0 dB
    after Return, follows the music in Relative mode, weights the level, and
    never jumps. */
class RideTests : public juce::UnitTest
{
public:
    RideTests() : juce::UnitTest ("RKRide engine") {}

    void runTest() override
    {
        const auto w = weightingAt1k();

        beginTest ("Converges to the target, within Max Boost and Max Cut");
        {
            RideEngine::Settings s;
            s.maxBoostDb = 18.0f;
            auto e = makeEngine (s);
            feed (e, 5.0, 1000.0, amplitudeForRms (-30.0f));
            expectWithinAbsoluteError (e.getGainDb(), -18.0f - (-30.0f + w), 0.3f, "lifted to the target");

            auto boost = makeEngine ({});
            feed (boost, 5.0, 1000.0, amplitudeForRms (-40.0f));
            expectWithinAbsoluteError (boost.getGainDb(), 6.0f, 0.05f, "stops at Max Boost");

            auto cut = makeEngine ({});
            feed (cut, 5.0, 1000.0, 1.0f);
            expectWithinAbsoluteError (cut.getGainDb(), -6.0f, 0.05f, "stops at Max Cut");
        }

        beginTest ("Glides at the Speed going up, twice as fast going down");
        {
            auto up = makeEngine ({});
            feed (up, 0.5, 1000.0, amplitudeForRms (-40.0f));   // wants +6 dB
            expectWithinAbsoluteError (up.getGainDb(), 6.0f * (1.0f - std::exp (-1.0f)), 0.35f, "63% after 500 ms");

            auto down = makeEngine ({});
            feed (down, 0.25, 1000.0, 1.0f);                    // wants -6 dB
            expectWithinAbsoluteError (down.getGainDb(), -6.0f * (1.0f - std::exp (-1.0f)), 0.4f, "63% after 250 ms");
        }

        beginTest ("Short pauses hold the gain; long ones return to 0 dB");
        {
            auto e = makeEngine ({});
            feed (e, 4.0, 1000.0, amplitudeForRms (-40.0f));
            expectWithinAbsoluteError (e.getGainDb(), 6.0f, 0.1f);
            feed (e, 2.0, 1000.0, 0.0f);                         // shorter than Return (3 s)
            expect (e.getGainDb() > 5.8f, "held: " + juce::String (e.getGainDb()));
            expect (e.takeMeter().holding);
            feed (e, 4.0, 1000.0, 0.0f);                         // now 6 s of silence
            expect (std::abs (e.getGainDb()) < 0.5f, "returned: " + juce::String (e.getGainDb()));

            // A phrase after a short pause starts from the held gain.
            auto f = makeEngine ({});
            feed (f, 4.0, 1000.0, amplitudeForRms (-40.0f));
            feed (f, 1.0, 1000.0, 0.0f);
            feed (f, 0.05, 1000.0, amplitudeForRms (-40.0f));
            expect (f.getGainDb() > 5.5f, "resumes from the held gain: " + juce::String (f.getGainDb()));
        }

        beginTest ("Relative mode follows the music");
        {
            RideEngine::Settings s;
            s.relative = true;
            s.aboveDb = 0.0f;
            s.maxBoostDb = 18.0f;

            auto e = makeEngine (s);
            feed (e, 5.0, 1000.0, amplitudeForRms (-30.0f), amplitudeForRms (-20.0f));
            expectWithinAbsoluteError (e.getGainDb(), 10.0f, 0.3f, "10 dB under the music");

            feed (e, 5.0, 1000.0, amplitudeForRms (-30.0f), amplitudeForRms (-14.0f));
            expectWithinAbsoluteError (e.getGainDb(), 16.0f, 0.3f, "the music 6 dB louder");

            feed (e, 5.0, 1000.0, amplitudeForRms (-30.0f), 0.0f);
            expectWithinAbsoluteError (e.getGainDb(), -18.0f - (-30.0f + w), 0.3f, "silent music: the absolute Target");
        }

        beginTest ("Low rumble counts less than the voice range");
        {
            auto low = makeEngine ({}), mid = makeEngine ({});
            feed (low, 1.0, 40.0, 0.5f);
            feed (mid, 1.0, 1000.0, 0.5f);
            const auto lowDb = low.takeMeter().levelDb, midDb = mid.takeMeter().levelDb;
            expect (midDb - lowDb > 9.0f, "40 Hz " + juce::String (lowDb, 1) + " dB, 1 kHz " + juce::String (midDb, 1) + " dB");
        }

        beginTest ("The gain never jumps, whatever the controls do");
        {
            auto e = makeEngine ({});
            juce::Random rng (8);
            auto previous = e.getGainDb();
            auto maxJump = 0.0f;

            for (int block = 0; block < 400; ++block)
            {
                RideEngine::Settings s;
                s.targetDb = -40.0f + 40.0f * rng.nextFloat();
                s.maxBoostDb = 18.0f * rng.nextFloat();
                s.maxCutDb = 18.0f * rng.nextFloat();
                s.speedMs = 100.0f + 2900.0f * rng.nextFloat();
                s.floorDb = -70.0f + 50.0f * rng.nextFloat();
                s.relative = rng.nextBool();
                e.setSettings (s);

                feed (e, 64.0 / kRate * 8.0, 220.0, block % 50 < 25 ? 0.02f : 0.9f, rng.nextFloat());
                maxJump = juce::jmax (maxJump, std::abs (e.getGainDb() - previous));
                previous = e.getGainDb();
            }

            // 512 samples (10.7 ms) at the fastest glide (50 ms down) move less than a fifth of the way.
            expect (maxJump < 7.0f, "largest move per 10 ms: " + juce::String (maxJump, 2) + " dB");
            expect (std::abs (e.getGainDb()) <= 18.0f);
        }
    }
};

static RideTests rideTests;
