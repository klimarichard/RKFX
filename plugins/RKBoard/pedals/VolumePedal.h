#pragma once

#include "plugin/EditorBase.h"
#include "plugin/ProcessorBase.h"

#include "VolumeEngine.h"

/** Volume pedal (RKBoard only): for an expression pedal through an Assign
    parameter. Before a drive it cleans up the distortion like a guitar's
    volume knob; after delay or reverb it swells while the tails ring on.
    Parameter IDs are permanent: never rename, reuse or reorder; only append. */
class RKVolumeProcessor : public rkfx::ProcessorBase
{
public:
    RKVolumeProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    VolumeEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *position, *taper, *minLevel;
    VolumeEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKVolumeProcessor)
};

class RKVolumeEditor : public rkfx::EditorBase
{
public:
    explicit RKVolumeEditor (RKVolumeProcessor&);

private:
    std::unique_ptr<rkfx::Knob> position, minLevel;
    std::unique_ptr<rkfx::SegmentedSwitch> taper;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKVolumeEditor)
};
