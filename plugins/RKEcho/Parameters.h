#pragma once

#include "dsp/TempoSync.h"
#include "plugin/ParamBuilder.h"

/** RKEcho parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace EchoParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;
        using rkfx::Units::parseNumber;

        juce::StringArray divisionNames, divisionKeys;
        for (int i = 0; i < rkfx::SyncDivision::kCount; ++i)
        {
            divisionNames.add (rkfx::SyncDivision::names[i]);
            divisionKeys.add (rkfx::SyncDivision::keys[i]);
        }

        auto channelOr = [] (const juce::String& zeroText)
        {
            return std::make_pair ([zeroText] (float v) { return v < 0.5f ? zeroText : juce::String ((int) v); },
                                   [zeroText] (const juce::String& s) { return s.trim().equalsIgnoreCase (zeroText) ? 0.0f : parseNumber (s); });
        };

        p.addTime    ("time", "Time", 10.0f, 2000.0f, 375.0f);
        p.addBool    ("sync", "Sync", true);
        p.addChoice  ("division", "Division", divisionNames, divisionKeys, rkfx::SyncDivision::index (8, 'd'));
        p.addInt     ("repeats", "Repeats", 1, 16, 4);
        p.addPercent ("feedback", "Feedback", 10.0f, 100.0f, 70.0f);
        p.addInt     ("vel_min", "Min Velocity", 1, 127, 10);
        p.addInt     ("step", "Step", -12, 12, 0);
        p.addBool    ("scale_steps", "Scale Steps", false);
        p.addChoice  ("key", "Key", { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" },
                      { "c", "c#", "d", "eb", "e", "f", "f#", "g", "ab", "a", "bb", "b" }, 0);
        p.addChoice  ("scale", "Scale",
                      { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian",
                        "Mixolydian", "Major Pentatonic", "Minor Pentatonic", "Blues", "Chromatic" },
                      { "major", "minor", "harmonic_minor", "melodic_minor", "dorian", "phrygian", "lydian",
                        "mixolydian", "major_pentatonic", "minor_pentatonic", "blues", "chromatic" }, 0);
        p.addChoice  ("length", "Length", { "As played", "Gate" }, { "as_played", "gate" }, 0);
        p.addPercent ("gate", "Gate", 10.0f, 100.0f, 50.0f);
        p.addPercent ("swing", "Swing", 0.0f, 50.0f, 0.0f);
        p.addBool    ("original", "Original", true);

        const auto same = channelOr ("Same");
        p.addFloat   ("channel", "Echo Channel", { 0.0f, 16.0f, 1.0f }, 0.0f, same.first, same.second);
        const auto off = channelOr ("Off");
        p.addFloat   ("alternate", "Alternate", { 0.0f, 16.0f, 1.0f }, 0.0f, off.first, off.second);

        return p;
    }
}
