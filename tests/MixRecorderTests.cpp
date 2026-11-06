#include "../plugins/RKMix/MixRecorder.h"

/** RKMix's recorder (MixRecorder): the files come back sample-exact, all the
    same length and aligned; a pair is one stereo file; the main and a bus;
    markers; a disk that can't keep up drops whole blocks (counted, the files
    still aligned) and never blocks the audio; a split take loses no sample. */
class MixRecorderTests : public juce::UnitTest
{
public:
    MixRecorderTests() : juce::UnitTest ("RKMix recorder") {}

    using Source = MixRecorder::Source;

    static constexpr double kRate = 48000.0;
    static constexpr int kBlock = 64;

    /** Input n's signal: a different value at every sample (never repeating between inputs). */
    static float signal (int input, juce::int64 i)
    {
        return 0.9f * (float) std::sin (0.001 * (double) (i + 1) * (1.0 + 0.37 * input) + input);
    }

    /** Runs @p blocks blocks: inputs 0..n-1, the main = (input 0, -input 0), bus 2 = input 3 (mono). */
    static void run (MixRecorder& r, juce::int64& position, int blocks, int numInputs = 4)
    {
        std::vector<std::vector<float>> in ((size_t) numInputs, std::vector<float> (kBlock));
        std::vector<float> neg (kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < numInputs; ++n)
                for (int i = 0; i < kBlock; ++i)
                    in[(size_t) n][(size_t) i] = signal (n, position + i);
            for (int i = 0; i < kBlock; ++i)
                neg[(size_t) i] = -in[0][(size_t) i];

            r.beginBlock (kBlock);
            for (int n = 0; n < numInputs; ++n)
                r.pushInput (n, in[(size_t) n].data());
            r.pushMain (in[0].data(), neg.data());
            r.pushBus (2, in[3].data(), nullptr);
            r.endBlock();
            position += kBlock;
        }
    }

    static MixRecorder::Take take (const juce::File& folder, bool asFloat)
    {
        MixRecorder::Take t;
        t.folder = folder;
        t.sampleRate = kRate;
        t.asFloat = asFloat;
        t.tracks = {
            { "01 Vox.wav", { { Source::input, 0, 0 } } },
            { "02-03 Keys.wav", { { Source::input, 1, 0 }, { Source::input, 2, 0 } } },
            { "Main.wav", { { Source::main, 0, 0 }, { Source::main, 0, 1 } } },
            { "Aux 03 IEM.wav", { { Source::bus, 2, 0 } } },
        };
        auto* info = new juce::DynamicObject();
        info->setProperty ("mix", "Test");
        t.info = juce::var (info);
        return t;
    }

    static std::unique_ptr<juce::AudioFormatReader> open (const juce::File& f)
    {
        juce::WavAudioFormat wav;
        return std::unique_ptr<juce::AudioFormatReader> (wav.createReaderFor (f.createInputStream().release(), true));
    }

    void runTest() override
    {
        juce::TemporaryFile temp;
        const auto root = temp.getFile();
        root.createDirectory();

        for (const auto asFloat : { true, false })
        {
            beginTest (juce::String ("Files come back sample-exact (") + (asFloat ? "float" : "24-bit") + "), the same length, aligned; session.json");
            const auto folder = root.getChildFile (asFloat ? "float" : "int");
            MixRecorder r;
            juce::int64 position = 0;
            run (r, position, 10);                                  // before Record: nothing
            expect (r.start (take (folder, asFloat)).wasOk());
            const auto first = position;
            run (r, position, 400);                                 // 25600 samples, over several disk passes
            r.addMarker ("Scene 3 Verse");
            run (r, position, 100);
            r.stop();
            run (r, position, 10);                                  // after Stop: nothing
            const auto length = 500 * kBlock;

            const auto tolerance = asFloat ? 0.0f : 1.0f / 8388608.0f;
            for (const auto& [name, sourceInput, channels] : std::vector<std::tuple<juce::String, int, int>> {
                     { "01 Vox.wav", 0, 1 }, { "02-03 Keys.wav", 1, 2 }, { "Main.wav", 0, 2 }, { "Aux 03 IEM.wav", 3, 1 } })
            {
                auto reader = open (folder.getChildFile (name));
                expect (reader != nullptr, name);
                if (reader == nullptr)
                    continue;
                expectEquals ((int) reader->lengthInSamples, length, name + ": every file the same length");
                expectEquals ((int) reader->numChannels, channels, name);
                juce::AudioBuffer<float> b (channels, length);
                reader->read (&b, 0, length, 0, true, true);
                float worst = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                    for (int i = 0; i < length; ++i)
                    {
                        auto expected = signal (sourceInput + (name.startsWith ("02") ? ch : 0), first + i);
                        if (name == "Main.wav" && ch == 1)
                            expected = -expected;
                        worst = juce::jmax (worst, std::abs (b.getSample (ch, i) - expected));
                    }
                expect (worst <= tolerance, name + ": worst " + juce::String (worst));
            }

            const auto session = juce::JSON::parse (folder.getChildFile ("session.json"));
            expectEquals ((double) session["sampleRate"], kRate);
            expectEquals ((int) session["length"], length);
            expectEquals (session["mix"].toString(), juce::String ("Test"));
            expectEquals ((int) session["files"]["02-03 Keys.wav"][1], 3, "a pair's right side came from input 3");
            expectEquals (session["files"]["Main.wav"][1].toString(), juce::String ("main R"));
            expectEquals (session["markers"].size(), 1);
            expectEquals ((int) session["markers"][0]["at"], 400 * kBlock, "the marker where it was set");
            expectEquals (session["markers"][0]["label"].toString(), juce::String ("Scene 3 Verse"));
            expectEquals ((int) session["dropouts"]["count"], 0);
        }

        beginTest ("A disk that can't keep up: whole blocks dropped, counted, the files still aligned; the audio never waits");
        {
            const auto folder = root.getChildFile ("stall");
            MixRecorder r;
            juce::int64 position = 0;
            expect (r.start (take (folder, false)).wasOk());
            r.stallDiskForTesting (true);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            run (r, position, (int) (6.0 * kRate / kBlock));       // 6 s into 4 s FIFOs
            const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
            expectGreaterThan (r.getDropouts(), 0, "dropouts counted");
            expectLessThan (ms, 1000.0, "the audio thread never waited for the disk");
            r.stallDiskForTesting (false);
            run (r, position, 10);
            const auto recorded = r.getRecordedSamples();
            r.stop();
            juce::int64 lengths[2] {};
            int n = 0;
            for (const auto* name : { "01 Vox.wav", "Main.wav" })
                if (auto reader = open (folder.getChildFile (name)))
                    lengths[n++] = reader->lengthInSamples;
            expectEquals (lengths[0], lengths[1], "aligned");
            expectEquals (lengths[0], recorded);
            const auto session = juce::JSON::parse (folder.getChildFile ("session.json"));
            expectEquals ((int) session["dropouts"]["count"], r.getDropouts());
            expectGreaterThan (session["dropouts"]["at"].size(), 0, "where");
        }

        beginTest ("A split take: the next one starts on the next block, no sample lost between them");
        {
            MixRecorder r;
            juce::int64 position = 0;
            expect (r.start (take (root.getChildFile ("a"), false)).wasOk());
            run (r, position, 30);
            const auto split = position;
            expect (r.start (take (root.getChildFile ("b"), false)).wasOk());
            run (r, position, 20);
            r.stop();
            auto a = open (root.getChildFile ("a/01 Vox.wav")), b = open (root.getChildFile ("b/01 Vox.wav"));
            expect (a != nullptr && b != nullptr);
            if (a != nullptr && b != nullptr)
            {
                expectEquals ((int) a->lengthInSamples, 30 * kBlock);
                expectEquals ((int) b->lengthInSamples, 20 * kBlock);
                juce::AudioBuffer<float> first (1, 1);
                b->read (&first, 0, 1, 0, true, false);
                expectWithinAbsoluteError (first.getSample (0, 0), signal (0, split), 1.0f / 8388608.0f, "b starts where a ended");
            }
        }

        beginTest ("Nothing to record, or a folder that can't be made: refused");
        {
            MixRecorder r;
            MixRecorder::Take t;
            t.folder = root.getChildFile ("empty");
            expect (r.start (t).failed());
            auto locked = root.getChildFile ("locked");
            locked.createDirectory();
            locked.setReadOnly (true);
            expect (r.start (take (locked.getChildFile ("x"), false)).failed());
            expect (! r.isRecording());
            locked.setReadOnly (false);
        }

        root.deleteRecursively();
    }
};

static MixRecorderTests mixRecorderTests;
