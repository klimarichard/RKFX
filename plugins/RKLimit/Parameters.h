#pragma once

#include "plugin/ParamBuilder.h"

/** RKLimit parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace LimitParams
{
    inline constexpr int oversamplingFactors[] = { 1, 2, 4 };

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addDecibels ("input", "Input Gain", 0.0f, 24.0f, 0.0f);
        p.addFloat    ("ceiling", "Ceiling", { -12.0f, 0.0f }, -1.0f,
                       [] (float db) { return juce::String (db, 1) + " dBFS"; }, rkfx::Units::parseNumber);
        p.addTime     ("release", "Release", 1.0f, 500.0f, 50.0f);
        p.addBool     ("release_auto", "Auto Release", false);
        p.addPercent  ("link", "Stereo Link", 0.0f, 100.0f, 100.0f);
        p.addChoice   ("oversampling", "Oversampling", { "Off", "2x", "4x" }, { "off", "2x", "4x" }, 0);

        return p;
    }
}
