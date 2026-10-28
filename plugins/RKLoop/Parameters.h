#pragma once

#include "plugin/ParamBuilder.h"

/** RKLoop parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append.

    record, stop, undo, clear, next and previous are footswitches: each
    press (off to on) acts once. Map them to momentary buttons. */
namespace LoopParams
{
    inline constexpr int kLoops = 8;
    inline const char* const buttonIds[] = { "record", "stop", "undo", "clear", "next", "previous" };

    inline juce::String roleId (int loop) { return "role" + juce::String (loop + 1); }   // loop 0..7

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addBool    ("record", "Record / Play / Overdub", false);
        p.addBool    ("stop", "Stop", false);
        p.addBool    ("undo", "Undo / Redo", false);
        p.addBool    ("clear", "Clear", false);
        p.addBool    ("reverse", "Reverse", false);
        p.addBool    ("half_speed", "Half Speed", false);
        p.addPercent ("level", "Loop Level", 0.0f, 100.0f, 100.0f);

        // Eight loops (added later, appended).
        p.addInt     ("loop", "Loop", 1, kLoops, 1);
        p.addBool    ("sync", "Sync", true);
        p.addBool    ("next", "Next Loop", false);
        p.addBool    ("previous", "Previous Loop", false);
        for (int i = 0; i < kLoops; ++i)
            p.addChoice (roleId (i), "Loop " + juce::String (i + 1) + " Role", { "Part", "Layer" }, { "part", "layer" }, 0);

        return p;
    }
}
