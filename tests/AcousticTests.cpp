#include <juce_audio_basics/juce_audio_basics.h>

#include "AcousticEngine.h"

#include <cmath>

/** RKAcoustic DSP: flat when everything is off, the measured response
    matches the design (body, quack, EQ, high-pass), the notch, phase and
    compressor work, and changing the voicing while playing never clicks. */
class AcousticTests : public juce::UnitTest
{
public:
    AcousticTests() : juce::UnitTest ("RKAcoustic engine") {}

    void runTest() override
    {
        using Voicing = AcousticEngine::Voicing;

        beginTest ("Everything off is transparent");
        {
            const auto s = flat();
            for (const auto hz : { 100.0, 1000.0, 8000.0 })
                expectWithinAbsoluteError (gainDb (s, hz), 0.0, 0.05, juce::String (hz) + " Hz");
        }

        beginTest ("Body and quack follow the voicing");
        {
            auto s = flat();
            s.voicing = Voicing::violin;
            s.body = 1.0f;
            expect (AcousticEngine::magnitudeDb (s, 48000.0, 280.0) > 4.0, "violin air resonance");
            expect (std::abs (AcousticEngine::magnitudeDb (s, 48000.0, 8000.0)) < 0.5, "highs untouched");

            s.body = 0.0f;
            s.quack = 1.0f;
            expectWithinAbsoluteError (AcousticEngine::magnitudeDb (s, 48000.0, 3000.0), -12.0, 0.1, "quack cut at 3 kHz");

            s.voicing = Voicing::bass;
            expectWithinAbsoluteError (AcousticEngine::magnitudeDb (s, 48000.0, 900.0), -12.0, 0.1, "bass quack at 900 Hz");
        }

        beginTest ("The measured response matches the design");
        {
            AcousticEngine::Settings s;
            s.voicing = Voicing::guitar;
            s.body = 0.8f;
            s.quack = 0.5f;
            s.highPassHz = 80.0f;
            s.lowDb = 3.0f;
            s.midDb = -4.0f;
            s.midHz = 600.0f;
            s.trebleDb = 2.0f;
            s.airDb = 4.0f;
            for (const auto hz : { 60.0, 100.0, 200.0, 600.0, 1600.0, 5000.0, 14000.0 })
                expectWithinAbsoluteError (gainDb (s, hz), AcousticEngine::magnitudeDb (s, 48000.0, hz), 0.3,
                                           juce::String (hz) + " Hz");
            expect (gainDb (s, 30.0) < -12.0, "the high-pass cuts below 80 Hz");
        }

        beginTest ("Notch: deep at its frequency, gone when off");
        {
            auto s = flat();
            s.notchHz = 500.0f;
            expect (gainDb (s, 500.0) < -20.0, "notched: " + juce::String (gainDb (s, 500.0)));
            expectWithinAbsoluteError (gainDb (s, 1000.0), 0.0, 1.0, "an octave away");
            s.notchHz = 0.0f;
            expectWithinAbsoluteError (gainDb (s, 500.0), 0.0, 0.05);
        }

        beginTest ("Phase invert");
        {
            auto s = flat();
            s.invert = true;
            auto in = sine (1000.0, 0.5f, 9600);
            auto out = run (s, in);
            float maxError = 0.0f;
            for (int i = 4800; i < 9600; ++i)
                maxError = juce::jmax (maxError, std::abs (out.getSample (0, i) + in.getSample (0, i)));
            expect (maxError < 0.01f, "inverted: " + juce::String (maxError));
        }

        beginTest ("Compressor: loud notes come down, quiet ones get the makeup gain");
        {
            auto s = flat();
            s.compress = 1.0f;
            const auto loud = levelDb (run (s, sine (1000.0, 0.5f, 48000)));        // -6 dBFS peak
            const auto quiet = levelDb (run (s, sine (1000.0, 0.003f, 48000)));     // about -50 dBFS
            expect (loud < -10.0f, "loud compressed: " + juce::String (loud));
            expectWithinAbsoluteError (quiet - (float) juce::Decibels::gainToDecibels (0.003 / std::sqrt (2.0)),
                                       AcousticEngine::makeupFor (1.0f), 0.3f, "quiet: makeup only");
        }

        beginTest ("Changing the voicing while playing never clicks");
        {
            AcousticEngine engine;
            auto s = flat();
            s.voicing = Voicing::violin;
            s.body = 1.0f;
            s.quack = 1.0f;
            engine.setSettings (s);
            engine.prepare (48000.0);

            auto buffer = sine (440.0, 0.5f, 48000);
            float maxStep = 0.0f, last = 0.0f;
            for (int start = 0; start < buffer.getNumSamples(); start += 64)
            {
                if (start == 24000)
                {
                    s.voicing = Voicing::bass;
                    s.notchHz = 450.0f;
                    engine.setSettings (s);
                }
                float* ch[] { buffer.getWritePointer (0, start) };
                engine.process (ch, 1, 64);
                for (int i = 0; i < 64; ++i)
                {
                    const auto x = buffer.getSample (0, start + i);
                    if (start + i > 2400)
                        maxStep = juce::jmax (maxStep, std::abs (x - last));
                    last = x;
                }
            }
            // A 440 Hz sine at 0.5 (boosted a few dB) moves at most ~0.06 per sample.
            expect (maxStep < 0.12f, "largest step " + juce::String (maxStep));
        }
    }

private:
    static AcousticEngine::Settings flat()
    {
        AcousticEngine::Settings s;
        s.voicing = AcousticEngine::Voicing::flat;
        s.body = 0.0f;
        s.quack = 0.0f;
        return s;
    }

    static juce::AudioBuffer<float> sine (double hz, float amplitude, int n)
    {
        juce::AudioBuffer<float> b (1, n);
        for (int i = 0; i < n; ++i)
            b.setSample (0, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / 48000.0));
        return b;
    }

    static juce::AudioBuffer<float> run (const AcousticEngine::Settings& s, juce::AudioBuffer<float> buffer)
    {
        AcousticEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        for (int start = 0; start < buffer.getNumSamples(); start += 64)
        {
            float* ch[] { buffer.getWritePointer (0, start) };
            engine.process (ch, 1, juce::jmin (64, buffer.getNumSamples() - start));
        }
        return buffer;
    }

    /** RMS level of the second half, in dB. */
    static float levelDb (const juce::AudioBuffer<float>& b)
    {
        const auto half = b.getNumSamples() / 2;
        return juce::Decibels::gainToDecibels (b.getRMSLevel (0, half, half));
    }

    /** Steady-state gain for a sine at @p hz, in dB. */
    static double gainDb (const AcousticEngine::Settings& s, double hz)
    {
        const auto n = 48000;
        const auto out = run (s, sine (hz, 0.1f, n));
        return juce::Decibels::gainToDecibels (out.getRMSLevel (0, n / 2, n / 2) / (0.1 / std::sqrt (2.0)));
    }
};

static AcousticTests acousticTests;
