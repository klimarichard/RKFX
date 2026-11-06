#include "../plugins/RKLoop/LoopExport.h"
#include "../plugins/RKLoop/PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

/** RKLoop Export: the WAV files (sample-exact, the smpl loop), the names,
    Export all (empty loops left out), a busy loop, an unwritable folder,
    and the Save Loops footswitch. */
class LoopExportTests : public juce::UnitTest
{
public:
    LoopExportTests() : juce::UnitTest ("RKLoop export") {}

    void runTest() override
    {
        LoopExporter::useSettingsFile (false);
        juce::TemporaryFile tempDir;
        const auto root = tempDir.getFile();
        root.createDirectory();

        beginTest ("WAV: 32-bit float is bit-exact, 24-bit within its rounding; the smpl loop covers the file");
        {
            auto audio = noise (2, 12345);
            for (const auto fmt : { LoopExport::Format::float32, LoopExport::Format::int24 })
            {
                const auto file = root.getChildFile (fmt == LoopExport::Format::float32 ? "float.wav" : "int.wav");
                expect (LoopExport::writeWav (file, audio, 48000.0, fmt).wasOk());

                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (file.createInputStream().release(), true));
                expect (reader != nullptr);
                if (reader == nullptr)
                    continue;
                expectEquals ((int) reader->lengthInSamples, 12345);
                expectEquals ((int) reader->numChannels, 2);
                expectEquals (reader->sampleRate, 48000.0);
                expectEquals ((int) reader->bitsPerSample, fmt == LoopExport::Format::float32 ? 32 : 24);
                expect (reader->usesFloatingPointData == (fmt == LoopExport::Format::float32));

                juce::AudioBuffer<float> back (2, 12345);
                reader->read (&back, 0, 12345, 0, true, true);
                const auto tolerance = fmt == LoopExport::Format::float32 ? 0.0f : 1.0f / 8388608.0f;
                expect (maxDifference (audio, back) <= tolerance, "difference " + juce::String (maxDifference (audio, back)));

                expectEquals (reader->metadataValues.getValue ("NumSampleLoops", {}), juce::String ("1"));
                expectEquals (reader->metadataValues.getValue ("Loop0Start", {}), juce::String ("0"));
                expectEquals (reader->metadataValues.getValue ("Loop0End", {}), juce::String ("12344"));
                expectEquals (reader->metadataValues.getValue ("Loop0Type", {}), juce::String ("0"));
            }

            auto mono = noise (1, 1000);
            const auto file = root.getChildFile ("mono.wav");
            expect (LoopExport::writeWav (file, mono, 44100.0, LoopExport::Format::int24).wasOk());
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (file.createInputStream().release(), true));
            expect (reader != nullptr && reader->numChannels == 1);
        }

        beginTest ("Names");
        {
            const juce::Time t (2026, 10, 6, 21, 14, 30);   // months from 0: November
            expectEquals (LoopExport::stamp (t), juce::String ("2026-11-06 21-14"));
            expectEquals (LoopExport::singleFileName (t, 2, true, std::nullopt), juce::String ("RKLoop 2026-11-06 21-14 Loop 3 Layer.wav"));
            expectEquals (LoopExport::singleFileName (t, 0, false, 120.0), juce::String ("RKLoop 2026-11-06 21-14 Loop 1 Part 120 BPM.wav"));
            expectEquals (LoopExport::bpmSuffix (92.46), juce::String (" 92.5 BPM"));
            expectEquals (LoopExport::bpmSuffix (99.97), juce::String (" 100 BPM"));
            expectEquals (LoopExport::folderName (t), juce::String ("RKLoop 2026-11-06 21-14"));
            expectEquals (LoopExport::fileInFolderName (0, false), juce::String ("Loop 1 Part.wav"));
        }

        beginTest ("Export all: the loops with audio into a new folder; a busy loop is skipped and named");
        {
            LoopEngine e;
            e.prepare (48000.0);
            record (e, 0, 9600);
            record (e, 2, 4800);
            LoopExporter x (e, context (2));
            x.setRetrySeconds (0.0);

            x.exportAll (root);
            x.waitForWriter();
            x.tick();
            const auto folders = root.findChildFiles (juce::File::findDirectories, false, "RKLoop *");
            expectEquals (folders.size(), 1);
            if (folders.size() == 1)
            {
                const auto files = folders[0].findChildFiles (juce::File::findFiles, false, "*.wav");
                expectEquals (files.size(), 2);
                expect (folders[0].getChildFile ("Loop 1 Part.wav").existsAsFile());
                expect (folders[0].getChildFile ("Loop 3 Layer.wav").existsAsFile());
            }
            expectEquals (x.getStatus(), juce::String ("Saved 2 loops"));

            // A second export in the same minute: a folder of its own.
            x.exportAll (root);
            x.waitForWriter();
            x.tick();
            expectEquals (root.findChildFiles (juce::File::findDirectories, false, "RKLoop *").size(), 2);

            // Loop 3 (playing) overdubbing: busy, so it's left out.
            expect (e.getState (2) == LoopEngine::State::playing);
            e.select (2);
            e.press (LoopEngine::Button::record);
            expect (e.isBusy (2));
            x.exportAll (root.getChildFile ("busy"));
            x.waitForWriter();
            x.tick();
            expect (x.getStatus().contains ("Loop 3 was busy"), x.getStatus());
            const auto busyFolders = root.getChildFile ("busy").findChildFiles (juce::File::findDirectories, false);
            expect (busyFolders.size() == 1 && busyFolders[0].findChildFiles (juce::File::findFiles, false, "*.wav").size() == 1);

            // Nothing at all: a message, no folder.
            LoopEngine empty;
            empty.prepare (48000.0);
            LoopExporter none (empty, context (2));
            none.exportAll (root.getChildFile ("none"));
            expect (! root.getChildFile ("none").exists());
            expect (none.statusIsError());
        }

        beginTest ("Export one loop: the file picked, mono in a mono slot; an unwritable folder gives an error");
        {
            LoopEngine e;
            e.prepare (48000.0);
            record (e, 1, 4800);
            LoopExporter x (e, context (1));

            const auto file = root.getChildFile ("one/My loop.wav");
            x.exportLoop (1, file);
            x.waitForWriter();
            x.tick();
            expect (file.existsAsFile());
            expectEquals (x.getStatus(), juce::String ("Saved Loop 2"));
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (file.createInputStream().release(), true));
            expect (reader != nullptr && reader->numChannels == 1 && reader->lengthInSamples == 4800);

            const auto locked = root.getChildFile ("locked");
            locked.createDirectory();
            locked.setReadOnly (true);
            x.exportLoop (1, locked.getChildFile ("sub/x.wav"));
            x.waitForWriter();
            x.tick();
            expect (x.statusIsError(), x.getStatus());
            expect (x.getStatus().startsWith ("Not saved"), x.getStatus());
            expectEquals (e.getLength (1), 4800, "the loop is untouched");
            locked.setReadOnly (false);
        }

        beginTest ("Save Loops acts once per press");
        {
            RKLoopProcessor p;
            auto& save = *p.findParam ("save_loops");
            save.setValueNotifyingHost (1.0f);
            save.setValueNotifyingHost (1.0f);
            expectEquals (p.getExporter().getPendingQuickSaves(), 1, "held: one press");
            save.setValueNotifyingHost (0.0f);
            save.setValueNotifyingHost (1.0f);
            save.setValueNotifyingHost (0.0f);
            expectEquals (p.getExporter().getPendingQuickSaves(), 2);
        }

        root.deleteRecursively();
    }

private:
    static LoopExporter::Context context (int channels)
    {
        return { [channels] { return channels; }, [] { return 48000.0; }, [] { return std::optional<double>(); },
                 [] (int i) { return i == 2; } };
    }

    static juce::AudioBuffer<float> noise (int channels, int n)
    {
        juce::Random r (7);
        juce::AudioBuffer<float> b (channels, n);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < n; ++i)
                b.setSample (ch, i, r.nextFloat() * 1.6f - 0.8f);
        return b;
    }

    /** Records @p n samples of noise into loop @p index (sync off: as long as played), then plays it. */
    static void record (LoopEngine& e, int index, int n)
    {
        e.setSync (false);
        e.select (index);
        e.press (LoopEngine::Button::record);
        auto b = noise (2, n);
        for (int start = 0; start < n; start += 64)
        {
            float* ch[] { b.getWritePointer (0, start), b.getWritePointer (1, start) };
            e.process (ch, 2, juce::jmin (64, n - start));
        }
        e.press (LoopEngine::Button::record);
    }

    static float maxDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        float d = 0.0f;
        for (int ch = 0; ch < juce::jmin (a.getNumChannels(), b.getNumChannels()); ++ch)
            for (int i = 0; i < juce::jmin (a.getNumSamples(), b.getNumSamples()); ++i)
                d = juce::jmax (d, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
        return d;
    }
};

static LoopExportTests loopExportTests;
