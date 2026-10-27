#pragma once

#include "plugin/ProcessorBase.h"

#include "GraphEQEngine.h"
#include "Parameters.h"

/** RKGraphEQ: graphic EQ with 10, 15 or 31 bands and a response curve. */
class RKGraphEQProcessor : public rkfx::ProcessorBase
{
public:
    RKGraphEQProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    /** The EQ as currently set (for the response display). */
    GraphEQEngine::Settings currentSettings() const;

    /** Index of the active band set: 0 = 10, 1 = 15, 2 = 31 bands. */
    int activeMode() const;

    juce::RangedAudioParameter* bandParam (int modeIndex, int band) const
    {
        return gains[(size_t) modeIndex][(size_t) band];
    }

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter* bands = nullptr;
    std::array<std::array<juce::RangedAudioParameter*, GraphEQEngine::kMaxBands>, 3> gains {};
    GraphEQEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKGraphEQProcessor)
};
