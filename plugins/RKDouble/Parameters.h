#pragma once

#include "plugin/ParamBuilder.h"

/** RKDouble parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace DoubleParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addTime      ("delay", "Delay", 10.0f, 60.0f, 25.0f);
        p.addPercent   ("drift", "Drift", 0.0f, 100.0f, 40.0f);
        p.addFrequency ("drift_speed", "Drift Speed", 0.1f, 2.0f, 0.5f);
        p.addPercent   ("tone", "Tone", -100.0f, 100.0f, -20.0f);
        p.addPercent   ("spread", "Spread", 0.0f, 100.0f, 60.0f);
        p.addDecibels  ("level", "Double Level", -24.0f, 0.0f, -3.0f);

        return p;
    }
}
