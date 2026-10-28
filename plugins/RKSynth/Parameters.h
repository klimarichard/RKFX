#pragma once

#include "plugin/ParamBuilder.h"

/** RKSynth parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace SynthParams
{
    inline constexpr int octaveShifts[] = { -2, -1, 0, 1 };

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice    ("range", "Range", { "Bass", "Guitar", "Violin" }, { "bass", "guitar", "violin" }, 1);
        p.addChoice    ("wave", "Wave", { "Saw", "Square", "Triangle" }, { "saw", "square", "triangle" }, 0);
        p.addChoice    ("octave", "Octave", { "-2", "-1", "0", "+1" }, { "-2", "-1", "0", "+1" }, 2);
        p.addPercent   ("sub", "Sub", 0.0f, 100.0f, 0.0f);
        p.addTime      ("glide", "Glide", 1.0f, 1000.0f, 10.0f);
        p.addFrequency ("cutoff", "Cutoff", 50.0f, 10000.0f, 1200.0f);
        p.addPercent   ("resonance", "Resonance", 0.0f, 100.0f, 30.0f);
        p.addPercent   ("env_amount", "Envelope Amount", 0.0f, 100.0f, 50.0f);
        p.addTime      ("decay", "Decay", 20.0f, 2000.0f, 300.0f);
        p.addPercent   ("dynamics", "Dynamics", 0.0f, 100.0f, 70.0f);
        p.addDecibels  ("threshold", "Threshold", -70.0f, -20.0f, -50.0f);

        return p;
    }
}
