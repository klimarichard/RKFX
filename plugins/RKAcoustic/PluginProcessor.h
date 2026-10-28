#pragma once

#include "plugin/ProcessorBase.h"

#include "AcousticEngine.h"
#include "Parameters.h"

/** RKAcoustic: preamp for piezo pickups (violin, viola, acoustic guitar, upright bass). */
class RKAcousticProcessor : public rkfx::ProcessorBase
{
public:
    RKAcousticProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    AcousticEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *voicing, *body, *quack, *highPass, *phase, *low, *mid, *midFreq,
                               *treble, *air, *notch, *compress;
    AcousticEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKAcousticProcessor)
};
