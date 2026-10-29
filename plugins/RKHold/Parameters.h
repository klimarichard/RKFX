#pragma once

#include "plugin/ParamBuilder.h"

/** RKHold parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. Hold and Clear are momentary: each press (off to
    on) acts once; map them to momentary footswitches. */
namespace HoldParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addBool     ("hold", "Hold", false);
        p.addChoice   ("hold_mode", "Hold Mode", { "Momentary", "Latch" }, { "momentary", "latch" }, 1);
        p.addChoice   ("mode", "Mode", { "Auto", "Note", "Texture" }, { "auto", "note", "texture" }, 0);
        p.addChoice   ("range", "Range", { "Violin", "Voice", "Guitar", "Bass" }, { "violin", "voice", "guitar", "bass" }, 0);
        p.addChoice   ("layers", "Layers", { "Replace", "Add" }, { "replace", "add" }, 0);
        p.addBool     ("clear", "Clear", false);
        p.addChoice   ("octave", "Octave", { "-1", "0", "+1" }, { "-1", "0", "+1" }, 1);
        p.addTime     ("attack", "Attack", 10.0f, 2000.0f, 80.0f);
        p.addTime     ("release", "Release", 50.0f, 10000.0f, 600.0f);
        p.addPercent  ("movement", "Movement", 0.0f, 100.0f, 40.0f);
        p.addFrequency ("tone", "Tone", 1000.0f, 20000.0f, 12000.0f);
        p.addPercent  ("width", "Width", 0.0f, 100.0f, 50.0f);
        p.addDecibels ("hold_level", "Hold Level", -60.0f, 6.0f, 0.0f);
        p.addDecibels ("dry_level", "Dry Level", -60.0f, 6.0f, 0.0f);

        return p;
    }
}
