#pragma once

#include "dsp/TempoSync.h"
#include "plugin/ParamBuilder.h"

/** RKArpeggiate parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace ArpParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        juce::StringArray divisionNames, divisionKeys;
        for (int i = 0; i < rkfx::SyncDivision::kCount; ++i)
        {
            divisionNames.add (rkfx::SyncDivision::names[i]);
            divisionKeys.add (rkfx::SyncDivision::keys[i]);
        }

        p.addChoice  ("pattern", "Pattern", { "Up", "Down", "Up/Down", "As Played", "Random", "Chord" },
                      { "up", "down", "updown", "played", "random", "chord" }, 0);
        p.addChoice  ("division", "Rate", divisionNames, divisionKeys, rkfx::SyncDivision::index (16));
        p.addInt     ("octaves", "Octaves", 1, 4, 1);
        p.addPercent ("gate", "Gate", 5.0f, 100.0f, 75.0f);
        p.addPercent ("swing", "Swing", 50.0f, 75.0f, 50.0f);
        p.addBool    ("latch", "Latch", false);
        p.addChoice  ("velocity_mode", "Velocity Mode", { "As Played", "Fixed" }, { "played", "fixed" }, 0);
        p.addInt     ("velocity", "Velocity", 1, 127, 100);

        return p;
    }
}
