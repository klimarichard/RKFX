#pragma once

#include "plugin/ParamBuilder.h"

/** RKVoice parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. Key, scale and interval names are RKHarmonize's. */
namespace VoiceParams
{
    inline constexpr int kNumVoices = 4;

    /** Chord-mode positions, in choice order: off, below, above. */
    inline constexpr int positions[] = { 0, -3, -2, -1, 1, 2, 3 };
    inline const juce::StringArray positionNames { "Off", "3rd below", "2nd below", "1st below", "1st above", "2nd above", "3rd above" };
    inline const juce::StringArray positionKeys { "off", "below3", "below2", "below1", "above1", "above2", "above3" };

    inline const juce::StringArray intervalNames { "Off", "Octave below", "6th below", "5th below", "4th below", "3rd below",
                                                   "3rd above", "4th above", "5th above", "6th above", "Octave above" };
    inline const juce::StringArray intervalKeys { "off", "octave_below", "6th_below", "5th_below", "4th_below", "3rd_below",
                                                  "3rd_above", "4th_above", "5th_above", "6th_above", "octave_above" };

    inline juce::String voiceId (int voice, const char* suffix) { return "v" + juce::String (voice) + "_" + suffix; }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice ("mode", "Mode", { "Chord", "Scale" }, { "chord", "scale" }, 0);
        p.addChoice ("range", "Range", { "Voice", "Bass", "Guitar", "Violin" }, { "voice", "bass", "guitar", "violin" }, 0);
        p.addChoice ("chord_tones", "Chord Tones", { "Any Octave", "As Voiced" }, { "any_octave", "as_voiced" }, 0);
        p.addChoice ("no_chord", "No Chord", { "Silent", "Latch", "Scale" }, { "silent", "latch", "scale" }, 0);

        juce::StringArray channelNames { "Omni" }, channelKeys { "omni" };
        for (int ch = 1; ch <= 16; ++ch)
        {
            channelNames.add (juce::String (ch));
            channelKeys.add (juce::String (ch));
        }
        p.addChoice ("midi_channel", "MIDI Channel", channelNames, channelKeys, 0);

        p.addChoice ("key", "Key", { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" },
                     { "c", "c#", "d", "eb", "e", "f", "f#", "g", "ab", "a", "bb", "b" }, 0);
        p.addChoice ("scale", "Scale",
                     { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian",
                       "Mixolydian", "Major Pentatonic", "Minor Pentatonic", "Blues", "Chromatic" },
                     { "major", "minor", "harmonic_minor", "melodic_minor", "dorian", "phrygian", "lydian",
                       "mixolydian", "major_pentatonic", "minor_pentatonic", "blues", "chromatic" }, 0);

        // Voice 1: 1st above / a 3rd above; voice 2: 1st below; 3 and 4 off.
        constexpr int defaultPosition[] = { 4, 3, 0, 0 };
        constexpr int defaultInterval[] = { 6, 0, 0, 0 };
        constexpr float defaultPan[] = { -30.0f, 30.0f, -60.0f, 60.0f };
        for (int v = 1; v <= kNumVoices; ++v)
        {
            const auto name = "Voice " + juce::String (v);
            p.addChoice   (voiceId (v, "position"), name + " Position", positionNames, positionKeys, defaultPosition[v - 1]);
            p.addChoice   (voiceId (v, "interval"), name + " Interval", intervalNames, intervalKeys, defaultInterval[v - 1]);
            p.addDecibels (voiceId (v, "level"), name + " Level", -60.0f, 6.0f, -3.0f);
            p.addPercent  (voiceId (v, "pan"), name + " Pan", -100.0f, 100.0f, defaultPan[v - 1]);
        }

        p.addFloat    ("detune", "Detune", { 0.0f, 20.0f }, 5.0f,
                       [] (float c) { return juce::String (c, 1) + " ct"; }, rkfx::Units::parseNumber);
        p.addFloat    ("humanize", "Humanize", { 0.0f, 10.0f }, 3.0f,
                       [] (float ms) { return juce::String (ms, 1) + " ms"; }, rkfx::Units::parseNumber);
        p.addFloat    ("glide", "Glide", { 0.0f, 200.0f }, 30.0f,
                       [] (float ms) { return juce::String (ms, 0) + " ms"; }, rkfx::Units::parseNumber);
        p.addDecibels ("dry", "Dry", -60.0f, 6.0f, 0.0f);
        p.addDecibels ("threshold", "Threshold", -70.0f, -20.0f, -50.0f);

        return p;
    }
}
