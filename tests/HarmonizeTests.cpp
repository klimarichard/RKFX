#include <juce_audio_basics/juce_audio_basics.h>

#include "HarmonizeEngine.h"

#include <vector>

/** RKHarmonize: diatonic harmonies in every kind of scale, notes outside
    the scale, shared pitches never cutting each other off, unknown
    note-offs and other events passing through, and releasing everything. */
class HarmonizeTests : public juce::UnitTest
{
public:
    HarmonizeTests() : juce::UnitTest ("RKHarmonize engine") {}

    void runTest() override
    {
        using Scale = HarmonizeEngine::Scale;
        using Interval = HarmonizeEngine::Interval;
        const auto harmony = [] (int note, int key, Scale s, Interval i) { return HarmonizeEngine::harmonyNote (note, key, s, i); };

        beginTest ("Diatonic: the key decides major or minor");
        {
            expectEquals (harmony (64, 0, Scale::major, Interval::thirdAbove), 67, "E -> G in C");
            expectEquals (harmony (62, 0, Scale::major, Interval::thirdAbove), 65, "D -> F (a minor third)");
            expectEquals (harmony (60, 0, Scale::major, Interval::thirdAbove), 64, "C -> E (a major third)");
            expectEquals (harmony (71, 0, Scale::major, Interval::thirdAbove), 74, "B -> D, into the next octave");
            expectEquals (harmony (60, 0, Scale::major, Interval::thirdBelow), 57, "C -> A below");
            expectEquals (harmony (60, 0, Scale::major, Interval::sixthBelow), 52, "C -> E below");
            expectEquals (harmony (60, 0, Scale::major, Interval::fifthAbove), 67);
            expectEquals (harmony (71, 0, Scale::major, Interval::fifthAbove), 77, "B -> F (diminished fifth, in the key)");
            expectEquals (harmony (60, 9, Scale::naturalMinor, Interval::thirdAbove), 64, "C -> E in A minor");
            expectEquals (harmony (64, 9, Scale::harmonicMinor, Interval::thirdAbove), 68, "E -> G# in A harmonic minor");
            expectEquals (harmony (61, 0, Scale::major, Interval::thirdAbove), 65, "C# (outside) keeps its distance: F");
        }

        beginTest ("Octaves, chromatic, pentatonic, range");
        {
            expectEquals (harmony (60, 0, Scale::major, Interval::octaveAbove), 72);
            expectEquals (harmony (60, 0, Scale::major, Interval::octaveBelow), 48);
            expectEquals (harmony (60, 0, Scale::chromatic, Interval::thirdAbove), 64);
            expectEquals (harmony (62, 0, Scale::chromatic, Interval::thirdAbove), 66, "parallel major thirds");
            expectEquals (harmony (67, 4, Scale::minorPentatonic, Interval::thirdAbove), 71, "G -> B in E minor pentatonic");
            expectEquals (harmony (60, 0, Scale::major, Interval::off), -1);
            expectEquals (harmony (125, 0, Scale::major, Interval::octaveAbove), -1, "beyond 127");
        }

        beginTest ("Notes in and out, shared pitches counted");
        {
            HarmonizeEngine e;
            e.prepare();
            HarmonizeEngine::Settings s;
            e.setSettings (s);   // C major, a 3rd above, the original kept

            auto out = run (e, { on (60, 100) });
            expect (has (out, true, 60, 100) && has (out, true, 64, 80), "C and E (at 80%)");

            out = run (e, { on (64, 90) });                      // E: already sounding as C's harmony
            expect (! has (out, true, 64, -1), "E isn't started twice");
            expect (has (out, true, 67, -1), "its harmony G is");

            out = run (e, { off (60) });
            expect (has (out, false, 60, -1) && ! has (out, false, 64, -1), "C ends; E still held by the played E");

            out = run (e, { off (64) });
            expect (has (out, false, 64, -1) && has (out, false, 67, -1));
        }

        beginTest ("Unknown note-offs and other events pass; release ends everything");
        {
            HarmonizeEngine e;
            e.prepare();
            e.setSettings ({});
            auto out = run (e, { off (50), juce::MidiMessage::controllerEvent (1, 64, 127) });
            expect (has (out, false, 50, -1), "a note-off it never saw");
            expect (out.getNumEvents() == 2, "and the sustain pedal");

            run (e, { on (60, 100) });
            juce::MidiBuffer released;
            e.releaseAll (released);
            expect (has (released, false, 60, -1) && has (released, false, 64, -1));
            out = run (e, { off (60) });
            expect (has (out, false, 60, -1), "after a release, the key's note-off just passes");
        }
    }

private:
    static juce::MidiMessage on (int note, int velocity) { return juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity); }
    static juce::MidiMessage off (int note) { return juce::MidiMessage::noteOff (1, note); }

    static juce::MidiBuffer run (HarmonizeEngine& e, std::initializer_list<juce::MidiMessage> events)
    {
        juce::MidiBuffer midi;
        int position = 0;
        for (const auto& m : events)
            midi.addEvent (m, position++);
        e.process (midi);
        return midi;
    }

    /** True if @p midi has a note-on (or note-off) for @p note (with @p velocity, if not -1). */
    static bool has (const juce::MidiBuffer& midi, bool noteOn, int note, int velocity)
    {
        for (const auto metadata : midi)
        {
            const auto m = metadata.getMessage();
            if (m.getNoteNumber() == note && (noteOn ? m.isNoteOn() : m.isNoteOff())
                && (velocity < 0 || m.getVelocity() == velocity))
                return true;
        }
        return false;
    }
};

static HarmonizeTests harmonizeTests;
