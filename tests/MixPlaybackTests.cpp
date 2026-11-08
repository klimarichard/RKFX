#include "../plugins/RKMix/MixPlayback.h"
#include "../plugins/RKMix/MixExport.h"
#include "../plugins/RKMix/PluginProcessor.h"

/** RKMix's virtual soundcheck (MixPlayback): files play into the inputs
    sample for sample; locate and loop are sample-accurate with 5 ms fades;
    a dry ring plays silence; the mapping rules; plain WAV folders; another
    sample rate is refused; through the console, a played-back channel
    sounds exactly as the same signal live. */
class MixPlaybackTests : public juce::UnitTest
{
public:
    MixPlaybackTests() : juce::UnitTest ("RKMix playback") {}

    static constexpr double kRate = 48000.0;
    static constexpr int kBlock = 128, kLength = 240000;   // 5 s: longer than the 3 s ring

    static float signal (int channel, juce::int64 i) { return 0.8f * (float) std::sin (0.0007 * (double) (i + 1) * (1.0 + channel) + channel); }

    static void writeWav (const juce::File& f, int channels, int firstSignal, double rate = kRate, int length = kLength)
    {
        juce::AudioBuffer<float> b (channels, length);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < length; ++i)
                b.setSample (ch, i, signal (firstSignal + ch, i));
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> out = f.createOutputStream();
        juce::WavAudioFormat wav;
        auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (channels).withBitsPerSample (32)
                                               .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        w->writeFromAudioSampleBuffer (b, 0, length);
    }

    /** A recording: input 1 (mono), inputs 3/4 (stereo), as RKMix writes it. */
    static juce::File makeRecording (const juce::File& folder, double rate = kRate)
    {
        folder.createDirectory();
        writeWav (folder.getChildFile ("01 Vox.wav"), 1, 0, rate);
        writeWav (folder.getChildFile ("03-04 Keys.wav"), 2, 2, rate);
        folder.getChildFile ("session.json").replaceWithText (
            R"({"sampleRate":)" + juce::String (rate) + R"(,"files":{"01 Vox.wav":[1],"03-04 Keys.wav":[3,4]},)"
            R"("markers":[{"at":24000,"label":"Scene 2"},{"at":48000,"label":"Scene 3"}],"names":["Vox","","Keys"]})");
        return folder;
    }

    struct Player
    {
        MixPlayback& p;
        juce::AudioBuffer<float> in { 32, kBlock };
        std::array<bool, 32> all {};
        explicit Player (MixPlayback& pb) : p (pb) { all.fill (true); p.prepare (kBlock); }
        /** One block; with @p waitForDisk, first lets the audio side take the last
            locate (fading out if it must) and waits until the ring is ready. */
        const float* block (bool waitForDisk = true)
        {
            if (waitForDisk)
            {
                for (int n = 0; n < 20 && p.needsAudio(); ++n)
                {
                    in.clear();
                    p.process (in.getArrayOfWritePointers(), all, kBlock);
                }
                p.waitUntilReady (3000);
            }
            in.clear();
            p.process (in.getArrayOfWritePointers(), all, kBlock);
            return in.getReadPointer (0);
        }
    };

    void runTest() override
    {
        juce::TemporaryFile temp;
        const auto root = temp.getFile();
        root.createDirectory();
        const auto folder = makeRecording (root.getChildFile ("take"));

        beginTest ("Scan: session.json maps each file's channels; markers and names");
        {
            const auto r = MixPlayback::scan (folder);
            expect (r.error.isEmpty(), r.error);
            expectEquals ((int) r.files.size(), 2);
            expectEquals (r.length, (juce::int64) kLength);
            expectEquals ((int) r.markers.size(), 2);
            expectEquals (r.markers[1].label, juce::String ("Scene 3"));
            expectEquals (r.names[2], juce::String ("Keys"));
        }

        beginTest ("Markers edited: added in time order, renamed, deleted; saved into session.json, the rest kept");
        {
            const auto copy = root.getChildFile ("edit");
            makeRecording (copy);
            MixPlayback p;
            expect (p.load (MixPlayback::scan (copy), kRate).wasOk());
            expectEquals (p.addMarker (36000), 1, "between Scene 2 and Scene 3");
            expectEquals (p.getRecording().markers[1].label, juce::String ("Marker 1"));
            expectEquals (p.addMarker (1000, "Intro"), 0);
            expectEquals (p.addMarker (60000), 4);
            expectEquals (p.getRecording().markers[4].label, juce::String ("Marker 2"), "the next number");
            expect (p.renameMarker (2, "Verse"));
            expect (! p.renameMarker (2, "   "), "an empty name is refused");
            expect (! p.renameMarker (9, "x"));
            expect (p.removeMarker (3));   // Scene 3
            expectEquals (p.markerIndexAt (0), -1);
            expectEquals (p.markerIndexAt (40000), 2);

            const auto r = MixPlayback::scan (copy);
            expectEquals ((int) r.markers.size(), 4);
            expectEquals (r.markers[0].label, juce::String ("Intro"));
            expectEquals (r.markers[2].label, juce::String ("Verse"));
            expectEquals (r.markers[2].at, (juce::int64) 36000);
            expectEquals (r.markers[3].label, juce::String ("Marker 2"));
            expectEquals ((int) r.files.size(), 2, "the files are still described");
            expectEquals (r.names[2], juce::String ("Keys"), "and the names");

            // A folder of plain WAVs keeps its markers in a session.json of its own.
            const auto plain = root.getChildFile ("plain markers");
            plain.createDirectory();
            writeWav (plain.getChildFile ("01 Vox.wav"), 1, 0);
            MixPlayback q;
            expect (q.load (MixPlayback::scan (plain), kRate).wasOk());
            q.addMarker (12000, "Chorus");
            const auto again = MixPlayback::scan (plain);
            expectEquals ((int) again.markers.size(), 1);
            expectEquals ((int) again.files.size(), 1, "still read as plain WAVs");
            expectEquals (again.files[0].inputs[0], 0);
        }

        beginTest ("Play: every input sample for sample (after the 5 ms fade-in), all aligned");
        {
            MixPlayback p;
            expect (p.load (MixPlayback::scan (folder), kRate).wasOk());
            Player player (p);
            in_ready (player);
            p.play();
            juce::int64 at = 0;
            float worst = 0.0f;
            for (int b = 0; b < 100; ++b)
            {
                player.block (false);
                for (const auto& [input, sig] : std::vector<std::pair<int, int>> { { 0, 0 }, { 2, 2 }, { 3, 3 } })
                    for (int i = 0; i < kBlock; ++i)
                        if (at + i >= 240)
                            worst = juce::jmax (worst, std::abs (player.in.getSample (input, i) - signal (sig, at + i)));
                at += kBlock;
            }
            expectEquals (worst, 0.0f, "bit-exact (float files)");
            expectEquals (player.in.getMagnitude (1, 0, kBlock), 0.0f, "an input with no file: silence");
            expectEquals (p.getPosition(), at);
            expectEquals (p.getUnderruns(), 0);
        }

        beginTest ("Locate and loop: sample-accurate, faded, no clicks");
        {
            MixPlayback p;
            expect (p.load (MixPlayback::scan (folder), kRate).wasOk());
            Player player (p);
            p.locate (30000);
            in_ready (player);
            p.play();
            juce::int64 at = 30000;
            player.block (false);       // fades in from 30000
            at += kBlock;
            const auto* x = player.block (false);
            expectWithinAbsoluteError (x[120], signal (0, at + 120), 1.0e-7f, "after a locate, from exactly there (past the 240-sample fade)");

            // Loop between the two markers.
            p.setLoop (true, 24000, 48000);
            expectEquals (p.getPosition(), (juce::int64) 30000 + 2 * kBlock, "inside the loop: stays where it is");
            float worstStep = 0.0f, last = 0.0f;
            bool wrapped = false;
            juce::int64 before = p.getPosition();
            for (int b = 0; b < 400; ++b)
            {
                player.block (b == 0);
                for (int i = 0; i < kBlock; ++i)
                {
                    const auto v = player.in.getSample (0, i);
                    if (b > 2)
                        worstStep = juce::jmax (worstStep, std::abs (v - last));
                    last = v;
                }
                if (p.getPosition() < before)
                    wrapped = true;
                before = p.getPosition();
            }
            expect (wrapped, "it went round");
            expect (p.getPosition() >= 24000 && p.getPosition() < 48000);
            expectLessThan (worstStep, 0.01f, "the seam and the locate are click-free");
        }

        beginTest ("A dry ring plays silence and counts an underrun; the audio never waits");
        {
            MixPlayback p;
            expect (p.load (MixPlayback::scan (folder), kRate).wasOk());
            Player player (p);
            in_ready (player);
            p.play();
            p.stallDiskForTesting (true);
            float loudestLate = 0.0f;
            for (int b = 0; b < 2 * (int) (MixPlayback::kRingSeconds * kRate / kBlock); ++b)
            {
                player.block (false);
                if (b > (int) (MixPlayback::kRingSeconds * kRate / kBlock) + 10)
                    loudestLate = juce::jmax (loudestLate, player.in.getMagnitude (0, 0, kBlock));
            }
            expectGreaterThan (p.getUnderruns(), 0);
            expectEquals (loudestLate, 0.0f, "silence, never garbage");
            p.stallDiskForTesting (false);
        }

        beginTest ("Mapping: a stereo file only on a pair; one source per input; plain WAVs by the number in their name");
        {
            MixPlayback p;
            expect (p.load (MixPlayback::scan (folder), kRate).wasOk());
            expect (! p.mapFile (1, 5), "a stereo file needs an odd input (1-based): its sides land on a pair");
            expect (p.mapFile (1, 6));
            expect (p.sourceOf (6) == std::make_pair (1, 0) && p.sourceOf (7) == std::make_pair (1, 1));
            expect (p.sourceOf (2).first < 0, "the old inputs are free");
            expect (p.mapFile (0, 7), "a mono file onto the pair's right input");
            expect (p.sourceOf (7) == std::make_pair (0, 0));
            expect (p.sourceOf (6).first < 0, "an input takes one source: the stereo file let go of its pair");

            const auto plain = root.getChildFile ("plain");
            plain.createDirectory();
            writeWav (plain.getChildFile ("05 Kick.wav"), 1, 0);
            writeWav (plain.getChildFile ("09-10 OH.wav"), 2, 1);
            writeWav (plain.getChildFile ("Guitar.wav"), 1, 3);
            const auto r = MixPlayback::scan (plain);
            expect (r.error.isEmpty(), r.error);
            std::map<juce::String, std::vector<int>> inputs;
            for (const auto& f : r.files)
                inputs[f.file.getFileName()] = f.inputs;
            expect (inputs["05 Kick.wav"] == std::vector<int> { 4 });
            expect (inputs["09-10 OH.wav"] == std::vector<int> { 8, 9 });
            expect (inputs["Guitar.wav"] == std::vector<int> { 0 }, "unnumbered: the first free input");
        }

        beginTest ("Another sample rate is refused, with a message");
        {
            const auto other = makeRecording (root.getChildFile ("44k"), 44100.0);
            MixPlayback p;
            const auto r = p.load (MixPlayback::scan (other), kRate);
            expect (r.failed());
            expect (r.getErrorMessage().contains ("44.1 kHz"), r.getErrorMessage());
        }

        beginTest ("Through the console: a channel on Playback sounds exactly as the same signal live; a loaded mix starts all Live");
        {
            RKMixProcessor live, played;
            for (auto* m : { &live, &played })
            {
                setParam (m->getSettings().param (mix::channelId (0, "eq_on")), 1.0f);
                setParam (m->getSettings().param (mix::channelId (0, "eq_lmid_gain")), 6.0f);
                setParam (m->getSettings().param (mix::channelId (0, "comp_on")), 1.0f);
                m->prepareToPlay (kRate, kBlock);
            }
            expect (played.getPlayback().load (MixPlayback::scan (folder), kRate).wasOk());
            played.setInputOnPlayback (0, true);
            expect (played.isInputOnPlayback (0) && ! played.isInputOnPlayback (1));

            juce::AudioBuffer<float> a (32, kBlock), b (32, kBlock);
            juce::MidiBuffer midi;
            // Ready, then play from the start; the live console gets the file's signal (faded in the same way).
            for (int n = 0; n < 20 && played.getPlayback().needsAudio(); ++n)
            {
                b.clear();
                played.processBlock (b, midi);
            }
            expect (played.getPlayback().waitUntilReady (3000));
            played.getPlayback().play();
            float worst = 0.0f, g = 0.0f;
            const auto step = 1.0f / (float) (int) (MixPlayback::kFadeMs * 0.001 * kRate);
            for (int block = 0; block < 60; ++block)
            {
                a.clear();
                b.clear();
                for (int i = 0; i < kBlock; ++i)
                {
                    const auto at = (juce::int64) block * kBlock + i;
                    g = juce::jmin (1.0f, g + step);    // the playback's own 5 ms fade-in
                    a.setSample (0, i, signal (0, at) * g);
                }
                live.processBlock (a, midi);
                played.processBlock (b, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < kBlock; ++i)
                        worst = juce::jmax (worst, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
            }
            expectLessThan (worst, 1.0e-6f, "the main, sample for sample");

            juce::MemoryBlock state;
            played.getStateInformation (state);
            played.setStateInformation (state.getData(), (int) state.getSize());
            expect (! played.anyInputOnPlayback(), "a loaded mix starts all Live");
        }

        beginTest ("Export: the main mix of a region, sample for sample as the console plays it live; the inserts' latency taken out");
        {
            for (const auto withLimiter : { false, true })
            {
                RKMixProcessor live, played;
                for (auto* m : { &live, &played })
                {
                    setParam (m->getSettings().param (mix::channelId (0, "eq_on")), 1.0f);
                    setParam (m->getSettings().param (mix::channelId (0, "eq_lmid_gain")), 6.0f);
                    setParam (m->getSettings().param (mix::channelId (0, "comp_on")), 1.0f);
                    if (withLimiter)
                        m->getRack().insert ({ InsertRack::Strip::main, 0, 0 }, "RKLimit", 2);
                    m->prepareToPlay (kRate, kBlock);
                }
                expect (played.getPlayback().load (MixPlayback::scan (folder), kRate).wasOk());
                const auto latency = withLimiter ? played.getRack().latencies().main : 0;

                // Live: the files' signals straight into inputs 1, 3 and 4 from the start.
                const int length = 48000 + latency + kBlock;
                juce::AudioBuffer<float> heard (2, length), a (32, kBlock);
                juce::MidiBuffer midi;
                for (int at = 0; at + kBlock <= length; at += kBlock)
                {
                    a.clear();
                    for (int i = 0; i < kBlock; ++i)
                        for (const auto in : { 0, 2, 3 })
                            a.setSample (in, i, signal (in, at + i));
                    live.processBlock (a, midi);
                    for (int side = 0; side < 2; ++side)
                        heard.copyFrom (side, at, a, side, 0, kBlock);
                }

                // Exported: Scene 2 to Scene 3 (the inputs are still Live: the export plays the files anyway).
                auto mixCopy = played.exportMix();
                expect ((mixCopy.inserts != nullptr) == withLimiter);
                const auto file = root.getChildFile (withLimiter ? "limited.wav" : "plain.wav");
                const MixExport::Region region { 24000, 48000, "Scene 2" };
                const auto r = MixExport::render (mixCopy, played.getPlayback().getRecording(), region, file, true);
                expect (r.wasOk(), r.getErrorMessage());

                juce::AudioFormatManager formats;
                formats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
                expect (reader != nullptr);
                if (reader == nullptr)
                    continue;
                expectEquals ((int) reader->numChannels, 2);
                expect (reader->lengthInSamples >= 24000 && reader->lengthInSamples <= 24000 + (int) (MixExport::kMaxTailSeconds * kRate));
                juce::AudioBuffer<float> got (2, 24000);
                reader->read (&got, 0, 24000, 0, true, true);
                float worst = 0.0f, loudest = 0.0f;
                for (int side = 0; side < 2; ++side)
                    for (int k = 0; k < 24000; ++k)
                    {
                        worst = juce::jmax (worst, std::abs (got.getSample (side, k) - heard.getSample (side, 24000 + k + latency)));
                        loudest = juce::jmax (loudest, std::abs (got.getSample (side, k)));
                    }
                expectGreaterThan (loudest, 0.1f, "there is a mix");
                expectLessThan (worst, 1.0e-5f, withLimiter ? "with RKLimit on the main, its latency taken out" : "the main, sample for sample");
            }
        }

        beginTest ("Export: a reverb's tail rings on (at most 3 s); cancelled leaves no file; each region its own file, never over another");
        {
            RKMixProcessor m;
            m.getRack().insert ({ InsertRack::Strip::main, 0, 0 }, "RKVerb", 2);
            m.prepareToPlay (kRate, kBlock);
            expect (m.getPlayback().load (MixPlayback::scan (folder), kRate).wasOk());
            const auto& recording = m.getPlayback().getRecording();

            auto withVerb = m.exportMix();
            const auto tailFile = root.getChildFile ("tail.wav");
            expect (MixExport::render (withVerb, recording, { 24000, 48000, "Tail" }, tailFile, false).wasOk());
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            {
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (tailFile));
                expect (reader != nullptr && reader->lengthInSamples > 24000 + 4800, "a tail");
                expect (reader != nullptr && reader->lengthInSamples <= 24000 + (int) (MixExport::kMaxTailSeconds * kRate));
                expect (reader != nullptr && reader->bitsPerSample == 24);
            }

            const auto cancelled = root.getChildFile ("cancelled.wav");
            expect (MixExport::render (withVerb, recording, { 24000, 48000, "x" }, cancelled, false, [] (float p) { return p < 0.3f; }).failed());
            expect (! cancelled.exists(), "nothing left behind");

            const auto exports = root.getChildFile ("Exports");
            MixExport ex;
            for (int round = 0; round < 2; ++round)
            {
                expect (ex.start (m.exportMix(), recording, { { 0, 24000, "Scene 1" }, { 24000, 48000, "Scene 2" } }, exports, false));
                for (int n = 0; n < 1000 && ex.isRunning(); ++n)
                    juce::Thread::sleep (10);
                expect (! ex.isRunning());
                expectEquals (ex.getStatus(), juce::String ("Exported 2 files"));
            }
            expect (exports.getChildFile ("Scene 1.wav").existsAsFile() && exports.getChildFile ("Scene 2.wav").existsAsFile());
            expect (exports.getChildFile ("Scene 2 (2).wav").existsAsFile(), "a second export doesn't overwrite the first");
        }

        root.deleteRecursively();
    }

    /** The ring ready at the position, nothing read yet. */
    void in_ready (Player& player)
    {
        for (int n = 0; n < 20 && player.p.needsAudio(); ++n)
        {
            player.in.clear();
            player.p.process (player.in.getArrayOfWritePointers(), player.all, kBlock);
        }
        expect (player.p.waitUntilReady (3000), "ready");
    }

    static void setParam (juce::RangedAudioParameter& p, float v) { p.setValueNotifyingHost (p.convertTo0to1 (v)); }
};

static MixPlaybackTests mixPlaybackTests;
