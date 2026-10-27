#pragma once

#include "plugin/ParamBuilder.h"

/** RKSaturate parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace SaturateParams
{
    inline constexpr int oversamplingFactors[] = { 1, 2, 4 };

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addDecibels ("drive", "Drive", 0.0f, 24.0f, 6.0f);
        p.addChoice   ("character", "Character", { "Tape", "Tube", "Transformer" }, { "tape", "tube", "transformer" }, 0);
        p.addPercent  ("tone", "Tone", -100.0f, 100.0f, 0.0f);
        p.addBool     ("autogain", "Auto Gain", true);
        p.addChoice   ("oversampling", "Oversampling", { "Off", "2x", "4x" }, { "off", "2x", "4x" }, 1);

        return p;
    }
}
