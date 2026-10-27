#pragma once

#include "dsp/TempoSync.h"
#include "plugin/ParamBuilder.h"

/** RKFlange parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace FlangeParams
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

        p.addFrequency ("rate", "Rate", 0.02f, 10.0f, 0.3f);
        p.addPercent   ("depth", "Depth", 0.0f, 100.0f, 60.0f);
        p.addTime      ("manual", "Manual", 0.1f, 10.0f, 2.0f);
        p.addPercent   ("feedback", "Feedback", -95.0f, 95.0f, 50.0f);
        p.addBool      ("sync", "Sync", false);
        p.addChoice    ("division", "Sync Division", divisionNames, divisionKeys, rkfx::SyncDivision::index (2));
        p.addDegrees   ("phase", "Stereo Phase", 0.0f, 180.0f, 90.0f);

        return p;
    }
}
