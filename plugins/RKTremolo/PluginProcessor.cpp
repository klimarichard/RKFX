#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKTremolo";
        c.stateVersion = 1;
        c.hasMix = false;                     // depth already sets the blend
        c.monoToStereo = true;                // auto-pan on a mono source
        c.accent = juce::Colour (0xff3fb2c4); // Modulation family, cyan blue
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }
}

RKTremoloProcessor::RKTremoloProcessor()
    : rkfx::ProcessorBase (makeConfig(), TremoloParams::create()),
      mode (findParam ("mode")), depth (findParam ("depth")), rate (findParam ("rate")),
      sync (findParam ("sync")), division (findParam ("division")), shape (findParam ("shape")),
      smooth (findParam ("smooth")), phase (findParam ("phase")), xover (findParam ("xover"))
{
}

TremoloEngine::Settings RKTremoloProcessor::currentSettings() const
{
    TremoloEngine::Settings s;
    s.mode        = valueOf (mode) >= 0.5f ? TremoloEngine::Mode::harmonic : TremoloEngine::Mode::classic;
    s.depth       = valueOf (depth) / 100.0f;
    s.rateHz      = valueOf (rate);
    s.sync        = valueOf (sync) >= 0.5f;
    s.division    = juce::roundToInt (valueOf (division));
    s.shape       = (rkfx::LFO::Shape) juce::roundToInt (valueOf (shape));
    s.smooth      = valueOf (smooth) / 100.0f;
    s.phaseDeg    = valueOf (phase);
    s.crossoverHz = valueOf (xover);
    return s;
}

void RKTremoloProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKTremoloProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKTremoloProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), getTransport());
}

juce::AudioProcessorEditor* RKTremoloProcessor::createEditor()
{
    return new RKTremoloEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKTremoloProcessor();
}
