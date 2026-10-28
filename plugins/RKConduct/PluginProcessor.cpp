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
        c.accent = juce::Colour (0xffb0478f); // MIDI family, plum
        c.family = rkfx::Palette::Family::midi;
        return c;
    }
}

RKConductProcessor::RKConductProcessor()
    : rkfx::ProcessorBase (makeConfig(), ConductParams::create()),
      tapEvery (findParam ("tap_every")), tempo (findParam ("tempo")), fileTempo (findParam ("file_tempo")),
      countIn (findParam ("count_in")), loop (findParam ("loop")), key1 (findParam ("key1")), key2 (findParam ("key2")),
      tapCC (findParam ("tap_cc")), tapButton (findParam ("tap")), stopButton (findParam ("stop"))
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

juce::var RKConductProcessor::getExtraState() const
{
    if (songData.isEmpty())
        return {};

    // The file itself, so a saved gig never depends on where it was.
    auto* state = new juce::DynamicObject();
    state->setProperty ("name", songName);
    state->setProperty ("data", juce::Base64::toBase64 (songData.getData(), songData.getSize()));   // standard base64
    juce::Array<juce::var> list;
    for (int t = 0; t < (int) tracks.size(); ++t)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("on", trackOn[(size_t) t].load());
        entry->setProperty ("channel", trackChannel[(size_t) t].load());
        list.add (juce::var (entry));
    }
    state->setProperty ("tracks", list);
    return juce::var (state);
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

    if (const auto* list = state["tracks"].getArray())
        for (int t = 0; t < list->size() && t < ConductEngine::kMaxTracks; ++t)
        {
            trackOn[(size_t) t] = (bool) (*list)[t]["on"];
            trackChannel[(size_t) t] = juce::jlimit (0, 16, (int) (*list)[t]["channel"]);
        }
}

void RKConductProcessor::processMidi (juce::MidiBuffer& midi, int numSamples)
{
    ConductEngine::Settings s;
    s.tapBeats = ConductParams::tapBeats[juce::jlimit (0, 7, juce::roundToInt (valueOf (tapEvery)))];
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
