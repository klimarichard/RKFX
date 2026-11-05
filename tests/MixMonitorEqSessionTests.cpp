#include <juce_audio_processors/juce_audio_processors.h>

#include "../plugins/RKMix/PluginProcessor.h"
#include "../plugins/RKGraphEQ/GraphEQLayout.h"

#include <cmath>

/** Monitor EQ through the whole processor: Aux 1 on Out 3/4, a "wedge and
    room" that is a +6 dB bell at 1 kHz, heard on input 7 one block later.
    Built into RKBoardTests, which links the processors. */
class MixMonitorEqSessionTests : public juce::UnitTest
{
public:
    MixMonitorEqSessionTests() : juce::UnitTest ("RKMix Monitor EQ session") {}

    static constexpr int kBlock = 128, kMic = 6;
    static constexpr double kRate = 48000.0;

    static void setParam (juce::RangedAudioParameter& p, float realValue) { p.setValueNotifyingHost (p.convertTo0to1 (realValue)); }

    /** The acoustic path: a bell (RBJ peaking) and a gain, from Out 3 to the mic. */
    struct Room
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float gain = 0.3f;
        std::vector<float> last = std::vector<float> ((size_t) kBlock, 0.0f);

        void bell (double hz, double db, double q)
        {
            const auto A = std::pow (10.0, db / 40.0), w = 2.0 * juce::MathConstants<double>::pi * hz / kRate;
            const auto alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha / A;
            b0 = (1.0 + alpha * A) / a0;
            b1 = -2.0 * std::cos (w) / a0;
            b2 = (1.0 - alpha * A) / a0;
            a1 = b1;
            a2 = (1.0 - alpha / A) / a0;
        }
        float run (float x)
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y * gain;
        }
    };

    struct Rig
    {
        RKMixProcessor p;
        Room room;
        juce::AudioBuffer<float> b { 32, kBlock };
        juce::MidiBuffer midi;

        Rig()
        {
            p.prepareToPlay (kRate, kBlock);
            auto& s = p.getSettings();
            setParam (s.param (mix::busId (0, "type")), (float) mix::BusType::aux);
            setParam (s.param (mix::busId (0, "format")), 1.0f);
            setParam (s.param (mix::busId (0, "out")), (float) mix::pairTarget (2));
            room.bell (1000.0, 6.0, 3.0);   // about 1/2 octave wide: the 250 Hz .. 4 kHz median stays put
            auto o = p.getMonitorEq().getOptions();
            o.input = kMic;
            o.speakerTarget = monitoreq::Target::flat;
            o.speakerPositions = 1;
            o.strength = 1.0f;
            p.getMonitorEq().setOptions (o);
            audio (0.1);
        }

        /** @p seconds of audio, the mic hearing Out 3 through the room. */
        void audio (double seconds)
        {
            for (int n = 0; n < (int) (seconds * kRate / kBlock); ++n)
            {
                b.clear();
                for (int i = 0; i < kBlock; ++i)
                    b.setSample (kMic, i, room.last[(size_t) i]);
                p.processBlock (b, midi);
                for (int i = 0; i < kBlock; ++i)
                    room.last[(size_t) i] = room.run (b.getSample (2, i));
            }
        }

        /** Runs audio with a timer tick every 0.1 s until @p done or @p seconds pass. */
        bool runUntil (std::function<bool()> done, double seconds = 20.0)
        {
            for (double t = 0.0; t < seconds; t += 0.1)
            {
                audio (0.1);
                p.getMonitorEq().tick();
                if (done())
                    return true;
            }
            return false;
        }

        int band (double hz) const
        {
            for (int b = 0; b < monitoreq::kBands; ++b)
                if (std::abs (MixMeasure::centres()[(size_t) b] - hz) < 1.0)
                    return b;
            return -1;
        }
    };

    void runTest() override
    {
        beginTest ("A run: floor, a position, done; the bell found and cut; Apply inserts a 31-band GraphEQ; Undo removes it");
        {
            Rig rig;
            auto& m = rig.p.getMonitorEq();
            setParam (rig.p.getSettings().param (mix::busId (1, "type")), (float) mix::BusType::group);
            expect (! m.start (1), "bus 2 isn't an Aux bus");
            expect (m.start (0));
            expect (m.getStep() == MixMonitorEqSession::Step::floor);
            expect (rig.runUntil ([&] { return m.getStep() == MixMonitorEqSession::Step::done; }), "done within 20 s");
            expect (m.hasResult());
            rig.audio (0.2);
            expect (! rig.p.getMeasure().isBusy(), "the bus is handed back");

            const auto s = m.getSuggestion();   // a copy: the second run replaces it
            const auto at1k = (size_t) rig.band (1000.0);
            expectGreaterThan (s.normalised[at1k], 4.0f, "the bell measured (its 1/3-octave average, under its +6 dB peak)");
            expectLessThan (s.normalised[at1k], 6.2f);
            expectLessThan (s.correction[at1k], -3.0f, "and cut");
            expectWithinAbsoluteError (s.correction[(size_t) rig.band (8000.0)], 0.0f, 0.5f, "flat elsewhere");

            expect (m.canApply(), m.applyProblem());
            m.apply();
            expectEquals (m.findGraphEq (0), 0, "a GraphEQ in insert 1");
            auto* eq = rig.p.getRack().get ({ InsertRack::Strip::bus, 0, 0 });
            expect (eq != nullptr);
            if (eq != nullptr)
            {
                expectEquals (rkfx::ProcessorBase::valueOf (eq->findParam ("bands")), 2.0f, "31 bands");
                expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (eq->findParam (GraphEQParams::bandId (2, (int) at1k))),
                                           s.gains[at1k], 0.05f);
            }

            // Measure again through it: much closer to flat.
            expect (m.start (0));
            expect (rig.runUntil ([&] { return m.getStep() == MixMonitorEqSession::Step::done; }));
            expectLessThan (std::abs (m.getSuggestion().normalised[at1k]), 2.0f, "the second pass: the bell mostly gone");
            expectWithinAbsoluteError (m.getMeasuredGains()[at1k], s.gains[at1k], 0.05f, "measured through the applied gains");

            // Undo (of the first Apply): nothing applied since, so it's that one.
            expect (m.canUndo());
            m.undo();
            expectEquals (m.findGraphEq (0), -1, "the inserted GraphEQ removed");
        }

        beginTest ("Apply into an existing GraphEQ (10 bands, read at the 31 centres); Undo puts it back");
        {
            Rig rig;
            auto& m = rig.p.getMonitorEq();
            auto* eq = rig.p.getRack().insert ({ InsertRack::Strip::bus, 0, 2 }, "RKGraphEQ", 2);
            expect (eq != nullptr);
            setParam (*eq->findParam (GraphEQParams::bandId (0, 5)), -4.0f);        // 10-band 1 kHz
            const auto before = m.graphEqGains (0, 2);
            expectWithinAbsoluteError (before[(size_t) rig.band (1000.0)], -4.0f, 0.3f, "the 10-band curve at the 31 centres");

            expect (m.start (0));
            expect (rig.runUntil ([&] { return m.getStep() == MixMonitorEqSession::Step::done; }));
            m.apply();
            expectEquals (m.findGraphEq (0), 2, "the existing one used");
            expectEquals (rkfx::ProcessorBase::valueOf (eq->findParam ("bands")), 2.0f);
            m.undo();
            expectEquals (rkfx::ProcessorBase::valueOf (eq->findParam ("bands")), 0.0f, "back to 10 bands");
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (eq->findParam (GraphEQParams::bandId (0, 5))), -4.0f, 1.0e-4f);
        }

        beginTest ("Full slots: Apply says why");
        {
            Rig rig;
            auto& m = rig.p.getMonitorEq();
            for (int s = 0; s < 4; ++s)
                rig.p.getRack().insert ({ InsertRack::Strip::bus, 0, s }, "RKUtility", 2);
            expect (m.start (0));
            expect (rig.runUntil ([&] { return m.getStep() == MixMonitorEqSession::Step::done; }));
            expect (! m.canApply());
            expect (m.applyProblem().contains ("slots"));
        }

        beginTest ("Positions: waiting between them, Next; Finish early; a scene recall stops a run");
        {
            Rig rig;
            auto& m = rig.p.getMonitorEq();
            auto o = m.getOptions();
            o.speakerPositions = 3;
            m.setOptions (o);
            expect (m.start (0));
            expect (rig.runUntil ([&] { return m.getStep() == MixMonitorEqSession::Step::waiting; }));
            expectEquals (m.positionsMeasured(), 1);
            expect (m.describeStep().contains ("Move the mic"));
            rig.audio (0.3);
            expect (rig.p.getMeasure().isBusy(), "the bus stays taken over between positions");
            m.next();
            expect (rig.runUntil ([&] { return m.getStep() == MixMonitorEqSession::Step::waiting; }));
            expectEquals (m.positionsMeasured(), 2);
            m.finish();
            expect (m.getStep() == MixMonitorEqSession::Step::done, "Finish with 2 of 3");

            expect (m.start (0));
            rig.runUntil ([] { return false; }, 1.0);
            rig.p.recallScene (3);
            m.tick();
            expect (m.getStep() == MixMonitorEqSession::Step::idle, "stopped (a new run had cleared the old result)");
            expect (m.getMessage().contains ("scene"));
            rig.audio (0.2);
            expect (! rig.p.getMeasure().isBusy(), "the bus handed back");
        }

        beginTest ("In-ear, each ear: left then right; the noise only on that side; a measurement file instead");
        {
            Rig rig;
            auto& m = rig.p.getMonitorEq();
            auto o = m.getOptions();
            o.type = monitoreq::Type::inEar;
            o.sides = MixMonitorEqSession::Sides::eachEar;
            o.inEarPositions = 2;
            o.inEarLevelDb = -10.0f;
            m.setOptions (o);
            expectEquals (m.getOptions().inEarLevelDb, -20.0f, "In-ear never louder than -20 dBFS");
            expect (m.positionSides (0) == MixMeasure::Sides::left);
            expect (m.positionSides (1) == MixMeasure::Sides::right);
            expect (m.start (0));
            expect (rig.runUntil ([&] { return m.getStep() == MixMonitorEqSession::Step::measuring; }));
            rig.audio (0.2);
            expectGreaterThan (rig.b.getMagnitude (2, 0, kBlock), 0.0f, "left (Out 3) carries the noise");
            expectEquals (rig.b.getMagnitude (3, 0, kBlock), 0.0f, "right (Out 4) silent");
            expect (rig.runUntil ([&] { return m.getStep() == MixMonitorEqSession::Step::waiting; }));
            expect (m.describeStep().contains ("right earpiece"));
            m.next();
            expect (rig.runUntil ([&] { return m.getStep() == MixMonitorEqSession::Step::done; }));
            expectLessThan (m.getEarDifferenceDb(), 3.0f, "the mic only hears the left output: the right ear has no data, so nothing to compare");

            // A file instead of measuring.
            const auto file = juce::File::createTempFile (".txt");
            file.replaceWithText ("20 0\n1000 0\n3000 8\n20000 0\n");
            expect (m.loadMeasurementFile (file));
            file.deleteFile();
            expect (m.getOptions().useFile);
            expect (m.start (0));
            expect (m.getStep() == MixMonitorEqSession::Step::done, "straight to done");
            rig.audio (0.2);
            expect (! rig.p.getMeasure().isBusy(), "no noise played");
        }

        beginTest ("Options are saved with the mix");
        {
            Rig rig;
            auto& m = rig.p.getMonitorEq();
            auto o = m.getOptions();
            o.type = monitoreq::Type::inEar;
            o.mic = monitoreq::Mic::sm58;
            o.strength = 0.5f;
            o.bassDb = 6.0f;
            m.setOptions (o);
            juce::MemoryBlock state;
            rig.p.getStateInformation (state);

            RKMixProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            const auto& r = q.getMonitorEq().getOptions();
            expect (r.type == monitoreq::Type::inEar);
            expect (r.mic == monitoreq::Mic::sm58);
            expectEquals (r.strength, 0.5f);
            expectEquals (r.bassDb, 6.0f);
            expectEquals (r.input, kMic);
        }
    }
};

static MixMonitorEqSessionTests mixMonitorEqSessionTests;
