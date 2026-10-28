#pragma once

#include "plugin/ParamBuilder.h"

/** RKOctave parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace OctaveParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice    ("range", "Range", { "Bass", "Guitar", "Violin" }, { "bass", "guitar", "violin" }, 1);
        p.addChoice    ("voice", "Voice", { "Natural", "Synth" }, { "natural", "synth" }, 0);
        p.addPercent   ("direct", "Direct", 0.0f, 100.0f, 100.0f);
        p.addPercent   ("octave1", "Octave 1", 0.0f, 100.0f, 80.0f);
        p.addPercent   ("octave2", "Octave 2", 0.0f, 100.0f, 0.0f);
        p.addFrequency ("tone", "Tone", 200.0f, 8000.0f, 1500.0f);
        p.addDecibels  ("threshold", "Threshold", -70.0f, -20.0f, -50.0f);

        return p;
    }
}
