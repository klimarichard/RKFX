#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/PresetFormat.h"

#include <cstdint>
#include <functional>
#include <unordered_map>

/** The mix settings: every value of the console that isn't a host parameter
    (trims, pans, links, outputs...). Held as parameters of a processor that
    never processes, as RKBoard's routing settings are, so the suite's knobs
    and switches work with them unchanged; the audio thread reads them like
    any parameter (atomic).

    Each setting belongs to a part of the mix (a channel, a bus, or the
    main), so a change only rebuilds that part: see ownerBits(). */
class MixSettings : public juce::AudioProcessor,
                    private juce::AudioProcessorParameter::Listener
{
public:
    MixSettings();
    ~MixSettings() override;

    juce::RangedAudioParameter& param (const juce::String& id) const;
    juce::RangedAudioParameter* findParam (const juce::String& id) const;
    float value (const juce::String& id) const;
    bool flag (const juce::String& id) const { return value (id) >= 0.5f; }
    int choice (const juce::String& id) const { return juce::roundToInt (value (id)); }

    /** The parts a setting belongs to: bit n = channel n, 32 + n = bus n, 63 = the main. */
    static std::uint64_t ownerBits (const juce::String& id);
    static constexpr std::uint64_t kMainBit = std::uint64_t (1) << 63;
    static constexpr std::uint64_t busBit (int bus) { return std::uint64_t (1) << (32 + bus); }

    /** Called (on the thread making the change) with the parts a change touched. */
    std::function<void (std::uint64_t ownerBits)> onChange;

    /** The "params" object of a preset: only non-default values. */
    juce::var toVar() const;
    /** Reads a "params" object leniently: values missing from it go back to
        their default. Returns notes about skipped values. */
    juce::StringArray fromVar (const juce::var& params);

    // juce::AudioProcessor (never used for audio)
    const juce::String getName() const override { return "RKMix settings"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    std::vector<rkfx::ParamInfo> infos;
    std::vector<juce::RangedAudioParameter*> params;
    std::vector<std::uint64_t> owners;                   // by parameter index
    std::unordered_map<std::string, juce::RangedAudioParameter*> byId;

    void parameterValueChanged (int parameterIndex, float) override;
    void parameterGestureChanged (int, bool) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixSettings)
};
