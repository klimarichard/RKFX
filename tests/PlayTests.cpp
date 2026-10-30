#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PlayEngine.h"
#include "PlayStream.h"
#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <functional>

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 256;

    /** Writes a 32-bit float stereo WAV of @p frames made by @p sample (frame, channel). */
    juce::File writeWav (const juce::String& name, double rate, int frames, const std::function<float (int, int)>& sample)
    {
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKPlayTests").getChildFile (name + ".wav");
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::AudioBuffer<float> buffer (2, frames);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < frames; ++i)
                buffer.setSample (ch, i, sample (i, ch));

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> out (file.createOutputStream());
        auto writer = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}
                                                   .withSampleRate (rate)
                                                   .withNumChannels (2)
                                                   .withBitsPerSample (32)
                                                   .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        writer->writeFromAudioSampleBuffer (buffer, 0, frames);
        writer.reset();
        return file;
    }

    std::unique_ptr<juce::AudioFormatReader> readerFor (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        return std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file));
    }

    float noise (int i, int ch, int seed)
    {
        // Deterministic white noise, -12 dBFS-ish.
        auto x = (juce::uint32) (i * 2 + ch) * 2654435761u + (juce::uint32) seed * 40503u;
        x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15;
        return ((float) (x & 0xffff) / 32768.0f - 1.0f) * 0.25f;
    }

    /** Runs the engine for @p frames, appending the output. */
    void run (PlayEngine& engine, juce::AudioBuffer<float>& out, int frames)
    {
        const auto start = out.getNumSamples();
        out.setSize (2, start + frames, true, true);
        juce::AudioBuffer<float> block (2, kBlock);
        for (int done = 0; done < frames; done += kBlock)
        {
            const auto n = juce::jmin (kBlock, frames - done);
            block.clear();
            engine.process (block.getArrayOfWritePointers(), n);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, start + done, block, ch, 0, n);
        }
    }

    double rms (const juce::AudioBuffer<float>& b, int ch, int start, int length)
    {
        double sum = 0.0;
        for (int i = start; i < start + length; ++i)
            sum += (double) b.getSample (ch, i) * b.getSample (ch, i);
        return std::sqrt (sum / length);
    }
}

class PlayTests : public juce::UnitTest
{
public:
    PlayTests() : juce::UnitTest ("RKPlay") {}

    void runTest() override
    {
        beginTest ("A file at the device rate streams bit-exact");
        {
            const auto file = writeWav ("exact", kRate, 100000, [] (int i, int ch) { return noise (i, ch, 1); });
            PlayStream stream;
            stream.prepare (kRate);
            stream.open (readerFor (file), 0.0);
            stream.fillNow();
            expect (stream.isSourceDone());
            expectEquals (stream.available(), 100000);

            juce::AudioBuffer<float> got (2, 100000);
            expectEquals (stream.read (got.getArrayOfWritePointers(), 0, 100000), 100000);
            float worst = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 100000; ++i)
                    worst = juce::jmax (worst, std::abs (got.getSample (ch, i) - noise (i, ch, 1)));
            expectEquals (worst, 0.0f);
        }

        beginTest ("Starting part-way, and a mono file on both sides");
        {
            const auto file = writeWav ("offset", kRate, 96000, [] (int i, int) { return (float) i / 96000.0f; });
            PlayStream stream;
            stream.prepare (kRate);
            stream.open (readerFor (file), 1.0);
            stream.fillNow();
            juce::AudioBuffer<float> got (2, 4);
            stream.read (got.getArrayOfWritePointers(), 0, 4);
            expectWithinAbsoluteError (got.getSample (0, 0), 48000.0f / 96000.0f, 1.0e-6f);
            expectWithinAbsoluteError (stream.getPositionSeconds(), 1.0 + 4.0 / kRate, 1.0e-9);
        }

        beginTest ("A 44.1 kHz file plays clean and in tune at 48 kHz");
        {
            const auto hz = 1000.0, sourceRate = 44100.0;
            const auto file = writeWav ("sine441", sourceRate, (int) (sourceRate * 3), [=] (int i, int)
                                        { return 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sourceRate); });
            PlayStream stream;
            stream.prepare (kRate);
            stream.open (readerFor (file), 0.0);
            stream.fillNow();
            expectWithinAbsoluteError (stream.available(), (int) (kRate * 3), 300, "the length, give or take the filter");

            juce::AudioBuffer<float> got (2, (int) kRate);
            stream.read (got.getArrayOfWritePointers(), 0, (int) kRate);

            // Fit a sine at exactly 1 kHz to the middle; what's left is the resampler's error.
            const int start = 4800, length = 38400;
            double s = 0.0, c = 0.0;
            for (int i = start; i < start + length; ++i)
            {
                const auto w = juce::MathConstants<double>::twoPi * hz * i / kRate;
                s += got.getSample (0, i) * std::sin (w);
                c += got.getSample (0, i) * std::cos (w);
            }
            s *= 2.0 / length;
            c *= 2.0 / length;
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (std::sqrt (s * s + c * c) / 0.5), 0.0, 0.1, "the level, within 0.1 dB");
            double error = 0.0;
            for (int i = start; i < start + length; ++i)
            {
                const auto w = juce::MathConstants<double>::twoPi * hz * i / kRate;
                const auto e = got.getSample (0, i) - (s * std::sin (w) + c * std::cos (w));
                error += e * e;
            }
            const auto errorDb = juce::Decibels::gainToDecibels (std::sqrt (error / length) / (0.5 / std::sqrt (2.0)));
            expectLessThan (errorDb, -80.0, "resampling error " + juce::String (errorDb, 1) + " dB");
        }

        beginTest ("Back to back with no crossfade: gapless, sample for sample");
        {
            const auto a = writeWav ("dcA", kRate, 24000, [] (int, int) { return 0.5f; });
            const auto b = writeWav ("dcB", kRate, 24000, [] (int, int) { return 0.25f; });
            PlayStream streams[2];
            PlayEngine engine;
            engine.prepare (kRate, kBlock);
            for (int i = 0; i < 2; ++i)
            {
                streams[i].prepare (kRate);
                streams[i].open (readerFor (i == 0 ? a : b), 0.0);
                streams[i].fillNow();
                engine.setStream (i, &streams[i]);
            }
            engine.start (0, 0.0, 1.0f);
            engine.arm (1, 0.0, 1.0f);
            juce::AudioBuffer<float> out (2, 0);
            run (engine, out, 48000 + 1000);
            expectEquals (out.getSample (0, 0), 0.5f);
            expectEquals (out.getSample (0, 23999), 0.5f);
            expectEquals (out.getSample (0, 24000), 0.25f, "B's first sample right after A's last");
            expectEquals (out.getSample (0, 47999), 0.25f);
            expectEquals (out.getSample (0, 48000), 0.0f, "then silence: nothing armed after B");

            PlayEngine::Event e;
            bool advanced = false, ended = false;
            while (engine.popEvent (e))
            {
                advanced = advanced || (e.type == PlayEngine::Event::Type::advanced && e.slot == 1);
                ended = ended || (e.type == PlayEngine::Event::Type::ended && e.slot == 1);
            }
            expect (advanced && ended);
        }

        beginTest ("A crossfade keeps the power even, and starts on time");
        {
            const auto a = writeWav ("noiseA", kRate, 4 * 48000, [] (int i, int ch) { return noise (i, ch, 2); });
            const auto b = writeWav ("noiseB", kRate, 4 * 48000, [] (int i, int ch) { return noise (i, ch, 3); });
            PlayStream streams[2];
            PlayEngine engine;
            engine.prepare (kRate, kBlock);
            for (int i = 0; i < 2; ++i)
            {
                streams[i].prepare (kRate);
                streams[i].open (readerFor (i == 0 ? a : b), 0.0);
                streams[i].fillNow();
                engine.setStream (i, &streams[i]);
            }
            engine.start (0, 0.0, 1.0f);
            engine.arm (1, 2.0, 1.0f);
            juce::AudioBuffer<float> out (2, 0);
            run (engine, out, 6 * 48000);

            const auto reference = rms (out, 0, 0, 48000);
            double worst = 0.0;
            for (int w = 0; w < 12; ++w)   // 2 s to 5 s, quarter-second windows
            {
                const auto level = rms (out, 0, 96000 + w * 12000, 12000);
                worst = juce::jmax (worst, std::abs (juce::Decibels::gainToDecibels (level / reference)));
            }
            expectLessThan (worst, 0.5, "power through the crossfade within 0.5 dB");
            expectEquals (out.getSample (0, 96000 - 1), noise (96000 - 1, 0, 2), "A alone until 2 s before its end");
        }

        beginTest ("Stopping, pausing and resuming never click");
        {
            const auto file = writeWav ("sine", kRate, 5 * 48000, [] (int i, int)
                                        { return 0.8f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / kRate); });
            PlayStream stream;
            stream.prepare (kRate);
            stream.open (readerFor (file), 0.0);
            stream.fillNow();
            PlayEngine engine;
            engine.prepare (kRate, kBlock);
            engine.setStream (0, &stream);
            engine.start (0, 0.01, 1.0f);
            juce::AudioBuffer<float> out (2, 0);
            run (engine, out, 12000);
            engine.pause (0.5);
            run (engine, out, 30000);
            expect (engine.isPaused());
            expectEquals ((float) rms (out, 0, out.getNumSamples() - 4800, 4800), 0.0f, "paused: silence");
            engine.resume (0.5);
            run (engine, out, 30000);
            engine.fadeOutAll (0.1);
            run (engine, out, 12000);

            // The largest step between samples is the sine's own (0.8 * 2π * 1000 / 48000).
            float step = 0.0f;
            for (int i = 1; i < out.getNumSamples(); ++i)
                step = juce::jmax (step, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
            expectLessThan (step, 0.8f * (float) (juce::MathConstants<double>::twoPi * 1000.0 / kRate) * 1.02f);
            expectEquals (out.getSample (0, out.getNumSamples() - 1), 0.0f, "stopped");
        }

        beginTest ("A disk that can't keep up: a fade and the next song, never a wait");
        {
            const auto file = writeWav ("slow", kRate, 10 * 48000, [] (int i, int ch) { return noise (i, ch, 4); });
            PlayStream stream;
            stream.prepare (kRate);
            stream.open (readerFor (file), 0.0);
            stream.prefill (0.5);   // and no disk thread to read the rest
            PlayEngine engine;
            engine.prepare (kRate, kBlock);
            engine.setStream (0, &stream);
            engine.start (0, 0.0, 1.0f);
            juce::AudioBuffer<float> out (2, 0);
            run (engine, out, 48000);

            bool underrun = false, ended = false;
            PlayEngine::Event e;
            while (engine.popEvent (e))
            {
                underrun = underrun || e.type == PlayEngine::Event::Type::underrun;
                ended = ended || e.type == PlayEngine::Event::Type::ended;
            }
            expect (underrun && ended);
            float step = 0.0f;
            for (int i = 1; i < out.getNumSamples(); ++i)
                step = juce::jmax (step, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
            expectLessThan (step, 0.51f, "faded, not cut (noise steps are up to 0.5)");
        }

        beginTest ("The player: plays a list in order, crossfading; the show is kept");
        {
            juce::Array<juce::File> files;
            for (int s = 0; s < 3; ++s)
                files.add (writeWav ("Artist - Song " + juce::String (s + 1), kRate, 48000, [s] (int, int) { return 0.1f * (float) (s + 1); }));

            RKPlayProcessor p;
            p.setPlayConfigDetails (2, 2, kRate, kBlock);
            p.prepareToPlay (kRate, kBlock);
            p.addFiles (files);
            p.setCrossfade (0.0);
            expectEquals ((int) p.getShow().songs.size(), 3);
            expectEquals (p.getShow().songs[1].artist, juce::String ("Artist"));
            expectEquals (p.getShow().songs[1].title, juce::String ("Song 2"));

            p.play();
            juce::AudioBuffer<float> buffer (2, kBlock);
            juce::MidiBuffer midi;
            std::vector<float> heard;
            for (int b = 0; b < (int) (2.5 * kRate) / kBlock; ++b)
            {
                buffer.clear();
                p.processBlock (buffer, midi);
                heard.push_back (buffer.getSample (0, kBlock / 2));
                p.update();
            }
            expectWithinAbsoluteError (heard[20], 0.1f, 1.0e-6f, "song 1");
            expectWithinAbsoluteError (heard[(int) (1.5 * kRate) / kBlock], 0.2f, 1.0e-6f, "song 2");
            expectEquals (p.getCurrentIndex(), 2);
            expect (p.getState() == RKPlayProcessor::State::playing);

            juce::MemoryBlock state;
            p.getStateInformation (state);
            RKPlayProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals ((int) q.getShow().songs.size(), 3);
            expectEquals (q.getShow().crossfadeSeconds, 0.0);
            expect (q.getShow().songs[2].file == files[2]);
            expect (q.getState() == RKPlayProcessor::State::stopped, "a loaded show never starts playing by itself");
        }

        beginTest ("A missing file is marked and skipped");
        {
            juce::Array<juce::File> files;
            files.add (writeWav ("One", kRate, 48000, [] (int, int) { return 0.3f; }));
            files.add (writeWav ("Gone", kRate, 48000, [] (int, int) { return 0.3f; }));
            files.add (writeWav ("Three", kRate, 48000, [] (int, int) { return 0.4f; }));
            RKPlayProcessor p;
            p.setPlayConfigDetails (2, 2, kRate, kBlock);
            p.prepareToPlay (kRate, kBlock);
            p.addFiles (files);
            files[1].deleteFile();

            juce::MemoryBlock state;
            p.getStateInformation (state);
            RKPlayProcessor q;
            q.setPlayConfigDetails (2, 2, kRate, kBlock);
            q.prepareToPlay (kRate, kBlock);
            q.setStateInformation (state.getData(), (int) state.getSize());
            expect (q.getShow().songs[1].status == PlaySong::Status::missing);
            expect (q.getNote().contains ("1 file"));
            q.play();
            expectEquals (q.getNextIndex(), 2, "the next playable song");
        }
    }
};

static PlayTests playTests;

int main (int, char**)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* r = runner.getResult (i))
            failures += r->failures;
    std::printf (failures > 0 ? "RKPlayTests: %d test failure(s)\n" : "RKPlayTests: all tests passed\n", failures);
    return failures > 0 ? 1 : 0;
}
