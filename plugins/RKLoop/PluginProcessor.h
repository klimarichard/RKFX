#pragma once

#include "plugin/ProcessorBase.h"

#include "LoopEngine.h"
#include "LoopExport.h"
#include "Parameters.h"

#include <array>

/** RKLoop: a live looper with eight loops (parts and layers, synced). */
class RKLoopProcessor : public rkfx::ProcessorBase
{
public:
    RKLoopProcessor();
    ~RKLoopProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    const LoopEngine& getEngine() const noexcept { return engine; }
    LoopExporter& getExporter() noexcept { return exporter; }

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    static constexpr int kButtons = 6;
    std::array<juce::RangedAudioParameter*, kButtons> buttons {};
    std::array<juce::RangedAudioParameter*, LoopEngine::kLoops> roles {};
    juce::RangedAudioParameter *reverse, *halfSpeed, *level, *loop, *sync;
    LoopEngine engine;
    std::atomic<int> channels { 2 };
    std::atomic<double> hostBpm { 0.0 };   // 0: the host gives no tempo
    LoopExporter exporter;                 // after the engine: it reads it
    juce::RangedAudioParameter* saveLoops = nullptr;
    std::atomic<bool> saveDown { false };
    int shownLoop = -1;                   // the Loop parameter as last applied (1..8)

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
