#pragma once

#include "plugin/ParamBuilder.h"

/** RKCompress parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace CompressParams
{
    inline juce::String ratioText (float r)
    {
        return juce::String (r, r < 10.0f ? 1 : 0) + ":1";
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addDecibels ("threshold", "Threshold", -60.0f, 0.0f, -18.0f);
        p.addFloat    ("ratio", "Ratio", rkfx::Units::logRange (1.0f, 20.0f), 4.0f, ratioText, rkfx::Units::parseNumber);
        p.addTime     ("attack", "Attack", 0.1f, 100.0f, 10.0f);
        p.addTime     ("release", "Release", 10.0f, 2000.0f, 150.0f);
        p.addBool     ("release_auto", "Auto Release", false);
        p.addFloat    ("knee", "Knee", { 0.0f, 12.0f }, 6.0f,
                       [] (float db) { return juce::String (db, 1) + " dB"; }, rkfx::Units::parseNumber);
        p.addChoice   ("detect", "Detection", { "Peak", "RMS" }, { "peak", "rms" }, 1);
        p.addChoice   ("sc_source", "Sidechain Source", { "Internal", "External" }, { "internal", "external" }, 0);
        p.addFrequencyOrOff ("sc_hpf", "Sidechain High-Pass", 40.0f, 300.0f, 0.0f, false);
        p.addBool     ("sc_listen", "Sidechain Listen", false);
        p.addPercent  ("link", "Stereo Link", 0.0f, 100.0f, 100.0f);
        p.addFloat    ("makeup", "Makeup", { 0.0f, 24.0f }, 0.0f, rkfx::Units::decibels, rkfx::Units::parseNumber);
        p.addBool     ("makeup_auto", "Auto Makeup", false);

        return p;
    }
}
