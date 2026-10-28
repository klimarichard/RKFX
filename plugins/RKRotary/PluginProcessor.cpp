#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKRotary";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 100.0f;
        c.monoToStereo = true;                // a rotary cabinet is mono in, stereo out
        c.accent = juce::Colour (0xff1f8fc0); // Modulation family, deep sky
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }
}

RKRotaryProcessor::RKRotaryProcessor()
    : rkfx::ProcessorBase (makeConfig(), RotaryParams::create()),
      speed (findParam ("speed")), slowRate (findParam ("slow_rate")), fastRate (findParam ("fast_rate")),
      hornAccel (findParam ("horn_accel")), drumAccel (findParam ("drum_accel")), balance (findParam ("balance")),
      distance (findParam ("distance")), width (findParam ("width")), drive (findParam ("drive")),
      oversampling (findParam ("oversampling"))
{
    reportLatency (rkfx::Oversampler::latencySamples (currentSettings().oversampling));
}

RotaryEngine::Settings RKRotaryProcessor::currentSettings() const
{
    RotaryEngine::Settings s;
    s.speed            = (RotaryEngine::Speed) juce::jlimit (0, 2, juce::roundToInt (valueOf (speed)));
    s.slowHz           = valueOf (slowRate);
    s.fastHz           = valueOf (fastRate);
    s.hornAccelSeconds = valueOf (hornAccel) / 1000.0f;
    s.drumAccelSeconds = valueOf (drumAccel) / 1000.0f;
    s.balance          = valueOf (balance) / 100.0f;
    s.distance         = valueOf (distance) / 100.0f;
    s.width            = valueOf (width) / 100.0f;
    s.drive            = valueOf (drive) / 100.0f;
    s.oversampling     = RotaryParams::oversamplingFactors[juce::jlimit (0, 2, juce::roundToInt (valueOf (oversampling)))];
    return s;
}

void RKRotaryProcessor::prepareDsp (double sampleRate, int maxBlockSize, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate, maxBlockSize);
    reportLatency (engine.latencySamples());
}

void RKRotaryProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKRotaryProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    reportLatency (engine.latencySamples());
}

juce::AudioProcessorEditor* RKRotaryProcessor::createEditor()
{
    return new RKRotaryEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKRotaryProcessor();
}
