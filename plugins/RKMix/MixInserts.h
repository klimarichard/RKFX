#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MixEngine.h"
#include "host/PedalRegistry.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>

/** RKMix's inserts: RKFX plugins from the pedal host, as RKBoard's pedals
    are (the plugin's own processor, editor and presets), in slots: 2 per
    channel, 4 per bus, 4 on the main, 2 on the mono sum. A slot's plugin
    runs mono or stereo, as its strip. Switching an insert off is the
    plugin's own bypass (a crossfade; off costs nothing).

    Threads: plugins are created, prepared and removed on the message
    thread; the audio thread sees them through a table swapped under a short
    spin lock, which the processor holds while the engine runs a block. A
    removed plugin is deleted after the swap. */
class InsertRack : public MixEngine::Inserts,
                   private juce::AudioProcessorListener,
                   private juce::AudioProcessorParameter::Listener
{
public:
    using Strip = MixEngine::Inserts::Strip;

    static constexpr int kChannelSlots = 2, kBusSlots = 4, kMainSlots = 4, kMonoSlots = 2;
    static constexpr int kTotalSlots = mix::kChannels * kChannelSlots + mix::kBuses * kBusSlots + kMainSlots + kMonoSlots;

    struct Slot
    {
        Strip strip = Strip::channel;
        int index = 0, slot = 0;
    };

    static int numSlots (Strip) noexcept;

    InsertRack();
    ~InsertRack() override;

    //==============================================================================
    // Message thread

    /** Prepares every plugin (and the ones added later) for this rate and block size. */
    void prepare (double sampleRate, int maxBlockSize);

    rkfx::ProcessorBase* get (Slot) const;
    const rkfx::pedals::PedalType* typeOf (Slot) const;

    /** Puts a plugin of @p typeName in @p slot (replacing what is there), with
        @p numChannels in and out. Returns it, or nullptr for an unknown type. */
    rkfx::ProcessorBase* insert (Slot, const juce::String& typeName, int numChannels);
    void remove (Slot);
    void clear();

    bool isOn (Slot) const;
    void setOn (Slot, bool);

    /** A strip changed between mono and stereo: its plugins are prepared again. */
    void setChannels (Strip, int index, int numChannels);

    /** The plugins' latency per strip (a linked pair's right channel: 0). */
    MixEngine::Latencies latencies() const;

    /** The inserts as JSON: [ { "strip": "ch", "index": 3, "slot": 1, "on": true,
        "preset": { a plugin preset } }, ... ] (only filled slots). */
    juce::var toVar() const;
    /** Replaces every insert; @p channelsOf gives a strip's channel count. */
    void fromVar (const juce::var&, const std::function<int (Strip, int)>& channelsOf);

    /** One strip's inserts (channel presets), and back into a strip. */
    juce::var stripToVar (Strip, int index) const;
    void stripFromVar (Strip, int index, const juce::var&, int numChannels);

    /** Called before a plugin is deleted (the editor closes its editor first). */
    std::function<void (rkfx::ProcessorBase*)> aboutToRemove;
    /** Called when a plugin's settings, an insert or its latency change. */
    std::function<void()> onChange, onLatencyChange;

    /** Bumped whenever a slot changes (the editor polls it). */
    int getVersion() const noexcept { return version.load(); }

    static juce::String stripKey (Strip);

    //==============================================================================
    // Audio thread

    /** Held by the processor while the engine runs a block. */
    juce::SpinLock& getLock() noexcept { return lock; }

    /** This block's play head and MIDI (events from @p midiStart on belong to it). */
    void setBlockContext (juce::AudioPlayHead*, const juce::MidiBuffer* midi, int midiStart) noexcept;

    void process (Strip, int index, float* const* io, int numSides, int numSamples) noexcept override;
    bool isActive (Strip, int index) const noexcept override;

private:
    static int flatIndex (Slot) noexcept;
    static int firstSlot (Strip, int index) noexcept;

    std::array<std::unique_ptr<rkfx::ProcessorBase>, kTotalSlots> owned;     // message thread
    std::array<const rkfx::pedals::PedalType*, kTotalSlots> types {};
    std::array<rkfx::ProcessorBase*, kTotalSlots> live {};                    // audio thread view (under lock)
    std::array<juce::RangedAudioParameter*, kTotalSlots> liveBypass {};
    juce::SpinLock lock;

    double sampleRate = 48000.0;
    int blockSize = 512;

    juce::AudioPlayHead* playHead = nullptr;
    const juce::MidiBuffer* blockMidi = nullptr;
    int blockMidiStart = 0;
    juce::MidiBuffer pedalMidi;

    std::atomic<int> version { 0 };
    bool loading = false;

    void prepare (rkfx::ProcessorBase&, int numChannels) const;
    void publish (int flat, rkfx::ProcessorBase*);
    void attach (rkfx::ProcessorBase&);
    void detach (rkfx::ProcessorBase&);
    static juce::RangedAudioParameter* bypassOf (rkfx::ProcessorBase&);

    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override;
    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertRack)
};
