#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKConduct";
        c.stateVersion = 1;
        c.midiEffect = true;
        c.presetFolders = true;   // a preset is a folder: preset.json and the MIDI file
        c.accent = juce::Colour (0xffb0478f); // MIDI family, plum
        c.family = rkfx::Palette::Family::midi;
        return c;
    }
}

RKConductProcessor::RKConductProcessor()
    : rkfx::ProcessorBase (makeConfig(), ConductParams::create()),
      tapEvery (findParam ("tap_every")), tempo (findParam ("tempo")), fileTempo (findParam ("file_tempo")),
      countIn (findParam ("count_in")), loop (findParam ("loop")), key1 (findParam ("key1")), key2 (findParam ("key2")),
      tapCC (findParam ("tap_cc")), tapButton (findParam ("tap")), stopButton (findParam ("stop")),
      timeNum (findParam ("time_num")), timeDen (findParam ("time_den"))
{
    for (auto& on : trackOn)
        on = true;
    tapButton->addListener (&buttonListener);
    stopButton->addListener (&buttonListener);
}

RKConductProcessor::~RKConductProcessor()
{
    tapButton->removeListener (&buttonListener);
    stopButton->removeListener (&buttonListener);
}

void RKConductProcessor::ButtonListener::parameterValueChanged (int index, float value)
{
    const auto down = value >= 0.5f;
    if (index == owner.tapButton->getParameterIndex())
    {
        if (down && ! tapDown.exchange (true))
            owner.engine.tap();
        else if (! down)
            tapDown = false;
    }
    else if (index == owner.stopButton->getParameterIndex())
    {
        if (down && ! stopDown.exchange (true))
            owner.engine.stop();
        else if (! down)
            stopDown = false;
    }
}

juce::String RKConductProcessor::useSong (const juce::MemoryBlock& data, const juce::String& name)
{
    auto song = ConductEngine::makeSong (data);
    if (song == nullptr)
        return "Not a MIDI file (or one timed in SMPTE frames)";

    songData = data;
    songName = name;
    tracks = song->tracks;
    songLength = song->lengthBeats;
    songTempo = song->tempoBpm;
    engine.setSong (std::move (song));   // the old one is deleted here, on the message thread
    ++songVersion;
    return {};
}

void RKConductProcessor::clearSong()
{
    if (songData.isEmpty() && songName.isEmpty())
        return;
    songData.reset();
    songName.clear();
    tracks.clear();
    songLength = 0.0;
    songTempo = 120.0;
    engine.setSong (nullptr);   // the old one is deleted here, on the message thread
    ++songVersion;
}

juce::String RKConductProcessor::loadFile (const juce::File& file)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data))
        return "Can't read " + file.getFileName();

    const auto error = useSong (data, file.getFileNameWithoutExtension());
    if (error.isEmpty())
        for (int t = 0; t < ConductEngine::kMaxTracks; ++t)
        {
            trackOn[(size_t) t] = true;
            trackChannel[(size_t) t] = 0;
        }
    return error;
}

namespace
{
    juce::var tracksVar (const std::vector<ConductEngine::TrackInfo>& tracks,
                         const std::array<std::atomic<bool>, ConductEngine::kMaxTracks>& trackOn,
                         const std::array<std::atomic<int>, ConductEngine::kMaxTracks>& trackChannel)
    {
        juce::Array<juce::var> list;
        for (int t = 0; t < (int) tracks.size(); ++t)
        {
            auto* entry = new juce::DynamicObject();
            entry->setProperty ("on", trackOn[(size_t) t].load());
            entry->setProperty ("channel", trackChannel[(size_t) t].load());
            list.add (juce::var (entry));
        }
        return list;
    }

    bool isMidiFile (const juce::File& f)
    {
        return f.hasFileExtension ("mid;midi;smf");
    }
}

juce::var RKConductProcessor::getExtraState() const
{
    if (songData.isEmpty())
        return {};

    // The file itself, so a saved gig never depends on where it was.
    auto* state = new juce::DynamicObject();
    state->setProperty ("name", songName);
    state->setProperty ("data", juce::Base64::toBase64 (songData.getData(), songData.getSize()));   // standard base64
    state->setProperty ("tracks", tracksVar (tracks, trackOn, trackChannel));
    return juce::var (state);
}

juce::var RKConductProcessor::writePresetFiles (const juce::File& folder) const
{
    // The preset's folder holds exactly one MIDI file: the song (or none).
    const auto fileName = songData.isEmpty() ? juce::String()
                                             : juce::File::createLegalFileName (songName.isNotEmpty() ? songName : "Song") + ".mid";
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*"))
        if (isMidiFile (f) && f.getFileName() != fileName)
            f.deleteFile();

    if (songData.isEmpty())
        return {};

    const auto target = folder.getChildFile (fileName);
    juce::MemoryBlock existing;
    if (! (target.loadFileAsData (existing) && existing == songData))
    {
        juce::TemporaryFile temp (target);
        if (! (temp.getFile().replaceWithData (songData.getData(), songData.getSize()) && temp.overwriteTargetFileWithTemporary()))
            return getExtraState();   // couldn't write it: embed it, so nothing is lost
    }

    auto* state = new juce::DynamicObject();
    state->setProperty ("name", songName);
    state->setProperty ("file", fileName);
    state->setProperty ("tracks", tracksVar (tracks, trackOn, trackChannel));
    return juce::var (state);
}

juce::String RKConductProcessor::readPresetFiles (const juce::var& extra, const juce::File& folder)
{
    if (extra.isObject() && extra.hasProperty ("data"))   // embedded (a preset that couldn't write its file)
    {
        setExtraState (extra);
        return {};
    }

    const auto fileName = extra.isObject() ? extra["file"].toString() : juce::String();
    if (fileName.isEmpty())
    {
        clearSong();
        return {};
    }

    juce::MemoryBlock data;
    if (! folder.getChildFile (fileName).loadFileAsData (data))
    {
        clearSong();
        return "MIDI file missing from the preset folder: " + fileName;
    }

    if (const auto error = useSong (data, extra["name"].toString()); error.isNotEmpty())
    {
        clearSong();
        return fileName + ": " + error;
    }
    applyTracks (extra["tracks"]);
    return {};
}

void RKConductProcessor::applyTracks (const juce::var& tracksState)
{
    for (int t = 0; t < ConductEngine::kMaxTracks; ++t)
    {
        trackOn[(size_t) t] = true;
        trackChannel[(size_t) t] = 0;
    }
    if (const auto* list = tracksState.getArray())
        for (int t = 0; t < list->size() && t < ConductEngine::kMaxTracks; ++t)
        {
            trackOn[(size_t) t] = (bool) (*list)[t]["on"];
            trackChannel[(size_t) t] = juce::jlimit (0, 16, (int) (*list)[t]["channel"]);
        }
}

void RKConductProcessor::setExtraState (const juce::var& state)
{
    juce::MemoryBlock data;
    juce::MemoryOutputStream decoded (data, false);
    if (! state.isObject() || ! juce::Base64::convertFromBase64 (decoded, state["data"].toString()))
        return;
    decoded.flush();

    if (useSong (data, state["name"].toString()).isNotEmpty())
        return;

    applyTracks (state["tracks"]);
}

void RKConductProcessor::processMidi (juce::MidiBuffer& midi, int numSamples)
{
    ConductEngine::Settings s;
    const auto every = juce::jlimit (0, ConductParams::kBarChoice, juce::roundToInt (valueOf (tapEvery)));
    s.tapBeats = every == ConductParams::kBarChoice ? getBarBeats() : ConductParams::tapBeats[every];
    s.tempoBpm = valueOf (fileTempo) >= 0.5f ? songTempo.load() : (double) valueOf (tempo);
    s.countIn = juce::roundToInt (valueOf (countIn));
    s.loop = valueOf (loop) >= 0.5f;
    s.key1 = juce::roundToInt (valueOf (key1));
    s.key2 = juce::roundToInt (valueOf (key2));
    s.tapController = juce::roundToInt (valueOf (tapCC));
    for (int t = 0; t < ConductEngine::kMaxTracks; ++t)
    {
        s.trackOn[(size_t) t] = trackOn[(size_t) t].load();
        s.trackChannel[(size_t) t] = trackChannel[(size_t) t].load();
    }
    engine.setSettings (s);
    engine.process (midi, numSamples);
}

int RKConductProcessor::getBeatsPerBar() const noexcept
{
    return juce::jlimit (1, 16, juce::roundToInt (valueOf (timeNum)));
}

int RKConductProcessor::getBeatValue() const noexcept
{
    return ConductParams::denominators[juce::jlimit (0, 3, juce::roundToInt (valueOf (timeDen)))];
}

double RKConductProcessor::getBarBeats() const noexcept
{
    return ConductParams::barBeats (getBeatsPerBar(), getBeatValue());
}

juce::AudioProcessorEditor* RKConductProcessor::createEditor()
{
    return new RKConductEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKConductProcessor();
}
#endif
