#pragma once

#include "dsp/TempoSync.h"
#include "plugin/ParamBuilder.h"

/** RKRingMod parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace RingModParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        juce::StringArray divisionNames, divisionKeys;
        for (int i = 0; i < rkfx::SyncDivision::kCount; ++i)
        {
            divisionNames.add (rkfx::SyncDivision::names[i]);
            divisionKeys.add (rkfx::SyncDivision::keys[i]);
        }

        p.addChoice    ("mode", "Mode", { "Free", "Tracked", "Synced" }, { "free", "tracked", "synced" }, 0);
        p.addChoice    ("wave", "Wave", { "Sine", "Triangle", "Square" }, { "sine", "triangle", "square" }, 0);
        p.addFrequency ("frequency", "Frequency", 1.0f, 4000.0f, 440.0f);
        p.addFloat     ("interval", "Interval", { -24.0f, 24.0f, 0.01f }, 7.0f,
                        [] (float st) { return (st > 0.004f ? "+" : "") + juce::String (st, 2) + " st"; },
                        [] (const juce::String& text) { return juce::jlimit (-24.0f, 24.0f, text.getFloatValue()); });
        p.addChoice    ("division", "Sync Division", divisionNames, divisionKeys, rkfx::SyncDivision::index (16));
        p.addChoice    ("range", "Range", { "Bass", "Guitar", "Violin" }, { "bass", "guitar", "violin" }, 1);
        p.addPercent   ("lfo_depth", "LFO Depth", 0.0f, 100.0f, 0.0f);
        p.addFrequency ("lfo_rate", "LFO Rate", 0.05f, 10.0f, 1.0f);

        return p;
    }
}
