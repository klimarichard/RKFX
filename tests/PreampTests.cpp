#include <juce_audio_basics/juce_audio_basics.h>

#include "PreampEngine.h"

#include <cmath>
#include <complex>

/** RKPreamp DSP: the tone stack behaves like the passive circuit (never
    boosts, bass / middle / treble act where they should, the digital filter
    matches the analog one), the gain stages are level-matched while the
    drive grows, Bright adds highs, switching voicings never clicks, and the
    latency follows the oversampling. */
class PreampTests : public juce::UnitTest
{
public:
    PreampTests() : juce::UnitTest ("RKPreamp engine") {}

    void runTest() override
    {
        using Voicing = PreampEngine::Voicing;
        const Voicing voicings[] { Voicing::cleanAmerican, Voicing::britishCrunch, Voicing::topBoost, Voicing::hotRod };

        beginTest ("Tone stack: passive, and the digital filter matches the circuit");
        {
            for (const auto v : voicings)
            {
                const auto& parts = PreampEngine::voiceFor (v).stack;
                auto maxDb = -100.0, worstError = 0.0;
                for (const auto t : { 0.0, 0.5, 1.0 })
                    for (const auto m : { 0.0, 0.5, 1.0 })
                        for (const auto l : { 0.0, 0.5, 1.0 })
                        {
                            const auto analog = PreampEngine::toneStack (parts, t, m, l);
                            const auto digital = PreampEngine::discretise (analog, 96000.0);
                            for (const auto hz : { 40.0, 100.0, 300.0, 1000.0, 3000.0, 8000.0 })
                            {
                                maxDb = juce::jmax (maxDb, digitalDb (digital, hz, 96000.0));
                                if (hz <= 3000.0)
                                    worstError = juce::jmax (worstError, std::abs (digitalDb (digital, hz, 96000.0) - analogDb (analog, hz)));
                            }
                        }
                expect (maxDb < 0.5, "never boosts: " + juce::String (maxDb));
                expect (worstError < 0.3, "bilinear matches the circuit up to 3 kHz: " + juce::String (worstError));
            }
        }

        beginTest ("Tone stack: each knob acts where it should, centred at 0 dB");
        {
            for (const auto v : voicings)
            {
                PreampEngine::Settings s;
                s.voicing = v;
                double sumDb = 0.0;
                for (int i = 0; i < 24; ++i)
                    sumDb += PreampEngine::toneStackDb (s, 48000.0, 100.0 * std::pow (40.0, i / 23.0));
                expectWithinAbsoluteError (sumDb / 24.0, 0.0, 1.0e-6, "centred: 0 dB on average");

                const auto change = [&] (float PreampEngine::Settings::* knob, double hz)
                {
                    auto low = s, high = s;
                    low.*knob = 0.0f;
                    high.*knob = 1.0f;
                    return PreampEngine::toneStackDb (high, 48000.0, hz) - PreampEngine::toneStackDb (low, 48000.0, hz);
                };
                expect (change (&PreampEngine::Settings::bass, 80.0) > 6.0, "bass at 80 Hz: " + juce::String (change (&PreampEngine::Settings::bass, 80.0)));
                expect (change (&PreampEngine::Settings::treble, 5000.0) > 6.0, "treble at 5 kHz: " + juce::String (change (&PreampEngine::Settings::treble, 5000.0)));
                expect (change (&PreampEngine::Settings::middle, 600.0) > 3.0, "middle at 600 Hz: " + juce::String (change (&PreampEngine::Settings::middle, 600.0)));
            }
        }

        beginTest ("Gain: more drive, about the same level");
        {
            for (const auto v : voicings)
            {
                PreampEngine::Settings low, high;
                low.voicing = high.voicing = v;
                low.gain = 0.2f;
                high.gain = 0.9f;
                const auto a = run (low, sine (440.0, 0.25f, 48000));
                const auto b = run (high, sine (440.0, 0.25f, 48000));
                const auto levelA = rmsDb (a), levelB = rmsDb (b);
                expect (std::abs (levelA - levelB) < 6.0f, "level " + juce::String (levelA) + " vs " + juce::String (levelB));
                expect (harmonics (b, 440.0) > harmonics (a, 440.0), "more harmonics with more gain");
                expect (std::isfinite (levelB) && b.getMagnitude (0, 0, b.getNumSamples()) < 4.0f, "bounded");
            }
        }

        beginTest ("Clean at low gain stays clean for soft playing");
        {
            PreampEngine::Settings s;
            s.voicing = Voicing::cleanAmerican;
            s.gain = 0.0f;
            expect (harmonics (run (s, sine (440.0, 0.05f, 48000)), 440.0) < 0.02, "clean");
        }

        beginTest ("Bright adds highs, most at low gain");
        {
            PreampEngine::Settings s;
            s.voicing = Voicing::cleanAmerican;
            s.gain = 0.0f;
            auto bright = s;
            bright.bright = true;
            const auto plain = rmsDb (run (s, sine (6000.0, 0.02f, 48000)));
            const auto lifted = rmsDb (run (bright, sine (6000.0, 0.02f, 48000)));
            expect (lifted - plain > 3.0f, "+" + juce::String (lifted - plain) + " dB at 6 kHz");
        }

        beginTest ("Switching the voicing dips instead of clicking");
        {
            PreampEngine engine;
            PreampEngine::Settings s;
            s.gain = 0.7f;
            engine.setSettings (s);
            engine.prepare (48000.0);

            auto buffer = sine (220.0, 0.25f, 48000);
            float maxStep = 0.0f, last = 0.0f, quietest = 1.0f;
            for (int start = 0; start < buffer.getNumSamples(); start += 64)
            {
                if (start == 24000)
                {
                    s.voicing = Voicing::hotRod;
                    s.oversampling = 4;
                    engine.setSettings (s);
                }
                float* ch[] { buffer.getWritePointer (0, start) };
                engine.process (ch, 1, 64);
                if (start > 24000 && start < 26000)
                    for (int w = 0; w < 64; w += 16)
                        quietest = juce::jmin (quietest, buffer.getMagnitude (0, start + w, 16));
                for (int i = 0; i < 64; ++i)
                {
                    const auto x = buffer.getSample (0, start + i);
                    if (start + i > 4800)
                        maxStep = juce::jmax (maxStep, std::abs (x - last));
                    last = x;
                }
            }
            expect (quietest < 0.01f, "dips to silence while switching");
            expectEquals (engine.latencySamples(), rkfx::Oversampler::latencySamples (4));
            // A driven 220 Hz wave at about -12 dBFS: steep edges, but no jump beyond them.
            expect (maxStep < 0.35f, "largest step " + juce::String (maxStep));
        }
    }

private:
    static juce::AudioBuffer<float> sine (double hz, float amplitude, int n)
    {
        juce::AudioBuffer<float> b (1, n);
        for (int i = 0; i < n; ++i)
            b.setSample (0, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / 48000.0));
        return b;
    }

    static juce::AudioBuffer<float> run (const PreampEngine::Settings& s, juce::AudioBuffer<float> buffer)
    {
        PreampEngine engine;
        engine.setSettings (s);
        engine.prepare (48000.0);
        for (int start = 0; start < buffer.getNumSamples(); start += 64)
        {
            float* ch[] { buffer.getWritePointer (0, start) };
            engine.process (ch, 1, juce::jmin (64, buffer.getNumSamples() - start));
        }
        return buffer;
    }

    static float rmsDb (const juce::AudioBuffer<float>& b)
    {
        const auto half = b.getNumSamples() / 2;
        return juce::Decibels::gainToDecibels (b.getRMSLevel (0, half, half));
    }

    /** Amplitude at @p hz over the second half (Goertzel, Hann window). */
    static double amplitude (const juce::AudioBuffer<float>& b, double hz)
    {
        const auto n = b.getNumSamples() / 2;
        const auto* data = b.getReadPointer (0, n);
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / 48000.0;
        const auto coeff = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
            const auto s0 = data[i] * win + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return 2.0 * std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / (n * 0.5);
    }

    /** Harmonics 2 to 5 relative to the fundamental. */
    static double harmonics (const juce::AudioBuffer<float>& b, double hz)
    {
        double sum = 0.0;
        for (int h = 2; h <= 5; ++h)
            sum += std::pow (amplitude (b, hz * h), 2.0);
        return std::sqrt (sum) / juce::jmax (1.0e-9, amplitude (b, hz));
    }

    static double digitalDb (const PreampEngine::Digital& d, double hz, double rate)
    {
        const auto z1 = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * hz / rate);
        std::complex<double> num = 0.0, den = 0.0, zk = 1.0;
        for (int k = 0; k < 4; ++k, zk *= z1)
        {
            num += d.b[k] * zk;
            den += d.a[k] * zk;
        }
        return 20.0 * std::log10 (std::abs (num / den));
    }

    static double analogDb (const PreampEngine::Analog& a, double hz)
    {
        const std::complex<double> s (0.0, 2.0 * juce::MathConstants<double>::pi * hz);
        const auto num = a.b1 * s + a.b2 * s * s + a.b3 * s * s * s;
        const auto den = 1.0 + a.a1 * s + a.a2 * s * s + a.a3 * s * s * s;
        return 20.0 * std::log10 (std::abs (num / den));
    }
};

static PreampTests preampTests;
