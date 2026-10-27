#include <juce_audio_basics/juce_audio_basics.h>

#include "plugin/OutputStage.h"

#include <cmath>

using namespace rkfx;

/** Null tests and glide behaviour of the standard controls. */
class OutputStageTests : public juce::UnitTest
{
public:
    OutputStageTests() : juce::UnitTest ("RKFX output stage") {}

    void runTest() override
    {
        beginTest ("Transparent defaults null exactly");
        {
            OutputStage stage;
            stage.prepare (kRate);
            stage.setMix (1.0f);
            stage.setOutputDecibels (0.0f);
            stage.setBypassed (false);
            stage.reset();

            auto dry = noise (1);
            auto io = dry;
            run (stage, dry, io);
            expect (identical (io, dry));
        }

        beginTest ("Dry/wet ends are exact, middle is equal power");
        {
            auto dry = noise (2);
            auto wet = noise (3);

            OutputStage stage;
            stage.prepare (kRate);
            stage.setMix (0.0f);
            stage.reset();
            auto io = wet;
            run (stage, dry, io);
            expect (identical (io, dry), "0% mix = dry only");

            stage.setMix (1.0f);
            stage.reset();
            io = wet;
            run (stage, dry, io);
            expect (identical (io, wet), "100% mix = wet only");

            stage.setMix (0.5f);
            stage.reset();
            io = wet;
            run (stage, dry, io);
            const auto g = std::cos (juce::MathConstants<float>::pi * 0.25f);
            expectWithinAbsoluteError (io.getSample (0, 10), g * dry.getSample (0, 10) + g * wet.getSample (0, 10), 1.0e-6f);
        }

        beginTest ("Output gain");
        {
            OutputStage stage;
            stage.prepare (kRate);
            stage.setOutputDecibels (-6.0f);
            stage.reset();

            auto dry = noise (4);
            auto io = dry;
            run (stage, dry, io);
            expectWithinAbsoluteError (io.getSample (1, 100), dry.getSample (1, 100) * juce::Decibels::decibelsToGain (-6.0f), 1.0e-6f);
        }

        beginTest ("Bypass fades to exactly dry and stops needing wet");
        {
            OutputStage stage;
            stage.prepare (kRate);
            stage.setOutputDecibels (6.0f);
            stage.reset();
            stage.setBypassed (true);
            expect (stage.wetNeeded(), "still fading");

            auto dry = noise (5);
            juce::AudioBuffer<float> io;
            for (int block = 0; block < 20; ++block)
            {
                io = noise (6 + block);
                run (stage, dry, io);
            }

            expect (! stage.wetNeeded());
            expect (identical (io, dry));
        }

        beginTest ("Changes glide without jumps");
        {
            // A constant signal through a 0 dB -> -24 dB change: no step larger
            // than a smooth 20 ms glide allows.
            OutputStage stage;
            stage.prepare (kRate);
            stage.reset();
            stage.setOutputDecibels (-24.0f);
            stage.setMix (0.3f);

            juce::AudioBuffer<float> dry (1, kBlock), io (1, kBlock);
            float previous = 1.0f, maxStep = 0.0f;
            for (int block = 0; block < 10; ++block)
            {
                for (int i = 0; i < kBlock; ++i)
                {
                    dry.setSample (0, i, 1.0f);
                    io.setSample (0, i, 1.0f);
                }
                stage.process (dry.getArrayOfReadPointers(), io.getArrayOfWritePointers(), 1, kBlock);
                for (int i = 0; i < kBlock; ++i)
                {
                    maxStep = std::max (maxStep, std::abs (io.getSample (0, i) - previous));
                    previous = io.getSample (0, i);
                }
            }
            expect (maxStep < 0.01f, "max step " + juce::String (maxStep));
        }
    }

private:
    static constexpr double kRate = 48000.0;
    static constexpr int kBlock = 256;

    static juce::AudioBuffer<float> noise (int seed)
    {
        juce::Random rng (seed);
        juce::AudioBuffer<float> b (2, kBlock);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlock; ++i)
                b.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);
        return b;
    }

    static void run (OutputStage& stage, const juce::AudioBuffer<float>& dry, juce::AudioBuffer<float>& io)
    {
        stage.process (dry.getArrayOfReadPointers(), io.getArrayOfWritePointers(), io.getNumChannels(), io.getNumSamples());
    }

    static bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (! juce::exactlyEqual (a.getSample (ch, i), b.getSample (ch, i)))
                    return false;
        return true;
    }
};

static OutputStageTests outputStageTests;
