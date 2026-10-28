#pragma once

#include "dsp/TempoSync.h"
#include "plugin/ParamBuilder.h"

/** RKVibrato parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace VibratoParams
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

        p.addFrequency ("rate", "Rate", 0.1f, 15.0f, 5.0f);
        p.addPercent   ("depth", "Depth", 0.0f, 100.0f, 30.0f);
        p.addChoice    ("shape", "Shape", { "Sine", "Triangle" }, { "sine", "triangle" }, 0);
        p.addBool      ("sync", "Sync", false);
        p.addChoice    ("division", "Sync Division", divisionNames, divisionKeys, rkfx::SyncDivision::index (16));
        p.addDegrees   ("phase", "Stereo Phase", 0.0f, 180.0f, 0.0f);
        p.addTime      ("rise", "Rise", 0.0f, 2000.0f, 0.0f);

        return p;
    }
}
