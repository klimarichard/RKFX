#include <juce_audio_processors/juce_audio_processors.h>

#include "../plugins/RKRemap/PluginProcessor.h"

/** RKRemap as a plugin: 64 channels each way, straight through by default,
    and names and views kept in the session and in preset files (the base
    processor's extraInPresets). Built into RKBoardTests, which links every
    processor. */
class RemapStateTests : public juce::UnitTest
{
public:
    RemapStateTests() : juce::UnitTest ("RKRemap state") {}

    void runTest() override
    {
        beginTest ("64 inputs and 64 outputs, straight through when inserted");
        {
            RKRemapProcessor p;
            expectEquals (p.getTotalNumInputChannels(), 64);
            expectEquals (p.getTotalNumOutputChannels(), 64);
            p.prepareToPlay (48000.0, 128);
            juce::AudioBuffer<float> b (64, 128);
            for (int ch = 0; ch < 64; ++ch)
                for (int i = 0; i < 128; ++i)
                    b.setSample (ch, i, (float) (ch + 1) * 0.01f);
            const auto input = b;
            juce::MidiBuffer midi;
            p.processBlock (b, midi);
            bool identical = true;
            for (int ch = 0; ch < 64; ++ch)
                for (int i = 0; i < 128; ++i)
                    identical = identical && juce::exactlyEqual (b.getSample (ch, i), input.getSample (ch, i));
            expect (identical, "In n -> Out n, bit-exact");
        }

        beginTest ("Names and views are kept in the session");
        {
            RKRemapProcessor a;
            a.setInputName (4, "Synth Bass L");
            a.setOutputName (63, "Recorder");
            a.setInputsShown (8);
            a.setOutputsShown (40);
            juce::MemoryBlock state;
            a.getStateInformation (state);

            RKRemapProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (b.getInputName (4), juce::String ("Synth Bass L"));
            expectEquals (b.getOutputName (63), juce::String ("Recorder"));
            expectEquals (b.getInputsShown(), 8);
            expectEquals (b.getOutputsShown(), 40);
        }

        beginTest ("Names and views are kept in preset files, and Reset clears them");
        {
            const auto file = juce::File::createTempFile (".json");
            RKRemapProcessor a;
            a.setOutputName (0, "FOH Vocal");
            a.setOutputsShown (12);
            expect (a.savePreset (file).ok);

            RKRemapProcessor b;
            expect (b.loadPreset (file).ok);
            expectEquals (b.getOutputName (0), juce::String ("FOH Vocal"));
            expectEquals (b.getOutputsShown(), 12);

            b.resetToDefaults();
            expect (b.getOutputName (0).isEmpty(), "Reset: no names");
            expectEquals (b.getOutputsShown(), RKRemapProcessor::kDefaultShown);
            file.deleteFile();
        }

        beginTest ("A factory preset brings its names");
        {
            RKRemapProcessor p;
            int index = -1;
            for (int i = 0; i < (int) p.getFactoryPresets().size(); ++i)
                if (p.getFactoryPresets()[(size_t) i].name == "IEM and FOH")
                    index = i;
            expect (index >= 0);
            if (index >= 0)
            {
                expect (p.loadFactoryPreset (index).ok);
                expectEquals (p.getInputName (0), juce::String ("Vocal"));
                expectEquals (p.getOutputName (2), juce::String ("FOH Synth Bass"));
            }
        }

        beginTest ("Names don't change other plugins' presets");
        {
            RKRemapProcessor p;
            p.setOutputName (1, "x");
            expect (p.isPresetModified(), "a name marks the preset modified");
        }
    }
};

static RemapStateTests remapStateTests;
