#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKVoice";
        c.stateVersion = 1;
        c.hasMix = false;                     // Dry and the voice levels are the mix
        c.monoToStereo = true;                // a mono mic in, the voices spread in stereo
        c.midiInput = true;                   // the chord
        c.accent = juce::Colour (0xff8c8ff0); // Modulation family, periwinkle
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }

    constexpr VoiceEngine::Range ranges[] = { VoiceEngine::Range::voice, VoiceEngine::Range::bass,
                                              VoiceEngine::Range::guitar, VoiceEngine::Range::violin };
}

RKVoiceProcessor::RKVoiceProcessor()
    : rkfx::ProcessorBase (makeConfig(), VoiceParams::create()),
      mode (findParam ("mode")), range (findParam ("range")), chordTones (findParam ("chord_tones")),
      noChord (findParam ("no_chord")), midiChannel (findParam ("midi_channel")), key (findParam ("key")),
      scale (findParam ("scale")), detune (findParam ("detune")), humanize (findParam ("humanize")),
      glide (findParam ("glide")), dry (findParam ("dry")), threshold (findParam ("threshold"))
{
    for (int v = 0; v < VoiceEngine::kNumVoices; ++v)
        voiceControls[(size_t) v] = { findParam (VoiceParams::voiceId (v + 1, "position")), findParam (VoiceParams::voiceId (v + 1, "interval")),
                                    findParam (VoiceParams::voiceId (v + 1, "level")), findParam (VoiceParams::voiceId (v + 1, "pan")) };
}

VoiceEngine::Settings RKVoiceProcessor::currentSettings() const
{
    auto choice = [] (const juce::RangedAudioParameter* p, int count) { return juce::jlimit (0, count - 1, juce::roundToInt (valueOf (p))); };

    VoiceEngine::Settings s;
    s.mode        = (VoiceEngine::Mode) choice (mode, 2);
    s.range       = ranges[choice (range, 4)];
    s.chordTones  = (VoiceEngine::ChordTones) choice (chordTones, 2);
    s.noChord     = (VoiceEngine::NoChord) choice (noChord, 3);
    s.key         = choice (key, 12);
    s.scale       = (VoiceEngine::Scale) choice (scale, rkfx::scales::kNumScales);
    for (size_t v = 0; v < s.voices.size(); ++v)
    {
        const auto& vp = voiceControls[v];
        s.voices[v].position = VoiceParams::positions[choice (vp.position, 7)];
        s.voices[v].interval = (VoiceEngine::Interval) choice (vp.interval, rkfx::scales::kNumIntervals);
        s.voices[v].levelDb  = valueOf (vp.level);
        s.voices[v].pan      = valueOf (vp.pan) / 100.0f;
    }
    s.detuneCents = valueOf (detune);
    s.humanizeMs  = valueOf (humanize);
    s.glideMs     = valueOf (glide);
    s.dryDb       = valueOf (dry);
    s.thresholdDb = valueOf (threshold);
    return s;
}

void RKVoiceProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKVoiceProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKVoiceProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const auto channel = juce::roundToInt (valueOf (midiChannel));   // 0 = omni

    // Process up to each MIDI event, apply it, carry on: a chord change lands on its sample.
    int done = 0;
    const auto run = [&] (int upTo)
    {
        if (upTo <= done)
            return;
        float* io[2] = { buffer.getWritePointer (0, done), buffer.getWritePointer (numChannels - 1, done) };
        engine.process (io, numChannels, upTo - done);
        done = upTo;
    };

    if (const auto* midi = getMidi())
    {
        const auto start = getChunkStart();
        for (const auto event : *midi)
        {
            const auto position = event.samplePosition - start;
            if (position < 0 || position >= numSamples)
                continue;
            const auto m = event.getMessage();
            if (channel != 0 && m.getChannel() != channel)
                continue;

            run (position);
            if (m.isNoteOn())
                engine.noteOn (m.getNoteNumber());
            else if (m.isNoteOff())
                engine.noteOff (m.getNoteNumber());
            else if (m.isSustainPedalOn())
                engine.setSustain (true);
            else if (m.isSustainPedalOff())
                engine.setSustain (false);
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                engine.allNotesOff();
        }
    }

    run (numSamples);
}

juce::AudioProcessorEditor* RKVoiceProcessor::createEditor()
{
    return new RKVoiceEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKVoiceProcessor();
}
#endif
