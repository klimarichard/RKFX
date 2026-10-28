#pragma once

#include "plugin/ParamBuilder.h"

/** RKRotary parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace RotaryParams
{
    inline constexpr int oversamplingFactors[] = { 1, 2, 4 };

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice    ("speed", "Speed", { "Slow", "Fast", "Brake" }, { "slow", "fast", "brake" }, 0);
        p.addFrequency ("slow_rate", "Slow Rate", 0.3f, 1.5f, 0.8f);
        p.addFrequency ("fast_rate", "Fast Rate", 4.0f, 8.0f, 6.5f);
        p.addTime      ("horn_accel", "Horn Acceleration", 200.0f, 3000.0f, 800.0f);
        p.addTime      ("drum_accel", "Drum Acceleration", 1000.0f, 8000.0f, 4000.0f);
        p.addPercent   ("balance", "Horn/Drum Balance", -100.0f, 100.0f, 0.0f);
        p.addPercent   ("distance", "Mic Distance", 0.0f, 100.0f, 50.0f);
        p.addPercent   ("width", "Stereo Width", 0.0f, 100.0f, 80.0f);
        p.addPercent   ("drive", "Drive", 0.0f, 100.0f, 0.0f);
        p.addChoice    ("oversampling", "Oversampling", { "Off", "2x", "4x" }, { "off", "2x", "4x" }, 1);

        return p;
    }
}
