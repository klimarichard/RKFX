#include <juce_audio_basics/juce_audio_basics.h>

#include "VibratoEngine.h"

#include <cmath>

/** RKVibrato DSP: exact pitch deviation at every rate and shape, no vibrato
    at depth 0, rise after a new note, stability. */
class VibratoTests : public juce::UnitTest
{
public:
    VibratoTests() : juce::UnitTest ("RKVibrato engine") {}

    void runTest() override
    {
        using Shape = VibratoEngine::Shape;

        beginTest ("The pitch swings by exactly the depth, at any rate and shape");
        {
            for (const auto shape : { Shape::sine, Shape::triangle })
                for (const float rate : { 1.0f, 5.0f, 12.0f })
                {
                    VibratoEngine::Settings s;
                    s.depth = 1.0f;   // one semitone
                    s.rateHz = rate;
                    s.shape = shape;
                    const auto range = pitchRange (s);
                    expectWithinAbsoluteError (range.second, 1000.0 * std::pow (2.0, 1.0 / 12.0), 3.0,
                                               "rate " + juce::String (rate) + ": highest " + juce::String (range.second, 1) + " Hz");
                    expectWithinAbsoluteError (range.first, 1000.0 * (2.0 - std::pow (2.0, 1.0 / 12.0)), 3.0,
                                               "rate " + juce::String (rate) + ": lowest " + juce::String (range.first, 1) + " Hz");
                }
        }

        beginTest ("Depth 0: just a short fixed delay, no vibrato");
        {
            VibratoEngine::Settings s;
            s.depth = 0.0f;
            VibratoEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);
            juce::AudioBuffer<float> b (1, 1000);
            b.clear();
            b.setSample (0, 0, 1.0f);
            engine.process (b.getArrayOfWritePointers(), 1, 1000, {});
            expectWithinAbsoluteError (b.getSample (0, 24), 1.0f, 1.0e-6f, "0.5 ms = 24 samples");
        }

        beginTest ("Rise fades the vibrato in after a new note");
        {
            VibratoEngine::Settings s;
            s.depth = 1.0f;
            s.rateHz = 6.0f;
            s.riseSeconds = 1.0f;
            const auto early = pitchRange (s, 0.0, 0.3), late = pitchRange (s, 1.2, 2.0);
            expect (early.second - early.first < (late.second - late.first) * 0.5,
                    "early swing " + juce::String (early.second - early.first, 1) + " Hz, late " + juce::String (late.second - late.first, 1) + " Hz");
        }

        beginTest ("Extremes stay finite and bounded");
        {
            VibratoEngine engine;
            engine.prepare (192000.0);
            juce::Random rng (53);
            juce::AudioBuffer<float> buffer (2, 64);
            bool ok = true;
            for (int block = 0; block < 6000; ++block)
            {
                if (block % 5 == 0)
                {
                    VibratoEngine::Settings s;
                    s.rateHz = rng.nextBool() ? 0.1f : 15.0f;
                    s.depth = rng.nextFloat();
                    s.shape = rng.nextBool() ? Shape::sine : Shape::triangle;
                    s.sync = rng.nextBool();
                    s.division = rng.nextInt (rkfx::SyncDivision::kCount);
                    s.phaseDeg = rng.nextFloat() * 180.0f;
                    s.riseSeconds = rng.nextBool() ? 0.0f : 2.0f;
                    engine.setSettings (s);
                }
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);
                engine.process (buffer.getArrayOfWritePointers(), 2, 64, {});
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        ok = ok && std::isfinite (buffer.getSample (ch, i)) && std::abs (buffer.getSample (ch, i)) < 2.0f;
            }
            expect (ok);
        }
    }

private:
    /** Lowest and highest instantaneous frequency of a 1 kHz tone through the
        vibrato (from zero-crossing intervals), between two times in seconds. */
    static std::pair<double, double> pitchRange (const VibratoEngine::Settings& s, double from = 1.0, double to = 3.0)
    {
        VibratoEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);

        const int length = (int) (to * 48000.0) + 480;
        juce::AudioBuffer<float> b (1, length);
        for (int i = 0; i < length; ++i)
            b.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / 48000.0));
        engine.process (b.getArrayOfWritePointers(), 1, length, {});

        // Upward zero crossings, refined by linear interpolation.
        std::vector<double> crossings;
        for (int i = (int) (from * 48000.0); i < length - 1; ++i)
        {
            const auto a = b.getSample (0, i), c = b.getSample (0, i + 1);
            if (a < 0.0f && c >= 0.0f)
                crossings.push_back (i + a / (a - c));
        }

        double lo = 1.0e9, hi = 0.0;
        for (size_t k = 1; k < crossings.size(); ++k)
        {
            const auto f = 48000.0 / (crossings[k] - crossings[k - 1]);
            lo = std::min (lo, f);
            hi = std::max (hi, f);
        }
        return { lo, hi };
    }
};

static VibratoTests vibratoTests;
