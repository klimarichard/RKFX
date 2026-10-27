#pragma once

#include "plugin/ProcessorBase.h"

#include "ParaEQEngine.h"
#include "Parameters.h"

/** RKParaEQ: seven-band parametric EQ with a response curve. */
class RKParaEQProcessor : public rkfx::ProcessorBase
{
public:
    RKParaEQProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    /** The EQ as currently set (for the response display). */
    ParaEQEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    struct BandParams
    {
        juce::RangedAudioParameter *on = nullptr, *type = nullptr, *freq = nullptr,
                                   *gain = nullptr, *q = nullptr, *slope = nullptr;
    };

    std::array<BandParams, ParaEQParams::kNumBands> params;
    ParaEQEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKParaEQProcessor)
};
