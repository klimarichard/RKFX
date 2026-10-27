#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKSaturate";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 100.0f;
        c.accent = juce::Colour (0xffe2583e); // Drive family, warm red
        return c;
    }
}

RKSaturateProcessor::RKSaturateProcessor()
    : rkfx::ProcessorBase (makeConfig(), SaturateParams::create()),
      drive (findParam ("drive")), character (findParam ("character")), tone (findParam ("tone")),
      autoGain (findParam ("autogain")), oversampling (findParam ("oversampling"))
{
    reportLatency (rkfx::Oversampler::latencySamples (currentSettings().oversampling));
}

SaturateEngine::Settings RKSaturateProcessor::currentSettings() const
{
    SaturateEngine::Settings s;
    s.driveDb      = valueOf (drive);
    s.character    = (SaturateEngine::Character) juce::jlimit (0, 2, juce::roundToInt (valueOf (character)));
    s.tone         = valueOf (tone) / 100.0f;
    s.autoGain     = valueOf (autoGain) >= 0.5f;
    s.oversampling = SaturateParams::oversamplingFactors[juce::jlimit (0, 2, juce::roundToInt (valueOf (oversampling)))];
    return s;
}

void RKSaturateProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
    reportLatency (engine.latencySamples());
}

void RKSaturateProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKSaturateProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    // The oversampling setting decides the latency (dry path and host follow).
    reportLatency (engine.latencySamples());
}

juce::AudioProcessorEditor* RKSaturateProcessor::createEditor()
{
    return new RKSaturateEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKSaturateProcessor();
}
