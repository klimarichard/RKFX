#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKRemap";
        c.stateVersion = 1;
        c.channels = RemapEngine::kChannels;  // one 64-channel bus each way
        c.hasMix = false;
        c.hasOutput = false;                  // trims are per output
        c.extraInPresets = true;              // names and views belong to the patch
        c.accent = juce::Colour (0xffa3adb8); // Utility family, slate
        c.family = rkfx::Palette::Family::utility;
        return c;
    }
}

RKRemapProcessor::RKRemapProcessor()
    : rkfx::ProcessorBase (makeConfig(), RemapParams::create())
{
    using namespace RemapParams;
    for (int k = 0; k < kChannels; ++k)
        outputParams[(size_t) k] = { findParam (outputId (k, "source")), findParam (outputId (k, "trim")), findParam (outputId (k, "mute")),
                                     findParam (outputId (k, "polarity")), findParam (outputId (k, "mono")) };
    for (int pair = 0; pair < RemapEngine::kPairs; ++pair)
    {
        inputLinks[(size_t) pair] = findParam (inputLinkId (pair));
        outputLinks[(size_t) pair] = findParam (outputLinkId (pair));
        inputMidSide[(size_t) pair] = findParam (inputMidSideId (pair));
    }
}

RemapEngine::Settings RKRemapProcessor::currentSettings() const
{
    RemapEngine::Settings s;
    for (int k = 0; k < kChannels; ++k)
    {
        const auto& p = outputParams[(size_t) k];
        auto& o = s.outputs[(size_t) k];
        o.source   = juce::roundToInt (valueOf (p.source));
        o.trimDb   = valueOf (p.trim);
        o.mute     = valueOf (p.mute) >= 0.5f;
        o.polarity = valueOf (p.polarity) >= 0.5f;
        o.mono     = valueOf (p.mono) >= 0.5f;
    }
    for (int pair = 0; pair < RemapEngine::kPairs; ++pair)
    {
        s.inputLinks[(size_t) pair] = valueOf (inputLinks[(size_t) pair]) >= 0.5f;
        s.outputLinks[(size_t) pair] = valueOf (outputLinks[(size_t) pair]) >= 0.5f;
        s.inputMidSide[(size_t) pair] = valueOf (inputMidSide[(size_t) pair]) >= 0.5f;
    }
    return s;
}

//==============================================================================
juce::String RKRemapProcessor::getInputName (int channel) const
{
    const juce::ScopedLock sl (namesLock);
    return inputNames[(size_t) channel];
}

juce::String RKRemapProcessor::getOutputName (int channel) const
{
    const juce::ScopedLock sl (namesLock);
    return outputNames[(size_t) channel];
}

void RKRemapProcessor::setInputName (int channel, const juce::String& name)
{
    {
        const juce::ScopedLock sl (namesLock);
        inputNames[(size_t) channel] = name.trim();
    }
    ++namesVersion;
    markPresetModified();
}

void RKRemapProcessor::setOutputName (int channel, const juce::String& name)
{
    {
        const juce::ScopedLock sl (namesLock);
        outputNames[(size_t) channel] = name.trim();
    }
    ++namesVersion;
    markPresetModified();
}

void RKRemapProcessor::setInputsShown (int n)
{
    inputsShown = juce::jlimit (1, kChannels, n);
    ++namesVersion;
    markPresetModified();
}

void RKRemapProcessor::setOutputsShown (int n)
{
    outputsShown = juce::jlimit (1, kChannels, n);
    ++namesVersion;
    markPresetModified();
}

juce::var RKRemapProcessor::getExtraState() const
{
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> ins, outs;
    {
        const juce::ScopedLock sl (namesLock);
        for (int ch = 0; ch < kChannels; ++ch)
        {
            ins.add (inputNames[(size_t) ch]);
            outs.add (outputNames[(size_t) ch]);
        }
    }
    root->setProperty ("inputNames", ins);
    root->setProperty ("outputNames", outs);
    root->setProperty ("inputsShown", inputsShown.load());
    root->setProperty ("outputsShown", outputsShown.load());
    return juce::var (root);
}

void RKRemapProcessor::setExtraState (const juce::var& extra)
{
    // A missing state (an older preset, Reset) means no names and the default views.
    {
        const juce::ScopedLock sl (namesLock);
        const auto* ins = extra["inputNames"].getArray();
        const auto* outs = extra["outputNames"].getArray();
        for (int ch = 0; ch < kChannels; ++ch)
        {
            inputNames[(size_t) ch] = ins != nullptr && ch < ins->size() ? ins->getReference (ch).toString() : juce::String();
            outputNames[(size_t) ch] = outs != nullptr && ch < outs->size() ? outs->getReference (ch).toString() : juce::String();
        }
    }
    inputsShown = juce::jlimit (1, kChannels, extra.hasProperty ("inputsShown") ? (int) extra["inputsShown"] : kDefaultShown);
    outputsShown = juce::jlimit (1, kChannels, extra.hasProperty ("outputsShown") ? (int) extra["outputsShown"] : kDefaultShown);
    ++namesVersion;
}

//==============================================================================
void RKRemapProcessor::prepareDsp (double sampleRate, int maxBlockSize, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate, maxBlockSize);
}

void RKRemapProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKRemapProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    auto raise = [] (std::atomic<float>& peak, float value)
    {
        auto current = peak.load();
        while (value > current && ! peak.compare_exchange_weak (current, value)) {}
    };
    for (int ch = 0; ch < kChannels; ++ch)
    {
        raise (inputPeaks[(size_t) ch], engine.takeInputPeak (ch));
        raise (outputPeaks[(size_t) ch], engine.takeOutputPeak (ch));
    }
}

juce::AudioProcessorEditor* RKRemapProcessor::createEditor()
{
    return new RKRemapEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKRemapProcessor();
}
#endif
