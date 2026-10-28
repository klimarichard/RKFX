#include "ProcessorBase.h"

#include "safety/NanGuard.h"

namespace rkfx
{

juce::AudioProcessor::BusesProperties ProcessorBase::makeBuses (bool withSidechain)
{
    auto buses = BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true);

    if (withSidechain)
        buses = buses.withInput ("Sidechain", juce::AudioChannelSet::stereo(), true);

    return buses;
}

ProcessorBase::ProcessorBase (PluginConfig cfg, ParamBuilder builder)
    : juce::AudioProcessor (makeBuses (cfg.sidechain)),
      config (std::move (cfg))
{
    // Standard controls, after the plugin's own parameters.
    if (config.hasMix)
        builder.addPercent ("mix", "Dry/Wet", 0.0f, 100.0f, config.mixDefault);
    if (config.hasOutput)
        builder.addDecibels ("output", "Output", -24.0f, 12.0f, 0.0f);
    builder.addBool ("bypass", "Bypass", false);

    for (auto& owned : builder.params)
    {
        auto* p = owned.release();
        addParameter (p);
        allParams.push_back (p);
    }

    mixParam    = findParam ("mix");
    outputParam = findParam ("output");
    bypassParam = findParam ("bypass");

    for (auto* p : allParams)
    {
        stateInfos.push_back (describe (*p));

        if (p != bypassParam)
        {
            presetInfos.push_back (stateInfos.back());
            p->addListener (this);
        }
    }

    factoryPresets = rkfx::getFactoryPresets();

    // New instances start from the user's Default.json, if there is one.
    const auto defaultFile = library.getDefaultPresetFile (config.name);
    if (defaultFile.existsAsFile())
        loadPreset (defaultFile);
}

ProcessorBase::~ProcessorBase()
{
    for (auto* p : allParams)
        if (p != bypassParam)
            p->removeListener (this);
}

juce::RangedAudioParameter* ProcessorBase::findParam (const juce::String& id) const
{
    for (auto* p : allParams)
        if (p->getParameterID() == id)
            return p;
    return nullptr;
}

//==============================================================================
bool ProcessorBase::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (config.sidechain && layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    if (in == out)
        return true;

    return config.monoToStereo && in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorParameter* ProcessorBase::getBypassParameter() const
{
    return bypassParam;
}

void ProcessorBase::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (1, samplesPerBlock);
    const auto numChannels = juce::jmax (getMainBusNumInputChannels(), getMainBusNumOutputChannels());

    dryBuffer.setSize (numChannels, maxBlock, false, false, true);
    dryDelay.setSize (numChannels, kMaxLatency + 1, false, true, true);
    dryDelayWrite = 0;

    outputStage.prepare (sampleRate);
    outputStage.setMix (mixParam != nullptr ? valueOf (mixParam) / 100.0f : 1.0f);
    outputStage.setOutputDecibels (outputParam != nullptr ? valueOf (outputParam) : 0.0f);
    outputStage.setBypassed (valueOf (bypassParam) >= 0.5f);
    outputStage.reset();

    prepareDsp (sampleRate, maxBlock, numChannels);
    resetDsp();
    wetWasIdle = false;
}

void ProcessorBase::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiIn = &midi;
    juce::ScopedNoDenormals noDenormals;

    const auto numIn  = getMainBusNumInputChannels();
    const auto numOut = getMainBusNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    // External sidechain: its channels follow the main ones in the buffer.
    int sidechainChannels = 0, sidechainOffset = 0;
    if (config.sidechain)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            sidechainChannels = bus->getNumberOfChannels();
            sidechainOffset = getChannelIndexInProcessBlockBuffer (true, 1, 0);
        }

    // Mono in -> stereo out: every output starts as the mono input.
    for (int ch = numIn; ch < numOut; ++ch)
    {
        if (numIn == 1)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
        else
            buffer.clear (ch, 0, numSamples);
    }

    if (maxBlock == 0 || numOut == 0)
        return;

    outputStage.setMix (mixParam != nullptr ? valueOf (mixParam) / 100.0f : 1.0f);
    outputStage.setOutputDecibels (outputParam != nullptr ? valueOf (outputParam) : 0.0f);
    outputStage.setBypassed (valueOf (bypassParam) >= 0.5f);

    updateTransport();
    const auto blockPpq = transport.ppq;
    const auto quartersPerSample = transport.bpm / (60.0 * getSampleRate());

    // Hosts may send more samples than announced: work in prepared-size chunks.
    for (int start = 0; start < numSamples; start += maxBlock)
    {
        transport.ppq = blockPpq + start * quartersPerSample;
        chunkStart = start;

        const auto length = juce::jmin (maxBlock, numSamples - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), numOut, start, length);

        hasSidechain = sidechainChannels > 0 && sidechainOffset + sidechainChannels <= buffer.getNumChannels();
        if (hasSidechain)
            sidechain.setDataToReferTo (buffer.getArrayOfWritePointers() + sidechainOffset, sidechainChannels, start, length);

        processChunk (chunk);
    }

    // Effects never pass MIDI on.
    midi.clear();
}

void ProcessorBase::updateTransport()
{
    transport.hasPosition = false;
    transport.playing = false;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 0.0)
                transport.bpm = *bpm;

            if (const auto ppq = position->getPpqPosition(); ppq.hasValue())
            {
                transport.ppq = *ppq;
                transport.hasPosition = true;
            }

            transport.playing = position->getIsPlaying();
        }
    }
}

void ProcessorBase::reportLatency (int samples)
{
    samples = juce::jlimit (0, kMaxLatency, samples);
    if (latency.exchange (samples) == samples && getLatencySamples() == samples)
        return;

    if (juce::MessageManager::existsAndIsCurrentThread())
        setLatencySamples (samples);
    else
        latencyNotifier.triggerAsyncUpdate();
}

void ProcessorBase::delayDry (int numChannels, int numSamples) noexcept
{
    const auto delay = latency.load();
    if (delay == 0)
        return;

    // Ring of kMaxLatency + 1 samples: write each dry sample, read the one `delay` samples older.
    constexpr int size = kMaxLatency + 1;
    for (int i = 0; i < numSamples; ++i)
    {
        const auto readIndex = (dryDelayWrite - delay + size) % size;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* ring = dryDelay.getWritePointer (ch);
            auto* dry = dryBuffer.getWritePointer (ch);
            ring[dryDelayWrite] = dry[i];
            dry[i] = ring[readIndex];
        }
        dryDelayWrite = (dryDelayWrite + 1) % size;
    }
}

void ProcessorBase::processChunk (juce::AudioBuffer<float>& chunk)
{
    const auto numChannels = chunk.getNumChannels();
    const auto numSamples = chunk.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, chunk, ch, 0, numSamples);

    delayDry (numChannels, numSamples);

    if (outputStage.wetNeeded())
    {
        // Coming back from a full bypass: start from clean state, no stale tails.
        if (wetWasIdle)
        {
            resetDsp();
            wetWasIdle = false;
        }

        processDsp (chunk);

        if (! allFinite (chunk.getArrayOfReadPointers(), numChannels, numSamples))
        {
            resetDsp();
            chunk.clear();
            return;
        }
    }
    else
    {
        wetWasIdle = true;
    }

    outputStage.process (dryBuffer.getArrayOfReadPointers(), chunk.getArrayOfWritePointers(), numChannels, numSamples);

    // NaN or infinity arriving from upstream: never pass it on.
    if (! allFinite (chunk.getArrayOfReadPointers(), numChannels, numSamples))
        chunk.clear();
}

//==============================================================================
ParamValues ProcessorBase::currentValues() const
{
    ParamValues values;
    for (auto* p : allParams)
        values[p->getParameterID()] = valueOf (p);
    return values;
}

void ProcessorBase::applyValues (const ParamValues& values)
{
    applyingValues = true;

    for (auto* p : allParams)
    {
        const auto it = values.find (p->getParameterID());
        if (it != values.end())
            p->setValueNotifyingHost (p->convertTo0to1 ((float) it->second));
    }

    applyingValues = false;
}

void ProcessorBase::setPresetState (const juce::String& name, const juce::File& file, bool modified)
{
    {
        const juce::ScopedLock sl (presetLock);
        presetName = name;
        presetFile = file;
    }
    presetModified = modified;
    ++presetStateCounter;
}

juce::String ProcessorBase::getPresetName() const
{
    const juce::ScopedLock sl (presetLock);
    return presetName;
}

juce::File ProcessorBase::getPresetFile() const
{
    const juce::ScopedLock sl (presetLock);
    return presetFile;
}

void ProcessorBase::parameterValueChanged (int, float)
{
    if (! applyingValues && ! presetModified.exchange (true))
        ++presetStateCounter;
}

//==============================================================================
PresetReadResult ProcessorBase::readPreset (const juce::String& json, const std::vector<ParamInfo>& infos)
{
    return PresetFormat::read (json, config.name, config.stateVersion, infos,
                               [this] (juce::DynamicObject& params, int fromVersion) { migrateParams (params, fromVersion); });
}

ProcessorBase::PresetResult ProcessorBase::loadPreset (const juce::File& file)
{
    if (! file.existsAsFile())
        return { false, "File not found: " + file.getFileName() };

    const auto r = readPreset (file.loadFileAsString(), presetInfos);
    if (! r.ok)
        return { false, r.error };

    applyValues (r.values);
    setPresetState (r.name.isNotEmpty() ? r.name : file.getFileNameWithoutExtension(), file, false);

    return { true, r.notes.joinIntoString ("; ") };
}

ProcessorBase::PresetResult ProcessorBase::loadFactoryPreset (int index)
{
    if (index < 0 || index >= (int) factoryPresets.size())
        return { false, "No such factory preset" };

    const auto& preset = factoryPresets[(size_t) index];
    const auto r = readPreset (preset.json, presetInfos);
    if (! r.ok)
        return { false, r.error };

    applyValues (r.values);
    // No file: Save on a factory preset asks for a new name (factory presets stay read-only).
    setPresetState (r.name.isNotEmpty() ? r.name : preset.name, {}, false);
    return { true, r.notes.joinIntoString ("; ") };
}

ProcessorBase::PresetResult ProcessorBase::savePreset (const juce::File& file)
{
    const auto json = PresetFormat::toJson (PresetFormat::toVar (config.name, config.stateVersion, {},
                                                                 presetInfos, currentValues(), true));

    if (! file.getParentDirectory().createDirectory() || ! PresetLibrary::writeAtomically (file, json))
        return { false, "Could not save " + file.getFileName() };

    setPresetState (file.getFileNameWithoutExtension(), file, false);
    return { true, {} };
}

void ProcessorBase::resetToDefaults()
{
    applyValues (PresetFormat::defaults (presetInfos));
    setPresetState ("Default", {}, false);
}

//==============================================================================
void ProcessorBase::getStateInformation (juce::MemoryBlock& destData)
{
    // Self-contained: every value (bypass included) plus the preset name, so
    // a gig never depends on a preset file existing.
    auto state = PresetFormat::toVar (config.name, config.stateVersion, getPresetName(),
                                      stateInfos, currentValues(), false);

    auto* root = state.getDynamicObject();
    root->setProperty ("modified", isPresetModified());
    if (const auto file = getPresetFile(); file != juce::File())
        root->setProperty ("file", file.getFullPathName());

    const auto json = PresetFormat::toJson (state);
    destData.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void ProcessorBase::setStateInformation (const void* data, int sizeInBytes)
{
    const auto json = juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes);

    const auto r = readPreset (json, stateInfos);
    if (! r.ok)
        return;

    applyValues (r.values);

    juce::var root;
    juce::JSON::parse (json, root);
    const auto path = root["file"].toString();

    setPresetState (r.name.isNotEmpty() ? r.name : "Default",
                    juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File(),
                    r.modified);
}

} // namespace rkfx
