#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKDelay";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 30.0f;
        c.monoToStereo = true;                // ping-pong on a mono source
        c.accent = juce::Colour (0xff3dae5b); // Delay family, green
        c.family = rkfx::Palette::Family::delay;
        return c;
    }
}

RKDelayProcessor::RKDelayProcessor()
    : rkfx::ProcessorBase (makeConfig(), DelayParams::create()),
      mode (findParam ("mode")), time (findParam ("time")), sync (findParam ("sync")),
      division (findParam ("division")), feedback (findParam ("feedback")), stereo (findParam ("stereo")),
      offset (findParam ("offset")), lowCut (findParam ("lowcut")), highCut (findParam ("highcut")),
      modDepth (findParam ("mod_depth")), modRate (findParam ("mod_rate")), duck (findParam ("duck")),
      shimmer (findParam ("shimmer")), shimmerInterval (findParam ("shimmer_interval"))
{
}

DelayEngine::Settings RKDelayProcessor::currentSettings() const
{
    DelayEngine::Settings s;
    s.mode      = (DelayEngine::Mode) juce::jlimit (0, 2, juce::roundToInt (valueOf (mode)));
    s.timeMs    = valueOf (time);
    s.sync      = valueOf (sync) >= 0.5f;
    s.division  = juce::roundToInt (valueOf (division));
    s.feedback  = valueOf (feedback) / 100.0f;
    s.stereo    = (DelayEngine::Stereo) juce::jlimit (0, 2, juce::roundToInt (valueOf (stereo)));
    s.offset    = valueOf (offset) / 100.0f;
    s.lowCutHz  = valueOf (lowCut);
    s.highCutHz = valueOf (highCut);
    s.modDepth  = valueOf (modDepth) / 100.0f;
    s.modRateHz = valueOf (modRate);
    s.duck      = valueOf (duck) / 100.0f;
    s.shimmer   = valueOf (shimmer) / 100.0f;
    s.shimmerRatio = rkfx::ShimmerInterval::ratio (juce::roundToInt (valueOf (shimmerInterval)));
    return s;
}

void RKDelayProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKDelayProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKDelayProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), getTransport());
}

juce::AudioProcessorEditor* RKDelayProcessor::createEditor()
{
    return new RKDelayEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKDelayProcessor();
}
#endif
