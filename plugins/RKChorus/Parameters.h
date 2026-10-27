#pragma once

#include "dsp/TempoSync.h"
#include "plugin/ParamBuilder.h"

/** RKChorus parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace ChorusParams
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

        p.addInt       ("voices", "Voices", 1, 4, 1);
        p.addFrequency ("rate", "Rate", 0.05f, 10.0f, 0.8f);
        p.addPercent   ("depth", "Depth", 0.0f, 100.0f, 40.0f);
        p.addTime      ("delay", "Delay", 5.0f, 30.0f, 12.0f);
        p.addPercent   ("width", "Width", 0.0f, 100.0f, 100.0f);
        p.addPercent   ("tone", "Tone", -100.0f, 100.0f, 0.0f);
        p.addBool      ("sync", "Sync", false);
        p.addChoice    ("division", "Sync Division", divisionNames, divisionKeys, rkfx::SyncDivision::index (4));

        return p;
    }
}
