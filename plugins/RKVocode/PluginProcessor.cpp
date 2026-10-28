#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKVocode";
        c.stateVersion = 1;
        c.hasMix = false;
        c.sidechain = true;                   // external carrier
        c.midiInput = true;                   // internal carrier played by MIDI
        c.monoToStereo = true;
        c.accent = juce::Colour (0xff5a3aa8); // Filter and EQ family, indigo violet
        c.family = rkfx::Palette::Family::filterEQ;
        return c;
    }
}

RKVocodeProcessor::RKVocodeProcessor()
    : rkfx::ProcessorBase (makeConfig(), VocodeParams::create()),
      bands (findParam ("bands")), freqLow (findParam ("freq_low")), freqHigh (findParam ("freq_high")),
      formant (findParam ("formant")), attack (findParam ("attack")), release (findParam ("release")),
      unvoiced (findParam ("unvoiced")), modMix (findParam ("mod_mix")), carrier (findParam ("carrier")),
      oscWave (findParam ("osc_wave")), oscMode (findParam ("osc_mode")), oscNote (findParam ("osc_note")),
      oscVoices (findParam ("osc_voices")), oscGlide (findParam ("osc_glide"))
{
}

VocodeEngine::Settings RKVocodeProcessor::currentSettings() const
{
    VocodeEngine::Settings s;
    s.bands            = VocodeParams::bandCounts[juce::jlimit (0, 3, juce::roundToInt (valueOf (bands)))];
    s.lowHz            = valueOf (freqLow);
    s.highHz           = valueOf (freqHigh);
    s.formantSemitones = valueOf (formant);
    s.attackMs         = valueOf (attack);
    s.releaseMs        = valueOf (release);
    s.unvoiced         = valueOf (unvoiced) / 100.0f;
    s.modulatorMix     = valueOf (modMix) / 100.0f;
    s.carrier          = valueOf (carrier) >= 0.5f ? VocodeEngine::Carrier::internal : VocodeEngine::Carrier::sidechain;
    s.wave             = (VocodeEngine::Wave) juce::jlimit (0, 3, juce::roundToInt (valueOf (oscWave)));
    s.pitchMode        = valueOf (oscMode) >= 0.5f ? VocodeEngine::PitchMode::fixed : VocodeEngine::PitchMode::midi;
    s.fixedNote        = juce::roundToInt (valueOf (oscNote));
    s.voices           = juce::roundToInt (valueOf (oscVoices));
    s.glideMs          = valueOf (oscGlide);
    return s;
}

void RKVocodeProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKVocodeProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKVocodeProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());

    const auto* sc = getSidechain();
    const auto* scData = sc != nullptr ? sc->getArrayOfReadPointers() : nullptr;
    const auto scChannels = sc != nullptr ? sc->getNumChannels() : 0;
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    // Process up to each MIDI event, apply it, carry on: notes start sample-accurately.
    int done = 0;
    const auto run = [&] (int upTo)
    {
        if (upTo <= done)
            return;
        float* io[2] = { buffer.getWritePointer (0, done), buffer.getWritePointer (numChannels - 1, done) };
        const float* side[2] = { nullptr, nullptr };
        if (scData != nullptr)
        {
            side[0] = scData[0] + done;
            side[1] = scData[juce::jmin (1, scChannels - 1)] + done;
        }
        engine.process (io, numChannels, upTo - done, scData != nullptr ? side : nullptr, juce::jmin (2, scChannels));
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

            run (position);
            const auto m = event.getMessage();
            if (m.isNoteOn())
                engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff())
                engine.noteOff (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                engine.allNotesOff();
        }
    }

    run (numSamples);
}

juce::AudioProcessorEditor* RKVocodeProcessor::createEditor()
{
    return new RKVocodeEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKVocodeProcessor();
}
