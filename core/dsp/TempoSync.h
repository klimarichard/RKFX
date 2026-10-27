#pragma once

#include <array>

namespace rkfx
{

/** The host's tempo and position at the start of the block being processed. */
struct Transport
{
    double bpm = 120.0;
    bool hasPosition = false;   // ppq is valid (the host provides a beat position)
    double ppq = 0.0;           // position in quarter notes
    bool playing = false;
};

/** Note divisions for tempo sync, shared by every plugin with a sync switch:
    1/1 to 1/32, each dotted, straight and triplet (18 values). */
namespace SyncDivision
{
    inline constexpr int kCount = 18;

    /** Display names, slowest first: "1/1 D", "1/1", "1/1 T", "1/2 D", ... */
    inline const char* const names[kCount] = {
        "1/1 D",  "1/1",  "1/1 T",  "1/2 D",  "1/2",  "1/2 T",
        "1/4 D",  "1/4",  "1/4 T",  "1/8 D",  "1/8",  "1/8 T",
        "1/16 D", "1/16", "1/16 T", "1/32 D", "1/32", "1/32 T"
    };

    /** Preset keys (lowercase). */
    inline const char* const keys[kCount] = {
        "1/1d",  "1/1",  "1/1t",  "1/2d",  "1/2",  "1/2t",
        "1/4d",  "1/4",  "1/4t",  "1/8d",  "1/8",  "1/8t",
        "1/16d", "1/16", "1/16t", "1/32d", "1/32", "1/32t"
    };

    inline constexpr int index (int denominator, char kind = ' ')
    {
        int row = 0;
        for (int d = 1; d < denominator; d *= 2)
            ++row;
        return row * 3 + (kind == 'd' ? 0 : kind == 't' ? 2 : 1);
    }

    /** Length of one cycle in quarter notes. */
    inline double beats (int divisionIndex)
    {
        const auto row = divisionIndex / 3;
        const auto kind = divisionIndex % 3;
        const auto straight = 4.0 / (double) (1 << row);
        return kind == 0 ? straight * 1.5 : kind == 2 ? straight * 2.0 / 3.0 : straight;
    }
}

} // namespace rkfx
