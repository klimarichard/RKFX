#pragma once

#include "plugin/ParamBuilder.h"

/** RKStrum parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace StrumParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice  ("direction", "Direction", { "Down", "Up", "Alternate" }, { "down", "up", "alternate" }, 0);
        p.addTime    ("speed", "Speed", 2.0f, 100.0f, 20.0f);
        p.addTime    ("window", "Chord Window", 2.0f, 50.0f, 10.0f);
        p.addPercent ("ramp", "Velocity Ramp", -50.0f, 50.0f, 0.0f);
        p.addPercent ("humanize", "Humanize", 0.0f, 100.0f, 0.0f);

        return p;
    }
}
