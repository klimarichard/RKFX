#pragma once

#include "plugin/ProcessorBase.h"

#include "ParaEQEngine.h"
#include "Parameters.h"

#include <atomic>

/** RKParaEQ: seven-band parametric EQ with a response curve. */
class RKParaEQProcessor : public rkfx::ProcessorBase
{
public:
    RKParaEQProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    /** The EQ as currently set (for the response display). */
    ParaEQEngine::Settings currentSettings() const;

    /** A gain band's detector level (loudest since the last call) and its
        dynamic gain right now (message thread, for the meter and the live
        curve). @p band is ParaEQEngine::BandIndex. */
    ParaEQEngine::Meter takeMeter (int band) noexcept;

    /** The gain band whose dynamics the editor shows (kept while the plugin is
        loaded; not a parameter, so it never marks a preset modified). */
    std::atomic<int> selectedBand { ParaEQEngine::mid };

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    struct BandParams
    {
        juce::RangedAudioParameter *on = nullptr, *type = nullptr, *freq = nullptr,
                                   *gain = nullptr, *q = nullptr, *slope = nullptr,
                                   *dyn = nullptr, *threshold = nullptr, *range = nullptr,
                                   *ratio = nullptr, *attack = nullptr, *release = nullptr;
    };

    std::array<BandParams, ParaEQParams::kNumBands> params;
    ParaEQEngine engine;

    struct AtomicMeter
    {
        std::atomic<float> levelDb { rkfx::Dynamics::kSilenceDb }, gainDb { 0.0f };
    };
    std::array<AtomicMeter, ParaEQEngine::kBands> meters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKParaEQProcessor)
};
