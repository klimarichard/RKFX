#include "MixRecorder.h"

#include <array>

namespace
{
    constexpr int kInputs = 32, kBuses = 16, kMaxDropoutPlaces = 512;

    juce::String sourceName (const MixRecorder::Source& s)
    {
        const auto side = juce::String (s.side == 0 ? " L" : " R");
        switch (s.kind)
        {
            case MixRecorder::Source::input: return juce::String (s.index + 1);
            case MixRecorder::Source::main:  return "main" + side;
            case MixRecorder::Source::bus:   return "bus" + juce::String (s.index + 1) + side;
        }
        return {};
    }
}

//==============================================================================
struct MixRecorder::Live
{
    struct File
    {
        Track track;
        juce::File file;
        std::unique_ptr<juce::AudioFormatWriter> writer;
        std::unique_ptr<juce::AbstractFifo> fifo;
        juce::AudioBuffer<float> buffer;
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;   // this block's room (audio thread)
        bool failed = false;
    };
    using Place = std::pair<int, int>;   // a file, its channel

    Take take;
    std::vector<std::unique_ptr<File>> files;
    std::array<std::vector<Place>, kInputs> inputs;
    std::array<std::vector<Place>, 2> main;
    std::array<std::array<std::vector<Place>, 2>, kBuses> buses;

    juce::Time started;
    juce::int64 samples = 0;                                  // audio thread
    std::array<juce::int64, kMaxDropoutPlaces> dropoutAt {};  // audio thread writes, read after the take
    std::atomic<int> numDropouts { 0 };
    std::vector<std::pair<juce::int64, juce::String>> markers;   // message thread (under diskLock)
    juce::StringArray notes;
    juce::int64 sinceFlush = 0;                               // disk thread
    bool writeFailed = false;

    /** Writes what the FIFOs hold (disk thread, or after the take with the audio thread gone). */
    void drain()
    {
        for (auto& f : files)
        {
            int s1, z1, s2, z2;
            f->fifo->prepareToRead (f->fifo->getNumReady(), s1, z1, s2, z2);
            if (z1 + z2 == 0)
                continue;
            auto ok = z1 <= 0 || f->writer->writeFromAudioSampleBuffer (f->buffer, s1, z1);
            ok = (z2 <= 0 || f->writer->writeFromAudioSampleBuffer (f->buffer, s2, z2)) && ok;
            f->fifo->finishedRead (z1 + z2);
            if (! ok)
                f->failed = writeFailed = true;
        }
    }
};

//==============================================================================
MixRecorder::MixRecorder() : juce::Thread ("RKMix recorder") {}

MixRecorder::~MixRecorder()
{
    stop();
    stopThread (2000);
}

double MixRecorder::bytesPerSecondFor (int numChannels, double sampleRate, bool asFloat)
{
    return numChannels * sampleRate * (asFloat ? 4.0 : 3.0);
}

juce::File MixRecorder::getFolder() const
{
    const juce::ScopedLock sl (diskLock);
    return live != nullptr ? live->take.folder : juce::File();
}

juce::Result MixRecorder::start (const Take& take)
{
    if (take.tracks.empty())
        return juce::Result::fail ("nothing to record");
    if (! take.folder.createDirectory())
        return juce::Result::fail ("can't create the folder " + take.folder.getFullPathName());

    auto next = std::make_unique<Live>();
    next->take = take;
    next->started = juce::Time::getCurrentTime();
    const auto fifoSize = (int) std::ceil (kFifoSeconds * take.sampleRate) + 1;
    int channels = 0;

    juce::WavAudioFormat wav;
    for (const auto& track : take.tracks)
    {
        auto f = std::make_unique<Live::File>();
        f->track = track;
        f->file = take.folder.getChildFile (track.fileName);
        const auto numChannels = (int) track.sources.size();
        std::unique_ptr<juce::OutputStream> stream = f->file.createOutputStream();
        if (stream == nullptr || numChannels < 1)
            return juce::Result::fail ("can't create " + f->file.getFullPathName());
        static_cast<juce::FileOutputStream*> (stream.get())->setPosition (0);
        static_cast<juce::FileOutputStream*> (stream.get())->truncate();

        const auto options = juce::AudioFormatWriterOptions{}
                                 .withSampleRate (take.sampleRate)
                                 .withNumChannels (numChannels)
                                 .withBitsPerSample (take.asFloat ? 32 : 24)
                                 .withSampleFormat (take.asFloat ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                                                 : juce::AudioFormatWriterOptions::SampleFormat::integral);
        f->writer = wav.createWriterFor (stream, options);
        if (f->writer == nullptr)
            return juce::Result::fail ("can't write a WAV file to " + f->file.getFullPathName());

        f->fifo = std::make_unique<juce::AbstractFifo> (fifoSize);
        f->buffer.setSize (numChannels, fifoSize);
        f->buffer.clear();

        const auto index = (int) next->files.size();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto& s = track.sources[(size_t) ch];
            if (s.kind == Source::input && juce::isPositiveAndBelow (s.index, kInputs))
                next->inputs[(size_t) s.index].emplace_back (index, ch);
            else if (s.kind == Source::main)
                next->main[(size_t) (s.side & 1)].emplace_back (index, ch);
            else if (s.kind == Source::bus && juce::isPositiveAndBelow (s.index, kBuses))
                next->buses[(size_t) s.index][(size_t) (s.side & 1)].emplace_back (index, ch);
        }
        channels += numChannels;
        next->files.push_back (std::move (f));
    }
    writeSession (*next);

    // Swap it in: the audio thread starts it on its next block (a running take ends on the block before).
    std::unique_ptr<Live> old;
    {
        const juce::ScopedLock dl (diskLock);
        {
            const juce::SpinLock::ScopedLockType sl (swapLock);
            old = std::move (live);
            live = std::move (next);
            recorded = 0;
            dropouts = 0;
        }
        rate = take.sampleRate;
        bytesPerSecond = bytesPerSecondFor (channels, take.sampleRate, take.asFloat);
        diskNearlyFull = false;
        recording = true;
        if (old != nullptr)
            finish (*old);
    }

    if (! isThreadRunning())
        startThread (juce::Thread::Priority::high);
    return juce::Result::ok();
}

void MixRecorder::stop()
{
    const juce::ScopedLock dl (diskLock);
    std::unique_ptr<Live> old;
    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        old = std::move (live);
    }
    recording = false;
    if (old != nullptr)
        finish (*old);
}

void MixRecorder::finish (Live& take)
{
    // The audio thread no longer sees it: write what's left, close the files.
    take.drain();
    for (auto& f : take.files)
        f->writer.reset();   // the header, final
    writeSession (take);
}

void MixRecorder::addMarker (const juce::String& label)
{
    const juce::ScopedLock dl (diskLock);
    if (live == nullptr)
        return;
    live->markers.emplace_back (recorded.load(), label);
    writeSession (*live);
}

int MixRecorder::getNumMarkers() const
{
    const juce::ScopedLock dl (diskLock);
    return live != nullptr ? (int) live->markers.size() : 0;
}

void MixRecorder::addNote (const juce::String& note)
{
    const juce::ScopedLock dl (diskLock);
    if (live != nullptr)
    {
        live->notes.add (note);
        writeSession (*live);
    }
}

void MixRecorder::writeSession (Live& take)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("app", "RKMix");
    root->setProperty ("version", 1);
    root->setProperty ("sampleRate", take.take.sampleRate);
    root->setProperty ("format", take.take.asFloat ? "32-bit float" : "24-bit");
    root->setProperty ("started", take.started.toISO8601 (true));
    root->setProperty ("length", recordedSamplesOf (take));

    // Each file and what its channels came from: inputs by number (1..32), "main L", "bus3 R".
    auto* files = new juce::DynamicObject();
    for (const auto& f : take.files)
    {
        juce::Array<juce::var> sources;
        for (const auto& s : f->track.sources)
            sources.add (s.kind == Source::input ? juce::var (s.index + 1) : juce::var (sourceName (s)));
        files->setProperty (f->track.fileName, sources);
    }
    root->setProperty ("files", juce::var (files));

    juce::Array<juce::var> markers;
    for (const auto& [at, label] : take.markers)
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("at", at);
        m->setProperty ("label", label);
        markers.add (juce::var (m));
    }
    root->setProperty ("markers", markers);

    auto* drops = new juce::DynamicObject();
    const auto count = take.numDropouts.load();
    drops->setProperty ("count", count);
    juce::Array<juce::var> at;
    for (int i = 0; i < juce::jmin (count, kMaxDropoutPlaces); ++i)
        at.add (take.dropoutAt[(size_t) i]);
    drops->setProperty ("at", at);
    root->setProperty ("dropouts", juce::var (drops));
    if (take.writeFailed)
        take.notes.addIfNotAlreadyThere ("a file couldn't be written completely (is the disk full?)");
    root->setProperty ("notes", take.notes);

    if (auto* extra = take.take.info.getDynamicObject())
        for (const auto& p : extra->getProperties())
            root->setProperty (p.name, p.value);

    take.take.folder.getChildFile ("session.json").replaceWithText (juce::JSON::toString (juce::var (root)));
}

juce::int64 MixRecorder::recordedSamplesOf (const Live& take)
{
    return take.samples;
}

//==============================================================================
void MixRecorder::run()
{
    auto lastSpaceCheck = 0.0;
    while (! threadShouldExit())
    {
        wait (20);
        if (stalled)
            continue;

        const juce::ScopedLock dl (diskLock);
        if (live == nullptr)
            continue;

        // Drain the FIFOs; bring the headers up to date every 10 s.
        live->drain();
        const auto now = recorded.load();
        if (now - live->sinceFlush >= (juce::int64) (kFlushSeconds * live->take.sampleRate))
        {
            live->sinceFlush = now;
            for (auto& f : live->files)
                if (f->writer != nullptr)
                    f->writer->flush();
            writeSession (*live);   // its length too: a power cut leaves session.json at most 10 s behind
        }

        // Under a minute left on the disk: whoever runs the recorder stops it.
        const auto ms = juce::Time::getMillisecondCounterHiRes();
        if (ms - lastSpaceCheck > 1000.0)
        {
            lastSpaceCheck = ms;
            const auto free = (double) live->take.folder.getBytesFreeOnVolume();
            if (free < bytesPerSecond.load() * 60.0 || live->writeFailed)
                diskNearlyFull = true;
        }
    }
}

//==============================================================================
void MixRecorder::beginBlock (int numSamples) noexcept
{
    swapLock.enter();
    blockTake = live.get();
    blockSamples = numSamples;
    blockOk = false;
    if (blockTake == nullptr || numSamples <= 0)
        return;

    // Room in every file, or the block goes into none of them (they stay aligned).
    blockOk = true;
    for (auto& f : blockTake->files)
    {
        f->fifo->prepareToWrite (numSamples, f->start1, f->size1, f->start2, f->size2);
        if (f->size1 + f->size2 < numSamples)
            blockOk = false;
    }
    if (! blockOk)
    {
        const auto n = blockTake->numDropouts.load();
        if (n < kMaxDropoutPlaces)
            blockTake->dropoutAt[(size_t) n] = blockTake->samples;
        blockTake->numDropouts = n + 1;
        dropouts = n + 1;
        return;
    }

    // Silence until something is pushed (a bus that didn't run this block).
    for (auto& f : blockTake->files)
        for (int ch = 0; ch < f->buffer.getNumChannels(); ++ch)
        {
            f->buffer.clear (ch, f->start1, f->size1);
            if (f->size2 > 0)
                f->buffer.clear (ch, f->start2, f->size2);
        }
}

namespace
{
    template <typename Places, typename Files>
    void copyInto (const Places& places, Files& files, const float* data) noexcept
    {
        for (const auto& [index, ch] : places)
        {
            auto& f = *files[(size_t) index];
            f.buffer.copyFrom (ch, f.start1, data, f.size1);
            if (f.size2 > 0)
                f.buffer.copyFrom (ch, f.start2, data + f.size1, f.size2);
        }
    }
}

void MixRecorder::pushInput (int input, const float* data) noexcept
{
    if (blockOk && juce::isPositiveAndBelow (input, kInputs) && data != nullptr)
        copyInto (blockTake->inputs[(size_t) input], blockTake->files, data);
}

void MixRecorder::pushMain (const float* left, const float* right) noexcept
{
    if (! blockOk)
        return;
    copyInto (blockTake->main[0], blockTake->files, left);
    copyInto (blockTake->main[1], blockTake->files, right != nullptr ? right : left);
}

void MixRecorder::pushBus (int bus, const float* left, const float* right) noexcept
{
    if (! blockOk || ! juce::isPositiveAndBelow (bus, kBuses))
        return;
    copyInto (blockTake->buses[(size_t) bus][0], blockTake->files, left);
    copyInto (blockTake->buses[(size_t) bus][1], blockTake->files, right != nullptr ? right : left);
}

void MixRecorder::discardForBenchmark() noexcept
{
    if (live != nullptr)
        for (auto& f : live->files)
            f->fifo->finishedRead (f->fifo->getNumReady());
}

void MixRecorder::endBlock() noexcept
{
    if (blockOk)
    {
        for (auto& f : blockTake->files)
            f->fifo->finishedWrite (f->size1 + f->size2);
        blockTake->samples += blockSamples;
        recorded = blockTake->samples;
    }
    blockOk = false;
    blockTake = nullptr;
    swapLock.exit();
}
