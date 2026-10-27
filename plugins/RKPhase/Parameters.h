#pragma once

#include "dsp/TempoSync.h"
#include "plugin/ParamBuilder.h"

/** RKPhase parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace PhaseParams
{
    inline constexpr int stageCounts[] = { 2, 4, 6, 8, 12 };

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        juce::StringArray divisionNames, divisionKeys;
        for (int i = 0; i < rkfx::SyncDivision::kCount; ++i)
        {
            divisionNames.add (rkfx::SyncDivision::names[i]);
            divisionKeys.add (rkfx::SyncDivision::keys[i]);
        }

        p.addChoice    ("stages", "Stages", { "2", "4", "6", "8", "12" }, { "2", "4", "6", "8", "12" }, 1);
        p.addFrequency ("center", "Center Frequency", 100.0f, 5000.0f, 800.0f);
        p.addPercent   ("feedback", "Feedback", -95.0f, 95.0f, 30.0f);
        p.addFrequency ("rate", "Rate", 0.02f, 10.0f, 0.5f);
        p.addPercent   ("depth", "Depth", 0.0f, 100.0f, 70.0f);
        p.addBool      ("sync", "Sync", false);
        p.addChoice    ("division", "Sync Division", divisionNames, divisionKeys, rkfx::SyncDivision::index (2));
        p.addDegrees   ("phase", "Stereo Phase", 0.0f, 180.0f, 90.0f);
        p.addPercent   ("env_amount", "Envelope Amount", -100.0f, 100.0f, 0.0f);
        p.addDecibels  ("env_sens", "Envelope Sensitivity", -24.0f, 24.0f, 0.0f);
        p.addTime      ("env_attack", "Envelope Attack", 1.0f, 100.0f, 5.0f);
        p.addTime      ("env_release", "Envelope Release", 20.0f, 1000.0f, 150.0f);

        return p;
    }
}
