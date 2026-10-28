#include <juce_audio_basics/juce_audio_basics.h>

#include "VoiceEngine.h"

#include <cmath>
#include <functional>
#include <initializer_list>

/** RKVoice DSP: the chord ranks of the design's table, Scale mode as
    RKHarmonize, shifted voices on pitch, the dry path untouched, No Chord
    (silent, latch, scale), the sustain pedal, the sung note's hysteresis,
    how soon the voices enter, and a click-free fade when the note stops. */
class VoiceTests : public juce::UnitTest
{
public:
    VoiceTests() : juce::UnitTest ("RKVoice engine") {}

    void runTest() override
    {
        using E = VoiceEngine;

        beginTest ("Chord ranks: the design's table (C-E-G held)");
        {
            const auto ceg = chordOf ({ 60, 64, 67 });
            // Singing D4 (62).
            expectEquals (E::chordNote (62, 1, ceg, E::ChordTones::anyOctave), 64);    // E
            expectEquals (E::chordNote (62, 2, ceg, E::ChordTones::anyOctave), 67);    // G
            expectEquals (E::chordNote (62, 3, ceg, E::ChordTones::anyOctave), 72);    // C, an octave up
            expectEquals (E::chordNote (62, -1, ceg, E::ChordTones::anyOctave), 60);   // C
            expectEquals (E::chordNote (62, -2, ceg, E::ChordTones::anyOctave), 55);   // G below
            expectEquals (E::chordNote (62, -3, ceg, E::ChordTones::anyOctave), 52);   // E below
            // Singing E4 (64), a chord tone: skipped.
            expectEquals (E::chordNote (64, 1, ceg, E::ChordTones::anyOctave), 67);
            expectEquals (E::chordNote (64, 2, ceg, E::ChordTones::anyOctave), 72);
            expectEquals (E::chordNote (64, 3, ceg, E::ChordTones::anyOctave), 76);
            expectEquals (E::chordNote (64, -1, ceg, E::ChordTones::anyOctave), 60);
            expectEquals (E::chordNote (64, -2, ceg, E::ChordTones::anyOctave), 55);
            expectEquals (E::chordNote (64, -3, ceg, E::ChordTones::anyOctave), 52);
            expectEquals (E::chordNote (62, 0, ceg, E::ChordTones::anyOctave), -1, "Off");
        }

        beginTest ("Chord ranks: as voiced, one-note chords, an empty chord, the MIDI range");
        {
            const auto ceg = chordOf ({ 60, 64, 67 });
            expectEquals (E::chordNote (62, 1, ceg, E::ChordTones::asVoiced), 64);
            expectEquals (E::chordNote (62, 3, ceg, E::ChordTones::asVoiced), -1, "nothing held that far up");
            expectEquals (E::chordNote (70, 1, ceg, E::ChordTones::asVoiced), -1, "nothing held above");
            expectEquals (E::chordNote (62, -1, ceg, E::ChordTones::asVoiced), 60);
            expectEquals (E::chordNote (62, -2, ceg, E::ChordTones::asVoiced), -1, "nothing held below C");

            const auto c = chordOf ({ 48 });
            expectEquals (E::chordNote (62, 1, c, E::ChordTones::anyOctave), 72, "one note: the C above");
            expectEquals (E::chordNote (62, -1, c, E::ChordTones::anyOctave), 60, "and the C below");
            expectEquals (E::chordNote (62, 1, E::NoteSet {}, E::ChordTones::anyOctave), -1, "empty chord");
            expectEquals (E::chordNote (125, 1, ceg, E::ChordTones::anyOctave), 127, "G9, the top note");
            expectEquals (E::chordNote (125, 2, ceg, E::ChordTones::anyOctave), -1, "past 127");
            expectEquals (E::chordNote (2, -1, ceg, E::ChordTones::anyOctave), 0, "down to 0");
        }

        beginTest ("Scale mode harmonizes as RKHarmonize");
        {
            auto s = quiet();
            s.mode = E::Mode::scale;
            s.key = 0;
            s.scale = E::Scale::major;
            s.voices[0].interval = E::Interval::thirdAbove;
            s.voices[1].interval = E::Interval::fifthBelow;
            E engine;
            run (engine, s, sine (220.0, 0.5, 48000));   // A3 (57)
            expectEquals (engine.getSungNote(), 57);
            expectEquals (engine.getVoiceNote (0), rkfx::scales::harmonyNote (57, 0, E::Scale::major, E::Interval::thirdAbove));
            expectEquals (engine.getVoiceNote (1), rkfx::scales::harmonyNote (57, 0, E::Scale::major, E::Interval::fifthBelow));
            expectEquals (engine.getVoiceNote (0), 60, "a 3rd above A in C major is C");
        }

        beginTest ("Shifted voices are on pitch (a voice-like tone, and a sine for shifts under an octave)");
        {
            // Singing A3; one voice, the first chord tone above or below a single held note.
            // PSOLA rebuilds a pitch from the input's periods, so a voice (rich in
            // harmonics) shifts any amount; a pure sine only shifts cleanly by less
            // than an octave, and no voice or instrument is one.
            struct Case { int heldNote, position, target; };
            for (const auto& c : { Case { 64, 1, 64 }, Case { 62, 1, 62 }, Case { 69, 1, 69 }, Case { 76, 2, 76 },   // +7, +5, +12, +19 (the 2nd E above)
                                   Case { 52, -1, 52 }, Case { 53, -1, 53 }, Case { 45, -1, 45 } })                   // -5, -4, -12
                for (const auto rich : { true, false })
                {
                    if (! rich && std::abs (c.target - 57) >= 12)
                        continue;
                    auto s = quiet();
                    s.voices[0].position = c.position;
                    E engine;
                    engine.noteOn (c.heldNote);
                    const auto input = rich ? voiceLike (220.0, 48000) : sine (220.0, 0.5, 48000);
                    const auto out = run (engine, s, input);
                    const auto expected = 440.0 * std::exp2 ((c.target - 69) / 12.0);
                    const auto measured = pitchOf (out, 14400, 48000, expected);
                    const auto cents = 1200.0 * std::log2 (measured / expected);
                    const auto harmonics = rich;
                    expect (std::abs (cents) < 5.0, juce::String (harmonics ? "voice-like tone" : "sine") + " to note " + juce::String (c.target)
                                                    + ": " + juce::String (measured, 2) + " Hz, " + juce::String (cents, 2) + " cents");
                    expectEquals (engine.getVoiceNote (0), c.target);
                }
        }

        beginTest ("The dry path is the input, sample for sample");
        {
            E::Settings s;
            for (auto& v : s.voices)
                v.position = 0;
            E engine;
            engine.prepare (48000.0);
            engine.setSettings (s);
            engine.reset();
            juce::AudioBuffer<float> buffer (2, 4800);
            juce::Random random (7);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    buffer.setSample (ch, i, random.nextFloat() * 0.6f - 0.3f);
            juce::AudioBuffer<float> original (buffer);
            for (int pos = 0; pos < buffer.getNumSamples(); pos += 64)
            {
                float* io[2] = { buffer.getWritePointer (0, pos), buffer.getWritePointer (1, pos) };
                engine.process (io, 2, 64);
            }
            bool same = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    same = same && buffer.getSample (ch, i) == original.getSample (ch, i);
            expect (same);
        }

        beginTest ("No chord held: silent by default, or the last chord, or the scale");
        {
            auto s = quiet();
            s.voices[0].position = 1;
            s.voices[0].interval = E::Interval::fifthAbove;
            {
                E engine;   // never a chord, Silent
                const auto out = run (engine, s, sine (220.0, 0.5, 48000));
                expect (out.getRMSLevel (0, 24000, 24000) < 1.0e-4f, "silent");
                expectEquals (engine.getVoiceNote (0), -1);
            }
            for (const auto noChord : { E::NoChord::latch, E::NoChord::scale })
            {
                s.noChord = noChord;
                E engine;
                engine.noteOn (64);
                run (engine, s, sine (220.0, 0.25, 48000));
                engine.noteOff (64);   // the chord is gone
                const auto out = run (engine, s, sine (220.0, 0.5, 48000), false);
                expect (out.getRMSLevel (0, 12000, 12000) > 0.05f, noChord == E::NoChord::latch ? "latched" : "scale");
                expectEquals (engine.getVoiceNote (0), noChord == E::NoChord::latch ? 64 : 64);   // E above A; a 5th above A3 in C major is E
            }
        }

        beginTest ("The sustain pedal keeps released notes in the chord");
        {
            auto s = quiet();
            s.voices[0].position = 1;
            E engine;
            engine.noteOn (64);
            engine.setSustain (true);
            engine.noteOff (64);
            run (engine, s, sine (220.0, 0.5, 48000));
            expectEquals (engine.getVoiceNote (0), 64, "held by the pedal");
            engine.setSustain (false);
            const auto out = run (engine, s, sine (220.0, 0.25, 48000), false);
            expect (out.getRMSLevel (0, 6000, 6000) < 1.0e-4f, "pedal up: silent");
        }

        beginTest ("Vibrato doesn't move the sung note; a real step does");
        {
            auto s = quiet();
            s.voices[0].position = 1;
            E engine;
            engine.noteOn (64);
            engine.prepare (48000.0);
            engine.setSettings (s);
            engine.reset();
            engine.noteOn (64);
            bool steady = true;
            // A3 with +-60 cents of vibrato at 5 Hz for a second.
            const auto vibrato = [] (double t) { return 220.0 * std::exp2 (0.6 / 12.0 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * t)); };
            processSweep (engine, vibrato, 0.0, 1.0, [&] (double t) { if (t > 0.1 && engine.getSungNote() != 57) steady = false; });
            expect (steady, "stays on A3");
            // Then a semitone up to Bb3 (58): within 100 ms.
            int changedAt = -1;
            processSweep (engine, [] (double) { return 233.08; }, 1.0, 1.3,
                          [&] (double t) { if (changedAt < 0 && engine.getSungNote() == 58) changedAt = (int) ((t - 1.0) * 1000.0); });
            expect (changedAt >= 0 && changedAt < 100, "Bb3 after " + juce::String (changedAt) + " ms");
        }

        beginTest ("The voices enter soon after the note and fade out without clicks");
        {
            auto s = quiet();
            s.voices[0].position = 1;
            E engine;
            engine.noteOn (69);   // an A: an octave above the sung A4
            juce::AudioBuffer<float> input (1, 48000);
            input.clear();
            auto note = voiceLike (440.0, 24000);
            note.applyGainRamp (0, 0, 960, 0.0f, 1.0f);              // a sung note's 20 ms attack
            note.applyGainRamp (0, 24000 - 240, 240, 1.0f, 0.0f);   // and its end over 5 ms
            input.copyFrom (0, 4800, note, 0, 0, 24000);
            const auto out = run (engine, s, input);
            // Steady level, then the first sample above half of it.
            float steady = 0.0f, steadyStep = 0.0f;
            for (int i = 14400; i < 24000; ++i)
            {
                steady = juce::jmax (steady, std::abs (out.getSample (0, i)));
                steadyStep = juce::jmax (steadyStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
            }
            int onset = -1;
            for (int i = 4800; i < 14400 && onset < 0; ++i)
                if (std::abs (out.getSample (0, i)) > 0.5f * steady)
                    onset = i - 4800;
            const auto onsetMs = onset * 1000.0 / 48000.0;
            logMessage ("    voice enters " + juce::String (onsetMs, 1) + " ms after the note");
            // The tracker settling on the note (about 15 ms with a sung attack), then the 20 ms fade.
            expect (onset >= 0 && onsetMs < 50.0, "entered after " + juce::String (onsetMs, 1) + " ms");
            float endStep = 0.0f;
            for (int i = 28800 - 100; i < 28800 + 9600; ++i)
                endStep = juce::jmax (endStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
            expect (endStep <= steadyStep * 1.2f, "no click at the end: step " + juce::String (endStep) + " vs " + juce::String (steadyStep));
            const auto tail = out.getRMSLevel (0, 28800 + 4800, 4800);
            expect (tail < 1.0e-4f, "silent after the fade: " + juce::String (tail, 6));
        }
    }

private:
    static VoiceEngine::NoteSet chordOf (std::initializer_list<int> notes)
    {
        VoiceEngine::NoteSet set {};
        for (auto n : notes)
            set[(size_t) n] = true;
        return set;
    }

    /** One voice audible (others off), no dry, no detune or humanize. */
    static VoiceEngine::Settings quiet()
    {
        VoiceEngine::Settings s;
        s.dryDb = -60.0f;
        s.detuneCents = 0.0f;
        s.humanizeMs = 0.0f;
        s.glideMs = 0.0f;
        for (auto& v : s.voices)
        {
            v.position = 0;
            v.interval = VoiceEngine::Interval::off;
            v.levelDb = 0.0f;
            v.pan = 0.0f;
        }
        return s;
    }

    /** A voice-like tone: 12 harmonics falling 6 dB per octave (a glottal-ish
        pulse), peak about 0.5. */
    static juce::AudioBuffer<float> voiceLike (double hz, int n)
    {
        juce::AudioBuffer<float> b (1, n);
        for (int i = 0; i < n; ++i)
        {
            double x = 0.0;
            for (int k = 1; k <= 12; ++k)
                x += std::sin (2.0 * juce::MathConstants<double>::pi * hz * k * i / 48000.0) / k;
            b.setSample (0, i, (float) (0.3 * x));
        }
        return b;
    }

    /** The pitch of @p b in [start, end) by autocorrelation, searched within
        half an octave of @p expected (an octave error then shows as a miss). */
    static double pitchOf (const juce::AudioBuffer<float>& b, int start, int end, double expected)
    {
        const auto* d = b.getReadPointer (0);
        const auto lo = (int) std::floor (48000.0 / (expected * 1.41)), hi = (int) std::ceil (48000.0 / (expected / 1.41));
        const auto n = end - start - hi;
        auto corr = [&] (int lag) {
            double xy = 0.0, xx = 0.0, yy = 0.0;
            for (int i = start; i < start + n; ++i)
            {
                xy += d[i] * d[i + lag];
                xx += d[i] * d[i];
                yy += d[i + lag] * d[i + lag];
            }
            return xy / std::sqrt (juce::jmax (1.0e-12, xx * yy));
        };
        int best = lo;
        double bestValue = -2.0;
        for (int lag = lo; lag <= hi; ++lag)
            if (const auto c = corr (lag); c > bestValue)
            {
                bestValue = c;
                best = lag;
            }
        // Parabolic refinement.
        const auto a = corr (best - 1), c = corr (best + 1);
        const auto denom = a - 2.0 * bestValue + c;
        const auto offset = std::abs (denom) > 1.0e-12 ? 0.5 * (a - c) / denom : 0.0;
        return 48000.0 / (best + juce::jlimit (-0.5, 0.5, offset));
    }

    static juce::AudioBuffer<float> sine (double hz, double amplitude, int n)
    {
        return tone (hz, { { 1.0, (float) amplitude } }, n);
    }

    static juce::AudioBuffer<float> tone (double hz, std::initializer_list<std::pair<double, float>> partials, int n)
    {
        juce::AudioBuffer<float> b (1, n);
        for (int i = 0; i < n; ++i)
        {
            double x = 0.0;
            for (const auto& [mult, amp] : partials)
                x += amp * std::sin (2.0 * juce::MathConstants<double>::pi * hz * mult * i / 48000.0);
            b.setSample (0, i, (float) x);
        }
        return b;
    }

    /** Runs @p input through the engine (prepared first unless @p fresh is
        false) in 64-sample blocks; returns the (mono) output. */
    static juce::AudioBuffer<float> run (VoiceEngine& engine, const VoiceEngine::Settings& s,
                                         const juce::AudioBuffer<float>& input, bool fresh = true)
    {
        if (fresh)
        {
            // prepare() keeps the held notes (MIDI may arrive before playback starts).
            engine.prepare (48000.0);
        }
        engine.setSettings (s);
        if (fresh)
            engine.reset();
        juce::AudioBuffer<float> out (input);
        for (int pos = 0; pos < out.getNumSamples(); pos += 64)
        {
            float* io[1] = { out.getWritePointer (0, pos) };
            engine.process (io, 1, juce::jmin (64, out.getNumSamples() - pos));
        }
        return out;
    }

    /** Feeds a sine of time-varying pitch from @p from to @p to seconds in
        64-sample blocks, calling @p check after each block. */
    static void processSweep (VoiceEngine& engine, std::function<double (double)> hzAt, double from, double to,
                              std::function<void (double)> check)
    {
        static double phase = 0.0;
        float block[64];
        for (double t = from; t < to; t += 64.0 / 48000.0)
        {
            for (int i = 0; i < 64; ++i)
            {
                phase += hzAt (t + i / 48000.0) / 48000.0;
                block[i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * phase);
            }
            float* io[1] = { block };
            engine.process (io, 1, 64);
            check (t);
        }
    }

};

static VoiceTests voiceTests;
