#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKDistort";
        c.stateVersion = 1;
        c.hasMix = true;                      // also for parallel distortion
        c.mixDefault = 100.0f;
        c.accent = juce::Colour (0xffc4262e); // Drive family, deep red
        return c;
    }
}

RKDistortProcessor::RKDistortProcessor()
    : rkfx::ProcessorBase (makeConfig(), DistortParams::create()),
      type (findParam ("type")), gain (findParam ("gain")), tight (findParam ("tight")), mid (findParam ("mid")),
      tone (findParam ("tone")), cab (findParam ("cab")), autoGain (findParam ("autogain")),
      oversampling (findParam ("oversampling"))
{
    reportLatency (rkfx::Oversampler::latencySamples (currentSettings().oversampling));
}

DistortEngine::Settings RKDistortProcessor::currentSettings() const
{
    DistortEngine::Settings s;
    s.type         = (DistortEngine::Type) juce::jlimit (0, 2, juce::roundToInt (valueOf (type)));
    s.gainDb       = valueOf (gain);
    s.tightHz      = valueOf (tight);
    s.midDb        = valueOf (mid);
    s.tone         = valueOf (tone) / 100.0f;
    s.cab          = (DistortEngine::Cab) juce::jlimit (0, 4, juce::roundToInt (valueOf (cab)));
    s.autoGain     = valueOf (autoGain) >= 0.5f;
    s.oversampling = DistortParams::oversamplingFactors[juce::jlimit (0, 2, juce::roundToInt (valueOf (oversampling)))];
    return s;
}

void RKDistortProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
    reportLatency (engine.latencySamples());
}

void RKDistortProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKDistortProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    reportLatency (engine.latencySamples());
}

juce::AudioProcessorEditor* RKDistortProcessor::createEditor()
{
    return new RKDistortEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKDistortProcessor();
}
