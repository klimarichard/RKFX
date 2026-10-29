#include <juce_audio_basics/juce_audio_basics.h>

#include "HoldEngine.h"

#include <cmath>
#include <functional>
#include <vector>

namespace
{
    constexpr double kRate = 48000.0;
    constexpr double kTwoPi = 6.28318530717958647692;

    /** A violin-like tone: ten harmonics of @p hz, falling 6 dB per octave. */
    float harmonicTone (int i, double hz, float amplitude = 0.3f)
    {
        double x = 0.0;
        for (int h = 1; h <= 10; ++h)
            x += std::sin (kTwoPi * hz * h * i / kRate) / h;
        return amplitude * (float) x;
    }

    /** Runs @p seconds of a signal through the engine (stereo, the same on both
        sides); @p atSample calls happen before the block that contains them. */
    std::vector<float> run (HoldEngine& e, double seconds, const std::function<float (int)>& signal, int& clock,
                            const std::function<void (int)>& before = {})
    {
        std::vector<float> out;
        const auto total = (int) (seconds * kRate);
        juce::AudioBuffer<float> b (2, 256);
        for (int start = 0; start < total; start += 256)
        {
            if (before)
                before (clock);
            const auto n = juce::jmin (256, total - start);
            for (int i = 0; i < n; ++i)
            {
                const auto x = signal (clock + i);
                b.setSample (0, i, x);
                b.setSample (1, i, x);
            }
            e.process (b.getArrayOfWritePointers(), 2, n);
            for (int i = 0; i < n; ++i)
                out.push_back (b.getSample (0, i));
            clock += n;
        }
        return out;
    }

    /** Normalised autocorrelation of x at @p lag (fractional lags interpolated). */
    double autocorrelation (const std::vector<float>& x, double lag, size_t from, size_t count)
    {
        double xy = 0.0, xx = 0.0, yy = 0.0;
        const auto whole = (size_t) lag;
        const auto t = lag - (double) whole;
        for (size_t i = from; i < from + count; ++i)
        {
            const auto a = (double) x[i];
            const auto b = (1.0 - t) * x[i + whole] + t * x[i + whole + 1];
            xy += a * b;
            xx += a * a;
            yy += b * b;
        }
        return xy / std::sqrt (xx * yy + 1.0e-20);
    }

    /** The period near @p guess (samples) with the highest autocorrelation, to 0.01 sample. */
    double periodNear (const std::vector<float>& x, double guess, size_t from, size_t count)
    {
        auto best = guess;
        auto bestR = -2.0;
        for (double lag = guess * 0.97; lag <= guess * 1.03; lag += 0.01)
        {
            const auto r = autocorrelation (x, lag, from, count);
            if (r > bestR)
            {
                bestR = r;
                best = lag;
            }
        }
        return best;
    }

    double rmsDb (const std::vector<float>& x, size_t from, size_t count)
    {
        double sum = 0.0;
        for (size_t i = from; i < from + count; ++i)
            sum += (double) x[i] * x[i];
        return 10.0 * std::log10 (sum / (double) count + 1.0e-20);
    }

    double energyAt (const std::vector<float>& x, size_t from, size_t count, double hz)
    {
        const auto coef = 2.0 * std::cos (kTwoPi * hz / kRate);
        double s1 = 0.0, s2 = 0.0;
        for (size_t i = from; i < from + count; ++i)
        {
            const auto s0 = x[i] + coef * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return (s1 * s1 + s2 * s2 - coef * s1 * s2) / ((double) count * count);
    }

    HoldEngine makeEngine (HoldEngine::Settings s)
    {
        HoldEngine e;
        e.setSettings (s);
        e.prepare (kRate);
        return e;
    }

    int activeLayers (const HoldEngine& e)
    {
        int n = 0;
        for (int i = 0; i < HoldEngine::kLayers + 1; ++i)
            n += e.getLayer (i).active ? 1 : 0;
        return n;
    }
}

/** RKHold DSP: a held note keeps its pitch and level, octaves, texture
    holds of chords, auto mode, layers, the footswitch, silence, dry, clicks. */
class HoldTests : public juce::UnitTest
{
public:
    HoldTests() : juce::UnitTest ("RKHold engine") {}

    void runTest() override
    {
        beginTest ("A held note keeps its pitch and a steady level");
        for (const auto movement : { 0.0f, 0.4f, 1.0f })
        {
            HoldEngine::Settings s;
            s.movement = movement;
            s.dryGainDb = -60.0f;
            s.toneHz = 20000.0f;
            auto e = makeEngine (s);
            int clock = 0;
            run (e, 0.5, [] (int i) { return harmonicTone (i, 220.0); }, clock);
            expect (e.hold(), "held");
            expectEquals (e.getLayer (0).note, 57, "A (220 Hz) recognised as a note");
            const auto out = run (e, 3.0, [] (int) { return 0.0f; }, clock);

            const auto period = periodNear (out, kRate / 220.0, 24000, 48000);
            const auto cents = 1200.0 * std::log2 ((kRate / period) / 220.0);
            expect (std::abs (cents) < 3.0, "movement " + juce::String (movement) + ": " + juce::String (cents, 2) + " cents");

            double lowest = 1.0e9, highest = -1.0e9;
            for (size_t w = 24000; w + 2400 <= out.size(); w += 2400)
            {
                const auto db = rmsDb (out, w, 2400);
                lowest = std::min (lowest, db);
                highest = std::max (highest, db);
            }
            expect (highest - lowest < 2.0, "steady: " + juce::String (highest - lowest, 2) + " dB over 50 ms windows");
        }

        beginTest ("Octave -1 and +1");
        for (const auto octave : { -1, 1 })
        {
            HoldEngine::Settings s;
            s.movement = 0.0f;
            s.dryGainDb = -60.0f;
            s.toneHz = 20000.0f;
            s.octave = octave;
            auto e = makeEngine (s);
            int clock = 0;
            run (e, 0.5, [] (int i) { return harmonicTone (i, 220.0); }, clock);
            e.hold();
            const auto out = run (e, 2.0, [] (int) { return 0.0f; }, clock);
            const auto expectedHz = 220.0 * std::pow (2.0, octave);

            // The new fundamental is there; the old one isn't the period any more.
            const auto atNew = autocorrelation (out, kRate / expectedHz, 24000, 24000);
            expect (atNew > 0.8, "periodic at " + juce::String (expectedHz) + " Hz: " + juce::String (atNew, 3));
            const auto atOld = autocorrelation (out, kRate / 220.0 * (octave < 0 ? 1.0 : 0.75), 24000, 24000);
            expect (atNew > atOld + 0.05, "the period moved: " + juce::String (atNew, 3) + " vs " + juce::String (atOld, 3));
        }

        beginTest ("Texture holds a chord");
        {
            HoldEngine::Settings s;
            s.mode = HoldEngine::Mode::texture;
            s.dryGainDb = -60.0f;
            s.toneHz = 20000.0f;
            auto e = makeEngine (s);
            int clock = 0;
            auto chord = [] (int i) { return 0.2f * (float) (std::sin (kTwoPi * 300.0 * i / kRate) + std::sin (kTwoPi * 450.0 * i / kRate)); };
            run (e, 0.5, chord, clock);
            std::vector<float> input;
            for (int i = 0; i < 24000; ++i)
                input.push_back (chord (i));
            e.hold();
            expectEquals (e.getLayer (0).note, -1, "a texture");
            const auto out = run (e, 3.0, [] (int) { return 0.0f; }, clock);

            // In 20 ms windows: grains start at random places, so a tone's phase moves from
            // grain to grain; a long measurement would average it away although it's heard.
            auto shortTermEnergy = [] (const std::vector<float>& x, size_t from, size_t count, double hz)
            {
                double sum = 0.0;
                int windows = 0;
                for (size_t w = from; w + 960 <= from + count; w += 960, ++windows)
                    sum += energyAt (x, w, 960, hz);
                return sum / windows;
            };
            for (const auto hz : { 300.0, 450.0 })
            {
                const auto ratioDb = 10.0 * std::log10 (shortTermEnergy (out, 48000, 48000, hz) / shortTermEnergy (input, 4800, 19200, hz));
                expect (std::abs (ratioDb) < 3.0, juce::String (hz) + " Hz held within 3 dB: " + juce::String (ratioDb, 2));
            }
        }

        beginTest ("Auto: a note for a steady tone, a texture for a chord");
        {
            auto e = makeEngine ({});
            int clock = 0;
            run (e, 0.5, [] (int i) { return harmonicTone (i, 330.0); }, clock);
            e.hold();
            expect (e.getLayer (0).note >= 0, "note");

            auto c = makeEngine ({});
            int clock2 = 0;
            run (c, 0.5, [] (int i) { return 0.2f * (float) (std::sin (kTwoPi * 300.0 * i / kRate) + std::sin (kTwoPi * 437.0 * i / kRate)); }, clock2);
            c.hold();
            expectEquals (c.getLayer (0).note, -1, "texture");
        }

        beginTest ("Layers: Add builds up to four, Replace keeps one");
        {
            HoldEngine::Settings s;
            s.add = true;
            s.releaseMs = 50.0f;
            auto e = makeEngine (s);
            int clock = 0;
            run (e, 0.5, [] (int i) { return harmonicTone (i, 220.0); }, clock);
            for (int k = 0; k < 5; ++k)
            {
                e.press();
                run (e, 0.1, [] (int i) { return harmonicTone (i, 220.0); }, clock);
            }
            run (e, 0.2, [] (int) { return 0.0f; }, clock);
            expectEquals (activeLayers (e), 4, "the fifth replaced the oldest");

            s.add = false;
            auto r = makeEngine (s);
            int clock2 = 0;
            run (r, 0.5, [] (int i) { return harmonicTone (i, 220.0); }, clock2);
            r.hold();
            run (r, 0.1, [] (int i) { return harmonicTone (i, 220.0); }, clock2);
            r.hold();
            run (r, 0.2, [] (int) { return 0.0f; }, clock2);
            expectEquals (activeLayers (r), 1, "Replace");
        }

        beginTest ("The footswitch: Latch toggles, Momentary follows");
        {
            HoldEngine::Settings s;
            s.releaseMs = 50.0f;
            auto latch = makeEngine (s);
            int clock = 0;
            run (latch, 0.5, [] (int i) { return harmonicTone (i, 220.0); }, clock);
            latch.press();
            latch.lift();
            expect (latch.isHolding(), "Latch: still holding after the lift");
            latch.press();
            expect (! latch.isHolding(), "Latch: the second press releases");

            s.latch = false;
            auto momentary = makeEngine (s);
            int clock2 = 0;
            run (momentary, 0.5, [] (int i) { return harmonicTone (i, 220.0); }, clock2);
            momentary.press();
            expect (momentary.isHolding(), "Momentary: holding while down");
            momentary.lift();
            expect (! momentary.isHolding(), "Momentary: released on lift");
        }

        beginTest ("Silence holds nothing");
        {
            auto e = makeEngine ({});
            int clock = 0;
            run (e, 0.5, [] (int) { return 0.0f; }, clock);
            expect (! e.hold());
            expectEquals (activeLayers (e), 0);
        }

        beginTest ("The dry signal is untouched while nothing is held");
        {
            auto e = makeEngine ({});
            juce::AudioBuffer<float> b (2, 4800);
            juce::Random rng (3);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4800; ++i)
                    b.setSample (ch, i, rng.nextFloat() - 0.5f);
            const auto input = b;
            e.process (b.getArrayOfWritePointers(), 2, 4800);
            bool identical = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4800; ++i)
                    identical = identical && juce::exactlyEqual (b.getSample (ch, i), input.getSample (ch, i));
            expect (identical);
        }

        beginTest ("Hold, release, add and octave changes never click");
        {
            HoldEngine::Settings s;
            s.attackMs = 20.0f;
            s.releaseMs = 100.0f;
            s.add = true;
            auto e = makeEngine (s);
            int clock = 0;
            auto settings = s;
            const auto out = run (e, 4.0, [] (int i) { return harmonicTone (i, 220.0, 0.2f); }, clock, [&] (int at)
            {
                if (at == 24064)  e.hold();
                if (at == 48128)  e.hold();
                if (at == 72192)  { settings.octave = -1; e.setSettings (settings); }
                if (at == 96000)  { settings.octave = 1; e.setSettings (settings); }
                if (at == 120064) e.releaseAll();
            });

            float maxStep = 0.0f;
            for (size_t i = 1; i < out.size(); ++i)
                maxStep = std::max (maxStep, std::abs (out[i] - out[i - 1]));
            // The tone (0.2 of ten harmonics of 220 Hz) and two layers of it move up to about 0.17 per sample.
            expect (maxStep < 0.3f, "max step " + juce::String (maxStep));
        }
    }
};

static HoldTests holdTests;
