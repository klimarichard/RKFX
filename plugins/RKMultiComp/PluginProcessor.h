#pragma once

#include "plugin/ProcessorBase.h"

#include "MultiCompEngine.h"
#include "Parameters.h"

#include <array>
#include <atomic>

/** RKMultiComp: a 3- or 4-band compressor with zero latency, phase-matched
    Dry/Wet and a band display. */
class RKMultiCompProcessor : public rkfx::ProcessorBase
{
public:
    RKMultiCompProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    MultiCompEngine::Settings currentSettings() const;

    /** A band's loudest level and most gain reduction since the last call
        (message thread, for the display and the band meters). */
    MultiCompEngine::Meter takeMeter (MultiCompEngine::Band) noexcept;

    /** The band the editor shows the knobs of (kept while the plugin is
        loaded; not a parameter, so it never marks a preset modified). */
    std::atomic<int> selectedBand { MultiCompEngine::low };

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    struct BandParams
    {
        juce::RangedAudioParameter *threshold, *ratio, *attack, *release, *gain, *solo, *on;
    };
    std::array<BandParams, MultiCompEngine::kNumBands> bandParams;

    juce::RangedAudioParameter *xoverLow, *xoverMid, *xoverHigh, *bands, *offset, *knee, *detect,
                               *releaseAuto, *link, *gainAuto, *mix;

    MultiCompEngine engine;

    struct AtomicMeter
    {
        std::atomic<float> levelDb { rkfx::Dynamics::kSilenceDb }, reductionDb { 0.0f };
    };
    std::array<AtomicMeter, MultiCompEngine::kNumBands> meters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKMultiCompProcessor)
};
