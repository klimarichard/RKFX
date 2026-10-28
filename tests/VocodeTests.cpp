#include <juce_audio_basics/juce_audio_basics.h>

#include "VocodeEngine.h"
#include "dsp/StateVariableFilter.h"

#include <cmath>

/** RKVocode DSP: the output follows the modulator's spectrum, the carrier
    sets the pitch (fixed note, MIDI notes, sidechain), formant shift,
    silence without MIDI notes, level, stability. */
class VocodeTests : public juce::UnitTest
{
public:
    VocodeTests() : juce::UnitTest ("RKVocode engine") {}

    void runTest() override
    {
        using Carrier = VocodeEngine::Carrier;
        using Mode = VocodeEngine::PitchMode;
        using Wave = VocodeEngine::Wave;

        beginTest ("The output takes the modulator's spectrum (noise carrier)");
        {
            VocodeEngine::Settings s;
            s.wave = Wave::noise;
            s.unvoiced = 0.0f;
            const auto out = run (s, band (1000.0f, 0.3f));
            const auto at1k = bandLevel (out, 1000.0f), at4k = bandLevel (out, 4000.0f);
            expect (at1k > at4k * 10.0f, "1 kHz " + juce::String (at1k) + " vs 4 kHz " + juce::String (at4k));
        }

        beginTest ("The level is in the modulator's range");
        {
            VocodeEngine::Settings s;
            s.unvoiced = 0.0f;
            const auto in = noise (0.2f);
            const auto out = run (s, in);
            const auto ratioDb = juce::Decibels::gainToDecibels (out.getRMSLevel (0, 24000, 24000) / in.getRMSLevel (0, 24000, 24000));
            logMessage ("output vs modulator level: " + juce::String (ratioDb, 1) + " dB");
            expect (std::abs (ratioDb) < 9.0f, juce::String (ratioDb, 1) + " dB");
        }

        beginTest ("Fixed note: the carrier's pitch comes through");
        {
            VocodeEngine::Settings s;
            s.fixedNote = 57;   // A2 = 220 Hz
            s.unvoiced = 0.0f;
            const auto out = run (s, noise (0.2f));
            expect (toneLevel (out, 220.0) > 4.0 * toneLevel (out, 250.0), "220 Hz fundamental");
        }

        beginTest ("MIDI mode: silent with no notes, plays the held notes");
        {
            VocodeEngine::Settings s;
            s.pitchMode = Mode::midi;
            s.unvoiced = 0.0f;
            VocodeEngine engine;
            engine.setSettings (s);
            engine.prepare (48000.0);

            auto silent = noise (0.2f);
            engine.process (silent.getArrayOfWritePointers(), 1, silent.getNumSamples(), nullptr, 0);
            expect (silent.getMagnitude (0, 4800, 43200) < 1.0e-4f, "no notes: silent");

            engine.noteOn (57, 1.0f);   // 220 Hz
            engine.noteOn (64, 1.0f);   // 329.6 Hz
            auto chord = noise (0.2f);
            engine.process (chord.getArrayOfWritePointers(), 1, chord.getNumSamples(), nullptr, 0);
            expect (toneLevel (chord, 220.0) > 4.0 * toneLevel (chord, 250.0), "first note");
            expect (toneLevel (chord, 329.63) > 4.0 * toneLevel (chord, 300.0), "second note");

            engine.noteOff (57);
            engine.noteOff (64);
            auto after = noise (0.2f);
            engine.process (after.getArrayOfWritePointers(), 1, after.getNumSamples(), nullptr, 0);
            expect (after.getMagnitude (0, 4800, 43200) < 1.0e-4f, "notes off: silent again (below -80 dB)");
        }

        beginTest ("Sidechain carrier: no sidechain, no sound");
        {
            VocodeEngine::Settings s;
            s.carrier = Carrier::sidechain;
            s.unvoiced = 0.0f;
            const auto out = run (s, noise (0.2f));
            expect (out.getMagnitude (0, 4800, 43200) < 1.0e-6f);
        }

        beginTest ("Formant shift moves the spectrum");
        {
            VocodeEngine::Settings s;
            s.wave = Wave::noise;
            s.unvoiced = 0.0f;
            s.formantSemitones = 12.0f;
            const auto out = run (s, band (1000.0f, 0.3f));
            expect (bandLevel (out, 2000.0f) > bandLevel (out, 1000.0f) * 2.0f, "an octave up");
        }

        beginTest ("Changing the band count never clicks and extremes stay finite");
        {
            VocodeEngine engine;
            engine.prepare (44100.0);
            juce::Random rng (59);
            juce::AudioBuffer<float> buffer (2, 64), side (2, 64);
            bool ok = true;
            float peak = 0.0f;
            for (int block = 0; block < 5000; ++block)
            {
                if (block % 7 == 0)
                {
                    VocodeEngine::Settings s;
                    const int counts[] = { 8, 16, 24, 32 };
                    s.bands = counts[rng.nextInt (4)];
                    s.lowHz = rng.nextBool() ? 50.0f : 500.0f;
                    s.highHz = rng.nextBool() ? 4000.0f : 12000.0f;
                    s.formantSemitones = rng.nextBool() ? -12.0f : 12.0f;
                    s.attackMs = rng.nextBool() ? 1.0f : 50.0f;
                    s.releaseMs = rng.nextBool() ? 10.0f : 500.0f;
                    s.unvoiced = rng.nextFloat();
                    s.modulatorMix = rng.nextFloat();
                    s.carrier = rng.nextBool() ? Carrier::internal : Carrier::sidechain;
                    s.wave = (Wave) rng.nextInt (4);
                    s.pitchMode = rng.nextBool() ? Mode::midi : Mode::fixed;
                    s.voices = 1 + rng.nextInt (8);
                    s.glideMs = rng.nextBool() ? 0.0f : 500.0f;
                    engine.setSettings (s);
                    if (rng.nextBool()) engine.noteOn (36 + rng.nextInt (60), 1.0f);
                    else engine.noteOff (36 + rng.nextInt (60));
                }
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                    {
                        buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);
                        side.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);
                    }
                engine.process (buffer.getArrayOfWritePointers(), 2, 64, side.getArrayOfReadPointers(), 2);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                    {
                        const auto v = buffer.getSample (ch, i);
                        ok = ok && std::isfinite (v);
                        peak = std::max (peak, std::abs (v));
                    }
            }
            logMessage ("extremes peak: " + juce::String (peak));
            expect (ok, "finite");
            expect (peak < 20.0f, "peak " + juce::String (peak));
        }
    }

private:
    static juce::AudioBuffer<float> noise (float level)
    {
        juce::Random rng (21);
        juce::AudioBuffer<float> b (1, 48000);
        for (int i = 0; i < 48000; ++i)
            b.setSample (0, i, level * (rng.nextFloat() * 2.0f - 1.0f));
        return b;
    }

    /** Noise filtered around one frequency (a formant-like band). */
    static juce::AudioBuffer<float> band (float hz, float level)
    {
        auto b = noise (level * 4.0f);
        rkfx::StateVariableFilter f1, f2;
        f1.prepare (48000.0); f2.prepare (48000.0);
        f1.setCutoff (hz, 4.0f); f2.setCutoff (hz, 4.0f);
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (0, i, f2.processNormalisedBandpass (f1.processNormalisedBandpass (b.getSample (0, i))));
        return b;
    }

    static juce::AudioBuffer<float> run (const VocodeEngine::Settings& s, juce::AudioBuffer<float> b)
    {
        VocodeEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        engine.process (b.getArrayOfWritePointers(), 1, b.getNumSamples(), nullptr, 0);
        return b;
    }

    /** RMS of the output through a narrow band-pass at @p hz. */
    static float bandLevel (const juce::AudioBuffer<float>& b, float hz)
    {
        rkfx::StateVariableFilter f1, f2;
        f1.prepare (48000.0); f2.prepare (48000.0);
        f1.setCutoff (hz, 4.0f); f2.setCutoff (hz, 4.0f);
        double acc = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const auto y = f2.processNormalisedBandpass (f1.processNormalisedBandpass (b.getSample (0, i)));
            if (i >= 24000)
                acc += (double) y * y;
        }
        return (float) std::sqrt (acc / 24000.0);
    }

    static double toneLevel (const juce::AudioBuffer<float>& b, double hz)
    {
        const int start = b.getNumSamples() / 2, n = b.getNumSamples() - start;
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / 48000.0;
        const auto coeff = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
            const auto s0 = b.getSample (0, start + i) * win + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return 4.0 * std::sqrt (std::max (1.0e-30, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / n;
    }
};

static VocodeTests vocodeTests;
