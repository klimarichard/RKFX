#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKFreqShift";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 100.0f;
        c.monoToStereo = true;                // stereo spread works on a mono source
        c.accent = juce::Colour (0xff2e9fd6); // Modulation family, sky blue
        return c;
    }
}

RKFreqShiftProcessor::RKFreqShiftProcessor()
    : rkfx::ProcessorBase (makeConfig(), FreqShiftParams::create())
{
    using namespace FreqShiftParams;

    for (int s = 0; s < kNumShifts; ++s)
        shiftParams[(size_t) s] = { findParam (id (s, "on")),     findParam (id (s, "freq")),
                                    findParam (id (s, "fine")),   findParam (id (s, "range")),
                                    findParam (id (s, "updown")), findParam (id (s, "feedback")),
                                    findParam (id (s, "level")) };

    spread  = findParam ("spread");
    lowCut  = findParam ("lowcut");
    highCut = findParam ("highcut");
}

void RKFreqShiftProcessor::updateEngine()
{
    for (int s = 0; s < FreqShiftParams::kNumShifts; ++s)
    {
        const auto& p = shiftParams[(size_t) s];
        const auto fine = valueOf (p.range) < 0.5f;

        FreqShiftEngine::ShiftSettings settings;
        settings.on       = valueOf (p.on) >= 0.5f;
        settings.hz       = fine ? valueOf (p.fine) : valueOf (p.freq);
        settings.down     = valueOf (p.updown) / 100.0f;
        settings.feedback = valueOf (p.feedback) / 100.0f;
        settings.level    = valueOf (p.level) / 100.0f;
        engine.setShift (s, settings);
    }

    engine.setSpread (valueOf (spread) / 100.0f);
    engine.setLowCut (valueOf (lowCut));
    engine.setHighCut (valueOf (highCut));
}

void RKFreqShiftProcessor::prepareDsp (double sampleRate, int, int)
{
    updateEngine();
    engine.prepare (sampleRate);
}

void RKFreqShiftProcessor::resetDsp()
{
    updateEngine();
    engine.reset();
}

void RKFreqShiftProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    updateEngine();
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKFreqShiftProcessor::createEditor()
{
    return new RKFreqShiftEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKFreqShiftProcessor();
}
