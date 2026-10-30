#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PlayAnalysis.h"
#include "PlayEngine.h"
#include "dsp/Loudness.h"
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


    /** A folder of short songs, each a steady level of its own (song n plays 0.01 * (first + n + 1)). */
    juce::Array<juce::File> songsIn (const juce::String& folder, int count, double seconds = 0.5, int first = 0)
    {
        juce::Array<juce::File> files;
        for (int n = 0; n < count; ++n)
            files.add (writeWav (folder + "/Song " + juce::String (n + 1).paddedLeft ('0', 2), kRate, (int) (seconds * kRate),
                                 [n, first] (int, int) { return 0.01f * (float) (first + n + 1); }));
        return files;
    }

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
