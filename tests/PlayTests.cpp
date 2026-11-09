#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PlayAnalysis.h"
#include "PlayEngine.h"
#include "PlayRemote.h"
#include "PlayRig.h"
#include "dsp/Loudness.h"
#include "PlayStream.h"
#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <random>

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


    /** A folder of short songs, each a steady level of its own (song n plays 0.01 * (first + n + 1)). */
    juce::Array<juce::File> songsIn (const juce::String& folder, int count, double seconds = 0.5, int first = 0)
    {
        juce::Array<juce::File> files;
        for (int n = 0; n < count; ++n)
            files.add (writeWav (folder + "/Song " + juce::String (n + 1).paddedLeft ('0', 2), kRate, (int) (seconds * kRate),
                                 [n, first] (int, int) { return 0.01f * (float) (first + n + 1); }));
        return files;
    }

    /** A web remote page without a network: what it is sent, kept as JSON. */
    struct FakeConnection : public rkfx::RemoteServer::Connection
    {
        explicit FakeConnection (const juce::String& from = "10.0.0.5") { path = "/ws/play"; address = from; }
        bool send (const juce::String& text) override { sent.add (text); return true; }
        void close() override { closed = true; }
        juce::StringArray sent;
        bool closed = false;

        /** The latest message of @p type sent (void if none). */
        juce::var last (const juce::String& type) const
        {
            juce::var found;
            for (const auto& text : sent)
                if (const auto v = juce::JSON::parse (text); v["type"].toString() == type)
                    found = v;
            return found;
        }
        int count (const juce::String& type) const
        {
            int n = 0;
            for (const auto& text : sent)
                n += juce::JSON::parse (text)["type"].toString() == type ? 1 : 0;
            return n;
        }
    };

    /** A player for tests: plain gain, no smart crossfade, streams filled on the calling thread. */
    struct TestPlayer : public RKPlayProcessor
    {
        TestPlayer()
        {
            useAnalysisCache (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKPlayTests/analysis.json"));
            setSynchronousDiskForTesting (true);
            setPlayConfigDetails (2, 2, kRate, kBlock);
            prepareToPlay (kRate, kBlock);
            setLevelMatch (false);
            setSmartCrossfade (false);
        }

        /** Runs for @p seconds; returns the songs heard, in turn (their numbers, 0 = silence),
            each once it has been steady for two blocks (past its fade-in). */
        std::vector<int> runFor (double seconds)
        {
            std::vector<int> heard;
            juce::AudioBuffer<float> buffer (2, kBlock);
            juce::MidiBuffer midi;
            int previous = -1;
            for (int b = 0; b < (int) (seconds * kRate) / kBlock; ++b)
            {
                buffer.clear();
                processBlock (buffer, midi);
                update();
                const auto n = juce::roundToInt (buffer.getSample (0, kBlock - 1) / 0.01f);
                if (n == previous && (heard.empty() || heard.back() != n))
                    heard.push_back (n);
                previous = n;
            }
            return heard;
        }
    };

    juce::String sequence (const std::vector<int>& v)
    {
        juce::StringArray s;
        for (auto n : v)
            s.add (juce::String (n));
        return s.joinIntoString (" ");
    }

    juce::File tempCache()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKPlayTests").getChildFile ("analysis.json");
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
            p.useAnalysisCache (tempCache());
            p.setSynchronousDiskForTesting (true);
            p.setPlayConfigDetails (2, 2, kRate, kBlock);
            p.prepareToPlay (kRate, kBlock);
            p.addFiles (files);
            p.setCrossfade (0.0);
            expectEquals ((int) p.getShow().playlists[0].songs.size(), 3);
            expectEquals (p.getShow().playlists[0].songs[1].artist, juce::String ("Artist"));
            expectEquals (p.getShow().playlists[0].songs[1].title, juce::String ("Song 2"));

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
            q.useAnalysisCache (tempCache());
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals ((int) q.getShow().playlists[0].songs.size(), 3);
            expectEquals (q.getShow().playlists[0].crossfadeSeconds, 0.0);
            expect (q.getShow().playlists[0].songs[2].file == files[2]);
            expect (q.getState() == RKPlayProcessor::State::stopped, "a loaded show never starts playing by itself");
        }

        beginTest ("A missing file is marked and skipped");
        {
            juce::Array<juce::File> files;
            files.add (writeWav ("One", kRate, 48000, [] (int, int) { return 0.3f; }));
            files.add (writeWav ("Gone", kRate, 48000, [] (int, int) { return 0.3f; }));
            files.add (writeWav ("Three", kRate, 48000, [] (int, int) { return 0.4f; }));
            RKPlayProcessor p;
            p.useAnalysisCache (tempCache());
            p.setSynchronousDiskForTesting (true);
            p.setPlayConfigDetails (2, 2, kRate, kBlock);
            p.prepareToPlay (kRate, kBlock);
            p.addFiles (files);
            files[1].deleteFile();

            juce::MemoryBlock state;
            p.getStateInformation (state);
            RKPlayProcessor q;
            q.useAnalysisCache (tempCache());
            q.setPlayConfigDetails (2, 2, kRate, kBlock);
            q.prepareToPlay (kRate, kBlock);
            q.setStateInformation (state.getData(), (int) state.getSize());
            expect (q.getShow().playlists[0].songs[1].status == PlaySong::Status::missing);
            expect (q.getNote().contains ("1 file"));
            q.play();
            expectEquals (q.peekNext().index, 2, "the next playable song");
        }

        //==============================================================================
        // Phase 2: analysis and loudness

        beginTest ("Loudness: the EBU reference, a 1 kHz sine at -23 dBFS reads -23.0 LUFS");
        {
            for (const auto rate : { 48000.0, 44100.0 })
            {
                rkfx::LoudnessMeter meter;
                meter.prepare (rate, 2);
                const auto amplitude = juce::Decibels::decibelsToGain (-23.0);
                juce::AudioBuffer<float> b (2, (int) rate * 20);
                for (int i = 0; i < b.getNumSamples(); ++i)
                    for (int ch = 0; ch < 2; ++ch)
                        b.setSample (ch, i, (float) (amplitude * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / rate)));
                meter.process (b.getArrayOfReadPointers(), b.getNumSamples());
                expectWithinAbsoluteError (meter.getIntegrated(), -23.0, 0.1, juce::String (rate));
            }
        }

        beginTest ("Loudness: gating ignores silence; true peak finds the peak between samples");
        {
            rkfx::LoudnessMeter meter;
            meter.prepare (kRate, 2);
            const auto amplitude = juce::Decibels::decibelsToGain (-23.0);
            juce::AudioBuffer<float> b (2, (int) kRate * 20);
            b.clear();
            for (int i = 0; i < (int) kRate * 10; ++i)   // 10 s of tone, then 10 s of silence
                for (int ch = 0; ch < 2; ++ch)
                    b.setSample (ch, i, (float) (amplitude * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / kRate)));
            meter.process (b.getArrayOfReadPointers(), b.getNumSamples());
            expectWithinAbsoluteError (meter.getIntegrated(), -23.0, 0.1, "the silence is gated out");

            // A sine at a quarter of the rate, sampled 45 degrees off its peaks: samples at 0.707.
            rkfx::LoudnessMeter peaks;
            peaks.prepare (kRate, 2);
            for (int i = 0; i < 48000; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    b.setSample (ch, i, (float) std::sin (juce::MathConstants<double>::halfPi * i + juce::MathConstants<double>::pi / 4.0));
            peaks.process (b.getArrayOfReadPointers(), 48000);
            expectWithinAbsoluteError (peaks.getSamplePeak(), 0.7071f, 0.001f);
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (peaks.getTruePeak()), 0.0f, 0.3f, "true peak at 0 dBTP");
        }

        beginTest ("Analysis: music start and end skip silence; the fade-out point is where a fade has gone quiet");
        {
            // 1 s silence, 6 s noise, a 4 s fade, 2 s silence
            const auto file = writeWav ("shape", kRate, 13 * 48000, [] (int i, int ch)
            {
                const auto t = i / kRate;
                if (t < 1.0 || t >= 11.0)
                    return 0.0f;
                const auto fade = t < 7.0 ? 1.0 : 1.0 - (t - 7.0) / 4.0;
                return (float) (noise (i, ch, 5) * fade * fade * fade);
            });
            const auto a = PlayAnalysis::analyse (*readerFor (file));
            expect (a.valid);
            expectWithinAbsoluteError (a.musicStart, 0.99, 0.02);
            expectGreaterThan (a.musicEnd, 9.0);
            expectLessThan (a.musicEnd, 11.1);
            expectGreaterThan (a.fadeOutPoint, 7.5, "after the fade begins");
            expectLessThan (a.fadeOutPoint, a.musicEnd - 0.5, "well before the music's end");
            expectEquals ((int) a.overview.size(), PlayAnalysis::kOverviewPoints);
            expectEquals ((int) a.overview[10], 0, "silence at the start");
            expectEquals ((int) a.overview[300], 64, "the noise's peak, 0.25");

            const auto back = PlayAnalysis::fromVar (a.toVar());
            expectWithinAbsoluteError (back.loudness, a.loudness, 1.0e-9);
            expect (back.overview == a.overview);
        }

        beginTest ("Level: pure gain to the target; past the peak ceiling only by the lift, limited");
        {
            PlayAnalysis a;
            a.valid = true;
            PlayLevel::Settings s;   // -18 LUFS, -0.5 dBTP, lift up to 3 dB

            a.loudness = -22.0; a.truePeakDb = -6.0;
            auto l = PlayLevel::plan (a, s, 0.0);
            expectWithinAbsoluteError (l.gainDb, 4.0, 1.0e-9);
            expectEquals (l.limitDb, 0.0);

            a.loudness = -24.0; a.truePeakDb = -1.0;            // needs +6, room for +0.5
            l = PlayLevel::plan (a, s, 0.0);
            expectWithinAbsoluteError (l.gainDb, 3.5, 1.0e-9, "0.5 of room + 3 of lift");
            expectWithinAbsoluteError (l.limitDb, 3.0, 1.0e-9);

            s.liftDb = 0.0;
            l = PlayLevel::plan (a, s, 0.0);
            expectWithinAbsoluteError (l.gainDb, 0.5, 1.0e-9, "lift off: gain only");
            expectEquals (l.limitDb, 0.0);

            a.loudness = -18.02; a.truePeakDb = -3.0;           // the archive: already there
            l = PlayLevel::plan (a, s, 0.0);
            expectEquals (l.gainDb, 0.0, "bit-exact");

            s.match = false;
            l = PlayLevel::plan (a, s, -2.0);
            expectEquals (l.gainDb, -2.0, "match off: the song's own gain only");
        }

        beginTest ("As mastered: no loudness matching, the peaks brought to the ceiling; gains only turn it down");
        {
            PlayAnalysis a;
            a.valid = true;
            PlayLevel::Settings s;   // -18 LUFS, -0.5 dBTP

            a.loudness = -8.0; a.truePeakDb = 0.4;               // a loud master over 0 dBTP
            auto l = PlayLevel::plan (a, s, 0.0, true);
            expectWithinAbsoluteError (l.gainDb, -0.9, 1.0e-6, "down to the ceiling, not to -18 LUFS");
            expectEquals (l.limitDb, 0.0, "never limited");

            a.loudness = -14.0; a.truePeakDb = -3.0;             // a quiet master
            l = PlayLevel::plan (a, s, 0.0, true);
            expectWithinAbsoluteError (l.gainDb, 2.5, 1.0e-6, "up to the ceiling");
            l = PlayLevel::plan (a, s, 4.0, true);
            expectWithinAbsoluteError (l.gainDb, 2.5, 1.0e-6, "a positive gain can't push past the ceiling");
            l = PlayLevel::plan (a, s, -6.0, true);
            expectWithinAbsoluteError (l.gainDb, -3.5, 1.0e-6, "a negative one turns it down");

            a.truePeakDb = -0.55f;
            expectEquals (PlayLevel::plan (a, s, 0.0, true).gainDb, 0.0, "already there: bit-exact");
            s.match = false;
            a.truePeakDb = -3.0f;
            expectWithinAbsoluteError (PlayLevel::plan (a, s, 0.0, true).gainDb, 2.5, 1.0e-6, "whatever Level match says");
            a.valid = false;
            expectEquals (PlayLevel::plan (a, s, -2.0, true).gainDb, -2.0, "not measured yet: as it is (gains down only)");

            const auto files = songsIn ("mastered", 2, 2.0);
            TestPlayer p;
            p.addFiles (0, files);
            p.setPlaylistAsMastered (0, true);
            juce::MemoryBlock state;
            p.getStateInformation (state);
            TestPlayer q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expect (q.getShow().playlists[0].asMastered, "kept by the show");

            // Measured: song 1 plays 0.01 (peak -40 dBFS), as mastered brought up to -0.5 dBTP.
            q.analyseNow();
            q.play();
            juce::AudioBuffer<float> buffer (2, kBlock);
            juce::MidiBuffer midi;
            for (int b = 0; b < 20; ++b)
            {
                buffer.clear();
                q.processBlock (buffer, midi);
                q.update();
            }
            // (A steady level starting from silence: its true peak is the step's overshoot, a little over 0.01.)
            const auto truePeak = q.getShow().playlists[0].songs[0].analysis.truePeakDb;
            expectWithinAbsoluteError (buffer.getSample (0, kBlock - 1), 0.01f * juce::Decibels::decibelsToGain (-0.5f - truePeak), 1.0e-4f,
                                       "the song's true peak at the ceiling");
        }

        beginTest ("The lift limiter holds the true peak at the ceiling, smoothly, and lines up");
        {
            PlayLimiter limiter;
            limiter.prepare (kRate, -0.5);
            juce::AudioBuffer<float> b (2, 96000);
            for (int i = 0; i < 96000; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    b.setSample (ch, i, noise (i, ch, 6) * 5.0f * (i % 48000 < 1200 ? 1.0f : 0.2f));   // a loud burst a second
            juce::AudioBuffer<float> in (b);
            limiter.process (b.getWritePointer (0), b.getWritePointer (1), 96000);

            rkfx::LoudnessMeter meter;
            meter.prepare (kRate, 2);
            meter.process (b.getArrayOfReadPointers(), 96000);
            expectLessThan (juce::Decibels::gainToDecibels (meter.getTruePeak()), -0.4f, "true peak at the ceiling");
            expectLessThan (limiter.getMostReductionDb(), -3.0f, "it did limit");

            // Long after a burst (6 release times): the input, delayed by the lookahead, untouched.
            const auto delay = limiter.getLatency();
            const auto i = 47000;
            expectWithinAbsoluteError (b.getSample (0, i + delay), in.getSample (0, i), 1.0e-3f, "within 0.5% (the release's last bit)");
        }

        beginTest ("A song plays between its music start and end, at its level");
        {
            const auto file = writeWav ("span", kRate, 3 * 48000, [] (int i, int) { return (float) i / (3 * 48000.0f) * 0.5f; });
            PlayStream stream;
            stream.prepare (kRate);
            PlayStream::Options o;
            o.startSeconds = 1.0;
            o.endSeconds = 2.0;
            o.gainDb = 6.0206;
            stream.open (readerFor (file), o);
            stream.fillNow();
            expectEquals (stream.available(), 48000, "one second");
            juce::AudioBuffer<float> got (2, 48000);
            stream.read (got.getArrayOfWritePointers(), 0, 48000);
            expectWithinAbsoluteError (got.getSample (0, 0), 2.0f * (48000.0f / (3 * 48000.0f) * 0.5f), 1.0e-5f, "doubled");
        }

        beginTest ("Smart crossfade: the next song is in by the fade-out point, not after a quiet tail");
        {
            // A: 4 s loud, then a 4 s tail 40 dB down; B: a steady tone.
            juce::Array<juce::File> files;
            files.add (writeWav ("A - Long Tail", kRate, 8 * 48000, [] (int i, int ch) { return noise (i, ch, 7) * (i < 4 * 48000 ? 1.0f : 0.01f); }));
            files.add (writeWav ("B - Tone", kRate, 8 * 48000, [] (int i, int)
                                 { return 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 100.0 * i / kRate); }));
            for (const auto smart : { true, false })
            {
                RKPlayProcessor p;
                p.useAnalysisCache (tempCache());
                p.setSynchronousDiskForTesting (true);
                p.setPlayConfigDetails (2, 2, kRate, kBlock);
                p.prepareToPlay (kRate, kBlock);
                p.addFiles (files);
                p.setLevelMatch (false);
                p.setCrossfade (1.0);
                p.setSmartCrossfade (smart);
                p.analyseNow();
                p.play();

                juce::AudioBuffer<float> buffer (2, kBlock);
                juce::MidiBuffer midi;
                for (int b = 0; b < (int) (5.5 * kRate) / kBlock; ++b)
                {
                    buffer.clear();
                    p.processBlock (buffer, midi);
                    p.update();
                }
                expectEquals (p.getCurrentIndex(), smart ? 1 : 0, smart ? "B by 5.5 s" : "still A's tail at 5.5 s");
            }
        }

        beginTest ("A start and end set by hand: faded in and out there, never a click");
        {
            const auto file = writeWav ("trim-stream", kRate, 3 * 48000, [] (int, int) { return 0.5f; });
            PlayStream stream;
            stream.prepare (kRate);
            PlayStream::Options o;
            o.startSeconds = 0.5;
            o.endSeconds = 2.5;
            o.fadeInSeconds = PlayShow::kTrimFadeIn;
            o.fadeOutSeconds = PlayShow::kTrimFadeOut;
            stream.open (readerFor (file), o);
            stream.fillNow();
            expectEquals (stream.available(), 96000, "two seconds");
            juce::AudioBuffer<float> got (2, 96000);
            stream.read (got.getArrayOfWritePointers(), 0, 96000);
            expectLessThan (std::abs (got.getSample (0, 0)), 0.01f, "fades in from silence");
            expectWithinAbsoluteError (got.getSample (0, 960), 0.5f, 1.0e-6f, "in after 10 ms");
            expectWithinAbsoluteError (got.getSample (0, 48000 - 1), 0.5f, 1.0e-3f, "untouched until a second before the end");
            expectWithinAbsoluteError (got.getSample (0, 96000 - 24000), 0.25f, 1.0e-3f, "half way down half a second before");
            expectLessThan (std::abs (got.getSample (1, 96000 - 1)), 1.0e-4f, "silent by the end");
            float step = 0.0f;
            for (int i = 1; i < 96000; ++i)
                step = juce::jmax (step, std::abs (got.getSample (0, i) - got.getSample (0, i - 1)));
            expectLessThan (step, 0.01f, "no step anywhere");
        }

        beginTest ("A trim wins over the analysis, stays within the song, and is kept by the show");
        {
            PlayShow show;
            PlaySong song;
            song.lengthSeconds = 200.0;
            song.analysis.valid = true;
            song.analysis.lengthSeconds = 200.0;
            song.analysis.musicStart = 0.3;
            song.analysis.musicEnd = 199.0;
            song.analysis.fadeOutPoint = 190.0;
            auto span = show.spanOf (song);
            expectEquals (span.start, 0.3);
            expectEquals (span.endPoint, 190.0);
            expect (! span.handStart && ! span.handEnd);

            song.startSeconds = 12.0;
            song.endSeconds = 150.0;
            span = show.spanOf (song);
            expectEquals (span.start, 12.0);
            expectEquals (span.end, 150.0);
            expectEquals (span.endPoint, 150.0, "a hand end: over there, no lead");
            expect (span.handStart && span.handEnd);
            show.smartCrossfade = false;
            expectEquals (show.spanOf (song).start, 12.0, "with Smart crossfade off too");

            double start = 199.5, end = 50.0;
            PlayShow::clampTrim (song, start, end);
            expectEquals (start, 200.0 - PlayShow::kMinTrimmedLength, "a start leaves room for the song");
            expectEquals (end, -1.0, "an end at the file's end is none");
            start = 10.0; end = 11.0;
            PlayShow::clampTrim (song, start, end);
            expectEquals (end, 10.0 + PlayShow::kMinTrimmedLength, "at least this long");
            start = 0.0; end = 300.0;
            PlayShow::clampTrim (song, start, end);
            expect (start < 0.0 && end < 0.0, "the whole file is no trim");

            const auto files = songsIn ("trim-show", 2, 4.0);
            TestPlayer p;
            p.addFiles (0, files);
            p.setSongTrim (0, 1, 1.0, 3.0);
            p.setSongGain (0, 1, -2.0);
            TestPlayer q;
            juce::MemoryBlock state;
            p.getStateInformation (state);
            q.setStateInformation (state.getData(), (int) state.getSize());
            const auto& kept = q.getShow().playlists[0].songs[1];
            expectEquals (kept.startSeconds, 1.0);
            expectEquals (kept.endSeconds, 3.0);
            expectEquals (kept.gainDb, -2.0);
            expect (! q.getShow().playlists[0].songs[0].isTrimmed());

            // A linked folder keeps a song's trim through Rescan.
            TestPlayer r;
            r.linkPlaylist (0, files[0].getParentDirectory());
            r.setSongTrim (0, 0, 0.5, 2.5);
            r.rescan (0);
            expectEquals (r.getShow().playlists[0].songs[0].endSeconds, 2.5, "kept by its path");
        }

        beginTest ("A trimmed song plays from its start to its end, then the next comes in");
        {
            const auto files = songsIn ("trim-play", 2, 4.0);
            TestPlayer p;
            p.addFiles (0, files);
            p.setCrossfade (0.0);
            p.setSongTrim (0, 0, -1.0, 2.0);   // song 1 over at 2 s
            p.setSongTrim (0, 1, 1.5, -1.0);   // song 2 from 1.5 s
            p.play();
            p.runFor (2.3);
            expectEquals (p.getCurrentIndex(), 1, "the second song by 2.3 s");
            expectGreaterThan (p.getPositionSeconds(), 1.5, "from its start");
            expectLessThan (p.getPositionSeconds(), 2.0);

            // The playing song takes a new end at once.
            TestPlayer q;
            q.addFiles (0, files);
            q.setCrossfade (0.0);
            q.play();
            q.runFor (0.5);
            q.setSongTrim (0, 0, -1.0, 2.0);
            q.runFor (1.8);
            expectEquals (q.getCurrentIndex(), 1, "the new end, not the file's");
        }

        beginTest ("The analysis cache: found again, and missed when the file changes");
        {
            const auto cacheFile = tempCache().getSiblingFile ("cache-test.json");
            cacheFile.deleteFile();
            const auto file = writeWav ("cached", kRate, 48000, [] (int i, int ch) { return noise (i, ch, 8); });
            {
                PlayAnalysisCache cache (cacheFile);
                cache.store (file, PlayAnalysis::analyse (*readerFor (file)));
                cache.save();
            }
            PlayAnalysisCache again (cacheFile);
            PlayAnalysis found;
            expect (again.find (file, found) && found.valid);

            writeWav ("cached", kRate, 24000, [] (int i, int ch) { return noise (i, ch, 9); });
            expect (! again.find (file, found), "a changed file is measured again");
        }

        beginTest ("Measuring waits while the music needs the disk, and goes gently while it plays");
        {
            const auto file = writeWav ("paced", kRate, 4 * 48000, [] (int i, int ch) { return noise (i, ch, 10); });
            PlayAnalyser analyser;
            analyser.setPace (PlayAnalysis::hold);
            analyser.add (file);
            juce::Thread::sleep (400);
            expect (analyser.takeResults().empty(), "held: nothing measured");
            expectEquals (analyser.pending(), 1);
            analyser.setPace (PlayAnalysis::gentle);
            std::vector<PlayAnalyser::Result> results;
            for (int i = 0; i < 100 && results.empty(); ++i)
            {
                juce::Thread::sleep (30);
                results = analyser.takeResults();
            }
            expect (results.size() == 1 && results[0].analysis.valid, "measured once let go");

            // The player: full speed when nothing plays, gently while a song does.
            const auto files = songsIn ("paced-list", 2, 20.0);
            TestPlayer p;
            p.addFiles (0, files);
            p.update();
            expectEquals (p.getAnalysisPace(), (int) PlayAnalysis::fullSpeed);
            p.play();
            p.runFor (0.1);
            expectEquals (p.getAnalysisPace(), (int) PlayAnalysis::gentle, "the read-ahead is full (disk filled at once in tests)");
            expect (! p.isAnalysisHeld());
            p.stop();
            p.update();
            expectEquals (p.getAnalysisPace(), (int) PlayAnalysis::fullSpeed);
        }

        //==============================================================================
        // Phase 3: playlists, the queue, End at, links

        beginTest ("A playlist in order: repeat, stop, or go on to another playlist at its end");
        {
            const auto first = songsIn ("lists/a", 3);
            const auto second = songsIn ("lists/b", 2, 0.5, 10);
            TestPlayer p;
            p.addFiles (0, first);
            p.setCrossfade (0.0);

            p.play();
            expectEquals (sequence (p.runFor (3.4)), juce::String ("1 2 3 1 2 3 1"), "repeat (the default)");

            p.stop();
            p.runFor (0.2);
            p.setAtEnd (0, PlayPlaylist::AtEnd::stop);
            p.jumpTo (0, 0);
            expectEquals (sequence (p.runFor (2.0)), juce::String ("1 2 3 0"), "stop at the end");
            expect (p.getState() == RKPlayProcessor::State::stopped);

            const auto b = p.addPlaylist ("Party");
            p.addFiles (b, second);
            p.setPlaylistCrossfade (b, 0.0);
            p.setAtEnd (0, PlayPlaylist::AtEnd::goTo, b);
            p.setAtEnd (b, PlayPlaylist::AtEnd::stop);
            p.jumpTo (0, 0);
            expectEquals (sequence (p.runFor (3.0)), juce::String ("1 2 3 11 12 0"), "then the Party list");
            expectEquals (p.getShow().active, b, "the Party list is playing now");
        }

        beginTest ("Shuffle: every song once a round, and a new round never starts with the last song");
        {
            TestPlayer p;
            p.addFiles (0, songsIn ("shuffle", 6));
            p.setOrder (0, PlayPlaylist::Order::shuffle);
            p.setCrossfade (0.0);
            p.play();
            std::vector<int> order;
            for (int k = 0; k < 18; ++k)
            {
                order.push_back (p.getCurrent().index);
                p.runFor (0.02);
                p.next();
                p.runFor (0.02);
            }
            for (int round = 0; round < 3; ++round)
            {
                std::vector<int> r (order.begin() + round * 6, order.begin() + round * 6 + 6);
                std::sort (r.begin(), r.end());
                expect (r == std::vector<int> ({ 0, 1, 2, 3, 4, 5 }), "round " + juce::String (round + 1) + ": " + sequence (order));
            }
            expect (order[6] != order[5] && order[12] != order[11], "no repeat across rounds: " + sequence (order));
        }

        beginTest ("Shuffle: no two songs by one artist in a row, where the list allows it");
        {
            const auto listOf = [] (std::initializer_list<const char*> artists)
            {
                PlayPlaylist p;
                for (const auto* a : artists)
                {
                    PlaySong s;
                    s.artist = a;
                    s.status = PlaySong::Status::ok;
                    p.songs.push_back (s);
                }
                return p;
            };
            const auto key = [] (const PlayPlaylist& p, int i) { return p.songs[(size_t) i].artist.trim().toLowerCase(); };
            const auto isRound = [] (const PlayPlaylist& p)
            {
                auto r = p.bag;
                std::sort (r.begin(), r.end());
                for (int i = 0; i < (int) r.size(); ++i)
                    if (r[(size_t) i] != i)
                        return false;
                return r.size() == p.songs.size();
            };
            // Neighbours by one artist, among the songs that play (an artist-less song matches none).
            const auto repeats = [&key] (const PlayPlaylist& p)
            {
                int n = 0;
                juce::String last;
                for (const auto i : p.bag)
                {
                    if (! p.songs[(size_t) i].isPlayable())
                        continue;
                    n += key (p, i).isNotEmpty() && key (p, i) == last ? 1 : 0;
                    last = key (p, i);
                }
                return n;
            };

            std::mt19937 random (7);
            bool allRounds = true, apart = true, newRound = true, pinned = true, fewest = true, missing = true, caseless = true, noArtist = true;
            for (int trial = 0; trial < 500; ++trial)
            {
                // Possible: 5 + 3 + 2, after a song by the 5 (the end of the last round).
                auto p = listOf ({ "A", "A", "A", "A", "A", "B", "B", "B", "C", "C" });
                PlayShow::reshuffle (p, 0, random);
                allRounds = allRounds && isRound (p);
                apart = apart && repeats (p) == 0;
                newRound = newRound && key (p, p.bag.front()) != "a";

                // The song playing now pinned first; the rest still apart from it.
                PlayShow::reshuffle (p, -1, random, 6);
                pinned = pinned && isRound (p) && p.bag.front() == 6 && repeats (p) == 0;

                // Impossible: 7 by one artist and 3 others can't avoid 3 repeats, and has no more.
                auto crowded = listOf ({ "A", "A", "A", "A", "A", "A", "A", "B", "B", "B" });
                PlayShow::reshuffle (crowded, -1, random);
                fewest = fewest && isRound (crowded) && repeats (crowded) == 3;

                // Missing songs are skipped, so they don't count; the ones that play still alternate.
                auto gaps = listOf ({ "A", "A", "A", "B", "B", "B", "A", "A" });
                gaps.songs[6].status = gaps.songs[7].status = PlaySong::Status::missing;
                PlayShow::reshuffle (gaps, -1, random);
                missing = missing && isRound (gaps) && repeats (gaps) == 0;

                // One artist however it's written.
                auto spelled = listOf ({ "ABBA", " abba ", "Queen", "Queen" });
                PlayShow::reshuffle (spelled, -1, random);
                caseless = caseless && repeats (spelled) == 0;

                // No artists: a plain shuffle that never starts with the last song.
                auto unnamed = listOf ({ "", "", "", "" });
                PlayShow::reshuffle (unnamed, 2, random);
                noArtist = noArtist && isRound (unnamed) && unnamed.bag.front() != 2;
            }
            expect (allRounds, "every song once a round");
            expect (apart, "no two by one artist together");
            expect (newRound, "a round never starts with the artist that just played");
            expect (pinned, "the song playing now starts the round");
            expect (fewest, "too many by one artist: the fewest repeats");
            expect (missing, "missing songs don't count");
            expect (caseless, "artists match whatever their case and spaces");
            expect (noArtist, "songs without an artist shuffle freely");

            // Played: across rounds too.
            juce::Array<juce::File> files;
            const char* artists[] = { "Alpha", "Alpha", "Alpha", "Beta", "Beta", "Gamma" };
            for (int n = 0; n < 6; ++n)
                files.add (writeWav (juce::String ("artists/") + artists[n] + " - Song " + juce::String (n + 1), kRate, (int) (0.5 * kRate),
                                     [n] (int, int) { return 0.01f * (float) (n + 1); }));
            TestPlayer player;
            player.addFiles (0, files);
            player.setOrder (0, PlayPlaylist::Order::shuffle);
            player.setCrossfade (0.0);
            player.play();
            std::vector<int> order;
            for (int k = 0; k < 24; ++k)
            {
                order.push_back (player.getCurrent().index);
                player.runFor (0.02);
                player.next();
                player.runFor (0.02);
            }
            const auto& songs = player.getShow().playlists[0].songs;
            bool together = false;
            for (size_t k = 1; k < order.size(); ++k)
                together = together || songs[(size_t) order[k]].artist == songs[(size_t) order[k - 1]].artist;
            expect (! together, "four rounds, never one artist twice in a row: " + sequence (order));
        }

        beginTest ("The queue plays before the playlist goes on where it was; Play now is a one-off");
        {
            TestPlayer p;
            const auto main = songsIn ("queue/main", 4, 0.5);
            p.addFiles (0, main);
            const auto other = p.addPlaylist ("Requests");
            p.addFiles (other, songsIn ("queue/other", 3, 0.5, 10));
            p.setCrossfade (0.0);

            p.play();
            p.runFor (0.1);
            p.queueNext (other, 2);   // song 3 of the other list
            p.queueNext (other, 1);   // song 2
            expectEquals ((int) p.getQueue().size(), 2);
            expectEquals (sequence (p.runFor (1.95)), juce::String ("1 13 12 2 3"), "song 1, the two requests, then the list goes on");
            expect (p.getQueue().empty());
            expectEquals (p.getShow().active, 0, "the list never changed");

            p.playNow (other, 0);
            p.runFor (0.05);
            expect (p.getCurrent().song.file == p.getShow().playlists[(size_t) other].songs[0].file);
            expect (p.getCurrent().queued);
            expectEquals (p.peekNext().index, 3, "then the list goes on after its song 3");
        }

        beginTest ("Switching playlists: after this song, and each list remembers where it was");
        {
            TestPlayer p;
            p.addFiles (0, songsIn ("switch/a", 4));
            const auto b = p.addPlaylist();
            p.addFiles (b, songsIn ("switch/b", 4));
            p.setPlaylistCrossfade (b, 0.0);
            p.setCrossfade (0.0);

            p.play();
            p.runFor (0.6);                                  // into list A's song 2
            expectEquals (p.getCurrent().index, 1);
            p.switchPlaylist (b, false);
            expectEquals (p.peekNext().playlist, b, "list B next");
            p.runFor (0.5);
            expectEquals (p.getCurrent().playlist, b);
            expectEquals (p.getCurrent().index, 0);

            p.switchPlaylist (0, true);                      // now: back to A
            p.runFor (0.05);
            expectEquals (p.getCurrent().playlist, 0);
            expectEquals (p.getCurrent().index, 2, "A goes on after its song 2");
        }

        beginTest ("Stop after each song: the next waits for Play");
        {
            TestPlayer p;
            p.addFiles (0, songsIn ("each", 3));
            p.setStopAfterEach (0, true);
            p.play();
            expectEquals (sequence (p.runFor (1.0)), juce::String ("1 0"));
            expect (p.getState() == RKPlayProcessor::State::stopped);
            p.play();
            expectEquals (sequence (p.runFor (1.0)), juce::String ("2 0"));
        }

        beginTest ("End at: fades out at the time, or stops after the song playing then");
        {
            auto now = juce::Time (2026, 9, 30, 21, 59, 59, 0, true);
            TestPlayer p;
            p.setClockForTesting ([&now] { return now; });
            p.addFiles (0, songsIn ("endat", 3, 2.0));
            p.setCrossfade (0.0);
            p.setFadeOut (0.5);
            p.setEndAt (true, 22 * 60, false);
            expectWithinAbsoluteError (p.getEndAtRemaining(), 1.0, 0.01);
            p.play();
            p.runFor (0.5);
            now = now + juce::RelativeTime::seconds (2.0);
            p.runFor (1.0);
            expect (p.getState() == RKPlayProcessor::State::stopped, "faded out at 22:00");
            expect (! p.getShow().endAt.on, "once");

            now = juce::Time (2026, 9, 30, 21, 59, 59, 0, true);
            p.setEndAt (true, 22 * 60, true);
            p.jumpTo (0, 0);
            p.runFor (0.5);
            now = now + juce::RelativeTime::seconds (2.0);
            const auto heard = p.runFor (3.0);
            expectEquals (sequence (heard), juce::String ("1 0"), "song 1 to its end, then nothing");
        }

        beginTest ("Fade in: Play from stopped fades the first song in; songs after it, and a list without it, start at once");
        {
            const auto levelAfter = [] (TestPlayer& p, double seconds)
            {
                juce::AudioBuffer<float> buffer (2, kBlock);
                juce::MidiBuffer midi;
                for (int b = 0; b < (int) (seconds * kRate) / kBlock; ++b)
                {
                    buffer.clear();
                    p.processBlock (buffer, midi);
                    p.update();
                }
                return buffer.getSample (0, kBlock - 1);
            };
            TestPlayer p;
            p.addFiles (0, songsIn ("fadein", 3, 6.0));
            p.setCrossfade (0.0);
            p.setPlaylistFadeIn (0, 2.0);
            p.play();
            const auto early = levelAfter (p, 0.5);
            const auto full = levelAfter (p, 2.5);
            expectWithinAbsoluteError (full, 0.01f, 0.0005f, "in by the fade's end");
            expect (early > 0.0f && early < 0.75f * full, "rising at 0.5 s of 2 s: " + juce::String (early));
            p.next();
            expectWithinAbsoluteError (levelAfter (p, 0.1), 0.02f, 0.0005f, "Next: no fade-in");

            p.stop();
            levelAfter (p, 0.2);
            p.setPlaylistFadeIn (0, 0.0);
            p.jumpTo (0, 0);
            expectWithinAbsoluteError (levelAfter (p, 0.05), 0.01f, 0.0005f, "no fade-in: at once");

            p.setPlaylistFadeIn (0, 8.0);
            juce::MemoryBlock state;
            p.getStateInformation (state);
            TestPlayer q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (q.getShow().playlists[0].fadeInSeconds, 8.0, 1.0e-9, "kept by the show");
        }

        beginTest ("Switch at: at the time, fade out then start the other list, crossfade into it, or after the song");
        {
            auto now = juce::Time (2026, 11, 3, 19, 9, 59, 0, true);
            TestPlayer p;
            p.setClockForTesting ([&now] { return now; });
            p.addFiles (0, songsIn ("switchat", 3, 5.0));
            const auto intro = p.addPlaylist ("Intro");
            p.addFiles (intro, songsIn ("switchat/b", 2, 5.0, 10));
            p.setCrossfade (0.0);
            p.setFadeOut (0.5);
            p.setSwitchAt (true, 19 * 60 + 10, intro, PlayShow::SwitchAt::How::fadeThenStart);
            expectWithinAbsoluteError (p.getSwitchAtRemaining(), 1.0, 0.01);
            p.play();
            p.runFor (0.3);
            now = now + juce::RelativeTime::seconds (2.0);
            p.runFor (0.1);
            expect (p.getState() == RKPlayProcessor::State::stopped, "fading out");
            expect (! p.getShow().switchAt.on, "once");
            p.runFor (0.3);
            expect (p.getState() == RKPlayProcessor::State::stopped, "waits for the fade");
            now = now + juce::RelativeTime::seconds (0.6);
            const auto heard = p.runFor (0.5);
            expect (p.getState() == RKPlayProcessor::State::playing && p.getCurrent().playlist == intro && p.getCurrent().index == 0,
                    "then the Intro from its first song");
            expectEquals (sequence (heard).getLastCharacters (2), juce::String ("11"), "its first song heard");

            // Stop during the fade: it doesn't start.
            now = juce::Time (2026, 11, 3, 19, 9, 59, 0, true);
            p.jumpTo (0, 0);
            p.setSwitchAt (true, 19 * 60 + 10, intro, PlayShow::SwitchAt::How::fadeThenStart);
            now = now + juce::RelativeTime::seconds (2.0);
            p.runFor (0.1);
            p.stop();
            now = now + juce::RelativeTime::seconds (1.0);
            p.runFor (0.3);
            expect (p.getState() == RKPlayProcessor::State::stopped, "Stop called it off");

            now = juce::Time (2026, 11, 3, 19, 9, 59, 0, true);
            p.jumpTo (0, 0);
            p.setSwitchAt (true, 19 * 60 + 10, intro, PlayShow::SwitchAt::How::crossfade);
            now = now + juce::RelativeTime::seconds (2.0);
            p.runFor (0.2);
            expect (p.getState() == RKPlayProcessor::State::playing && p.getCurrent().playlist == intro, "crossfade: at once");

            now = juce::Time (2026, 11, 3, 19, 9, 59, 0, true);
            p.jumpTo (0, 0);
            p.setSwitchAt (true, 19 * 60 + 10, intro, PlayShow::SwitchAt::How::afterSong);
            now = now + juce::RelativeTime::seconds (2.0);
            p.runFor (0.2);
            expectEquals (p.getCurrent().playlist, 0, "after the song: this one plays on");
            expectEquals (p.getPendingSwitch(), intro);

            now = juce::Time (2026, 11, 3, 19, 9, 59, 0, true);
            p.stop();
            p.switchPlaylist (0, true);
            p.setSwitchAt (true, 19 * 60 + 10, intro, PlayShow::SwitchAt::How::fadeThenStart);
            now = now + juce::RelativeTime::seconds (2.0);
            p.runFor (0.2);
            expect (p.getState() == RKPlayProcessor::State::stopped && p.getShow().active == intro, "stopped: Play starts it");

            p.setSwitchAt (true, 7 * 60 + 5, intro, PlayShow::SwitchAt::How::crossfade);
            juce::MemoryBlock state;
            p.getStateInformation (state);
            TestPlayer q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            const auto& s = q.getShow().switchAt;
            expect (s.on && s.minutes == 7 * 60 + 5 && s.to == intro && s.how == PlayShow::SwitchAt::How::crossfade, "kept by the show");
            q.removePlaylist (intro);
            expect (! q.getShow().switchAt.on, "its playlist removed: off");
        }

        beginTest (".m3u8: a linked playlist reads it (relative paths, comments), and exports it back");
        {
            const auto files = songsIn ("m3u/songs", 3);
            const auto list = files[0].getParentDirectory().getParentDirectory().getChildFile ("set.m3u8");
            list.replaceWithText ("#EXTM3U\n#EXTINF:1,Three\nsongs/Song 03.wav\n\nsongs/Song 01.wav\n#a comment\nsongs/Missing.wav\n");
            TestPlayer p;
            expect (p.linkPlaylist (0, list));
            const auto& songs = p.getShow().playlists[0].songs;
            expectEquals ((int) songs.size(), 3);
            expect (songs[0].file == files[2] && songs[1].file == files[0]);
            expect (songs[2].status == PlaySong::Status::missing);

            const auto out = list.getSiblingFile ("out.m3u8");
            expect (p.exportPlaylist (0, out));
            expect (out.loadFileAsString().contains ("songs/Song 03.wav"), "relative to the file");
            expect (PlayShow::filesIn (out, false, p.getFormats())[1] == files[0]);
        }

        beginTest ("A linked folder: its songs, sorted, again on Rescan");
        {
            const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKPlayTests/linked");
            folder.deleteRecursively();
            songsIn ("linked", 2);
            TestPlayer p;
            p.linkPlaylist (0, folder);
            expectEquals ((int) p.getShow().playlists[0].songs.size(), 2);
            expect (p.getShow().playlists[0].isLinked());
            songsIn ("linked", 3);
            p.rescan (0);
            expectEquals ((int) p.getShow().playlists[0].songs.size(), 3, "a song added to the folder");
            p.addFiles (0, songsIn ("elsewhere", 1));
            expectEquals ((int) p.getShow().playlists[0].songs.size(), 3, "a linked list takes nothing by hand");
        }

        beginTest ("A moved Music folder: the songs are found by their place in it; Relink finds others by name");
        {
            const auto base = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKPlayTests/music");
            base.deleteRecursively();
            const auto files = songsIn ("music/Library", 2);
            juce::MemoryBlock state;
            {
                TestPlayer p;
                p.setMusicFolder (base.getChildFile ("Library"));
                p.addFiles (0, files);
                p.getStateInformation (state);
            }
            // Another Mac: the folder somewhere else.
            const auto moved = base.getChildFile ("Elsewhere/Library");
            moved.getParentDirectory().createDirectory();
            base.getChildFile ("Library").moveFileTo (moved);

            TestPlayer q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expect (q.getShow().playlists[0].songs[0].status == PlaySong::Status::missing);
            q.setMusicFolder (moved);
            expect (q.getShow().playlists[0].songs[0].isPlayable() && q.getShow().playlists[0].songs[1].isPlayable());

            // A song moved within the library: Relink finds it by name.
            const auto sub = moved.getChildFile ("Sub");
            sub.createDirectory();
            moved.getChildFile ("Song 02.wav").moveFileTo (sub.getChildFile ("Song 02.wav"));
            q.setStateInformation (state.getData(), (int) state.getSize());
            q.setMusicFolder (moved);
            expect (q.getShow().playlists[0].songs[1].status == PlaySong::Status::missing);
            expectEquals (q.relink (moved), 1);
            expect (q.getShow().playlists[0].songs[1].isPlayable());
        }

        beginTest ("Show check: a file back again is found by Check again; Remove takes the missing out");
        {
            const auto files = songsIn ("check", 3, 1.0);
            TestPlayer p;
            p.addFiles (0, files);
            p.setSongTrim (0, 1, -1.0, 0.0);   // (too short to trim: none)
            p.setSongGain (0, 1, -3.0);
            juce::MemoryBlock state;
            p.getStateInformation (state);

            const auto moved = files[1].getSiblingFile ("away.wav");
            files[1].moveFileTo (moved);
            TestPlayer q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (q.getShow().numProblems(), 1);
            expectEquals (q.recheck(), 0, "still gone");
            moved.moveFileTo (files[1]);
            expectEquals (q.recheck(), 1, "plugged in again");
            expectEquals (q.getShow().numProblems(), 0);
            expectEquals (q.getShow().playlists[0].songs[1].gainDb, -3.0, "its gain kept");

            files[1].moveFileTo (moved);
            TestPlayer r;
            r.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (r.removeProblemSongs(), 1);
            expectEquals ((int) r.getShow().playlists[0].songs.size(), 2);
            expectEquals (r.getShow().numProblems(), 0);
            moved.moveFileTo (files[1]);
        }

        beginTest ("The Playlist host parameter crossfades into that list; a show keeps its playlists");
        {
            TestPlayer p;
            p.addFiles (0, songsIn ("param/a", 3, 2.0));
            const auto b = p.addPlaylist ("Second");
            p.addFiles (b, songsIn ("param/b", 3, 2.0));
            p.setOrder (b, PlayPlaylist::Order::shuffle);
            p.setAtEnd (0, PlayPlaylist::AtEnd::goTo, b);
            p.play();
            p.runFor (0.2);
            auto* param = p.findParam ("playlist");
            param->setValueNotifyingHost (param->convertTo0to1 (2.0f));
            p.runFor (0.2);
            expectEquals (p.getCurrent().playlist, b);

            juce::MemoryBlock state;
            p.getStateInformation (state);
            TestPlayer q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals ((int) q.getShow().playlists.size(), 2);
            expectEquals (q.getShow().playlists[1].name, juce::String ("Second"));
            expect (q.getShow().playlists[1].order == PlayPlaylist::Order::shuffle);
            expect (q.getShow().playlists[0].atEnd == PlayPlaylist::AtEnd::goTo && q.getShow().playlists[0].goTo == 1);
            expect (q.getState() == RKPlayProcessor::State::stopped);
        }

        //==============================================================================
        // Phase 4: ducking, pads, pre-listen

        // Runs @p p for @p seconds with @p key on its input; returns the main output's level at the end (RMS of the last block).
        const auto levelAfter = [] (TestPlayer& p, double seconds, float key = 0.0f)
        {
            juce::AudioBuffer<float> buffer (2, kBlock);
            juce::MidiBuffer midi;
            double level = 0.0;
            for (int b = 0; b < (int) (seconds * kRate) / kBlock; ++b)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < kBlock; ++i)
                        buffer.setSample (ch, i, key * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (b * kBlock + i) / kRate));
                p.processBlock (buffer, midi);
                p.update();
                level = buffer.getRMSLevel (0, 0, kBlock);
            }
            return level;
        };

        beginTest ("Duck by hand: the music dips by the depth over the attack, and comes back over the release");
        {
            TestPlayer p;
            p.addFiles (0, songsIn ("duck", 1, 8.0));
            PlayShow::Duck d;
            d.depthDb = -12.0;
            d.attackSeconds = 0.2;
            d.releaseSeconds = 0.5;
            p.setDuck (d);
            p.play();
            const auto full = levelAfter (p, 0.3);
            p.setManualDuck (true);
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (levelAfter (p, 0.3) / full), -12.0, 0.1, "down 12 dB after the attack");
            p.setManualDuck (false);
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (levelAfter (p, 0.25) / full), -6.0, 1.0, "halfway back at half the release");
            expectWithinAbsoluteError (levelAfter (p, 0.4), full, 1.0e-6, "back");
        }

        beginTest ("Duck from the input: a voice dips the music, the hold covers pauses, and the input is never heard");
        {
            TestPlayer p;
            p.addFiles (0, songsIn ("autoduck", 1, 10.0));
            PlayShow::Duck d;
            d.automatic = true;
            d.thresholdDb = -35.0;
            d.holdSeconds = 1.0;
            d.attackSeconds = 0.1;
            d.releaseSeconds = 0.2;
            p.setDuck (d);
            p.play();
            const auto full = levelAfter (p, 0.3);
            const auto ducked = levelAfter (p, 0.5, 0.1f);   // a -23 dB "voice"
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (ducked / full), -12.0, 0.2, "ducked (and the voice itself not in the output)");
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (levelAfter (p, 0.6) / full), -12.0, 0.2, "held through a pause");
            expectWithinAbsoluteError (levelAfter (p, 1.0), full, 1.0e-6, "back after the hold and release");

            d.automatic = false;
            p.setDuck (d);
            p.update();
            expectWithinAbsoluteError (levelAfter (p, 0.5, 0.1f), full, 1.0e-6, "off: the input changes nothing");
        }

        beginTest ("Pads: play over the music (ducking it if set), stop on a second press; Stop the music fades it out");
        {
            TestPlayer p;
            p.addFiles (0, songsIn ("padmusic", 1, 20.0));
            const auto pad = writeWav ("pad", 44100.0, 44100, [] (int, int) { return 0.2f; });   // 1 s, another rate
            expect (p.loadPad (0, pad));
            expectEquals (p.getShow().pads[0].name, juce::String ("pad"));
            p.play();
            const auto music = levelAfter (p, 0.3);
            auto options = p.getShow().pads[0];
            options.ducksMusic = false;
            p.setPadOptions (0, options);
            p.triggerPad (0);
            expectWithinAbsoluteError (levelAfter (p, 0.3), music + 0.2, 0.002, "the pad over the music");
            expect (p.isPadPlaying (0));
            expectWithinAbsoluteError (levelAfter (p, 1.0), music, 1.0e-6, "a one-shot: over after its second");

            options.ducksMusic = true;
            p.setPadOptions (0, options);
            p.triggerPad (0);
            expectWithinAbsoluteError (levelAfter (p, 0.5), music * juce::Decibels::decibelsToGain (-12.0) + 0.2, 0.003, "ducking the music");
            p.triggerPad (0);
            levelAfter (p, 0.1);
            expect (! p.isPadPlaying (0), "a second press stops it");

            options.stopsMusic = true;
            p.setPadOptions (0, options);
            p.setFadeOut (0.5);
            p.triggerPad (0);
            expect (p.getState() == RKPlayProcessor::State::stopped, "the music fades out");

            const auto tooLong = writeWav ("long", kRate, (int) (61 * kRate), [] (int, int) { return 0.1f; });
            expect (! p.loadPad (1, tooLong), "a pad holds up to a minute");
            expect (p.getNote().contains ("minute"));

            juce::MemoryBlock state;
            p.getStateInformation (state);
            TestPlayer q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expect (q.getShow().pads[0].file == pad && q.getShow().pads[0].stopsMusic);
        }

        beginTest ("Pre-listen: a song on the cue output while the main plays on, untouched; it stops when the song is played");
        {
            TestPlayer p;
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (juce::AudioChannelSet::stereo());
            layout.outputBuses.add (juce::AudioChannelSet::stereo());
            layout.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (p.setBusesLayout (layout));
            p.prepareToPlay (kRate, kBlock);
            expect (p.hasCueOutput());

            p.addFiles (0, songsIn ("cue", 3, 4.0));
            p.setCrossfade (0.0);
            p.play();
            p.preListen (0, 2);
            juce::AudioBuffer<float> buffer (4, kBlock);
            juce::MidiBuffer midi;
            for (int b = 0; b < 20; ++b)
            {
                buffer.clear();
                p.processBlock (buffer, midi);
                p.update();
            }
            expectWithinAbsoluteError (buffer.getSample (0, 100), 0.01f, 1.0e-6f, "the main: song 1");
            expectWithinAbsoluteError (buffer.getSample (2, 100), 0.03f, 1.0e-6f, "the cue: song 3");
            expect (p.isPreListening());

            p.jumpTo (0, 2);
            for (int b = 0; b < 10; ++b)
            {
                buffer.clear();
                p.processBlock (buffer, midi);
                p.update();
            }
            expect (! p.isPreListening(), "played for real: pre-listen stops");
            expectEquals (buffer.getSample (2, 100), 0.0f);
        }

        //==============================================================================
        // Phase 5: the standalone app (OSC, MIDI, history, recovery)

        const auto rigFolder = [] (const juce::String& name)
        {
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKPlayTests/rig-" + name);
            f.deleteRecursively();
            return f;
        };

        beginTest ("OSC: playback from TouchOSC, buttons acting on their press");
        {
            TestPlayer p;
            p.setStandaloneForTesting (rigFolder ("osc"));
            auto& rig = *p.getRig();
            p.addFiles (0, songsIn ("osc", 3, 5.0));
            const auto b = p.addPlaylist ("Party");
            p.addFiles (b, songsIn ("osc/b", 2, 5.0, 10));

            rig.handleOsc (juce::OSCMessage ("/rkplay/play", 0.0f));
            expect (p.getState() == RKPlayProcessor::State::stopped, "a release does nothing");
            rig.handleOsc (juce::OSCMessage ("/rkplay/play", 1.0f));
            expect (p.getState() == RKPlayProcessor::State::playing);
            p.runFor (0.1);
            rig.handleOsc (juce::OSCMessage ("/rkplay/next"));
            p.runFor (0.1);
            expectEquals (p.getCurrent().index, 1, "next (no argument: a press)");
            rig.handleOsc (juce::OSCMessage ("/rkplay/duck", 1.0f));
            expect (p.isManualDuck());
            rig.handleOsc (juce::OSCMessage ("/rkplay/duck", 0.0f));
            expect (! p.isManualDuck());
            rig.handleOsc (juce::OSCMessage ("/rkplay/playlist/2", 1.0f));
            p.runFor (0.1);
            expectEquals (p.getCurrent().playlist, b, "the Party list");
            rig.handleOsc (juce::OSCMessage ("/rkplay/level", 0.5f));
            expectWithinAbsoluteError (p.findParam ("level")->getValue(), 0.5f, 1.0e-6f);
            rig.handleOsc (juce::OSCMessage ("/rkplay/endat", juce::String ("23:15")));
            expect (p.getShow().endAt.on && p.getShow().endAt.minutes == 23 * 60 + 15);
            rig.handleOsc (juce::OSCMessage ("/rkplay/endat/off"));
            expect (! p.getShow().endAt.on);
            rig.handleOsc (juce::OSCMessage ("/somethingelse/play", 1.0f));
            rig.handleOsc (juce::OSCMessage ("/rkplay/fade", 1.0f));
            expect (p.getState() == RKPlayProcessor::State::stopped);
        }

        beginTest ("OSC feedback: everything once, then only what changed; lights on the buttons' addresses");
        {
            TestPlayer p;
            p.setStandaloneForTesting (rigFolder ("feedback"));
            auto& rig = *p.getRig();
            p.addFiles (0, songsIn ("feedback", 2, 5.0));
            const auto all = rig.takeFeedback (true);
            const auto find = [] (const std::vector<juce::OSCMessage>& list, const juce::String& address) -> const juce::OSCMessage*
            {
                for (const auto& m : list)
                    if (m.getAddressPattern().toString() == address)
                        return &m;
                return nullptr;
            };
            expect (find (all, "/rkplay/title") != nullptr && (*find (all, "/rkplay/title"))[0].getString() == "Song 01", "stopped: what Play starts");
            expect (find (all, "/rkplay/play") != nullptr && juce::exactlyEqual ((*find (all, "/rkplay/play"))[0].getFloat32(), 0.0f));
            expect (find (all, "/rkplay/playlist/1/name") != nullptr);
            expect (rig.takeFeedback (false).empty(), "nothing changed");

            p.play();
            p.runFor (0.1);
            const auto changed = rig.takeFeedback (false);
            expect (find (changed, "/rkplay/play") != nullptr && juce::exactlyEqual ((*find (changed, "/rkplay/play"))[0].getFloat32(), 1.0f), "Play lights");
            expect (find (changed, "/rkplay/playlist/1/name") == nullptr, "unchanged names aren't sent again");
            rig.handleOsc (juce::OSCMessage ("/rkplay/hello"));
            expect (rig.takeFeedback (false).size() == all.size(), "hello: everything again");
        }

        beginTest ("OSC song list: a page of the playing list, select a song, play it next or now; the queue, the level in dB");
        {
            TestPlayer p;
            p.setStandaloneForTesting (rigFolder ("songs"));
            auto& rig = *p.getRig();
            p.addFiles (0, songsIn ("songs", 25, 5.0));
            const auto value = [&rig] (const juce::String& address) -> juce::String
            {
                for (const auto& m : rig.takeFeedback (true))
                    if (m.getAddressPattern().toString() == "/rkplay/" + address)
                        return m[0].isString() ? m[0].getString() : juce::String (m[0].getFloat32());
                return "(none)";
            };
            expectEquals (value ("songs/page"), juce::String ("1-10 of 25"));
            expectEquals (value ("song/1/name"), juce::String ("Song 01"));
            expectEquals (value ("song/1/time"), juce::String ("0:05"));
            expectEquals (value ("song/11/name"), juce::String ("(none)"), "10 rows");
            rig.handleOsc (juce::OSCMessage ("/rkplay/songs/down", 1.0f));
            expectEquals (value ("songs/page"), juce::String ("11-20 of 25"));
            rig.handleOsc (juce::OSCMessage ("/rkplay/songs/down", 1.0f));
            rig.handleOsc (juce::OSCMessage ("/rkplay/songs/down", 1.0f));
            expectEquals (value ("songs/page"), juce::String ("21-25 of 25"), "the last page");
            expectEquals (value ("song/6/name"), juce::String());
            rig.handleOsc (juce::OSCMessage ("/rkplay/songs/up", 1.0f));
            rig.handleOsc (juce::OSCMessage ("/rkplay/song/3", 1.0f));
            expectEquals (rig.getSelected(), 12);
            expectEquals (value ("song/3"), juce::String ("1"), "the selected row lights");
            expectEquals (value ("selected"), juce::String ("Song 13"));

            rig.handleOsc (juce::OSCMessage ("/rkplay/selected/next", 1.0f));
            expect (p.getQueue().size() == 1 && p.getQueue().front().index == 12, "Play next: the queue");
            expectEquals (value ("queue/1"), juce::String ("Song 13"));
            expectEquals (value ("queue/count"), juce::String ("1 request"));
            p.clearQueue();
            rig.handleOsc (juce::OSCMessage ("/rkplay/selected/now", 1.0f));
            p.runFor (0.1);
            expect (p.getState() == RKPlayProcessor::State::playing && p.getCurrent().index == 12, "Play now");
            expect (value ("song/3/name").endsWith ("Song 13") && value ("song/3/name") != "Song 13", "the playing song is marked");

            rig.handleOsc (juce::OSCMessage ("/rkplay/songs/rows", 20.0f));
            expectEquals (value ("songs/page"), juce::String ("1-20 of 25"), "a tablet's 20 rows");
            rig.handleOsc (juce::OSCMessage ("/rkplay/level/db", -6.0f));
            expectEquals (value ("level/db"), juce::String ("-6.0 dB"));
            p.removeSong (0, 0);
            expectEquals (rig.getSelected(), 12);
            value ("selected");
            expectEquals (rig.getSelected(), -1, "the list changed: nothing selected");
        }

        beginTest ("Web remote: PIN and token, the players listed, now and the lists, every command");
        {
            const auto settings = juce::File::createTempFile ("rkplay-remote.json");
            PlayRemoteHub hub;
            hub.setSettingsFileForTesting (settings);
            hub.setEnabled (true);   // not serving: no player asked to
            expect (! hub.isListening());
            expect (hub.setPin ("2468"));
            expect (! hub.setPin ("12a4") && ! hub.setPin ("123"));

            TestPlayer p, other;
            p.addFiles (0, songsIn ("remote/main", 6, 5.0));
            const auto requests = p.addPlaylist ("Requests");
            p.addFiles (requests, songsIn ("remote/requests", 3, 5.0, 10));
            p.setCrossfade (0.0);
            hub.addPlayer (p, false);
            hub.addPlayer (other, false);

            FakeConnection page;
            const auto say = [&] (const juce::String& json) { hub.received (page, json); hub.tick(); hub.send (page); };
            hub.opened (page);

            // Nothing before the PIN.
            say (R"({"t":"play"})");
            expect (p.getState() == RKPlayProcessor::State::stopped);
            say (R"({"t":"hello","name":"iPhone","pin":"1111"})");
            expect ((bool) page.last ("auth")["wrong"]);
            say (R"({"t":"hello","name":"iPhone","pin":"2468"})");
            const auto token = page.last ("auth")["token"].toString();
            expect ((bool) page.last ("auth")["ok"] && token.length() >= 16);
            expectEquals (hub.numSignedIn(), 1);

            const auto list = page.last ("players")["list"];
            expectEquals (list.size(), 2);
            const auto id = (int) list[0]["id"];
            expectEquals (id, hub.idOf (p));

            page.sent.clear();
            say (R"({"t":"open","p":)" + juce::String (id) + "}");
            auto now = page.last ("now");
            expectEquals (now["state"].toString(), juce::String ("stopped"));
            expectEquals (now["title"].toString(), juce::String ("Song 01"), "stopped: the song Play starts");
            expectEquals (page.last ("lists")["lists"].size(), 2);
            expectEquals ((int) page.last ("lists")["lists"][1]["count"], 3);
            expectEquals (page.last ("pads")["pads"].size(), PlayEngine::kPads);
            expectEquals (page.last ("queue")["items"].size(), 0);

            // Unchanged: nothing sent again.
            page.sent.clear();
            hub.tick(); hub.send (page);
            expectEquals (page.sent.size(), 0);

            // The transport.
            say (R"({"t":"play"})");
            p.runFor (0.6);
            hub.tick(); hub.send (page);
            now = page.last ("now");
            expectEquals (now["state"].toString(), juce::String ("playing"));
            expectEquals ((int) now["playing"]["index"], 0);
            expect ((double) now["pos"] >= 0.5 && (double) now["pos"] < 0.75, "the position in quarter seconds");
            expectWithinAbsoluteError ((double) now["len"], 5.0, 0.05);
            expectEquals (now["next"].toString(), juce::String ("Song 02"));
            say (R"({"t":"next"})");
            p.runFor (0.1);
            expectEquals (p.getCurrent().index, 1);
            say (R"({"t":"pause"})");
            expect (p.getState() == RKPlayProcessor::State::paused);
            say (R"({"t":"playpause"})");
            expect (p.getState() == RKPlayProcessor::State::playing);

            // The song list: sent when asked for, again only when it changes.
            page.sent.clear();
            say (R"({"t":"songs","playlist":0})");
            auto songs = page.last ("songs");
            expectEquals ((int) songs["playlist"], 0);
            expectEquals (songs["songs"].size(), 6);
            expectEquals (songs["songs"][2][0].toString(), juce::String ("Song 03"));
            expectWithinAbsoluteError ((double) songs["songs"][2][2], 5.0, 0.05);
            const auto file3 = songs["songs"][2][4].toString();
            expect (file3.endsWith ("Song 03.wav"));
            page.sent.clear();
            p.runFor (0.3);
            hub.tick(); hub.send (page);
            expectEquals (page.count ("songs"), 0, "playing on changes no list");

            // Play next, checked by the file: an edit in the window moved it.
            p.removeSong (0, 5);
            p.moveSong (0, 2, 4);
            hub.tick(); hub.send (page);
            expectEquals (page.last ("songs")["songs"].size(), 5, "the list changed: sent again");
            say (R"({"t":"queueNext","playlist":0,"index":2,"file":)" + juce::JSON::toString (file3) + "}");
            expect (p.getQueue().size() == 1 && p.getQueue().front().index == 4, "Song 03, where it is now");
            say (R"({"t":"queueNext","playlist":1,"index":0,"file":"/nowhere/Gone.wav"})");
            expectEquals ((int) p.getQueue().size(), 1, "a song that's gone never plays");
            expect (! page.last ("moved").isVoid());
            const auto req2 = p.getShow().playlists[(size_t) requests].songs[1].file.getFullPathName();
            say (R"({"t":"queueNext","playlist":1,"index":1,"file":)" + juce::JSON::toString (req2) + "}");
            expectEquals ((int) page.last ("queue")["items"].size(), 2);
            expectEquals (page.last ("queue")["items"][1]["title"].toString(), juce::String ("Song 02"));

            // The queue: move, remove, clear.
            const auto second = (int) page.last ("queue")["items"][1]["id"];
            say (R"({"t":"queueMove","id":)" + juce::String (second) + R"(,"to":0})");
            expect (p.getQueue().front().playlist == requests, "moved to the front");
            say (R"({"t":"queueRemove","id":)" + juce::String (second) + "}");
            expect (p.getQueue().size() == 1 && p.getQueue().front().playlist == 0);
            say (R"({"t":"queueClear"})");
            expect (p.getQueue().empty());

            // Play now, and go on from a song.
            say (R"({"t":"playNow","playlist":1,"index":2,"file":)"
                 + juce::JSON::toString (p.getShow().playlists[(size_t) requests].songs[2].file.getFullPathName()) + "}");
            p.runFor (0.1);
            expect (p.getCurrent().queued && p.getCurrent().playlist == requests && p.getCurrent().index == 2, "Play now: a one-off");
            const auto song1 = p.getShow().playlists[0].songs[0].file.getFullPathName();
            say (R"({"t":"jump","playlist":0,"index":0,"file":)" + juce::JSON::toString (song1) + "}");
            p.runFor (0.1);
            expect (! p.getCurrent().queued && p.getCurrent().playlist == 0 && p.getCurrent().index == 0, "the list goes on from there");

            // Playlists: after this song, cancelled; now.
            say (R"({"t":"switch","playlist":1,"now":false})");
            expectEquals (p.getPendingSwitch(), requests);
            expectEquals ((int) page.last ("now")["pending"], requests);
            say (R"({"t":"cancelSwitch"})");
            expectEquals (p.getPendingSwitch(), -1);
            expect (! page.last ("now").hasProperty ("pending"));
            say (R"({"t":"switch","playlist":1,"now":true})");
            p.runFor (0.1);
            expectEquals (p.getShow().active, requests);
            expectEquals ((int) page.last ("lists")["active"], requests);

            // Level, Duck, a pad, End at.
            say (R"({"t":"level","v":0.25})");
            expectWithinAbsoluteError (p.findParam ("level")->getValue(), 0.25f, 1.0e-6f);
            expect (page.last ("now")["levelText"].toString().endsWith (" dB"));
            say (R"({"t":"duck","on":true})");
            expect (p.isManualDuck());
            expect ((bool) page.last ("now")["duck"]);
            say (R"({"t":"duck","on":false})");
            expect (! p.isManualDuck());
            say (R"({"t":"pad","n":12})");   // no such pad: nothing
            say (R"({"t":"endAt","time":"23:45"})");
            expect (p.getShow().endAt.on);
            expectEquals (p.getShow().endAt.minutes, 23 * 60 + 45);
            expectEquals (page.last ("now")["endAt"]["time"].toString(), juce::String ("23:45"));
            say (R"({"t":"endAt","time":"tea"})");
            expectEquals (p.getShow().endAt.minutes, 23 * 60 + 45, "not a time: unchanged");
            say (R"({"t":"endAt","off":true})");
            expect (! p.getShow().endAt.on);

            // Stop and Panic.
            say (R"({"t":"stop"})");
            p.runFor (1.0);
            say (R"({"t":"panic"})");
            expect (p.getState() == RKPlayProcessor::State::stopped);

            // The same device again: its token, no PIN.
            FakeConnection again ("10.0.0.6");
            hub.opened (again);
            hub.received (again, R"({"t":"hello","token":")" + token + "\"}");
            hub.tick(); hub.send (again);
            expect ((bool) again.last ("auth")["ok"]);
            expectEquals ((int) hub.getDevices().size(), 2);
            hub.closed (again);

            // Five wrong PINs: wait 30 s.
            FakeConnection guess ("10.0.0.9");
            hub.opened (guess);
            for (int i = 0; i < 5; ++i)
                hub.received (guess, R"({"t":"hello","pin":"0000"})");
            hub.received (guess, R"({"t":"hello","pin":"2468"})");
            hub.tick(); hub.send (guess);
            expect ((int) guess.last ("auth")["wait"] > 0, "locked out, even with the right PIN");
            hub.closed (guess);

            // A player that goes: the page hears it.
            page.sent.clear();
            hub.removePlayer (p);
            hub.tick(); hub.send (page);
            expect (! page.last ("gone").isVoid());
            expectEquals (page.last ("players")["list"].size(), 1);

            // A new PIN lets everyone go; the settings are saved.
            hub.setPin ("97531");
            expect (page.closed);
            PlayRemoteHub reloaded;
            reloaded.setSettingsFileForTesting (settings);
            expectEquals (reloaded.getPin(), juce::String ("97531"));
            expect (reloaded.isEnabled());
            expectEquals (reloaded.getPort(), PlayRemoteHub::kDefaultPort);
            reloaded.setEnabled (false);
            hub.removePlayer (other);

            // The pages.
            expect (hub.answer ("/").body.toString().contains ("play.js"));
            expect (hub.answer ("/p/rk.js").body.toString().contains ("const RK"));
            expect (hub.answer ("/p/play.js").body.toString().contains ("PlayRemoteHub"));
            expectEquals (hub.answer ("/p/../secret").status, 404);
            expect (hub.answer ("/manifest/play.json").body.toString().contains ("RKPlay"));
            expectEquals (hub.answer ("/icon.png").contentType, juce::String ("image/png"));
            expect (hub.link (true).endsWith (":8082/#pin=97531"));
            settings.deleteFile();
        }

        beginTest ("Web remote search: every word, accents ignored, across playlists, at most 200");
        {
            expectEquals (PlayRemoteHub::fold (juce::String (juce::CharPointer_UTF8 ("Caf\xc3\xa9 \xc5\x98\xc3\xadjen \xc5\xbdLU\xc5\xa4OU\xc4\x8cK\xc3\x9d Stra\xc3\x9f" "e"))),
                          juce::String ("cafe rijen zlutoucky strasse"));

            TestPlayer p;
            const auto quiet = [] (int, int) { return 0.01f; };
            p.addFiles (0, { writeWav ("search/The Beatles - Let It Be", kRate, 4800, quiet),
                             writeWav ("search/The Beatles - Help", kRate, 4800, quiet) });
            const auto dinner = p.addPlaylist ("Dinner");
            p.addFiles (dinner, { writeWav (juce::String (juce::CharPointer_UTF8 ("search/Karel Gott - Lady Carneval \xc5\x99\xc3\xadjen")), kRate, 4800, quiet) });

            PlayRemoteHub hub;
            const auto results = [&] (const juce::String& q) { return juce::JSON::parse (hub.searchJson (p, q))["results"]; };
            expectEquals (results ("beatles let").size(), 1, "every word, in any order");
            expectEquals (results ("let beatles")[0][2].toString(), juce::String ("Let It Be"));
            expectEquals (results ("BEATLES").size(), 2, "case ignored");
            const auto gott = results ("carneval rijen");
            expectEquals (gott.size(), 1, "accents ignored");
            expectEquals ((int) gott[0][0], dinner);
            expectEquals ((int) gott[0][1], 0);
            expectEquals (gott[0][3].toString(), juce::String ("Karel Gott"));
            expectEquals (results ("").size(), 0);
            expectEquals (results ("nothing like it").size(), 0);

            p.addFiles (0, songsIn ("search/many", 210, 0.1));
            const auto many = juce::JSON::parse (hub.searchJson (p, "song"));
            expectEquals (many["results"].size(), PlayRemoteHub::kMaxResults);
            expect ((bool) many["more"]);

            expectEquals (PlayRemoteHub::findSong (p, 0, 1, p.getShow().playlists[0].songs[1].file.getFullPathName()), 1);
            expectEquals (PlayRemoteHub::findSong (p, 0, 0, p.getShow().playlists[0].songs[1].file.getFullPathName()), 1, "found where it is");
            expectEquals (PlayRemoteHub::findSong (p, 5, 0, "x"), -1);
        }

        beginTest ("MIDI: a learned footswitch plays, program changes switch playlists");
        {
            TestPlayer p;
            p.setStandaloneForTesting (rigFolder ("midi"));
            auto& rig = *p.getRig();
            p.addFiles (0, songsIn ("midi", 2, 5.0));
            const auto b = p.addPlaylist();
            p.addFiles (b, songsIn ("midi/b", 2, 5.0, 10));

            const auto send = [&p] (const juce::MidiMessage& m)
            {
                juce::AudioBuffer<float> buffer (2, kBlock);
                juce::MidiBuffer midi;
                midi.addEvent (m, 0);
                buffer.clear();
                p.processBlock (buffer, midi);
                p.update();
            };
            rig.learn (PlayRig::playPause);
            send (juce::MidiMessage::controllerEvent (1, 64, 127));
            expectEquals (rig.describe (PlayRig::playPause), juce::String ("CC 64, channel 1"));
            expect (p.getState() == RKPlayProcessor::State::stopped, "learning doesn't act");
            send (juce::MidiMessage::controllerEvent (1, 64, 0));
            send (juce::MidiMessage::controllerEvent (1, 64, 127));
            expect (p.getState() == RKPlayProcessor::State::playing, "the footswitch going down");
            send (juce::MidiMessage::controllerEvent (1, 64, 127));
            expect (p.getState() == RKPlayProcessor::State::playing, "held: no second press");

            send (juce::MidiMessage::programChange (1, 1));
            p.runFor (0.1);
            expectEquals (p.getCurrent().playlist, b, "program change 2 (number 1): playlist 2");

            // The mapping is the rig's: another instance has it.
            TestPlayer q;
            q.setStandaloneForTesting (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKPlayTests/rig-midi"));
            expectEquals (q.getRig()->describe (PlayRig::playPause), juce::String ("CC 64, channel 1"));
        }

        beginTest ("History: what played, a file a night; recovery after a crash");
        {
            const auto folder = rigFolder ("session");
            {
                TestPlayer p;
                p.setStandaloneForTesting (folder);
                p.addFiles (0, songsIn ("session", 3, 5.0));
                p.play();
                p.runFor (0.2);
                juce::Thread::sleep (1100);   // the history counts a song played for a second or more
                p.next();
                p.runFor (0.2);
                const auto log = p.getRig()->historyFile (juce::Time::getCurrentTime()).loadFileAsString();
                expect (log.startsWith ("time,playlist,title"));
                expect (log.contains ("Playlist 1,Song 01,"), log);

                p.getRig()->writeSession (false);   // the autosave; then the app "crashes" (no clean save)
                p.setStandaloneForTesting (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKPlayTests/elsewhere"));
            }
            TestPlayer q;
            q.setStandaloneForTesting (folder);
            expect (q.getRig()->recover(), "recovered");
            expectEquals ((int) q.getShow().playlists[0].songs.size(), 3, "the show is back");
            expect (q.getState() == RKPlayProcessor::State::stopped, "stopped");
            expectEquals (q.peekNext().index, 1, "the song that was playing, ready");
            expect (q.getNote().contains ("unexpectedly"));

            q.getRig()->writeSession (true);   // a normal quit
            TestPlayer r;
            r.setStandaloneForTesting (folder);
            expect (! r.getRig()->recover(), "nothing to recover after a normal quit");
        }

        beginTest ("Factory shows all load cleanly, with what their manual promises");
        {
            TestPlayer p;
            const auto dir = juce::File (RKFX_SOURCE_DIR).getChildFile ("plugins/RKPlay/presets");
            expectEquals ((int) p.getFactoryPresets().size(), dir.findChildFiles (juce::File::findFiles, false, "*.json").size());
            expectEquals ((int) p.getFactoryPresets().size(), 4);
            for (int i = 0; i < (int) p.getFactoryPresets().size(); ++i)
            {
                const auto& preset = p.getFactoryPresets()[(size_t) i];
                const auto r = p.loadFactoryPreset (i);
                expect (r.ok && r.message.isEmpty(), preset.name + ": " + r.message);
                expect (! p.isPresetModified(), preset.name + " unmodified");
                const auto& show = p.getShow();
                expectEquals (show.level.targetLufs, -18.0, preset.name);
                expectEquals (show.level.peakCeilingDb, -0.5);
                expect (show.numProblems() == 0 && p.getNote().isEmpty(), preset.name + ": " + p.getNote());
                if (preset.name == "Wedding Reception")
                {
                    expectEquals ((int) show.playlists.size(), 4);
                    expectEquals (show.playlists[1].name, juce::String ("Dinner"));
                    expectEquals (show.playlists[1].gainDb, -4.0);
                    expect (show.playlists[2].stopAfterEach && show.playlists[3].order == PlayPlaylist::Order::shuffle);
                    expect (show.duck.automatic);
                    expect (show.pads[0].isEmpty() && show.pads[0].name == "Entrance" && show.pads[0].stopsMusic);
                    expect (show.pads[1].name == "Fanfare" && show.pads[1].ducksMusic);
                }
                if (preset.name == "Ceremony")
                    expect (show.playlists[0].stopAfterEach && show.playlists[0].crossfadeSeconds == 0.0 && show.fadeOutSeconds == 3.0);
            }

            // A named, empty pad keeps its name when a sound is loaded; a sound replaced takes the new name.
            for (int i = 0; i < (int) p.getFactoryPresets().size(); ++i)
                if (p.getFactoryPresets()[(size_t) i].name == "Wedding Reception")
                    p.loadFactoryPreset (i);
            const auto padFile = songsIn ("factory-pad", 1, 2.0)[0];
            expect (p.loadPad (1, padFile));
            expectEquals (p.getShow().pads[1].name, juce::String ("Fanfare"));
            expect (p.loadPad (1, padFile));
            expectEquals (p.getShow().pads[1].name, padFile.getFileNameWithoutExtension());
        }
    }
};

static PlayTests playTests;

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    // RKPlayTests --analyse <files...>: what RKPlay measures (a check against other meters);
    // --cache <files...> also keeps it in RKPlay's analysis cache.
    if (argc > 2 && (juce::String (argv[1]) == "--analyse" || juce::String (argv[1]) == "--cache"))
    {
        PlayAnalysisCache cache (PlayAnalysisCache::defaultFile());
        for (int i = 2; i < argc; ++i)
            if (auto reader = readerFor (juce::File (juce::String::fromUTF8 (argv[i]))))
            {
                const auto a = PlayAnalysis::analyse (*reader);
                if (juce::String (argv[1]) == "--cache")
                    cache.store (juce::File (juce::String::fromUTF8 (argv[i])), a);
                std::printf ("%7.2f LUFS  TP %6.2f dBTP  start %5.2f  end %7.2f  fade-out %7.2f  of %7.2f  %s\n",
                             a.loudness, a.truePeakDb, a.musicStart, a.musicEnd, a.fadeOutPoint, a.lengthSeconds,
                             juce::File (juce::String::fromUTF8 (argv[i])).getFileName().toRawUTF8());
            }
        cache.save();
        return 0;
    }
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
