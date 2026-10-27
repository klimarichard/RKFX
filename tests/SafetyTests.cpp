#include <juce_audio_basics/juce_audio_basics.h>

#include "safety/DCBlocker.h"
#include "safety/NanGuard.h"
#include "safety/SoftClip.h"

#include <cmath>
#include <limits>

using namespace rkfx;

class SafetyTests : public juce::UnitTest
{
public:
    SafetyTests() : juce::UnitTest ("RKFX safety layer") {}

    void runTest() override
    {
        beginTest ("Soft clip is bounded, odd, monotonic and transparent near zero");
        {
            expectEquals (softClip (0.0f), 0.0f);
            expectWithinAbsoluteError (softClip (0.01f), 0.01f, 1.0e-6f);

            float previous = -2.0f;
            for (float x = -100.0f; x <= 100.0f; x += 0.01f)
            {
                const auto y = softClip (x);
                expect (y >= -1.0f && y <= 1.0f);
                expect (y >= previous);
                expectWithinAbsoluteError (softClip (-x), -y, 1.0e-6f);
                previous = y;
            }

            expectEquals (softClip (1.0e30f), 1.0f);
            expectEquals (softClip (-1.0e30f), -1.0f);
            expectWithinAbsoluteError (softClip (0.5f, 2.0f), 0.5f, 0.01f);
        }

        beginTest ("Feedback loop through the soft clipper stays bounded");
        {
            // A runaway loop (loop gain 1.5) with a loud input never escapes the clipper.
            float y = 0.0f;
            for (int i = 0; i < 100000; ++i)
            {
                const auto x = (i % 100 < 50) ? 10.0f : -10.0f;
                y = softClip (x + 1.5f * y);
                expect (std::isfinite (y) && std::abs (y) <= 1.0f);
            }
        }

        beginTest ("DC blocker removes offset and passes audio");
        {
            constexpr double rate = 48000.0;
            DCBlocker blocker;
            blocker.prepare (rate);

            float out = 0.0f;
            for (int i = 0; i < (int) rate * 2; ++i)
                out = blocker.process (0.5f);
            expect (std::abs (out) < 1.0e-3f, "DC should decay");

            // A 1 kHz sine passes at (very nearly) unity.
            blocker.reset();
            float peak = 0.0f;
            for (int i = 0; i < (int) rate; ++i)
            {
                const auto x = std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f * (float) i / (float) rate);
                const auto y2 = blocker.process (x);
                if (i > (int) rate / 2)
                    peak = std::max (peak, std::abs (y2));
            }
            expectWithinAbsoluteError (peak, 1.0f, 0.01f);
        }

        beginTest ("NaN guard detects NaN and infinity");
        {
            juce::AudioBuffer<float> buffer (2, 64);
            buffer.clear();
            for (int i = 0; i < 64; ++i)
                buffer.setSample (1, i, std::sin ((float) i));

            expect (allFinite (buffer.getArrayOfReadPointers(), 2, 64));

            buffer.setSample (1, 17, std::numeric_limits<float>::quiet_NaN());
            expect (! allFinite (buffer.getArrayOfReadPointers(), 2, 64));

            buffer.setSample (1, 17, std::numeric_limits<float>::infinity());
            expect (! allFinite (buffer.getArrayOfReadPointers(), 2, 64));

            buffer.setSample (1, 17, -std::numeric_limits<float>::infinity());
            expect (! allFinite (buffer.getArrayOfReadPointers(), 2, 64));
        }
    }
};

static SafetyTests safetyTests;
