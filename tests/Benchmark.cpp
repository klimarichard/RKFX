#include <juce_audio_basics/juce_audio_basics.h>

#include "ChorusEngine.h"
#include "CompressorEngine.h"
#include "DoubleEngine.h"
#include "FilterEngine.h"
#include "GateEngine.h"
#include "FreqShiftEngine.h"
#include "GraphEQEngine.h"
#include "ParaEQEngine.h"
#include "TremoloEngine.h"

#include <cstdio>
#include <functional>
#include <memory>
#include <type_traits>

/** CPU benchmarks at 48 kHz with 64-sample blocks (the spec's reference
    setting), stereo. Prints microseconds per block and the share of the
    block's real-time budget. With --record <file>, appends a CSV line per
    case so builds can be compared over time. */

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 64;
    constexpr int kBlocks = 50000;   // ~67 s of audio per case

    using Processor = std::function<void (juce::AudioBuffer<float>&)>;

    /** Each case builds a prepared engine and returns the per-block call. */
    struct Case
    {
        const char* plugin;
        const char* name;
        std::function<Processor()> make;
    };

    template <typename Engine, typename Setup>
    Processor makeEngine (Setup setup)
    {
        auto engine = std::make_shared<Engine>();
        setup (*engine);
        engine->prepare (kRate);
        return [engine] (juce::AudioBuffer<float>& b)
        {
            if constexpr (std::is_same_v<Engine, CompressorEngine> || std::is_same_v<Engine, GateEngine>)
                engine->process (b.getArrayOfWritePointers(), b.getNumChannels(), b.getNumSamples(), nullptr, 0);
            else if constexpr (std::is_same_v<Engine, FilterEngine> || std::is_same_v<Engine, TremoloEngine>
                               || std::is_same_v<Engine, ChorusEngine>)
                engine->process (b.getArrayOfWritePointers(), b.getNumChannels(), b.getNumSamples(), {});
            else
                engine->process (b.getArrayOfWritePointers(), b.getNumChannels(), b.getNumSamples());
        };
    }

    double run (const Case& c)
    {
        auto process = c.make();

        juce::Random rng (1);
        juce::AudioBuffer<float> buffer (2, kBlock);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

        float sink = 0.0f;
        const auto start = juce::Time::getHighResolutionTicks();

        for (int b = 0; b < kBlocks; ++b)
        {
            process (buffer);
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
        { "RKFreqShift", "default (1 shift)", []
          {
              return makeEngine<FreqShiftEngine> ([] (FreqShiftEngine& e) { e.setShift (0, { true, 100.0f, 0.0f, 0.0f, 1.0f }); });
          } },
        { "RKFreqShift", "4 shifts, feedback, spread", []
          {
              return makeEngine<FreqShiftEngine> ([] (FreqShiftEngine& e)
              {
                  for (int k = 0; k < 4; ++k)
                      e.setShift (k, { true, 100.0f * (float) (k + 1), 0.3f, 0.5f, 1.0f });
                  e.setSpread (0.5f);
              });
          } },
        { "RKFilter", "default (LP 12, static)", []
          {
              return makeEngine<FilterEngine> ([] (FilterEngine& e) { e.setSettings ({}); });
          } },
        { "RKFilter", "48 dB, envelope + LFO", []
          {
              return makeEngine<FilterEngine> ([] (FilterEngine& e)
              {
                  FilterEngine::Settings s;
                  s.stages = 4;
                  s.resonance = 0.7f;
                  s.envAmount = 0.5f;
                  s.lfoDepth = 0.5f;
                  s.lfoPhaseDeg = 90.0f;
                  e.setSettings (s);
              });
          } },
        { "RKParaEQ", "default (transparent)", []
          {
              return makeEngine<ParaEQEngine> ([] (ParaEQEngine& e) { e.setSettings (ParaEQEngine::defaults()); });
          } },
        { "RKParaEQ", "all 7 bands active, 48 dB cuts", []
          {
              return makeEngine<ParaEQEngine> ([] (ParaEQEngine& e)
              {
                  auto s = ParaEQEngine::defaults();
                  s.highPass = { true, 60.0f, 4 };
                  s.lowPass = { true, 15000.0f, 4 };
                  for (auto& b : s.bands)
                      b.gainDb = 4.0f;
                  e.setSettings (s);
              });
          } },
        { "RKGraphEQ", "default (transparent)", []
          {
              return makeEngine<GraphEQEngine> ([] (GraphEQEngine& e) { e.setSettings ({}); });
          } },
        { "RKGraphEQ", "31 bands, all active", []
          {
              return makeEngine<GraphEQEngine> ([] (GraphEQEngine& e)
              {
                  GraphEQEngine::Settings s;
                  s.mode = GraphEQEngine::Mode::thirtyOne;
                  for (auto& g : s.gainsDb[2])
                      g = 3.0f;
                  e.setSettings (s);
              });
          } },
        { "RKCompress", "default (RMS, internal)", []
          {
              return makeEngine<CompressorEngine> ([] (CompressorEngine& e) { e.setSettings ({}); });
          } },
        { "RKCompress", "peak, auto release, sidechain HPF", []
          {
              return makeEngine<CompressorEngine> ([] (CompressorEngine& e)
              {
                  CompressorEngine::Settings s;
                  s.detection = rkfx::Dynamics::LevelDetector::Mode::peak;
                  s.autoRelease = true;
                  s.sidechainHpfHz = 100.0f;
                  s.link = 0.5f;
                  e.setSettings (s);
              });
          } },
        { "RKGate", "default (gate, linked)", []
          {
              return makeEngine<GateEngine> ([] (GateEngine& e) { e.setSettings ({}); });
          } },
        { "RKGate", "expander, unlinked, key filters", []
          {
              return makeEngine<GateEngine> ([] (GateEngine& e)
              {
                  GateEngine::Settings s;
                  s.mode = GateEngine::Mode::expander;
                  s.link = false;
                  s.keyHighPassHz = 100.0f;
                  s.keyLowPassHz = 5000.0f;
                  e.setSettings (s);
              });
          } },
        { "RKTremolo", "default (classic sine)", []
          {
              return makeEngine<TremoloEngine> ([] (TremoloEngine& e) { e.setSettings ({}); });
          } },
        { "RKTremolo", "harmonic, square, stereo phase", []
          {
              return makeEngine<TremoloEngine> ([] (TremoloEngine& e)
              {
                  TremoloEngine::Settings s;
                  s.mode = TremoloEngine::Mode::harmonic;
                  s.shape = rkfx::LFO::Shape::square;
                  s.phaseDeg = 90.0f;
                  e.setSettings (s);
              });
          } },
        { "RKChorus", "default (1 voice)", []
          {
              return makeEngine<ChorusEngine> ([] (ChorusEngine& e) { e.setSettings ({}); });
          } },
        { "RKChorus", "4 voices, tone, width", []
          {
              return makeEngine<ChorusEngine> ([] (ChorusEngine& e)
              {
                  ChorusEngine::Settings s;
                  s.voices = 4;
                  s.tone = -0.3f;
                  s.width = 0.7f;
                  e.setSettings (s);
              });
          } },
        { "RKDouble", "default", []
          {
              return makeEngine<DoubleEngine> ([] (DoubleEngine& e) { e.setSettings ({}); });
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
