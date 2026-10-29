#pragma once

#include "plugin/ParamBuilder.h"

/** RKTrigger parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace TriggerParams
{
    inline constexpr int kTriggers = 2;

    inline juce::String id (int trigger, const char* name) { return "t" + juce::String (trigger + 1) + "_" + name; }

    inline juce::String noteName (int note)
    {
        return juce::MidiMessage::getMidiNoteName (note, true, true, 4) + " (" + juce::String (note) + ")";
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;
        using rkfx::Units::parseNumber;

        for (int t = 0; t < kTriggers; ++t)
        {
            const auto name = "Trigger " + juce::String (t + 1) + " ";
            p.addDecibels       (id (t, "threshold"), name + "Threshold", -60.0f, 0.0f, -30.0f);
            p.addFrequencyOrOff (id (t, "hpf"), name + "High-Pass", 20.0f, 500.0f, 0.0f, false);
            p.addFrequencyOrOff (id (t, "lpf"), name + "Low-Pass", 1000.0f, 16000.0f, 0.0f, true);
            p.addTime           (id (t, "scan"), name + "Scan", 1.0f, 10.0f, 2.0f);
            p.addTime           (id (t, "mask"), name + "Mask", 5.0f, 200.0f, 40.0f);
            p.addFloat          (id (t, "retrigger"), name + "Retrigger", { 2.0f, 24.0f, 1.0f }, 6.0f,
                                 [] (float db) { return db < 2.5f ? juce::String ("Off") : juce::String ((int) db) + " dB"; },
                                 [] (const juce::String& s) { return s.trim().equalsIgnoreCase ("off") ? 2.0f : parseNumber (s); });
            p.addDecibels       (id (t, "dynamics"), name + "Dynamics", 6.0f, 48.0f, 30.0f);
            p.addPercent        (id (t, "curve"), name + "Curve", -100.0f, 100.0f, 0.0f);
            p.addInt            (id (t, "vel_min"), name + "Velocity Min", 1, 127, 1);
            p.addInt            (id (t, "vel_max"), name + "Velocity Max", 1, 127, 127);
            p.addFloat          (id (t, "note"), name + "Note", { 0.0f, 127.0f, 1.0f }, t == 0 ? 36.0f : 38.0f,
                                 [] (float v) { return noteName ((int) v); },
                                 [] (const juce::String& s)
                                 {
                                     // "36", or a name such as "C1" / "D#2".
                                     const auto text = s.trim().upToFirstOccurrenceOf (" ", false, false);
                                     if (text.containsOnly ("0123456789"))
                                         return parseNumber (text);
                                     for (int n = 0; n < 128; ++n)
                                         if (juce::MidiMessage::getMidiNoteName (n, true, true, 4).equalsIgnoreCase (text))
                                             return (float) n;
                                     return 36.0f;
                                 });
            p.addInt            (id (t, "channel"), name + "Channel", 1, 16, 10);
            p.addTime           (id (t, "length"), name + "Length", 5.0f, 500.0f, 50.0f);
        }

        p.addChoice  ("mode", "Mode", { "Mono", "Dual" }, { "mono", "dual" }, 0);
        p.addPercent ("crosstalk", "Crosstalk", 0.0f, 100.0f, 50.0f);
        p.addChoice  ("audio", "Audio", { "Thru", "Mute" }, { "thru", "mute" }, 0);

        return p;
    }
}
