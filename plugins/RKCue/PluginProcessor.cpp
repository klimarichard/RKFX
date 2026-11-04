#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKCue";
        c.stateVersion = 1;
        c.midiEffect = true;
        c.presetFolders = true;   // a preset is a folder: preset.json and the slots' MIDI files
        c.accent = juce::Colour (0xffb66bd0); // MIDI family, lilac
        c.family = rkfx::Palette::Family::midi;
        return c;
    }

    bool isMidiFile (const juce::File& f)
    {
        return f.hasFileExtension ("mid;midi;smf");
    }
}

RKCueProcessor::RKCueProcessor()
    : rkfx::ProcessorBase (makeConfig(), CueParams::create()),
      tempoSource (findParam ("tempo_source")), tempo (findParam ("tempo")), thru (findParam ("thru")),
      trigChannel (findParam ("trig_channel")), stopAll (findParam ("stop_all"))
{
    using CueParams::slotId;
    for (int s = 0; s < kSlots; ++s)
    {
        auto& p = slotParams[(size_t) s];
        p.low       = findParam (slotId (s, "low"));
        p.high      = findParam (slotId (s, "high"));
        p.root      = findParam (slotId (s, "root"));
        p.pitch     = findParam (slotId (s, "pitch"));
        p.transpose = findParam (slotId (s, "transpose"));
        p.drums     = findParam (slotId (s, "drums"));
        p.mode      = findParam (slotId (s, "mode"));
        p.loop      = findParam (slotId (s, "loop"));
        p.poly      = findParam (slotId (s, "poly"));
        p.velocity  = findParam (slotId (s, "velocity"));
        p.channel   = findParam (slotId (s, "channel"));
        playButtons[(size_t) s] = findParam (CueParams::playId (s));
        stopButtons[(size_t) s] = findParam (CueParams::stopId (s));
        playButtons[(size_t) s]->addListener (&buttonListener);
        stopButtons[(size_t) s]->addListener (&buttonListener);
    }
    stopAll->addListener (&buttonListener);
}

RKCueProcessor::~RKCueProcessor()
{
    for (int s = 0; s < kSlots; ++s)
    {
        playButtons[(size_t) s]->removeListener (&buttonListener);
        stopButtons[(size_t) s]->removeListener (&buttonListener);
    }
    stopAll->removeListener (&buttonListener);
}

void RKCueProcessor::ButtonListener::parameterValueChanged (int index, float value)
{
    const auto pressed = value >= 0.5f;

    // Acts on the press; a repeated "on" (a host re-sending the value) doesn't.
    auto edge = [&] (size_t which) { return pressed ? ! down[which].exchange (true) : down[which].exchange (false); };

    if (index == owner.stopAll->getParameterIndex())
    {
        if (edge (2 * kSlots) && pressed)
            owner.engine.stopAll();
        return;
    }
    for (int s = 0; s < kSlots; ++s)
    {
        if (index == owner.playButtons[(size_t) s]->getParameterIndex())
        {
            if (edge ((size_t) s))
            {
                if (pressed)
                    owner.engine.play (s);
                else
                    owner.engine.release (s);
            }
            return;
        }
        if (index == owner.stopButtons[(size_t) s]->getParameterIndex())
        {
            if (edge ((size_t) (kSlots + s)) && pressed)
                owner.engine.stop (s);
            return;
        }
    }
}

//==============================================================================
juce::String RKCueProcessor::useFile (int slot, const juce::MemoryBlock& data, const juce::String& name)
{
    auto song = rkfx::MidiSong::read (data, CueEngine::kMaxTracks);
    if (song == nullptr)
        return "Not a MIDI file (or one timed in SMPTE frames)";

    auto& f = files[(size_t) slot];
    f.data = data;
    f.name = name;
    f.tracks = song->tracks;
    f.lengthBeats = song->lengthBeats;
    f.lengthSeconds = song->lengthSeconds();
    f.tempoBpm = song->tempoBpm;
    engine.setSong (slot, std::move (song));   // the old one is deleted here, on the message thread
    ++filesVersion;
    return {};
}

void RKCueProcessor::clearSlot (int slot)
{
    auto& f = files[(size_t) slot];
    if (! f.hasFile() && f.name.isEmpty())
        return;
    f = SlotFile();
    engine.setSong (slot, nullptr);   // the old one is deleted here, on the message thread
    ++filesVersion;
    markPresetModified();
}

void RKCueProcessor::clearAll()
{
    for (int s = 0; s < kSlots; ++s)
    {
        auto& f = files[(size_t) s];
        if (! f.hasFile() && f.name.isEmpty())
            continue;
        f = SlotFile();
        engine.setSong (s, nullptr);
        ++filesVersion;
    }
}

juce::String RKCueProcessor::loadFile (int slot, const juce::File& file)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data))
        return "Can't read " + file.getFileName();
    const auto error = useFile (slot, data, file.getFileNameWithoutExtension());
    if (error.isEmpty())
        markPresetModified();
    return error;
}

void RKCueProcessor::setSlotName (int slot, const juce::String& name)
{
    auto& f = files[(size_t) slot];
    if (! f.hasFile() || f.name == name)
        return;
    f.name = name;
    ++filesVersion;
    markPresetModified();
}

//==============================================================================
juce::String RKCueProcessor::presetFileName (int slot, const juce::String& name)
{
    return juce::String (slot + 1).paddedLeft ('0', 2) + " " + juce::File::createLegalFileName (name.isNotEmpty() ? name : "Slot") + ".mid";
}

juce::var RKCueProcessor::getExtraState() const
{
    // The files themselves, so a saved gig never depends on where they were.
    juce::Array<juce::var> list;
    for (int s = 0; s < kSlots; ++s)
    {
        const auto& f = files[(size_t) s];
        if (! f.hasFile())
            continue;
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("slot", s + 1);
        entry->setProperty ("name", f.name);
        entry->setProperty ("data", juce::Base64::toBase64 (f.data.getData(), f.data.getSize()));   // standard base64
        list.add (juce::var (entry));
    }
    if (list.isEmpty())
        return {};

    auto* state = new juce::DynamicObject();
    state->setProperty ("slots", list);
    return juce::var (state);
}

void RKCueProcessor::setExtraState (const juce::var& state)
{
    clearAll();
    if (const auto* list = state["slots"].getArray())
        for (const auto& entry : *list)
        {
            const auto slot = (int) entry["slot"] - 1;
            if (slot < 0 || slot >= kSlots)
                continue;
            juce::MemoryBlock data;
            juce::MemoryOutputStream decoded (data, false);
            if (! juce::Base64::convertFromBase64 (decoded, entry["data"].toString()))
                continue;
            decoded.flush();
            useFile (slot, data, entry["name"].toString());
        }
}

juce::var RKCueProcessor::writePresetFiles (const juce::File& folder) const
{
    // The preset's folder holds exactly the slots' MIDI files.
    juce::StringArray names;
    for (int s = 0; s < kSlots; ++s)
        if (files[(size_t) s].hasFile())
            names.add (presetFileName (s, files[(size_t) s].name));
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*"))
        if (isMidiFile (f) && ! names.contains (f.getFileName()))
            f.deleteFile();

    juce::Array<juce::var> list;
    for (int s = 0; s < kSlots; ++s)
    {
        const auto& f = files[(size_t) s];
        if (! f.hasFile())
            continue;

        auto* entry = new juce::DynamicObject();
        entry->setProperty ("slot", s + 1);
        entry->setProperty ("name", f.name);

        const auto target = folder.getChildFile (presetFileName (s, f.name));
        juce::MemoryBlock existing;
        auto written = target.loadFileAsData (existing) && existing == f.data;
        if (! written)
        {
            juce::TemporaryFile temp (target);
            written = temp.getFile().replaceWithData (f.data.getData(), f.data.getSize()) && temp.overwriteTargetFileWithTemporary();
        }
        if (written)
            entry->setProperty ("file", target.getFileName());
        else
            entry->setProperty ("data", juce::Base64::toBase64 (f.data.getData(), f.data.getSize()));   // couldn't write it: embed it, so nothing is lost
        list.add (juce::var (entry));
    }
    if (list.isEmpty())
        return {};

    auto* state = new juce::DynamicObject();
    state->setProperty ("slots", list);
    return juce::var (state);
}

juce::String RKCueProcessor::readPresetFiles (const juce::var& extra, const juce::File& folder)
{
    clearAll();   // a preset's empty slots are empty
    juce::StringArray missing;
    if (const auto* list = extra["slots"].getArray())
        for (const auto& entry : *list)
        {
            const auto slot = (int) entry["slot"] - 1;
            if (slot < 0 || slot >= kSlots)
                continue;

            juce::MemoryBlock data;
            const auto fileName = entry["file"].toString();
            if (entry.hasProperty ("data"))
            {
                juce::MemoryOutputStream decoded (data, false);
                if (! juce::Base64::convertFromBase64 (decoded, entry["data"].toString()))
                    continue;
            }
            else if (fileName.isEmpty() || ! folder.getChildFile (fileName).loadFileAsData (data))
            {
                missing.add (fileName.isNotEmpty() ? fileName : entry["name"].toString());
                continue;
            }

            if (const auto error = useFile (slot, data, entry["name"].toString()); error.isNotEmpty())
                missing.add (fileName + ": " + error);
        }

    return missing.isEmpty() ? juce::String()
                             : "MIDI files missing from the preset folder: " + missing.joinIntoString (", ");
}

//==============================================================================
void RKCueProcessor::processMidi (juce::MidiBuffer& midi, int numSamples)
{
    CueEngine::Settings s;
    s.tempoSource = (CueEngine::TempoSource) juce::jlimit (0, 2, juce::roundToInt (valueOf (tempoSource)));
    s.fixedBpm = valueOf (tempo);
    s.hostBpm = getTransport().bpm;
    s.hostHasBpm = getTransport().hasBpm;
    s.thru = valueOf (thru) >= 0.5f;
    s.triggerChannel = juce::jlimit (0, 16, juce::roundToInt (valueOf (trigChannel)));

    for (int n = 0; n < kSlots; ++n)
    {
        const auto& p = slotParams[(size_t) n];
        auto& slot = s.slots[(size_t) n];
        slot.low        = juce::roundToInt (valueOf (p.low));
        slot.high       = juce::roundToInt (valueOf (p.high));
        slot.root       = juce::roundToInt (valueOf (p.root));
        slot.trackPitch = valueOf (p.pitch) >= 0.5f;
        slot.transpose  = juce::roundToInt (valueOf (p.transpose));
        slot.keepDrums  = valueOf (p.drums) >= 0.5f;
        slot.mode       = (CueEngine::Mode) juce::jlimit (0, 2, juce::roundToInt (valueOf (p.mode)));
        slot.loop       = valueOf (p.loop) >= 0.5f;
        slot.poly       = valueOf (p.poly) >= 0.5f;
        slot.velocity   = valueOf (p.velocity) / 100.0f;
        slot.channel    = juce::jlimit (0, 16, juce::roundToInt (valueOf (p.channel)));
    }
    engine.setSettings (s);
    engine.process (midi, numSamples);
}

juce::AudioProcessorEditor* RKCueProcessor::createEditor()
{
    return new RKCueEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKCueProcessor();
}
#endif
