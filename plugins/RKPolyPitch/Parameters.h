#pragma once

#include "plugin/ParamBuilder.h"

/** RKPolyPitch parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. Momentary is a footswitch: on while held. */
namespace PolyPitchParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;
        using rkfx::Units::parseNumber;

        auto semitones = [] (float v) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v)) + " st"; };
        auto cents = [] (float v) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct"; };

        p.addChoice  ("mode", "Mode", { "Whammy", "Harmony", "Detune" }, { "whammy", "harmony", "detune" }, 0);
        p.addChoice  ("range", "Range", { "Violin", "Guitar", "Bass" }, { "violin", "guitar", "bass" }, 1);
        p.addFloat   ("heel", "Heel", { -24.0f, 24.0f, 1.0f }, 0.0f, semitones, parseNumber);
        p.addFloat   ("toe", "Toe", { -24.0f, 24.0f, 1.0f }, 12.0f, semitones, parseNumber);
        p.addPercent ("pedal", "Pedal", 0.0f, 100.0f, 0.0f);
        p.addTime    ("glide", "Glide", 0.0f, 500.0f, 30.0f);
        p.addBool    ("momentary", "Momentary", false);
        p.addTime    ("rise", "Rise", 0.0f, 2000.0f, 50.0f);
        p.addTime    ("fall", "Fall", 0.0f, 2000.0f, 150.0f);
        p.addFloat   ("fine", "Fine", { -50.0f, 50.0f, 1.0f }, 0.0f, cents, parseNumber);
        p.addFloat   ("detune", "Detune", { 2.0f, 30.0f, 1.0f }, 10.0f, cents, parseNumber);
        p.addDecibels ("voice_level", "Voice Level", -60.0f, 6.0f, -3.0f);
        p.addPercent ("width", "Width", 0.0f, 100.0f, 60.0f);
        p.addFrequency ("tone", "Tone", 1000.0f, 20000.0f, 20000.0f);
        p.addBool    ("low_latency", "Low Latency", false);

        return p;
    }
}
