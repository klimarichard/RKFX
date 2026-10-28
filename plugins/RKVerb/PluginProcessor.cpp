#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKVerb";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 25.0f;
        c.monoToStereo = true;                // mono in, stereo reverb out
        c.accent = juce::Colour (0xfff08a24); // Reverb family, orange
        c.family = rkfx::Palette::Family::reverb;
        return c;
    }
}

RKVerbProcessor::RKVerbProcessor()
    : rkfx::ProcessorBase (makeConfig(), VerbParams::create()),
      type (findParam ("type")), decay (findParam ("decay")), size (findParam ("size")),
      predelay (findParam ("predelay")), predelaySync (findParam ("predelay_sync")),
      predelayDivision (findParam ("predelay_division")), damping (findParam ("damping")),
      lowCut (findParam ("lowcut")), highCut (findParam ("highcut")), mod (findParam ("mod")),
      width (findParam ("width")), freeze (findParam ("freeze"))
{
}

ReverbEngine::Settings RKVerbProcessor::currentSettings (double bpm) const
{
    ReverbEngine::Settings s;
    s.type         = (ReverbEngine::Type) juce::jlimit (0, 2, juce::roundToInt (valueOf (type)));
    s.decaySeconds = valueOf (decay) / 1000.0f;
    s.size         = valueOf (size) / 100.0f;
    s.damping      = valueOf (damping) / 100.0f;
    s.lowCutHz     = valueOf (lowCut);
    s.highCutHz    = valueOf (highCut);
    s.modulation   = valueOf (mod) / 100.0f;
    s.width        = valueOf (width) / 100.0f;
    s.freeze       = valueOf (freeze) >= 0.5f;

    if (valueOf (predelaySync) >= 0.5f)
    {
        const auto beats = VerbParams::divisionBeats (juce::roundToInt (valueOf (predelayDivision)));
        s.predelayMs = (float) juce::jmin (ReverbEngine::kMaxPredelaySeconds * 1000.0, beats * 60000.0 / juce::jmax (1.0, bpm));
    }
    else
    {
        s.predelayMs = valueOf (predelay);
    }

    return s;
}

void RKVerbProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings (120.0));
    engine.prepare (sampleRate);
}

void RKVerbProcessor::resetDsp()
{
    engine.setSettings (currentSettings (120.0));
    engine.reset();
}

void RKVerbProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings (getTransport().bpm));
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKVerbProcessor::createEditor()
{
    return new RKVerbEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKVerbProcessor();
}
