#include <juce_audio_processors/juce_audio_processors.h>

#include "../plugins/RKMix/PluginProcessor.h"
#include "../plugins/RKMix/MixQr.h"
#include "../plugins/RKMix/MixOsc.h"

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

        beginTest ("Factory mixes all load cleanly, with what their manual promises");
        {
            RKMixProcessor p;
            const auto dir = juce::File (RKFX_SOURCE_DIR).getChildFile ("plugins/RKMix/presets");
            expectEquals ((int) p.getFactoryPresets().size(), dir.findChildFiles (juce::File::findFiles, false, "*.json").size());
            expect (p.getFactoryPresets().size() >= 4);
            for (int i = 0; i < (int) p.getFactoryPresets().size(); ++i)
            {
                const auto& preset = p.getFactoryPresets()[(size_t) i];
                const auto r = p.loadFactoryPreset (i);
                expect (r.ok && r.message.isEmpty(), preset.name + ": " + r.message);
                expectEquals (p.getPresetName(), preset.name);
                expect (! p.isPresetModified(), preset.name + " unmodified");
                if (preset.name == "Band with In-Ears")
                {
                    expect (p.isLinked (2) && p.getBusType (0) == mix::BusType::group && p.getBusType (1) == mix::BusType::fx);
                    expectEquals (p.getSettings().choice (mix::busId (2, "out")), mix::pairTarget (2), "IEM Vocals on Out 3/4");
                    expect (p.getRack().get ({ InsertRack::Strip::bus, 1, 0 }) != nullptr, "a reverb on the FX bus");
                    expectEquals (p.getUserPage (0).size(), 6);
                    expectEquals (p.getSettings().choice (mix::busId (2, "panmode")), (int) mix::PanMode::own, "IEM Vocals: own pans");
                    expectWithinAbsoluteError (p.getSettings().value (mix::channelSendPanId (2, 2)), -40.0f, 0.01f, "guitar left");
                    expectEquals (p.getDcaName (0), juce::String ("Vocals"));
                    expectEquals (p.getDcaMembers (1).size(), 4, "DCA Band: guitar, bass, keys, the Drums group");
                    expectEquals (p.getMuteGroupName (0), juce::String ("Band"));
                    expectEquals (p.getMuteGroupMembers (0).size(), 6, "Mute Band: every instrument channel");
                }
                if (preset.name == "Recording Feed")
                    expectEquals (p.getSettings().choice (mix::channelId (15, "direct_out")), 32);
            }
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
            setParam (s.param (mix::busId (0, "out")), (float) mix::pairTarget (4));   // Out 5/6     // Out 3/4
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

        beginTest ("Own send pans: kept in the session; a mix saved with Centre opens as Own");
        {
            RKMixProcessor a;
            auto& s = a.getSettings();
            setParam (s.param (mix::busId (3, "panmode")), (float) mix::PanMode::own);
            setParam (s.param (mix::channelSendPanId (5, 3)), -40.0f);
            juce::MemoryBlock state;
            a.getStateInformation (state);
            RKMixProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (b.getSettings().choice (mix::busId (3, "panmode")), (int) mix::PanMode::own);
            expectWithinAbsoluteError (b.getSettings().value (mix::channelSendPanId (5, 3)), -40.0f, 0.01f);

            auto* old = new juce::DynamicObject();
            old->setProperty ("bus2_panmode", "centre");
            old->setProperty ("bus3_panmode", "mirror");
            RKMixProcessor c;
            const auto notes = c.getSettings().fromVar (juce::var (old));
            expect (notes.isEmpty(), notes.joinIntoString ("; "));
            expectEquals (c.getSettings().choice (mix::busId (1, "panmode")), (int) mix::PanMode::own);
            expectEquals (c.getSettings().choice (mix::busId (2, "panmode")), (int) mix::PanMode::mirror);
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

        beginTest ("Recall scope: every scene value has a place; ids land where they belong");
        {
            using Scope = MixScenes::Scope;
            RKMixProcessor p;
            juce::StringArray ids;
            for (auto* param : p.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (param))
                    ids.add (r->getParameterID());
            for (auto* param : p.getSettings().getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (param))
                    ids.add (r->getParameterID());

            // With nothing in scope, no scene value is recalled; with everything, all are.
            Scope none;
            none.categories = none.sendBuses = 0;
            for (const auto& id : ids)
                if (MixScenes::isSceneValue (id))
                {
                    expect (! MixScenes::isInScope (none, id), id + " outside an empty scope");
                    expect (MixScenes::isInScope (Scope(), id), id + " inside the full scope");
                }

            const auto only = [] (Scope::Category c) { Scope s; s.categories = 1u << c; s.sendBuses = 0; return s; };
            Scope bus1;
            bus1.categories = 0;
            bus1.sendBuses = 1u;
            expect (MixScenes::isInScope (bus1, mix::channelSendId (4, 0)), "a channel's send to bus 1");
            expect (MixScenes::isInScope (bus1, mix::channelSendPanId (4, 0)), "its Own pan");
            for (const auto* field : mix::sendEqFields)
            {
                expect (MixScenes::isSceneValue (mix::channelSendEqId (4, 0, field)), juce::String ("send EQ ") + field + " is in the scenes");
                expect (MixScenes::isInScope (bus1, mix::channelSendEqId (4, 0, field)), juce::String ("its send EQ: ") + field);
                expect (! MixScenes::isInScope (bus1, mix::channelSendEqId (4, 1, field)), juce::String ("not the send EQ to bus 2: ") + field);
                expect (! MixScenes::isInScope (only (Scope::channelProcessing), mix::channelSendEqId (4, 0, field)), "not channel processing");
            }
            expect (MixScenes::isInScope (bus1, mix::busSendId (3, 0)), "a bus's send to bus 1");
            expect (! MixScenes::isInScope (bus1, mix::channelSendId (4, 1)), "not the sends to bus 2");
            expect (! MixScenes::isInScope (bus1, mix::busSendId (0, 1)), "nor bus 1's own sends");
            expect (! MixScenes::isInScope (bus1, mix::channelFaderId (4)));
            expect (MixScenes::isInScope (only (Scope::faders), mix::busMuteId (2)));
            expect (MixScenes::isInScope (only (Scope::faders), "main_fader"));
            expect (MixScenes::isInScope (only (Scope::pans), mix::channelId (3, "pan")));
            expect (MixScenes::isInScope (only (Scope::pans), mix::busId (3, "pan")));
            expect (MixScenes::isInScope (only (Scope::pans), "main_bal"));
            expect (MixScenes::isInScope (only (Scope::channelProcessing), mix::channelId (3, "eq_on")));
            expect (MixScenes::isInScope (only (Scope::channelProcessing), mix::channelId (3, "trim")));
            expect (MixScenes::isInScope (only (Scope::busProcessing), mix::chainId (2, "lim_on")));
            expect (MixScenes::isInScope (only (Scope::busProcessing), mix::chainId (mix::kMainChain, "comp_on")));
            expect (MixScenes::isInScope (only (Scope::routing), mix::channelAssignId (3, 2)));
            expect (MixScenes::isInScope (only (Scope::routing), mix::channelId (3, "main")));
            expect (MixScenes::isInScope (only (Scope::routing), mix::busId (3, "main")));
            expect (MixScenes::isInScope (only (Scope::faders), mix::dcaFaderId (2)), "a DCA fader");
            expect (MixScenes::isInScope (only (Scope::faders), mix::dcaMuteId (2)), "a DCA mute");
            expect (MixScenes::isInScope (only (Scope::faders), mix::muteGroupId (7)), "a mute group");
            expect (MixScenes::isInScope (only (Scope::routing), mix::channelDcaId (3, 2)), "DCA membership is an assign");
            expect (MixScenes::isInScope (only (Scope::routing), mix::busMuteGroupId (3, 2)), "mute group membership too");
        }

        beginTest ("Recall scope: a scene that recalls only the sends to bus 1 leaves the rest; kept in the session; copied");
        {
            using Strip = InsertRack::Strip;
            using Scope = MixScenes::Scope;
            RKMixProcessor p;
            auto& s = p.getSettings();
            setParam (s.param ("scene_glide"), 0.0f);
            auto& fader = *p.findParam (mix::channelFaderId (0));
            p.getRack().insert ({ Strip::channel, 0, 0 }, "RKChorus", 1);

            // Scene 1: everything. Scene 2: only the sends to bus 1.
            setParam (fader, -6.0f);
            setParam (s.param (mix::channelSendId (0, 0)), -20.0f);
            setParam (s.param (mix::channelSendId (0, 1)), -20.0f);
            Scope sends1;
            sends1.categories = 0;
            sends1.sendBuses = 1u;
            p.getScenes().setScope (1, sends1);
            p.recallScene (1);
            setParam (s.param (mix::channelSendId (0, 0)), 0.0f);
            setParam (s.param (mix::channelSendPanId (0, 0)), 50.0f);

            // Back in scene 1 (everything): change what scene 2 must not touch.
            p.recallScene (0);
            expectWithinAbsoluteError (s.value (mix::channelSendId (0, 0)), -20.0f, 0.01f);
            setParam (fader, -30.0f);
            setParam (s.param (mix::channelSendId (0, 1)), -3.0f);
            setParam (s.param (mix::channelId (0, "pan")), 25.0f);
            p.getRack().setOn ({ Strip::channel, 0, 0 }, false);

            p.recallScene (1);
            expectWithinAbsoluteError (s.value (mix::channelSendId (0, 0)), 0.0f, 0.01f, "the send to bus 1 recalls");
            expectWithinAbsoluteError (s.value (mix::channelSendPanId (0, 0)), 50.0f, 0.01f, "with its pan");
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (&fader), -30.0f, 0.01f, "the fader stays");
            expectWithinAbsoluteError (s.value (mix::channelSendId (0, 1)), -3.0f, 0.01f, "the send to bus 2 stays");
            expectWithinAbsoluteError (s.value (mix::channelId (0, "pan")), 25.0f, 0.01f, "the pan stays");
            expect (! p.getRack().isOn ({ Strip::channel, 0, 0 }), "inserts stay");

            // Kept in the session (scene 2's scope, scene 1's everything).
            juce::MemoryBlock state;
            p.getStateInformation (state);
            RKMixProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expect (q.getScenes().getScope (1) == sends1);
            expect (q.getScenes().getScope (0).isEverything());

            // A scope set before the scene is first stored stays when it is.
            Scope pans;
            pans.categories = 1u << Scope::pans;
            pans.sendBuses = 0;
            q.getScenes().setScope (5, pans);
            q.recallScene (5);
            q.recallScene (0);
            expect (q.getScenes().getScope (5) == pans);

            // Copying a scene copies its scope.
            q.getScenes().copy (1, 7);
            expect (q.getScenes().getScope (7) == sends1);
            expect (q.getScenes().getScope (1).describe ([] (int bus) { return "Bus " + mix::num (bus); }) == "Sends to Bus 1");
        }

        beginTest ("Send EQ: reaches the audio; the session and presets keep it (only what's set); scenes, scoped with Sends to B#; undo");
        {
            RKMixProcessor p;
            auto& s = p.getSettings();
            const auto eqId = [] (int ch, int bus, const char* field) { return mix::channelSendEqId (ch, bus, field); };

            // A mix without send EQs doesn't grow.
            const auto before = juce::JSON::toString (s.toVar()).length();
            setParam (s.param (eqId (0, 0, "on")), 1.0f);
            setParam (s.param (eqId (0, 0, "lc")), 400.0f);
            setParam (s.param (eqId (0, 0, "mg")), -4.0f);
            setParam (s.param (eqId (0, 0, "mq")), 2.0f);
            setParam (s.param (eqId (0, 0, "hsf")), 5000.0f);
            const auto saved = s.toVar();
            expectEquals (saved.getDynamicObject()->getProperties().size(), 5, "only the five values set");
            expect (juce::JSON::toString (saved).length() > before);

            // The fields reach the engine's settings in their order.
            const auto eq = p.getSendEq (0, 0);
            expect (eq.on);
            expectWithinAbsoluteError (eq.lowCutHz, 400.0f, 0.5f);
            expectWithinAbsoluteError (eq.midDb, -4.0f, 0.01f);
            expectWithinAbsoluteError (eq.midQ, 2.0f, 0.01f);
            expectWithinAbsoluteError (eq.highHz, 5000.0f, 1.0f);
            expectWithinAbsoluteError (eq.lowHz, 150.0f, 0.5f);
            expect (p.hasSendEq (0));
            setParam (s.param (mix::busId (2, "type")), 0.0f);               // a group: no send EQ
            expect (! p.hasSendEq (2));

            // The audio: input 1 (a steady 0.5) through the low cut to Aux 1 on Out 5 dies away; Aux 2 keeps it.
            setParam (s.param (mix::busId (0, "format")), 0.0f);
            setParam (s.param (mix::busId (0, "out")), 5.0f);
            setParam (s.param (mix::busId (1, "format")), 0.0f);
            setParam (s.param (mix::busId (1, "out")), 6.0f);
            setParam (s.param (mix::channelSendId (0, 0)), 0.0f);
            setParam (s.param (mix::channelSendId (0, 1)), 0.0f);
            p.prepareToPlay (48000.0, 128);
            auto b = run (p, 60);
            expectLessThan (std::abs (b.getSample (4, 64)), 1.0e-3f, "the steady signal is cut in Aux 1");
            expectWithinAbsoluteError (b.getSample (5, 64), 0.5f, 1.0e-4f, "Aux 2 untouched");

            // Kept in the session.
            juce::MemoryBlock state;
            p.getStateInformation (state);
            RKMixProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expect (q.getSettings().flag (eqId (0, 0, "on")));
            expectWithinAbsoluteError (q.getSettings().value (eqId (0, 0, "mg")), -4.0f, 0.01f);

            // Scenes: scene 2 recalls only the sends to bus 1, with their EQ.
            using Scope = MixScenes::Scope;
            setParam (s.param ("scene_glide"), 0.0f);
            Scope sends1;
            sends1.categories = 0;
            sends1.sendBuses = 1u;
            p.getScenes().setScope (1, sends1);
            p.recallScene (1);
            setParam (s.param (eqId (0, 0, "mg")), 6.0f);
            setParam (s.param (eqId (0, 1, "on")), 1.0f);
            p.recallScene (0);
            expectWithinAbsoluteError (s.value (eqId (0, 0, "mg")), -4.0f, 0.01f, "scene 1 has its own");
            expect (! s.flag (eqId (0, 1, "on")), "scene 1: the send EQ to bus 2 off");
            setParam (s.param (eqId (0, 1, "hsg")), 3.0f);
            p.recallScene (1);
            expectWithinAbsoluteError (s.value (eqId (0, 0, "mg")), 6.0f, 0.01f, "the send EQ to bus 1 recalls");
            expectWithinAbsoluteError (s.value (eqId (0, 1, "hsg")), 3.0f, 0.01f, "the send EQ to bus 2 stays (out of scope)");

            // Undo.
            auto& undo = p.getUndo();
            undo.poll();
            undo.closeStep();
            setParam (s.param (eqId (3, 4, "lsg")), -9.0f);
            undo.poll();
            undo.closeStep();
            expectEquals (undo.undoDescription(), juce::String ("Ch 4 Send 5 EQ Low Gain"));
            undo.undo();
            expectWithinAbsoluteError (s.value (eqId (3, 4, "lsg")), 0.0f, 0.01f);
        }

        beginTest ("Send EQ copy and paste: one send, every send to a bus (a linked pair once)");
        {
            RKMixProcessor p;
            auto& s = p.getSettings();
            setParam (s.param (mix::channelSendEqId (0, 2, "on")), 1.0f);
            setParam (s.param (mix::channelSendEqId (0, 2, "lc")), 200.0f);
            setParam (s.param (mix::channelSendEqId (0, 2, "hsg")), -6.0f);
            p.setChannelName (0, "Kick");
            const auto copy = p.copySendEq (0, 2);
            expect (RKMixProcessor::isSendEqCopy (copy));
            expect (! RKMixProcessor::isChannelCopy (copy));
            expectEquals (RKMixProcessor::describeCopy (copy), juce::String ("Send EQ from Kick"));

            p.pasteSendEq (5, 7, copy);
            expect (s.flag (mix::channelSendEqId (5, 7, "on")));
            expectWithinAbsoluteError (s.value (mix::channelSendEqId (5, 7, "lc")), 200.0f, 0.5f);
            expectWithinAbsoluteError (s.value (mix::channelSendEqId (5, 7, "hsg")), -6.0f, 0.01f);
            expect (! s.flag (mix::channelSendEqId (5, 6, "on")), "only that bus");

            setParam (s.param (mix::linkId (1)), 1.0f);                       // channels 3 and 4 linked
            p.pasteSendEqToBus (9, copy);
            int on = 0;
            for (int ch = 0; ch < mix::kChannels; ++ch)
                on += s.flag (mix::channelSendEqId (ch, 9, "on")) ? 1 : 0;
            expectEquals (on, 31, "every channel, the pair's follower left alone");
            expect (! s.flag (mix::channelSendEqId (3, 9, "on")));
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

        beginTest ("DCAs: a member's fader moves by the DCA's level; DCA mutes and mute groups silence their members");
        {
            RKMixProcessor p;
            p.prepareToPlay (48000.0, 128);
            const auto centred = 0.5f * std::sqrt (0.5f);
            auto& s = p.getSettings();
            setParam (s.param (mix::channelDcaId (0, 2)), 1.0f);
            setParam (*p.findParam (mix::dcaFaderId (2)), -6.0f);
            expectWithinAbsoluteError (run (p, 20).getSample (0, 64), centred * mix::faderGain (-6.0f), 1.0e-5f, "DCA 3 at -6 dB");

            setParam (*p.findParam (mix::channelFaderId (0)), -4.0f);
            expectWithinAbsoluteError (run (p, 20).getSample (0, 64), centred * mix::faderGain (-10.0f), 1.0e-5f,
                                       "the channel's fader and the DCA add up");
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (p.findParam (mix::channelFaderId (0))), -4.0f, 0.01f,
                                       "the channel's own fader stays where it is");

            setParam (*p.findParam (mix::dcaFaderId (2)), mix::kFaderMinDb);
            expectEquals (run (p, 20).getMagnitude (0, 0, 128), 0.0f, "a DCA at the bottom: silence");

            setParam (*p.findParam (mix::dcaFaderId (2)), 10.0f);
            setParam (*p.findParam (mix::channelFaderId (0)), 8.0f);
            expectWithinAbsoluteError (run (p, 20).getSample (0, 64), centred * mix::faderGain (10.0f), 1.0e-4f,
                                       "never past the top of a fader");

            setParam (*p.findParam (mix::dcaFaderId (2)), 0.0f);
            setParam (*p.findParam (mix::channelFaderId (0)), 0.0f);
            setParam (*p.findParam (mix::dcaMuteId (2)), 1.0f);
            expectEquals (run (p, 20).getMagnitude (0, 0, 128), 0.0f, "the DCA's mute");
            expect (p.isChannelGroupMuted (0));
            setParam (*p.findParam (mix::dcaMuteId (2)), 0.0f);
            expect (! p.isChannelGroupMuted (0));

            setParam (s.param (mix::channelDcaId (0, 2)), 0.0f);
            setParam (*p.findParam (mix::dcaFaderId (2)), -20.0f);
            expectWithinAbsoluteError (run (p, 20).getSample (0, 64), centred, 1.0e-5f, "not a member: untouched");

            // Mute groups: a channel's pre-fader send to an Aux bus goes silent too.
            setParam (s.param (mix::busId (0, "out")), (float) mix::pairTarget (4));   // Out 5/6
            setParam (s.param (mix::channelSendId (0, 0)), 0.0f);
            setParam (s.param (mix::channelMuteGroupId (0, 4)), 1.0f);
            auto b = run (p, 20);
            expect (b.getMagnitude (4, 0, 128) > 0.1f, "the in-ear mix before the mute group");
            setParam (*p.findParam (mix::muteGroupId (4)), 1.0f);
            b = run (p, 20);
            expectEquals (b.getMagnitude (0, 0, 128), 0.0f, "mute group 5: the main");
            expectEquals (b.getMagnitude (4, 0, 128), 0.0f, "and the pre-fader send");

            // A bus in a mute group.
            setParam (s.param (mix::channelMuteGroupId (0, 4)), 0.0f);
            setParam (s.param (mix::busMuteGroupId (0, 4)), 1.0f);
            b = run (p, 20);
            expect (b.getMagnitude (0, 0, 128) > 0.1f, "the channel back in the main");
            expectEquals (b.getMagnitude (4, 0, 128), 0.0f, "the Aux bus muted by the group");
            expect (p.isBusGroupMuted (0));
            expectEquals (p.getMuteGroupMembers (4).size(), 1);
            expectEquals (p.getMuteGroupMembers (4)[0], RKMixProcessor::kUserPageBus);

            // Names, colours and membership are kept with the mix.
            p.setDcaName (2, "Drums");
            p.setDcaColour (2, 5);
            p.setMuteGroupName (4, "Band");
            setParam (s.param (mix::channelDcaId (6, 2)), 1.0f);
            juce::MemoryBlock state;
            p.getStateInformation (state);
            RKMixProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (q.getDcaName (2), juce::String ("Drums"));
            expectEquals (q.getDcaColour (2), 5);
            expectEquals (q.getMuteGroupName (4), juce::String ("Band"));
            expectEquals (q.getDcaMembers (2).size(), 1);
            expectEquals (q.getDcaMembers (2)[0], 6);
            expect (q.isBusGroupMuted (0), "the mute group's state");
        }

        beginTest ("Scenes recall the DCA faders, mutes and mute groups");
        {
            RKMixProcessor p;
            setParam (p.getSettings().param ("scene_glide"), 0.0f);
            setParam (*p.findParam (mix::dcaFaderId (0)), -12.0f);
            p.recallScene (1);
            setParam (*p.findParam (mix::dcaFaderId (0)), 3.0f);
            setParam (*p.findParam (mix::muteGroupId (1)), 1.0f);
            p.recallScene (0);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (p.findParam (mix::dcaFaderId (0))), -12.0f, 0.01f);
            expect (rkfx::ProcessorBase::valueOf (p.findParam (mix::muteGroupId (1))) < 0.5f);
            p.recallScene (1);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (p.findParam (mix::dcaFaderId (0))), 3.0f, 0.01f);
        }

        beginTest ("Undo: a step at a time, back and forward; a new change drops the redo; a load clears the history");
        {
            RKMixProcessor p;
            auto& undo = p.getUndo();
            auto& fader = *p.findParam (mix::channelFaderId (2));
            auto& trim = p.getSettings().param (mix::channelId (2, "trim"));
            const auto value = [] (juce::RangedAudioParameter& r) { return rkfx::ProcessorBase::valueOf (&r); };
            expect (! undo.canUndo());

            // A drag: several values, one step.
            setParam (fader, -3.0f); undo.poll();
            setParam (fader, -8.0f); undo.poll();
            undo.closeStep();
            setParam (trim, 6.0f); undo.poll();
            undo.closeStep();
            expect (undo.canUndo());
            expectEquals (undo.undoDescription(), juce::String ("Ch 3 Trim"));

            undo.undo();
            expectWithinAbsoluteError (value (trim), 0.0f, 0.01f, "the trim back");
            expectWithinAbsoluteError (value (fader), -8.0f, 0.01f, "the fader not yet");
            undo.undo();
            expectWithinAbsoluteError (value (fader), 0.0f, 0.01f, "the whole drag back to where it started");
            expect (! undo.canUndo() && undo.canRedo());

            undo.redo();
            expectWithinAbsoluteError (value (fader), -8.0f, 0.01f, "redo");
            undo.poll();
            expect (undo.canRedo(), "an undo or redo is not itself a change");

            setParam (fader, -1.0f); undo.poll();
            expect (! undo.canRedo(), "a new change drops what was undone");
            undo.undo();
            expectWithinAbsoluteError (value (fader), -8.0f, 0.01f);

            // A load clears it.
            juce::MemoryBlock state;
            p.getStateInformation (state);
            p.setStateInformation (state.getData(), (int) state.getSize());
            expect (! undo.canUndo() && ! undo.canRedo());
        }

        beginTest ("Undo a scene recall: the values and the scene before it; neither scene changes");
        {
            RKMixProcessor p;
            auto& undo = p.getUndo();
            setParam (p.getSettings().param ("scene_glide"), 0.0f);
            auto& fader = *p.findParam (mix::channelFaderId (0));
            const auto value = [] (juce::RangedAudioParameter& r) { return rkfx::ProcessorBase::valueOf (&r); };

            setParam (fader, -10.0f);
            p.recallScene (1);              // scene 2 starts as the mix is
            setParam (fader, -20.0f);       // scene 2's own fader
            p.recallScene (0);              // back to scene 1: -10
            undo.poll();
            undo.closeStep();
            p.recallScene (1);              // scene 2: -20
            undo.poll();
            undo.closeStep();
            expectWithinAbsoluteError (value (fader), -20.0f, 0.01f);
            expectEquals (undo.undoDescription(), juce::String ("Scene 2 recall"));

            undo.undo();
            expectEquals (p.getScenes().getCurrent(), 0, "scene 1 current again");
            expectEquals (juce::roundToInt (value (*p.findParam ("scene"))), 1, "the Scene parameter too");
            expectWithinAbsoluteError (value (fader), -10.0f, 0.01f, "scene 1's fader");

            p.recallScene (1);
            expectWithinAbsoluteError (value (fader), -20.0f, 0.01f, "scene 2 kept its values");
            p.recallScene (0);
            expectWithinAbsoluteError (value (fader), -10.0f, 0.01f, "and scene 1 its own");
        }

        beginTest ("Copy and paste: a section, the whole strip with inserts; a bus's processing; nothing else moves");
        {
            using Section = RKMixProcessor::Section;
            using Strip = InsertRack::Strip;
            RKMixProcessor p;
            auto& s = p.getSettings();
            setParam (s.param (mix::channelId (0, "gate_on")), 1.0f);
            setParam (s.param (mix::channelId (0, "gate_thr")), -33.0f);
            setParam (s.param (mix::channelId (0, "gate_key")), 4.0f);     // routing: not copied
            setParam (s.param (mix::channelId (0, "eq_on")), 1.0f);
            setParam (s.param (mix::channelId (0, "pan")), -50.0f);        // routing: not copied
            p.setChannelName (0, "Kick");
            p.getRack().insert ({ Strip::channel, 0, 1 }, "RKChorus", 1);

            const auto gate = p.copyChannel (0, Section::gate);
            expectEquals (RKMixProcessor::describeCopy (gate), juce::String ("Gate from Kick"));
            expect (RKMixProcessor::isChannelCopy (gate) && ! RKMixProcessor::isChainCopy (gate));
            p.pasteChannel (5, gate);
            expect (s.flag (mix::channelId (5, "gate_on")));
            expectWithinAbsoluteError (s.value (mix::channelId (5, "gate_thr")), -33.0f, 0.01f);
            expectEquals (s.choice (mix::channelId (5, "gate_key")), 0, "the key is routing");
            expect (! s.flag (mix::channelId (5, "eq_on")), "only the gate");
            expect (p.getRack().get ({ Strip::channel, 5, 1 }) == nullptr, "no inserts with a section");

            p.pasteChannel (6, p.copyChannel (0, Section::whole));
            expect (s.flag (mix::channelId (6, "eq_on")) && s.flag (mix::channelId (6, "gate_on")));
            expectWithinAbsoluteError (s.value (mix::channelId (6, "pan")), 0.0f, 0.01f, "not the pan");
            expect (p.getRack().get ({ Strip::channel, 6, 1 }) != nullptr, "the whole strip brings its inserts");
            expect (p.getChannelName (6).isEmpty(), "not the name");

            // A bus's compressor onto the main; a channel copy doesn't paste onto a chain.
            setParam (s.param (mix::chainId (2, "comp_on")), 1.0f);
            setParam (s.param (mix::chainId (2, "comp_ratio")), 6.0f);
            setParam (s.param (mix::chainId (2, "eq_on")), 1.0f);
            p.pasteChain (mix::kMainChain, p.copyChain (2, Section::comp));
            expect (s.flag (mix::chainId (mix::kMainChain, "comp_on")));
            expectWithinAbsoluteError (s.value (mix::chainId (mix::kMainChain, "comp_ratio")), 6.0f, 0.01f);
            expect (! s.flag (mix::chainId (mix::kMainChain, "eq_on")));
            p.pasteChain (3, gate);
            expect (! s.flag (mix::chainId (3, "comp_on")), "a channel's gate is not for a bus");
        }

        beginTest ("Remote: SHA-1 and the WebSocket handshake key; a QR code for a link");
        {
            const juce::String abc ("abc");
            const auto digest = MixWebServer::sha1 (abc.toRawUTF8(), 3);
            expectEquals (juce::String::toHexString (digest.getData(), 20, 0), juce::String ("a9993e364706816aba3e25717850c26c9cd0d89d"));
            const juce::String key ("dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11");   // RFC 6455's example
            const auto accept = MixWebServer::sha1 (key.toRawUTF8(), key.getNumBytesAsUTF8());
            expectEquals (juce::Base64::toBase64 (accept.getData(), accept.getSize()), juce::String ("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));

            MixQr qr ("http://Richards-MacBook-Pro.local:8080/m/3f9a2c71b0aa");
            expect (qr.isValid());
            expectEquals (qr.getSize(), 33, "version 4");
            expect (qr.isDark (0, 0) && qr.isDark (6, 6) && ! qr.isDark (1, 1), "a finder pattern in the corner");
            expect (! MixQr (juce::String::repeatedString ("x", 300)).isValid(), "too long for version 10");
        }

        beginTest ("Remote: a bus's page and manifest; a phone changes only its own bus");
        {
            RKMixProcessor p;
            auto& remote = p.getRemote();
            const auto dir = juce::File::createTempFile ("rkmix-remote");
            remote.setSettingsFileForTesting (dir.getChildFile ("remote.json"));
            auto& s = p.getSettings();
            p.setBusName (2, "IEM <Vocals>");
            p.setChannelName (0, "Lead Vocal");
            setParam (s.param (mix::busId (3, "type")), (float) (int) mix::BusType::fx);

            const auto key = remote.getKey (2);
            expectEquals (key.length(), 12);
            expect (remote.getKey (2) != remote.getKey (3), "one link per bus");
            const auto page = remote.answer ("/m/" + key);
            expectEquals (page.status, 200);
            expect (page.body.toString().contains ("IEM &lt;Vocals&gt;"), "the bus name, escaped");
            expect (page.body.toString().contains ("const KEY = \"" + key + "\""), "the page knows its key");
            expectEquals (remote.answer ("/m/000000000000").status, 404, "a wrong key");
            const auto manifest = juce::JSON::parse (remote.answer ("/manifest/" + key + ".json").body.toString());
            expectEquals (manifest["start_url"].toString(), "/m/" + key);
            expectEquals (manifest["display"].toString(), juce::String ("standalone"));
            expectEquals (remote.answer ("/icon.png").contentType, juce::String ("image/png"));

            const auto mixJson = juce::JSON::parse (remote.mixFor (2));
            expectEquals (mixJson["type"].toString(), juce::String ("mix"));
            expectEquals (mixJson["bus"]["name"].toString(), juce::String ("IEM <Vocals>"));
            const auto* rows = mixJson["rows"].getArray();
            expect (rows != nullptr && rows->size() == 32 + 1, "32 channels and the FX return");
            expect ((bool) (*rows)[0]["used"], "a named channel is in use");
            expect (! (bool) (*rows)[1]["used"], "an unnamed silent one isn't");

            remote.receive (2, R"({"t":"send","k":"ch1","v":-6})", true);
            expectWithinAbsoluteError (s.value (mix::channelSendId (0, 2)), -6.0f, 0.01f, "its send");
            remote.receive (2, R"({"t":"pan","k":"ch1","v":-40})", true);
            expectWithinAbsoluteError (s.value (mix::channelSendPanId (0, 2)), -40.0f, 0.01f, "its send pan");
            remote.receive (2, R"({"t":"send","k":"bus4","v":-10})", true);
            expectWithinAbsoluteError (s.value (mix::busSendId (3, 2)), -10.0f, 0.01f, "the reverb return");
            remote.receive (2, R"({"t":"fader","v":-3})", true);
            remote.receive (2, R"({"t":"mute","v":1})", true);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (p.findParam (mix::busFaderId (2))), -3.0f, 0.01f);
            expect (p.findParam (mix::busMuteId (2))->getValue() > 0.5f);

            // Nothing else: another bus, the channel's own fader, a group bus's page.
            remote.receive (2, R"({"t":"send","k":"ch1","v":-6,"bus":5})", true);
            expectEquals (s.value (mix::channelSendId (0, 4)), mix::kFaderMinDb, "never another bus's send");
            remote.receive (2, R"({"t":"fader","k":"ch1","v":-20})", true);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (p.findParam (mix::channelFaderId (0))), 0.0f, 0.01f);
            remote.receive (2, R"({"t":"set","id":"ch1_trim","v":12})", true);
            expectWithinAbsoluteError (s.value (mix::channelId (0, "trim")), 0.0f, 0.01f);
            remote.receive (3, R"({"t":"fader","v":-30})", true);
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (p.findParam (mix::busFaderId (3))), 0.0f, 0.01f, "an FX bus has no remote");
            dir.deleteRecursively();
        }

        beginTest ("Remote send EQ: in the phone's mix (settings, curve when not flat); a phone sets only its own bus's");
        {
            RKMixProcessor p;
            auto& remote = p.getRemote();
            const auto dir = juce::File::createTempFile ("rkmix-remote");
            remote.setSettingsFileForTesting (dir.getChildFile ("remote.json"));
            auto& s = p.getSettings();
            const auto eqId = [] (int ch, int bus, const char* f) { return mix::channelSendEqId (ch, bus, f); };

            auto mixJson = juce::JSON::parse (remote.mixFor (2));
            auto row = (*mixJson["rows"].getArray())[0];
            expectEquals (row["eq"].size(), mix::kSendEqFields, "nine settings");
            expect (! row.hasProperty ("eqc"), "flat: no curve");

            remote.receive (2, R"({"t":"seq","k":"ch1","f":"on","n":1})", true);
            remote.receive (2, R"({"t":"seq","k":"ch1","f":"mg","n":0.7})", true);
            expect (s.flag (eqId (0, 2, "on")));
            expectWithinAbsoluteError (s.value (eqId (0, 2, "mg")), 6.0f, 0.01f, "0.7 of -15..+15");
            mixJson = juce::JSON::parse (remote.mixFor (2));
            row = (*mixJson["rows"].getArray())[0];
            expectWithinAbsoluteError ((float) (double) row["eq"][0], 1.0f, 1.0e-4f);
            expectEquals (row["eqc"].size(), 61);
            float most = -100.0f;
            for (int i = 0; i < row["eqc"].size(); ++i)
                most = juce::jmax (most, (float) (double) row["eqc"][i]);
            expectWithinAbsoluteError (most, 6.0f, 0.15f, "the curve peaks at the bell's gain");

            // Nothing else: another bus's send EQ, a pair's follower, an unknown field, a bad value.
            expect (! s.flag (eqId (0, 3, "on")), "never another bus's");
            setParam (s.param (mix::linkId (0)), 1.0f);
            remote.receive (2, R"({"t":"seq","k":"ch2","f":"on","n":1})", true);
            expect (! s.flag (eqId (1, 2, "on")), "a linked pair's right channel isn't a row");
            remote.receive (2, R"({"t":"seq","k":"ch3","f":"trim","n":1})", true);
            expectWithinAbsoluteError (s.value (mix::channelId (2, "trim")), 0.0f, 0.01f);
            remote.receive (2, R"({"t":"seq","k":"ch3","f":"lsg","n":5})", true);
            expectWithinAbsoluteError (s.value (eqId (2, 2, "lsg")), 15.0f, 0.01f, "clamped to the range");
            dir.deleteRecursively();
        }

        beginTest ("Names by OSC: Gig Performer's names on every remote page; scenes; the settings");
        {
            RKMixProcessor p;
            auto& remote = p.getRemote();
            const auto dir = juce::File::createTempFile ("rkmix-osc");
            remote.setSettingsFileForTesting (dir.getChildFile ("remote.json"));
            auto& osc = remote.getOsc();
            expect (! osc.isEnabled(), "off at first");
            expectEquals (osc.getPort(), MixOsc::kDefaultPort);
            expectEquals (osc.getStatus(), juce::String ("Off"));

            const auto send = [&osc] (const char* address, auto... args)
            {
                osc.handle (juce::OSCMessage (juce::OSCAddressPattern (address), args...));
            };
            osc.setEnabled (true);
            send ("/rkmix/title", juce::String ("Song 3"));
            send ("/rkmix/subtitle", juce::String ("Chorus"));
            const auto mixJson = juce::JSON::parse (remote.mixFor (2));
            expectEquals (mixJson["now"]["t"].toString(), juce::String ("Song 3"), "a phone's page");
            expectEquals (mixJson["now"]["s"].toString(), juce::String ("Chorus"));
            const auto structure = juce::JSON::parse (remote.getConsole().structureJson());
            expectEquals (structure["now"]["t"].toString(), juce::String ("Song 3"), "the iPad's");
            expectEquals (structure["now"]["s"].toString(), juce::String ("Chorus"));

            send ("/rkmix/now", juce::String ("Song 4"));
            expectEquals (osc.getNow().title, juce::String ("Song 4"), "both at once");
            expect (osc.getNow().subtitle.isEmpty(), "the second is optional");
            send ("/rkmix/now", juce::String ("Song 5"), juce::String ("Verse \"2\""));
            expectEquals (osc.getNow().subtitle, juce::String ("Verse \"2\""));
            expectEquals (juce::JSON::parse (remote.mixFor (2))["now"]["s"].toString(), juce::String ("Verse \"2\""), "escaped in the JSON");
            send ("/rkmix/title", juce::String ("Line one\nline two"));
            expectEquals (osc.getNow().title, juce::String ("Line one line two"), "one line");
            send ("/rkmix/title", juce::String::repeatedString ("x", 500));
            expectEquals (osc.getNow().title.length(), MixOsc::kMaxLength, "not too long");
            send ("/rkmix/title", 7);
            expectEquals (osc.getNow().title, juce::String ("7"), "a number is a name too");
            send ("/other/title", juce::String ("Nope"));
            send ("/rkmix/volume", 1.0f);
            expectEquals (osc.getNow().title, juce::String ("7"), "other addresses are ignored");
            send ("/rkmix/clear");
            expect (osc.getNow().isEmpty(), "cleared");

            // Scenes 1 to 16, as the window numbers them (an int, a float or text).
            send ("/rkmix/scene", 3);
            expectEquals (p.getScenes().getCurrent(), 2);
            send ("/rkmix/scene", 5.0f);
            expectEquals (p.getScenes().getCurrent(), 4);
            send ("/rkmix/scene", juce::String ("2"));
            expectEquals (p.getScenes().getCurrent(), 1);
            send ("/rkmix/scene", 17);
            send ("/rkmix/scene", 0);
            expectEquals (p.getScenes().getCurrent(), 1, "out of range: nothing");

            // Off: the names go (none left over from an earlier show).
            send ("/rkmix/title", juce::String ("Song 6"));
            osc.setEnabled (false);
            expect (osc.getNow().isEmpty());
            expectEquals (juce::JSON::parse (remote.mixFor (2))["now"]["t"].toString(), juce::String());

            // The settings are the computer's (remote.json); the names never are.
            osc.setEnabled (true);
            osc.setPort (9200);
            osc.setAcceptsFromNetwork (true);
            send ("/rkmix/title", juce::String ("Song 7"));
            {
                RKMixProcessor q;
                q.getRemote().setSettingsFileForTesting (dir.getChildFile ("remote.json"));
                auto& o = q.getRemote().getOsc();
                expect (o.isEnabled() && o.acceptsFromNetwork());
                expectEquals (o.getPort(), 9200);
                expect (o.getNow().isEmpty(), "the names aren't saved");
            }

            // The socket: bound to this computer only by default; the first RKMix takes the port.
            {
                const auto port = 20000 + juce::Random::getSystemRandom().nextInt (2000);
                MixOsc first (p), second (p);
                juce::DynamicObject::Ptr settings (new juce::DynamicObject());
                settings->setProperty ("on", true);
                settings->setProperty ("port", port);
                first.settingsFromVar (juce::var (settings.get()));
                second.settingsFromVar (juce::var (settings.get()));
                expect (! first.isListening(), "not until begun");
                first.begin();
                expect (first.isListening(), first.getStatus());
                expect (first.getStatus().contains ("127.0.0.1"), first.getStatus());
                second.begin();
                expect (! second.isListening(), "the port is taken");
                expect (second.getStatus().contains ("in use"), second.getStatus());
            }
            dir.deleteRecursively();
        }

        beginTest ("Console remote: PIN, remembered devices, subscriptions, commands");
        {
            RKMixProcessor p;
            auto& remote = p.getRemote();
            const auto dir = juce::File::createTempFile ("rkmix-console");
            remote.setSettingsFileForTesting (dir.getChildFile ("remote.json"));
            auto& console = remote.getConsole();
            expectEquals (console.getPin().length(), 4, "a PIN from the start");
            expect (console.setPin ("2468"));
            expect (! console.setPin ("12a4") && ! console.setPin ("123"), "digits only, 4 to 8");

            struct Fake : MixWebServer::Connection
            {
                juce::StringArray sent;
                bool closed = false;
                bool send (const juce::String& t) override { sent.add (t); return true; }
                void close() override { closed = true; }
                juce::var last (const juce::String& type)
                {
                    for (int i = sent.size(); --i >= 0;)
                        if (const auto v = juce::JSON::parse (sent[i]); v["type"].toString() == type)
                            return v;
                    return {};
                }
            };
            const auto exchange = [&] (Fake& c, const juce::String& message)
            {
                console.received (c, message);
                console.tick();
                console.send (c, {});
            };

            Fake a;
            a.address = "10.0.0.5";
            console.opened (a);
            exchange (a, R"({"t":"set","id":"ch_fader_1","v":0.5})");
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (p.findParam (mix::channelFaderId (0))), 0.0f, 0.01f,
                                       "nothing before signing in");
            exchange (a, R"({"t":"hello","pin":"1111","name":"iPad"})");
            expect ((bool) a.last ("auth")["wrong"], "a wrong PIN");
            exchange (a, R"({"t":"hello","pin":"2468","name":"iPad"})");
            const auto token = a.last ("auth")["token"].toString();
            expect ((bool) a.last ("auth")["ok"] && token.length() >= 16, "signed in, with a token");
            expect (a.last ("s")["ch"].size() == mix::kChannels, "the structure follows");

            // Values: subscribed IDs come with their description, then only changes.
            exchange (a, R"({"t":"sub","ids":["ch_fader_1","ch1_trim","bus1_type","nonsense"]})");
            const auto v = a.last ("v")["p"];
            expect (v.hasProperty ("ch_fader_1") && v.hasProperty ("ch1_trim") && ! v.hasProperty ("nonsense"));
            expectEquals (v["bus1_type"][4].size(), 3, "a choice's names");
            expectEquals (v["ch1_trim"][1].toString(), juce::String ("0.0 dB"));
            a.sent.clear();
            exchange (a, R"({"t":"set","id":"ch1_trim","v":0.75})");
            expectWithinAbsoluteError (p.getSettings().value (mix::channelId (0, "trim")), 12.0f, 0.01f, "a setting moved");
            const auto changed = a.last ("v")["p"];
            expect (changed.hasProperty ("ch1_trim") && ! changed.hasProperty ("ch_fader_1"), "only what changed");
            exchange (a, R"({"t":"set","id":"bypass","v":1})");
            expect (p.findParam ("bypass")->getValue() < 0.5f, "never the plugin's bypass");

            // Commands
            exchange (a, R"({"t":"solo","s":"ch3","on":true})");
            expect (p.isChannelSoloed (2));
            exchange (a, R"({"t":"clearSolo"})");
            expect (! p.isAnythingSoloed());
            exchange (a, R"({"t":"name","s":"bus2","text":"Wedges"})");
            expectEquals (p.getBusName (1), juce::String ("Wedges"));
            exchange (a, R"({"t":"colour","s":"ch1","c":4})");
            expectEquals (p.getChannelColour (0), 4);
            exchange (a, R"({"t":"curves","k":["eq:ch1","comp:ch1"]})");
            expectEquals (a.last ("curve")["d"].size(), 121, "a compressor curve");

            // Inserts: add one, its parameters by ID, on/off, copy and paste, remove.
            exchange (a, R"({"t":"insAdd","s":"ch2","n":1,"p":"RKCompress"})");
            const InsertRack::Slot slot { InsertRack::Strip::channel, 1, 1 };
            expect (p.getRack().get (slot) != nullptr, "an insert added");
            exchange (a, R"({"t":"insParams","s":"ch2","n":1})");
            const auto params = a.last ("insParams")["params"];
            expect (params.size() > 3, "its controls listed");
            const auto firstId = params[0]["id"].toString();
            expect (firstId.startsWith ("ins:" + juce::String (InsertRack::flatIndex (slot)) + ":"));
            exchange (a, R"({"t":"sub","ids":[")" + firstId + "\"]}");
            expect (a.last ("v")["p"].hasProperty (firstId), "an insert's value");
            exchange (a, R"({"t":"set","id":")" + firstId + R"(","v":0.1})");
            expectWithinAbsoluteError (p.getRack().get (slot)->getParameters()[0]->getValue(), 0.1f, 0.01f, "an insert's knob moved");
            exchange (a, R"({"t":"insOn","s":"ch2","n":1,"on":false})");
            expect (! p.getRack().isOn (slot));
            exchange (a, R"({"t":"insCopy","s":"ch2","n":1})");
            exchange (a, R"({"t":"insPaste","s":"bus3","n":0})");
            expect (p.getRack().get ({ InsertRack::Strip::bus, 2, 0 }) != nullptr, "pasted onto a bus");
            exchange (a, R"({"t":"insRemove","s":"ch2","n":1})");
            expect (p.getRack().get (slot) == nullptr, "removed");
            exchange (a, R"({"t":"set","id":")" + firstId + R"(","v":0.5})");   // gone: nothing happens

            // Section copy and paste; a user page's strips.
            setParam (p.getSettings().param (mix::channelId (4, "gate_thr")), -33.0f);
            exchange (a, R"({"t":"copy","s":"ch5","sec":"gate"})");
            expectEquals (a.last ("clip")["kind"].toString(), juce::String ("channel"));
            exchange (a, R"({"t":"paste","s":"ch7"})");
            expectWithinAbsoluteError (p.getSettings().value (mix::channelId (6, "gate_thr")), -33.0f, 0.01f, "the gate pasted");
            exchange (a, R"({"t":"paste","s":"bus1"})");   // a channel's copy never onto a bus
            exchange (a, R"({"t":"userPage","p":1,"list":[3,101,3,999]})");
            expect (p.getUserPage (1) == juce::Array<int> { 3, 101 }, "valid strips, once each");

            // Remembered: the token signs in without the PIN, until the device is disconnected.
            Fake b;
            console.opened (b);
            exchange (b, R"({"t":"hello","token":")" + token + "\"}");
            expect ((bool) b.last ("auth")["ok"], "the token is enough");
            const auto devices = console.getDevices();
            expectEquals ((int) devices.size(), 2);
            console.disconnect (devices[1].id);
            expect (b.closed, "disconnected");
            console.closed (b);
            Fake c;
            console.opened (c);
            exchange (c, R"({"t":"hello","token":")" + token + "\"}");
            expect (! (bool) c.last ("auth")["ok"], "a forgotten device types the PIN again");

            // Five wrong PINs: wait.
            for (int i = 0; i < MixConsole::kMaxFailures; ++i)
                exchange (c, R"({"t":"hello","pin":"0000"})");
            exchange (c, R"({"t":"hello","pin":"2468"})");
            expect ((int) c.last ("auth")["wait"] > 0 && ! (bool) c.last ("auth")["ok"], "locked out for a while");

            // The settings survive a restart; the console app is served.
            RKMixProcessor q;
            q.getRemote().setSettingsFileForTesting (dir.getChildFile ("remote.json"));
            expectEquals (q.getRemote().getConsole().getPin(), juce::String ("2468"));
            expect (q.getRemote().answer ("/console").body.toString().contains ("/c/console.js"));
            expectEquals (q.getRemote().answer ("/c/rk.js").contentType, juce::String ("text/javascript; charset=utf-8"));
            expectEquals (q.getRemote().answer ("/c/../remote.json").status, 404);
            console.closed (a);
            console.closed (c);
            dir.deleteRecursively();
        }

        beginTest ("Remote: a WebSocket on the local network sets the send and gets the mix");
        {
            RKMixProcessor p;
            auto& remote = p.getRemote();
            const auto dir = juce::File::createTempFile ("rkmix-remote-ws");
            dir.createDirectory();
            const auto port = 18000 + juce::Random::getSystemRandom().nextInt (2000);
            dir.getChildFile ("remote.json").replaceWithText (R"({"enabled":true,"port":)" + juce::String (port) + R"(,"secret":"0123456789abcdef0123"})");
            remote.setSettingsFileForTesting (dir.getChildFile ("remote.json"));
            expect (remote.isListening(), remote.getStatus());

            juce::StreamingSocket socket;
            expect (socket.connect ("127.0.0.1", port, 2000));
            const auto request = "GET /ws/" + remote.getKey (0) + " HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                                 "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
            socket.write (request.toRawUTF8(), (int) request.getNumBytesAsUTF8());
            juce::MemoryOutputStream reply;
            for (int i = 0; i < 2000 && ! reply.toString().contains ("\r\n\r\n"); ++i)
                if (socket.waitUntilReady (true, 50) > 0)
                {
                    char c;
                    if (socket.read (&c, 1, false) == 1)
                        reply.writeByte (c);
                }
            expect (reply.toString().startsWith ("HTTP/1.1 101"), reply.toString());
            expect (reply.toString().contains ("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), reply.toString());

            // A masked text frame, as a browser sends it.
            const juce::String msg (R"({"t":"send","k":"ch2","v":-12})");
            juce::MemoryOutputStream frame;
            frame.writeByte ((char) 0x81);
            frame.writeByte ((char) (0x80 | msg.length()));
            const std::uint8_t mask[4] { 0x12, 0x34, 0x56, 0x78 };
            frame.write (mask, 4);
            for (int i = 0; i < msg.length(); ++i)
                frame.writeByte ((char) (msg.toRawUTF8()[i] ^ mask[i % 4]));
            socket.write (frame.getData(), (int) frame.getDataSize());

            // The pusher sends the mix (a text frame starting {"type":"mix").
            juce::MemoryOutputStream in;
            for (int i = 0; i < 60 && ! in.toString().contains ("\"type\":\"mix\""); ++i)
                if (socket.waitUntilReady (true, 50) > 0)
                {
                    char buf[4096];
                    if (const auto n = socket.read (buf, sizeof (buf), false); n > 0)
                        in.write (buf, (size_t) n);
                }
            expect (in.toString().contains ("\"type\":\"mix\""), "the mix arrives");

            for (int i = 0; i < 40 && juce::exactlyEqual (p.getSettings().value (mix::channelSendId (1, 0)), mix::kFaderMinDb); ++i)
            {
                juce::Thread::sleep (25);
                remote.flush();
            }
            expectWithinAbsoluteError (p.getSettings().value (mix::channelSendId (1, 0)), -12.0f, 0.01f, "the phone's send");
            socket.close();
            remote.setEnabled (false);
            expect (! remote.isListening());
            dir.deleteRecursively();
        }

        beginTest ("Copy an insert: its settings and on/off, from a channel to a stereo bus, a channel, the main");
        {
            using Strip = InsertRack::Strip;
            RKMixProcessor p;
            auto& rack = p.getRack();
            auto* chorus = rack.insert ({ Strip::channel, 0, 0 }, "RKChorus", 1);
            setParam (*chorus->findParam ("depth"), 77.0f);
            rack.setOn ({ Strip::channel, 0, 0 }, false);
            p.setChannelName (0, "Guitar");
            expect (p.copyInsert ({ Strip::channel, 1, 0 }).isVoid(), "an empty slot copies nothing");

            const auto copy = p.copyInsert ({ Strip::channel, 0, 0 });
            expect (RKMixProcessor::isInsertCopy (copy) && ! RKMixProcessor::isChannelCopy (copy));
            expectEquals (RKMixProcessor::describeCopy (copy), juce::String ("RKChorus from Guitar"));

            for (const auto slot : { InsertRack::Slot { Strip::bus, 3, 2 }, InsertRack::Slot { Strip::channel, 7, 1 },
                                     InsertRack::Slot { Strip::main, 0, 3 } })
            {
                expect (p.pasteInsert (slot, copy));
                auto* pasted = rack.get (slot);
                expect (pasted != nullptr && pasted != chorus && pasted->getName() == "RKChorus");
                expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (pasted->findParam ("depth")), 77.0f, 0.01f, "its settings");
                expect (! rack.isOn (slot), "and off, as the copied one");
            }
            expectEquals (p.channelsOf (Strip::bus, 3), 2, "a stereo bus: the copy runs in stereo");
            expect (! p.pasteInsert ({ Strip::mono, 0, 3 }, copy), "the mono sum has 2 slots");
            expect (! p.pasteInsert ({ Strip::bus, 0, 0 }, p.copyChannel (0, RKMixProcessor::Section::gate)), "not a strip section");
        }

        beginTest ("The host sees the faders and mutes, assignables, scene, talk, DCAs and mute groups (158 + bypass)");
        {
            RKMixProcessor p;
            expectEquals (p.getParameters().size(), 159);
            expect (p.findParam ("dca_fader_8") != nullptr && p.findParam ("dca_mute_8") != nullptr && p.findParam ("mgrp_8") != nullptr);
            expect (p.findParam ("ch_fader_32") != nullptr && p.findParam ("bus_mute_16") != nullptr
                    && p.findParam ("mono_mute") != nullptr && p.findParam ("assign32") != nullptr
                    && p.findParam ("scene") != nullptr && p.findParam ("talk") != nullptr);
        }
    }
};

static MixStateTests mixStateTests;
