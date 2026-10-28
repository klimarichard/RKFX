#pragma once

#include "plugin/ParamBuilder.h"

/** RKHarmonize parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace HarmonizeParams
{
    inline const juce::StringArray intervalNames { "Off", "Octave below", "6th below", "5th below", "4th below", "3rd below",
                                                   "3rd above", "4th above", "5th above", "6th above", "Octave above" };
    inline const juce::StringArray intervalKeys { "off", "octave_below", "6th_below", "5th_below", "4th_below", "3rd_below",
                                                  "3rd_above", "4th_above", "5th_above", "6th_above", "octave_above" };

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice  ("key", "Key", { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" },
                      { "c", "c#", "d", "eb", "e", "f", "f#", "g", "ab", "a", "bb", "b" }, 0);
        p.addChoice  ("scale", "Scale",
                      { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian",
                        "Mixolydian", "Major Pentatonic", "Minor Pentatonic", "Blues", "Chromatic" },
                      { "major", "minor", "harmonic_minor", "melodic_minor", "dorian", "phrygian", "lydian",
                        "mixolydian", "major_pentatonic", "minor_pentatonic", "blues", "chromatic" }, 0);
        p.addChoice  ("voice1", "Voice 1", intervalNames, intervalKeys, 6);
        p.addChoice  ("voice2", "Voice 2", intervalNames, intervalKeys, 0);
        p.addPercent ("velocity", "Harmony Velocity", 0.0f, 100.0f, 80.0f);
        p.addBool    ("original", "Original Note", true);

        return p;
    }
}
