#pragma once

#include "plugin/ParamBuilder.h"

#include "UtilityEngine.h"

/** RKUtility parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace UtilityParams
{
    /** Gain: -60 to +24 dB, plus "-inf" (stored as -100) at the bottom of the travel. */
    inline juce::NormalisableRange<float> gainRange()
    {
        constexpr float offZone = 0.05f, lo = -60.0f, hi = 24.0f;
        return { UtilityEngine::kSilentDb, hi,
                 [=] (float, float, float x) { return x < offZone ? UtilityEngine::kSilentDb : lo + (hi - lo) * (x - offZone) / (1.0f - offZone); },
                 [=] (float, float, float v) { return v <= -80.0f ? 0.0f : offZone + (1.0f - offZone) * (juce::jlimit (lo, hi, v) - lo) / (hi - lo); },
                 [=] (float, float, float v) { return v <= -80.0f ? UtilityEngine::kSilentDb : juce::jlimit (lo, hi, v); } };
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;
        const auto ms = [] (float v) { return juce::String (v, 2) + " ms"; };

        p.addFloat    ("gain", "Gain", gainRange(), 0.0f,
                       [] (float db) { return db <= -80.0f ? juce::String ("-inf dB") : rkfx::Units::decibels (db); },
                       [] (const juce::String& t)
                       {
                           return t.containsIgnoreCase ("inf") ? UtilityEngine::kSilentDb : rkfx::Units::parseNumber (t);
                       });
        p.addPercent  ("pan", "Pan / Balance", -100.0f, 100.0f, 0.0f);
        p.addPercent  ("width", "Width", 0.0f, 200.0f, 100.0f);
        p.addFrequencyOrOff ("bass_mono", "Bass Mono", 40.0f, 300.0f, 0.0f, false);
        p.addChoice   ("pol_left", "Polarity Left", { "Normal", "Inverted" }, { "normal", "inverted" }, 0);
        p.addChoice   ("pol_right", "Polarity Right", { "Normal", "Inverted" }, { "normal", "inverted" }, 0);
        p.addBool     ("swap", "Swap Left and Right", false);
        p.addChoice   ("channel_mode", "Channel Mode",
                       { "Stereo", "Left Only", "Right Only", "Mid/Side Encode", "Mid/Side Decode" },
                       { "stereo", "left", "right", "msencode", "msdecode" }, 0);
        p.addFloat    ("delay_left", "Delay Left", { 0.0f, UtilityEngine::kMaxDelayMs, 0.01f }, 0.0f, ms, rkfx::Units::parseNumber);
        p.addFloat    ("delay_right", "Delay Right", { 0.0f, UtilityEngine::kMaxDelayMs, 0.01f }, 0.0f, ms, rkfx::Units::parseNumber);
        p.addBool     ("mute", "Mute", false);

        return p;
    }
}
