#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>

/** RKMix's shape and the helpers shared by the engine, the processor and the
    editor (GUI-free): sizes, parameter IDs, the fader law, pan laws and
    output targets. IDs are permanent: never rename or reuse them. */
namespace mix
{
    constexpr int kChannels = 32;
    constexpr int kPairs = kChannels / 2;
    constexpr int kBuses = 16;

    //==============================================================================
    // Output targets (main, mono, buses): 0 = off, 1..32 = one output,
    // 33..48 = an output pair (Out 1/2 .. Out 31/32).
    constexpr int kTargetOff = 0;
    constexpr int kFirstPairTarget = kChannels + 1;
    constexpr int kNumTargets = kFirstPairTarget + kPairs;

    constexpr bool isPairTarget (int target) noexcept { return target >= kFirstPairTarget; }

    /** The first output a target writes (0-based), or -1 for off. */
    constexpr int firstOutputOf (int target) noexcept
    {
        return target <= kTargetOff ? -1 : isPairTarget (target) ? (target - kFirstPairTarget) * 2 : target - 1;
    }

    constexpr int pairTarget (int firstOutput) noexcept { return kFirstPairTarget + firstOutput / 2; }

    inline juce::String targetName (int target)
    {
        if (target <= kTargetOff)
            return "Off";
        const auto first = firstOutputOf (target) + 1;
        return isPairTarget (target) ? "Out " + juce::String (first) + "/" + juce::String (first + 1)
                                     : "Out " + juce::String (first);
    }

    inline juce::String targetKey (int target)
    {
        if (target <= kTargetOff)
            return "off";
        const auto first = firstOutputOf (target) + 1;
        return isPairTarget (target) ? juce::String (first) + "/" + juce::String (first + 1) : juce::String (first);
    }

    //==============================================================================
    // The fader law: position 0..1 to dB, -100 dB (the bottom) is silence.
    // 0 dB sits at three quarters of the travel, +10 dB at the top.
    constexpr float kFaderMinDb = -100.0f;
    constexpr float kFaderMaxDb = 10.0f;

    namespace detail
    {
        struct LawPoint { float position, db; };
        constexpr std::array<LawPoint, 7> kFaderLaw { { { 0.0f, -100.0f }, { 0.1f, -60.0f }, { 0.25f, -40.0f },
                                                        { 0.45f, -20.0f }, { 0.6f, -10.0f }, { 0.75f, 0.0f },
                                                        { 1.0f, 10.0f } } };
    }

    inline float faderDbAt (float position) noexcept
    {
        position = std::clamp (position, 0.0f, 1.0f);
        for (size_t i = 1; i < detail::kFaderLaw.size(); ++i)
        {
            const auto& a = detail::kFaderLaw[i - 1];
            const auto& b = detail::kFaderLaw[i];
            if (position <= b.position)
                return a.db + (b.db - a.db) * (position - a.position) / (b.position - a.position);
        }
        return kFaderMaxDb;
    }

    inline float faderPositionOf (float db) noexcept
    {
        db = std::clamp (db, kFaderMinDb, kFaderMaxDb);
        for (size_t i = 1; i < detail::kFaderLaw.size(); ++i)
        {
            const auto& a = detail::kFaderLaw[i - 1];
            const auto& b = detail::kFaderLaw[i];
            if (db <= b.db)
                return a.position + (b.position - a.position) * (db - a.db) / (b.db - a.db);
        }
        return 1.0f;
    }

    /** Linear gain of a fader or send level: the bottom is exact silence, and
        values within 0.001 dB of 0 are exactly unity (bit-exact routes). */
    inline float faderGain (float db) noexcept
    {
        if (db <= kFaderMinDb + 0.001f)
            return 0.0f;
        if (std::abs (db) < 0.001f)
            return 1.0f;
        return std::pow (10.0f, db * 0.05f);
    }

    /** A trim (no silence at the bottom), exactly unity near 0 dB. */
    inline float trimGain (float db) noexcept
    {
        return std::abs (db) < 0.001f ? 1.0f : std::pow (10.0f, db * 0.05f);
    }

    //==============================================================================
    // Pan: -1 (left) .. +1 (right).

    /** A mono source into a stereo bus: constant power, -3 dB each side at the centre. */
    inline void monoPanGains (float pan, float& left, float& right) noexcept
    {
        const auto angle = (std::clamp (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * 3.14159265358979323846f;
        left = std::cos (angle);
        right = std::sin (angle);
        if (pan <= -1.0f) right = 0.0f;   // exact at the ends
        if (pan >= 1.0f)  left = 0.0f;
    }

    /** A stereo source's balance: the far side turns down, the near side stays at unity. */
    inline void balanceGains (float pan, float& left, float& right) noexcept
    {
        pan = std::clamp (pan, -1.0f, 1.0f);
        left = pan > 0.0f ? 1.0f - pan : 1.0f;
        right = pan < 0.0f ? 1.0f + pan : 1.0f;
    }

    inline juce::String panText (float percent)
    {
        const auto whole = juce::roundToInt (percent);
        if (whole == 0)
            return "C";
        return (whole < 0 ? "L" : "R") + juce::String (std::abs (whole));
    }

    inline float panFromText (const juce::String& text)
    {
        const auto t = text.trim().toUpperCase();
        if (t.isEmpty() || t == "C")
            return 0.0f;
        const auto amount = std::abs (t.retainCharacters ("0123456789.").getFloatValue());
        if (t.startsWith ("L") || t.startsWith ("-"))
            return -amount;
        return amount;
    }

    //==============================================================================
    // Buses and taps

    enum class BusType { group = 0, aux = 1, fx = 2 };

    inline juce::String busTypeName (BusType t)
    {
        return t == BusType::group ? "Group" : t == BusType::aux ? "Aux" : "FX";
    }

    /** How an Aux bus places the channels: centred, as their pans (the
        audience's view), or mirrored (the band's view, facing the audience). */
    enum class PanMode { centre = 0, follow, mirror };

    /** Where a send takes a channel's signal, in strip order. */
    enum class Tap { input = 0, preEq, preComp, preInserts, preFader, postFader };
    constexpr int kNumTaps = 6;

    inline juce::String tapName (Tap t)
    {
        switch (t)
        {
            case Tap::input:      return "Input";
            case Tap::preEq:      return "Pre-EQ";
            case Tap::preComp:    return "Pre-Comp";
            case Tap::preInserts: return "Pre-Inserts";
            case Tap::preFader:   return "Pre-Fader";
            case Tap::postFader:  return "Post-Fader";
        }
        return {};
    }

    inline juce::String tapKey (Tap t)
    {
        switch (t)
        {
            case Tap::input:      return "input";
            case Tap::preEq:      return "pre_eq";
            case Tap::preComp:    return "pre_comp";
            case Tap::preInserts: return "pre_inserts";
            case Tap::preFader:   return "pre_fader";
            case Tap::postFader:  return "post_fader";
        }
        return {};
    }

    //==============================================================================
    // Parameter IDs. Host parameters (in the processor) and mix settings (held
    // by MixSettings, saved in the mix state) never share an ID.

    inline juce::String num (int index) { return juce::String (index + 1); }

    // Host parameters
    inline juce::String channelFaderId (int ch) { return "ch_fader_" + num (ch); }
    inline juce::String channelMuteId (int ch)  { return "ch_mute_" + num (ch); }
    inline juce::String busFaderId (int bus)    { return "bus_fader_" + num (bus); }
    inline juce::String busMuteId (int bus)     { return "bus_mute_" + num (bus); }
    inline juce::String assignId (int slot)     { return "assign" + num (slot); }

    // Mix settings
    inline juce::String channelId (int ch, const char* name) { return "ch" + num (ch) + "_" + name; }
    inline juce::String busId (int bus, const char* name)    { return "bus" + num (bus) + "_" + name; }
    inline juce::String linkId (int pair)                    { return "link" + num (pair); }

    // A channel's send level, tap and group assign for bus b; a bus's send and
    // group assign to bus c.
    inline juce::String channelSendId (int ch, int bus)   { return channelId (ch, ("send" + num (bus)).toRawUTF8()); }
    inline juce::String channelTapId (int ch, int bus)    { return channelId (ch, ("tap" + num (bus)).toRawUTF8()); }
    inline juce::String channelAssignId (int ch, int bus) { return channelId (ch, ("grp" + num (bus)).toRawUTF8()); }
    inline juce::String busSendId (int bus, int to)       { return busId (bus, ("send" + num (to)).toRawUTF8()); }

    // The EQ, compressor and limiter of a bus, the main or the mono sum: "bus3_eq_on", "main_eq_on", "mono_eq_on".
    constexpr int kProcessingChains = kBuses + 2;                  // the buses, the main, the mono sum
    constexpr int kMainChain = kBuses, kMonoChain = kBuses + 1;
    inline juce::String chainOwner (int chain)
    {
        return chain == kMainChain ? juce::String ("main") : chain == kMonoChain ? juce::String ("mono") : "bus" + num (chain);
    }
    inline juce::String chainId (int chain, const char* name) { return chainOwner (chain) + "_" + name; }
    inline juce::String busAssignId (int bus, int to)     { return busId (bus, ("grp" + num (to)).toRawUTF8()); }

    /** The channel whose controls channel @p ch uses: the left of a linked pair. */
    template <typename Links>
    int leaderOf (const Links& links, int ch) noexcept
    {
        return ch % 2 == 1 && links[(size_t) (ch / 2)] ? ch - 1 : ch;
    }
}
