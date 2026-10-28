#pragma once

#include "plugin/ProcessorBase.h"

#include "LoopEngine.h"
#include "Parameters.h"

#include <array>

/** RKLoop: a live looper (record, overdub, undo, reverse, half speed). */
class RKLoopProcessor : public rkfx::ProcessorBase
{
public:
    RKLoopProcessor();
    ~RKLoopProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    const LoopEngine& getEngine() const noexcept { return engine; }

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    static constexpr int kButtons = 4;
    std::array<juce::RangedAudioParameter*, kButtons> buttons {};
    juce::RangedAudioParameter *reverse, *halfSpeed, *level;
    LoopEngine engine;

    // Footswitch presses, counted when the parameter changes (so a quick
    // click between two audio blocks is never missed).
    std::array<std::atomic<int>, kButtons> presses {};
    std::array<std::atomic<bool>, kButtons> down {};

    struct ButtonListener : public juce::AudioProcessorParameter::Listener
    {
        explicit ButtonListener (RKLoopProcessor& p) : owner (p) {}
        void parameterValueChanged (int parameterIndex, float newValue) override { owner.buttonChanged (parameterIndex, newValue); }
        void parameterGestureChanged (int, bool) override {}
        RKLoopProcessor& owner;
    };
    ButtonListener buttonListener { *this };

    void buttonChanged (int parameterIndex, float newValue);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKLoopProcessor)
};
