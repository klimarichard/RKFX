#include "MixMonitorEq.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace monitoreq
{
namespace
{
    const auto& centres = MixMeasure::centres;

    // Bands 250 Hz .. 4 kHz: where responses and targets are normalised to 0 dB.
    constexpr int kNormaliseFirst = 11, kNormaliseLast = 23;

    struct Limits
    {
        float cut, boost;
    };
    Limits limitsFor (Type t) { return t == Type::speaker ? Limits { -12.0f, 4.0f } : Limits { -10.0f, 6.0f }; }

    constexpr float kOutOfRangeDb = -12.0f;      // Speaker: below the normalised level by this at the ends
    constexpr float kNullDepthDb = 8.0f;         // a dip this deep ...
    constexpr int kNullMaxBands = 2;             // ... and this narrow is a null, never boosted
    constexpr double kInEarTrendFromHz = 8500.0; // In-ear: above 8 kHz only the trend
    constexpr int kInEarTrendReach = 3;
    constexpr float kInEarTrendLimitDb = 3.0f;

    float tiltDb (double hz) { return hz > 1000.0 ? (float) -std::log2 (hz / 1000.0) : 0.0f; }

    float highPassDb (double hz, double cornerHz)
    {
        const auto x4 = std::pow (hz / cornerHz, 4.0);
        return (float) (10.0 * std::log10 (x4 / (1.0 + x4)));
    }

    /** ISO 11904-2 diffuse field at the eardrum, 1/3-octave values (approximate, 0 dB in the bass). */
    constexpr Bands kDiffuseField { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.2f, 0.3f,
                                    0.5f, 0.6f, 0.8f, 1.1f, 1.6f, 2.1f, 2.6f, 3.6f, 5.6f, 8.6f,
                                    11.4f, 12.4f, 11.3f, 9.4f, 7.0f, 4.6f, 3.1f, 0.8f, -1.0f, -3.0f };

    // Mic curves: typical on-axis responses read from the manufacturers'
    // published charts, 1/3-octave values, 0 dB at 1 kHz. Approximate
    // (units vary by about +-2 dB); reviewed before shipping.
    //                                20     25     31.5   40     50     63     80     100    125    160    200
    //                                250    315    400    500    630    800    1k     1.25k  1.6k   2k
    //                                2.5k   3.15k  4k     5k     6.3k   8k     10k    12.5k  16k    20k
    constexpr Bands kSm58 { -18.0f, -15.0f, -12.0f, -10.0f, -8.0f, -6.0f, -4.5f, -3.0f, -2.0f, -1.0f, -0.5f,
                            0.0f,   0.0f,   0.0f,   0.0f,   0.0f,  0.0f,  0.0f,  0.0f,  0.5f,  1.0f,
                            2.0f,   3.5f,   4.5f,   5.0f,   4.0f,  2.0f,  2.5f,  0.0f, -5.0f, -12.0f };
    constexpr Bands kSm57 { -20.0f, -17.0f, -14.0f, -12.0f, -10.0f, -8.0f, -6.0f, -4.5f, -3.0f, -2.0f, -1.0f,
                            -0.5f,  0.0f,   0.0f,   0.0f,   0.0f,   0.0f,  0.0f,  0.0f,  0.0f,  0.5f,
                            1.5f,   2.5f,   4.0f,   5.5f,   5.0f,   3.0f,  3.0f,  0.5f, -4.0f, -12.0f };
    constexpr Bands kV7   { -15.0f, -12.0f, -9.0f,  -7.0f,  -5.0f,  -3.5f, -2.5f, -1.5f, -1.0f, -0.5f, 0.0f,
                            0.0f,   0.0f,   0.0f,   0.0f,   0.0f,   0.0f,  0.0f,  0.0f,  0.5f,  1.0f,
                            2.0f,   3.0f,   3.5f,   3.5f,   3.0f,   2.5f,  2.0f,  1.0f, -3.0f, -10.0f };
    constexpr Bands kFlat {};

    /** The median of the bands in use within first..last (robust to a bump or a dip inside). */
    float medianOver (const Bands& db, const Flags& use, int first, int last, bool& found)
    {
        std::array<float, kBands> values {};
        int n = 0;
        for (int b = first; b <= last; ++b)
            if (use[(size_t) b])
                values[(size_t) n++] = db[(size_t) b];
        found = n > 0;
        if (n == 0)
            return 0.0f;
        std::sort (values.begin(), values.begin() + n);
        return n % 2 == 1 ? values[(size_t) (n / 2)] : 0.5f * (values[(size_t) (n / 2 - 1)] + values[(size_t) (n / 2)]);
    }

    bool parseNumber (const std::string& token, double& value)
    {
        if (token.empty())
            return false;
        char* end = nullptr;
        value = std::strtod (token.c_str(), &end);
        return end != token.c_str() && *end == '\0' && std::isfinite (value);
    }
}

//==============================================================================
std::vector<Target> targetsFor (Type t)
{
    if (t == Type::speaker)
        return { Target::gentleTilt, Target::flat, Target::vocalWedge };
    return { Target::speakersInRoom, Target::diffuseField };
}

const char* targetName (Target t)
{
    switch (t)
    {
        case Target::flat:           return "Flat";
        case Target::gentleTilt:     return "Gentle tilt";
        case Target::vocalWedge:     return "Vocal wedge";
        case Target::diffuseField:   return "Diffuse field";
        case Target::speakersInRoom: return "Speakers in a room";
    }
    return "";
}

const char* micName (Mic m)
{
    switch (m)
    {
        case Mic::flat: return "Flat (measurement mic)";
        case Mic::sm58: return "Shure SM58";
        case Mic::sm57: return "Shure SM57";
        case Mic::v7:   return "sE Electronics V7";
    }
    return "";
}

const Bands& micCurve (Mic m)
{
    switch (m)
    {
        case Mic::sm58: return kSm58;
        case Mic::sm57: return kSm57;
        case Mic::v7:   return kV7;
        case Mic::flat: break;
    }
    return kFlat;
}

Bands target (Target t, float bassDb)
{
    Bands out {};
    for (int b = 0; b < kBands; ++b)
    {
        const auto hz = centres()[(size_t) b];
        auto& v = out[(size_t) b];
        switch (t)
        {
            case Target::flat:           v = 0.0f; break;
            case Target::gentleTilt:     v = tiltDb (hz); break;
            case Target::vocalWedge:     v = tiltDb (hz) + highPassDb (hz, 150.0); break;
            case Target::diffuseField:   v = kDiffuseField[(size_t) b]; break;
            case Target::speakersInRoom:
                v = kDiffuseField[(size_t) b] + tiltDb (hz) + bassDb / (float) (1.0 + std::pow (hz / 150.0, 4.0));
                break;
        }
    }
    return out;
}

//==============================================================================
std::optional<Bands> readResponseFile (const std::string& text)
{
    std::vector<std::pair<double, double>> points;   // log2 Hz, dB
    std::istringstream lines (text);
    std::string line;
    while (std::getline (lines, line))
    {
        for (auto& c : line)
            if (c == ',' || c == ';' || c == '\t' || c == '\r')
                c = ' ';
        std::istringstream fields (line);
        std::string first, second;
        if (! (fields >> first))
            continue;
        const auto lead = (unsigned char) first[0];
        if (lead == '*' || lead == '"' || std::isalpha (lead) != 0)
            continue;
        double hz = 0.0, db = 0.0;
        if (! (fields >> second) || ! parseNumber (first, hz) || ! parseNumber (second, db) || hz <= 0.0)
            continue;
        points.emplace_back (std::log2 (hz), db);
    }
    if (points.size() < 3)
        return std::nullopt;
    std::sort (points.begin(), points.end());

    const auto interpolate = [&] (double x)
    {
        if (x <= points.front().first)
            return points.front().second;
        if (x >= points.back().first)
            return points.back().second;
        const auto upper = std::lower_bound (points.begin(), points.end(), std::make_pair (x, -1.0e300));
        const auto lower = upper - 1;
        const auto span = upper->first - lower->first;
        const auto t = span > 0.0 ? (x - lower->first) / span : 0.0;
        return lower->second + t * (upper->second - lower->second);
    };

    Bands out {};
    for (int b = 0; b < kBands; ++b)
    {
        const auto x = std::log2 (centres()[(size_t) b]);
        double sum = 0.0;
        int n = 0;
        for (const auto& [px, db] : points)
            if (px >= x - 1.0 / 6.0 && px < x + 1.0 / 6.0)
            {
                sum += db;
                ++n;
            }
        out[(size_t) b] = (float) (n >= 2 ? sum / n : interpolate (x));
    }
    return out;
}

//==============================================================================
Window windowBetween (const MixMeasure::Sums& from, const MixMeasure::Sums& to, double sampleRate)
{
    Window w;
    const auto n = (double) (to.samples - from.samples);
    if (n <= 0.0)
        return w;
    for (int b = 0; b < kBands; ++b)
    {
        w.input[(size_t) b] = std::max (0.0, to.input[(size_t) b] - from.input[(size_t) b]) / n;
        w.reference[(size_t) b] = std::max (0.0, to.reference[(size_t) b] - from.reference[(size_t) b]) / n;
    }
    w.seconds = n / sampleRate;
    return w;
}

Response measuredResponse (const Window& floor, const std::vector<Window>& positions, const Bands& micDb, double sampleRate)
{
    Response r;
    const auto overFloor = std::pow (10.0, kMinSignalOverFloorDb / 10.0);
    for (int b = 0; b < kBands; ++b)
    {
        if (! MixMeasure::isMeasured (b, sampleRate))
            continue;
        double sum = 0.0;
        int n = 0;
        for (const auto& p : positions)
        {
            const auto in = p.input[(size_t) b], ref = p.reference[(size_t) b], fl = floor.input[(size_t) b];
            if (ref <= 0.0 || in < fl * overFloor || in <= 0.0)
                continue;
            sum += (in - fl) / ref;
            ++n;
        }
        if (n == 0 || sum <= 0.0)
            continue;
        auto compensation = -micDb[(size_t) b];
        if (compensation > kMaxMicCompensationDb)
        {
            compensation = kMaxMicCompensationDb;
            r.lowConfidence[(size_t) b] = true;
        }
        r.db[(size_t) b] = (float) (10.0 * std::log10 (sum / n)) + compensation;
        r.hasData[(size_t) b] = true;
    }
    return r;
}

Response fileResponse (const Bands& db)
{
    Response r;
    r.db = db;
    r.hasData.fill (true);
    return r;
}

float maxDifferenceDb (const Response& a, const Response& b)
{
    // Each normalised first, so a level difference between the sides doesn't count.
    const auto normalised = [] (const Response& r)
    {
        bool found = false;
        const auto mean = medianOver (r.db, r.hasData, kNormaliseFirst, kNormaliseLast, found);
        Bands out = r.db;
        for (auto& v : out)
            v -= mean;
        return out;
    };
    const auto na = normalised (a), nb = normalised (b);
    float worst = 0.0f;
    for (int i = 0; i < kBands; ++i)
        if (a.hasData[(size_t) i] && b.hasData[(size_t) i])
            worst = std::max (worst, std::abs (na[(size_t) i] - nb[(size_t) i]));
    return worst;
}

//==============================================================================
Suggestion suggest (const Response& response, const Options& options, const Bands& currentGains)
{
    Suggestion s;
    const auto limits = limitsFor (options.type);

    for (int b = 0; b < kBands; ++b)
    {
        s.noData[(size_t) b] = ! response.hasData[(size_t) b];
        s.lowConfidence[(size_t) b] = response.lowConfidence[(size_t) b];
    }

    // Normalise the response and the target to 0 dB over 250 Hz .. 4 kHz.
    bool found = false;
    const auto mean = medianOver (response.db, response.hasData, kNormaliseFirst, kNormaliseLast, found);
    s.gains = currentGains;
    if (! found)
    {
        s.noData.fill (true);
        return s;
    }
    const auto t = target (options.target, options.bassDb);
    Flags all {};
    all.fill (true);
    bool any = false;
    const auto targetMean = medianOver (t, all, kNormaliseFirst, kNormaliseLast, any);
    for (int b = 0; b < kBands; ++b)
    {
        s.normalised[(size_t) b] = response.hasData[(size_t) b] ? response.db[(size_t) b] - mean : 0.0f;
        s.targetDb[(size_t) b] = t[(size_t) b] - targetMean;
    }

    // Speaker: the ends the wedge doesn't reach.
    if (options.type == Type::speaker)
    {
        for (int b = 0; b < kBands; ++b)
        {
            if (! response.hasData[(size_t) b])
                continue;
            if (s.normalised[(size_t) b] >= kOutOfRangeDb)
                break;
            s.outOfRange[(size_t) b] = true;
        }
        for (int b = kBands - 1; b >= 0; --b)
        {
            if (! response.hasData[(size_t) b])
                continue;
            if (s.normalised[(size_t) b] >= kOutOfRangeDb)
                break;
            s.outOfRange[(size_t) b] = true;
        }
    }

    Flags usable {};
    Bands raw {};
    for (int b = 0; b < kBands; ++b)
    {
        usable[(size_t) b] = response.hasData[(size_t) b] && ! s.outOfRange[(size_t) b];
        raw[(size_t) b] = usable[(size_t) b] ? s.targetDb[(size_t) b] - s.normalised[(size_t) b] : 0.0f;
    }

    // Nulls: deep, narrow dips (cancellations) are never boosted.
    for (int b = 0; b < kBands;)
    {
        if (! usable[(size_t) b] || raw[(size_t) b] <= kNullDepthDb)
        {
            ++b;
            continue;
        }
        auto end = b;
        while (end < kBands && usable[(size_t) end] && raw[(size_t) end] > kNullDepthDb)
            ++end;
        if (end - b <= kNullMaxBands)
            for (int n = b; n < end; ++n)
            {
                s.null[(size_t) n] = true;
                raw[(size_t) n] = 0.0f;
            }
        b = end;
    }

    // 1-2-1 smoothing over usable neighbours (a missing neighbour counts as the band itself).
    Bands smooth {};
    for (int b = 0; b < kBands; ++b)
    {
        if (! usable[(size_t) b])
            continue;
        const auto at = [&] (int n) { return n >= 0 && n < kBands && usable[(size_t) n] ? raw[(size_t) n] : raw[(size_t) b]; };
        smooth[(size_t) b] = 0.25f * (at (b - 1) + 2.0f * raw[(size_t) b] + at (b + 1));
    }

    // In-ear: above 8 kHz only the broad trend, gently.
    if (options.type == Type::inEar)
        for (int b = 0; b < kBands; ++b)
        {
            if (! usable[(size_t) b] || centres()[(size_t) b] < kInEarTrendFromHz)
                continue;
            double sum = 0.0;
            int n = 0;
            for (int k = b - kInEarTrendReach; k <= b + kInEarTrendReach; ++k)
                if (k >= 0 && k < kBands && usable[(size_t) k])
                {
                    sum += raw[(size_t) k];
                    ++n;
                }
            smooth[(size_t) b] = std::clamp ((float) (sum / n), -kInEarTrendLimitDb, kInEarTrendLimitDb);
        }

    const auto strength = std::clamp (options.strength, 0.0f, 1.0f);
    for (int b = 0; b < kBands; ++b)
    {
        auto c = usable[(size_t) b] ? smooth[(size_t) b] : 0.0f;
        if (s.null[(size_t) b])
            c = std::min (c, 0.0f);
        if (s.lowConfidence[(size_t) b])
            c *= 0.5f;
        c = std::clamp (c, limits.cut, limits.boost) * strength;
        s.correction[(size_t) b] = c;
        s.gains[(size_t) b] = std::clamp (currentGains[(size_t) b] + c, -kGraphEqRangeDb, kGraphEqRangeDb);
    }
    return s;
}
}
