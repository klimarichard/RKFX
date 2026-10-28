#pragma once

#include "plugin/ParamBuilder.h"

/** RKPreamp parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace PreampParams
{
    inline constexpr int oversamplingFactors[] = { 1, 2, 4 };

    /** An amp knob: 0 to 10, shown with one decimal. */
    inline void addAmpKnob (rkfx::ParamBuilder& p, const juce::String& id, const juce::String& name, float defaultValue)
    {
        p.addFloat (id, name, { 0.0f, 10.0f }, defaultValue,
                    [] (float v) { return juce::String (v, 1); },
                    [] (const juce::String& text) { return juce::jlimit (0.0f, 10.0f, text.getFloatValue()); });
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice ("voicing", "Voicing", { "Clean American", "British Crunch", "Top Boost", "Hot-Rod" },
                     { "clean", "crunch", "topboost", "hotrod" }, 1);
        addAmpKnob (p, "gain", "Gain", 5.0f);
        addAmpKnob (p, "bass", "Bass", 5.0f);
        addAmpKnob (p, "middle", "Middle", 5.0f);
        addAmpKnob (p, "treble", "Treble", 5.0f);
        addAmpKnob (p, "presence", "Presence", 5.0f);
        p.addBool   ("bright", "Bright", false);
        p.addChoice ("cab", "Cab Filter", { "Off", "Small (1x12)", "Medium (2x12)", "Large (4x12)", "Bass (1x15)" },
                     { "off", "small", "medium", "large", "bass" }, 0);
        p.addChoice ("oversampling", "Oversampling", { "Off", "2x", "4x" }, { "off", "2x", "4x" }, 1);

        return p;
    }
}
