#pragma once

#include "plugin/ParamBuilder.h"

/** RKLoop parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append.

    record, stop, undo and clear are footswitches: each press (off to on)
    acts once. Map them to momentary buttons. */
namespace LoopParams
{
    inline const char* const buttonIds[] = { "record", "stop", "undo", "clear" };

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

        return p;
    }
}
