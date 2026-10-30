#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKMix";
        c.stateVersion = 1;
        c.channels = mix::kChannels;          // one 32-channel bus each way
        c.hasMix = false;
        c.hasOutput = false;                  // the main and mono faders are the output
        c.midiInput = true;                   // inserts with MIDI input, scenes by program change
        c.extraInPresets = true;              // a preset is the whole console
        c.bypassMutes = true;                 // raw inputs must never reach the PA or the in-ears
        c.accent = juce::Colour (0xff6b8ca6); // Utility family, steel
        c.family = rkfx::Palette::Family::utility;
        return c;
    }

    /** The settings' ID suffixes, in RKMixProcessor::ChannelField order. */
    const char* const kChannelFields[] = {
        "trim", "pol", "polr", "delay_on", "delay", "pan", "main",
        "hpf_on", "hpf", "hpf_slope",
        "gate_on", "gate_thr", "gate_range", "gate_att", "gate_hold", "gate_rel", "gate_key",
        "eq_on", "eq_low_type", "eq_high_type",
        "eq_low_freq", "eq_low_gain", "eq_low_q", "eq_lmid_freq", "eq_lmid_gain", "eq_lmid_q",
        "eq_hmid_freq", "eq_hmid_gain", "eq_hmid_q", "eq_high_freq", "eq_high_gain", "eq_high_q",
        "comp_on", "comp_thr", "comp_ratio", "comp_knee", "comp_att", "comp_rel", "comp_makeup", "comp_key",
    };

    void raise (std::atomic<float>& peak, float value) noexcept
    {
        auto current = peak.load();
        while (value > current && ! peak.compare_exchange_weak (current, value)) {}
    }
}

RKMixProcessor::RKMixProcessor()
    : rkfx::ProcessorBase (makeConfig(), MixParams::createHost())
{
    using namespace mix;
    for (int ch = 0; ch < kChannels; ++ch)
    {
        static_assert (std::size (kChannelFields) == numChannelFields);
        for (int f = 0; f < numChannelFields; ++f)
            channelParams[(size_t) ch][(size_t) f] = &settings.param (channelId (ch, kChannelFields[f]));
        channelFaders[(size_t) ch] = findParam (channelFaderId (ch));
        channelMutes[(size_t) ch] = findParam (channelMuteId (ch));
    }
    for (int pair = 0; pair < kPairs; ++pair)
        linkParams[(size_t) pair] = &settings.param (linkId (pair));
    mainBalance = &settings.param ("main_bal");
    mainOutput = &settings.param ("main_out");
    monoOutput = &settings.param ("mono_out");

    mainFader = findParam ("main_fader");
    mainMute = findParam ("main_mute");
    monoFader = findParam ("mono_fader");
    monoMute = findParam ("mono_mute");

    settings.onChange = [this] (std::uint64_t parts)
    {
        changed.fetch_or (parts);
        if (! loadingMix.load())
            markPresetModified();
    };
}

RKMixProcessor::~RKMixProcessor()
{
    settings.onChange = nullptr;
}

//==============================================================================
juce::String RKMixProcessor::getChannelName (int channel) const
{
    const juce::ScopedLock sl (namesLock);
    return channelNames[(size_t) channel];
}

void RKMixProcessor::setChannelName (int channel, const juce::String& name)
{
    {
        const juce::ScopedLock sl (namesLock);
        channelNames[(size_t) channel] = name.trim();
    }
    ++namesVersion;
    markPresetModified();
}

void RKMixProcessor::setChannelColour (int channel, int colour)
{
    channelColours[(size_t) channel] = juce::jlimit (0, kNumColours - 1, colour);
    ++namesVersion;
    markPresetModified();
}

juce::var RKMixProcessor::getExtraState() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("settings", settings.toVar());

    juce::Array<juce::var> names, colours;
    {
        const juce::ScopedLock sl (namesLock);
        for (int ch = 0; ch < kChannels; ++ch)
        {
            names.add (channelNames[(size_t) ch]);
            colours.add (channelColours[(size_t) ch].load());
        }
    }
    root->setProperty ("names", names);
    root->setProperty ("colours", colours);
    return juce::var (root);
}

void RKMixProcessor::setExtraState (const juce::var& extra)
{
    // A missing state (Reset, an empty preset) is the default console.
    loadingMix = true;
    settings.fromVar (extra["settings"]);
    loadingMix = false;

    {
        const juce::ScopedLock sl (namesLock);
        const auto* names = extra["names"].getArray();
        for (int ch = 0; ch < kChannels; ++ch)
            channelNames[(size_t) ch] = names != nullptr && ch < names->size() ? names->getReference (ch).toString() : juce::String();
    }
    const auto* colours = extra["colours"].getArray();
    for (int ch = 0; ch < kChannels; ++ch)
        channelColours[(size_t) ch] = juce::jlimit (0, kNumColours - 1, colours != nullptr && ch < colours->size() ? (int) colours->getReference (ch) : 0);
    ++namesVersion;
}

//==============================================================================
// Channel presets: a channel's fixed strip with its name and colour, not its
// routing (pan, Main, keys, sends), in the "RKMix Channels" preset folder.

bool RKMixProcessor::isStripField (int field)
{
    return field != pan && field != toMain && field != gateKey && field != compKey;
}

std::vector<rkfx::ParamInfo> RKMixProcessor::stripInfos() const
{
    std::vector<rkfx::ParamInfo> infos;
    for (int f = 0; f < numChannelFields; ++f)
        if (isStripField (f))
        {
            auto info = rkfx::describe (*channelParams[0][(size_t) f]);
            info.id = kChannelFields[f];
            infos.push_back (info);
        }
    return infos;
}

juce::File RKMixProcessor::getChannelPresetFolder()
{
    return getLibrary().getPresetFolder ("RKMix Channels");
}

RKMixProcessor::PresetResult RKMixProcessor::saveChannelPreset (int ch, const juce::File& file)
{
    rkfx::ParamValues values;
    for (int f = 0; f < numChannelFields; ++f)
        if (isStripField (f))
            values[kChannelFields[f]] = valueOf (channelParams[(size_t) ch][(size_t) f]);

    auto preset = rkfx::PresetFormat::toVar ("RKMix Channel", 1, file.getFileNameWithoutExtension(), stripInfos(), values, true);
    preset.getDynamicObject()->setProperty ("channelName", getChannelName (ch));
    preset.getDynamicObject()->setProperty ("colour", getChannelColour (ch));

    if (! file.getParentDirectory().createDirectory()
        || ! rkfx::PresetLibrary::writeAtomically (file, rkfx::PresetFormat::toJson (preset)))
        return { false, "Could not save " + file.getFileName() };
    return { true, {} };
}

RKMixProcessor::PresetResult RKMixProcessor::loadChannelPreset (int ch, const juce::File& file)
{
    const auto json = file.loadFileAsString();
    const auto r = rkfx::PresetFormat::read (json, "RKMix Channel", 1, stripInfos());
    if (! r.ok)
        return { false, r.error };

    for (int f = 0; f < numChannelFields; ++f)
        if (isStripField (f))
            if (const auto it = r.values.find (kChannelFields[f]); it != r.values.end())
            {
                auto* p = channelParams[(size_t) ch][(size_t) f];
                p->setValueNotifyingHost (p->convertTo0to1 ((float) it->second));
            }

    const auto root = juce::JSON::parse (json);
    if (root.hasProperty ("channelName"))
        setChannelName (ch, root["channelName"].toString());
    if (root.hasProperty ("colour"))
        setChannelColour (ch, (int) root["colour"]);
    return { true, r.notes.joinIntoString ("; ") };
}

void RKMixProcessor::resetChannelStrip (int ch)
{
    for (int f = 0; f < numChannelFields; ++f)
        if (isStripField (f))
        {
            auto* p = channelParams[(size_t) ch][(size_t) f];
            p->setValueNotifyingHost (p->getDefaultValue());
        }
}

//==============================================================================
void RKMixProcessor::readSettings (std::uint64_t parts)
{
    for (int ch = 0; ch < kChannels; ++ch)
    {
        if ((parts & (std::uint64_t (1) << ch)) == 0)
            continue;

        engineSettings.channels[(size_t) ch] = channelSettings (ch);
    }

    if ((parts & MixSettings::kMainBit) != 0)
    {
        for (int pair = 0; pair < mix::kPairs; ++pair)
            engineSettings.links[(size_t) pair] = valueOf (linkParams[(size_t) pair]) >= 0.5f;
        engineSettings.main.balance = valueOf (mainBalance) * 0.01f;
        engineSettings.main.output = juce::roundToInt (valueOf (mainOutput));
        engineSettings.main.monoOutput = juce::roundToInt (valueOf (monoOutput));
    }
}

MixEngine::Channel RKMixProcessor::channelSettings (int ch) const
{
    const auto& p = channelParams[(size_t) ch];
    const auto get = [&p] (int field) { return valueOf (p[(size_t) field]); };
    const auto on = [&p] (int field) { return valueOf (p[(size_t) field]) >= 0.5f; };
    const auto index = [&p] (int field) { return juce::roundToInt (valueOf (p[(size_t) field])); };

    MixEngine::Channel c;
    auto& strip = c.strip;
    strip.trimDb = get (trim);
    strip.polarity = { on (pol), on (polr) };
    strip.delayOn = on (delayOn);
    strip.delayMs = get (delay);

    strip.hpf = { on (hpfOn), get (hpf), index (hpfSlope) == 1 };
    strip.gate = { on (gateOn), get (gateThr), get (gateRange), get (gateAtt), get (gateHold), get (gateRel), index (gateKey) };

    strip.eq.on = on (eqOn);
    for (int b = 0; b < StripDsp::kEqBands; ++b)
    {
        auto& band = strip.eq.bands[(size_t) b];
        band.bell = b == 0 ? index (eqLowType) == 1 : b == StripDsp::kEqBands - 1 ? index (eqHighType) == 1 : true;
        band.hz = get (eqBands + 3 * b);
        band.gainDb = get (eqBands + 3 * b + 1);
        band.q = get (eqBands + 3 * b + 2);
    }

    strip.comp = { on (compOn), get (compThr), get (compRatio), get (compKnee), get (compAtt), get (compRel),
                   get (compMakeup), index (compKey) };

    c.pan = get (pan) * 0.01f;
    c.toMain = on (toMain);
    return c;
}

MixEngine::Levels RKMixProcessor::readLevels() const
{
    MixEngine::Levels levels;
    for (int ch = 0; ch < kChannels; ++ch)
    {
        levels.channelDb[(size_t) ch] = valueOf (channelFaders[(size_t) ch]);
        levels.channelMute[(size_t) ch] = valueOf (channelMutes[(size_t) ch]) >= 0.5f;
    }
    levels.mainDb = valueOf (mainFader);
    levels.mainMute = valueOf (mainMute) >= 0.5f;
    levels.monoDb = valueOf (monoFader);
    levels.monoMute = valueOf (monoMute) >= 0.5f;
    return levels;
}

void RKMixProcessor::prepareDsp (double sampleRate, int maxBlockSize, int)
{
    changed = 0;
    readSettings (MixEngine::kEverything);
    engine.prepare (sampleRate, maxBlockSize);
    engine.setLevels (readLevels());
    engine.setSettings (engineSettings);
    engine.reset();
}

void RKMixProcessor::resetDsp()
{
    changed = 0;
    readSettings (MixEngine::kEverything);
    engine.setLevels (readLevels());
    engine.setSettings (engineSettings);
    engine.reset();
}

void RKMixProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < kChannels)
    {
        buffer.clear();
        return;
    }

    if (const auto parts = changed.exchange (0); parts != 0)
    {
        readSettings (parts);
        engine.setSettings (engineSettings, parts);
    }
    engine.setLevels (readLevels());

    const auto measure = windowsOpen.load() > 0;
    engine.process (getDryInput().getArrayOfReadPointers(), buffer.getArrayOfWritePointers(), buffer.getNumSamples(), measure);
    if (! measure)
        return;

    const auto lower = [] (std::atomic<float>& reduction, float value)
    {
        auto current = reduction.load();
        while (value < current && ! reduction.compare_exchange_weak (current, value)) {}
    };
    for (int ch = 0; ch < kChannels; ++ch)
    {
        raise (channelPeaks[(size_t) ch], engine.takeChannelPeak (ch));
        lower (gateReduction[(size_t) ch], engine.takeGateReductionDb (ch));
        lower (compReduction[(size_t) ch], engine.takeCompReductionDb (ch));
    }
    raise (mainPeaks[0], engine.takeMainPeak (0));
    raise (mainPeaks[1], engine.takeMainPeak (1));
    raise (monoPeak, engine.takeMonoPeak());
}

juce::AudioProcessorEditor* RKMixProcessor::createEditor()
{
    return new RKMixEditor (*this);
}

// The plugin's entry point (left out where another one is built in: RKBoardTests).
#ifndef RKMIX_NO_ENTRY_POINT
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKMixProcessor();
}
#endif
