#include <juce_audio_basics/juce_audio_basics.h>

#include "ArpEngine.h"

#include <vector>

/** RKArpeggiate: each pattern's order, steps on the exact sample at the
    tempo, gate, octaves, chord, stopping when the keys are up, latch, swing,
    locking to the host's beat, fixed velocity, and passing other events. */
class ArpTests : public juce::UnitTest
{
public:
    ArpTests() : juce::UnitTest ("RKArpeggiate engine") {}

    void runTest() override
    {
        using Pattern = ArpEngine::Pattern;
        constexpr int step = 6000;   // a 1/16 at 120 BPM, 48 kHz

        beginTest ("Up, on the exact sample, with the gate");
        {
            ArpEngine::Settings s;
            s.gate = 0.5f;
            const auto r = play (s, { 60, 64, 67 }, 4 * step);
            expect (notesOf (r.ons) == std::vector<int> { 60, 64, 67, 60 });
            expect (timesOf (r.ons) == std::vector<int> { 0, step, 2 * step, 3 * step });
            expectEquals (r.offs.front().time, step / 2, "half a step long");
        }

        beginTest ("Down, up and down, as played, octaves, chord");
        {
            ArpEngine::Settings s;
            s.pattern = Pattern::down;
            expect (notesOf (play (s, { 60, 64, 67 }, 3 * step).ons) == std::vector<int> { 67, 64, 60 });

            s.pattern = Pattern::upDown;
            expect (notesOf (play (s, { 60, 64, 67 }, 6 * step).ons) == std::vector<int> { 60, 64, 67, 64, 60, 64 });

            s.pattern = Pattern::played;
            expect (notesOf (play (s, { 67, 60, 64 }, 3 * step).ons) == std::vector<int> { 67, 60, 64 });

            s.pattern = Pattern::up;
            s.octaves = 2;
            expect (notesOf (play (s, { 60, 64, 67 }, 6 * step).ons) == std::vector<int> { 60, 64, 67, 72, 76, 79 });

            s.pattern = Pattern::chord;
            s.octaves = 1;
            const auto chord = play (s, { 60, 64, 67 }, 2 * step);
            expectEquals ((int) chord.ons.size(), 6, "all three, twice");
            expect (timesOf (chord.ons) == std::vector<int> { 0, 0, 0, step, step, step });
        }

        beginTest ("Keys up: the arpeggio ends, every note ended");
        {
            ArpEngine::Settings s;
            const auto r = play (s, { 60, 64 }, 6 * step, 2 * step + 10);
            expect (timesOf (r.ons) == std::vector<int> { 0, step, 2 * step }, "nothing after the keys are up");
            expectEquals (r.ons.size(), r.offs.size(), "every note-on has its note-off");
        }

        beginTest ("Latch keeps it going; a new chord after all keys were up replaces it");
        {
            ArpEngine::Settings s;
            s.latch = true;
            const auto r = play (s, { 60, 64 }, 6 * step, 100);
            expectEquals ((int) r.ons.size(), 6, "plays on after the keys are up");

            ArpEngine e;
            e.prepare (48000.0);
            e.setSettings (s);
            Collected c;
            block (e, c, { on (60), on (64) }, 0, 100);
            block (e, c, { off (60), off (64) }, 100, 100);
            block (e, c, { on (70) }, 200, 3 * step);
            expect (notesOf (c.ons).back() == 70 && notesOf (c.ons)[(size_t) c.ons.size() - 2] == 70, "only the new note now");
        }

        beginTest ("Swing delays every second step");
        {
            ArpEngine::Settings s;
            s.swing = 0.75f;
            const auto r = play (s, { 60, 64 }, 4 * step);
            expect (timesOf (r.ons) == std::vector<int> { 0, step + step / 2, 2 * step, 3 * step + step / 2 });
        }

        beginTest ("Locked to the host: a new arpeggio starts on the next step");
        {
            ArpEngine e;
            e.prepare (48000.0);
            e.setSettings ({});
            rkfx::Transport t;
            t.bpm = 120.0;
            t.playing = true;
            t.hasPosition = true;
            t.ppq = 0.1;                    // 0.4 of a 1/16 into it
            Collected c;
            juce::MidiBuffer midi;
            midi.addEvent (on (60), 0);
            e.process (midi, 2 * step, t);
            collect (midi, 0, c);
            expect (! c.ons.empty());
            expectEquals (c.ons.front().time, (int) (0.6 * step), "on the grid, not on the key");
        }

        beginTest ("Fixed velocity, and other events pass");
        {
            ArpEngine::Settings s;
            s.fixedVelocity = true;
            s.velocity = 77;
            const auto r = play (s, { 60 }, step);
            expectEquals (r.ons.front().velocity, 77);

            ArpEngine e;
            e.prepare (48000.0);
            e.setSettings ({});
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 50), 10);
            e.process (midi, 512, {});
            expectEquals (midi.getNumEvents(), 1);
        }
    }

private:
    struct Event { int note, time, velocity; };
    struct Collected { std::vector<Event> ons, offs; };

    static juce::MidiMessage on (int note) { return juce::MidiMessage::noteOn (1, note, (juce::uint8) 100); }
    static juce::MidiMessage off (int note) { return juce::MidiMessage::noteOff (1, note); }

    static void collect (const juce::MidiBuffer& midi, int offset, Collected& c)
    {
        for (const auto metadata : midi)
        {
            const auto m = metadata.getMessage();
            if (m.isNoteOn())
                c.ons.push_back ({ m.getNoteNumber(), offset + metadata.samplePosition, m.getVelocity() });
            else if (m.isNoteOff())
                c.offs.push_back ({ m.getNoteNumber(), offset + metadata.samplePosition, 0 });
        }
    }

    /** Runs @p length samples in 64-sample blocks; @p events at the first sample. */
    static void block (ArpEngine& e, Collected& c, std::initializer_list<juce::MidiMessage> events, int start, int length)
    {
        for (int at = 0; at < length; at += 64)
        {
            juce::MidiBuffer midi;
            if (at == 0)
                for (const auto& m : events)
                    midi.addEvent (m, 0);
            const auto n = juce::jmin (64, length - at);
            e.process (midi, n, {});
            collect (midi, start + at, c);
        }
    }

    /** Holds @p notes from sample 0 (released at @p releaseAt, if > 0) for @p length samples. */
    static Collected play (const ArpEngine::Settings& s, std::initializer_list<int> notes, int length, int releaseAt = 0)
    {
        ArpEngine e;
        e.prepare (48000.0);
        e.setSettings (s);
        Collected c;
        for (int at = 0; at < length; at += 64)
        {
            juce::MidiBuffer midi;
            if (at == 0)
                for (const auto n : notes)
                    midi.addEvent (on (n), 0);
            if (releaseAt > 0 && releaseAt >= at && releaseAt < at + 64)
                for (const auto n : notes)
                    midi.addEvent (off (n), releaseAt - at);
            e.process (midi, juce::jmin (64, length - at), {});
            collect (midi, at, c);
        }
        return c;
    }

    static std::vector<int> notesOf (const std::vector<Event>& events)
    {
        std::vector<int> v;
        for (const auto& e : events)
            v.push_back (e.note);
        return v;
    }

    static std::vector<int> timesOf (const std::vector<Event>& events)
    {
        std::vector<int> v;
        for (const auto& e : events)
            v.push_back (e.time);
        return v;
    }
};

static ArpTests arpTests;
