// RKConduct's presets are folders: preset.json and the MIDI file it plays,
// so a preset moved to another computer still has its song.

#include "PluginProcessor.h"
#include "plugin/PresetLibrary.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

using rkfx::PresetLibrary;

namespace
{
    /** A MIDI file of @p tracks tracks, each with @p notes quarter notes. */
    juce::File writeMidi (const juce::File& file, int tracks, int notes)
    {
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        for (int t = 0; t < tracks; ++t)
        {
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Track " + juce::String (t + 1)), 0);
            for (int n = 0; n < notes; ++n)
            {
                seq.addEvent (juce::MidiMessage::noteOn (1, 60 + t, (juce::uint8) 100), n * 480);
                seq.addEvent (juce::MidiMessage::noteOff (1, 60 + t), n * 480 + 240);
            }
            seq.updateMatchedPairs();
            midi.addTrack (seq);
        }
        file.deleteFile();
        juce::FileOutputStream out (file);
        midi.writeTo (out);
        return file;
    }

    juce::Array<juce::File> midiFilesIn (const juce::File& folder)
    {
        return folder.findChildFiles (juce::File::findFiles, false, "*.mid");
    }
}

class ConductPresetTests : public juce::UnitTest
{
public:
    ConductPresetTests() : juce::UnitTest ("RKConduct preset folders") {}

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("RKConductTests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        root.createDirectory();
        const auto song = writeMidi (root.getChildFile ("Opening Song.mid"), 2, 8);
        const auto preset = root.getChildFile ("Presets/Opening/preset.json");

        beginTest ("Folder preset helpers");
        {
            expect (PresetLibrary::isFolderPreset (preset));
            expect (! PresetLibrary::isFolderPreset (root.getChildFile ("Opening.json")));
            expectEquals (PresetLibrary::presetName (preset), juce::String ("Opening"));
            expectEquals (PresetLibrary::presetName (root.getChildFile ("Flat.json")), juce::String ("Flat"));
        }

        beginTest ("Saving writes the MIDI file into the preset's folder, not into preset.json");
        {
            RKConductProcessor p;
            expect (p.loadFile (song).isEmpty());
            p.setTrackOn (1, false);
            p.setTrackChannel (0, 5);

            const auto r = p.savePreset (preset);
            expect (r.ok, r.message);
            expectEquals (p.getPresetName(), juce::String ("Opening"));
            expect (preset.existsAsFile());

            const auto files = midiFilesIn (preset.getParentDirectory());
            expectEquals (files.size(), 1);
            expectEquals (files[0].getFileName(), juce::String ("Opening Song.mid"));
            expect (files[0].getSize() == song.getSize(), "the file's own bytes");

            const auto extra = juce::JSON::parse (preset)["extra"];
            expectEquals (extra["file"].toString(), juce::String ("Opening Song.mid"));
            expect (! extra.hasProperty ("data"), "not embedded");
        }

        beginTest ("A moved preset folder loads with its song and track settings");
        {
            const auto moved = root.getChildFile ("Elsewhere/Opening");
            expect (preset.getParentDirectory().copyDirectoryTo (moved));
            song.deleteFile();   // the original MIDI file is gone

            RKConductProcessor p;
            const auto r = p.loadPreset (moved.getChildFile ("preset.json"));
            expect (r.ok && r.message.isEmpty(), r.message);
            expectEquals (p.getSongName(), juce::String ("Opening Song"));
            expectEquals ((int) p.getTracks().size(), 2);
            expect (p.isTrackOn (0) && ! p.isTrackOn (1));
            expectEquals (p.getTrackChannel (0), 5);
            expectEquals (p.getPresetName(), juce::String ("Opening"));
        }

        beginTest ("Saving another song over the preset leaves one MIDI file");
        {
            RKConductProcessor p;
            expect (p.loadPreset (preset).ok);
            expect (p.loadFile (writeMidi (root.getChildFile ("Encore.mid"), 1, 4)).isEmpty());
            expect (p.savePreset (preset).ok);

            const auto files = midiFilesIn (preset.getParentDirectory());
            expectEquals (files.size(), 1);
            expectEquals (files[0].getFileName(), juce::String ("Encore.mid"));

            RKConductProcessor q;
            expect (q.loadPreset (preset).ok);
            expectEquals (q.getSongName(), juce::String ("Encore"));
            expect (q.isTrackOn (0), "a new song's tracks start on");
        }

        beginTest ("A folder preset without its MIDI file: a note, and no song");
        {
            for (const auto& f : midiFilesIn (preset.getParentDirectory()))
                f.deleteFile();

            RKConductProcessor p;
            expect (p.loadFile (writeMidi (root.getChildFile ("Other.mid"), 1, 2)).isEmpty());
            const auto r = p.loadPreset (preset);
            expect (r.ok);
            expect (r.message.contains ("Encore.mid"), r.message);
            expect (p.getSongName().isEmpty(), "the old song is not kept");
        }

        beginTest ("A preset saved with no song has no MIDI file, and clears the song");
        {
            const auto empty = root.getChildFile ("Presets/Empty/preset.json");
            RKConductProcessor p;
            expect (p.savePreset (empty).ok);
            expectEquals (midiFilesIn (empty.getParentDirectory()).size(), 0);

            RKConductProcessor q;
            expect (q.loadFile (writeMidi (root.getChildFile ("Some.mid"), 1, 2)).isEmpty());
            expect (q.loadPreset (empty).ok);
            expect (q.getSongName().isEmpty());
        }

        beginTest ("An old flat .json preset still loads, keeping the song");
        {
            const auto flat = root.getChildFile ("Presets/Old.json");
            flat.replaceWithText (R"({"plugin": "RKConduct", "version": 1, "params": {}})");

            RKConductProcessor p;
            expect (p.loadFile (writeMidi (root.getChildFile ("Kept.mid"), 1, 2)).isEmpty());
            const auto r = p.loadPreset (flat);
            expect (r.ok, r.message);
            expectEquals (p.getSongName(), juce::String ("Kept"));
            expectEquals (p.getPresetName(), juce::String ("Old"));
        }

        beginTest ("The session still embeds the song (a gig never depends on files)");
        {
            RKConductProcessor p;
            expect (p.loadFile (root.getChildFile ("Kept.mid")).isEmpty());
            juce::MemoryBlock state;
            p.getStateInformation (state);
            root.getChildFile ("Kept.mid").deleteFile();

            RKConductProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (q.getSongName(), juce::String ("Kept"));
        }

        root.deleteRecursively();
    }
};

static ConductPresetTests conductPresetTests;

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
    std::printf (failures > 0 ? "RKConductTests: %d test failure(s)\n" : "RKConductTests: all tests passed\n", failures);
    return failures > 0 ? 1 : 0;
}
