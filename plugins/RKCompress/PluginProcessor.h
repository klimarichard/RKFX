#pragma once

#include "plugin/ProcessorBase.h"

#include "CompressorEngine.h"
#include "Parameters.h"

#include <atomic>

/** RKCompress: feed-forward compressor with external sidechain, parallel
    compression, transfer curve and gain reduction meter. */
class RKCompressProcessor : public rkfx::ProcessorBase
{
public:
    RKCompressProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    CompressorEngine::Settings currentSettings() const;

    /** Most gain reduction / loudest detector level since the last call
        (message thread, for the meter and the curve's dot). */
    float takeGainReductionDb() noexcept { return meterReduction.exchange (0.0f); }
    float takeInputLevelDb() noexcept    { return meterInput.exchange (rkfx::Dynamics::kSilenceDb); }

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *threshold, *ratio, *attack, *release, *releaseAuto, *knee, *detect,
                               *scSource, *scHpf, *scListen, *link, *makeup, *makeupAuto;

    CompressorEngine engine;
    std::atomic<float> meterReduction { 0.0f }, meterInput { rkfx::Dynamics::kSilenceDb };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKCompressProcessor)
};
