#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKFlange";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 50.0f;                 // equal blend = deepest flanging
        c.monoToStereo = true;                // stereo phase on a mono source
        c.accent = juce::Colour (0xff3b82cc); // Modulation family, cornflower
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }
}

RKFlangeProcessor::RKFlangeProcessor()
    : rkfx::ProcessorBase (makeConfig(), FlangeParams::create()),
      rate (findParam ("rate")), depth (findParam ("depth")), manual (findParam ("manual")),
      feedback (findParam ("feedback")), sync (findParam ("sync")), division (findParam ("division")),
      phase (findParam ("phase"))
{
}

FlangeEngine::Settings RKFlangeProcessor::currentSettings() const
{
    FlangeEngine::Settings s;
    s.rateHz   = valueOf (rate);
    s.depth    = valueOf (depth) / 100.0f;
    s.manualMs = valueOf (manual);
    s.feedback = valueOf (feedback) / 100.0f;
    s.sync     = valueOf (sync) >= 0.5f;
    s.division = juce::roundToInt (valueOf (division));
    s.phaseDeg = valueOf (phase);
    return s;
}

void RKFlangeProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKFlangeProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKFlangeProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), getTransport());
}

juce::AudioProcessorEditor* RKFlangeProcessor::createEditor()
{
    return new RKFlangeEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKFlangeProcessor();
}
#endif
