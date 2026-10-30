#pragma once

#include <algorithm>
#include <vector>

/** A ring buffer written a block at a time and read back at any delay,
    several times per block (RKMix's latency alignment: one source, several
    destinations each needing a different delay). Allocation-free after
    prepare(). */
class BlockDelay
{
public:
    void prepare (int maxDelaySamples, int maxBlock)
    {
        int size = 16;
        while (size < maxDelaySamples + maxBlock + 1)
            size *= 2;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writeIndex = 0;
        maxDelay = maxDelaySamples;
    }

    void clear() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void write (const float* x, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            buffer[(size_t) ((writeIndex + i) & mask)] = x[i];
        writeIndex = (writeIndex + numSamples) & mask;
    }

    /** The last block written, @p delay samples later (clamped to the line's length). */
    void read (float* dest, int numSamples, int delay) const noexcept
    {
        const auto start = writeIndex - numSamples - std::clamp (delay, 0, maxDelay);
        for (int i = 0; i < numSamples; ++i)
            dest[i] = buffer[(size_t) ((start + i) & mask)];
    }

private:
    std::vector<float> buffer;
    int mask = 0, writeIndex = 0, maxDelay = 0;
};
