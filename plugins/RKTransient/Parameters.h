#pragma once

#include "plugin/ParamBuilder.h"

/** RKTransient parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace TransientParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addPercent ("attack", "Attack", -100.0f, 100.0f, 0.0f);
        p.addPercent ("sustain", "Sustain", -100.0f, 100.0f, 0.0f);
        p.addTime    ("attack_time", "Attack Time", 1.0f, 50.0f, 10.0f);
        p.addTime    ("sustain_time", "Sustain Time", 20.0f, 500.0f, 100.0f);
        p.addFrequencyOrOff ("focus_hpf", "Focus High-Pass", 20.0f, 5000.0f, 0.0f, false);
        p.addFrequencyOrOff ("focus_lpf", "Focus Low-Pass", 100.0f, 20000.0f, 0.0f, true);
        p.addBool    ("focus_listen", "Focus Listen", false);
        p.addBool    ("link", "Stereo Link", true);

        return p;
    }
}
