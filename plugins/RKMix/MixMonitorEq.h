#pragma once

#include "MixMeasure.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

/** Monitor EQ's maths (docs/design/RKMix Monitor EQ.md): from measured band
    powers (or a loaded response file) to a 31-band RKGraphEQ suggestion.
    Message-thread code: no audio, no JUCE, no allocation worries. */
namespace monitoreq
{
    constexpr int kBands = MixMeasure::kBands;
    using Bands = std::array<float, kBands>;
    using Flags = std::array<bool, kBands>;

    constexpr float kGraphEqRangeDb = 12.0f;     // RKGraphEQ's band range, +-
    constexpr float kMaxMicCompensationDb = 6.0f;
    constexpr float kMinSignalOverFloorDb = 10.0f;

    enum class Type { speaker, inEar };
    enum class Target { flat, gentleTilt, vocalWedge, diffuseField, speakersInRoom };
    enum class Mic { flat, sm58, sm57, v7 };

    /** The targets each type offers, first = its default. */
    std::vector<Target> targetsFor (Type);
    const char* targetName (Target);
    const char* micName (Mic);

    /** A mic's typical response per band (dB, 0 at 1 kHz). */
    const Bands& micCurve (Mic);

    /** A target per band (dB). @p bassDb: Speakers in a room's bass shelf. */
    Bands target (Target, float bassDb = 4.0f);

    /** A response file (a calibration file or a published measurement):
        lines of frequency and dB, separated by spaces, tabs, commas or
        semicolons; lines starting with '*', '"' or a letter (a header,
        "Sens Factor = ...") are skipped, and columns after the second are
        ignored. Each band is the mean of the points within its edges (or
        interpolated in log frequency where there are fewer than two),
        held flat beyond the file's range. Empty when fewer than 3 points. */
    std::optional<Bands> readResponseFile (const std::string& text);

    /** One stretch of a measurement: the mean power per band of the input and the noise. */
    struct Window
    {
        std::array<double, kBands> input {}, reference {};
        double seconds = 0.0;
    };

    /** The window between two reads of the running sums (@p from earlier). */
    Window windowBetween (const MixMeasure::Sums& from, const MixMeasure::Sums& to, double sampleRate);

    /** The response per band (dB, before normalising), with what can be trusted. */
    struct Response
    {
        Bands db {};
        Flags hasData {};
        Flags lowConfidence {};     // the mic was more than 6 dB down there
    };

    /** The response from the noise floor and the positions (averaged as power).
        A band counts in a position when the input was 10 dB above the floor
        there; the floor's power is taken off first. The mic's own curve is
        taken off, but never by more than +6 dB (low confidence there).
        Bands the rate can't measure have no data. */
    Response measuredResponse (const Window& floor, const std::vector<Window>& positions, const Bands& micDb,
                               double sampleRate);

    /** A loaded measurement (In-ear without a coupler): every band has data. */
    Response fileResponse (const Bands& db);

    /** The largest difference between two responses in bands both have (dB), e.g. left against right. */
    float maxDifferenceDb (const Response&, const Response&);

    struct Options
    {
        Type type = Type::speaker;
        Target target = Target::gentleTilt;
        float bassDb = 4.0f;        // Speakers in a room
        float strength = 0.75f;     // 0..1
    };

    struct Suggestion
    {
        Bands normalised {};        // the response, 0 dB = its median over 250 Hz .. 4 kHz
        Bands targetDb {};          // the target, normalised the same way
        Bands correction {};        // what the EQ should add on top of what was measured
        Bands gains {};             // the new RKGraphEQ gains: current + correction, within +-12 dB
        Flags noData {}, outOfRange {}, null {}, lowConfidence {};
    };

    /** The correction for a response measured through @p currentGains. */
    Suggestion suggest (const Response&, const Options&, const Bands& currentGains);
}
