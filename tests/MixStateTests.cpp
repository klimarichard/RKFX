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
            setParam (s.param (mix::channelId (2, "direct_out")), 9.0f); // routing: not in the preset
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
            expectEquals (t.choice (mix::channelId (9, "direct_out")), mix::kTargetOff, "the direct out is routing");
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

        beginTest ("Inserts: mono or stereo as their channel; kept in the session with their settings and on/off");
        {
            using Strip = InsertRack::Strip;
            RKMixProcessor a;
            a.prepareToPlay (48000.0, 128);
            auto& rack = a.getRack();
            auto* chorus = rack.insert ({ Strip::channel, 0, 0 }, "RKChorus", a.channelsOf (Strip::channel, 0));
            expect (chorus != nullptr);
            expectEquals (chorus->getTotalNumOutputChannels(), 1, "a mono channel's insert runs mono");

            setParam (a.getSettings().param (mix::linkId (0)), 1.0f);    // channels 1/2 linked
            expectEquals (chorus->getTotalNumOutputChannels(), 2, "linked: stereo");

            setParam (*chorus->findParam ("depth"), 77.0f);
            rack.insert ({ Strip::bus, 3, 2 }, "RKVerb", a.channelsOf (Strip::bus, 3));
            rack.setOn ({ Strip::bus, 3, 2 }, false);
            rack.insert ({ Strip::main, 0, 0 }, "RKLimit", 2);
            const auto b = run (a, 4);
            expect (std::isfinite (b.getSample (0, 64)));

            juce::MemoryBlock state;
            a.getStateInformation (state);
            RKMixProcessor c;
            c.setStateInformation (state.getData(), (int) state.getSize());
            auto& r = c.getRack();
            expect (r.get ({ Strip::channel, 0, 0 }) != nullptr && r.get ({ Strip::channel, 0, 0 })->getConfig().name == "RKChorus");
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (r.get ({ Strip::channel, 0, 0 })->findParam ("depth")), 77.0f, 0.01f);
            expectEquals (r.get ({ Strip::channel, 0, 0 })->getTotalNumOutputChannels(), 2);
            expect (r.get ({ Strip::bus, 3, 2 }) != nullptr && ! r.isOn ({ Strip::bus, 3, 2 }), "off stays off");
            expect (r.get ({ Strip::main, 0, 0 }) != nullptr);
            expect (r.get ({ Strip::channel, 0, 1 }) == nullptr);
        }

        beginTest ("Inserts: in channel presets; Reset strip removes them");
        {
            using Strip = InsertRack::Strip;
            const auto file = juce::File::createTempFile (".json");
            RKMixProcessor a;
            a.getRack().insert ({ Strip::channel, 4, 1 }, "RKDeEss", 1);
            expect (a.saveChannelPreset (4, file).ok);

            RKMixProcessor b;
            expect (b.loadChannelPreset (7, file).ok);
            expect (b.getRack().get ({ Strip::channel, 7, 1 }) != nullptr && b.getRack().get ({ Strip::channel, 7, 1 })->getConfig().name == "RKDeEss");
            b.resetChannelStrip (7);
            expect (b.getRack().get ({ Strip::channel, 7, 1 }) == nullptr);
            file.deleteFile();
        }

        beginTest ("Inserts: a latent insert's latency is reported, and the other channels wait for it");
        {
            using Strip = InsertRack::Strip;
            RKMixProcessor p;
            p.prepareToPlay (48000.0, 128);
            auto* distort = p.getRack().insert ({ Strip::channel, 0, 0 }, "RKDistort", 1);
            const auto latency = distort->getLatencySamples();
            expectGreaterThan (latency, 0, "RKDistort oversamples");

            setParam (p.getSettings().param (mix::channelId (0, "main")), 0.0f);   // only channel 3 on the main
            juce::AudioBuffer<float> b (32, 128);
            juce::MidiBuffer midi;
            int at = -1;
            for (int n = 0; n < 4 && at < 0; ++n)
            {
                b.clear();
                if (n == 1)
                    b.setSample (2, 10, 1.0f);
                p.processBlock (b, midi);
                for (int i = 0; i < 128 && at < 0; ++i)
                    if (std::abs (b.getSample (0, i)) > 0.1f)
                        at = n * 128 + i;
            }
            expectEquals (p.getLatencySamples(), latency, "reported to the host");
            expectEquals (at, 128 + 10 + latency, "channel 3 lined up with channel 1's insert");
        }

        beginTest ("A bus switched between mono and stereo keeps its place: Out 5/6 <-> Out 5");
        {
            RKMixProcessor p;
            auto& s = p.getSettings();
            setParam (s.param (mix::busId (2, "out")), (float) mix::pairTarget (4));   // stereo bus on Out 5/6
            setParam (s.param (mix::busId (2, "format")), 0.0f);
            expectEquals (s.choice (mix::busId (2, "out")), 5, "mono: Out 5");
            setParam (s.param (mix::busId (2, "out")), 6.0f);                          // Out 6
            setParam (s.param (mix::busId (2, "format")), 1.0f);
            expectEquals (s.choice (mix::busId (2, "out")), mix::pairTarget (4), "stereo: Out 5/6");
        }

        beginTest ("A channel's direct out follows its pair: Out 9 <-> Out 9/10");
        {
            RKMixProcessor p;
            auto& s = p.getSettings();
            setParam (s.param (mix::channelId (4, "direct_out")), 9.0f);
            setParam (s.param (mix::linkId (2)), 1.0f);                                 // channels 5/6
            expectEquals (s.choice (mix::channelId (4, "direct_out")), mix::pairTarget (8), "linked: Out 9/10");
            setParam (s.param (mix::linkId (2)), 0.0f);
            expectEquals (s.choice (mix::channelId (4, "direct_out")), 9, "unlinked: Out 9");
        }

        beginTest ("User pages: kept in the session in the order chosen; at most 16 strips, no repeats");
        {
            RKMixProcessor a;
            a.setUserPage (0, { 6, RKMixProcessor::kUserPageBus + 2, 0, 6 });
            juce::Array<int> many;
            for (int ch = 0; ch < 20; ++ch)
                many.add (ch);
            a.setUserPage (1, many);
            expectEquals (a.getUserPage (0).size(), 3, "no repeats");
            expectEquals (a.getUserPage (1).size(), 16, "16 at most");

            juce::MemoryBlock state;
            a.getStateInformation (state);
            RKMixProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expect (b.getUserPage (0) == juce::Array<int> { 6, RKMixProcessor::kUserPageBus + 2, 0 });
            expectEquals (b.getUserPage (1).size(), 16);

            b.resetToDefaults();
            expect (b.getUserPage (0).isEmpty(), "Reset clears them");
        }

        beginTest ("Bus and main processing reach the audio and are kept in the session; pan mode too");
        {
            RKMixProcessor a;
            auto& s = a.getSettings();
            setParam (s.param (mix::busId (0, "out")), (float) mix::pairTarget (2));     // Out 3/4
            setParam (s.param (mix::channelSendId (0, 0)), 0.0f);
            setParam (*a.findParam (mix::busFaderId (0)), 10.0f);
            setParam (s.param (mix::chainId (0, "lim_on")), 1.0f);
            setParam (s.param (mix::chainId (0, "lim_ceiling")), -12.0f);
            setParam (s.param (mix::busId (0, "panmode")), (float) mix::PanMode::mirror);
            setParam (s.param (mix::chainId (mix::kMainChain, "eq_on")), 1.0f);
            setParam (s.param (mix::chainId (mix::kMonoChain, "lpf_on")), 1.0f);
            a.prepareToPlay (48000.0, 128);
            const auto b = run (a, 20);
            expectLessOrEqual (juce::jmax (b.getMagnitude (2, 0, 128), b.getMagnitude (3, 0, 128)),
                               std::pow (10.0f, -12.0f / 20.0f) + 1.0e-6f, "the bus limiter's ceiling, fader at +10 dB");

            juce::MemoryBlock state;
            a.getStateInformation (state);
            RKMixProcessor c;
            c.setStateInformation (state.getData(), (int) state.getSize());
            const auto bus = c.chainSettings (0);
            expect (bus.limit.on);
            expectWithinAbsoluteError (bus.limit.ceilingDb, -12.0f, 0.01f);
            expect (c.chainSettings (mix::kMainChain).eq.on && c.chainSettings (mix::kMonoChain).eq.lpfOn);
            expectEquals (c.getSettings().choice (mix::busId (0, "panmode")), (int) mix::PanMode::mirror);
        }

        beginTest ("MIDI out: an RKTrigger insert's notes leave RKMix; conflicts; note-offs when it leaves play");
        {
            using Strip = InsertRack::Strip;
            RKMixProcessor p;
            expect (p.producesMidi() && p.acceptsMidi());
            p.prepareToPlay (48000.0, 128);
            auto& rack = p.getRack();
            auto* kick = rack.insert ({ Strip::channel, 0, 0 }, "RKTrigger", 1);
            expect (kick != nullptr && rack.sendsMidi ({ Strip::channel, 0, 0 }));
            setParam (*kick->findParam ("t1_threshold"), -40.0f);
            setParam (*kick->findParam ("t1_length"), 500.0f);       // still sounding when it is removed

            // A hit on input 1: a note-on, channel 10, C1 (36), at its position.
            juce::AudioBuffer<float> b (32, 128);
            juce::MidiBuffer midi;
            int noteOnAt = -1;
            for (int n = 0; n < 4; ++n)
            {
                b.clear();
                midi.clear();
                if (n == 1)
                    for (int i = 20; i < 60; ++i)
                        b.setSample (0, i, 0.8f * (float) std::sin (i * 0.3));
                p.processBlock (b, midi);
                for (const auto m : midi)
                    if (m.getMessage().isNoteOn())
                    {
                        expectEquals (m.getMessage().getChannel(), 10);
                        expectEquals (m.getMessage().getNoteNumber(), 36);
                        noteOnAt = n * 128 + m.samplePosition;
                    }
            }
            expect (noteOnAt >= 128 + 20 && noteOnAt < 256, "the note-on comes with the hit");

            // A second trigger on the same note conflicts; on another note it doesn't.
            auto* snare = rack.insert ({ Strip::channel, 1, 0 }, "RKTrigger", 1);
            expectEquals ((int) rack.getConflicts().size(), 1, "both on Ch 10, C1 (36)");
            setParam (*snare->findParam ("t1_note"), 38.0f);
            rack.flushPendingUpdate();
            expect (rack.getConflicts().empty(), "C1 and D1 on Ch 10: fine");
            auto* follow = rack.insert ({ Strip::channel, 2, 0 }, "RKFollow", 1);
            setParam (*follow->findParam ("channel"), 10.0f);
            rack.flushPendingUpdate();
            expectEquals ((int) rack.getConflicts().size(), 2, "RKFollow on Ch 10 clashes with both triggers");

            // Removing the sounding trigger sends its note-off.
            rack.remove ({ Strip::channel, 0, 0 });
            b.clear();
            midi.clear();
            p.processBlock (b, midi);
            bool noteOff = false;
            for (const auto m : midi)
                noteOff = noteOff || (m.getMessage().isNoteOff() && m.getMessage().getNoteNumber() == 36 && m.getMessage().getChannel() == 10);
            expect (noteOff, "no hung note");
        }

        beginTest ("MIDI thru: MIDI in reaches the output only when on");
        {
            RKMixProcessor p;
            p.prepareToPlay (48000.0, 128);
            juce::AudioBuffer<float> b (32, 128);
            juce::MidiBuffer midi;
            b.clear();
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 5);
            p.processBlock (b, midi);
            expect (midi.isEmpty(), "off: nothing passes");

            setParam (p.getSettings().param ("midi_thru"), 1.0f);
            midi.clear();
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 5);
            p.processBlock (b, midi);
            expectEquals (midi.getNumEvents(), 1, "on: it passes");
        }

        beginTest ("Solo is never saved; talkback and the oscillator are never on when a mix loads");
        {
            RKMixProcessor a;
            a.setChannelSolo (3, true);
            a.setBusSolo (2, true);
            setParam (*a.findParam ("talk"), 1.0f);
            setParam (a.getSettings().param ("osc_on"), 1.0f);
            setParam (a.getSettings().param ("talk_src"), 4.0f);
            expect (a.isAnythingSoloed());
            juce::MemoryBlock state;
            a.getStateInformation (state);

            RKMixProcessor b;
            b.setChannelSolo (0, true);
            b.setStateInformation (state.getData(), (int) state.getSize());
            expect (! b.isAnythingSoloed(), "solo cleared on load");
            expect (rkfx::ProcessorBase::valueOf (b.findParam ("talk")) < 0.5f, "Talk off");
            expect (! b.getSettings().flag ("osc_on"), "the oscillator off");
            expectEquals (b.getSettings().choice ("talk_src"), 4, "its setup is kept");
        }

        beginTest ("Solo through the processor: a channel to the cue output (PFL), the main untouched");
        {
            RKMixProcessor p;
            setParam (p.getSettings().param ("cue_out"), (float) mix::pairTarget (6));   // Out 7/8
            setParam (p.getSettings().param ("cue_src"), 0.0f);                          // silence when nothing is soloed
            p.prepareToPlay (48000.0, 128);
            expectEquals (run (p, 4).getMagnitude (6, 0, 128), 0.0f, "nothing soloed: silence");
            p.setChannelSolo (0, true);
            const auto b = run (p, 8);   // past the 10 ms fade
            expectWithinAbsoluteError (b.getSample (6, 64), 0.5f, 1.0e-4f, "PFL of channel 1");
            expectWithinAbsoluteError (b.getSample (0, 64), 0.5f * std::sqrt (0.5f), 1.0e-4f, "the main as before");
        }

        beginTest ("Scenes: values recall, structure stays, recall-safe strips keep theirs; edits go into the active scene");
        {
            using Strip = InsertRack::Strip;
            RKMixProcessor p;
            auto& s = p.getSettings();
            setParam (s.param ("scene_glide"), 0.0f);
            auto& fader1 = *p.findParam (mix::channelFaderId (0));
            auto& fader2 = *p.findParam (mix::channelFaderId (1));
            auto* chorus = p.getRack().insert ({ Strip::channel, 0, 0 }, "RKChorus", 1);

            // Scene 1
            setParam (fader1, -6.0f);
            setParam (s.param (mix::channelId (0, "pan")), -40.0f);
            p.getScenes().setSceneControlled (InsertRack::flatIndex ({ Strip::channel, 0, 0 }), "depth", true);
            setParam (*chorus->findParam ("depth"), 20.0f);

            // Scene 2: starts as scene 1; then changed
            p.recallScene (1);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (&fader1), -6.0f, 0.01f, "a new scene is the mix as it was");
            setParam (fader1, -20.0f);
            setParam (fader2, -10.0f);
            setParam (s.param (mix::channelId (0, "pan")), 30.0f);
            setParam (*chorus->findParam ("depth"), 80.0f);
            p.getRack().setOn ({ Strip::channel, 0, 0 }, false);
            setParam (s.param (mix::linkId (5)), 1.0f);                 // structure
            setParam (s.param (mix::channelId (1, "recallsafe")), 1.0f);

            p.recallScene (0);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (&fader1), -6.0f, 0.01f, "scene 1's fader");
            expectWithinAbsoluteError (s.value (mix::channelId (0, "pan")), -40.0f, 0.01f, "scene 1's pan");
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (chorus->findParam ("depth")), 20.0f, 0.01f, "a scene-controlled insert knob");
            expect (p.getRack().isOn ({ Strip::channel, 0, 0 }), "the insert on again");
            expect (s.flag (mix::linkId (5)), "a link is structure: it stays");
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (&fader2), -10.0f, 0.01f, "a recall-safe channel keeps its fader");
            expectEquals (juce::roundToInt (rkfx::ProcessorBase::valueOf (p.findParam ("scene"))), 1, "the Scene parameter follows");

            p.recallScene (1);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (&fader1), -20.0f, 0.01f, "scene 2 kept its edits");

            // The session keeps the scenes and the current one.
            juce::MemoryBlock state;
            p.getStateInformation (state);
            RKMixProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            setParam (q.getSettings().param ("scene_glide"), 0.0f);
            expectEquals (q.getScenes().getCurrent(), 1);
            q.recallScene (0);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (q.findParam (mix::channelFaderId (0))), -6.0f, 0.01f, "scene 1 after reload");

            // The host's Scene parameter recalls.
            setParam (*q.findParam ("scene"), 2.0f);
            expectEquals (q.getScenes().getCurrent(), 1);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (q.findParam (mix::channelFaderId (0))), -20.0f, 0.01f, "via the Scene parameter");
        }

        beginTest ("Assignables: an Assign moves its control within its range, and the control moves the Assign");
        {
            using Kind = MixAssigns::Target::Kind;
            RKMixProcessor p;
            auto& assigns = p.getAssigns();
            auto& fader = *p.findParam (mix::channelFaderId (2));
            auto& assign1 = *p.findParam (mix::assignId (0));
            assigns.assign (0, Kind::mix, mix::channelFaderId (2));
            assign1.setValueNotifyingHost (0.0f);
            expectEquals (rkfx::ProcessorBase::valueOf (&fader), mix::kFaderMinDb, "Assign at 0: the fader at the bottom");
            fader.setValueNotifyingHost (fader.convertTo0to1 (0.0f));
            expectWithinAbsoluteError (assign1.getValue(), fader.getValue(), 1.0e-6f, "the fader moves the Assign");

            assigns.setRange (0, 0.5f, 0.75f, true);
            assign1.setValueNotifyingHost (1.0f);
            expectWithinAbsoluteError (fader.getValue(), 0.5f, 1.0e-6f, "inverted: the top of the Assign is the minimum");

            // A mix setting, and a switch.
            auto& pan = p.getSettings().param (mix::channelId (2, "pan"));
            assigns.assign (1, Kind::mix, mix::channelId (2, "pan"));
            p.findParam (mix::assignId (1))->setValueNotifyingHost (1.0f);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (&pan), 100.0f, 0.01f);
            assigns.assign (2, Kind::mix, mix::channelMuteId (2));
            p.findParam (mix::assignId (2))->setValueNotifyingHost (0.7f);
            expect (rkfx::ProcessorBase::valueOf (p.findParam (mix::channelMuteId (2))) >= 0.5f, "a switch: on at 50% and above");

            // An insert's knob; removing the insert ends the assignment.
            auto* delay = p.getRack().insert ({ InsertRack::Strip::bus, 0, 0 }, "RKDelay", 2);
            const auto slot = InsertRack::flatIndex ({ InsertRack::Strip::bus, 0, 0 });
            assigns.assign (3, Kind::insert, "mix", slot);
            p.findParam (mix::assignId (3))->setValueNotifyingHost (0.0f);
            expectWithinAbsoluteError (delay->findParam ("mix")->getValue(), 0.0f, 1.0e-6f, "an insert's knob");
            expect (assigns.describe (3).contains ("Bus 1, insert 1"), assigns.describe (3));

            juce::MemoryBlock state;
            p.getStateInformation (state);
            RKMixProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expect (q.getAssigns().get (3).kind == Kind::insert && q.getAssigns().get (0).invert, "kept in the session");

            p.getRack().remove ({ InsertRack::Strip::bus, 0, 0 });
            expect (assigns.get (3).kind == Kind::none, "gone with its insert");
        }

        beginTest ("Standalone: program changes recall scenes; learned controllers drive Assigns and Talk; the tempo is kept");
        {
            const auto rig = juce::File::createTempFile ("rkmix-rig");
            rig.createDirectory();
            RKMixProcessor p;
            p.setStandaloneForTesting (rig);
            p.prepareToPlay (48000.0, 128);
            juce::AudioBuffer<float> b (32, 128);

            const auto send = [&] (const juce::MidiMessage& m, int blocks = 1)
            {
                for (int n = 0; n < blocks; ++n)
                {
                    juce::MidiBuffer midi;
                    if (n == 0)
                        midi.addEvent (m, 0);
                    b.clear();
                    p.processBlock (b, midi);
                }
            };

            send (juce::MidiMessage::programChange (1, 2));
            expectEquals (p.getScenes().getCurrent(), 2, "program change 2: scene 3");

            p.getMidiMap()->set ({ MixLearn::firstAssign + 4, 1, 11, false });
            send (juce::MidiMessage::controllerEvent (1, 11, 127), 80);
            expectWithinAbsoluteError (p.findParam (mix::assignId (4))->getValue(), 1.0f, 1.0e-3f, "CC 11 drives Assign 5 (glided)");

            p.getMidiMap()->set ({ MixLearn::talk, 1, 64, true });
            send (juce::MidiMessage::controllerEvent (1, 64, 127));
            expect (rkfx::ProcessorBase::valueOf (p.findParam ("talk")) >= 0.5f, "Talk while held");
            send (juce::MidiMessage::controllerEvent (1, 64, 0));
            expect (rkfx::ProcessorBase::valueOf (p.findParam ("talk")) < 0.5f, "...and off when let go");

            // The mappings are the rig's: another instance reads them.
            RKMixProcessor q;
            q.setStandaloneForTesting (rig);
            expect (q.getMidiMap()->find (MixLearn::firstAssign + 4).has_value());

            p.setTempo (97.0);
            juce::MemoryBlock state;
            p.getStateInformation (state);
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (q.getTempo(), 97.0, 1.0e-6, "the tempo is kept with the mix");
            rig.deleteRecursively();
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
