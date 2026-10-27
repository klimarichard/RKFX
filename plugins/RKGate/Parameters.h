#pragma once

#include "plugin/ParamBuilder.h"

#include "GateEngine.h"

/** RKGate parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace GateParams
{
    /** Range: 0 to -80 dB, plus "Closed" (stored as -100) at the bottom of the travel. */
    inline juce::NormalisableRange<float> rangeRange()
    {
        constexpr float closedZone = 0.05f;
        return { GateEngine::kClosedDb, 0.0f,
                 [] (float, float, float x)
                 {
                     return x < closedZone ? GateEngine::kClosedDb : -80.0f + 80.0f * (x - closedZone) / (1.0f - closedZone);
                 },
                 [] (float, float, float v)
                 {
                     return v <= -90.0f ? 0.0f : closedZone + (1.0f - closedZone) * (juce::jlimit (-80.0f, 0.0f, v) + 80.0f) / 80.0f;
                 },
                 [] (float, float, float v)
                 {
                     return v <= -90.0f ? GateEngine::kClosedDb : juce::jlimit (-80.0f, 0.0f, v);
                 } };
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;
        const auto plainDb = [] (float db) { return juce::String (db, 1) + " dB"; };

        p.addChoice   ("mode", "Mode", { "Gate", "Expander" }, { "gate", "expander" }, 0);
        p.addDecibels ("threshold", "Threshold", -80.0f, 0.0f, -50.0f);
        p.addFloat    ("ratio", "Ratio", rkfx::Units::logRange (1.5f, 10.0f), 2.0f,
                       [] (float r) { return "1:" + juce::String (r, 1); }, rkfx::Units::parseNumber);
        p.addFloat    ("range", "Range", rangeRange(), GateEngine::kClosedDb,
                       [] (float db) { return db <= -90.0f ? juce::String ("Closed") : rkfx::Units::decibels (db); },
                       [] (const juce::String& text)
                       {
                           const auto t = text.trim().toLowerCase();
                           if (t.startsWith ("closed") || t.startsWith ("off") || t.contains ("inf"))
                               return GateEngine::kClosedDb;
                           return -std::abs (rkfx::Units::parseNumber (text));
                       });
        p.addTime     ("attack", "Attack", 0.05f, 50.0f, 0.5f);
        p.addTime     ("hold", "Hold", 0.0f, 500.0f, 50.0f);
        p.addTime     ("release", "Release", 5.0f, 2000.0f, 100.0f);
        p.addFloat    ("hysteresis", "Hysteresis", { 0.0f, 12.0f }, 3.0f, plainDb, rkfx::Units::parseNumber);
        p.addChoice   ("sc_source", "Sidechain Source", { "Internal", "External" }, { "internal", "external" }, 0);
        p.addFrequencyOrOff ("key_hpf", "Key High-Pass", 20.0f, 5000.0f, 0.0f, false);
        p.addFrequencyOrOff ("key_lpf", "Key Low-Pass", 500.0f, 20000.0f, 0.0f, true);
        p.addBool     ("key_listen", "Key Listen", false);
        p.addBool     ("link", "Stereo Link", true);

        return p;
    }
}
