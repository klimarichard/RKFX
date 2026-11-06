#include "MixPlayback.h"

namespace
{
    constexpr int kChunk = 4096;

    std::unique_ptr<juce::AudioFormatReader> openReader (const juce::File& f)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        return std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (f));
    }

    /** The input number a plain file's name starts with ("05 Keys.wav", "05-06 Keys.wav"), 1-based, or 0. */
    int leadingNumber (const juce::String& name)
    {
        const auto digits = name.initialSectionContainingOnly ("0123456789");
        return digits.isNotEmpty() ? digits.getIntValue() : 0;
    }
}

//==============================================================================
MixPlayback::Recording MixPlayback::scan (const juce::File& folder)
{
    Recording r;
    r.folder = folder;
    const auto addFile = [&r] (const juce::File& f) -> File*
    {
        auto reader = openReader (f);
        if (reader == nullptr)
            return nullptr;
        File file;
        file.file = f;
        file.numChannels = (int) reader->numChannels;
        file.length = reader->lengthInSamples;
        file.sampleRate = reader->sampleRate;
        file.inputs.assign ((size_t) file.numChannels, -1);
        r.files.push_back (file);
        return &r.files.back();
    };

    const auto session = juce::JSON::parse (folder.getChildFile ("session.json"));
    if (auto* files = session["files"].getDynamicObject())
    {
        // RKMix's own recording: each file and what its channels came from.
        r.sampleRate = (double) session["sampleRate"];
        for (const auto& p : files->getProperties())
        {
            auto* file = addFile (folder.getChildFile (p.name.toString()));
            if (file == nullptr)
                continue;
            const auto* sources = p.value.getArray();
            for (int ch = 0; sources != nullptr && ch < juce::jmin (file->numChannels, sources->size()); ++ch)
            {
                const auto& s = sources->getReference (ch);
                if (s.isInt() || s.isDouble() || s.isInt64())
                    file->inputs[(size_t) ch] = juce::jlimit (-1, kInputs - 1, (int) s - 1);
                else
                    file->role = s.toString().upToFirstOccurrenceOf (" ", false, false);
            }
        }
        if (const auto* markers = session["markers"].getArray())
            for (const auto& m : *markers)
                r.markers.push_back ({ (juce::int64) m["at"], m["label"].toString() });
        if (const auto* names = session["names"].getArray())
            for (const auto& n : *names)
                r.names.add (n.toString());
    }
    else
    {
        // Plain WAVs: by the number each name starts with, else in order.
        auto list = folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
        list.sort();
        std::array<bool, kInputs> taken {};
        std::vector<File*> unnumbered;
        for (const auto& f : list)
        {
            auto* file = addFile (f);
            if (file == nullptr)
                continue;
            const auto first = leadingNumber (f.getFileName()) - 1;
            const auto fits = first >= 0 && first + file->numChannels <= kInputs && (file->numChannels != 2 || first % 2 == 0)
                              && file->numChannels <= 2;
            if (fits)
                for (int ch = 0; ch < file->numChannels; ++ch)
                {
                    file->inputs[(size_t) ch] = first + ch;
                    taken[(size_t) (first + ch)] = true;
                }
            else
                unnumbered.push_back (file);
        }
        for (auto* file : unnumbered)
        {
            if (file->numChannels > 2)
                continue;
            for (int in = 0; in + file->numChannels <= kInputs; in += file->numChannels == 2 ? 2 : 1)
                if (! taken[(size_t) in] && (file->numChannels == 1 || ! taken[(size_t) in + 1]))
                {
                    for (int ch = 0; ch < file->numChannels; ++ch)
                    {
                        file->inputs[(size_t) ch] = in + ch;
                        taken[(size_t) (in + ch)] = true;
                    }
                    break;
                }
        }
    }

    if (r.files.empty())
    {
        r.error = "no audio files in " + folder.getFileName();
        return r;
    }
    for (const auto& f : r.files)
    {
        if (r.sampleRate <= 0.0)
            r.sampleRate = f.sampleRate;
        if (std::abs (f.sampleRate - r.sampleRate) > 0.5)
            r.error = f.file.getFileName() + " has another sample rate than the others";
        r.length = juce::jmax (r.length, f.length);
    }
    return r;
}

//==============================================================================
MixPlayback::MixPlayback() : juce::Thread ("RKMix playback") {}

MixPlayback::~MixPlayback()
{
    stopThread (2000);
}

void MixPlayback::prepare (int maxBlockSize)
{
    const juce::SpinLock::ScopedLockType sl (audioLock);
    scratch.setSize (kInputs, juce::jmax (1, maxBlockSize));
}

juce::Result MixPlayback::load (const Recording& r, double sampleRate)
{
    if (r.error.isNotEmpty())
        return juce::Result::fail (r.error);
    if (std::abs (r.sampleRate - sampleRate) > 0.5)
        return juce::Result::fail ("recorded at " + juce::String (r.sampleRate / 1000.0, 1) + " kHz, but the session runs at "
                                   + juce::String (sampleRate / 1000.0, 1) + " kHz");

    std::vector<std::unique_ptr<juce::AudioFormatReader>> newReaders;
    int widest = 1;
    for (const auto& f : r.files)
    {
        newReaders.push_back (openReader (f.file));
        if (newReaders.back() == nullptr)
            return juce::Result::fail ("can't read " + f.file.getFileName());
        widest = juce::jmax (widest, f.numChannels);
    }

    const auto ringSize = (int) std::ceil (kRingSeconds * sampleRate) + 1;
    {
        const juce::ScopedLock dl (lock);
        const juce::SpinLock::ScopedLockType sl (audioLock);
        recording = r;
        readers = std::move (newReaders);
        fifo = std::make_unique<juce::AbstractFifo> (ringSize);
        ring.setSize (kInputs, ringSize);
        ring.clear();
        chunk.setSize (widest, kChunk);
        fadeSamples = juce::jmax (1, (int) (kFadeMs * 0.001 * sampleRate));
        length = r.length;
        loopOn = false;
        playing = false;
        underruns = 0;
        requestGen = 0;
        ackGen = 0;
        readyGen = -1;
        audioGen = 0;
        diskGen = -1;
        position = 0;
        seekTarget = 0;
        shownPosition = 0;
        gain = 0.0f;
        updateRoutes();
        loaded = true;
    }
    if (! isThreadRunning())
        startThread (juce::Thread::Priority::high);
    return juce::Result::ok();
}

void MixPlayback::unload()
{
    const juce::ScopedLock dl (lock);
    const juce::SpinLock::ScopedLockType sl (audioLock);
    loaded = false;
    playing = false;
    readers.clear();
    recording = {};
    routes.fill ({ -1, -1 });
}

void MixPlayback::updateRoutes()
{
    routes.fill ({ -1, -1 });
    for (int i = 0; i < (int) recording.files.size(); ++i)
        for (int ch = 0; ch < (int) recording.files[(size_t) i].inputs.size(); ++ch)
            if (const auto in = recording.files[(size_t) i].inputs[(size_t) ch]; juce::isPositiveAndBelow (in, kInputs))
                routes[(size_t) in] = { i, ch };
}

std::pair<int, int> MixPlayback::sourceOf (int input) const
{
    const juce::ScopedLock dl (lock);
    return juce::isPositiveAndBelow (input, kInputs) ? routes[(size_t) input] : std::pair<int, int> { -1, -1 };
}

bool MixPlayback::mapFile (int index, int firstInput)
{
    const juce::ScopedLock dl (lock);
    if (! juce::isPositiveAndBelow (index, (int) recording.files.size()))
        return false;
    auto& file = recording.files[(size_t) index];
    const auto n = file.numChannels;
    if (firstInput >= 0 && (n > 2 || firstInput + n > kInputs || (n == 2 && firstInput % 2 != 0)))
        return false;

    // An input takes one source: whatever fed these inputs before lets go
    // (a stereo file lets go of its whole pair).
    if (firstInput >= 0)
        for (auto& other : recording.files)
        {
            bool overlaps = false;
            for (const auto in : other.inputs)
                overlaps = overlaps || (in >= firstInput && in < firstInput + n);
            if (overlaps)
                for (auto& in : other.inputs)
                    in = -1;
        }
    for (int ch = 0; ch < n; ++ch)
        file.inputs[(size_t) ch] = firstInput >= 0 ? firstInput + ch : -1;
    updateRoutes();
    relocate (shownPosition.load());   // the ring refills with the new mapping
    return true;
}

//==============================================================================
void MixPlayback::relocate (juce::int64 target)
{
    seekTarget = juce::jlimit<juce::int64> (0, juce::jmax<juce::int64> (0, length.load()), target);
    shownPosition = seekTarget.load();
    ++requestGen;
}

void MixPlayback::play()   { if (loaded) playing = true; }
void MixPlayback::pause()  { playing = false; }
void MixPlayback::stop()   { playing = false; relocate (0); }
void MixPlayback::locate (juce::int64 sample) { relocate (sample); }

void MixPlayback::setLoop (bool on, juce::int64 start, juce::int64 end)
{
    start = juce::jlimit<juce::int64> (0, length.load(), start);
    end = juce::jlimit<juce::int64> (0, length.load(), end);
    if (on && end - start < 2 * fadeSamples)
        on = false;
    loopStart = start;
    loopEnd = end;
    loopOn = on;
    const auto now = shownPosition.load();
    relocate (on && (now < start || now >= end) ? start : now);
}

juce::int64 MixPlayback::markerNear (bool next) const
{
    const auto now = shownPosition.load();
    const auto back = (juce::int64) (0.5 * juce::jmax (1.0, recording.sampleRate));
    juce::int64 best = next ? recording.length : 0;
    for (const auto& m : recording.markers)
    {
        if (next && m.at > now && m.at < best)
            best = m.at;
        if (! next && m.at < now - back && m.at > best)
            best = m.at;
    }
    return best;
}

void MixPlayback::loopAroundPosition (bool on)
{
    if (! on)
    {
        setLoop (false, 0, 0);
        return;
    }
    juce::int64 start = 0, end = recording.length;
    for (const auto& m : recording.markers)
    {
        if (m.at <= shownPosition.load())
            start = juce::jmax (start, m.at);
        else
            end = juce::jmin (end, m.at);
    }
    setLoop (true, start, end);
}

bool MixPlayback::waitUntilReady (int ms) const
{
    const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < until)
    {
        if (readyGen.load() == requestGen.load() && ackGen.load() == requestGen.load())
            return true;
        juce::Thread::sleep (2);
    }
    return false;
}

//==============================================================================
void MixPlayback::run()
{
    while (! threadShouldExit())
    {
        wait (5);
        if (stalled)
            continue;
        const juce::ScopedLock dl (lock);
        if (! loaded || fifo == nullptr)
            continue;

        // The audio thread has let go of the old position: empty the ring, start from the new one.
        if (const auto acked = ackGen.load(); acked != diskGen)
        {
            diskGen = acked;
            fifo->reset();
            diskPosition = seekTarget.load();
            diskLoop = loopOn.load();
            diskLoopStart = loopStart.load();
            diskLoopEnd = loopEnd.load();
        }
        fill();
        const auto enough = (int) (kStartSeconds * recording.sampleRate);
        if (readyGen.load() != diskGen && (fifo->getNumReady() >= juce::jmin (enough, fifo->getTotalSize() - 1)
                                           || (! diskLoop && diskPosition >= length.load())))
            readyGen = diskGen;
    }
}

void MixPlayback::fill()
{
    while (fifo->getFreeSpace() > 0)
    {
        auto n = juce::jmin (fifo->getFreeSpace(), kChunk);
        if (diskLoop && diskPosition < diskLoopEnd)
            n = (int) juce::jmin<juce::int64> (n, diskLoopEnd - diskPosition);

        int s1, z1, s2, z2;
        fifo->prepareToWrite (n, s1, z1, s2, z2);
        n = z1 + z2;
        if (n <= 0)
            return;

        for (int in = 0; in < kInputs; ++in)
        {
            ring.clear (in, s1, z1);
            if (z2 > 0)
                ring.clear (in, s2, z2);
        }

        // Every file from the same place; each mapped channel into its input's ring channel.
        for (int i = 0; i < (int) readers.size(); ++i)
        {
            bool used = false;
            for (const auto& [file, ch] : routes)
                used = used || file == i;
            if (! used)
                continue;
            readers[(size_t) i]->read (&chunk, 0, n, diskPosition, true, chunk.getNumChannels() > 1);
            for (int in = 0; in < kInputs; ++in)
                if (routes[(size_t) in].first == i)
                {
                    const auto ch = routes[(size_t) in].second;
                    ring.copyFrom (in, s1, chunk, ch, 0, z1);
                    if (z2 > 0)
                        ring.copyFrom (in, s2, chunk, ch, z1, z2);
                }
        }

        // A loop's seam: faded out before its end and in after its start.
        if (diskLoop)
            for (int k = 0; k < n; ++k)
            {
                const auto p = diskPosition + k;
                const auto toEnd = diskLoopEnd - p, fromStart = p - diskLoopStart;
                const auto g = (float) juce::jlimit<juce::int64> (0, fadeSamples, juce::jmin (toEnd, fromStart)) / (float) fadeSamples;
                if (g < 1.0f)
                {
                    const auto index = k < z1 ? s1 + k : s2 + (k - z1);
                    for (int in = 0; in < kInputs; ++in)
                        ring.setSample (in, index, ring.getSample (in, index) * g);
                }
            }

        fifo->finishedWrite (n);
        diskPosition += n;
        if (diskLoop && diskPosition >= diskLoopEnd)
            diskPosition = diskLoopStart;
    }
}

//==============================================================================
void MixPlayback::process (float* const* inputs, const std::array<bool, kInputs>& onPlayback, int numSamples) noexcept
{
    const auto silence = [&]
    {
        for (int in = 0; in < kInputs; ++in)
            if (onPlayback[(size_t) in])
                juce::FloatVectorOperations::clear (inputs[in], numSamples);
    };

    const juce::SpinLock::ScopedTryLockType sl (audioLock);
    if (! sl.isLocked() || ! loaded || fifo == nullptr || numSamples > scratch.getNumSamples())
    {
        silence();
        return;
    }

    const auto gen = requestGen.load();
    const auto pending = gen != audioGen;
    const auto ready = readyGen.load() == audioGen;
    const auto target = playing.load() && ready && ! pending ? 1.0f : 0.0f;

    // Silent and staying so: a new position is taken now; nothing is read.
    if (gain <= 0.0f && target <= 0.0f)
    {
        if (pending)
        {
            audioGen = gen;
            position = seekTarget.load();
            shownPosition = position;
            ackGen = gen;
        }
        silence();
        return;
    }

    // The ring ran dry: silence, counted.
    if (! ready || fifo->getNumReady() < numSamples)
    {
        if (playing)
            ++underruns;
        gain = 0.0f;
        silence();
        return;
    }

    int s1, z1, s2, z2;
    fifo->prepareToRead (numSamples, s1, z1, s2, z2);
    for (int in = 0; in < kInputs; ++in)
    {
        scratch.copyFrom (in, 0, ring, in, s1, z1);
        if (z2 > 0)
            scratch.copyFrom (in, z1, ring, in, s2, z2);
    }
    fifo->finishedRead (z1 + z2);

    // 5 ms fades on play, pause, stop and locate.
    if (! juce::exactlyEqual (gain, target) || gain < 1.0f)
    {
        const auto step = 1.0f / (float) fadeSamples;
        for (int i = 0; i < numSamples; ++i)
        {
            gain = target > gain ? juce::jmin (target, gain + step) : juce::jmax (target, gain - step);
            for (int in = 0; in < kInputs; ++in)
                scratch.setSample (in, i, scratch.getSample (in, i) * gain);
        }
    }

    position += numSamples;
    if (loopOn && position >= loopEnd.load() && loopEnd.load() > loopStart.load())
        position = loopStart.load() + (position - loopEnd.load()) % (loopEnd.load() - loopStart.load());
    else if (! loopOn && position >= length.load())
    {
        position = length.load();
        playing = false;
    }
    shownPosition = position;

    for (int in = 0; in < kInputs; ++in)
        if (onPlayback[(size_t) in])
            juce::FloatVectorOperations::copy (inputs[in], scratch.getReadPointer (in), numSamples);
}
