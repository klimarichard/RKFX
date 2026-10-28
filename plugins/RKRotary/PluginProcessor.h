#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "RotaryEngine.h"

/** RKRotary: Leslie-style rotary speaker with horn and drum rotors. */
class RKRotaryProcessor : public rkfx::ProcessorBase
{
public:
    RKRotaryProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    RotaryEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *speed, *slowRate, *fastRate, *hornAccel, *drumAccel,
                               *balance, *distance, *width, *drive, *oversampling;
    RotaryEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKRotaryProcessor)
};
