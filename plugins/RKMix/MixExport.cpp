#include "MixExport.h"

#include <algorithm>

namespace
{
    /** The inserts' play head: the export's position at the mix's tempo, playing. */
    struct ExportPlayHead : public juce::AudioPlayHead
    {
        double bpm = 120.0, sampleRate = 48000.0;
        juce::int64 samples = 0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setTimeInSamples (samples);
            info.setTimeInSeconds ((double) samples / sampleRate);
            info.setPpqPosition ((double) samples / sampleRate * bpm / 60.0);
            info.setIsPlaying (true);
            return info;
        }
    };

    /** The main as it leaves for its output (after its limiter, lined up with every output). */
    struct MainTap : public MixEngine::OutputTaps
    {
        juce::AudioBuffer<float> block { 2, MixExport::kBlock };
        void main (const float* left, const float* right, int numSamples) noexcept override
        {
            block.copyFrom (0, 0, left, numSamples);
            block.copyFrom (1, 0, right != nullptr ? right : left, numSamples);
        }
        void bus (int, const float*, const float*, int) noexcept override {}
    };
}

//==============================================================================
MixExport::MixExport() : juce::Thread ("RKMix export") {}

MixExport::~MixExport()
{
    stopThread (10000);
}

juce::File MixExport::fileFor (const juce::File& folder, const juce::String& name)
{
    auto base = juce::File::createLegalFileName (name.trim());
    if (base.isEmpty())
        base = "Export";
    auto file = folder.getChildFile (base + ".wav");
    for (int n = 2; file.exists(); ++n)
        file = folder.getChildFile (base + " (" + juce::String (n) + ").wav");   // "Take 03 (2).wav"
    return file;
}

bool MixExport::start (Mix m, const MixPlayback::Recording& r, std::vector<Region> list, const juce::File& into, bool asFloatFormat)
{
    if (running.load() || list.empty())
        return false;
    stopThread (1000);   // the last one has finished; its inserts go here, on the message thread
    mix = std::move (m);
    recording = r;
    regions = std::move (list);
    folder = into;
    asFloat = asFloatFormat;
    progress = 0.0f;
    running = true;
    setStatus ("Exporting...");
    startThread (juce::Thread::Priority::low);   // the live mix comes first
    return true;
}

void MixExport::cancel()
{
    signalThreadShouldExit();
}

juce::String MixExport::getStatus() const
{
    const juce::ScopedLock sl (statusLock);
    return status;
}

bool MixExport::lastFailed() const
{
    const juce::ScopedLock sl (statusLock);
    return failed;
}

void MixExport::setStatus (const juce::String& s, bool failure)
{
    const juce::ScopedLock sl (statusLock);
    status = s;
    failed = failure;
}

void MixExport::run()
{
    juce::int64 total = 0, done = 0;
    for (const auto& r : regions)
        total += juce::jmax<juce::int64> (1, r.end - r.start);

    int written = 0;
    juce::String problem;
    if (! folder.createDirectory())
        problem = "can't create " + folder.getFullPathName();
    for (size_t i = 0; i < regions.size() && problem.isEmpty() && ! threadShouldExit(); ++i)
    {
        const auto& region = regions[i];
        setStatus (region.name + (regions.size() > 1 ? " (" + juce::String ((int) i + 1) + " of " + juce::String ((int) regions.size()) + ")"
                                                      : juce::String()));
        const auto length = juce::jmax<juce::int64> (1, region.end - region.start);
        const auto result = render (mix, recording, region, fileFor (folder, region.name), asFloat, [this, done, length, total] (float p)
        {
            progress = (float) ((double) done + p * (double) length) / (float) juce::jmax<juce::int64> (1, total);
            return ! threadShouldExit();
        });
        if (result.failed())
            problem = result.getErrorMessage();
        else
            ++written;
        done += length;
    }

    if (problem.isNotEmpty() && ! threadShouldExit())
        setStatus ("Export failed: " + problem, true);
    else if (threadShouldExit())
        setStatus ("Export cancelled" + (written > 0 ? " (" + juce::String (written) + " written)" : juce::String()));
    else
        setStatus ("Exported " + (written == 1 ? juce::String ("1 file") : juce::String (written) + " files"));
    progress = 1.0f;
    running = false;
    ++version;
}

//==============================================================================
juce::Result MixExport::render (Mix& mix, const MixPlayback::Recording& recording, const Region& region, const juce::File& file,
                                bool asFloat, const std::function<bool (float)>& onProgress)
{
    const auto rate = recording.sampleRate;
    if (rate <= 0.0 || region.end <= region.start)
        return juce::Result::fail ("nothing to export");

    // The recording's files, as mapped.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::vector<std::unique_ptr<juce::AudioFormatReader>> readers;
    int widest = 1;
    for (const auto& f : recording.files)
    {
        bool used = false;
        for (const auto in : f.inputs)
            used = used || juce::isPositiveAndBelow (in, MixEngine::kChannels);
        readers.emplace_back (used ? formats.createReaderFor (f.file) : nullptr);
        if (used && readers.back() == nullptr)
            return juce::Result::fail ("can't read " + f.file.getFileName());
        widest = juce::jmax (widest, f.numChannels);
    }

    // A fresh engine with the mix; the inserts prepared again (their state from the last region gone).
    auto engine = std::make_unique<MixEngine>();
    engine->prepare (rate, kBlock);
    ExportPlayHead head;
    head.bpm = mix.bpm;
    head.sampleRate = rate;
    if (mix.inserts != nullptr)
    {
        mix.inserts->prepare (rate, kBlock);
        engine->setInserts (mix.inserts.get());
        engine->setLatencies (mix.inserts->latencies());
    }
    // The scene markers on the way: every step from the preroll to the region's end, in order.
    const auto preRollSamples = juce::jmin (region.start, (juce::int64) (kPreRollSeconds * rate));
    const auto runFrom = region.start - preRollSamples;
    struct Event { juce::int64 at; const State* state; };
    std::vector<Event> events;
    const State* initial = nullptr;
    for (const auto& change : mix.changes)
        for (size_t k = 0; k < change.steps.size(); ++k)
        {
            const auto at = change.at + (juce::int64) k * change.stepSamples;
            if (at <= runFrom)
                initial = &change.steps[k];
            else if (at < region.end)
                events.push_back ({ at, &change.steps[k] });
        }
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.at < b.at; });

    const auto applyInserts = [&mix] (const std::array<bool, InsertRack::kTotalSlots>& on, const std::vector<float>& knobs)
    {
        if (mix.inserts == nullptr)
            return;
        for (int slot = 0; slot < InsertRack::kTotalSlots; ++slot)
        {
            const auto where = InsertRack::slotAt (slot);
            if (mix.inserts->get (where) != nullptr && mix.inserts->isOn (where) != on[(size_t) slot])
                mix.inserts->setOn (where, on[(size_t) slot]);
        }
        for (size_t i = 0; i < mix.knobRefs.size() && i < knobs.size(); ++i)
            if (auto* plugin = mix.inserts->get (InsertRack::slotAt (mix.knobRefs[i].slot)))
                if (auto* p = plugin->findParam (mix.knobRefs[i].paramId); p != nullptr && ! juce::exactlyEqual (p->getValue(), knobs[i]))
                    p->setValueNotifyingHost (knobs[i]);
    };
    const auto noSolo = [] (MixEngine::Levels levels)
    {
        levels.channelSolo.fill (false);
        levels.busSolo.fill (false);
        levels.talk = levels.osc = false;
        return levels;
    };

    if (initial != nullptr)
    {
        engine->setSettings (initial->settings);
        engine->setLevels (noSolo (initial->levels));
        applyInserts (initial->insertOn, initial->knobs);
    }
    else
    {
        engine->setSettings (mix.settings);
        engine->setLevels (noSolo (mix.levels));
        applyInserts (mix.insertOn, mix.knobs);
    }
    engine->reset();
    MainTap tap;
    engine->setOutputTaps (&tap);
    const auto latency = engine->getLatency();
    size_t nextEvent = 0;

    // The file: written beside, moved into place when complete.
    juce::TemporaryFile temp (file);
    std::unique_ptr<juce::AudioFormatWriter> writer;
    {
        std::unique_ptr<juce::OutputStream> out = temp.getFile().createOutputStream();
        if (out == nullptr)
            return juce::Result::fail ("can't write into " + file.getParentDirectory().getFullPathName());
        juce::WavAudioFormat wav;
        auto options = juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (asFloat ? 32 : 24);
        if (asFloat)
            options = options.withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        writer = wav.createWriterFor (out, options);
        if (writer == nullptr)
            return juce::Result::fail ("can't write a WAV file");
    }

    const auto preRoll = juce::jmin (region.start, (juce::int64) (kPreRollSeconds * rate));
    const auto from = region.start - preRoll;                 // the first input sample run
    const auto maxTail = (juce::int64) (kMaxTailSeconds * rate);
    const auto runTotal = preRoll + latency + (region.end - region.start) + maxTail;

    juce::AudioBuffer<float> inputs (MixEngine::kChannels, kBlock), outputs (MixEngine::kChannels, kBlock), chunk (widest, kBlock);
    juce::AudioBuffer<float> tail (2, (int) maxTail);
    juce::MidiBuffer midi;
    int tailLength = 0;

    for (juce::int64 run = 0; run < runTotal;)
    {
        auto at = from + run;                                  // this block's first input sample

        // A scene marker's step due now: the mix changes here (blocks are split at each one).
        while (nextEvent < events.size() && events[nextEvent].at <= at)
        {
            const auto& state = *events[nextEvent++].state;
            engine->setSettings (state.settings);
            engine->setLevels (noSolo (state.levels));
            applyInserts (state.insertOn, state.knobs);
        }
        auto blockLength = juce::jmin<juce::int64> (kBlock, runTotal - run);
        if (nextEvent < events.size())
            blockLength = juce::jmin (blockLength, events[nextEvent].at - at);
        const auto n = (int) juce::jmax<juce::int64> (1, blockLength);
        run += n;

        // The inputs: each file's channels into the inputs they feed, silent from the region's end on.
        inputs.clear();
        const auto live = (int) juce::jlimit<juce::int64> (0, n, region.end - at);
        if (live > 0)
            for (size_t i = 0; i < readers.size(); ++i)
            {
                if (readers[i] == nullptr)
                    continue;
                const auto& f = recording.files[i];
                readers[i]->read (&chunk, 0, live, at, true, chunk.getNumChannels() > 1);
                for (int ch = 0; ch < f.numChannels; ++ch)
                    if (const auto in = f.inputs[(size_t) ch]; juce::isPositiveAndBelow (in, MixEngine::kChannels))
                        inputs.copyFrom (in, 0, chunk, ch, 0, live);
            }

        head.samples = at;
        if (mix.inserts != nullptr)
            mix.inserts->setBlockContext (&head, nullptr, 0);
        engine->process (inputs.getArrayOfReadPointers(), outputs.getArrayOfWritePointers(), n, false);
        if (mix.inserts != nullptr)
        {
            midi.clear();
            mix.inserts->takeMidi (midi);   // what RKTrigger and RKFollow send goes nowhere here
        }

        // Output sample k of this block left the inputs at at + k - latency.
        for (int k = 0; k < n; ++k)
        {
            const auto t = at + k - latency;
            if (t < region.start)
                continue;
            const auto rest = n - k;
            if (t < region.end)
            {
                const auto count = (int) juce::jmin<juce::int64> (rest, region.end - t);
                const float* sides[] = { tap.block.getReadPointer (0, k), tap.block.getReadPointer (1, k) };
                if (! writer->writeFromFloatArrays (sides, 2, count))
                    return juce::Result::fail ("the disk is full, or the file couldn't be written");
                k += count - 1;
            }
            else
            {
                const auto count = (int) juce::jmin<juce::int64> (rest, maxTail - tailLength);
                for (int side = 0; side < 2; ++side)
                    tail.copyFrom (side, tailLength, tap.block, side, k, count);
                tailLength += count;
                k += count - 1;
            }
        }

        if (onProgress != nullptr && ! onProgress ((float) ((double) run / (double) runTotal)))
            return juce::Result::fail ("cancelled");
    }

    // The tail: up to where it falls under the floor, faded out.
    const auto floor = juce::Decibels::decibelsToGain ((float) kTailFloorDb);
    int keep = 0;
    for (int i = tailLength; --i >= 0;)
        if (std::abs (tail.getSample (0, i)) > floor || std::abs (tail.getSample (1, i)) > floor)
        {
            keep = i + 1;
            break;
        }
    if (keep > 0)
    {
        const auto fade = juce::jmin (keep, (int) (kTailFadeMs * 0.001 * rate));
        tail.applyGainRamp (keep - fade, fade, 1.0f, 0.0f);
        if (! writer->writeFromAudioSampleBuffer (tail, 0, keep))
            return juce::Result::fail ("the disk is full, or the file couldn't be written");
    }

    writer.reset();
    if (! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("can't write " + file.getFileName());
    return juce::Result::ok();
}
