#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKUtility";
        c.stateVersion = 1;
        c.hasMix = false;
        c.hasOutput = false;                  // Gain replaces Output
        c.accent = juce::Colour (0xffb4b4b4); // Utility family, light gray
        return c;
    }
}

RKUtilityProcessor::RKUtilityProcessor()
    : rkfx::ProcessorBase (makeConfig(), UtilityParams::create()),
      gain (findParam ("gain")), pan (findParam ("pan")), width (findParam ("width")), bassMono (findParam ("bass_mono")),
      polLeft (findParam ("pol_left")), polRight (findParam ("pol_right")), swap (findParam ("swap")),
      channelMode (findParam ("channel_mode")), delayLeft (findParam ("delay_left")), delayRight (findParam ("delay_right")),
      mute (findParam ("mute"))
{
}

UtilityEngine::Settings RKUtilityProcessor::currentSettings() const
{
    UtilityEngine::Settings s;
    s.gainDb       = valueOf (gain);
    s.pan          = valueOf (pan) / 100.0f;
    s.width        = valueOf (width) / 100.0f;
    s.bassMonoHz   = valueOf (bassMono);
    s.invertLeft   = valueOf (polLeft) >= 0.5f;
    s.invertRight  = valueOf (polRight) >= 0.5f;
    s.swap         = valueOf (swap) >= 0.5f;
    s.channelMode  = (UtilityEngine::ChannelMode) juce::jlimit (0, 4, juce::roundToInt (valueOf (channelMode)));
    s.delayLeftMs  = valueOf (delayLeft);
    s.delayRightMs = valueOf (delayRight);
    s.mute         = valueOf (mute) >= 0.5f;
    return s;
}

void RKUtilityProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKUtilityProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKUtilityProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKUtilityProcessor::createEditor()
{
    return new RKUtilityEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKUtilityProcessor();
}
