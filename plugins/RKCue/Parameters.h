#pragma once

#include "plugin/ParamBuilder.h"

/** RKCue parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. The globals come first, then each slot's
    settings (s1_low ... s16_channel), then the Play and Stop buttons. The
    buttons act on each press (off to on); Play's release is the key-up of
    a Gate slot. */
namespace CueParams
{
    inline constexpr int kSlots = 16;

    /** "s3_low": slot 3's Low Key (slots count from 1). */
    inline juce::String slotId (int slot, const char* name) { return "s" + juce::String (slot + 1) + "_" + name; }
    inline juce::String playId (int slot) { return "play_" + juce::String (slot + 1); }
    inline juce::String stopId (int slot) { return "stop_" + juce::String (slot + 1); }

    inline juce::String noteName (int note) { return juce::MidiMessage::getMidiNoteName (note, true, true, 3); }   // C3 = 60

    /** A MIDI note, shown as its name. */
    inline void addNote (rkfx::ParamBuilder& p, const juce::String& id, const juce::String& name, float defaultNote)
    {
        p.addFloat (id, name, { 0.0f, 127.0f, 1.0f }, defaultNote,
                    [] (float v) { return noteName (juce::roundToInt (v)); },
                    [] (const juce::String& text)
                    {
                        for (int n = 0; n < 128; ++n)
                            if (noteName (n).equalsIgnoreCase (text.trim()))
                                return (float) n;
                        return juce::jlimit (0.0f, 127.0f, text.getFloatValue());
                    });
    }

    inline juce::StringArray channelNames (const juce::String& first)
    {
        juce::StringArray names { first };
        for (int ch = 1; ch <= 16; ++ch)
            names.add (juce::String (ch));
        return names;
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice ("tempo_source", "Tempo Source", { "File", "Host", "Fixed" }, { "file", "host", "fixed" }, 0);
        p.addFloat  ("tempo", "Tempo", { 20.0f, 300.0f, 0.1f }, 120.0f,
                     [] (float v) { return juce::String (v, 1) + " BPM"; },
                     [] (const juce::String& t) { return juce::jlimit (20.0f, 300.0f, t.getFloatValue()); });
        p.addBool   ("thru", "Thru", true);
        {
            auto keys = channelNames ("any");
            p.addChoice ("trig_channel", "Trigger Channel", channelNames ("Any"), keys, 0);
        }
        p.addBool   ("stop_all", "Stop All", false);

        for (int s = 0; s < kSlots; ++s)
        {
            const auto name = "Slot " + juce::String (s + 1) + " ";
            const auto key = (float) (36 + s);   // C1, C#1, ...
            addNote     (p, slotId (s, "low"), name + "Low Key", key);
            addNote     (p, slotId (s, "high"), name + "High Key", key);
            addNote     (p, slotId (s, "root"), name + "Root Key", key);
            p.addBool   (slotId (s, "pitch"), name + "Track Pitch", false);
            p.addInt    (slotId (s, "transpose"), name + "Transpose", -24, 24, 0, "st");
            p.addBool   (slotId (s, "drums"), name + "Keep Drums", true);
            p.addChoice (slotId (s, "mode"), name + "Play Mode", { "One-shot", "Gate", "Toggle" }, { "oneshot", "gate", "toggle" }, 0);
            p.addBool   (slotId (s, "loop"), name + "Loop", false);
            p.addBool   (slotId (s, "poly"), name + "Poly", false);
            p.addPercent (slotId (s, "velocity"), name + "Velocity", 0.0f, 100.0f, 100.0f);
            p.addChoice (slotId (s, "channel"), name + "Channel", channelNames ("As recorded"), channelNames ("recorded"), 0);
        }

        for (int s = 0; s < kSlots; ++s)
            p.addBool (playId (s), "Play " + juce::String (s + 1), false);
        for (int s = 0; s < kSlots; ++s)
            p.addBool (stopId (s), "Stop " + juce::String (s + 1), false);

        return p;
    }
}
