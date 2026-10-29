#pragma once

#include "plugin/ParamBuilder.h"

/** RKFollow parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace FollowParams
{
    inline constexpr int kPressure = 128;   // expression controller value meaning Channel Pressure

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;
        using rkfx::Units::parseNumber;

        p.addBool     ("notes", "Notes", true);
        p.addChoice   ("range", "Range", { "Violin", "Voice", "Guitar", "Bass" }, { "violin", "voice", "guitar", "bass" }, 0);
        p.addDecibels ("threshold", "Threshold", -70.0f, -20.0f, -50.0f);
        p.addInt      ("transpose", "Transpose", -24, 24, 0, "st");
        p.addInt      ("vel_min", "Velocity Min", 1, 127, 40);
        p.addInt      ("vel_max", "Velocity Max", 1, 127, 127);
        p.addFloat    ("retrigger", "Retrigger", { 5.0f, 24.0f, 1.0f }, 12.0f,
                       [] (float db) { return db < 5.5f ? juce::String ("Off") : juce::String ((int) db) + " dB"; },
                       [] (const juce::String& s) { return s.trim().equalsIgnoreCase ("off") ? 5.0f : parseNumber (s); });
        p.addChoice   ("bend", "Bend", { "Off", "On", "MPE" }, { "off", "on", "mpe" }, 1);
        p.addChoice   ("bend_range", "Bend Range", { "2", "12", "24", "48" }, { "2", "12", "24", "48" }, 0);
        p.addTime     ("glide", "Glide", 0.0f, 100.0f, 0.0f);
        p.addInt      ("channel", "Channel", 1, 16, 1);

        p.addBool     ("expr", "Expression", true);
        p.addFloat    ("expr_cc", "Controller", { 0.0f, (float) kPressure, 1.0f }, 11.0f,
                       [] (float v) { return v >= (float) kPressure ? juce::String ("Pressure") : "CC " + juce::String ((int) v); },
                       [] (const juce::String& s) { return s.trim().startsWithIgnoreCase ("p") ? (float) kPressure
                                                                                       : parseNumber (s.retainCharacters ("0123456789")); });
        p.addDecibels ("expr_floor", "Floor", -70.0f, -10.0f, -50.0f);
        p.addDecibels ("expr_ceiling", "Ceiling", -40.0f, 0.0f, -10.0f);
        p.addInt      ("expr_min", "Expression Min", 0, 127, 0);
        p.addInt      ("expr_max", "Expression Max", 0, 127, 127);
        p.addPercent  ("expr_curve", "Curve", -100.0f, 100.0f, 0.0f);
        p.addTime     ("expr_response", "Response", 5.0f, 500.0f, 50.0f);

        return p;
    }

    inline int bendRangeSemitones (int index)
    {
        constexpr int ranges[] = { 2, 12, 24, 48 };
        return ranges[juce::jlimit (0, 3, index)];
    }
}
