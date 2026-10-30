#include "PlayStream.h"

#include <cmath>

namespace
{
    // The windowed-sinc interpolator delays by this many source samples: that
    // many output frames are dropped at the start, and as many zeros flushed at the end.
    constexpr int kSincLatency = (int) juce::WindowedSincInterpolator::getBaseLatency();
}

PlayStream::~PlayStream()
{
    close();
}

void PlayStream::prepare (double newDeviceRate)
{
    close();
    deviceRate = newDeviceRate;
    const auto capacity = (int) std::ceil (kBufferSeconds * deviceRate) + 1;
    ring.setSize (2, capacity);
    fifo = std::make_unique<juce::AbstractFifo> (capacity);
}

void PlayStream::open (std::unique_ptr<juce::AudioFormatReader> newReader, double start)
{
    close();
    if (newReader == nullptr || fifo == nullptr || newReader->sampleRate <= 0.0)
        return;

    reader = std::move (newReader);
    ratio = reader->sampleRate / deviceRate;
    const auto length = reader->lengthInSamples;
    lengthSeconds = (double) length / reader->sampleRate;
    startSeconds = juce::jlimit (0.0, lengthSeconds, start);
    sourcePos = juce::jlimit<juce::int64> (0, length, (juce::int64) std::llround (startSeconds * reader->sampleRate));
    totalFrames = (juce::int64) ((double) (length - sourcePos) / ratio);

    const auto resampling = ! juce::exactlyEqual (ratio, 1.0);
    source.setSize (2, kChunk + 2 * kSincLatency + 16, false, false, true);
    resampled.setSize (2, (int) std::ceil (source.getNumSamples() / ratio) + 16, false, false, true);
    sourceCount = 0;
    sourceEnded = false;
    for (auto& i : interpolators)
        i.reset();
    // Drop the interpolator's delay at the start: its first frames are its own lead-in.
    dropFrames = resampling ? (int) std::lround (kSincLatency / ratio) : 0;

    fifo->reset();
    framesPlayed = 0;
    sourceDone = false;
}

void PlayStream::close()
{
    reader.reset();
    if (fifo != nullptr)
        fifo->reset();
    framesPlayed = 0;
    totalFrames = 0;
    sourceDone = false;
}

//==============================================================================
int PlayStream::read (float* const* dest, int offset, int numFrames) noexcept
{
    if (fifo == nullptr)
        return 0;

    int start1, size1, start2, size2;
    fifo->prepareToRead (numFrames, start1, size1, start2, size2);
    for (int ch = 0; ch < 2; ++ch)
    {
        if (size1 > 0)
            juce::FloatVectorOperations::copy (dest[ch] + offset, ring.getReadPointer (ch, start1), size1);
        if (size2 > 0)
            juce::FloatVectorOperations::copy (dest[ch] + offset + size1, ring.getReadPointer (ch, start2), size2);
    }
    fifo->finishedRead (size1 + size2);
    framesPlayed += size1 + size2;
    return size1 + size2;
}

//==============================================================================
void PlayStream::writeRing (const float* const* data, int numFrames)
{
    int start1, size1, start2, size2;
    fifo->prepareToWrite (numFrames, start1, size1, start2, size2);
    jassert (size1 + size2 == numFrames);
    for (int ch = 0; ch < 2; ++ch)
    {
        if (size1 > 0)
            juce::FloatVectorOperations::copy (ring.getWritePointer (ch, start1), data[ch], size1);
        if (size2 > 0)
            juce::FloatVectorOperations::copy (ring.getWritePointer (ch, start2), data[ch] + size1, size2);
    }
    fifo->finishedWrite (size1 + size2);
}

bool PlayStream::readSomething()
{
    if (reader == nullptr || sourceDone.load())
        return false;

    const auto free = fifo->getFreeSpace();
    const auto resampling = ! juce::exactlyEqual (ratio, 1.0);

    if (! resampling)
    {
        // Straight copy: read as much as fits, up to a chunk.
        const auto want = (int) juce::jmin<juce::int64> (juce::jmin (free, kChunk), reader->lengthInSamples - sourcePos);
        if (want <= 0)
        {
            if (sourcePos >= reader->lengthInSamples)
                sourceDone = true;
            return false;
        }
        reader->read (&source, 0, want, sourcePos, true, true);   // a mono file on both sides
        sourcePos += want;
        const float* data[2] = { source.getReadPointer (0), source.getReadPointer (1) };
        writeRing (data, want);
        if (sourcePos >= reader->lengthInSamples)
            sourceDone = true;
        return true;
    }

    // Resampling: top up the source buffer, then make as many frames as it allows.
    if (! sourceEnded && sourceCount < kChunk)
    {
        const auto want = (int) juce::jmin<juce::int64> (kChunk - sourceCount, reader->lengthInSamples - sourcePos);
        if (want > 0)
        {
            juce::AudioBuffer<float> view (source.getArrayOfWritePointers(), 2, sourceCount, want);
            reader->read (&view, 0, want, sourcePos, true, true);
            sourcePos += want;
            sourceCount += want;
        }
        if (sourcePos >= reader->lengthInSamples)
        {
            // The file's end: zeros push the interpolator's last samples out.
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::clear (source.getWritePointer (ch, sourceCount), 2 * kSincLatency);
            sourceCount += 2 * kSincLatency;
            sourceEnded = true;
        }
    }

    auto frames = (int) std::floor ((sourceCount - 4) / ratio);
    frames = juce::jmin (frames, free + dropFrames, resampled.getNumSamples());
    if (frames <= 0)
    {
        if (sourceEnded)
            sourceDone = true;
        return false;
    }

    int used = 0;
    for (int ch = 0; ch < 2; ++ch)
        used = interpolators[ch].process (ratio, source.getReadPointer (ch), resampled.getWritePointer (ch), frames);
    for (int ch = 0; ch < 2; ++ch)
        std::memmove (source.getWritePointer (ch), source.getReadPointer (ch, used), sizeof (float) * (size_t) (sourceCount - used));
    sourceCount -= used;

    const auto drop = juce::jmin (dropFrames, frames);
    dropFrames -= drop;
    if (frames > drop)
    {
        const float* data[2] = { resampled.getReadPointer (0, drop), resampled.getReadPointer (1, drop) };
        writeRing (data, juce::jmin (frames - drop, fifo->getFreeSpace()));
    }
    if (sourceEnded && sourceCount <= 4)
        sourceDone = true;
    return true;
}

int PlayStream::useTimeSlice()
{
    // Read until the ring is full or a few chunks are in, then rest a little.
    for (int i = 0; i < 4; ++i)
        if (fifo == nullptr || fifo->getFreeSpace() < kChunk / 2 || ! readSomething())
            break;
    if (reader == nullptr || sourceDone.load())
        return 200;
    return fifo->getFreeSpace() < kChunk ? 20 : 1;
}

void PlayStream::prefill (double seconds)
{
    const auto frames = (int) (seconds * deviceRate);
    while (fifo != nullptr && available() < frames && fifo->getFreeSpace() > 0 && readSomething())
    {
    }
}

void PlayStream::fillNow()
{
    while (fifo != nullptr && fifo->getFreeSpace() > 0 && readSomething())
    {
    }
}
