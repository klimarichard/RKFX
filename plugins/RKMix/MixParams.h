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

    /** The ID suffixes of a processing chain (bus, main, mono), in the order
        RKMixProcessor reads them (ChainField). */
    inline const std::vector<const char*>& chainFields()
    {
        static const std::vector<const char*> fields {
            "eq_on", "hpf_on", "hpf", "hpf_slope", "lpf_on", "lpf", "lpf_slope", "eq_low_type", "eq_high_type",
            "eq_b1_freq", "eq_b1_gain", "eq_b1_q", "eq_b2_freq", "eq_b2_gain", "eq_b2_q", "eq_b3_freq", "eq_b3_gain", "eq_b3_q",
            "eq_b4_freq", "eq_b4_gain", "eq_b4_q", "eq_b5_freq", "eq_b5_gain", "eq_b5_q", "eq_b6_freq", "eq_b6_gain", "eq_b6_q",
            "comp_on", "comp_thr", "comp_ratio", "comp_knee", "comp_att", "comp_rel", "comp_auto", "comp_makeup",
            "lim_on", "lim_ceiling", "lim_rel"
        };
        return fields;
    }

    inline void addProcessing (rkfx::ParamBuilder& p, int chain)
    {
        using namespace mix;
        const auto name = chain == kMainChain ? juce::String ("Main ") : chain == kMonoChain ? juce::String ("Mono ") : "Bus " + num (chain) + " ";
        const auto id = [chain] (const char* suffix) { return chainId (chain, suffix); };

        p.addBool      (id ("eq_on"), name + "EQ", false);
        p.addBool      (id ("hpf_on"), name + "HPF", false);
        p.addFrequency (id ("hpf"), name + "HPF Frequency", 20.0f, 400.0f, 40.0f);
        p.addChoice    (id ("hpf_slope"), name + "HPF Slope", { "12 dB", "24 dB" }, { "12", "24" }, 0);
        p.addBool      (id ("lpf_on"), name + "LPF", false);
        p.addFrequency (id ("lpf"), name + "LPF Frequency", 1000.0f, 20000.0f, 16000.0f);
        p.addChoice    (id ("lpf_slope"), name + "LPF Slope", { "12 dB", "24 dB" }, { "12", "24" }, 0);
        p.addChoice    (id ("eq_low_type"), name + "EQ Low Type", { "Shelf", "Bell" }, { "shelf", "bell" }, 0);
        p.addChoice    (id ("eq_high_type"), name + "EQ High Type", { "Shelf", "Bell" }, { "shelf", "bell" }, 0);
        const float freqs[] = { 80.0f, 160.0f, 400.0f, 1000.0f, 2500.0f, 6000.0f };
        for (int b = 0; b < 6; ++b)
        {
            const auto band = "eq_b" + juce::String (b + 1);
            const auto label = name + "EQ " + juce::String (b + 1) + " ";
            const auto q = b == 0 || b == 5 ? 0.7f : 1.0f;
            p.addFrequency (id ((band + "_freq").toRawUTF8()), label + "Frequency", 20.0f, 20000.0f, freqs[b]);
            p.addDecibels  (id ((band + "_gain").toRawUTF8()), label + "Gain", -15.0f, 15.0f, 0.0f);
            p.addFloat     (id ((band + "_q").toRawUTF8()), label + "Q", rkfx::Units::logRange (0.3f, 10.0f), q,
                            [] (float v) { return juce::String (v, 2); }, rkfx::Units::parseNumber);
        }

        p.addBool     (id ("comp_on"), name + "Comp", false);
        p.addDecibels (id ("comp_thr"), name + "Comp Threshold", -60.0f, 0.0f, -20.0f);
        p.addFloat    (id ("comp_ratio"), name + "Comp Ratio", rkfx::Units::logRange (1.0f, 20.0f), 2.0f,
                       [] (float r) { return juce::String (r, r < 10.0f ? 1 : 0) + ":1"; }, rkfx::Units::parseNumber);
        p.addDecibels (id ("comp_knee"), name + "Comp Knee", 0.0f, 12.0f, 6.0f);
        p.addTime     (id ("comp_att"), name + "Comp Attack", 0.1f, 100.0f, 30.0f);
        p.addTime     (id ("comp_rel"), name + "Comp Release", 10.0f, 2000.0f, 150.0f);
        p.addBool     (id ("comp_auto"), name + "Comp Auto Release", true);
        p.addDecibels (id ("comp_makeup"), name + "Comp Makeup", 0.0f, 24.0f, 0.0f);

        p.addBool     (id ("lim_on"), name + "Limit", false);
        p.addDecibels (id ("lim_ceiling"), name + "Limit Ceiling", -20.0f, -0.1f, -1.0f);
        p.addTime     (id ("lim_rel"), name + "Limit Release", 10.0f, 1000.0f, 100.0f);
    }
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

        // Phase 3: buses, and every channel's sends, taps and group assigns (appended).
        juce::StringArray tapNames, tapKeys, sendTapNames { "Bus default" }, sendTapKeys { "default" };
        for (int t = 0; t < kNumTaps; ++t)
        {
            tapNames.add (tapName ((Tap) t));
            tapKeys.add (tapKey ((Tap) t));
            sendTapNames.add (tapName ((Tap) t));
            sendTapKeys.add (tapKey ((Tap) t));
        }

        for (int bus = 0; bus < kBuses; ++bus)
        {
            const auto name = busName (bus);
            p.addChoice (busId (bus, "type"), name + "Type", { "Group", "Aux", "FX" }, { "group", "aux", "fx" }, (int) BusType::aux);
            p.addChoice (busId (bus, "format"), name + "Format", { "Mono", "Stereo" }, { "mono", "stereo" }, 1);
            addTarget   (p, busId (bus, "out"), name + "Output", kTargetOff);
            p.addBool   (busId (bus, "main"), name + "Main", true);
            p.addChoice (busId (bus, "tap"), name + "Default Tap", tapNames, tapKeys, (int) Tap::preFader);
            p.addChoice (busId (bus, "panmode"), name + "Pan", { "Centre", "Follow", "Mirror" }, { "centre", "follow", "mirror" },
                         (int) PanMode::follow);
            addPan      (p, busId (bus, "pan"), name + "Pan");
            for (int to = 0; to < kBuses; ++to)
                addFader (p, busSendId (bus, to), name + "Send " + num (to), kFaderMinDb);
            for (int to = 0; to < kBuses; ++to)
                p.addBool (busAssignId (bus, to), name + "To Group " + num (to), false);
        }

        for (int ch = 0; ch < kChannels; ++ch)
        {
            const auto name = channelName (ch);
            for (int bus = 0; bus < kBuses; ++bus)
                addFader (p, channelSendId (ch, bus), name + "Send " + num (bus), kFaderMinDb);
            for (int bus = 0; bus < kBuses; ++bus)
                p.addChoice (channelTapId (ch, bus), name + "Tap " + num (bus), sendTapNames, sendTapKeys, 0);
            for (int bus = 0; bus < kBuses; ++bus)
                p.addBool (channelAssignId (ch, bus), name + "To Group " + num (bus), false);
        }

        // The buses', the main's and the mono sum's EQ, compressor and limiter (appended).
        for (int chain = 0; chain < kProcessingChains; ++chain)
            addProcessing (p, chain);

        // MIDI in passed on to the MIDI output (appended).
        p.addBool ("midi_thru", "MIDI Thru", false);

        // Solo, the cue output, talkback, the oscillator (appended).
        for (int ch = 0; ch < kChannels; ++ch)
            p.addBool (channelId (ch, "solosafe"), channelName (ch) + "Solo Safe", false);
        for (int bus = 0; bus < kBuses; ++bus)
            p.addChoice (busId (bus, "solosafe"), busName (bus) + "Solo Safe", { "Auto", "On", "Off" }, { "auto", "on", "off" }, 0);
        p.addChoice ("solo_mode", "Solo Mode", { "Cue", "In place" }, { "cue", "in_place" }, 0);
        p.addChoice ("solo_tap", "Solo Tap", { "PFL", "AFL" }, { "pfl", "afl" }, 0);
        addTarget   (p, "cue_out", "Cue Output", kTargetOff);
        juce::StringArray sourceNames { "Silence", "Main" }, sourceKeys { "silence", "main" };
        for (int bus = 0; bus < kBuses; ++bus)
        {
            sourceNames.add ("Bus " + num (bus));
            sourceKeys.add ("bus" + num (bus));
        }
        p.addChoice ("cue_src", "Cue Source", sourceNames, sourceKeys, 1);

        juce::StringArray inputNames { "Off" }, inputKeys { "off" };
        for (int ch = 0; ch < kChannels; ++ch)
        {
            inputNames.add ("In " + num (ch));
            inputKeys.add (num (ch));
        }
        p.addChoice ("talk_src", "Talk Source", inputNames, inputKeys, 0);
        addFader    (p, "talk_level", "Talk Level");
        p.addBool   ("talk_latch", "Talk Latch", false);
        for (int bus = 0; bus < kBuses; ++bus)
            p.addBool ("talk_to" + num (bus), "Talk to Bus " + num (bus), true);
        p.addBool   ("talk_main", "Talk to Main", false);

        p.addBool      ("osc_on", "Oscillator", false);
        p.addChoice    ("osc_type", "Oscillator Signal", { "Sine", "Pink noise" }, { "sine", "pink" }, 0);
        p.addFrequency ("osc_freq", "Oscillator Frequency", 20.0f, 20000.0f, 1000.0f);
        p.addDecibels  ("osc_level", "Oscillator Level", -60.0f, 0.0f, -18.0f);
        for (int bus = 0; bus < kBuses; ++bus)
            p.addBool ("osc_to" + num (bus), "Oscillator to Bus " + num (bus), false);
        p.addBool      ("osc_main", "Oscillator to Main", false);
        addTarget      (p, "osc_out", "Oscillator Output", kTargetOff);

        // Scenes: the glide, and the strips a recall leaves alone (appended).
        p.addTime ("scene_glide", "Scene Glide", 0.0f, 5000.0f, 100.0f);
        for (int ch = 0; ch < kChannels; ++ch)
            p.addBool (channelId (ch, "recallsafe"), channelName (ch) + "Recall Safe", false);
        for (int bus = 0; bus < kBuses; ++bus)
            p.addBool (busId (bus, "recallsafe"), busName (bus) + "Recall Safe", false);

        // Direct outputs: a channel straight to an output, at a tap (appended).
        for (int ch = 0; ch < kChannels; ++ch)
        {
            addTarget   (p, channelId (ch, "direct_out"), channelName (ch) + "Direct Output", kTargetOff);
            p.addChoice (channelId (ch, "direct_tap"), channelName (ch) + "Direct Tap", tapNames, tapKeys, (int) Tap::input);
        }

        return p;
    }
}
