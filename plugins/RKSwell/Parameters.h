#pragma once

#include "plugin/ParamBuilder.h"

/** RKSwell parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace SwellParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addDecibels ("threshold", "Threshold", -60.0f, 0.0f, -40.0f);
        p.addTime     ("time", "Swell Time", 10.0f, 3000.0f, 400.0f);
        p.addChoice   ("curve", "Curve", { "Linear", "Exponential", "S-Curve" }, { "linear", "exponential", "scurve" }, 2);
        p.addPercent  ("retrigger", "Retrigger Sensitivity", 0.0f, 100.0f, 50.0f);
        p.addBool     ("link", "Stereo Link", true);

        return p;
    }
}
