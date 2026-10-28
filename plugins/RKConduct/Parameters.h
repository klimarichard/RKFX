#pragma once

#include "plugin/ParamBuilder.h"

/** RKConduct parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. tap and stop are buttons: each press
    (off to on) acts once. */
namespace ConductParams
{
    inline constexpr double tapBeats[] = { 4.0, 3.0, 2.0, 1.5, 1.0, 0.75, 0.5, 0.25 };

    /** A MIDI note (or Off, below 0), shown as its name (C3 = 60). */
    inline void addNote (rkfx::ParamBuilder& p, const juce::String& id, const juce::String& name, float defaultNote)
    {
        p.addFloat (id, name, { -1.0f, 127.0f, 1.0f }, defaultNote,
                    [] (float v) { return v < 0.0f ? juce::String ("Off") : juce::MidiMessage::getMidiNoteName (juce::roundToInt (v), true, true, 3); },
                    [] (const juce::String& text)
                    {
                        if (text.trim().equalsIgnoreCase ("off"))
                            return -1.0f;
                        for (int n = 0; n < 128; ++n)
                            if (juce::MidiMessage::getMidiNoteName (n, true, true, 3).equalsIgnoreCase (text.trim()))
                                return (float) n;
                        return juce::jlimit (-1.0f, 127.0f, text.getFloatValue());
                    });
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice ("tap_every", "Tap Every", { "1/1", "1/2 D", "1/2", "1/4 D", "1/4", "1/8 D", "1/8", "1/16" },
                     { "1/1", "1/2d", "1/2", "1/4d", "1/4", "1/8d", "1/8", "1/16" }, 4);
        p.addFloat  ("tempo", "Tempo", { 20.0f, 300.0f, 0.1f }, 120.0f,
                     [] (float v) { return juce::String (v, 1) + " BPM"; },
                     [] (const juce::String& t) { return juce::jlimit (20.0f, 300.0f, t.getFloatValue()); });
        p.addBool   ("file_tempo", "Start at the File's Tempo", true);
        p.addInt    ("count_in", "Count-in Taps", 0, 8, 0);
        p.addBool   ("loop", "Loop", false);
        addNote     (p, "key1", "Tap Key 1", 36.0f);
        addNote     (p, "key2", "Tap Key 2", 38.0f);
        p.addFloat  ("tap_cc", "Tap Controller", { -1.0f, 127.0f, 1.0f }, -1.0f,
                     [] (float v) { return v < 0.0f ? juce::String ("Off") : "CC " + juce::String (juce::roundToInt (v)); },
                     [] (const juce::String& t) { return t.trim().equalsIgnoreCase ("off") ? -1.0f : juce::jlimit (-1.0f, 127.0f, t.retainCharacters ("0123456789").getFloatValue()); });
        p.addBool   ("tap", "Tap", false);
        p.addBool   ("stop", "Stop", false);

        return p;
    }
}
