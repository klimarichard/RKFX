#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKVibrato";
        c.stateVersion = 1;
        c.hasMix = false;                     // mixing dry would turn it into a chorus
        c.monoToStereo = true;                // stereo phase on a mono source
        c.accent = juce::Colour (0xff79b8e8); // Modulation family, powder blue
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }
}

RKVibratoProcessor::RKVibratoProcessor()
    : rkfx::ProcessorBase (makeConfig(), VibratoParams::create()),
      rate (findParam ("rate")), depth (findParam ("depth")), shape (findParam ("shape")), sync (findParam ("sync")),
      division (findParam ("division")), phase (findParam ("phase")), rise (findParam ("rise"))
{
}

VibratoEngine::Settings RKVibratoProcessor::currentSettings() const
{
    VibratoEngine::Settings s;
    s.rateHz      = valueOf (rate);
    s.depth       = valueOf (depth) / 100.0f;
    s.shape       = valueOf (shape) >= 0.5f ? VibratoEngine::Shape::triangle : VibratoEngine::Shape::sine;
    s.sync        = valueOf (sync) >= 0.5f;
    s.division    = juce::roundToInt (valueOf (division));
    s.phaseDeg    = valueOf (phase);
    s.riseSeconds = valueOf (rise) / 1000.0f;
    return s;
}

void RKVibratoProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKVibratoProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKVibratoProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), getTransport());
}

juce::AudioProcessorEditor* RKVibratoProcessor::createEditor()
{
    return new RKVibratoEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKVibratoProcessor();
}
#endif
