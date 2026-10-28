#include "dsp/LinkwitzRiley4.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace
{
    constexpr double kRate = 48000.0;

    /** Steady-state gain (RMS out / RMS in, so the sampling phase doesn't
        matter) of a sine at @p hz through the split: low, high and low + high. */
    void measure (double hz, float crossover, float& low, float& high, float& sum)
    {
        rkfx::LinkwitzRiley4 split;
        split.prepare (kRate);
        split.setFrequency (crossover);

        double in2 = 0.0, low2 = 0.0, high2 = 0.0, sum2 = 0.0;
        const auto settle = (int) kRate;   // one second
        for (int i = 0; i < settle + (int) kRate / 4; ++i)
        {
            const auto x = (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / kRate);
            float l = 0.0f, h = 0.0f;
            split.process (x, l, h);
            if (i >= settle)
            {
                in2 += x * x;
                low2 += l * l;
                high2 += h * h;
                sum2 += (l + h) * (l + h);
            }
        }

        low  = (float) std::sqrt (low2 / in2);
        high = (float) std::sqrt (high2 / in2);
        sum  = (float) std::sqrt (sum2 / in2);
    }

    float dB (float gain) { return 20.0f * std::log10 (std::max (gain, 1.0e-9f)); }
}

class LinkwitzRileyTests : public juce::UnitTest
{
public:
    LinkwitzRileyTests() : juce::UnitTest ("LinkwitzRiley4") {}

    void runTest() override
    {
        beginTest ("Low + high sums flat");
        for (const auto crossover : { 50.0f, 500.0f, 5000.0f })
            for (const auto hz : { 30.0, 100.0, 400.0, 500.0, 700.0, 2000.0, 5000.0, 12000.0 })
            {
                float low, high, sum;
                measure (hz, crossover, low, high, sum);
                expectWithinAbsoluteError (dB (sum), 0.0f, 0.05f,
                                           juce::String (hz) + " Hz, crossover " + juce::String (crossover));
            }

        beginTest ("Each band is -6 dB at the crossover and falls 24 dB/octave");
        {
            float low, high, sum;
            measure (1000.0, 1000.0f, low, high, sum);
            expectWithinAbsoluteError (dB (low), -6.02f, 0.1f);
            expectWithinAbsoluteError (dB (high), -6.02f, 0.1f);

            measure (4000.0, 1000.0f, low, high, sum);   // two octaves above
            expect (dB (low) < -45.0f, "low band at 4 kHz: " + juce::String (dB (low)));
            measure (250.0, 1000.0f, low, high, sum);    // two octaves below
            expect (dB (high) < -45.0f, "high band at 250 Hz: " + juce::String (dB (high)));
        }
    }
};

static LinkwitzRileyTests linkwitzRileyTests;
