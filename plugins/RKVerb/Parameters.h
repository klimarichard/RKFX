#pragma once

#include "plugin/ParamBuilder.h"

/** RKVerb parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace VerbParams
{
    /** Pre-delay sync divisions: 1/64 to 1/8, dotted, straight and triplet. */
    inline constexpr int kDivisionCount = 12;
    inline const char* const divisionNames[kDivisionCount] = {
        "1/64 D", "1/64", "1/64 T", "1/32 D", "1/32", "1/32 T",
        "1/16 D", "1/16", "1/16 T", "1/8 D",  "1/8",  "1/8 T" };
    inline const char* const divisionKeys[kDivisionCount] = {
        "1/64d", "1/64", "1/64t", "1/32d", "1/32", "1/32t",
        "1/16d", "1/16", "1/16t", "1/8d",  "1/8",  "1/8t" };

    /** Length of a division in quarter notes. */
    inline double divisionBeats (int index)
    {
        const auto straight = 4.0 / (double) (64 >> (index / 3));
        const auto kind = index % 3;
        return kind == 0 ? straight * 1.5 : kind == 2 ? straight * 2.0 / 3.0 : straight;
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        juce::StringArray names, keys;
        for (int i = 0; i < kDivisionCount; ++i)
        {
            names.add (divisionNames[i]);
            keys.add (divisionKeys[i]);
        }

        p.addChoice  ("type", "Type", { "Room", "Hall", "Plate" }, { "room", "hall", "plate" }, 1);
        p.addTime    ("decay", "Decay", 200.0f, 20000.0f, 2500.0f);
        p.addPercent ("size", "Size", 0.0f, 100.0f, 60.0f);
        p.addTime    ("predelay", "Pre-Delay", 0.0f, 200.0f, 20.0f);
        p.addBool    ("predelay_sync", "Pre-Delay Sync", false);
        p.addChoice  ("predelay_division", "Pre-Delay Division", names, keys, 7);
        p.addPercent ("damping", "Damping", 0.0f, 100.0f, 50.0f);
        p.addFrequency ("lowcut", "Low Cut", 20.0f, 1000.0f, 150.0f);
        p.addFrequency ("highcut", "High Cut", 1000.0f, 20000.0f, 10000.0f);
        p.addPercent ("mod", "Modulation", 0.0f, 100.0f, 20.0f);
        p.addPercent ("width", "Width", 0.0f, 100.0f, 100.0f);
        p.addBool    ("freeze", "Freeze", false);

        return p;
    }
}
