#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "FactoryPresets.h"
#include "OutputStage.h"
#include "dsp/TempoSync.h"
#include "ParamBuilder.h"
#include "PresetFormat.h"
#include "PresetLibrary.h"
#include "gui/LookAndFeel.h"

#include <atomic>

namespace rkfx
{

/** What every plugin tells the base processor about itself. */
struct PluginConfig
{
    juce::String name;               // "RKFreqShift": plugin name, preset folder, "plugin" key in JSON
    int stateVersion = 1;            // bump when a migration is needed; never decrease
    bool hasMix = false;             // dry/wet only where musically meaningful
    bool hasOutput = true;           // false when the plugin's own gain replaces Output (RKUtility)
    float mixDefault = 100.0f;       // percent
    bool monoToStereo = false;       // also accept mono in -> stereo out (chorus, reverb...)
    bool sidechain = false;          // adds a stereo "Sidechain" input bus (compressor, gate)
    bool midiInput = false;          // receives MIDI (vocoder carrier); also needs MIDI_INPUT in CMake
    juce::Colour accent;             // per-plugin family tint (active toggles only)
    Palette::Family family = Palette::Family::utility;   // tints the window background
};

/** The base every RKFX plugin derives from. It owns:

    - the standard controls (dry/wet where enabled, output, bypass), appended
      after the plugin's own parameters, and the output stage applying them;
    - the safety layer around the plugin's DSP: denormal flushing, mono/stereo
      handling, a NaN/infinity guard that resets the DSP and outputs silence;
    - state: the host state is the preset JSON with every value, plus the
      preset name and modified flag, with versioned migration;
    - presets: load/save/save-as, Default.json for new instances.

    A plugin implements prepareDsp(), resetDsp() and processDsp(), which
    receives the wet signal in place, never more than the prepared block
    size, and is skipped entirely while fully bypassed. */
class ProcessorBase : public juce::AudioProcessor,
                      private juce::AudioProcessorParameter::Listener
{
public:
    ProcessorBase (PluginConfig config, ParamBuilder pluginParams);
    ~ProcessorBase() override;

    const PluginConfig& getConfig() const noexcept { return config; }

    /** Lookup by ID (set up at construction; safe to call any time). */
    juce::RangedAudioParameter* findParam (const juce::String& id) const;

    /** Real-unit value of a parameter (atomic read, audio-thread safe). */
    static float valueOf (const juce::RangedAudioParameter* p) noexcept
    {
        return p->convertFrom0to1 (p->getValue());
    }

    //==============================================================================
    // Presets (message thread)
    struct PresetResult
    {
        bool ok = false;
        juce::String message;    // error, or a short note about skipped values
    };

    PresetResult loadPreset (const juce::File& file);

    /** The built-in presets (read-only; Save on one of them works like Save as). */
    const std::vector<FactoryPreset>& getFactoryPresets() const noexcept { return factoryPresets; }
    PresetResult loadFactoryPreset (int index);
    PresetResult savePreset (const juce::File& file);
    void resetToDefaults();

    /** The preset as a JSON object. With @p fullState, what the host state
        holds (every value, bypass, name, modified flag, file); otherwise what a
        preset file holds (only non-default values, no bypass). RKBoard stores
        its pedals this way. */
    juce::var getPresetVar (bool fullState) const;

    /** Loads an object made by getPresetVar() (the same @p fullState). */
    PresetResult loadPresetVar (const juce::var& preset, bool fullState);

    juce::String getPresetName() const;
    juce::File getPresetFile() const;
    bool isPresetModified() const noexcept { return presetModified.load(); }

    /** Bumped whenever the name, file or modified flag changes, so the editor
        can cheaply poll for header updates. */
    int getPresetStateCounter() const noexcept { return presetStateCounter.load(); }

    PresetLibrary& getLibrary() noexcept { return library; }

    //==============================================================================
    // juce::AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) final;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    const juce::String getName() const override { return config.name; }
    bool acceptsMidi() const override { return config.midiInput; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool hasEditor() const override { return true; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

protected:
    /** Allocate and compute coefficients. Never allocate in processDsp(). */
    virtual void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) = 0;

    /** Clear all internal state (delay lines, filters, envelopes). */
    virtual void resetDsp() = 0;

    /** Process the wet signal in place. @p buffer holds the input. */
    virtual void processDsp (juce::AudioBuffer<float>& buffer) = 0;

    /** Host tempo and beat position at the start of the buffer handed to
        processDsp() (valid inside processDsp only). */
    const Transport& getTransport() const noexcept { return transport; }

    /** The external sidechain input for the buffer handed to processDsp(),
        or nullptr when the plugin has none or the host didn't enable it.
        Unconnected inputs are silent. Valid inside processDsp only. */
    const juce::AudioBuffer<float>* getSidechain() const noexcept { return hasSidechain ? &sidechain : nullptr; }

    /** MIDI arriving with the buffer handed to processDsp(): events at
        positions getChunkStart() .. getChunkStart() + numSamples belong to it
        (subtract getChunkStart() for the position in the buffer). Valid
        inside processDsp only; nullptr for plugins without MIDI input. */
    const juce::MidiBuffer* getMidi() const noexcept { return config.midiInput ? midiIn : nullptr; }
    int getChunkStart() const noexcept { return chunkStart; }

    /** Declares the latency the plugin's DSP adds (oversampling only; every
        other plugin reports 0). Safe from any thread: the dry signal (used by
        dry/wet and bypass) is delayed to match from the next block on, and
        the host is told on the message thread. Up to kMaxLatency samples. */
    void reportLatency (int samples);
    static constexpr int kMaxLatency = 63;

    /** Bring an older state's params object up to the current version. */
    virtual void migrateParams (juce::DynamicObject& /*params*/, int /*fromVersion*/) {}

private:
    PluginConfig config;
    PresetLibrary library;
    std::vector<FactoryPreset> factoryPresets;

    std::vector<juce::RangedAudioParameter*> allParams;      // in host order
    std::vector<ParamInfo> stateInfos;                       // every parameter (host state)
    std::vector<ParamInfo> presetInfos;                      // everything but bypass (preset files)
    juce::RangedAudioParameter* mixParam = nullptr;
    juce::RangedAudioParameter* outputParam = nullptr;
    juce::RangedAudioParameter* bypassParam = nullptr;

    OutputStage outputStage;
    Transport transport;
    juce::AudioBuffer<float> sidechain;   // view into the host buffer, no allocation
    bool hasSidechain = false;
    const juce::MidiBuffer* midiIn = nullptr;
    int chunkStart = 0;

    // Dry-path delay matching the reported latency (ring buffer, 64 samples per channel).
    std::atomic<int> latency { 0 };
    juce::AudioBuffer<float> dryDelay;
    int dryDelayWrite = 0;

    struct LatencyNotifier : public juce::AsyncUpdater
    {
        explicit LatencyNotifier (ProcessorBase& o) : owner (o) {}
        void handleAsyncUpdate() override { owner.setLatencySamples (owner.latency.load()); }
        ProcessorBase& owner;
    };
    LatencyNotifier latencyNotifier { *this };
    juce::AudioBuffer<float> dryBuffer;
    int maxBlock = 0;
    bool wetWasIdle = true;

    // Preset state
    mutable juce::CriticalSection presetLock;
    juce::String presetName { "Default" };
    juce::File presetFile;
    std::atomic<bool> presetModified { false };
    std::atomic<int> presetStateCounter { 0 };
    std::atomic<bool> applyingValues { false };

    static BusesProperties makeBuses (bool withSidechain);
    void processChunk (juce::AudioBuffer<float>& chunk);
    void delayDry (int numChannels, int numSamples) noexcept;
    void updateTransport();

    ParamValues currentValues() const;
    void applyValues (const ParamValues& values);
    void setPresetState (const juce::String& name, const juce::File& file, bool modified);
    PresetReadResult readPreset (const juce::String& json, const std::vector<ParamInfo>& infos);

    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorBase)
};

} // namespace rkfx
