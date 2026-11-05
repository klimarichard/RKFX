#include <juce_core/juce_core.h>

#include "MixMonitorEq.h"

#include <cmath>

using namespace monitoreq;

namespace
{
    int bandAt (double hz)
    {
        for (int b = 0; b < kBands; ++b)
            if (std::abs (MixMeasure::centres()[(size_t) b] - hz) < 1.0)
                return b;
        return -1;
    }

    /** A response with data everywhere, @p db per band. */
    Response responseOf (const Bands& db)
    {
        return fileResponse (db);
    }

    Bands flatGains() { return Bands {}; }
}

class MixMonitorEqTests : public juce::UnitTest
{
public:
    MixMonitorEqTests() : juce::UnitTest ("RKMix Monitor EQ maths") {}

    void runTest() override
    {
        beginTest ("Targets: Gentle tilt -1 dB per octave above 1 kHz; Vocal wedge's roll-off; the in-ear bass shelf");
        {
            const auto tilt = target (Target::gentleTilt);
            expectEquals (tilt[(size_t) bandAt (500.0)], 0.0f);
            expectWithinAbsoluteError (tilt[(size_t) bandAt (2000.0)], -1.0f, 0.01f);
            expectWithinAbsoluteError (tilt[(size_t) bandAt (16000.0)], -4.0f, 0.01f);

            const auto vocal = target (Target::vocalWedge);
            expectWithinAbsoluteError (vocal[(size_t) bandAt (1000.0)], 0.0f, 0.1f);
            expectLessThan (vocal[(size_t) bandAt (80.0)], -9.0f, "rolled off below 150 Hz");

            const auto room = target (Target::speakersInRoom, 6.0f);
            const auto diffuse = target (Target::diffuseField);
            expectWithinAbsoluteError (room[0] - diffuse[0], 6.0f, 0.01f, "the bass shelf at 20 Hz");
            expectWithinAbsoluteError (room[(size_t) bandAt (1000.0)] - diffuse[(size_t) bandAt (1000.0)], 0.0f, 0.01f, "the shelf is gone by 1 kHz");
            expectGreaterThan (diffuse[(size_t) bandAt (3150.0)], 10.0f, "the ear's rise around 3 kHz");

            expect (targetsFor (Type::speaker).front() == Target::gentleTilt);
            expect (targetsFor (Type::inEar).front() == Target::speakersInRoom);
        }

        beginTest ("Mic curves: 0 dB at 1 kHz, presence rise, down at the ends");
        {
            for (auto m : { Mic::sm58, Mic::sm57, Mic::v7 })
            {
                const auto& c = micCurve (m);
                expectEquals (c[(size_t) bandAt (1000.0)], 0.0f, micName (m));
                expectGreaterThan (c[(size_t) bandAt (5000.0)], 2.0f, micName (m));
                expectLessThan (c[(size_t) bandAt (20000.0)], -6.0f, micName (m));
                expectLessThan (c[0], -6.0f, micName (m));
            }
            for (auto v : micCurve (Mic::flat))
                expectEquals (v, 0.0f);
        }

        beginTest ("Response files: comments, a header and the sensitivity line skipped; commas, tabs; band means");
        {
            const auto file = std::string ("\"Sens Factor =-1.2dB, SERNO: 700-1234\"\n"
                                           "* a comment\n"
                                           "Freq(Hz)\tSPL(dB)\tPhase\n"
                                           "10\t-6.0\t0\n"
                                           "1000,2.0\n"
                                           "1000;4.0\n"
                                           "\n"
                                           "20000 -10.0 12\n");
            const auto bands = readResponseFile (file);
            expect (bands.has_value());
            if (bands)
            {
                expectWithinAbsoluteError ((*bands)[(size_t) bandAt (1000.0)], 3.0f, 1.0e-4f, "the mean of the points in the band");
                expectWithinAbsoluteError ((*bands)[(size_t) bandAt (20000.0)], -10.0f, 1.0e-4f);
                // 100 Hz: between 10 Hz (-6) and 1 kHz (3, the mean of the two 1 kHz points: interpolated from the nearest, 2 or 4)
                expectGreaterThan ((*bands)[(size_t) bandAt (100.0)], -6.0f);
                expectLessThan ((*bands)[(size_t) bandAt (100.0)], 4.0f);
            }
            expect (! readResponseFile ("100 0\n200 1\n").has_value(), "fewer than 3 points");
            expect (! readResponseFile ("garbage\nmore garbage\n").has_value());

            std::string dense;
            for (double hz = 10.0; hz < 24000.0; hz *= 1.01)
                dense += std::to_string (hz) + " " + std::to_string (20.0 * std::log10 (hz / 1000.0)) + "\n";
            const auto slope = readResponseFile (dense);
            expect (slope.has_value());
            if (slope)
                expectWithinAbsoluteError ((*slope)[(size_t) bandAt (2000.0)], 6.02f, 0.1f, "a dense file averages to the centre's value");
        }

        beginTest ("Measured response: power ratio per band, floor check, mic compensation capped at +6 dB");
        {
            Window floor, position;
            for (int b = 0; b < kBands; ++b)
            {
                floor.input[(size_t) b] = 1.0e-8;
                position.reference[(size_t) b] = 1.0e-2;
                position.input[(size_t) b] = 1.0e-2 * 0.25;            // -6 dB
            }
            position.input[3] = 5.0e-8;                                // only 7 dB over the floor
            Bands mic {};
            mic[(size_t) bandAt (5000.0)] = 4.0f;                     // the mic adds 4 dB there
            mic[(size_t) bandAt (16000.0)] = -9.0f;                   // and is 9 dB down there

            const auto r = measuredResponse (floor, { position }, mic, 48000.0);
            expectWithinAbsoluteError (r.db[(size_t) bandAt (1000.0)], -6.02f, 0.01f);
            expect (! r.hasData[3], "under 10 dB over the floor: no data");
            expectWithinAbsoluteError (r.db[(size_t) bandAt (5000.0)], -10.02f, 0.01f, "the mic's rise taken off");
            expectWithinAbsoluteError (r.db[(size_t) bandAt (16000.0)], -0.02f, 0.01f, "compensation capped at +6");
            expect (r.lowConfidence[(size_t) bandAt (16000.0)]);
            expect (! r.lowConfidence[(size_t) bandAt (5000.0)]);

            const auto at44 = measuredResponse (floor, { position }, mic, 44100.0);
            expect (! at44.hasData[30], "20 kHz isn't measured at 44.1 kHz");

            auto louder = position;
            for (auto& v : louder.input)
                v *= 4.0;                                              // 0 dB
            const auto two = measuredResponse (floor, { position, louder }, Bands {}, 48000.0);
            expectWithinAbsoluteError (two.db[(size_t) bandAt (1000.0)], (float) (10.0 * std::log10 ((0.25 + 1.0) / 2.0)), 0.01f,
                                       "positions averaged as power");
        }

        beginTest ("Window between two reads of the sums");
        {
            MixMeasure::Sums a, b;
            a.samples = 1000;
            b.samples = 49000;
            a.input[5] = 1.0;
            b.input[5] = 49.0;
            b.reference[5] = 96.0;
            const auto w = windowBetween (a, b, 48000.0);
            expectWithinAbsoluteError (w.seconds, 1.0, 1.0e-9);
            expectWithinAbsoluteError (w.input[5], 48.0 / 48000.0, 1.0e-12);
            expectWithinAbsoluteError (w.reference[5], 96.0 / 48000.0, 1.0e-12);
        }

        beginTest ("Suggest: on target, no correction; a level offset doesn't matter");
        {
            Bands db {};
            db.fill (-30.0f);
            Options o;
            o.target = Target::flat;
            const auto s = suggest (responseOf (db), o, flatGains());
            for (int b = 0; b < kBands; ++b)
                expectWithinAbsoluteError (s.correction[(size_t) b], 0.0f, 1.0e-5f);

            const auto tilt = target (Target::gentleTilt);
            Bands tilted {};
            for (int b = 0; b < kBands; ++b)
                tilted[(size_t) b] = tilt[(size_t) b] + 12.0f;
            o.target = Target::gentleTilt;
            const auto t = suggest (responseOf (tilted), o, flatGains());
            for (int b = 0; b < kBands; ++b)
                expectWithinAbsoluteError (t.correction[(size_t) b], 0.0f, 0.05f, "a wedge already on the tilt");
        }

        beginTest ("Suggest: a broad bump is cut (at the strength), a broad dip boosted only to +4 dB (Speaker)");
        {
            Options o;
            o.target = Target::flat;
            o.strength = 1.0f;
            Bands db {};
            for (int b = bandAt (2000.0); b <= bandAt (4000.0); ++b)
                db[(size_t) b] = 6.0f;                                 // 4 bands up 6 dB
            auto s = suggest (responseOf (db), o, flatGains());
            const auto mid = (size_t) bandAt (3150.0);
            expectLessThan (s.correction[mid], -4.0f, "cut");
            expectGreaterThan (s.correction[mid], -6.5f);
            o.strength = 0.5f;
            const auto half = suggest (responseOf (db), o, flatGains());
            expectWithinAbsoluteError (half.correction[mid], s.correction[mid] * 0.5f, 1.0e-4f, "strength scales it");

            o.strength = 1.0f;
            Bands dip {};
            for (int b = bandAt (400.0); b <= bandAt (1000.0); ++b)
                dip[(size_t) b] = -6.0f;                               // 5 bands, 6 dB down
            s = suggest (responseOf (dip), o, flatGains());
            expectWithinAbsoluteError (s.correction[(size_t) bandAt (630.0)], 4.0f, 1.0e-4f, "boost limited to +4");
            expect (! s.null[(size_t) bandAt (630.0)], "broad: not a null");
        }

        beginTest ("Suggest: a deep narrow dip is a null (never boosted); the wedge's range is found");
        {
            Options o;
            o.target = Target::flat;
            o.strength = 1.0f;
            Bands db {};
            db[(size_t) bandAt (500.0)] = -15.0f;                     // one band, a cancellation
            for (int b = 0; b <= bandAt (63.0); ++b)
                db[(size_t) b] = -20.0f;                               // the wedge doesn't go below 80 Hz
            db[(size_t) bandAt (20000.0)] = -25.0f;
            const auto s = suggest (responseOf (db), o, flatGains());
            expect (s.null[(size_t) bandAt (500.0)]);
            expectLessOrEqual (s.correction[(size_t) bandAt (500.0)], 0.0f);
            for (int b = 0; b <= bandAt (63.0); ++b)
            {
                expect (s.outOfRange[(size_t) b]);
                expectEquals (s.correction[(size_t) b], 0.0f);
            }
            expect (! s.outOfRange[(size_t) bandAt (80.0)]);
            expect (s.outOfRange[(size_t) bandAt (20000.0)]);
            expect (! s.outOfRange[(size_t) bandAt (16000.0)]);

            // In-ear: no range trimming.
            o.type = Type::inEar;
            o.target = Target::diffuseField;
            const auto ie = suggest (responseOf (db), o, flatGains());
            expect (! ie.outOfRange[0]);
        }

        beginTest ("Suggest: no data and low confidence; current gains added and clamped to +-12 dB");
        {
            Options o;
            o.target = Target::flat;
            o.strength = 1.0f;
            Bands db {};
            for (int b = bandAt (2000.0); b <= bandAt (5000.0); ++b)
                db[(size_t) b] = 6.0f;
            auto r = responseOf (db);
            r.hasData[(size_t) bandAt (2500.0)] = false;
            r.lowConfidence[(size_t) bandAt (4000.0)] = true;
            const auto plain = suggest (responseOf (db), o, flatGains());
            Bands current {};
            current.fill (-10.0f);
            const auto s = suggest (r, o, current);
            expectEquals (s.correction[(size_t) bandAt (2500.0)], 0.0f, "no data: no correction");
            expect (s.noData[(size_t) bandAt (2500.0)]);
            expectLessThan (std::abs (s.correction[(size_t) bandAt (4000.0)]), std::abs (plain.correction[(size_t) bandAt (4000.0)]) * 0.75f,
                            "low confidence: halved");
            expectEquals (s.gains[(size_t) bandAt (3150.0)], -12.0f, "clamped to the GraphEQ's range");
            expectWithinAbsoluteError (s.gains[(size_t) bandAt (500.0)], -10.0f, 0.01f, "the current gain kept where nothing changes");
        }

        beginTest ("Suggest, In-ear: above 8 kHz only the trend, within +-3 dB; limits -10 / +6");
        {
            Options o;
            o.type = Type::inEar;
            o.target = Target::diffuseField;
            o.strength = 1.0f;
            auto db = target (Target::diffuseField);
            db[(size_t) bandAt (12500.0)] += 10.0f;                   // a fit resonance
            const auto s = suggest (responseOf (db), o, flatGains());
            expectGreaterOrEqual (s.correction[(size_t) bandAt (12500.0)], -3.0f, "not chased");
            expectLessThan (s.correction[(size_t) bandAt (12500.0)], -0.5f, "but the trend follows");
            expectWithinAbsoluteError (s.correction[(size_t) bandAt (1000.0)], 0.0f, 0.05f);

            auto low = target (Target::diffuseField);
            for (int b = bandAt (63.0); b <= bandAt (160.0); ++b)
                low[(size_t) b] -= 9.0f;                               // a weak seal: no bass
            const auto t = suggest (responseOf (low), o, flatGains());
            expectWithinAbsoluteError (t.correction[(size_t) bandAt (100.0)], 6.0f, 1.0e-4f, "In-ear boosts up to +6");
        }

        beginTest ("Left against right: a level difference doesn't count, a shape difference does");
        {
            Bands left {}, right {};
            right.fill (5.0f);
            expectWithinAbsoluteError (maxDifferenceDb (responseOf (left), responseOf (right)), 0.0f, 1.0e-5f);
            right[(size_t) bandAt (8000.0)] += 4.0f;
            expectWithinAbsoluteError (maxDifferenceDb (responseOf (left), responseOf (right)), 4.0f, 1.0e-4f);
        }
    }
};

static MixMonitorEqTests mixMonitorEqTests;
