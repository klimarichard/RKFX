#pragma once

#include "plugin/ParamBuilder.h"

#include "RemapEngine.h"

/** RKRemap parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. Per output k (1..64): source, trim, mute,
    polarity, mono; then the input pair links, the output pair links, and
    the input pairs' L/R or M/S. */
namespace RemapParams
{
    inline juce::String outputId (int output, const char* name) { return "out_" + juce::String (output + 1) + "_" + name; }
    inline juce::String inputLinkId (int pair) { return "in_link_" + juce::String (pair + 1); }
    inline juce::String outputLinkId (int pair) { return "out_link_" + juce::String (pair + 1); }
    inline juce::String inputMidSideId (int pair) { return "in_ms_" + juce::String (pair + 1); }

    /** The Source choices: Off, In 1 .. In 64, In 1/2 .. In 63/64. */
    inline juce::String sourceName (int value)
    {
        if (value <= RemapEngine::kSourceOff)
            return "Off";
        if (value <= RemapEngine::kChannels)
            return "In " + juce::String (value);
        const auto left = (value - RemapEngine::kFirstPairSource) * 2 + 1;
        return "In " + juce::String (left) + "/" + juce::String (left + 1);
    }

    inline juce::String sourceKey (int value)
    {
        if (value <= RemapEngine::kSourceOff)
            return "off";
        if (value <= RemapEngine::kChannels)
            return juce::String (value);
        const auto left = (value - RemapEngine::kFirstPairSource) * 2 + 1;
        return juce::String (left) + "/" + juce::String (left + 1);
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        juce::StringArray names, keys;
        for (int v = 0; v < RemapEngine::kFirstPairSource + RemapEngine::kPairs; ++v)
        {
            names.add (sourceName (v));
            keys.add (sourceKey (v));
        }

        for (int k = 0; k < RemapEngine::kChannels; ++k)
        {
            const auto name = "Out " + juce::String (k + 1) + " ";
            p.addChoice   (outputId (k, "source"), name + "Source", names, keys, k + 1);   // straight through: In k -> Out k
            p.addDecibels (outputId (k, "trim"), name + "Trim", -24.0f, 12.0f, 0.0f);
            p.addBool     (outputId (k, "mute"), name + "Mute", false);
            p.addBool     (outputId (k, "polarity"), name + "Polarity", false);
            p.addBool     (outputId (k, "mono"), name + "Mono", false);
        }
        for (int pair = 0; pair < RemapEngine::kPairs; ++pair)
            p.addBool (inputLinkId (pair), "In " + juce::String (pair * 2 + 1) + "/" + juce::String (pair * 2 + 2) + " Link", false);
        for (int pair = 0; pair < RemapEngine::kPairs; ++pair)
            p.addBool (outputLinkId (pair), "Out " + juce::String (pair * 2 + 1) + "/" + juce::String (pair * 2 + 2) + " Link", false);

        // Added later (appended): a linked input pair as mid/side.
        for (int pair = 0; pair < RemapEngine::kPairs; ++pair)
            p.addChoice (inputMidSideId (pair), "In " + juce::String (pair * 2 + 1) + "/" + juce::String (pair * 2 + 2) + " Pair",
                         { "L/R", "M/S" }, { "lr", "ms" }, 0);

        return p;
    }
}
