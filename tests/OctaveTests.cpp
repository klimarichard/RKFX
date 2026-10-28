#include <juce_audio_basics/juce_audio_basics.h>

#include "OctaveEngine.h"

#include <cmath>
#include <initializer_list>

/** RKOctave DSP: tracks the fundamental of each range (also with strong
    harmonics), divides by two and four, follows pitch changes, stays
    silent below the threshold, passes Direct unchanged, never clicks. */
class OctaveTests : public juce::UnitTest
{
public:
    OctaveTests() : juce::UnitTest ("RKOctave engine") {}

    void runTest() override
    {
        using Range = OctaveEngine::Range;
        using Voice = OctaveEngine::Voice;

        beginTest ("Direct alone passes the input unchanged");
        {
            OctaveEngine::Settings s;
            s.octave1 = 0.0f;
            auto in = tone ({ { 1.0, 0.5f } }, 220.0, 9600);
            auto out = run (s, in);
            float maxError = 0.0f;
            for (int i = 0; i < in.getNumSamples(); ++i)
                maxError = juce::jmax (maxError, std::abs (out.getSample (0, i) - in.getSample (0, i)));
            expect (maxError < 1.0e-6f, "max error " + juce::String (maxError));
        }

        beginTest ("Synth: one and two octaves below a sine");
        {
            for (const auto two : { false, true })
            {
                OctaveEngine::Settings s;
                s.voice = Voice::synth;
                s.direct = 0.0f;
                s.octave1 = two ? 0.0f : 1.0f;
                s.octave2 = two ? 1.0f : 0.0f;
                OctaveEngine engine;
                const auto out = run (s, tone ({ { 1.0, 0.5f } }, 220.0, 48000), &engine);

                const auto target = two ? 55.0 : 110.0;
                const auto wanted = amplitudeAfter (out, target, 9600);
                expectWithinAbsoluteError (engine.getTrackedHz(), 220.0f, 3.0f, "tracks 220 Hz");
                expect (wanted > 0.2f, juce::String (target) + " Hz: " + juce::String (wanted));
                for (const auto other : { 220.0, two ? 110.0 : 440.0 })
                    expect (amplitudeAfter (out, other, 9600) < wanted * 0.1f, juce::String (other) + " Hz is weak");
            }
        }

        beginTest ("Natural: the input's own sound an octave down");
        {
            OctaveEngine::Settings s;
            s.direct = 0.0f;
            s.octave1 = 1.0f;
            s.toneHz = 8000.0f;
            const auto out = run (s, tone ({ { 1.0, 0.5f } }, 220.0, 48000));
            const auto sub = amplitudeAfter (out, 110.0, 9600);
            expect (sub > 0.1f, "110 Hz: " + juce::String (sub));
            expect (amplitudeAfter (out, 220.0, 9600) < sub * 0.3f, "little left at 220 Hz");
        }

        beginTest ("Tracks the fundamental in every range, also with strong harmonics");
        {
            struct Case { Range range; double hz; std::initializer_list<std::pair<double, float>> partials; const char* what; };
            const Case cases[] {
                { Range::bass,   41.2,   { { 1.0, 0.5f } }, "bass E1" },
                { Range::guitar, 82.4,   { { 1.0, 0.3f }, { 2.0, 0.2f }, { 3.0, 0.1f }, { 4.0, 0.08f } }, "guitar low E" },
                { Range::violin, 196.0,  { { 1.0, 0.15f }, { 2.0, 0.3f }, { 3.0, 0.2f }, { 4.0, 0.15f }, { 5.0, 0.1f } }, "violin G, weak fundamental" },
                { Range::violin, 130.8,  { { 1.0, 0.2f }, { 2.0, 0.3f }, { 3.0, 0.2f }, { 4.0, 0.1f } }, "5-string violin low C" },
                { Range::guitar, 65.4,   { { 1.0, 0.3f }, { 2.0, 0.2f }, { 3.0, 0.1f } }, "cello C2" },
                { Range::violin, 659.3,  { { 1.0, 0.3f }, { 2.0, 0.15f }, { 3.0, 0.1f } }, "violin E5" },
                { Range::violin, 1318.5, { { 1.0, 0.3f }, { 2.0, 0.1f } }, "violin E6" },
            };

            for (const auto& c : cases)
            {
                OctaveEngine::Settings s;
                s.range = c.range;
                s.voice = Voice::synth;
                s.direct = 0.0f;
                OctaveEngine engine;
                const auto out = run (s, tone (c.partials, c.hz, 48000), &engine);
                expectWithinAbsoluteError (engine.getTrackedHz(), (float) c.hz, (float) c.hz * 0.03f, c.what);
                expect (amplitudeAfter (out, c.hz * 0.5, 24000) > 3.0f * amplitudeAfter (out, c.hz, 24000),
                        juce::String (c.what) + ": the octave below dominates");
            }
        }

        beginTest ("Follows legato pitch changes, up and down");
        {
            for (const auto& [first, second] : { std::pair { 220.0, 330.0 }, std::pair { 440.0, 220.0 } })
            {
                auto in = tone ({ { 1.0, 0.5f } }, first, 24000);
                in.setSize (1, 48000, true, true);
                // Continue in phase, so there is no gap or step between the notes.
                const auto phase = 2.0 * juce::MathConstants<double>::pi * first * 24000.0 / 48000.0;
                for (int i = 0; i < 24000; ++i)
                    in.setSample (0, 24000 + i, 0.5f * (float) std::sin (phase + 2.0 * juce::MathConstants<double>::pi * second * i / 48000.0));

                OctaveEngine::Settings s;
                s.voice = Voice::synth;
                OctaveEngine engine;
                run (s, in, &engine);
                expectWithinAbsoluteError (engine.getTrackedHz(), (float) second, (float) second * 0.02f,
                                           juce::String (first) + " -> " + juce::String (second));
            }
        }

        beginTest ("Below the threshold the octaves are silent");
        {
            OctaveEngine::Settings s;
            s.direct = 0.0f;
            s.thresholdDb = -50.0f;
            OctaveEngine engine;
            const auto quiet = run (s, tone ({ { 1.0, 0.001f } }, 220.0, 9600), &engine);   // -60 dB
            expect (quiet.getMagnitude (0, 0, 9600) < 1.0e-6f, "no octave from a -60 dB note");
            expectEquals (engine.getTrackedHz(), 0.0f);

            // A note, then silence: the octave fades out with the note.
            auto in = tone ({ { 1.0, 0.5f } }, 220.0, 9600);
            in.setSize (1, 19200, true, true);
            const auto out = run (s, in);
            expect (out.getMagnitude (0, 9600 + 2400, 4800) < 1.0e-3f, "silent 50 ms after the note");
        }

        beginTest ("No clicks at the flip-flop edges");
        {
            for (const auto voice : { Voice::natural, Voice::synth })
            {
                OctaveEngine::Settings s;
                s.voice = voice;
                s.direct = 0.0f;
                s.octave1 = 1.0f;
                s.octave2 = 1.0f;
                s.toneHz = 8000.0f;
                const auto out = run (s, tone ({ { 1.0, 0.5f } }, 220.0, 24000));
                float maxStep = 0.0f;
                for (int i = 4800; i < out.getNumSamples(); ++i)
                    maxStep = juce::jmax (maxStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
                expect (maxStep < 0.06f, "largest step " + juce::String (maxStep));
            }
        }
    }

private:
    /** A tone of @p partials (harmonic number, amplitude) at @p hz, 48 kHz. */
    static juce::AudioBuffer<float> tone (std::initializer_list<std::pair<double, float>> partials, double hz, int n)
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

    static juce::AudioBuffer<float> run (const OctaveEngine::Settings& s, juce::AudioBuffer<float> buffer,
                                         OctaveEngine* engine = nullptr)
    {
        OctaveEngine local;
        auto& e = engine != nullptr ? *engine : local;
        e.setSettings (s);
        e.prepare (48000.0);
        for (int start = 0; start < buffer.getNumSamples(); start += 64)
        {
            const auto n = juce::jmin (64, buffer.getNumSamples() - start);
            float* ch[] { buffer.getWritePointer (0, start) };
            e.process (ch, 1, n);
        }
        return buffer;
    }

    /** Amplitude at @p hz over the last @p n samples. */
    static float amplitudeAfter (const juce::AudioBuffer<float>& b, double hz, int n)
    {
        return amplitude (b.getReadPointer (0, b.getNumSamples() - n), n, hz, 48000.0);
    }

    /** Amplitude of a sine at @p hz in @p data (Goertzel, Hann window). */
    static float amplitude (const float* data, int n, double hz, double rate)
    {
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / rate;
        const auto coeff = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const auto win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
            const auto s0 = data[i] * win + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const auto power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        return (float) (2.0 * std::sqrt (juce::jmax (0.0, power)) / (n * 0.5));
    }
};

static OctaveTests octaveTests;
