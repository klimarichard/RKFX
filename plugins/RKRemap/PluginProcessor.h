#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "RemapEngine.h"

#include <array>
#include <atomic>

/** RKRemap: a 64 x 64 patch bay. Each output picks a source (an input, or a
    linked input pair), with trim, mute, polarity and a mono sum; inputs and
    outputs have names, and the window shows as many as set. */
class RKRemapProcessor : public rkfx::ProcessorBase
{
public:
    static constexpr int kChannels = RemapEngine::kChannels;

    RKRemapProcessor();
    ~RKRemapProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    RemapEngine::Settings currentSettings() const;

    // Names and views (message thread). Saved with the session and in presets.
    juce::String getInputName (int channel) const;
    juce::String getOutputName (int channel) const;
    void setInputName (int channel, const juce::String&);
    void setOutputName (int channel, const juce::String&);
    int getInputsShown() const noexcept { return inputsShown.load(); }
    int getOutputsShown() const noexcept { return outputsShown.load(); }
    void setInputsShown (int);
    void setOutputsShown (int);

    /** Bumped when names or views change (the editor polls it). */
    int getNamesVersion() const noexcept { return namesVersion.load(); }

    /** The loudest sample since the last call (message thread, for the lights). */
    float takeInputPeak (int channel) noexcept  { return inputPeaks[(size_t) channel].exchange (0.0f); }
    float takeOutputPeak (int channel) noexcept { return outputPeaks[(size_t) channel].exchange (0.0f); }

    static constexpr int kDefaultShown = 16;

    /** Set while a window shows the signal lights (no measuring otherwise). */
    std::atomic<int> windowsOpen { 0 };

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

    juce::var getExtraState() const override;
    void setExtraState (const juce::var&) override;

private:
    struct OutputParams { juce::RangedAudioParameter *source, *trim, *mute, *polarity, *mono; };
    std::array<OutputParams, kChannels> outputParams;
    std::array<juce::RangedAudioParameter*, RemapEngine::kPairs> inputLinks {}, outputLinks {}, inputMidSide {};

    RemapEngine engine;

    // The routing is rebuilt only when a parameter changes (416 of them: reading
    // them every block cost more than the routing itself).
    std::atomic<bool> settingsChanged { true };
    struct ChangeListener : public juce::AudioProcessorParameter::Listener
    {
        explicit ChangeListener (RKRemapProcessor& p) : owner (p) {}
        void parameterValueChanged (int, float) override { owner.settingsChanged = true; }
        void parameterGestureChanged (int, bool) override {}
        RKRemapProcessor& owner;
    };
    ChangeListener changeListener { *this };

    mutable juce::CriticalSection namesLock;
    std::array<juce::String, kChannels> inputNames, outputNames;
    std::atomic<int> inputsShown { kDefaultShown }, outputsShown { kDefaultShown }, namesVersion { 0 };

    std::array<std::atomic<float>, kChannels> inputPeaks {}, outputPeaks {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKRemapProcessor)
};
