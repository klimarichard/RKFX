#pragma once

#include "plugin/ParamBuilder.h"

/** RKDistort parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace DistortParams
{
    inline constexpr int oversamplingFactors[] = { 1, 2, 4 };

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice   ("type", "Type", { "Overdrive", "Distortion", "Fuzz" }, { "overdrive", "distortion", "fuzz" }, 0);
        p.addDecibels ("gain", "Gain", 0.0f, 48.0f, 18.0f);
        p.addFrequencyOrOff ("tight", "Tight", 40.0f, 500.0f, 0.0f, false);
        p.addDecibels ("mid", "Mid", -12.0f, 12.0f, 0.0f);
        p.addPercent  ("tone", "Tone", -100.0f, 100.0f, 0.0f);
        p.addChoice   ("cab", "Cab Filter", { "Off", "Small (1x12)", "Medium (2x12)", "Large (4x12)", "Bass (1x15)" },
                       { "off", "small", "medium", "large", "bass" }, 0);
        p.addBool     ("autogain", "Auto Gain", false);
        p.addChoice   ("oversampling", "Oversampling", { "Off", "2x", "4x" }, { "off", "2x", "4x" }, 2);

        return p;
    }
}
