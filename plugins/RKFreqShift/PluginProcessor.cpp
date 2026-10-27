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
        c.accent = juce::Colour (0xff2e9fd6); // Modulation family, sky blue
        return c;
    }

    rkfx::ParamBuilder makeParameters()
    {
        rkfx::ParamBuilder params;
        return params;
    }
}

RKFreqShiftProcessor::RKFreqShiftProcessor()
    : rkfx::ProcessorBase (makeConfig(), makeParameters())
{
}

void RKFreqShiftProcessor::prepareDsp (double, int, int) {}

void RKFreqShiftProcessor::resetDsp() {}

void RKFreqShiftProcessor::processDsp (juce::AudioBuffer<float>&) {}

juce::AudioProcessorEditor* RKFreqShiftProcessor::createEditor()
{
    return new RKFreqShiftEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKFreqShiftProcessor();
}
