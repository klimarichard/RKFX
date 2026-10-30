#include "MixInserts.h"

namespace
{
    using Strip = MixEngine::Inserts::Strip;

    Strip stripFromKey (const juce::String& key)
    {
        return key == "bus" ? Strip::bus : key == "main" ? Strip::main : key == "mono" ? Strip::mono : Strip::channel;
    }
}

int InsertRack::numSlots (Strip strip) noexcept
{
    switch (strip)
    {
        case Strip::channel: return kChannelSlots;
        case Strip::bus:     return kBusSlots;
        case Strip::main:    return kMainSlots;
        case Strip::mono:    return kMonoSlots;
    }
    return 0;
}

int InsertRack::firstSlot (Strip strip, int index) noexcept
{
    constexpr auto buses = mix::kChannels * kChannelSlots;
    constexpr auto main = buses + mix::kBuses * kBusSlots;
    switch (strip)
    {
        case Strip::channel: return index * kChannelSlots;
        case Strip::bus:     return buses + index * kBusSlots;
        case Strip::main:    return main;
        case Strip::mono:    return main + kMainSlots;
    }
    return 0;
}

int InsertRack::flatIndex (Slot s) noexcept
{
    return firstSlot (s.strip, s.index) + juce::jlimit (0, numSlots (s.strip) - 1, s.slot);
}

juce::String InsertRack::stripKey (Strip strip)
{
    switch (strip)
    {
        case Strip::channel: return "ch";
        case Strip::bus:     return "bus";
        case Strip::main:    return "main";
        case Strip::mono:    return "mono";
    }
    return {};
}

InsertRack::InsertRack()
{
    rkfx::pedals::registerFactoryPresets();
    pedalMidi.ensureSize (4096);
}

InsertRack::~InsertRack()
{
    for (auto& p : owned)
        if (p != nullptr)
            detach (*p);
}

//==============================================================================
juce::RangedAudioParameter* InsertRack::bypassOf (rkfx::ProcessorBase& p)
{
    return p.findParam ("bypass");
}

void InsertRack::prepare (rkfx::ProcessorBase& pedal, int numChannels) const
{
    // Mono or stereo in and out, as the strip; a sidechain bus (compressor, gate) stays off.
    const auto set = numChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    auto buses = pedal.getBusesLayout();
    for (auto& bus : buses.inputBuses)
        bus = juce::AudioChannelSet::disabled();
    buses.inputBuses.getReference (0) = set;
    buses.outputBuses.getReference (0) = set;
    pedal.setBusesLayout (buses);

    pedal.setRateAndBufferSizeDetails (sampleRate, blockSize);
    pedal.prepareToPlay (sampleRate, blockSize);
}

void InsertRack::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    blockSize = juce::jmax (1, maxBlockSize);
    for (auto& p : owned)
        if (p != nullptr)
            prepare (*p, p->getTotalNumOutputChannels());
}

void InsertRack::attach (rkfx::ProcessorBase& p)
{
    p.addListener (this);
    for (auto* param : p.getParameters())
        param->addListener (this);
}

void InsertRack::detach (rkfx::ProcessorBase& p)
{
    p.removeListener (this);
    for (auto* param : p.getParameters())
        param->removeListener (this);
}

void InsertRack::publish (int flat, rkfx::ProcessorBase* p)
{
    auto* bypass = p != nullptr ? bypassOf (*p) : nullptr;
    const juce::SpinLock::ScopedLockType sl (lock);
    live[(size_t) flat] = p;
    liveBypass[(size_t) flat] = bypass;
}

rkfx::ProcessorBase* InsertRack::get (Slot s) const
{
    return owned[(size_t) flatIndex (s)].get();
}

const rkfx::pedals::PedalType* InsertRack::typeOf (Slot s) const
{
    return owned[(size_t) flatIndex (s)] != nullptr ? types[(size_t) flatIndex (s)] : nullptr;
}

rkfx::ProcessorBase* InsertRack::insert (Slot s, const juce::String& typeName, int numChannels)
{
    const auto* type = rkfx::pedals::findPedalType (typeName);
    if (type == nullptr)
        return nullptr;

    auto pedal = type->create();
    prepare (*pedal, numChannels);
    attach (*pedal);

    const auto flat = flatIndex (s);
    remove (s);
    types[(size_t) flat] = type;
    owned[(size_t) flat] = std::move (pedal);
    publish (flat, owned[(size_t) flat].get());

    ++version;
    if (! loading)
    {
        if (onChange != nullptr) onChange();
        if (onLatencyChange != nullptr) onLatencyChange();
    }
    return owned[(size_t) flat].get();
}

void InsertRack::remove (Slot s)
{
    const auto flat = flatIndex (s);
    if (owned[(size_t) flat] == nullptr)
        return;

    if (aboutToRemove != nullptr)
        aboutToRemove (owned[(size_t) flat].get());

    publish (flat, nullptr);   // the audio thread lets go first
    detach (*owned[(size_t) flat]);
    owned[(size_t) flat].reset();
    types[(size_t) flat] = nullptr;

    ++version;
    if (! loading)
    {
        if (onChange != nullptr) onChange();
        if (onLatencyChange != nullptr) onLatencyChange();
    }
}

void InsertRack::clear()
{
    for (int flat = 0; flat < kTotalSlots; ++flat)
        if (owned[(size_t) flat] != nullptr)
        {
            if (aboutToRemove != nullptr)
                aboutToRemove (owned[(size_t) flat].get());
            publish (flat, nullptr);
            detach (*owned[(size_t) flat]);
            owned[(size_t) flat].reset();
            types[(size_t) flat] = nullptr;
        }
    ++version;
    if (onLatencyChange != nullptr)
        onLatencyChange();
}

bool InsertRack::isOn (Slot s) const
{
    auto* p = get (s);
    return p != nullptr && rkfx::ProcessorBase::valueOf (bypassOf (*p)) < 0.5f;
}

void InsertRack::setOn (Slot s, bool on)
{
    if (auto* p = get (s))
    {
        auto* bypass = bypassOf (*p);
        bypass->setValueNotifyingHost (on ? 0.0f : 1.0f);
        ++version;
    }
}

void InsertRack::setChannels (Strip strip, int index, int numChannels)
{
    for (int slot = 0; slot < numSlots (strip); ++slot)
    {
        const auto flat = flatIndex ({ strip, index, slot });
        auto* p = owned[(size_t) flat].get();
        if (p == nullptr || p->getTotalNumOutputChannels() == numChannels)
            continue;

        // Out of the audio path while it is prepared again.
        publish (flat, nullptr);
        prepare (*p, numChannels);
        publish (flat, p);
    }
}

MixEngine::Latencies InsertRack::latencies() const
{
    MixEngine::Latencies l;
    const auto sum = [this] (Strip strip, int index)
    {
        int total = 0;
        for (int slot = 0; slot < numSlots (strip); ++slot)
            if (auto* p = owned[(size_t) flatIndex ({ strip, index, slot })].get())
                total += p->getLatencySamples();
        return total;
    };
    for (int ch = 0; ch < mix::kChannels; ++ch)
        l.channels[(size_t) ch] = sum (Strip::channel, ch);
    for (int bus = 0; bus < mix::kBuses; ++bus)
        l.buses[(size_t) bus] = sum (Strip::bus, bus);
    l.main = sum (Strip::main, 0);
    l.mono = sum (Strip::mono, 0);
    return l;
}

//==============================================================================
juce::var InsertRack::stripToVar (Strip strip, int index) const
{
    juce::Array<juce::var> list;
    for (int slot = 0; slot < numSlots (strip); ++slot)
        if (auto* p = get ({ strip, index, slot }))
        {
            auto* item = new juce::DynamicObject();
            item->setProperty ("slot", slot + 1);
            item->setProperty ("on", isOn ({ strip, index, slot }));
            item->setProperty ("preset", p->getPresetVar (false));
            list.add (juce::var (item));
        }
    return list;
}

void InsertRack::stripFromVar (Strip strip, int index, const juce::var& list, int numChannels)
{
    for (int slot = 0; slot < numSlots (strip); ++slot)
        remove ({ strip, index, slot });

    if (const auto* items = list.getArray())
        for (const auto& item : *items)
        {
            const Slot s { strip, index, (int) item["slot"] - 1 };
            if (s.slot < 0 || s.slot >= numSlots (strip))
                continue;
            const auto& preset = item["preset"];
            if (auto* p = insert (s, preset["plugin"].toString(), numChannels))
            {
                p->loadPresetVar (preset, false);
                setOn (s, item.hasProperty ("on") ? (bool) item["on"] : true);
            }
        }
}

juce::var InsertRack::toVar() const
{
    juce::Array<juce::var> list;
    const auto add = [&] (Strip strip, int index)
    {
        const auto items = stripToVar (strip, index);   // kept alive while its items are read
        for (const auto& item : *items.getArray())
        {
            item.getDynamicObject()->setProperty ("strip", stripKey (strip));
            item.getDynamicObject()->setProperty ("index", index + 1);
            list.add (item);
        }
    };
    for (int ch = 0; ch < mix::kChannels; ++ch)
        add (Strip::channel, ch);
    for (int bus = 0; bus < mix::kBuses; ++bus)
        add (Strip::bus, bus);
    add (Strip::main, 0);
    add (Strip::mono, 0);
    return list;
}

void InsertRack::fromVar (const juce::var& list, const std::function<int (Strip, int)>& channelsOf)
{
    loading = true;
    clear();

    if (const auto* items = list.getArray())
        for (const auto& item : *items)
        {
            const auto strip = stripFromKey (item["strip"].toString());
            const auto index = juce::jlimit (0, strip == Strip::bus ? mix::kBuses - 1 : strip == Strip::channel ? mix::kChannels - 1 : 0,
                                             (int) item["index"] - 1);
            const Slot s { strip, index, (int) item["slot"] - 1 };
            if (s.slot < 0 || s.slot >= numSlots (strip))
                continue;
            const auto& preset = item["preset"];
            if (auto* p = insert (s, preset["plugin"].toString(), channelsOf (strip, index)))
            {
                p->loadPresetVar (preset, false);
                setOn (s, item.hasProperty ("on") ? (bool) item["on"] : true);
            }
        }

    loading = false;
    ++version;
    if (onLatencyChange != nullptr)
        onLatencyChange();
}

//==============================================================================
void InsertRack::audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details)
{
    if (details.latencyChanged && onLatencyChange != nullptr)
        onLatencyChange();
}

void InsertRack::parameterValueChanged (int, float)
{
    ++version;
    if (! loading && onChange != nullptr)
        onChange();
}

//==============================================================================
void InsertRack::setBlockContext (juce::AudioPlayHead* head, const juce::MidiBuffer* midi, int midiStart) noexcept
{
    playHead = head;
    blockMidi = midi;
    blockMidiStart = midiStart;
}

void InsertRack::process (Strip strip, int index, float* const* io, int numSides, int numSamples) noexcept
{
    const auto first = firstSlot (strip, index);
    for (int slot = 0; slot < numSlots (strip); ++slot)
    {
        auto* p = live[(size_t) (first + slot)];
        if (p == nullptr || p->getTotalNumOutputChannels() != numSides)
            continue;

        // Each insert gets the incoming MIDI (the vocoder plays its carrier from it).
        pedalMidi.clear();
        if (blockMidi != nullptr)
            pedalMidi.addEvents (*blockMidi, blockMidiStart, numSamples, -blockMidiStart);

        p->setPlayHead (playHead);
        juce::AudioBuffer<float> view (io, numSides, numSamples);
        p->processBlock (view, pedalMidi);
    }
}

bool InsertRack::isActive (Strip strip, int index) const noexcept
{
    // An insert that is on keeps running (a reverb's or a delay's tail rings out).
    const auto first = firstSlot (strip, index);
    for (int slot = 0; slot < numSlots (strip); ++slot)
        if (auto* bypass = liveBypass[(size_t) (first + slot)])
            if (bypass->getValue() < 0.5f)
                return true;
    return false;
}
