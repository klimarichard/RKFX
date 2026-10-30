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

        // Phase 2: the fixed strip's HPF, gate, EQ and compressor (appended).
        juce::StringArray keyNames { "Self" }, keyKeys { "self" };
        for (int ch = 0; ch < kChannels; ++ch)
        {
            keyNames.add ("In " + num (ch));
            keyKeys.add (num (ch));
        }
        for (int ch = 0; ch < kChannels; ++ch)
        {
            const auto name = channelName (ch);
            p.addBool      (channelId (ch, "hpf_on"), name + "HPF", false);
            p.addFrequency (channelId (ch, "hpf"), name + "HPF Frequency", 20.0f, 400.0f, 80.0f);
            p.addChoice    (channelId (ch, "hpf_slope"), name + "HPF Slope", { "12 dB", "24 dB" }, { "12", "24" }, 0);

            p.addBool     (channelId (ch, "gate_on"), name + "Gate", false);
            p.addDecibels (channelId (ch, "gate_thr"), name + "Gate Threshold", -80.0f, 0.0f, -50.0f);
            p.addFloat    (channelId (ch, "gate_range"), name + "Gate Range", { -80.0f, 0.0f }, -80.0f,
                           [] (float db) { return db <= -79.95f ? juce::String ("Closed") : rkfx::Units::decibels (db); },
                           [] (const juce::String& t) { return t.trim().equalsIgnoreCase ("closed") ? -80.0f : rkfx::Units::parseNumber (t); });
            p.addTime     (channelId (ch, "gate_att"), name + "Gate Attack", 0.1f, 50.0f, 1.0f);
            p.addTime     (channelId (ch, "gate_hold"), name + "Gate Hold", 0.0f, 500.0f, 50.0f);
            p.addTime     (channelId (ch, "gate_rel"), name + "Gate Release", 5.0f, 2000.0f, 150.0f);
            p.addChoice   (channelId (ch, "gate_key"), name + "Gate Key", keyNames, keyKeys, 0);

            p.addBool (channelId (ch, "eq_on"), name + "EQ", false);
            p.addChoice (channelId (ch, "eq_low_type"), name + "EQ Low Type", { "Shelf", "Bell" }, { "shelf", "bell" }, 0);
            p.addChoice (channelId (ch, "eq_high_type"), name + "EQ High Type", { "Shelf", "Bell" }, { "shelf", "bell" }, 0);
            const char* bands[] = { "low", "lmid", "hmid", "high" };
            const char* bandNames[] = { "Low", "Low Mid", "High Mid", "High" };
            const float freqs[] = { 100.0f, 250.0f, 2500.0f, 8000.0f };
            const float qs[] = { 0.7f, 1.0f, 1.0f, 0.7f };
            for (int b = 0; b < 4; ++b)
            {
                const auto prefix = juce::String ("eq_") + bands[b];
                const auto label = name + "EQ " + bandNames[b] + " ";
                p.addFrequency (channelId (ch, (prefix + "_freq").toRawUTF8()), label + "Frequency", 20.0f, 20000.0f, freqs[b]);
                p.addDecibels  (channelId (ch, (prefix + "_gain").toRawUTF8()), label + "Gain", -15.0f, 15.0f, 0.0f);
                p.addFloat     (channelId (ch, (prefix + "_q").toRawUTF8()), label + "Q", rkfx::Units::logRange (0.3f, 10.0f), qs[b],
                                [] (float q) { return juce::String (q, 2); }, rkfx::Units::parseNumber);
            }

            p.addBool     (channelId (ch, "comp_on"), name + "Comp", false);
            p.addDecibels (channelId (ch, "comp_thr"), name + "Comp Threshold", -60.0f, 0.0f, -20.0f);
            p.addFloat    (channelId (ch, "comp_ratio"), name + "Comp Ratio", rkfx::Units::logRange (1.0f, 20.0f), 3.0f,
                           [] (float r) { return juce::String (r, r < 10.0f ? 1 : 0) + ":1"; }, rkfx::Units::parseNumber);
            p.addDecibels (channelId (ch, "comp_knee"), name + "Comp Knee", 0.0f, 12.0f, 6.0f);
            p.addTime     (channelId (ch, "comp_att"), name + "Comp Attack", 0.1f, 100.0f, 10.0f);
            p.addTime     (channelId (ch, "comp_rel"), name + "Comp Release", 10.0f, 2000.0f, 150.0f);
            p.addDecibels (channelId (ch, "comp_makeup"), name + "Comp Makeup", 0.0f, 24.0f, 0.0f);
            p.addChoice   (channelId (ch, "comp_key"), name + "Comp Key", keyNames, keyKeys, 0);
        }

        for (int pair = 0; pair < kPairs; ++pair)
            p.addBool (linkId (pair), "Ch " + num (pair * 2) + "/" + num (pair * 2 + 1) + " Link", false);

        addPan    (p, "main_bal", "Main Balance");
        addTarget (p, "main_out", "Main Output", pairTarget (0));
        addTarget (p, "mono_out", "Mono Output", kTargetOff);

        return p;
    }
}
