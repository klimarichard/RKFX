#pragma once

#include "plugin/ParamBuilder.h"

/** RKRide parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace RideParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addDecibels ("target", "Target", -40.0f, 0.0f, -18.0f);
        p.addDecibels ("max_boost", "Max Boost", 0.0f, 18.0f, 6.0f);
        p.addDecibels ("max_cut", "Max Cut", 0.0f, 18.0f, 6.0f);
        p.addTime     ("speed", "Speed", 100.0f, 3000.0f, 500.0f);
        p.addDecibels ("floor", "Floor", -70.0f, -20.0f, -50.0f);
        p.addTime     ("return", "Return", 1000.0f, 10000.0f, 3000.0f);
        p.addChoice   ("mode", "Mode", { "Absolute", "Relative" }, { "absolute", "relative" }, 0);
        p.addDecibels ("above", "Above Music", -12.0f, 12.0f, 3.0f);

        return p;
    }
}
