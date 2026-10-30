#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "host/PedalRegistry.h"

namespace rkfx { std::vector<FactoryPreset> factoryPresetList_RKMix(); }

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        // Before the base reads them: every insert's factory presets, and the console's.
        rkfx::pedals::registerFactoryPresets();
        rkfx::registerFactoryPresets ("RKMix", &rkfx::factoryPresetList_RKMix);

        rkfx::PluginConfig c;
        c.name = "RKMix";
        c.stateVersion = 1;
        c.channels = mix::kChannels;          // one 32-channel bus each way
        c.hasMix = false;
        c.hasOutput = false;                  // the main and mono faders are the output
        c.midiInput = true;                   // inserts with MIDI input, scenes by program change
        c.midiOutput = true;                  // what RKTrigger and RKFollow inserts send, and MIDI thru
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
        "direct_out", "direct_tap",
    };

    void raise (std::atomic<float>& peak, float value) noexcept
    {
        auto current = peak.load();
        while (value > current && ! peak.compare_exchange_weak (current, value)) {}
    }
}

//==============================================================================
juce::String MixLearn::key (int target)
{
    if (target >= firstAssign && target < firstAssign + 32) return "assign" + juce::String (target - firstAssign + 1);
    if (target == scene) return "scene";
    if (target == tap)   return "tap";
    if (target == talk)  return "talk";
    return {};
}

int MixLearn::fromKey (const juce::String& k)
{
    if (k.startsWith ("assign"))
        if (const auto n = k.substring (6).getIntValue(); n >= 1 && n <= 32)
            return firstAssign + n - 1;
    if (k == "scene") return scene;
    if (k == "tap")   return tap;
    if (k == "talk")  return talk;
    return -1;
}

juce::String MixLearn::describe (int target)
{
    if (target >= firstAssign && target < firstAssign + 32) return "Assign " + juce::String (target - firstAssign + 1);
    if (target == scene) return "Scene";
    if (target == tap)   return "Tap Tempo";
    if (target == talk)  return "Talk";
    return {};
}

bool MixLearn::isSwitch (int target)
{
    return target == tap || target == talk;
}

//==============================================================================
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
        for (int bus = 0; bus < kBuses; ++bus)
        {
            channelRouting[(size_t) ch].sends[(size_t) bus] = &settings.param (channelSendId (ch, bus));
            channelRouting[(size_t) ch].taps[(size_t) bus] = &settings.param (channelTapId (ch, bus));
            channelRouting[(size_t) ch].assigns[(size_t) bus] = &settings.param (channelAssignId (ch, bus));
        }
    }
    for (int bus = 0; bus < kBuses; ++bus)
    {
        busFaders[(size_t) bus] = findParam (busFaderId (bus));
        busMutes[(size_t) bus] = findParam (busMuteId (bus));
        auto& b = busParams[(size_t) bus];
        b.type = &settings.param (busId (bus, "type"));
        b.format = &settings.param (busId (bus, "format"));
        b.output = &settings.param (busId (bus, "out"));
        b.toMain = &settings.param (busId (bus, "main"));
        b.tap = &settings.param (busId (bus, "tap"));
        b.panMode = &settings.param (busId (bus, "panmode"));
        b.pan = &settings.param (busId (bus, "pan"));
        for (int to = 0; to < kBuses; ++to)
        {
            b.routing.sends[(size_t) to] = &settings.param (busSendId (bus, to));
            b.routing.assigns[(size_t) to] = &settings.param (busAssignId (bus, to));
        }
    }
    for (int chain = 0; chain < kProcessingChains; ++chain)
        for (const auto* field : MixParams::chainFields())
            chainParams[(size_t) chain].push_back (&settings.param (chainId (chain, field)));
    for (int pair = 0; pair < kPairs; ++pair)
        linkParams[(size_t) pair] = &settings.param (linkId (pair));
    mainBalance = &settings.param ("main_bal");
    mainOutput = &settings.param ("main_out");
    monoOutput = &settings.param ("mono_out");
    midiThru = &settings.param ("midi_thru");
    talkParam = findParam ("talk");
    oscOn = &settings.param ("osc_on");
    for (int ch = 0; ch < kChannels; ++ch)
        soloSafeParams[(size_t) ch] = &settings.param (channelId (ch, "solosafe"));
    for (int bus = 0; bus < kBuses; ++bus)
        busSoloSafeParams[(size_t) bus] = &settings.param (busId (bus, "solosafe"));

    mainFader = findParam ("main_fader");
    mainMute = findParam ("main_mute");
    monoFader = findParam ("mono_fader");
    monoMute = findParam ("mono_mute");

    settings.onChange = [this] (std::uint64_t parts)
    {
        changed.fetch_or (parts);
        if (! loadingMix.load())
            markPresetModified();

        // A bus switched between mono and stereo keeps its place: Out 5/6 <-> Out 5 (a
        // channel's direct output too, as its pair links or unlinks).
        if (! loadingMix.load() && (parts & (std::uint64_t (0xffff) << 32)) != 0)
            matchBusOutputs();
        if (! loadingMix.load() && (parts & MixSettings::kMainBit) != 0)
            matchDirectOutputs();

        // A pair linked or unlinked, a bus mono or stereo: its inserts follow.
        if ((parts & (MixSettings::kMainBit | (std::uint64_t (0xffff) << 32))) != 0)
        {
            if (juce::MessageManager::existsAndIsCurrentThread())
                syncInsertLayouts();
            else
                layoutSync.triggerAsyncUpdate();
        }
    };

    engine.setInserts (&rack);
    rack.onChange = [this] { if (! loadingMix.load()) markPresetModified(); };
    rack.onLatencyChange = [this] { updateLatencies(); };

    scenes = std::make_unique<MixScenes> (settings, *this, rack);
    assigns = std::make_unique<MixAssigns> (settings, *this, rack);
    assigns->describeTarget = [this] (const MixAssigns::Target& t) { return describeTarget (t); };
    sceneParam = findParam ("scene");
    sceneParam->addListener (&sceneSync);

    // The standalone app: MIDI learn and its rig settings in Application Support/RKFX/RKMix.
    standalone = wrapperType == wrapperType_Standalone;
    if (standalone)
    {
        rigFolder = rkfx::PresetLibrary::defaultSettingsFile().getParentDirectory().getChildFile ("RKMix");
        loadRig();
    }

    // An insert leaving: its assignments and scene knobs go, and the editor closes it first.
    rack.aboutToRemove = [this] (rkfx::ProcessorBase* plugin)
    {
        if (insertAboutToRemove != nullptr)
            insertAboutToRemove (plugin);
        if (const auto slot = rack.slotOf (plugin); slot >= 0)
        {
            assigns->insertRemoved (slot);
            scenes->insertRemoved (slot);
        }
    };
}

RKMixProcessor::~RKMixProcessor()
{
    sceneParam->removeListener (&sceneSync);
    assigns.reset();
    rack.aboutToRemove = nullptr;
    settings.onChange = nullptr;
    rack.onChange = nullptr;
    rack.onLatencyChange = nullptr;
    rack.clear();
}

void RKMixProcessor::matchBusOutputs()
{
    for (int bus = 0; bus < mix::kBuses; ++bus)
    {
        auto& out = *busParams[(size_t) bus].output;
        const auto target = juce::roundToInt (valueOf (&out));
        const auto first = mix::firstOutputOf (target);
        if (first < 0 || mix::isPairTarget (target) == isBusStereo (bus))
            continue;
        const auto matched = isBusStereo (bus) ? mix::pairTarget (first) : first + 1;
        out.setValueNotifyingHost (out.convertTo0to1 ((float) matched));
    }
}

void RKMixProcessor::matchDirectOutputs()
{
    for (int ch = 0; ch < mix::kChannels; ++ch)
    {
        if (ch % 2 == 1 && isLinked (ch / 2))
            continue;   // a linked pair's right channel: the left one's settings
        auto& out = *channelParams[(size_t) ch][directOut];
        const auto target = juce::roundToInt (valueOf (&out));
        const auto first = mix::firstOutputOf (target);
        const auto stereo = ch % 2 == 0 && isLinked (ch / 2);
        if (first < 0 || mix::isPairTarget (target) == stereo)
            continue;
        const auto matched = stereo ? mix::pairTarget (first) : first + 1;
        out.setValueNotifyingHost (out.convertTo0to1 ((float) matched));
    }
}

//==============================================================================
void RKMixProcessor::setStandaloneForTesting (const juce::File& folder)
{
    standalone = true;
    rigFolder = folder;
    loadRig();
}

void RKMixProcessor::loadRig()
{
    midiMap = std::make_unique<rkfx::MidiMap> (rigFolder.getChildFile ("midi-map.json"),
                                               rkfx::MidiMap::Targets { MixLearn::key, MixLearn::fromKey, MixLearn::isSwitch });
    juce::var root;
    if (juce::JSON::parse (rigFolder.getChildFile ("settings.json").loadFileAsString(), root).wasOk())
        followClock = (bool) root.getProperty ("followClock", false);
    standalonePlayHead.bpm = tempo.load();
}

void RKMixProcessor::saveRig() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("followClock", followClock.load());
    rigFolder.createDirectory();
    rkfx::PresetLibrary::writeAtomically (rigFolder.getChildFile ("settings.json"), juce::JSON::toString (juce::var (root)));
}

void RKMixProcessor::setTempo (double bpm)
{
    tempo = juce::jlimit (20.0, 300.0, bpm);
    standalonePlayHead.bpm = tempo.load();
    markPresetModified();
}

void RKMixProcessor::tapTempo()
{
    if (const auto bpm = tapper.tap (juce::Time::getMillisecondCounterHiRes() * 0.001))
        setTempo (*bpm);
}

void RKMixProcessor::setFollowClock (bool follow)
{
    followClock = follow;
    clock.reset();
    saveRig();
}

void RKMixProcessor::LearnNotifier::handleAsyncUpdate()
{
    if (owner.midiMap != nullptr && owner.learnedTarget >= 0)
        owner.midiMap->set ({ owner.learnedTarget, owner.learnedChannel, owner.learnedCC, false });
    owner.learnedTarget = -1;
    ++owner.learnVersion;
}

void RKMixProcessor::handleStandaloneMidi (int numSamples)
{
    const auto* midi = getMidi();
    const auto rate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    if (midi != nullptr)
        for (const auto metadata : *midi)
        {
            if (metadata.samplePosition < getChunkStart() || metadata.samplePosition >= getChunkStart() + numSamples)
                continue;
            const auto message = metadata.getMessage();
            const auto time = sampleTime + (metadata.samplePosition - getChunkStart()) / rate;

            if (message.isMidiClock())
            {
                clock.tick (time);
            }
            else if (message.isProgramChange())
            {
                // Program changes 0-15 recall scenes 1-16.
                if (message.getProgramChangeNumber() < MixScenes::kNumScenes)
                    sceneParam->setValueNotifyingHost (sceneParam->convertTo0to1 ((float) message.getProgramChangeNumber() + 1.0f));
            }
            else if (message.isController())
            {
                const auto channel = message.getChannel(), cc = message.getControllerNumber();

                // Learning: this controller is the one.
                if (const auto target = learning.exchange (-1); target >= 0)
                {
                    learnedChannel = channel;
                    learnedCC = cc;
                    learnedTarget = target;
                    learnNotifier.triggerAsyncUpdate();
                    continue;
                }
                if (midiMap != nullptr)
                    if (const auto code = midiMap->lookup (channel, cc); code >= 0)
                        applyController (code % 1000, code >= 1000, message.getControllerValue(), time);
            }
        }

    sampleTime += numSamples / rate;
    if (followClock.load())
        if (const auto bpm = clock.bpm (sampleTime))
        {
            tempo = *bpm;
            standalonePlayHead.bpm = *bpm;
        }
}

void RKMixProcessor::applyController (int target, bool momentary, int value, double time)
{
    const auto pressed = value >= 64;
    const auto wasPressed = target < (int) switchHeld.size() && switchHeld[(size_t) target];
    if (target < (int) switchHeld.size())
        switchHeld[(size_t) target] = pressed;
    const auto pressEdge = pressed && ! wasPressed;

    if (target >= MixLearn::firstAssign && target < MixLearn::firstAssign + MixAssigns::kNumAssigns)
    {
        // An expression pedal: glide to it, so the 7-bit steps never zipper.
        const auto slot = target - MixLearn::firstAssign;
        ccTargets[(size_t) slot] = (float) value / 127.0f;
        ccGliding[(size_t) slot] = true;
    }
    else if (target == MixLearn::scene)
    {
        sceneParam->setValueNotifyingHost (sceneParam->convertTo0to1 ((float) juce::jlimit (1, MixScenes::kNumScenes, value + 1)));
    }
    else if (target == MixLearn::tap)
    {
        if (pressEdge && ! followClock.load())
            if (const auto bpm = tapper.tap (time))
            {
                tempo = *bpm;
                standalonePlayHead.bpm = *bpm;
            }
    }
    else if (target == MixLearn::talk)
    {
        // Momentary: on while held; toggle: each press.
        const auto on = momentary ? pressed : (pressEdge ? valueOf (talkParam) < 0.5f : valueOf (talkParam) >= 0.5f);
        if (on != (valueOf (talkParam) >= 0.5f))
            talkParam->setValueNotifyingHost (on ? 1.0f : 0.0f);
    }
}

void RKMixProcessor::glideControllers (int numSamples)
{
    const auto rate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const auto amount = juce::jmin (1.0f, (float) numSamples / (float) (0.03 * rate));   // ~30 ms
    for (int slot = 0; slot < MixAssigns::kNumAssigns; ++slot)
    {
        if (! ccGliding[(size_t) slot])
            continue;
        auto& param = *findParam (mix::assignId (slot));
        const auto target = ccTargets[(size_t) slot].load();
        const auto now = param.getValue();
        const auto next = std::abs (target - now) < 1.0e-4f ? target : now + amount * (target - now);
        param.setValueNotifyingHost (next);
        if (juce::exactlyEqual (next, target))
            ccGliding[(size_t) slot] = false;
    }
}

void RKMixProcessor::recallScene (int scene)
{
    scenes->recall (scene);
    const auto value = sceneParam->convertTo0to1 ((float) (scenes->getCurrent() + 1));
    if (! juce::exactlyEqual (sceneParam->getValue(), value))
        sceneParam->setValueNotifyingHost (value);
}

void RKMixProcessor::SceneSync::parameterValueChanged (int, float)
{
    owner.pendingScene = juce::roundToInt (valueOf (owner.sceneParam)) - 1;
    if (juce::MessageManager::existsAndIsCurrentThread())
        handleAsyncUpdate();
    else
        triggerAsyncUpdate();
}

void RKMixProcessor::SceneSync::handleAsyncUpdate()
{
    if (const auto scene = owner.pendingScene.exchange (-1); scene >= 0 && scene != owner.scenes->getCurrent())
        owner.scenes->recall (scene);
}

juce::String RKMixProcessor::describeTarget (const MixAssigns::Target& t) const
{
    if (t.kind == MixAssigns::Target::Kind::insert)
    {
        const auto slot = InsertRack::slotAt (t.slot);
        auto* plugin = rack.get (slot);
        const auto* param = plugin != nullptr ? plugin->findParam (t.paramId) : nullptr;
        juce::String strip = slot.strip == InsertRack::Strip::channel ? "Ch " + mix::num (slot.index)
                           : slot.strip == InsertRack::Strip::bus ? "Bus " + mix::num (slot.index)
                           : slot.strip == InsertRack::Strip::main ? juce::String ("Main") : juce::String ("Mono");
        return strip + ", insert " + juce::String (slot.slot + 1) + ": " + (param != nullptr ? param->getName (40) : t.paramId);
    }

    // A mix setting or host parameter: its own name ("Ch 3 Pan"), with the channel's name when it has one.
    juce::RangedAudioParameter* param = settings.findParam (t.paramId);
    if (param == nullptr)
        param = findParam (t.paramId);
    auto text = param != nullptr ? param->getName (60) : t.paramId;
    if (t.paramId.startsWith ("ch"))
    {
        const auto ch = t.paramId.startsWith ("ch_") ? t.paramId.fromLastOccurrenceOf ("_", false, false).getIntValue() - 1
                                                     : t.paramId.substring (2).getIntValue() - 1;
        if (juce::isPositiveAndBelow (ch, kChannels) && getChannelName (ch).isNotEmpty())
            text = getChannelName (ch) + ": " + text.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false);
    }
    return text;
}

void RKMixProcessor::setChannelSolo (int ch, bool on)
{
    channelSolo[(size_t) (ch % 2 == 1 && isLinked (ch / 2) ? ch - 1 : ch)] = on;
    ++soloVersion;
}

void RKMixProcessor::setBusSolo (int bus, bool on)
{
    busSolo[(size_t) bus] = on;
    ++soloVersion;
}

void RKMixProcessor::clearSolo()
{
    for (auto& s : channelSolo)
        s = false;
    for (auto& s : busSolo)
        s = false;
    ++soloVersion;
}

bool RKMixProcessor::isAnythingSoloed() const noexcept
{
    for (const auto& s : channelSolo)
        if (s.load())
            return true;
    for (const auto& s : busSolo)
        if (s.load())
            return true;
    return false;
}

int RKMixProcessor::channelsOf (InsertRack::Strip strip, int index) const
{
    switch (strip)
    {
        case InsertRack::Strip::channel: return index % 2 == 0 && isLinked (index / 2) ? 2 : 1;
        case InsertRack::Strip::bus:     return isBusStereo (index) ? 2 : 1;
        case InsertRack::Strip::main:    return 2;
        case InsertRack::Strip::mono:    return 1;
    }
    return 2;
}

void RKMixProcessor::syncInsertLayouts()
{
    for (int ch = 0; ch < kChannels; ++ch)
        rack.setChannels (InsertRack::Strip::channel, ch, channelsOf (InsertRack::Strip::channel, ch));
    for (int bus = 0; bus < mix::kBuses; ++bus)
        rack.setChannels (InsertRack::Strip::bus, bus, channelsOf (InsertRack::Strip::bus, bus));
    updateLatencies();
}

void RKMixProcessor::updateLatencies()
{
    const auto l = rack.latencies();
    {
        const juce::SpinLock::ScopedLockType sl (latencyLock);
        pendingLatencies = l;
    }
    latenciesChanged = true;
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

juce::String RKMixProcessor::getBusName (int bus) const
{
    const juce::ScopedLock sl (namesLock);
    return busNames[(size_t) bus];
}

void RKMixProcessor::setBusName (int bus, const juce::String& name)
{
    {
        const juce::ScopedLock sl (namesLock);
        busNames[(size_t) bus] = name.trim();
    }
    ++namesVersion;
    markPresetModified();
}

void RKMixProcessor::setBusColour (int bus, int colour)
{
    busColours[(size_t) bus] = juce::jlimit (0, kNumColours - 1, colour);
    ++namesVersion;
    markPresetModified();
}

juce::String RKMixProcessor::getOutputName (int output) const
{
    const juce::ScopedLock sl (namesLock);
    return outputNames[(size_t) output];
}

void RKMixProcessor::setOutputName (int output, const juce::String& name)
{
    {
        const juce::ScopedLock sl (namesLock);
        outputNames[(size_t) output] = name.trim();
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

    juce::Array<juce::var> names, colours, buses, busColourList, outputs;
    {
        const juce::ScopedLock sl (namesLock);
        for (int ch = 0; ch < kChannels; ++ch)
        {
            names.add (channelNames[(size_t) ch]);
            colours.add (channelColours[(size_t) ch].load());
            outputs.add (outputNames[(size_t) ch]);
        }
        for (int bus = 0; bus < mix::kBuses; ++bus)
        {
            buses.add (busNames[(size_t) bus]);
            busColourList.add (busColours[(size_t) bus].load());
        }
    }
    root->setProperty ("tempo", tempo.load());
    root->setProperty ("inserts", rack.toVar());
    root->setProperty ("scenes", scenes->toVar());
    root->setProperty ("assigns", assigns->toVar());
    root->setProperty ("names", names);
    root->setProperty ("colours", colours);
    root->setProperty ("busNames", buses);
    root->setProperty ("busColours", busColourList);
    root->setProperty ("outputNames", outputs);
    return juce::var (root);
}

void RKMixProcessor::setExtraState (const juce::var& extra)
{
    // A missing state (Reset, an empty preset) is the default console.
    tempo = juce::jlimit (20.0, 300.0, (double) extra.getProperty ("tempo", 120.0));
    standalonePlayHead.bpm = tempo.load();

    // Solo is never saved; talkback and the oscillator are never on when a mix loads.
    clearSolo();
    talkParam->setValueNotifyingHost (0.0f);

    loadingMix = true;
    settings.fromVar (extra["settings"]);
    oscOn->setValueNotifyingHost (0.0f);
    rack.fromVar (extra["inserts"], [this] (InsertRack::Strip strip, int index) { return channelsOf (strip, index); });
    assigns->fromVar (extra["assigns"]);
    scenes->fromVar (extra["scenes"]);
    sceneParam->setValueNotifyingHost (sceneParam->convertTo0to1 ((float) (scenes->getCurrent() + 1)));
    loadingMix = false;

    const auto text = [] (const juce::var& list, int index)
    {
        const auto* a = list.getArray();
        return a != nullptr && index < a->size() ? a->getReference (index).toString() : juce::String();
    };
    const auto colour = [] (const juce::var& list, int index)
    {
        const auto* a = list.getArray();
        return juce::jlimit (0, kNumColours - 1, a != nullptr && index < a->size() ? (int) a->getReference (index) : 0);
    };
    {
        const juce::ScopedLock sl (namesLock);
        for (int ch = 0; ch < kChannels; ++ch)
        {
            channelNames[(size_t) ch] = text (extra["names"], ch);
            outputNames[(size_t) ch] = text (extra["outputNames"], ch);
        }
        for (int bus = 0; bus < mix::kBuses; ++bus)
            busNames[(size_t) bus] = text (extra["busNames"], bus);
    }
    for (int ch = 0; ch < kChannels; ++ch)
        channelColours[(size_t) ch] = colour (extra["colours"], ch);
    for (int bus = 0; bus < mix::kBuses; ++bus)
        busColours[(size_t) bus] = colour (extra["busColours"], bus);
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
    preset.getDynamicObject()->setProperty ("inserts", rack.stripToVar (InsertRack::Strip::channel, ch));
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
    rack.stripFromVar (InsertRack::Strip::channel, ch, root["inserts"], channelsOf (InsertRack::Strip::channel, ch));
    if (root.hasProperty ("channelName"))
        setChannelName (ch, root["channelName"].toString());
    if (root.hasProperty ("colour"))
        setChannelColour (ch, (int) root["colour"]);
    return { true, r.notes.joinIntoString ("; ") };
}

void RKMixProcessor::resetChannelStrip (int ch)
{
    rack.stripFromVar (InsertRack::Strip::channel, ch, {}, channelsOf (InsertRack::Strip::channel, ch));
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

    for (int bus = 0; bus < mix::kBuses; ++bus)
    {
        if ((parts & MixSettings::busBit (bus)) == 0)
            continue;

        const auto& p = busParams[(size_t) bus];
        auto& b = engineSettings.buses[(size_t) bus];
        b.type = (mix::BusType) juce::roundToInt (valueOf (p.type));
        b.stereo = juce::roundToInt (valueOf (p.format)) == 1;
        b.output = juce::roundToInt (valueOf (p.output));
        b.toMain = valueOf (p.toMain) >= 0.5f;
        b.defaultTap = (mix::Tap) juce::roundToInt (valueOf (p.tap));
        b.panMode = (mix::PanMode) juce::roundToInt (valueOf (p.panMode));
        b.dsp = chainSettings (bus);
        const auto safe = juce::roundToInt (valueOf (busSoloSafeParams[(size_t) bus]));   // Auto: an FX bus
        b.soloSafe = safe == 1 || (safe == 0 && b.type == mix::BusType::fx);
        b.pan = valueOf (p.pan) * 0.01f;
        for (int to = 0; to < mix::kBuses; ++to)
        {
            b.sendDb[(size_t) to] = valueOf (p.routing.sends[(size_t) to]);
            b.assign[(size_t) to] = valueOf (p.routing.assigns[(size_t) to]) >= 0.5f;
        }
    }

    if ((parts & MixSettings::kMainBit) != 0)
    {
        for (int pair = 0; pair < mix::kPairs; ++pair)
            engineSettings.links[(size_t) pair] = valueOf (linkParams[(size_t) pair]) >= 0.5f;
        engineSettings.main.balance = valueOf (mainBalance) * 0.01f;
        engineSettings.main.output = juce::roundToInt (valueOf (mainOutput));
        engineSettings.main.monoOutput = juce::roundToInt (valueOf (monoOutput));
        engineSettings.main.dsp = chainSettings (mix::kMainChain);
        engineSettings.main.monoDsp = chainSettings (mix::kMonoChain);

        auto& m = engineSettings.monitor;
        m.soloInPlace = settings.choice ("solo_mode") == 1;
        m.afl = settings.choice ("solo_tap") == 1;
        m.cueOutput = settings.choice ("cue_out");
        m.cueSource = settings.choice ("cue_src");
        m.talkSource = settings.choice ("talk_src");
        m.talkDb = settings.value ("talk_level");
        m.talkMain = settings.flag ("talk_main");
        m.oscPink = settings.choice ("osc_type") == 1;
        m.oscHz = settings.value ("osc_freq");
        m.oscDb = settings.value ("osc_level");
        m.oscMain = settings.flag ("osc_main");
        m.oscOutput = settings.choice ("osc_out");
        for (int bus = 0; bus < mix::kBuses; ++bus)
        {
            m.talkTo[(size_t) bus] = settings.flag ("talk_to" + mix::num (bus));
            m.oscTo[(size_t) bus] = settings.flag ("osc_to" + mix::num (bus));
        }
    }
}

BusDsp::Settings RKMixProcessor::chainSettings (int chain) const
{
    // In MixParams::chainFields() order.
    enum { fEqOn, fHpfOn, fHpfHz, fHpfSlope, fLpfOn, fLpfHz, fLpfSlope, fLowType, fHighType, fBands,
           fCompOn = fBands + 18, fCompThr, fCompRatio, fCompKnee, fCompAtt, fCompRel, fCompAuto, fCompMakeup,
           fLimOn, fLimCeiling, fLimRel };

    const auto& p = chainParams[(size_t) chain];
    const auto get = [&p] (int field) { return valueOf (p[(size_t) field]); };
    const auto on = [&p] (int field) { return valueOf (p[(size_t) field]) >= 0.5f; };

    BusDsp::Settings s;
    s.eq.on = on (fEqOn);
    s.eq.hpfOn = on (fHpfOn);
    s.eq.hpfHz = get (fHpfHz);
    s.eq.hpfSteep = get (fHpfSlope) >= 0.5f;
    s.eq.lpfOn = on (fLpfOn);
    s.eq.lpfHz = get (fLpfHz);
    s.eq.lpfSteep = get (fLpfSlope) >= 0.5f;
    for (int b = 0; b < BusDsp::kEqBands; ++b)
    {
        auto& band = s.eq.bands[(size_t) b];
        band.bell = b == 0 ? get (fLowType) >= 0.5f : b == BusDsp::kEqBands - 1 ? get (fHighType) >= 0.5f : true;
        band.hz = get (fBands + 3 * b);
        band.gainDb = get (fBands + 3 * b + 1);
        band.q = get (fBands + 3 * b + 2);
    }
    s.comp = { on (fCompOn), get (fCompThr), get (fCompRatio), get (fCompKnee), get (fCompAtt), get (fCompRel), get (fCompMakeup), on (fCompAuto) };
    s.limit = { on (fLimOn), get (fLimCeiling), get (fLimRel) };
    return s;
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
    c.soloSafe = valueOf (soloSafeParams[(size_t) ch]) >= 0.5f;
    c.directOut = index (directOut);
    c.directTap = (mix::Tap) index (directTap);

    const auto& routing = channelRouting[(size_t) ch];
    for (int bus = 0; bus < mix::kBuses; ++bus)
    {
        c.sendDb[(size_t) bus] = valueOf (routing.sends[(size_t) bus]);
        c.tap[(size_t) bus] = juce::roundToInt (valueOf (routing.taps[(size_t) bus])) - 1;   // 0 = the bus's default
        c.assign[(size_t) bus] = valueOf (routing.assigns[(size_t) bus]) >= 0.5f;
    }
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
    for (int bus = 0; bus < mix::kBuses; ++bus)
    {
        levels.busDb[(size_t) bus] = valueOf (busFaders[(size_t) bus]);
        levels.busMute[(size_t) bus] = valueOf (busMutes[(size_t) bus]) >= 0.5f;
    }
    for (int ch = 0; ch < kChannels; ++ch)
        levels.channelSolo[(size_t) ch] = channelSolo[(size_t) ch].load();
    for (int bus = 0; bus < mix::kBuses; ++bus)
        levels.busSolo[(size_t) bus] = busSolo[(size_t) bus].load();
    levels.talk = valueOf (talkParam) >= 0.5f;
    levels.osc = valueOf (oscOn) >= 0.5f;
    levels.mainDb = valueOf (mainFader);
    levels.mainMute = valueOf (mainMute) >= 0.5f;
    levels.monoDb = valueOf (monoFader);
    levels.monoMute = valueOf (monoMute) >= 0.5f;
    return levels;
}

void RKMixProcessor::releaseMidi (juce::MidiBuffer& midi)
{
    // Bypass engaged: every note the inserts are sounding ends.
    const juce::SpinLock::ScopedLockType sl (rack.getLock());
    rack.releaseAllMidi (midi);
}

void RKMixProcessor::prepareDsp (double sampleRate, int maxBlockSize, int)
{
    insertMidi.ensureSize (8192);
    rack.prepare (sampleRate, maxBlockSize);
    updateLatencies();
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

    if (latenciesChanged.exchange (false))
    {
        const juce::SpinLock::ScopedTryLockType sl (latencyLock);
        if (sl.isLocked())
            engine.setLatencies (pendingLatencies);
        else
            latenciesChanged = true;   // next block
    }
    if (const auto latency = engine.getLatency(); latency != reportedLatency)
    {
        reportedLatency = latency;
        reportLatency (latency);
    }

    // The standalone app: MIDI learn, program changes, tempo; its own play head for the inserts.
    if (standalone)
    {
        handleStandaloneMidi (buffer.getNumSamples());
        glideControllers (buffer.getNumSamples());
        standalonePlayHead.advance (buffer.getNumSamples(), getSampleRate());
    }

    const auto measure = windowsOpen.load() > 0;
    {
        // The inserts stay put while they run (a removed one is deleted after this).
        const juce::SpinLock::ScopedLockType sl (rack.getLock());
        rack.setBlockContext (standalone ? &standalonePlayHead : getPlayHead(), getMidi(), getChunkStart());
        engine.process (getDryInput().getArrayOfReadPointers(), buffer.getArrayOfWritePointers(), buffer.getNumSamples(), measure);
        insertMidi.clear();
        rack.takeMidi (insertMidi);
    }

    // MIDI out: what the inserts sent, and the MIDI in when MIDI thru is on.
    for (const auto metadata : insertMidi)
        sendMidi (metadata.getMessage(), metadata.samplePosition);
    if (valueOf (midiThru) >= 0.5f)
        if (const auto* midi = getMidi())
            for (const auto metadata : *midi)
                if (metadata.samplePosition >= getChunkStart() && metadata.samplePosition < getChunkStart() + buffer.getNumSamples())
                    sendMidi (metadata.getMessage(), metadata.samplePosition - getChunkStart());
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
    for (int bus = 0; bus < mix::kBuses; ++bus)
    {
        for (int side = 0; side < 2; ++side)
            raise (busPeaks[(size_t) bus][(size_t) side], engine.takeBusPeak (bus, side));
        lower (chainCompReduction[(size_t) bus], engine.takeBusCompReductionDb (bus));
        lower (chainLimitReduction[(size_t) bus], engine.takeBusLimitReductionDb (bus));
    }
    lower (chainCompReduction[(size_t) mix::kMainChain], engine.getMainDsp().takeCompReductionDb());
    lower (chainLimitReduction[(size_t) mix::kMainChain], engine.getMainDsp().takeLimitReductionDb());
    lower (chainCompReduction[(size_t) mix::kMonoChain], engine.getMonoDsp().takeCompReductionDb());
    lower (chainLimitReduction[(size_t) mix::kMonoChain], engine.getMonoDsp().takeLimitReductionDb());
    raise (cuePeaks[0], engine.takeCuePeak (0));
    raise (cuePeaks[1], engine.takeCuePeak (1));
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
