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
        auto* p = &settings;
        channelParams[(size_t) ch] = { &p->param (channelId (ch, "trim")), &p->param (channelId (ch, "pol")),
                                       &p->param (channelId (ch, "polr")), &p->param (channelId (ch, "delay_on")),
                                       &p->param (channelId (ch, "delay")), &p->param (channelId (ch, "pan")),
                                       &p->param (channelId (ch, "main")) };
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
void RKMixProcessor::readSettings (std::uint64_t parts)
{
    for (int ch = 0; ch < kChannels; ++ch)
    {
        if ((parts & (std::uint64_t (1) << ch)) == 0)
            continue;

        const auto& p = channelParams[(size_t) ch];
        auto& c = engineSettings.channels[(size_t) ch];
        c.trimDb = valueOf (p.trim);
        c.polarity = { valueOf (p.pol) >= 0.5f, valueOf (p.polr) >= 0.5f };
        c.delayOn = valueOf (p.delayOn) >= 0.5f;
        c.delayMs = valueOf (p.delay);
        c.pan = valueOf (p.pan) * 0.01f;
        c.toMain = valueOf (p.main) >= 0.5f;
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

    for (int ch = 0; ch < kChannels; ++ch)
        raise (channelPeaks[(size_t) ch], engine.takeChannelPeak (ch));
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
