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

        beginTest ("A channel preset holds the strip, name and colour, not the routing");
        {
            const auto file = juce::File::createTempFile (".json");
            RKMixProcessor a;
            auto& s = a.getSettings();
            setParam (s.param (mix::channelId (2, "hpf_on")), 1.0f);
            setParam (s.param (mix::channelId (2, "hpf")), 150.0f);
            setParam (s.param (mix::channelId (2, "eq_hmid_gain")), 4.5f);
            setParam (s.param (mix::channelId (2, "comp_on")), 1.0f);
            setParam (s.param (mix::channelId (2, "gate_key")), 7.0f);   // routing: not in the preset
            setParam (s.param (mix::channelId (2, "pan")), 50.0f);       // routing: not in the preset
            a.setChannelName (2, "Lead Vocal");
            a.setChannelColour (2, 3);
            expect (a.saveChannelPreset (2, file).ok);

            RKMixProcessor b;
            expect (b.loadChannelPreset (9, file).ok);
            const auto& t = b.getSettings();
            expect (t.flag (mix::channelId (9, "hpf_on")) && t.flag (mix::channelId (9, "comp_on")));
            expectWithinAbsoluteError (t.value (mix::channelId (9, "hpf")), 150.0f, 0.1f);
            expectWithinAbsoluteError (t.value (mix::channelId (9, "eq_hmid_gain")), 4.5f, 0.01f);
            expectEquals (t.choice (mix::channelId (9, "gate_key")), 0, "keys are routing");
            expectWithinAbsoluteError (t.value (mix::channelId (9, "pan")), 0.0f, 0.01f, "pan is routing");
            expectEquals (b.getChannelName (9), juce::String ("Lead Vocal"));
            expectEquals (b.getChannelColour (9), 3);

            b.resetChannelStrip (9);
            expect (! t.flag (mix::channelId (9, "hpf_on")), "Reset strip");
            file.deleteFile();
        }

        beginTest ("The strip's settings reach the audio (HPF on a low tone)");
        {
            RKMixProcessor p;
            p.prepareToPlay (48000.0, 128);
            setParam (p.getSettings().param (mix::channelId (0, "hpf_on")), 1.0f);
            setParam (p.getSettings().param (mix::channelId (0, "hpf")), 400.0f);
            setParam (p.getSettings().param (mix::channelId (0, "hpf_slope")), 1.0f);
            juce::AudioBuffer<float> b (32, 128);
            juce::MidiBuffer midi;
            float loudest = 0.0f;
            for (int n = 0; n < 100; ++n)
            {
                b.clear();
                for (int i = 0; i < 128; ++i)
                    b.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 40.0 * (n * 128 + i) / 48000.0));
                p.processBlock (b, midi);
                if (n > 80)
                    loudest = juce::jmax (loudest, b.getMagnitude (0, 0, 128));
            }
            expectLessThan (loudest, 0.01f, "40 Hz under a 400 Hz 24 dB/oct high-pass");
        }

        beginTest ("Buses through the processor: an Aux send to its output, bus faders, names in the session");
        {
            RKMixProcessor a;
            auto& s = a.getSettings();
            setParam (s.param (mix::busId (0, "format")), 0.0f);             // mono
            setParam (s.param (mix::busId (0, "out")), 5.0f);                // Out 5
            setParam (s.param (mix::channelSendId (0, 0)), 0.0f);            // ch 1 -> bus 1 at 0 dB
            setParam (s.param (mix::busId (2, "type")), 0.0f);               // bus 3: a group
            setParam (s.param (mix::channelAssignId (0, 2)), 1.0f);
            a.setBusName (0, "IEM Vocal");
            a.setBusColour (0, 5);
            a.setOutputName (4, "IEM");
            a.prepareToPlay (48000.0, 128);
            auto b = run (a, 8);
            expectWithinAbsoluteError (b.getSample (4, 64), 0.5f, 1.0e-4f, "pre-fader send, mono bus, unity");

            setParam (*a.findParam (mix::busFaderId (0)), -6.0f);
            b = run (a, 8);
            expectWithinAbsoluteError (b.getSample (4, 64), 0.5f * std::pow (10.0f, -0.3f), 1.0e-4f, "the bus fader");

            juce::MemoryBlock state;
            a.getStateInformation (state);
            RKMixProcessor c;
            c.setStateInformation (state.getData(), (int) state.getSize());
            expect (c.getBusType (2) == mix::BusType::group);
            expect (! c.isBusStereo (0));
            expectEquals (c.getBusName (0), juce::String ("IEM Vocal"));
            expectEquals (c.getBusColour (0), 5);
            expectEquals (c.getOutputName (4), juce::String ("IEM"));
            expect (c.getSettings().flag (mix::channelAssignId (0, 2)));
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
