#include <juce_audio_basics/juce_audio_basics.h>

#include "MixEngine.h"

#include <cmath>

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 64;
    constexpr int kChannels = MixEngine::kChannels;
    constexpr int kMic = 5;                                 // the input the "mic" arrives on

    /** Channel 1 (a 440 Hz sine) into Aux 1 (stereo, Out 1/2), nothing on the main. */
    MixEngine::Settings wedge()
    {
        MixEngine::Settings s;
        for (auto& c : s.channels)
            c.toMain = false;
        s.buses[0] = {};
        s.buses[0].output = mix::pairTarget (0);
        s.channels[0].sendDb[0] = 0.0f;
        return s;
    }

    /** Inserts that scale bus 1 by a gain (the noise must pass the bus's inserts). */
    struct GainInserts : public MixEngine::Inserts
    {
        float gain = 1.0f;
        void process (Strip strip, int index, float* const* io, int numSides, int numSamples) noexcept override
        {
            if (strip == Strip::bus && index == 0)
                for (int side = 0; side < numSides; ++side)
                    juce::FloatVectorOperations::multiply (io[side], gain, numSamples);
        }
        bool isActive (Strip, int) const noexcept override { return false; }
    };

    /** Runs the engine, the mic hearing Out 1 one block later through @p acoustic (gain). */
    struct Rig
    {
        MixEngine engine;
        juce::AudioBuffer<float> in { kChannels, kBlock }, out { kChannels, kBlock };
        std::vector<float> lastOut = std::vector<float> ((size_t) kBlock, 0.0f);
        float acoustic = 1.0f;
        std::int64_t t = 0;
        double outEnergy[2] {}, sineCorrelation = 0.0;
        int energySamples = 0;
        float maxStep = 0.0f, previous = 0.0f;

        explicit Rig (const MixEngine::Settings& s, float busDb = 0.0f)
        {
            engine.prepare (kRate, kBlock);
            engine.setSettings (s);
            MixEngine::Levels l;
            l.busDb[0] = busDb;
            engine.setLevels (l);
        }

        void run (double seconds, bool collect = false)
        {
            const auto blocks = (int) (seconds * kRate / kBlock);
            for (int n = 0; n < blocks; ++n)
            {
                in.clear();
                for (int i = 0; i < kBlock; ++i)
                {
                    in.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * (double) (t + i) / kRate));
                    in.setSample (kMic, i, acoustic * lastOut[(size_t) i]);
                }
                engine.process (in.getArrayOfReadPointers(), out.getArrayOfWritePointers(), kBlock, false);
                for (int i = 0; i < kBlock; ++i)
                {
                    const auto x = out.getSample (0, i);
                    lastOut[(size_t) i] = x;
                    maxStep = juce::jmax (maxStep, std::abs (x - previous));
                    previous = x;
                    if (collect)
                    {
                        for (int side = 0; side < 2; ++side)
                            outEnergy[side] += (double) out.getSample (side, i) * out.getSample (side, i);
                        sineCorrelation += (double) x * in.getSample (0, i);
                    }
                }
                if (collect)
                    energySamples += kBlock;
                t += kBlock;
            }
        }

        double rmsDb (int side) const { return 10.0 * std::log10 (outEnergy[side] / juce::jmax (1, energySamples) + 1.0e-30); }
        void clearCollected() { outEnergy[0] = outEnergy[1] = sineCorrelation = 0.0; energySamples = 0; }
    };

    /** The 0.5 sine on channel 1 through a centre pan (-3 dB) and the bus fader, as RMS dB. */
    double sineDb (float faderDb)
    {
        return 20.0 * std::log10 (0.5 * std::sqrt (0.5) * juce::Decibels::decibelsToGain (faderDb) * std::sqrt (0.5));
    }

    double bandDb (const MixMeasure::Sums& a, const MixMeasure::Sums& b, int band)
    {
        const auto input = b.input[(size_t) band] - a.input[(size_t) band];
        const auto reference = b.reference[(size_t) band] - a.reference[(size_t) band];
        return 10.0 * std::log10 (input / reference);
    }
}

class MixMeasureTests : public juce::UnitTest
{
public:
    MixMeasureTests() : juce::UnitTest ("RKMix Monitor EQ measuring") {}

    void runTest() override
    {
        beginTest ("Band filters: Q for -3 dB at the band edges; the top band only below 0.45 x the rate");
        {
            expectWithinAbsoluteError (MixMeasure::bandQ(), 2.779, 0.001);
            expect (MixMeasure::isMeasured (30, 48000.0));
            expect (! MixMeasure::isMeasured (30, 44100.0), "20 kHz at 44.1 kHz");
            expect (MixMeasure::isMeasured (29, 44100.0));
        }

        beginTest ("Idle: nothing taken over, the bus as it was");
        {
            Rig rig (wedge(), -6.0f);
            expect (! rig.engine.getMeasure().isBusy());
            rig.run (0.2);
            rig.run (0.5, true);
            expectWithinAbsoluteError (rig.rmsDb (0), sineDb (-6.0f), 0.2, "the sine at the fader (-6 dB), panned centre");
            expectEquals ((int) rig.engine.getMeasure().read().samples, 0, "no sums while idle");
        }

        beginTest ("Takeover: the bus carries only the noise, at its level, whatever the fader and mute");
        {
            Rig rig (wedge(), -30.0f);
            auto& m = rig.engine.getMeasure();
            expect (m.start (0, kMic, MixMeasure::Sides::both));
            expect (! m.start (0, kMic, MixMeasure::Sides::both), "one run at a time");
            m.setNoise (true, -20.0f);
            rig.run (1.5);
            expect (m.read().steady);
            rig.run (2.0, true);
            expectWithinAbsoluteError (rig.rmsDb (0), -20.0, 0.5, "the noise at -20 dBFS RMS");
            expectWithinAbsoluteError (rig.rmsDb (1), -20.0, 0.5);
            expectLessThan (std::abs (rig.sineCorrelation / rig.energySamples), 1.0e-3, "the channel's sine is gone");
            expectLessThan (rig.maxStep, 0.5f, "no jumps");
        }

        beginTest ("The response: input against the noise, per band, through the bus's inserts");
        {
            Rig rig (wedge());
            GainInserts inserts;
            inserts.gain = 0.5f;                          // -6 dB in the bus's inserts
            rig.engine.setInserts (&inserts);
            rig.acoustic = 0.25f;                         // -12 dB "in the air"
            auto& m = rig.engine.getMeasure();
            m.start (0, kMic, MixMeasure::Sides::both);
            m.setNoise (true, -20.0f);
            rig.run (2.0);
            const auto a = m.read();
            rig.run (5.0);
            const auto b = m.read();
            expectGreaterThan ((double) (b.steadySamples - a.steadySamples), 4.9 * kRate);
            for (int band = 0; band < MixMeasure::kBands; ++band)
                expectWithinAbsoluteError (bandDb (a, b, band), -18.0, band < 3 ? 1.0 : 0.5,
                                           "band " + juce::String (MixMeasure::centres()[(size_t) band]) + " Hz");
            expectGreaterThan (b.inputPeak, 0.0f, "the input's peak is followed");
        }

        beginTest ("Noise floor: taken over, silent, steady; the input still measured");
        {
            Rig rig (wedge());
            auto& m = rig.engine.getMeasure();
            m.start (0, kMic, MixMeasure::Sides::both);
            rig.run (0.2);
            rig.run (0.5, true);
            expectEquals (rig.outEnergy[0], 0.0, "the bus is silent");
            expect (m.read().steady);
            expect (! m.read().noiseOn);
        }

        beginTest ("Sides: In-ear left only, the right side silent");
        {
            Rig rig (wedge());
            auto& m = rig.engine.getMeasure();
            m.start (0, kMic, MixMeasure::Sides::right);
            m.setNoise (true, -30.0f);
            rig.run (1.5);
            rig.run (0.5, true);
            expectEquals (rig.outEnergy[0], 0.0, "left silent");
            expectWithinAbsoluteError (rig.rmsDb (1), -30.0, 0.5);
        }

        beginTest ("Stop: the noise fades out, the bus is handed back to its signal and fader");
        {
            Rig rig (wedge(), -6.0f);
            auto& m = rig.engine.getMeasure();
            m.start (0, kMic, MixMeasure::Sides::both);
            m.setNoise (true, -12.0f);
            rig.run (1.5);
            m.stop();
            expect (m.isBusy(), "busy until handed back");
            rig.run (0.15);
            expect (! m.isBusy(), "handed back within 150 ms");
            rig.run (0.5, true);
            expectWithinAbsoluteError (rig.rmsDb (0), sineDb (-6.0f), 0.2, "the sine at its fader again");
            expect (m.start (0, kMic, MixMeasure::Sides::both), "a new run can start");
        }
    }
};

static MixMeasureTests mixMeasureTests;
