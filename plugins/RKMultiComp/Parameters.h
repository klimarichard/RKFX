#pragma once

#include "plugin/ParamBuilder.h"

#include "MultiCompEngine.h"

/** RKMultiComp parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace MultiCompParams
{
    struct BandInfo
    {
        MultiCompEngine::Band band;
        const char* prefix;   // parameter ID prefix
        const char* name;     // in 4-band mode
    };

    /** The four parameter sets. In 3-band mode "lmid" is the Mid band. */
    inline constexpr BandInfo bands[] = {
        { MultiCompEngine::low,     "low",  "Low" },
        { MultiCompEngine::lowMid,  "lmid", "Low-Mid" },
        { MultiCompEngine::highMid, "hmid", "High-Mid" },
        { MultiCompEngine::high,    "high", "High" },
    };

    inline juce::String id (const BandInfo& b, const char* suffix)
    {
        return juce::String (b.prefix) + "_" + suffix;
    }

    /** The band's name as shown: Low-Mid is plain "Mid" in 3-band mode. */
    inline juce::String displayName (MultiCompEngine::Band band, bool fourBands)
    {
        if (band == MultiCompEngine::lowMid && ! fourBands)
            return "Mid";
        return bands[(int) band].name;
    }

    inline juce::String ratioText (float r)
    {
        return juce::String (r, r < 10.0f ? 1 : 0) + ":1";
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;
        const auto defaults = MultiCompEngine::defaultBands();

        for (const auto& b : bands)
        {
            const auto& d = defaults[(size_t) b.band];
            const juce::String name (b.name);

            p.addDecibels (id (b, "threshold"), name + " Threshold", -60.0f, 0.0f, d.thresholdDb);
            p.addFloat    (id (b, "ratio"), name + " Ratio", rkfx::Units::logRange (1.0f, 20.0f), d.ratio,
                           ratioText, rkfx::Units::parseNumber);
            p.addTime     (id (b, "attack"), name + " Attack", 0.1f, 100.0f, d.attackMs);
            p.addTime     (id (b, "release"), name + " Release", 10.0f, 2000.0f, d.releaseMs);
            p.addDecibels (id (b, "gain"), name + " Gain", -12.0f, 24.0f, 0.0f);
            p.addBool     (id (b, "solo"), name + " Solo", false);
            p.addBool     (id (b, "on"), name + " On", true);
        }

        p.addFrequency ("xover_low", "Low Crossover", 40.0f, 1000.0f, 150.0f);
        p.addFrequency ("xover_mid", "Mid Crossover", 200.0f, 5000.0f, 1000.0f);
        p.addFrequency ("xover_high", "High Crossover", 600.0f, 12000.0f, 3500.0f);

        p.addChoice   ("bands", "Bands", { "3", "4" }, { "3", "4" }, 0);
        p.addDecibels ("offset", "Offset", -24.0f, 24.0f, 0.0f);
        p.addFloat    ("knee", "Knee", { 0.0f, 12.0f }, 6.0f,
                       [] (float db) { return juce::String (db, 1) + " dB"; }, rkfx::Units::parseNumber);
        p.addChoice   ("detect", "Detection", { "Peak", "RMS" }, { "peak", "rms" }, 1);
        p.addBool     ("release_auto", "Auto Release", false);
        p.addPercent  ("link", "Stereo Link", 0.0f, 100.0f, 100.0f);
        p.addBool     ("gain_auto", "Auto Gain", false);

        return p;
    }
}
