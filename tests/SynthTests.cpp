#include <juce_audio_basics/juce_audio_basics.h>

#include "SynthEngine.h"

#include <cmath>
#include <initializer_list>

/** RKSynth DSP: plays the tracked pitch (a bass's low E included, with
    strong harmonics), shifts octaves, adds the sub, stays silent below the
    threshold, the filter envelope opens on a new note, legato notes glide,
    and Dynamics sets how much the level follows the playing. */
class SynthTests : public juce::UnitTest
{
public:
    SynthTests() : juce::UnitTest ("RKSynth engine") {}

    void runTest() override
    {
        using Range = SynthEngine::Range;
        using Wave = SynthEngine::Wave;

        beginTest ("PolyBLEP is zero away from the edge");
        {
            expectEquals (SynthEngine::polyBlep (0.5f, 0.01f), 0.0f);
            expect (SynthEngine::polyBlep (0.001f, 0.01f) < 0.0f);
        }

        beginTest ("Plays the bass's note, an octave down on request");
        {
            for (const auto octave : { 0, -1 })
            {
                SynthEngine::Settings s;
                s.range = Range::bass;
                s.octave = octave;
                s.envAmount = 0.0f;
                s.cutoffHz = 2000.0f;
                SynthEngine engine;
                // A bass's low E: weak fundamental, strong harmonics.
                const auto out = run (s, tone (41.2, { { 1.0, 0.2f }, { 2.0, 0.3f }, { 3.0, 0.2f }, { 4.0, 0.1f } }, 96000), &engine);
                expectWithinAbsoluteError (engine.getPlayingHz(), 41.2f, 1.0f, "tracks the low E");
                const auto hz = 41.2 * (octave == 0 ? 1.0 : 0.5);
                const auto fundamental = amplitudeAfter (out, hz, 48000);
                expect (fundamental > 0.05, juce::String (hz) + " Hz: " + juce::String (fundamental));
                if (octave == -1)
                    expect (fundamental > amplitudeAfter (out, 41.2 * 1.5, 48000) * 2.0, "the octave below is the fundamental");
            }
        }

        beginTest ("Every range plays its notes");
        {
            struct Case { Range range; double hz; };
            for (const auto& c : { Case { Range::guitar, 196.0 }, Case { Range::violin, 659.3 }, Case { Range::bass, 98.0 } })
            {
                SynthEngine::Settings s;
                s.range = c.range;
                SynthEngine engine;
                run (s, tone (c.hz, { { 1.0, 0.3f }, { 2.0, 0.15f } }, 48000), &engine);
                expectWithinAbsoluteError (engine.getPlayingHz(), (float) c.hz, (float) c.hz * 0.02f);
            }
        }

        beginTest ("Sub adds the octave below");
        {
            SynthEngine::Settings s;
            s.wave = Wave::triangle;
            s.envAmount = 0.0f;
            s.cutoffHz = 5000.0f;
            const auto plain = amplitudeAfter (run (s, tone (220.0, { { 1.0, 0.3f } }, 48000)), 110.0, 24000);
            s.sub = 1.0f;
            const auto withSub = amplitudeAfter (run (s, tone (220.0, { { 1.0, 0.3f } }, 48000)), 110.0, 24000);
            expect (plain < 0.01 && withSub > 0.1, juce::String (plain) + " -> " + juce::String (withSub));
        }

        beginTest ("Silent below the threshold, and after the note");
        {
            SynthEngine::Settings s;
            const auto quiet = run (s, tone (220.0, { { 1.0, 0.001f } }, 24000));
            expect (quiet.getMagnitude (0, 0, 24000) < 1.0e-4f, "no synth from a -60 dB note");

            auto in = tone (220.0, { { 1.0, 0.3f } }, 24000);
            in.setSize (1, 48000, true, true);
            const auto out = run (s, in);
            expect (out.getMagnitude (0, 24000 + 14400, 9600) < 1.0e-3f, "silent 300 ms after the note");
        }

        beginTest ("The filter envelope opens on a new note and closes over the decay");
        {
            SynthEngine::Settings s;
            s.cutoffHz = 300.0f;
            s.envAmount = 1.0f;
            s.decayMs = 200.0f;
            s.dynamics = 0.0f;
            const auto out = run (s, tone (110.0, { { 1.0, 0.3f } }, 48000));
            // Harmonic 20 (2.2 kHz) is far above the cutoff once the envelope has decayed.
            const auto early = amplitudeIn (out, 110.0 * 20, 1200, 2400);
            const auto late = amplitudeIn (out, 110.0 * 20, 36000, 2400);
            expect (early > late * 4.0, "bright then dark: " + juce::String (early) + " vs " + juce::String (late));
        }

        beginTest ("Legato notes glide");
        {
            SynthEngine::Settings s;
            s.glideMs = 200.0f;
            auto in = tone (220.0, { { 1.0, 0.3f } }, 24000);
            in.setSize (1, 48000, true, true);
            const auto phase = 2.0 * juce::MathConstants<double>::pi * 220.0 * 24000.0 / 48000.0;
            for (int i = 0; i < 24000; ++i)
                in.setSample (0, 24000 + i, 0.3f * (float) std::sin (phase + 2.0 * juce::MathConstants<double>::pi * 330.0 * i / 48000.0));

            SynthEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);
            float midway = 0.0f;
            for (int start = 0; start < 48000; start += 64)
            {
                float* ch[] { in.getWritePointer (0, start) };
                engine.process (ch, 1, 64);
                if (start == 24000 + 4800)   // 100 ms after the change
                    midway = engine.getPlayingHz();
            }
            expect (midway > 230.0f && midway < 320.0f, "between the notes: " + juce::String (midway));
            expectWithinAbsoluteError (engine.getPlayingHz(), 330.0f, 5.0f, "arrives");
        }

        beginTest ("Dynamics: steady or following the playing");
        {
            for (const auto dynamics : { 0.0f, 1.0f })
            {
                SynthEngine::Settings s;
                s.dynamics = dynamics;
                s.envAmount = 0.0f;
                const auto soft = rms (run (s, tone (220.0, { { 1.0, 0.03f } }, 48000)));
                const auto loud = rms (run (s, tone (220.0, { { 1.0, 0.3f } }, 48000)));
                const auto spread = juce::Decibels::gainToDecibels (loud / soft);
                if (dynamics < 0.5f)
                    expect (spread < 1.0f, "steady: " + juce::String (spread) + " dB");
                else
                    expect (spread > 15.0f, "follows: " + juce::String (spread) + " dB");
            }
        }
    }

private:
    static juce::AudioBuffer<float> tone (double hz, std::initializer_list<std::pair<double, float>> partials, int n)
    {
        juce::AudioBuffer<float> b (1, n);
        for (int i = 0; i < n; ++i)
        {
            double x = 0.0;
            for (const auto& [harmonic, amp] : partials)
                x += amp * std::sin (2.0 * juce::MathConstants<double>::pi * hz * harmonic * i / 48000.0);
            b.setSample (0, i, (float) x);
        }
        return b;
    }

    static juce::AudioBuffer<float> run (const SynthEngine::Settings& s, juce::AudioBuffer<float> buffer, SynthEngine* engine = nullptr)
    {
        SynthEngine local;
        auto& e = engine != nullptr ? *engine : local;
        e.setSettings (s);
        e.prepare (48000.0);
        for (int start = 0; start < buffer.getNumSamples(); start += 64)
        {
            float* ch[] { buffer.getWritePointer (0, start) };
            e.process (ch, 1, juce::jmin (64, buffer.getNumSamples() - start));
        }
        return buffer;
    }

    static float rms (const juce::AudioBuffer<float>& b)
    {
        const auto half = b.getNumSamples() / 2;
        return b.getRMSLevel (0, half, half);
    }

    static double amplitudeAfter (const juce::AudioBuffer<float>& b, double hz, int n)
    {
        return amplitudeIn (b, hz, b.getNumSamples() - n, n);
    }

    /** Amplitude at @p hz in [start, start + n) (Goertzel, Hann window). */
    static double amplitudeIn (const juce::AudioBuffer<float>& b, double hz, int start, int n)
    {
        const auto* data = b.getReadPointer (0, start);
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / 48000.0;
        const auto coeff = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
            const auto s0 = data[i] * win + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return 2.0 * std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / (n * 0.5);
    }
};

static SynthTests synthTests;
