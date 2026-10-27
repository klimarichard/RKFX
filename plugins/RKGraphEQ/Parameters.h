#pragma once

#include "plugin/ParamBuilder.h"

#include "GraphEQLayout.h"

/** RKGraphEQ parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append.

    Each band count has its own set (g10_b01..g10_b10, g15_b01..g15_b15,
    g31_b01..g31_b31), so switching never loses settings and host mappings
    stay fixed. */
namespace GraphEQParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice ("bands", "Bands", { "10", "15", "31" }, { "10", "15", "31" }, 0);

        for (int m = 0; m < 3; ++m)
            for (int b = 0; b < modeCount (m); ++b)
            {
                const auto hz = GraphEQEngine::centres (modes[m])[b];
                p.addDecibels (bandId (m, b), juce::String (modeCount (m)) + "-Band " + frequencyLabel (hz) + " Hz",
                               -12.0f, 12.0f, 0.0f);
            }

        return p;
    }
}
