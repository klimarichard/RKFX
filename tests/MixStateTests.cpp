#include <juce_audio_processors/juce_audio_processors.h>

#include "../plugins/RKMix/PluginProcessor.h"

/** RKMix as a plugin: 32 channels each way, the default console, bypass to
    silence, and the mix (settings, names, colours) kept in the session and
    in preset files. Built into RKBoardTests, which links the processors. */
class MixStateTests : public juce::UnitTest
{
public:
    MixStateTests() : juce::UnitTest ("RKMix state") {}

    static void setParam (juce::RangedAudioParameter& p, float realValue)
    {
        p.setValueNotifyingHost (p.convertTo0to1 (realValue));
    }

    /** Processes @p blocks blocks with input 1 a steady 0.5 and every other input silent. */
    static juce::AudioBuffer<float> run (RKMixProcessor& p, int blocks = 1)
    {
        juce::AudioBuffer<float> b (32, 128);
        juce::MidiBuffer midi;
        for (int n = 0; n < blocks; ++n)
        {
            b.clear();
            for (int i = 0; i < 128; ++i)
                b.setSample (0, i, 0.5f);
            p.processBlock (b, midi);
        }
        return b;
    }

    void runTest() override
    {
        beginTest ("32 inputs and 32 outputs; a channel reaches the main on Out 1/2, -3 dB centred");
        {
            RKMixProcessor p;
            expectEquals (p.getTotalNumInputChannels(), 32);
            expectEquals (p.getTotalNumOutputChannels(), 32);
            p.prepareToPlay (48000.0, 128);
            const auto b = run (p);
            expectWithinAbsoluteError (b.getSample (0, 64), 0.5f * std::sqrt (0.5f), 1.0e-5f);
            expectWithinAbsoluteError (b.getSample (1, 64), 0.5f * std::sqrt (0.5f), 1.0e-5f);
            float rest = 0.0f;
            for (int ch = 2; ch < 32; ++ch)
                rest = juce::jmax (rest, b.getMagnitude (ch, 0, 128));
            expectEquals (rest, 0.0f, "nothing else on the other outputs");
        }

        beginTest ("Bypass is silence on every output");
        {
            RKMixProcessor p;
            p.prepareToPlay (48000.0, 128);
            setParam (*p.findParam ("bypass"), 1.0f);
            const auto b = run (p, 10);   // past the 10 ms fade
            float loudest = 0.0f;
            for (int ch = 0; ch < 32; ++ch)
                loudest = juce::jmax (loudest, b.getMagnitude (ch, 0, 128));
            expectEquals (loudest, 0.0f);
        }

        beginTest ("A mix setting reaches the audio; host faders and mutes too");
        {
            RKMixProcessor p;
            p.prepareToPlay (48000.0, 128);
            setParam (p.getSettings().param (mix::channelId (0, "main")), 0.0f);
            expectEquals (run (p, 8).getMagnitude (0, 0, 128), 0.0f, "Main switch off");

            setParam (p.getSettings().param (mix::channelId (0, "main")), 1.0f);
            setParam (*p.findParam (mix::channelMuteId (0)), 1.0f);
            expectEquals (run (p, 8).getMagnitude (0, 0, 128), 0.0f, "muted");

            setParam (*p.findParam (mix::channelMuteId (0)), 0.0f);
            setParam (p.getSettings().param ("main_out"), (float) mix::pairTarget (8));   // Out 9/10
            const auto b = run (p, 8);
            expectEquals (b.getMagnitude (0, 0, 128), 0.0f);
            expectGreaterThan (b.getMagnitude (8, 0, 128), 0.3f, "the main moved to Out 9/10");
        }

        beginTest ("The mix is kept in the session");
        {
            RKMixProcessor a;
            setParam (a.getSettings().param (mix::linkId (2)), 1.0f);
            setParam (a.getSettings().param (mix::channelId (4, "trim")), 7.5f);
            setParam (a.getSettings().param ("mono_out"), 12.0f);
            setParam (*a.findParam (mix::channelFaderId (4)), -12.0f);
            a.setChannelName (4, "Keys");
            a.setChannelColour (4, 6);
            juce::MemoryBlock state;
            a.getStateInformation (state);

            RKMixProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expect (b.isLinked (2));
            expectWithinAbsoluteError (b.getSettings().value (mix::channelId (4, "trim")), 7.5f, 0.01f);
            expectEquals (b.getSettings().choice ("mono_out"), 12);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (b.findParam (mix::channelFaderId (4))), -12.0f, 0.01f);
            expectEquals (b.getChannelName (4), juce::String ("Keys"));
            expectEquals (b.getChannelColour (4), 6);
        }

        beginTest ("The mix is kept in preset files, and Reset clears it");
        {
            const auto file = juce::File::createTempFile (".json");
            RKMixProcessor a;
            setParam (a.getSettings().param (mix::channelId (0, "pan")), -30.0f);
            a.setChannelName (0, "Vocal");
            expect (a.isPresetModified(), "a setting or name marks the preset modified");
            expect (a.savePreset (file).ok);

            RKMixProcessor b;
            expect (b.loadPreset (file).ok);
            expectWithinAbsoluteError (b.getSettings().value (mix::channelId (0, "pan")), -30.0f, 0.01f);
            expectEquals (b.getChannelName (0), juce::String ("Vocal"));
            expect (! b.isPresetModified());

            b.resetToDefaults();
            expectWithinAbsoluteError (b.getSettings().value (mix::channelId (0, "pan")), 0.0f, 0.01f);
            expect (b.getChannelName (0).isEmpty(), "Reset: no names");
            file.deleteFile();
        }

        beginTest ("The host sees the faders and mutes, assignables, scene and talk (134 + bypass)");
        {
            RKMixProcessor p;
            expectEquals (p.getParameters().size(), 135);
            expect (p.findParam ("ch_fader_32") != nullptr && p.findParam ("bus_mute_16") != nullptr
                    && p.findParam ("mono_mute") != nullptr && p.findParam ("assign32") != nullptr
                    && p.findParam ("scene") != nullptr && p.findParam ("talk") != nullptr);
        }
    }
};

static MixStateTests mixStateTests;
