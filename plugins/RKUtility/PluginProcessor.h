#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "UtilityEngine.h"

/** RKUtility: gain, balance, width, bass mono, polarity, channel routing,
    mid/side, mute and per-channel delay for mic alignment. */
class RKUtilityProcessor : public rkfx::ProcessorBase
{
public:
    RKUtilityProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    UtilityEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *gain, *pan, *width, *bassMono, *polLeft, *polRight, *swap, *channelMode,
                               *delayLeft, *delayRight, *mute;
    UtilityEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKUtilityProcessor)
};
