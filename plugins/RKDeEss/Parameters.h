#pragma once

#include "plugin/ParamBuilder.h"

/** RKDeEss parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append.

    Two thresholds, one per detection mode ("threshold" absolute,
    "threshold_rel" relative), so switching modes never loses a setting and
    each has its own range and default. */
namespace DeEssParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;
        const auto plainDb = [] (float db) { return juce::String (db, 1) + " dB"; };

        p.addFrequency ("freq", "Frequency", 2000.0f, 12000.0f, 6000.0f);
        p.addChoice    ("detect", "Detection", { "Relative", "Absolute" }, { "relative", "absolute" }, 0);
        p.addDecibels  ("threshold", "Threshold (Absolute)", -60.0f, 0.0f, -24.0f);
        p.addFloat     ("range", "Range", { 0.0f, 24.0f }, 8.0f, plainDb, rkfx::Units::parseNumber);
        p.addChoice    ("mode", "Mode", { "Split-Band", "Wideband" }, { "split", "wide" }, 0);
        p.addBool      ("listen", "Listen", false);
        p.addBool      ("link", "Stereo Link", true);
        p.addDecibels  ("threshold_rel", "Threshold (Relative)", -24.0f, 12.0f, 0.0f);

        return p;
    }
}
