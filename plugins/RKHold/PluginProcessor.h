#pragma once

#include "plugin/ProcessorBase.h"

#include "HoldEngine.h"
#include "Parameters.h"

#include <array>
#include <atomic>

/** RKHold: a note freeze / sustainer. Hold captures what's sounding and keeps
    it as a drone (Note or Texture layers, up to four); the dry signal is
    never delayed. */
class RKHoldProcessor : public rkfx::ProcessorBase
{
public:
    RKHoldProcessor();
    ~RKHoldProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    HoldEngine::Settings currentSettings() const;

    /** The sounding layers for the display (message thread), oldest first. */
    HoldEngine::LayerInfo getLayer (int index) const noexcept;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *hold, *holdMode, *mode, *range, *layers, *clear, *octave, *attack, *release,
                               *movement, *tone, *width, *holdLevel, *dryLevel;

    HoldEngine engine;

    // Footswitch presses and lifts, counted when the parameters change (so a
    // quick tap between two audio blocks is never missed).
    std::atomic<int> holdPresses { 0 }, holdLifts { 0 }, clearPresses { 0 };
    std::atomic<bool> holdDown { false }, clearDown { false };

    struct Listener : public juce::AudioProcessorParameter::Listener
    {
        explicit Listener (RKHoldProcessor& p) : owner (p) {}
        void parameterValueChanged (int index, float value) override { owner.switchChanged (index, value); }
        void parameterGestureChanged (int, bool) override {}
        RKHoldProcessor& owner;
    };
    Listener listener { *this };
    void switchChanged (int parameterIndex, float value);

    struct AtomicLayer
    {
        std::atomic<bool> active { false };
        std::atomic<int> note { -1 };
        std::atomic<float> level { 0.0f };
    };
    std::array<AtomicLayer, HoldEngine::kLayers> shownLayers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKHoldProcessor)
};
