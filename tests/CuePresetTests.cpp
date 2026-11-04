// RKCue's presets are folders: preset.json and the slots' MIDI files, so a
// preset moved to another computer still plays the same files.

#include "PluginProcessor.h"
#include "plugin/PresetLibrary.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

namespace
{
    /** A MIDI file with @p notes quarter notes. */
    juce::File writeMidi (const juce::File& file, int notes)
    {
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        juce::MidiMessageSequence seq;
        for (int n = 0; n < notes; ++n)
        {
            seq.addEvent (juce::MidiMessage::noteOn (1, 60 + n, (juce::uint8) 100), n * 480);
            seq.addEvent (juce::MidiMessage::noteOff (1, 60 + n), n * 480 + 240);
        }
        seq.updateMatchedPairs();
        midi.addTrack (seq);
        file.deleteFile();
        juce::FileOutputStream out (file);
        midi.writeTo (out);
        return file;
    }

    juce::Array<juce::File> midiFilesIn (const juce::File& folder)
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, "*.mid");
        files.sort();
        return files;
    }

    void setValue (rkfx::ProcessorBase& p, const char* id, float value)
    {
        auto* param = p.findParam (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }
}

class CuePresetTests : public juce::UnitTest
{
public:
    CuePresetTests() : juce::UnitTest ("RKCue presets and state") {}

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("RKCueTests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        root.createDirectory();
        const auto fill = writeMidi (root.getChildFile ("Drum Fill.mid"), 4);
        const auto riff = writeMidi (root.getChildFile ("Bass Riff.mid"), 8);
        const auto preset = root.getChildFile ("Presets/Set One/preset.json");

        beginTest ("Saving writes each slot's MIDI file into the preset's folder");
        {
            RKCueProcessor p;
            expect (p.loadFile (0, fill).isEmpty());
            expect (p.loadFile (4, riff).isEmpty());
            p.setSlotName (4, "Riff");
            setValue (p, "s5_pitch", 1.0f);

            const auto r = p.savePreset (preset);
            expect (r.ok, r.message);
            const auto files = midiFilesIn (preset.getParentDirectory());
            expectEquals (files.size(), 2);
            expectEquals (files[0].getFileName(), juce::String ("01 Drum Fill.mid"));
            expectEquals (files[1].getFileName(), juce::String ("05 Riff.mid"));
            expect (! juce::JSON::parse (preset)["extra"]["slots"][0].hasProperty ("data"), "not embedded");
        }

        beginTest ("A moved preset folder loads with its files, names and settings");
        {
            const auto moved = root.getChildFile ("Elsewhere/Set One");
            expect (preset.getParentDirectory().copyDirectoryTo (moved));
            fill.deleteFile();

            RKCueProcessor p;
            expect (p.loadFile (9, riff).isEmpty());   // replaced: the preset's slot 10 is empty
            const auto r = p.loadPreset (moved.getChildFile ("preset.json"));
            expect (r.ok && r.message.isEmpty(), r.message);
            expectEquals (p.getSlot (0).name, juce::String ("Drum Fill"));
            expectEquals (p.getSlot (4).name, juce::String ("Riff"));
            expectEquals ((int) p.getSlot (4).tracks.size(), 1);
            expect (! p.getSlot (9).hasFile(), "an empty slot in the preset clears the slot");
            expect (rkfx::ProcessorBase::valueOf (p.findParam ("s5_pitch")) >= 0.5f);
        }

        beginTest ("Saving again with fewer files leaves only those");
        {
            RKCueProcessor p;
            expect (p.loadPreset (preset).ok);
            p.clearSlot (0);
            expect (p.savePreset (preset).ok);
            expectEquals (midiFilesIn (preset.getParentDirectory()).size(), 1);
        }

        beginTest ("A missing file: a note, and the slot is empty");
        {
            for (const auto& f : midiFilesIn (preset.getParentDirectory()))
                f.deleteFile();
            RKCueProcessor p;
            const auto r = p.loadPreset (preset);
            expect (r.ok);
            expect (r.message.contains ("05 Riff.mid"), r.message);
            expect (! p.getSlot (4).hasFile());
        }

        beginTest ("Factory presets change the layout and keep the files");
        {
            RKCueProcessor p;
            expect (p.loadFile (0, riff).isEmpty());
            const auto& factory = p.getFactoryPresets();
            expect (factory.size() >= 5);
            for (int i = 0; i < (int) factory.size(); ++i)
            {
                const auto r = p.loadFactoryPreset (i);
                expect (r.ok && r.message.isEmpty(), factory[(size_t) i].name + ": " + r.message);
            }
            expect (p.getSlot (0).hasFile());
        }

        beginTest ("The session embeds every file");
        {
            RKCueProcessor p;
            expect (p.loadFile (2, riff).isEmpty());
            expect (p.loadFile (15, riff).isEmpty());
            juce::MemoryBlock state;
            p.getStateInformation (state);
            riff.deleteFile();

            RKCueProcessor q;
            expect (q.loadFile (0, writeMidi (root.getChildFile ("Old.mid"), 1)).isEmpty());
            q.setStateInformation (state.getData(), (int) state.getSize());
            expect (! q.getSlot (0).hasFile(), "the session's empty slot");
            expectEquals (q.getSlot (2).name, juce::String ("Bass Riff"));
            expect (q.getSlot (15).hasFile());
        }

        beginTest ("Not a MIDI file");
        {
            const auto text = root.getChildFile ("notes.mid");
            text.replaceWithText ("hello");
            RKCueProcessor p;
            expect (p.loadFile (0, text).isNotEmpty());
            expect (! p.getSlot (0).hasFile());
        }

        root.deleteRecursively();
    }
};

static CuePresetTests cuePresetTests;

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* r = runner.getResult (i))
            failures += r->failures;
    std::printf (failures > 0 ? "RKCueTests: %d test failure(s)\n" : "RKCueTests: all tests passed\n", failures);
    return failures > 0 ? 1 : 0;
}
