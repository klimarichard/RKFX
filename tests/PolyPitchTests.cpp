#include <juce_audio_basics/juce_audio_basics.h>

#include "PolyPitchEngine.h"
#include "dsp/PitchShifter.h"

#include <cmath>
#include <functional>
#include <vector>

namespace
{
    constexpr double kRate = 48000.0;
    constexpr double kTwoPi = 6.28318530717958647692;

    std::vector<float> run (PolyPitchEngine& e, double seconds, const std::function<float (int)>& signal,
                            const std::function<void (int)>& before = {})
    {
        std::vector<float> out;
        const auto total = (int) (seconds * kRate);
        juce::AudioBuffer<float> b (2, 256);
        for (int start = 0; start < total; start += 256)
        {
            if (before)
                before (start);
            const auto n = juce::jmin (256, total - start);
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    b.setSample (ch, i, signal (start + i));
            e.process (b.getArrayOfWritePointers(), 2, n);
            for (int i = 0; i < n; ++i)
                out.push_back (b.getSample (0, i));
        }
        return out;
    }


    /** The strongest frequency within 5% of @p guess (Goertzel over the whole span, 0.1% steps). */
    double peakFrequency (const std::vector<float>& x, size_t from, size_t count, double guess)
    {
        auto best = guess;
        auto bestEnergy = -1.0;
        for (double hz = guess * 0.95; hz <= guess * 1.05; hz *= 1.001)
        {
            const auto coef = 2.0 * std::cos (kTwoPi * hz / kRate);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = from; i < from + count; ++i)
            {
                const auto s0 = x[i] + coef * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            const auto energy = s1 * s1 + s2 * s2 - coef * s1 * s2;
            if (energy > bestEnergy)
            {
                bestEnergy = energy;
                best = hz;
            }
        }
        return best;
    }

    /** Average energy at @p hz in 20 ms windows (granular voices move in phase). */
    double shortTermEnergy (const std::vector<float>& x, size_t from, size_t count, double hz)
    {
        const auto coef = 2.0 * std::cos (kTwoPi * hz / kRate);
        double total = 0.0;
        int windows = 0;
        for (size_t w = from; w + 960 <= from + count; w += 960, ++windows)
        {
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = w; i < w + 960; ++i)
            {
                const auto s0 = x[i] + coef * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            total += (s1 * s1 + s2 * s2 - coef * s1 * s2) / (960.0 * 960.0);
        }
        return total / windows;
    }

    PolyPitchEngine makeEngine (const PolyPitchEngine::Settings& s)
    {
        PolyPitchEngine e;
        e.setSettings (s);
        e.prepare (kRate);
        return e;
    }

    PolyPitchEngine::Settings fixedShift (float semitones)
    {
        PolyPitchEngine::Settings s;
        s.heel = s.toe = semitones;
        s.glideMs = 0.0f;
        return s;
    }

    float sine (int i, double hz, float amplitude = 0.4f) { return amplitude * (float) std::sin (kTwoPi * hz * i / kRate); }
}

/** RKPolyPitch: pitch across the whole range, chords, the stated delays,
    the dry signal untouched in Harmony and Detune, the pedal and the
    momentary switch, no clicks, and the shimmer shifter unchanged. */
class PolyPitchTests : public juce::UnitTest
{
public:
    PolyPitchTests() : juce::UnitTest ("RKPolyPitch engine") {}

    void runTest() override
    {
        beginTest ("Whammy shifts by the set interval, from -24 to +24");
        for (const auto semitones : { -24.0f, -12.0f, -5.0f, 0.0f, 7.0f, 12.0f, 24.0f })
        {
            auto e = makeEngine (fixedShift (semitones));
            const auto out = run (e, 1.0, [] (int i) { return sine (i, 440.0); });
            const auto expected = 440.0 * std::pow (2.0, semitones / 12.0);
            // The spectrum peak: zero crossings miscount around the shifter's crossfades.
            expectWithinAbsoluteError (peakFrequency (out, 12000, 36000, expected), expected, expected * 0.01, juce::String (semitones) + " st");
        }

        beginTest ("A chord keeps both notes, each shifted");
        {
            auto e = makeEngine (fixedShift (12.0f));
            auto chord = [] (int i) { return sine (i, 300.0, 0.25f) + sine (i, 450.0, 0.25f); };
            const auto out = run (e, 1.5, chord);
            std::vector<float> input;
            for (int i = 0; i < 72000; ++i)
                input.push_back (chord (i));
            for (const auto hz : { 300.0, 450.0 })
            {
                const auto db = 10.0 * std::log10 (shortTermEnergy (out, 24000, 48000, hz * 2.0) / shortTermEnergy (input, 24000, 48000, hz));
                logMessage (juce::String (hz) + " Hz an octave up: " + juce::String (db, 2) + " dB");
                expect (std::abs (db) < 3.0, juce::String (hz) + " Hz an octave up: " + juce::String (db, 2) + " dB");
            }
        }

        beginTest ("The delay matches what the display states, per range and with Low Latency");
        for (const auto range : { PolyPitchEngine::Range::violin, PolyPitchEngine::Range::guitar, PolyPitchEngine::Range::bass })
            for (const auto low : { false, true })
            {
                auto s = fixedShift (0.0f);
                s.range = range;
                s.lowLatency = low;
                auto e = makeEngine (s);
                // A short burst; the output's energy centre against the input's.
                const auto burstAt = 9600;
                auto burst = [] (int i)
                {
                    const auto t = i - 9600;
                    return t < 0 || t >= 480 ? 0.0f : sine (t, 1000.0) * (float) (0.5 - 0.5 * std::cos (kTwoPi * t / 480.0));
                };
                const auto out = run (e, 0.5, burst);
                double energy = 0.0, weighted = 0.0;
                for (size_t i = 0; i < out.size(); ++i)
                {
                    energy += (double) out[i] * out[i];
                    weighted += (double) out[i] * out[i] * (double) i;
                }
                const auto measuredMs = (weighted / energy - (burstAt + 240)) * 1000.0 / kRate;
                const auto statedMs = PolyPitchEngine::delayMs (range, low);
                expectWithinAbsoluteError (measuredMs, (double) statedMs, 0.35 * statedMs + 1.0,
                                           "range " + juce::String ((int) range) + (low ? " low" : "") + ": " + juce::String (measuredMs, 2)
                                           + " ms, stated " + juce::String (statedMs, 2));
            }

        beginTest ("Harmony and Detune leave the dry signal untouched");
        for (const auto mode : { PolyPitchEngine::Mode::harmony, PolyPitchEngine::Mode::detune })
        {
            auto s = fixedShift (7.0f);
            s.mode = mode;
            s.voiceGainDb = -60.0f;
            auto e = makeEngine (s);
            juce::AudioBuffer<float> b (2, 9600);
            juce::Random rng (2);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 9600; ++i)
                    b.setSample (ch, i, rng.nextFloat() - 0.5f);
            const auto input = b;
            e.process (b.getArrayOfWritePointers(), 2, 9600);
            bool identical = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 9600; ++i)
                    identical = identical && juce::exactlyEqual (b.getSample (ch, i), input.getSample (ch, i));
            expect (identical);
        }

        beginTest ("The pedal sets the shift between heel and toe; Momentary rises and falls");
        {
            PolyPitchEngine::Settings s;
            s.heel = 0.0f;
            s.toe = 12.0f;
            s.pedal = 0.5f;
            s.glideMs = 30.0f;
            auto e = makeEngine (s);
            run (e, 0.3, [] (int i) { return sine (i, 440.0); });
            expectWithinAbsoluteError (e.getShift(), 6.0f, 0.05f, "half way: a tritone");

            s.pedal = 0.0f;
            s.momentary = true;
            s.riseMs = 50.0f;
            s.fallMs = 150.0f;
            auto m = makeEngine (s);
            m.setSettings (s);
            auto settings = s;
            settings.momentary = false;
            m.setSettings (settings);
            m.reset();
            settings.momentary = true;
            m.setSettings (settings);
            run (m, 0.025, [] (int i) { return sine (i, 440.0); });
            expectWithinAbsoluteError (m.getShift(), 6.0f, 0.5f, "half way up after half the Rise");
            run (m, 0.05, [] (int i) { return sine (i, 440.0); });
            expectWithinAbsoluteError (m.getShift(), 12.0f, 0.05f, "at Toe after Rise");
            settings.momentary = false;
            m.setSettings (settings);
            run (m, 0.16, [] (int i) { return sine (i, 440.0); });
            expectWithinAbsoluteError (m.getShift(), 0.0f, 0.05f, "back after Fall");
        }

        beginTest ("Pedal sweeps, Momentary, mode and range changes never click");
        {
            PolyPitchEngine::Settings s;
            s.glideMs = 30.0f;
            auto e = makeEngine (s);
            auto settings = s;
            auto chord = [] (int i) { return sine (i, 220.0, 0.2f) + sine (i, 330.0, 0.2f); };
            const auto out = run (e, 4.0, chord, [&] (int at)
            {
                if (at < 48000)              { settings.pedal = (float) at / 48000.0f; e.setSettings (settings); }
                if (at == 48128)             { settings.momentary = true; e.setSettings (settings); }
                if (at == 72192)             { settings.momentary = false; e.setSettings (settings); }
                if (at == 96000)             { settings.mode = PolyPitchEngine::Mode::harmony; e.setSettings (settings); }
                if (at == 120064)            { settings.range = PolyPitchEngine::Range::violin; e.setSettings (settings); }
                if (at == 144128)            { settings.mode = PolyPitchEngine::Mode::detune; e.setSettings (settings); }
                if (at == 168192)            { settings.lowLatency = true; settings.mode = PolyPitchEngine::Mode::whammy; e.setSettings (settings); }
            });
            float maxStep = 0.0f;
            for (size_t i = 1; i < out.size(); ++i)
                maxStep = std::max (maxStep, std::abs (out[i] - out[i - 1]));
            // Two notes at 0.2, up to an octave up plus the dry signal, move up to about 0.12 per sample.
            expect (maxStep < 0.2f, "max step " + juce::String (maxStep));
        }

        beginTest ("The shimmer shifter is unchanged by the configurable windows");
        {
            rkfx::PitchShifter a, b;
            a.prepare (kRate);
            b.prepare (kRate, 50.0f, 35.0f);
            a.setRatio (2.0f);
            b.setRatio (2.0f);
            bool identical = true;
            for (int i = 0; i < 48000; ++i)
            {
                const auto x = sine (i, 440.0);
                identical = identical && juce::exactlyEqual (a.process (x), b.process (x));
            }
            expect (identical);
            expectEquals (a.latencySamples(), 1022, "50 and 35 ms at 48 kHz, as before");
        }
    }
};

static PolyPitchTests polyPitchTests;
