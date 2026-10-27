#include <juce_audio_basics/juce_audio_basics.h>

#include "FreqShiftEngine.h"

#include <cstdio>
#include <functional>

/** CPU benchmarks at 48 kHz with 64-sample blocks (the spec's reference
    setting), stereo. Prints microseconds per block and the share of the
    block's real-time budget. With --record <file>, appends a CSV line per
    case so builds can be compared over time. */

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 64;
    constexpr int kBlocks = 50000;   // ~67 s of audio per case

    struct Case
    {
        const char* plugin;
        const char* name;
        std::function<void (FreqShiftEngine&)> setup;
    };

    double run (const Case& c)
    {
        FreqShiftEngine engine;
        c.setup (engine);
        engine.prepare (kRate);

        juce::Random rng (1);
        juce::AudioBuffer<float> buffer (2, kBlock);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

        float sink = 0.0f;
        const auto start = juce::Time::getHighResolutionTicks();

        for (int b = 0; b < kBlocks; ++b)
        {
            engine.process (buffer.getArrayOfWritePointers(), 2, kBlock);
            sink += buffer.getSample (0, 0);
            buffer.setSample (0, 0, rng.nextFloat()); // keep the input alive
        }

        const auto seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        if (sink == 12345.0f) std::printf (" ");
        return seconds * 1.0e6 / kBlocks;
    }
}

int main (int argc, char** argv)
{
    const Case cases[] = {
        { "RKFreqShift", "default (1 shift)", [] (FreqShiftEngine& e) { e.setShift (0, { true, 100.0f, 0.0f, 0.0f, 1.0f }); } },
        { "RKFreqShift", "4 shifts, feedback, spread", [] (FreqShiftEngine& e)
          {
              for (int k = 0; k < 4; ++k)
                  e.setShift (k, { true, 100.0f * (float) (k + 1), 0.3f, 0.5f, 1.0f });
              e.setSpread (0.5f);
          } },
    };

    juce::File recordFile;
    if (argc >= 3 && juce::String (argv[1]) == "--record")
        recordFile = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);

    const auto budget = kBlock * 1.0e6 / kRate;

    for (const auto& c : cases)
    {
        const auto us = run (c);
        std::printf ("%-12s %-28s %7.2f us/block  %5.2f %% of real time\n", c.plugin, c.name, us, 100.0 * us / budget);

        if (recordFile != juce::File())
        {
            if (! recordFile.existsAsFile())
                recordFile.replaceWithText ("date,plugin,case,us_per_64_block,percent_realtime\n");
            recordFile.appendText (juce::Time::getCurrentTime().toISO8601 (false) + "," + c.plugin + ",\"" + c.name + "\","
                                   + juce::String (us, 3) + "," + juce::String (100.0 * us / budget, 3) + "\n");
        }
    }

    return 0;
}
