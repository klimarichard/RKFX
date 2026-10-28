#pragma once

#include "plugin/ParamBuilder.h"

#include <cstring>

/** RKVocode parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace VocodeParams
{
    inline constexpr int bandCounts[] = { 8, 16, 24, 32 };

    /** Note names in Studio One / Yamaha style: MIDI 60 = C3. */
    inline juce::String noteName (int note)
    {
        static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 2);
    }

    inline int parseNote (const juce::String& text)
    {
        const auto t = text.trim().toUpperCase();
        static const char* const names[] = { "C#", "D#", "F#", "G#", "A#", "C", "D", "E", "F", "G", "A", "B" };
        static const int offsets[] = { 1, 3, 6, 8, 10, 0, 2, 4, 5, 7, 9, 11 };
        for (int i = 0; i < 12; ++i)
            if (t.startsWith (names[i]))
                return offsets[i] + 12 * (t.substring ((int) std::strlen (names[i])).getIntValue() + 2);
        return t.getIntValue();
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice    ("bands", "Bands", { "8", "16", "24", "32" }, { "8", "16", "24", "32" }, 1);
        p.addFrequency ("freq_low", "Low Frequency", 50.0f, 500.0f, 100.0f);
        p.addFrequency ("freq_high", "High Frequency", 4000.0f, 12000.0f, 8000.0f);
        p.addFloat     ("formant", "Formant Shift", { -12.0f, 12.0f }, 0.0f,
                        [] (float st) { return (st > 0.05f ? "+" : "") + juce::String (st, 1) + " st"; }, rkfx::Units::parseNumber);
        p.addTime      ("attack", "Attack", 1.0f, 50.0f, 5.0f);
        p.addTime      ("release", "Release", 10.0f, 500.0f, 50.0f);
        p.addPercent   ("unvoiced", "Unvoiced", 0.0f, 100.0f, 30.0f);
        p.addPercent   ("mod_mix", "Modulator Mix", 0.0f, 100.0f, 0.0f);

        p.addChoice    ("carrier", "Carrier Source", { "Sidechain", "Internal" }, { "sidechain", "internal" }, 1);
        p.addChoice    ("osc_wave", "Waveform", { "Saw", "Pulse", "Noise", "Saw + Noise" }, { "saw", "pulse", "noise", "sawnoise" }, 0);
        p.addChoice    ("osc_mode", "Pitch Mode", { "MIDI", "Fixed" }, { "midi", "fixed" }, 1);

        // Fixed note C1..C6 (MIDI 36..96), shown and typed as note names.
        p.params.push_back (std::make_unique<juce::AudioParameterInt> (
            p.nextId ("osc_note"), "Fixed Note", 36, 96, 60,
            juce::AudioParameterIntAttributes()
                .withStringFromValueFunction ([] (int n, int) { return noteName (n); })
                .withValueFromStringFunction ([] (const juce::String& t) { return parseNote (t); })));

        p.addInt       ("osc_voices", "Voices", 1, 8, 8);
        p.addTime      ("osc_glide", "Glide", 0.0f, 500.0f, 0.0f);

        return p;
    }
}
