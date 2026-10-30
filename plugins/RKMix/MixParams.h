#pragma once

#include "plugin/ParamBuilder.h"

#include "MixLayout.h"

/** RKMix's parameters. IDs are permanent: never rename, reuse or reorder;
    only append.

    Host parameters (the processor): the channel faders and mutes, the bus
    faders and mutes, the main and mono faders and mutes, 32 assignables,
    the scene and talkback. Everything else is a mix setting (MixSettings):
    saved with the mix, not seen by the host. */
namespace MixParams
{
    /** The fader law as a parameter range: -100 dB (silence) to +10 dB. */
    inline juce::NormalisableRange<float> faderRange()
    {
        return { mix::kFaderMinDb, mix::kFaderMaxDb,
                 [] (float, float, float position) { return mix::faderDbAt (position); },
                 [] (float, float, float db) { return mix::faderPositionOf (db); },
                 [] (float, float, float db) { return juce::jlimit (mix::kFaderMinDb, mix::kFaderMaxDb, db); } };
    }

    inline juce::String faderText (float db)
    {
        return db <= mix::kFaderMinDb + 0.001f ? juce::String ("-inf dB") : rkfx::Units::decibels (db);
    }

    inline float faderFromText (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        if (t.contains ("inf") || t == "off")
            return mix::kFaderMinDb;
        return juce::jlimit (mix::kFaderMinDb, mix::kFaderMaxDb, rkfx::Units::parseNumber (t));
    }

    inline void addFader (rkfx::ParamBuilder& p, const juce::String& id, const juce::String& name, float defaultDb = 0.0f)
    {
        p.addFloat (id, name, faderRange(), defaultDb, faderText, faderFromText);
    }

    inline void addPan (rkfx::ParamBuilder& p, const juce::String& id, const juce::String& name)
    {
        p.addFloat (id, name, { -100.0f, 100.0f }, 0.0f, mix::panText, mix::panFromText);
    }

    inline void addTarget (rkfx::ParamBuilder& p, const juce::String& id, const juce::String& name, int defaultTarget)
    {
        juce::StringArray names, keys;
        for (int t = 0; t < mix::kNumTargets; ++t)
        {
            names.add (mix::targetName (t));
            keys.add (mix::targetKey (t));
        }
        p.addChoice (id, name, names, keys, defaultTarget);
    }

    inline juce::String channelName (int ch) { return "Ch " + mix::num (ch) + " "; }
    inline juce::String busName (int bus)    { return "Bus " + mix::num (bus) + " "; }

    /** The host parameters, in their permanent order. */
    inline rkfx::ParamBuilder createHost()
    {
        using namespace mix;
        rkfx::ParamBuilder p;
        for (int ch = 0; ch < kChannels; ++ch)
            addFader (p, channelFaderId (ch), channelName (ch) + "Fader");
        for (int ch = 0; ch < kChannels; ++ch)
            p.addBool (channelMuteId (ch), channelName (ch) + "Mute", false);
        for (int bus = 0; bus < kBuses; ++bus)
            addFader (p, busFaderId (bus), busName (bus) + "Fader");
        for (int bus = 0; bus < kBuses; ++bus)
            p.addBool (busMuteId (bus), busName (bus) + "Mute", false);
        addFader (p, "main_fader", "Main Fader");
        p.addBool ("main_mute", "Main Mute", false);
        addFader (p, "mono_fader", "Mono Fader");
        p.addBool ("mono_mute", "Mono Mute", false);
        for (int slot = 0; slot < 32; ++slot)
            p.addPercent (assignId (slot), "Assign " + num (slot), 0.0f, 100.0f, 0.0f);
        p.addInt ("scene", "Scene", 1, 16, 1);
        p.addBool ("talk", "Talk", false);
        return p;
    }

    /** The mix settings (not host parameters). Keys in the mix state. */
    inline rkfx::ParamBuilder createSettings()
    {
        using namespace mix;
        rkfx::ParamBuilder p;

        for (int ch = 0; ch < kChannels; ++ch)
        {
            const auto name = channelName (ch);
            p.addDecibels (channelId (ch, "trim"), name + "Trim", -24.0f, 24.0f, 0.0f);
            p.addBool     (channelId (ch, "pol"), name + "Polarity", false);
            p.addBool     (channelId (ch, "polr"), name + "Polarity R", false);
            p.addBool     (channelId (ch, "delay_on"), name + "Delay", false);
            p.addFloat    (channelId (ch, "delay"), name + "Delay Time", { 0.0f, 20.0f, 0.01f }, 0.0f,
                           [] (float ms) { return juce::String (ms, 2) + " ms"; }, rkfx::Units::parseTime);
            addPan        (p, channelId (ch, "pan"), name + "Pan");
            p.addBool     (channelId (ch, "main"), name + "Main", true);
        }

        for (int pair = 0; pair < kPairs; ++pair)
            p.addBool (linkId (pair), "Ch " + num (pair * 2) + "/" + num (pair * 2 + 1) + " Link", false);

        addPan    (p, "main_bal", "Main Balance");
        addTarget (p, "main_out", "Main Output", pairTarget (0));
        addTarget (p, "mono_out", "Mono Output", kTargetOff);

        return p;
    }
}
