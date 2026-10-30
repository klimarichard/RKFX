#include <juce_audio_basics/juce_audio_basics.h>

#include "ChorusEngine.h"
#include "CompressorEngine.h"
#include "DeEssEngine.h"
#include "DelayEngine.h"
#include "DistortEngine.h"
#include "DoubleEngine.h"
#include "FilterEngine.h"
#include "FlangeEngine.h"
#include "GateEngine.h"
#include "LimitEngine.h"
#include "FreqShiftEngine.h"
#include "GraphEQEngine.h"
#include "ParaEQEngine.h"
#include "PhaseEngine.h"
#include "ReverbEngine.h"
#include "RotaryEngine.h"
#include "SaturateEngine.h"
#include "SwellEngine.h"
#include "TransientEngine.h"
#include "TremoloEngine.h"
#include "UtilityEngine.h"
#include "VibratoEngine.h"
#include "VocodeEngine.h"
#include "OctaveEngine.h"
#include "AcousticEngine.h"
#include "PreampEngine.h"
#include "SynthEngine.h"
#include "VoiceEngine.h"
#include "RingModEngine.h"
#include "LoopEngine.h"
#include "MultiCompEngine.h"
#include "RideEngine.h"
#include "FollowEngine.h"
#include "TriggerEngine.h"
#include "HoldEngine.h"
#include "PolyPitchEngine.h"
#include "RemapEngine.h"
#include "MixEngine.h"

#include <cstdio>
#include <functional>
#include <memory>
#include <type_traits>

/** CPU benchmarks at 48 kHz with 64-sample blocks (the spec's reference
    setting), stereo. Prints microseconds per block and the share of the
    block's real-time budget. With --record <file>, appends a CSV line per
    case so builds can be compared over time; --only <Plugin> runs one
    plugin's cases. */

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
            if constexpr (std::is_same_v<Engine, CompressorEngine> || std::is_same_v<Engine, GateEngine>
                      || std::is_same_v<Engine, VocodeEngine> || std::is_same_v<Engine, RideEngine>)
                engine->process (b.getArrayOfWritePointers(), b.getNumChannels(), b.getNumSamples(), nullptr, 0);
            else if constexpr (std::is_same_v<Engine, FilterEngine> || std::is_same_v<Engine, TremoloEngine>
                               || std::is_same_v<Engine, ChorusEngine> || std::is_same_v<Engine, FlangeEngine>
                               || std::is_same_v<Engine, PhaseEngine> || std::is_same_v<Engine, DelayEngine>
                               || std::is_same_v<Engine, VibratoEngine> || std::is_same_v<Engine, RingModEngine>)
                engine->process (b.getArrayOfWritePointers(), b.getNumChannels(), b.getNumSamples(), {});
            else if constexpr (std::is_same_v<Engine, FollowEngine> || std::is_same_v<Engine, TriggerEngine>)
            {
                static juce::MidiBuffer midi;   // MIDI out, cleared every block
                midi.clear();
                engine->process (b.getArrayOfWritePointers(), b.getNumChannels(), b.getNumSamples(), midi);
            }
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
        { "RKParaEQ", "all 5 gain bands dynamic, 48 dB cuts", []
          {
              return makeEngine<ParaEQEngine> ([] (ParaEQEngine& e)
              {
                  auto s = ParaEQEngine::defaults();
                  s.highPass = { true, 60.0f, 4 };
                  s.lowPass = { true, 15000.0f, 4 };
                  for (auto& b : s.bands)
                  {
                      b.gainDb = 4.0f;
                      b.dynamic.on = true;
                  }
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
        { "RKFlange", "default", []
          {
              return makeEngine<FlangeEngine> ([] (FlangeEngine& e) { e.setSettings ({}); });
          } },
        { "RKPhase", "default (4 stages)", []
          {
              return makeEngine<PhaseEngine> ([] (PhaseEngine& e) { e.setSettings ({}); });
          } },
        { "RKPhase", "Uni-Vibe", []
          {
              return makeEngine<PhaseEngine> ([] (PhaseEngine& e)
              {
                  PhaseEngine::Settings s;
                  s.univibe = true;
                  e.setSettings (s);
              });
          } },
        { "RKPhase", "12 stages, envelope", []
          {
              return makeEngine<PhaseEngine> ([] (PhaseEngine& e)
              {
                  PhaseEngine::Settings s;
                  s.stages = 12;
                  s.envAmount = 0.5f;
                  e.setSettings (s);
              });
          } },
        { "RKDelay", "default (digital, stereo)", []
          {
              return makeEngine<DelayEngine> ([] (DelayEngine& e) { e.setSettings ({}); });
          } },
        { "RKDelay", "stereo, shimmer 60%", []
          {
              return makeEngine<DelayEngine> ([] (DelayEngine& e)
              {
                  DelayEngine::Settings s;
                  s.feedback = 0.55f;
                  s.shimmer = 0.6f;
                  e.setSettings (s);
              });
          } },
        { "RKDelay", "tape, ping-pong, modulation, ducking", []
          {
              return makeEngine<DelayEngine> ([] (DelayEngine& e)
              {
                  DelayEngine::Settings s;
                  s.mode = DelayEngine::Mode::tape;
                  s.stereo = DelayEngine::Stereo::pingPong;
                  s.modDepth = 0.3f;
                  s.duck = 0.5f;
                  e.setSettings (s);
              });
          } },
        { "RKSaturate", "default (tape, 2x)", []
          {
              return makeEngine<SaturateEngine> ([] (SaturateEngine& e) { e.setSettings ({}); });
          } },
        { "RKSaturate", "transformer, 4x", []
          {
              return makeEngine<SaturateEngine> ([] (SaturateEngine& e)
              {
                  SaturateEngine::Settings s;
                  s.character = SaturateEngine::Character::transformer;
                  s.oversampling = 4;
                  s.tone = 0.3f;
                  e.setSettings (s);
              });
          } },
        { "RKDistort", "default (overdrive, 4x)", []
          {
              return makeEngine<DistortEngine> ([] (DistortEngine& e) { e.setSettings ({}); });
          } },
        { "RKDistort", "fuzz, tight, mid, 4x12 cab, 4x", []
          {
              return makeEngine<DistortEngine> ([] (DistortEngine& e)
              {
                  DistortEngine::Settings s;
                  s.type = DistortEngine::Type::fuzz;
                  s.tightHz = 120.0f;
                  s.midDb = 4.0f;
                  s.cab = DistortEngine::Cab::large;
                  e.setSettings (s);
              });
          } },
        { "RKVerb", "default (hall)", []
          {
              return makeEngine<ReverbEngine> ([] (ReverbEngine& e) { e.setSettings ({}); });
          } },
        { "RKVerb", "hall, shimmer 50%", []
          {
              return makeEngine<ReverbEngine> ([] (ReverbEngine& e)
              {
                  ReverbEngine::Settings s;
                  s.shimmer = 0.5f;
                  e.setSettings (s);
              });
          } },
        { "RKLimit", "default", []
          {
              return makeEngine<LimitEngine> ([] (LimitEngine& e) { e.setSettings ({}); });
          } },
        { "RKLimit", "+12 dB input, auto release, 4x", []
          {
              return makeEngine<LimitEngine> ([] (LimitEngine& e)
              {
                  LimitEngine::Settings s;
                  s.inputDb = 12.0f;
                  s.autoRelease = true;
                  s.oversampling = 4;
                  e.setSettings (s);
              });
          } },
        { "RKTransient", "attack + sustain, focus filters", []
          {
              return makeEngine<TransientEngine> ([] (TransientEngine& e)
              {
                  TransientEngine::Settings s;
                  s.attack = 0.5f;
                  s.sustain = -0.3f;
                  s.focusHighPassHz = 100.0f;
                  s.focusLowPassHz = 8000.0f;
                  e.setSettings (s);
              });
          } },
        { "RKDeEss", "default (relative, split)", []
          {
              return makeEngine<DeEssEngine> ([] (DeEssEngine& e) { e.setSettings ({}); });
          } },
        { "RKMultiComp", "default (3 bands, RMS)", []
          {
              return makeEngine<MultiCompEngine> ([] (MultiCompEngine& e) { e.setSettings ({}); });
          } },
        { "RKMultiComp", "4 bands, peak, auto release, Dry/Wet 50%", []
          {
              return makeEngine<MultiCompEngine> ([] (MultiCompEngine& e)
              {
                  MultiCompEngine::Settings s;
                  s.fourBands = true;
                  s.detection = rkfx::Dynamics::LevelDetector::Mode::peak;
                  s.autoRelease = true;
                  s.mix = 0.5f;
                  e.setSettings (s);
              });
          } },
        { "RKRide", "default", []
          {
              return makeEngine<RideEngine> ([] (RideEngine& e) { e.setSettings ({}); });
          } },
        { "RKFollow", "default (notes, bend, expression)", []
          {
              return makeEngine<FollowEngine> ([] (FollowEngine& e) { e.setSettings ({}); });
          } },
        { "RKTrigger", "dual, filters", []
          {
              return makeEngine<TriggerEngine> ([] (TriggerEngine& e)
              {
                  TriggerEngine::Settings s;
                  s.mode = TriggerEngine::Mode::dual;
                  s.triggers[0].lowPassHz = 2000.0f;
                  s.triggers[1].highPassHz = 100.0f;
                  e.setSettings (s);
              });
          } },
        { "RKHold", "four texture layers, octave down", []
          {
              // Fill the history, then hold four layers (texture: the busier kind), before timing.
              auto engine = std::make_shared<HoldEngine>();
              HoldEngine::Settings s;
              s.add = true;
              s.octave = -1;
              s.mode = HoldEngine::Mode::texture;
              engine->setSettings (s);
              engine->prepare (kRate);
              juce::Random rng (7);
              juce::AudioBuffer<float> b (2, kBlock);
              for (int block = 0; block < 1000; ++block)
              {
                  for (int ch = 0; ch < 2; ++ch)
                      for (int i = 0; i < kBlock; ++i)
                          b.setSample (ch, i, rng.nextFloat() - 0.5f);
                  engine->process (b.getArrayOfWritePointers(), 2, kBlock);
                  if (block >= 900 && block % 20 == 0)
                      engine->hold();
              }
              return Processor ([engine] (juce::AudioBuffer<float>& buffer)
              {
                  engine->process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
              });
          } },
        { "RKPolyPitch", "whammy, guitar, +12", []
          {
              return makeEngine<PolyPitchEngine> ([] (PolyPitchEngine& e)
              {
                  PolyPitchEngine::Settings s;
                  s.heel = s.toe = 12.0f;
                  e.setSettings (s);
              });
          } },
        { "RKPolyPitch", "whammy, bass, +24 (most splices)", []
          {
              return makeEngine<PolyPitchEngine> ([] (PolyPitchEngine& e)
              {
                  PolyPitchEngine::Settings s;
                  s.range = PolyPitchEngine::Range::bass;
                  s.heel = s.toe = 24.0f;
                  e.setSettings (s);
              });
          } },
        { "RKPolyPitch", "detune", []
          {
              return makeEngine<PolyPitchEngine> ([] (PolyPitchEngine& e)
              {
                  PolyPitchEngine::Settings s;
                  s.mode = PolyPitchEngine::Mode::detune;
                  e.setSettings (s);
              });
          } },
        { "RKRemap", "64 outputs, half of them mono sums of pairs", []
          {
              // Its own 64-channel buffer (the benchmark's is stereo).
              auto engine = std::make_shared<RemapEngine>();
              auto buffer = std::make_shared<juce::AudioBuffer<float>> (RemapEngine::kChannels, kBlock);
              RemapEngine::Settings s;
              for (int pair = 0; pair < RemapEngine::kPairs; ++pair)
                  s.inputLinks[(size_t) pair] = true;
              for (int k = 0; k < RemapEngine::kChannels; ++k)
                  s.outputs[(size_t) k].source = k % 2 == 0 ? RemapEngine::kFirstPairSource + k / 2 : RemapEngine::kChannels - k;
              engine->setSettings (s);
              engine->prepare (kRate, kBlock);
              juce::Random rng (3);
              for (int ch = 0; ch < RemapEngine::kChannels; ++ch)
                  for (int i = 0; i < kBlock; ++i)
                      buffer->setSample (ch, i, rng.nextFloat() - 0.5f);
              return Processor ([engine, buffer] (juce::AudioBuffer<float>&)
              {
                  engine->process (buffer->getArrayOfWritePointers(), RemapEngine::kChannels, kBlock);
              });
          } },
        { "RKMix", "32 mono channels to the main and mono sum, delay on", []
          {
              auto engine = std::make_shared<MixEngine>();
              auto in = std::make_shared<juce::AudioBuffer<float>> (MixEngine::kChannels, kBlock);
              auto out = std::make_shared<juce::AudioBuffer<float>> (MixEngine::kChannels, kBlock);
              MixEngine::Settings s;
              for (int ch = 0; ch < MixEngine::kChannels; ++ch)
              {
                  s.channels[(size_t) ch].pan = (float) (ch % 5) * 0.4f - 0.8f;
                  s.channels[(size_t) ch].delayOn = true;
                  s.channels[(size_t) ch].delayMs = 1.5f;
              }
              s.main.monoOutput = 3;
              engine->prepare (kRate, kBlock);
              engine->setSettings (s);
              engine->reset();
              juce::Random rng (5);
              for (int ch = 0; ch < MixEngine::kChannels; ++ch)
                  for (int i = 0; i < kBlock; ++i)
                      in->setSample (ch, i, rng.nextFloat() - 0.5f);
              return Processor ([engine, in, out] (juce::AudioBuffer<float>&)
              {
                  engine->process (in->getArrayOfReadPointers(), out->getArrayOfWritePointers(), kBlock, false);
              });
          } },
        { "RKSwell", "default", []
          {
              return makeEngine<SwellEngine> ([] (SwellEngine& e) { e.setSettings ({}); });
          } },
        { "RKRotary", "fast, no drive", []
          {
              return makeEngine<RotaryEngine> ([] (RotaryEngine& e)
              {
                  RotaryEngine::Settings s;
                  s.speed = RotaryEngine::Speed::fast;
                  e.setSettings (s);
              });
          } },
        { "RKRotary", "fast, drive 50%, 2x", []
          {
              return makeEngine<RotaryEngine> ([] (RotaryEngine& e)
              {
                  RotaryEngine::Settings s;
                  s.speed = RotaryEngine::Speed::fast;
                  s.drive = 0.5f;
                  e.setSettings (s);
              });
          } },
        { "RKVibrato", "default", []
          {
              return makeEngine<VibratoEngine> ([] (VibratoEngine& e) { e.setSettings ({}); });
          } },
        { "RKVocode", "16 bands, fixed saw", []
          {
              return makeEngine<VocodeEngine> ([] (VocodeEngine& e) { e.setSettings ({}); });
          } },
        { "RKVocode", "32 bands, 8 MIDI voices", []
          {
              return makeEngine<VocodeEngine> ([] (VocodeEngine& e)
              {
                  VocodeEngine::Settings s;
                  s.bands = 32;
                  s.pitchMode = VocodeEngine::PitchMode::midi;
                  e.setSettings (s);
                  for (int n = 0; n < 8; ++n)
                      e.noteOn (48 + 3 * n, 1.0f);
              });
          } },
        { "RKUtility", "width, bass mono, delay, balance", []
          {
              return makeEngine<UtilityEngine> ([] (UtilityEngine& e)
              {
                  UtilityEngine::Settings s;
                  s.width = 1.4f;
                  s.bassMonoHz = 120.0f;
                  s.delayLeftMs = 1.2f;
                  s.pan = 0.2f;
                  e.setSettings (s);
              });
          } },
        { "RKLoop", "overdubbing", []
          {
              // A one-second loop, then overdubbing over it.
              auto engine = std::make_shared<LoopEngine>();
              engine->prepare (kRate);
              engine->press (LoopEngine::Button::record);
              juce::AudioBuffer<float> second (2, (int) kRate);
              second.clear();
              engine->process (second.getArrayOfWritePointers(), 2, second.getNumSamples());
              engine->press (LoopEngine::Button::record);
              engine->press (LoopEngine::Button::record);
              return Processor ([engine] (juce::AudioBuffer<float>& b)
              {
                  engine->process (b.getArrayOfWritePointers(), b.getNumChannels(), b.getNumSamples());
              });
          } },
        { "RKRingMod", "tracked, square, LFO", []
          {
              return makeEngine<RingModEngine> ([] (RingModEngine& e)
              {
                  RingModEngine::Settings s;
                  s.mode = RingModEngine::Mode::tracked;
                  s.wave = RingModEngine::Wave::square;
                  s.lfoDepth = 0.3f;
                  e.setSettings (s);
              });
          } },
        { "RKSynth", "saw + sub, resonant filter", []
          {
              return makeEngine<SynthEngine> ([] (SynthEngine& e)
              {
                  SynthEngine::Settings s;
                  s.sub = 0.5f;
                  s.resonance = 0.6f;
                  e.setSettings (s);
              });
          } },
        { "RKVoice", "four voices, a held chord", []
          {
              return makeEngine<VoiceEngine> ([] (VoiceEngine& e)
              {
                  VoiceEngine::Settings s;
                  s.voices[0].position = 1;
                  s.voices[1].position = -1;
                  s.voices[2].position = 2;
                  s.voices[3].position = -2;
                  e.setSettings (s);
                  for (const auto note : { 60, 64, 67 })
                      e.noteOn (note);
              });
          } },
        { "RKPreamp", "hot-rod (3 stages), 2x, large cab", []
          {
              return makeEngine<PreampEngine> ([] (PreampEngine& e)
              {
                  PreampEngine::Settings s;
                  s.voicing = PreampEngine::Voicing::hotRod;
                  s.bright = true;
                  s.cab = rkfx::CabType::large;
                  e.setSettings (s);
              });
          } },
        { "RKPreamp", "hot-rod (3 stages), 4x, large cab", []
          {
              return makeEngine<PreampEngine> ([] (PreampEngine& e)
              {
                  PreampEngine::Settings s;
                  s.voicing = PreampEngine::Voicing::hotRod;
                  s.cab = rkfx::CabType::large;
                  s.oversampling = 4;
                  e.setSettings (s);
              });
          } },
        { "RKAcoustic", "violin, every band, notch, compressor", []
          {
              return makeEngine<AcousticEngine> ([] (AcousticEngine& e)
              {
                  AcousticEngine::Settings s;
                  s.voicing = AcousticEngine::Voicing::violin;
                  s.highPassHz = 80.0f;
                  s.lowDb = 2.0f;
                  s.midDb = -3.0f;
                  s.trebleDb = 1.0f;
                  s.airDb = 3.0f;
                  s.notchHz = 300.0f;
                  s.compress = 0.5f;
                  e.setSettings (s);
              });
          } },
        { "RKOctave", "both octaves, natural", []
          {
              return makeEngine<OctaveEngine> ([] (OctaveEngine& e)
              {
                  OctaveEngine::Settings s;
                  s.octave2 = 0.5f;
                  e.setSettings (s);
              });
          } },
    };

    // --record <csv>: append the results; --only <Plugin>: run that plugin's cases alone.
    juce::File recordFile;
    juce::String only;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (juce::String (argv[i]) == "--record")
            recordFile = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
        else if (juce::String (argv[i]) == "--only")
            only = argv[i + 1];
    }

    const auto budget = kBlock * 1.0e6 / kRate;

    for (const auto& c : cases)
    {
        if (only.isNotEmpty() && only != c.plugin)
            continue;

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
