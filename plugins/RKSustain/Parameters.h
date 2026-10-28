#pragma once

#include "plugin/ParamBuilder.h"

/** RKSustain parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace SustainParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice ("mode", "Mode", { "Sustain", "Sostenuto", "Hold", "Latch" }, { "sustain", "sostenuto", "hold", "latch" }, 0);
        p.addBool   ("pedal", "Pedal", false);
        p.addBool   ("use_cc", "Pedal from MIDI", true);
        p.addInt    ("cc", "Pedal Controller", 0, 127, 64);

        return p;
    }
}
