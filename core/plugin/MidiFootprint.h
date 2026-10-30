#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <bitset>

namespace rkfx
{

/** What a plugin with MIDI output can send at its current settings: per
    channel, the notes and controllers, and whether it sends channel-wide
    messages (pitch bend, channel pressure), which act on every note there.
    Two sources conflict when they can send the same note or controller on
    the same channel, or when one sends channel-wide messages on a channel
    the other uses (RKMix warns about it). */
struct MidiFootprint
{
    std::array<std::bitset<128>, 16> notes, controllers;
    std::bitset<16> channelWide;

    void addNote (int channel, int note)    { notes[(size_t) ((channel - 1) & 15)].set ((size_t) (note & 127)); }
    void addController (int channel, int cc) { controllers[(size_t) ((channel - 1) & 15)].set ((size_t) (cc & 127)); }
    void addChannelWide (int channel)        { channelWide.set ((size_t) ((channel - 1) & 15)); }

    bool uses (int channel) const
    {
        const auto c = (size_t) ((channel - 1) & 15);
        return notes[c].any() || controllers[c].any() || channelWide[c];
    }

    bool isEmpty() const
    {
        for (int ch = 1; ch <= 16; ++ch)
            if (uses (ch))
                return false;
        return true;
    }

    /** Where @p a and @p b clash, as short phrases ("Ch 10, C1 (36)"); empty when they don't. */
    static juce::StringArray conflicts (const MidiFootprint& a, const MidiFootprint& b)
    {
        juce::StringArray where;
        for (int ch = 1; ch <= 16; ++ch)
        {
            const auto c = (size_t) (ch - 1);
            const auto prefix = "Ch " + juce::String (ch) + ", ";
            if ((a.channelWide[c] && b.uses (ch)) || (b.channelWide[c] && a.uses (ch)))
            {
                where.add (prefix + "pitch bend / pressure");
                continue;
            }
            const auto notes = a.notes[c] & b.notes[c];
            for (int n = 0; n < 128 && where.size() < 8; ++n)
                if (notes[(size_t) n])
                    where.add (prefix + juce::MidiMessage::getMidiNoteName (n, true, true, 3) + " (" + juce::String (n) + ")");
            const auto ccs = a.controllers[c] & b.controllers[c];
            for (int cc = 0; cc < 128 && where.size() < 8; ++cc)
                if (ccs[(size_t) cc])
                    where.add (prefix + "CC " + juce::String (cc));
        }
        return where;
    }

    /** A short summary: "Ch 10: C1 (36), D1 (38)". */
    juce::String describe() const
    {
        juce::StringArray parts;
        for (int ch = 1; ch <= 16; ++ch)
        {
            const auto c = (size_t) (ch - 1);
            if (! uses (ch))
                continue;
            juce::StringArray items;
            const auto count = notes[c].count();
            if (count > 4)
                items.add (count == 128 ? juce::String ("any note") : juce::String ((int) count) + " notes");
            else
                for (int n = 0; n < 128; ++n)
                    if (notes[c][(size_t) n])
                        items.add (juce::MidiMessage::getMidiNoteName (n, true, true, 3) + " (" + juce::String (n) + ")");
            for (int cc = 0; cc < 128; ++cc)
                if (controllers[c][(size_t) cc])
                    items.add ("CC " + juce::String (cc));
            if (channelWide[c])
                items.add ("bend/pressure");
            parts.add ("Ch " + juce::String (ch) + ": " + items.joinIntoString (", "));
            if (parts.size() == 3)
            {
                parts.add ("...");
                break;
            }
        }
        return parts.joinIntoString ("; ");
    }
};

} // namespace rkfx
