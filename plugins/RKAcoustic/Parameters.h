#pragma once

#include "plugin/ParamBuilder.h"

/** RKAcoustic parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace AcousticParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice    ("voicing", "Voicing", { "Flat", "Violin", "Viola", "Guitar", "Upright Bass" },
                        { "flat", "violin", "viola", "guitar", "bass" }, 3);
        p.addPercent   ("body", "Body", 0.0f, 100.0f, 50.0f);
        p.addPercent   ("quack", "Quack", 0.0f, 100.0f, 30.0f);
        p.addFrequencyOrOff ("highpass", "High-Pass", 20.0f, 300.0f, 0.0f, false);
        p.addBool      ("phase", "Phase Invert", false);
        p.addDecibels  ("low", "Low", -12.0f, 12.0f, 0.0f);
        p.addDecibels  ("mid", "Mid", -12.0f, 12.0f, 0.0f);
        p.addFrequency ("mid_freq", "Mid Frequency", 200.0f, 5000.0f, 800.0f);
        p.addDecibels  ("treble", "Treble", -12.0f, 12.0f, 0.0f);
        p.addDecibels  ("air", "Air", 0.0f, 12.0f, 0.0f);
        p.addFrequencyOrOff ("notch", "Notch", 40.0f, 2000.0f, 0.0f, false);
        p.addPercent   ("compress", "Compress", 0.0f, 100.0f, 0.0f);

        return p;
    }
}
