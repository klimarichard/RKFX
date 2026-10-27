#pragma once

#include "dsp/TempoSync.h"
#include "plugin/ParamBuilder.h"

/** RKTremolo parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace TremoloParams
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

        p.addChoice    ("mode", "Mode", { "Classic", "Harmonic" }, { "classic", "harmonic" }, 0);
        p.addPercent   ("depth", "Depth", 0.0f, 100.0f, 50.0f);
        p.addFrequency ("rate", "Rate", 0.1f, 20.0f, 4.0f);
        p.addBool      ("sync", "Sync", false);
        p.addChoice    ("division", "Sync Division", divisionNames, divisionKeys, rkfx::SyncDivision::index (8));
        p.addChoice    ("shape", "Shape", { "Sine", "Triangle", "Square", "Saw Up", "Saw Down" },
                        { "sine", "triangle", "square", "sawup", "sawdown" }, 0);
        p.addPercent   ("smooth", "Smoothing", 0.0f, 100.0f, 30.0f);
        p.addDegrees   ("phase", "Stereo Phase", 0.0f, 180.0f, 0.0f);
        p.addFrequency ("xover", "Crossover", 200.0f, 2000.0f, 600.0f);

        return p;
    }
}
