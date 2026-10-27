#pragma once

#include "plugin/ParamBuilder.h"

/** RKParaEQ parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace ParaEQParams
{
    /** One entry per band, in signal order. */
    struct BandInfo
    {
        const char* prefix;      // "hp", "low", ...
        const char* title;       // shown on the strip
        bool cut;                // high-pass / low-pass
        bool shelfOrBell;        // low / high: type switch
        float minHz, maxHz, defaultHz, defaultQ;
    };

    inline constexpr BandInfo bands[] = {
        { "hp",      "HP",       true,  false, 20.0f,   1000.0f,  80.0f,    0.0f },
        { "low",     "Low",      false, true,  20.0f,   1000.0f,  100.0f,   0.71f },
        { "lowmid",  "Low Mid",  false, false, 40.0f,   2000.0f,  250.0f,   1.0f },
        { "mid",     "Mid",      false, false, 200.0f,  8000.0f,  1000.0f,  1.0f },
        { "highmid", "High Mid", false, false, 600.0f,  16000.0f, 3500.0f,  1.0f },
        { "high",    "High",     false, true,  1000.0f, 20000.0f, 8000.0f,  0.71f },
        { "lp",      "LP",       true,  false, 1000.0f, 20000.0f, 12000.0f, 0.0f },
    };

    inline constexpr int kNumBands = (int) (sizeof (bands) / sizeof (bands[0]));

    inline juce::String id (const BandInfo& b, const char* name) { return juce::String (b.prefix) + "_" + name; }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        for (const auto& b : bands)
        {
            const juce::String title (b.title);

            p.addBool (id (b, "on"), title + " On", ! b.cut);

            if (b.shelfOrBell)
                p.addChoice (id (b, "type"), title + " Type", { "Shelf", "Bell" }, { "shelf", "bell" }, 0);

            p.addFrequency (id (b, "freq"), title + " Frequency", b.minHz, b.maxHz, b.defaultHz);

            if (b.cut)
            {
                p.addChoice (id (b, "slope"), title + " Slope", { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" },
                             { "12", "24", "36", "48" }, 0);
            }
            else
            {
                p.addDecibels (id (b, "gain"), title + " Gain", -18.0f, 18.0f, 0.0f);
                p.addFloat (id (b, "q"), title + " Q", rkfx::Units::logRange (0.3f, 10.0f), b.defaultQ,
                            [] (float q) { return juce::String (q, q < 1.0f ? 2 : 1); }, rkfx::Units::parseNumber);
            }
        }

        return p;
    }
}
