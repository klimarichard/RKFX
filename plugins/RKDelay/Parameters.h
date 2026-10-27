#pragma once

#include "dsp/TempoSync.h"
#include "plugin/ParamBuilder.h"

/** RKDelay parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace DelayParams
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

        p.addChoice    ("mode", "Mode", { "Digital", "Analog", "Tape" }, { "digital", "analog", "tape" }, 0);
        p.addTime      ("time", "Time", 1.0f, 2000.0f, 375.0f);
        p.addBool      ("sync", "Sync", false);
        p.addChoice    ("division", "Sync Division", divisionNames, divisionKeys, rkfx::SyncDivision::index (8, 'd'));
        p.addPercent   ("feedback", "Feedback", 0.0f, 100.0f, 35.0f);
        p.addChoice    ("stereo", "Stereo Mode", { "Mono", "Stereo", "Ping-Pong" }, { "mono", "stereo", "pingpong" }, 1);
        p.addPercent   ("offset", "Right Time Offset", -50.0f, 50.0f, 0.0f);
        p.addFrequency ("lowcut", "Low Cut", 20.0f, 1000.0f, 100.0f);
        p.addFrequency ("highcut", "High Cut", 1000.0f, 20000.0f, 8000.0f);
        p.addPercent   ("mod_depth", "Modulation Depth", 0.0f, 100.0f, 0.0f);
        p.addFrequency ("mod_rate", "Modulation Rate", 0.1f, 5.0f, 0.6f);
        p.addPercent   ("duck", "Ducking", 0.0f, 100.0f, 0.0f);

        return p;
    }
}
