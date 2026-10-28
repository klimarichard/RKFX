#include "BoardProcessor.h"

#include "BoardEditor.h"
#include "plugin/PresetFormat.h"

namespace rkboard
{

namespace
{
    constexpr int kStateVersion = 1;

    juce::String bypassId (int id) { return "bypass" + juce::String (id); }

    /** Sets a flag for the duration of a scope (edits while loading don't mark the board modified). */
    struct LoadingScope
    {
        explicit LoadingScope (std::atomic<bool>& f) : flag (f) { flag = true; }
        ~LoadingScope() { flag = false; }
        std::atomic<bool>& flag;
    };
}

//==============================================================================
BoardProcessor::BoardProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Host parameters, append-only; the version hint is the position, so AU
    // hosts list them in this order.
    int hint = 1;
    for (int id = 1; id <= kMaxPedals; ++id)
    {
        auto* p = new juce::AudioParameterBool (juce::ParameterID { bypassId (id), hint++ },
                                                "Pedal " + juce::String (id) + " Bypass", false);
        bypassParams[(size_t) id - 1] = p;
        addParameter (p);
        p->addListener (this);
    }

    const auto levelRange = juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f);
    const auto levelAttributes = juce::AudioParameterFloatAttributes().withLabel ("dB");
    inputParam  = new juce::AudioParameterFloat (juce::ParameterID { "input", hint++ }, "Input", levelRange, 0.0f, levelAttributes);
    outputParam = new juce::AudioParameterFloat (juce::ParameterID { "output", hint++ }, "Output", levelRange, 0.0f, levelAttributes);
    addParameter (inputParam);
    addParameter (outputParam);
    inputParam->addListener (this);
    outputParam->addListener (this);

    pedalMidi.ensureSize (4096);

    for (auto* p : routing.getParameters())
        p->addListener (&routingWatcher);
    lastSplitMode = routing.getSplitMode();

    // New instances start from the user's board Default.json, if there is one.
    const auto defaultFile = library.getDefaultPresetFile ("RKBoard");
    if (defaultFile.existsAsFile())
        loadBoardPreset (defaultFile);
}

BoardProcessor::~BoardProcessor()
{
    for (auto* p : bypassParams)
        p->removeListener (this);
    inputParam->removeListener (this);
    outputParam->removeListener (this);
    for (auto* p : routing.getParameters())
        p->removeListener (&routingWatcher);

    for (int id = 1; id <= kMaxPedals; ++id)
        detachPedal (id);
}

//==============================================================================
std::vector<int> BoardProcessor::getChain() const
{
    std::vector<int> all;
    for (const auto& section : layout)
        all.insert (all.end(), section.begin(), section.end());
    return all;
}

Section BoardProcessor::sectionOf (int id) const
{
    for (int s = 0; s < kNumSections; ++s)
        if (std::find (layout[(size_t) s].begin(), layout[(size_t) s].end(), id) != layout[(size_t) s].end())
            return (Section) s;
    return Section::pre;
}

bool BoardProcessor::isSplitActive() const
{
    return ! getSection (Section::pathA).empty() || ! getSection (Section::pathB).empty();
}

rkfx::ProcessorBase* BoardProcessor::getPedal (int id) const
{
    return id >= 1 && id <= kMaxPedals ? pedals[(size_t) id - 1].get() : nullptr;
}

std::unique_ptr<rkfx::ProcessorBase> BoardProcessor::createPedal (const juce::String& typeName) const
{
    const auto* type = findPedalType (typeName);
    if (type == nullptr)
        return nullptr;

    auto pedal = type->create();
    preparePedal (*pedal);
    return pedal;
}

void BoardProcessor::preparePedal (rkfx::ProcessorBase& pedal) const
{
    // Stereo in and out; a sidechain bus (compressor, gate) stays off.
    auto buses = pedal.getBusesLayout();
    for (auto& bus : buses.inputBuses)
        bus = juce::AudioChannelSet::disabled();
    buses.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    buses.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    pedal.setBusesLayout (buses);

    pedal.setRateAndBufferSizeDetails (currentSampleRate, currentBlockSize);
    pedal.prepareToPlay (currentSampleRate, currentBlockSize);
}

int BoardProcessor::freeId() const
{
    for (int id = 1; id <= kMaxPedals; ++id)
        if (pedals[(size_t) id - 1] == nullptr)
            return id;
    return 0;
}

void BoardProcessor::attachPedal (int id, std::unique_ptr<rkfx::ProcessorBase> pedal)
{
    jassert (pedals[(size_t) id - 1] == nullptr);

    links[(size_t) id - 1] = std::make_unique<PedalLink> (*this, id);
    for (auto* p : pedal->getParameters())
        p->addListener (links[(size_t) id - 1].get());

    // The board's bypass parameter shows the pedal's state.
    syncingBypass = true;
    bypassParams[(size_t) id - 1]->setValueNotifyingHost (pedal->getBypassParameter()->getValue());
    syncingBypass = false;

    // Not in the chain yet, so the audio thread can't see it.
    const juce::SpinLock::ScopedLockType sl (chainLock);
    pedals[(size_t) id - 1] = std::move (pedal);
}

std::unique_ptr<rkfx::ProcessorBase> BoardProcessor::detachPedal (int id)
{
    if (id < 1 || id > kMaxPedals || pedals[(size_t) id - 1] == nullptr)
        return nullptr;

    if (pedalAboutToBeRemoved)
        pedalAboutToBeRemoved (id);

    for (auto& section : layout)
        section.erase (std::remove (section.begin(), section.end(), id), section.end());
    publishChain();

    std::unique_ptr<rkfx::ProcessorBase> pedal;
    {
        const juce::SpinLock::ScopedLockType sl (chainLock);
        pedal = std::move (pedals[(size_t) id - 1]);
    }

    for (auto* p : pedal->getParameters())
        p->removeListener (links[(size_t) id - 1].get());
    links[(size_t) id - 1].reset();

    return pedal;
}

void BoardProcessor::publishChain()
{
    Chain next;
    double tail = 0.0;

    for (int s = 0; s < kNumSections; ++s)
        for (const auto id : layout[(size_t) s])
            if (auto* pedal = getPedal (id))
            {
                auto& section = next[(size_t) s];
                section.pedals[(size_t) section.size++] = pedal;
                if (const auto* type = findPedalType (pedal->getConfig().name))
                    tail = juce::jmax (tail, type->tailSeconds);
            }

    {
        const juce::SpinLock::ScopedLockType sl (chainLock);
        chain = next;
    }

    tailSeconds = tail;
    updateLatency();
}

int BoardProcessor::addPedal (const juce::String& typeName, Section section, int position)
{
    const auto id = freeId();
    if (id == 0)
        return 0;

    auto pedal = createPedal (typeName);
    if (pedal == nullptr)
        return 0;

    attachPedal (id, std::move (pedal));

    auto& list = layout[(size_t) section];
    if (position < 0 || position > (int) list.size())
        position = (int) list.size();
    list.insert (list.begin() + position, id);
    publishChain();
    boardChanged();
    return id;
}

int BoardProcessor::duplicatePedal (int sourceId)
{
    auto* source = getPedal (sourceId);
    const auto id = freeId();
    if (source == nullptr || id == 0)
        return 0;

    auto pedal = createPedal (source->getConfig().name);
    pedal->loadPresetVar (source->getPresetVar (true), true);
    attachPedal (id, std::move (pedal));

    auto& list = layout[(size_t) sectionOf (sourceId)];
    const auto at = std::find (list.begin(), list.end(), sourceId);
    list.insert (at == list.end() ? list.end() : at + 1, id);
    publishChain();
    boardChanged();
    return id;
}

void BoardProcessor::removePedal (int id)
{
    if (detachPedal (id) != nullptr)
        boardChanged();
}

void BoardProcessor::movePedal (int id, Section section, int position)
{
    if (getPedal (id) == nullptr)
        return;

    const auto from = sectionOf (id);
    auto& source = layout[(size_t) from];
    const auto it = std::find (source.begin(), source.end(), id);
    const auto fromIndex = (int) std::distance (source.begin(), it);
    source.erase (it);

    // Within the same section, positions count the pedals before the move.
    if (from == section && position > fromIndex)
        --position;

    auto& target = layout[(size_t) section];
    position = juce::jlimit (0, (int) target.size(), position);
    target.insert (target.begin() + position, id);

    publishChain();
    boardChanged();
}

void BoardProcessor::clearBoard()
{
    for (int id = 1; id <= kMaxPedals; ++id)
        detachPedal (id);
    for (auto& section : layout)
        section.clear();
    publishChain();
}

bool BoardProcessor::isPedalBypassed (int id) const
{
    if (auto* pedal = getPedal (id))
        return pedal->getBypassParameter()->getValue() >= 0.5f;
    return false;
}

void BoardProcessor::setPedalBypassed (int id, bool bypassed)
{
    if (id >= 1 && id <= kMaxPedals && getPedal (id) != nullptr)
        bypassParams[(size_t) id - 1]->setValueNotifyingHost (bypassed ? 1.0f : 0.0f);
}

void BoardProcessor::boardChanged()
{
    ++boardCounter;
    markModified();
}

void BoardProcessor::markModified()
{
    if (! loadingBoard && ! presetModified.exchange (true))
        ++presetStateCounter;
}

//==============================================================================
void BoardProcessor::parameterValueChanged (int parameterIndex, float newValue)
{
    // Board bypass parameter -> its pedal (may run on the audio thread).
    if (parameterIndex >= 0 && parameterIndex < kMaxPedals)
    {
        if (syncingBypass.exchange (true))
            return;

        {
            const juce::SpinLock::ScopedLockType sl (chainLock);
            if (auto* pedal = pedals[(size_t) parameterIndex].get())
                pedal->getBypassParameter()->setValueNotifyingHost (newValue >= 0.5f ? 1.0f : 0.0f);
        }

        syncingBypass = false;
        ++boardCounter;
        return;
    }

    // Input / Output levels
    markModified();
}

void BoardProcessor::RoutingWatcher::parameterValueChanged (int, float)
{
    board.markModified();
    ++board.boardCounter;

    // Message thread only (the split mode is never automated).
    const auto mode = board.routing.getSplitMode();
    if (mode == board.lastSplitMode || board.loadingBoard)
    {
        board.lastSplitMode = mode;
        return;
    }

    auto& panA = board.routing.param ("pan_a");
    auto& panB = board.routing.param ("pan_b");
    const auto set = [] (juce::RangedAudioParameter& p, float pct) { p.setValueNotifyingHost (p.convertTo0to1 (pct)); };

    const auto centred = juce::approximatelyEqual (board.routing.value ("pan_a"), 0.0f)
                      && juce::approximatelyEqual (board.routing.value ("pan_b"), 0.0f);
    const auto hardLR = juce::approximatelyEqual (board.routing.value ("pan_a"), -100.0f)
                     && juce::approximatelyEqual (board.routing.value ("pan_b"), 100.0f);

    // Left (path A) back to the left, right (path B) back to the right; and
    // to the centre again when leaving the stereo split.
    board.lastSplitMode = mode;
    if (mode == SplitMode::stereo && centred)
    {
        set (panA, -100.0f);
        set (panB, 100.0f);
    }
    else if (mode != SplitMode::stereo && hardLR)
    {
        set (panA, 0.0f);
        set (panB, 0.0f);
    }
}

void BoardProcessor::PedalLink::parameterValueChanged (int parameterIndex, float newValue)
{
    auto* pedal = board.getPedal (id);
    if (pedal == nullptr)
        return;

    auto* param = pedal->getParameters()[parameterIndex];
    if (param == pedal->getBypassParameter())
    {
        // Pedal bypass (its editor's switch) -> the board's parameter.
        if (! board.syncingBypass.exchange (true))
        {
            board.bypassParams[(size_t) id - 1]->setValueNotifyingHost (newValue);
            board.syncingBypass = false;
        }
        ++board.boardCounter;
        return;
    }

    // Like the plugins, bypass doesn't make a preset "modified"; values do.
    board.markModified();
}

//==============================================================================
int BoardProcessor::sectionLatency (const SectionChain& section) const
{
    int latency = 0;
    for (int i = 0; i < section.size; ++i)
        latency += section.pedals[(size_t) i]->getLatencySamples();
    return latency;
}

void BoardProcessor::updateLatency()
{
    // Pre, the slower path, post.
    const auto split = chain[1].size > 0 || chain[2].size > 0;
    const auto latency = sectionLatency (chain[0])
                       + (split ? juce::jmax (sectionLatency (chain[1]), sectionLatency (chain[2])) : 0)
                       + sectionLatency (chain[3]);

    if (totalLatency.exchange (latency) != latency)
        triggerAsyncUpdate();
}

void BoardProcessor::handleAsyncUpdate()
{
    setLatencySamples (totalLatency.load());
}

double BoardProcessor::getTailLengthSeconds() const
{
    return tailSeconds.load();
}

bool BoardProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void BoardProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = juce::jmax (1, samplesPerBlock);

    inputGain.reset (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputParam->get()));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->get()));

    for (int path = 0; path < 2; ++path)
    {
        pathBuffers[path].setSize (2, currentBlockSize, false, true, true);
        pathDelays[path].prepare (2);
        for (auto& gain : mergeGains[path])
            gain.reset (sampleRate, 0.02);
    }
    for (auto& crossover : crossovers)
    {
        crossover.prepare (sampleRate);
        crossover.reset();
    }
    updateMergeTargets (true);
    splitWasActive = false;

    // Message thread here (hosts prepare outside processing), so the pedals
    // can be prepared in place.
    for (auto& pedal : pedals)
        if (pedal != nullptr)
            preparePedal (*pedal);
}

void BoardProcessor::PathDelay::process (juce::AudioBuffer<float>& buffer, int numSamples, int delay) noexcept
{
    delay = juce::jlimit (0, kSize - 1, delay);
    const auto channels = juce::jmin (buffer.getNumChannels(), ring.getNumChannels());

    for (int i = 0; i < numSamples; ++i)
    {
        const auto read = (write - delay + kSize) % kSize;
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* r = ring.getWritePointer (ch);
            auto* x = buffer.getWritePointer (ch);
            r[write] = x[i];
            x[i] = r[read];
        }
        write = (write + 1) % kSize;
    }
}

void BoardProcessor::updateMergeTargets (bool snap)
{
    // Per path: level, then a balance pan (centre = unity on both sides, so
    // a centred path is unchanged), and B's polarity.
    for (int path = 0; path < 2; ++path)
    {
        const auto suffix = path == 0 ? "_a" : "_b";
        const auto level = juce::Decibels::decibelsToGain (routing.value (juce::String ("level") + suffix));
        const auto pan = routing.value (juce::String ("pan") + suffix) / 100.0f;
        const auto sign = (path == 1 && routing.value ("invert_b") >= 0.5f) ? -1.0f : 1.0f;

        const float side[2] = { juce::jmin (1.0f, 1.0f - pan), juce::jmin (1.0f, 1.0f + pan) };
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto target = sign * level * side[ch];
            if (snap)
                mergeGains[path][ch].setCurrentAndTargetValue (target);
            else
                mergeGains[path][ch].setTargetValue (target);
        }
    }
}

void BoardProcessor::processSection (const SectionChain& section, juce::AudioBuffer<float>& buffer,
                                     const juce::MidiBuffer& midi, int midiOffset)
{
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < section.size; ++i)
    {
        auto* pedal = section.pedals[(size_t) i];
        pedal->setPlayHead (getPlayHead());

        // Each pedal gets the incoming MIDI (the vocoder plays its carrier from it).
        pedalMidi.clear();
        pedalMidi.addEvents (midi, midiOffset, numSamples, -midiOffset);
        pedal->processBlock (buffer, pedalMidi);
    }
}

void BoardProcessor::processChunk (juce::AudioBuffer<float>& main, const juce::MidiBuffer& midi, int midiOffset)
{
    const auto numSamples = main.getNumSamples();

    processSection (chain[0], main, midi, midiOffset);

    const auto split = chain[1].size > 0 || chain[2].size > 0;
    if (split)
    {
        if (! splitWasActive)
        {
            // Coming in: start clean, at the current merge settings.
            for (auto& crossover : crossovers)
                crossover.reset();
            for (auto& delay : pathDelays)
                delay.ring.clear();
            updateMergeTargets (true);
        }

        juce::AudioBuffer<float> paths[2] = {
            juce::AudioBuffer<float> (pathBuffers[0].getArrayOfWritePointers(), 2, numSamples),
            juce::AudioBuffer<float> (pathBuffers[1].getArrayOfWritePointers(), 2, numSamples)
        };

        // Split
        switch (routing.getSplitMode())
        {
            case SplitMode::full:
                for (auto& path : paths)
                    for (int ch = 0; ch < 2; ++ch)
                        path.copyFrom (ch, 0, main, ch, 0, numSamples);
                break;

            case SplitMode::frequency:
            {
                const auto hz = routing.value ("crossover");
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto& crossover = crossovers[(size_t) ch];
                    crossover.setFrequency (hz);
                    const auto* in = main.getReadPointer (ch);
                    auto* low = paths[0].getWritePointer (ch);
                    auto* high = paths[1].getWritePointer (ch);
                    for (int i = 0; i < numSamples; ++i)
                        crossover.process (in[i], low[i], high[i]);
                }
                break;
            }

            case SplitMode::stereo:
                // Left to A, right to B, each on both channels.
                for (int ch = 0; ch < 2; ++ch)
                {
                    paths[0].copyFrom (ch, 0, main, 0, 0, numSamples);
                    paths[1].copyFrom (ch, 0, main, 1, 0, numSamples);
                }
                break;
        }

        processSection (chain[1], paths[0], midi, midiOffset);
        processSection (chain[2], paths[1], midi, midiOffset);

        // Line the faster path up with the slower one.
        const auto latencyA = sectionLatency (chain[1]), latencyB = sectionLatency (chain[2]);
        const auto slowest = juce::jmax (latencyA, latencyB);
        pathDelays[0].process (paths[0], numSamples, slowest - latencyA);
        pathDelays[1].process (paths[1], numSamples, slowest - latencyB);

        // Merge
        updateMergeTargets (false);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* out = main.getWritePointer (ch);
            const auto* a = paths[0].getReadPointer (ch);
            const auto* b = paths[1].getReadPointer (ch);
            auto& gainA = mergeGains[0][ch];
            auto& gainB = mergeGains[1][ch];
            for (int i = 0; i < numSamples; ++i)
                out[i] = gainA.getNextValue() * a[i] + gainB.getNextValue() * b[i];
        }
    }
    splitWasActive = split;

    processSection (chain[3], main, midi, midiOffset);
}

void BoardProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn = getMainBusNumInputChannels();
    const auto numSamples = buffer.getNumSamples();

    // Mono in: both channels start as the input.
    if (numIn == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    if (buffer.getNumChannels() < 2)
        return;

    inputGain.setTargetValue (juce::Decibels::decibelsToGain (inputParam->get()));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputParam->get()));

    inputGain.applyGain (buffer, numSamples);

    {
        const juce::SpinLock::ScopedLockType sl (chainLock);

        // Hosts may send more samples than announced: the path buffers hold
        // one prepared block, so work in chunks of that size.
        for (int start = 0; start < numSamples; start += currentBlockSize)
        {
            const auto length = juce::jmin (currentBlockSize, numSamples - start);
            juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), 2, start, length);
            processChunk (chunk, midi, start);
        }

        // A drive pedal changing its oversampling changes the board's latency.
        updateLatency();
    }

    outputGain.applyGain (buffer, numSamples);
    midi.clear();
}

//==============================================================================
juce::var BoardProcessor::toVar (bool fullState) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("plugin", "RKBoard");
    root->setProperty ("version", kStateVersion);
    if (fullState)
    {
        root->setProperty ("name", getPresetName());
        root->setProperty ("modified", isPresetModified());
        if (const auto file = getPresetFile(); file != juce::File())
            root->setProperty ("file", file.getFullPathName());
    }

    auto* board = new juce::DynamicObject();
    // In the parameters' 0.1 dB steps, so a level reads back exactly.
    const auto level = [] (const juce::AudioParameterFloat& p) { return std::round ((double) p.get() * 10.0) / 10.0; };
    board->setProperty ("input", level (*inputParam));
    board->setProperty ("output", level (*outputParam));
    root->setProperty ("board", juce::var (board));
    root->setProperty ("routing", routing.toVar (! fullState));

    juce::Array<juce::var> list;
    for (int s = 0; s < kNumSections; ++s)
        for (const auto id : layout[(size_t) s])
            if (auto* pedal = getPedal (id))
            {
                auto* entry = new juce::DynamicObject();
                entry->setProperty ("id", id);
                entry->setProperty ("section", sectionKey ((Section) s));
                entry->setProperty ("bypass", isPedalBypassed (id));
                entry->setProperty ("preset", pedal->getPresetVar (fullState));
                list.add (juce::var (entry));
            }
    root->setProperty ("pedals", list);

    return juce::var (root);
}

BoardProcessor::PresetResult BoardProcessor::fromVar (const juce::var& root, bool fullState)
{
    if (! root.isObject() || root["plugin"].toString() != "RKBoard")
        return { false, "Not an RKBoard preset" };

    const LoadingScope loading (loadingBoard);
    juce::StringArray notes;

    clearBoard();

    if (const auto* list = root["pedals"].getArray())
    {
        for (const auto& entry : *list)
        {
            const auto preset = entry["preset"];
            const auto typeName = preset["plugin"].toString();

            if ((int) getChain().size() >= kMaxPedals)
            {
                notes.add ("More than " + juce::String (kMaxPedals) + " pedals: skipped " + typeName);
                continue;
            }

            auto pedal = createPedal (typeName);
            if (pedal == nullptr)
            {
                notes.add ("Unknown pedal skipped: " + (typeName.isNotEmpty() ? typeName : juce::String ("(none)")));
                continue;
            }

            const auto r = pedal->loadPresetVar (preset, fullState);
            if (r.message.isNotEmpty())
                notes.add (pedal->getConfig().name + ": " + r.message);

            // Keep the stored ID (host mappings follow it) when it is valid and free.
            auto id = (int) entry["id"];
            if (id < 1 || id > kMaxPedals || pedals[(size_t) id - 1] != nullptr)
                id = freeId();

            pedal->getBypassParameter()->setValueNotifyingHost ((bool) entry["bypass"] ? 1.0f : 0.0f);
            attachPedal (id, std::move (pedal));
            layout[(size_t) sectionFromKey (entry["section"].toString())].push_back (id);
        }
    }

    // Free IDs: their bypass parameters rest at "on" (not bypassed).
    syncingBypass = true;
    for (int id = 1; id <= kMaxPedals; ++id)
        if (pedals[(size_t) id - 1] == nullptr)
            bypassParams[(size_t) id - 1]->setValueNotifyingHost (0.0f);
    syncingBypass = false;

    const auto board = root["board"];
    const auto setLevel = [] (juce::RangedAudioParameter& p, const juce::var& v)
    {
        const auto dB = v.isVoid() ? 0.0f : juce::jlimit (-24.0f, 12.0f, (float) (double) v);
        p.setValueNotifyingHost (p.convertTo0to1 (dB));
    };
    setLevel (*inputParam, board["input"]);
    setLevel (*outputParam, board["output"]);

    // Missing routing (older presets) = the defaults.
    notes.addArray (routing.fromVar (root["routing"]));
    lastSplitMode = routing.getSplitMode();

    publishChain();
    ++boardCounter;

    return { true, notes.joinIntoString ("; ") };
}

//==============================================================================
void BoardProcessor::setPresetState (const juce::String& name, const juce::File& file, bool modified)
{
    {
        const juce::ScopedLock sl (presetLock);
        presetName = name;
        presetFile = file;
    }
    presetModified = modified;
    ++presetStateCounter;
}

juce::String BoardProcessor::getPresetName() const
{
    const juce::ScopedLock sl (presetLock);
    return presetName;
}

juce::File BoardProcessor::getPresetFile() const
{
    const juce::ScopedLock sl (presetLock);
    return presetFile;
}

BoardProcessor::PresetResult BoardProcessor::loadBoardPreset (const juce::File& file)
{
    if (! file.existsAsFile())
        return { false, "File not found: " + file.getFileName() };

    juce::var root;
    if (juce::JSON::parse (file.loadFileAsString(), root).failed())
        return { false, file.getFileName() + " is not valid JSON" };

    const auto r = fromVar (root, false);
    if (r.ok)
        setPresetState (file.getFileNameWithoutExtension(), file, false);
    return r;
}

BoardProcessor::PresetResult BoardProcessor::saveBoardPreset (const juce::File& file)
{
    const auto json = juce::JSON::toString (toVar (false), false);

    if (! file.getParentDirectory().createDirectory() || ! rkfx::PresetLibrary::writeAtomically (file, json))
        return { false, "Could not save " + file.getFileName() };

    setPresetState (file.getFileNameWithoutExtension(), file, false);
    return { true, {} };
}

void BoardProcessor::resetBoard()
{
    {
        const LoadingScope loading (loadingBoard);
        clearBoard();
        inputParam->setValueNotifyingHost (inputParam->convertTo0to1 (0.0f));
        outputParam->setValueNotifyingHost (outputParam->convertTo0to1 (0.0f));
        routing.fromVar ({});
        lastSplitMode = routing.getSplitMode();
        ++boardCounter;
    }
    setPresetState ("Default", {}, false);
}

void BoardProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Self-contained, like the plugins: every pedal with every value.
    const auto json = juce::JSON::toString (toVar (true), true);
    destData.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void BoardProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::var root;
    if (juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes), root).failed())
        return;

    if (! fromVar (root, true).ok)
        return;

    const auto path = root["file"].toString();
    setPresetState (root["name"].toString().isNotEmpty() ? root["name"].toString() : "Default",
                    juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File(),
                    (bool) root["modified"]);
}

juce::AudioProcessorEditor* BoardProcessor::createEditor()
{
    return new BoardEditor (*this);
}

} // namespace rkboard

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new rkboard::BoardProcessor();
}
