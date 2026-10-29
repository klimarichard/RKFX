#include <juce_audio_basics/juce_audio_basics.h>

#include "EchoEngine.h"

#include <map>
#include <vector>

namespace
{
    constexpr double kRate = 48000.0;

    struct Event { int time; juce::MidiMessage message; };

    /** Plays @p input (absolute times) through the engine in 256-sample
        blocks for @p seconds; returns the output with absolute times. */
    std::vector<Event> run (EchoEngine& e, const std::vector<Event>& input, double seconds, double bpm = 120.0)
    {
        std::vector<Event> output;
        rkfx::Transport transport;
        transport.bpm = bpm;
        const auto total = (int) (seconds * kRate);
        juce::MidiBuffer midi;
        for (int start = 0; start < total; start += 256)
        {
            midi.clear();
            for (const auto& ev : input)
                if (ev.time >= start && ev.time < start + 256)
                    midi.addEvent (ev.message, ev.time - start);
            e.process (midi, 256, transport);
            for (const auto m : midi)
                output.push_back ({ start + m.samplePosition, m.getMessage() });
        }
        return output;
    }

    std::vector<Event> ons (const std::vector<Event>& events)
    {
        std::vector<Event> result;
        for (const auto& ev : events)
            if (ev.message.isNoteOn())
                result.push_back (ev);
        return result;
    }

    /** No note-on for a note that is already sounding; every note ended. */
    bool clean (const std::vector<Event>& events, bool requireEnded = true)
    {
        std::map<int, int> sounding;
        for (const auto& ev : events)
        {
            const auto key = ev.message.getChannel() * 128 + ev.message.getNoteNumber();
            if (ev.message.isNoteOn())
            {
                if (sounding[key] > 0)
                    return false;
                ++sounding[key];
            }
            else if (ev.message.isNoteOff())
                sounding[key] = 0;
        }
        if (requireEnded)
            for (const auto& [key, count] : sounding)
                if (count != 0)
                    return false;
        return true;
    }

    EchoEngine makeEngine (const EchoEngine::Settings& s)
    {
        EchoEngine e;
        e.setSettings (s);
        e.prepare (kRate);
        return e;
    }

    EchoEngine::Settings freeTime (float ms, int repeats, float feedback)
    {
        EchoEngine::Settings s;
        s.sync = false;
        s.timeMs = ms;
        s.repeats = repeats;
        s.feedback = feedback;
        return s;
    }

    std::vector<Event> playNote (int note, int velocity, int at, int length, int channel = 1)
    {
        return { { at, juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity) },
                 { at + length, juce::MidiMessage::noteOff (channel, note) } };
    }
}

/** RKEcho: repeat timing (free, synced, swing), velocity and Min Velocity,
    transposition (semitones and scale steps), lengths, channels, the
    owner rule (never a doubled note-on), pass-through and the event limit. */
class EchoTests : public juce::UnitTest
{
public:
    EchoTests() : juce::UnitTest ("RKEcho engine") {}

    void runTest() override
    {
        beginTest ("Repeats come every echo time; swing delays every second one");
        {
            auto e = makeEngine (freeTime (100.0f, 3, 1.0f));
            const auto notes = ons (run (e, playNote (60, 100, 1000, 2000), 1.0));
            expect (notes.size() == 4, "the note and three repeats");
            for (size_t k = 1; k < notes.size(); ++k)
                expectEquals (notes[k].time, 1000 + (int) k * 4800);

            auto s = freeTime (100.0f, 3, 1.0f);
            s.swing = 0.2f;
            auto swung = makeEngine (s);
            const auto swungNotes = ons (run (swung, playNote (60, 100, 1000, 2000), 1.0));
            expect (swungNotes.size() == 4);
            if (swungNotes.size() == 4)
            {
                expectEquals (swungNotes[1].time, 1000 + 4800);
                expectEquals (swungNotes[2].time, 1000 + 9600 + 960, "the second repeat 20% late");
                expectEquals (swungNotes[3].time, 1000 + 14400);
            }

            EchoEngine::Settings synced;
            synced.sync = true;
            synced.division = rkfx::SyncDivision::index (8, ' ');
            synced.repeats = 2;
            synced.feedback = 1.0f;
            auto se = makeEngine (synced);
            const auto syncedNotes = ons (run (se, playNote (60, 100, 0, 1000), 1.0, 120.0));
            expect (syncedNotes.size() == 3 && syncedNotes[1].time == 12000, "an eighth at 120 bpm is 250 ms");
        }

        beginTest ("Velocity falls by Feedback and stops under Min Velocity");
        {
            auto s = freeTime (50.0f, 8, 0.5f);
            s.velocityMin = 20;
            auto e = makeEngine (s);
            const auto notes = ons (run (e, playNote (60, 100, 0, 1000), 1.0));
            std::vector<int> velocities;
            for (const auto& n : notes)
                velocities.push_back (n.message.getVelocity());
            expect (velocities == std::vector<int> { 100, 50, 25 }, "100, 50, 25, then under 20");
        }

        beginTest ("Steps: semitones and scale steps, stopping at the range's edge");
        {
            auto s = freeTime (50.0f, 3, 1.0f);
            s.step = 7;
            auto e = makeEngine (s);
            std::vector<int> notes;
            for (const auto& n : ons (run (e, playNote (60, 100, 0, 500), 1.0)))
                notes.push_back (n.message.getNoteNumber());
            expect (notes == std::vector<int> { 60, 67, 74, 81 });

            s.step = 2;
            s.scaleSteps = true;
            auto scale = makeEngine (s);
            notes.clear();
            for (const auto& n : ons (run (scale, playNote (60, 100, 0, 500), 1.0)))
                notes.push_back (n.message.getNoteNumber());
            expect (notes == std::vector<int> { 60, 64, 67, 71 }, "thirds in C major");

            s.scaleSteps = false;
            s.step = 12;
            auto edge = makeEngine (s);
            expectEquals ((int) ons (run (edge, playNote (100, 100, 0, 500), 1.0)).size(), 3, "100, 112, 124, then out of range");
        }

        beginTest ("Lengths: as played (capped at the echo time), or the gate");
        {
            auto lengthOfFirstRepeat = [] (const EchoEngine::Settings& s, int playedLength)
            {
                auto e = makeEngine (s);
                const auto events = run (e, playNote (60, 100, 0, playedLength), 2.0);
                int on = -1;
                int count = 0;
                for (const auto& ev : events)
                {
                    if (ev.message.isNoteOn() && ++count == 2) on = ev.time;
                    if (ev.message.isNoteOff() && on >= 0) return ev.time - on;
                }
                return -1;
            };
            auto s = freeTime (200.0f, 1, 1.0f);
            expectEquals (lengthOfFirstRepeat (s, 2400), 2400, "as played: 50 ms");
            expectEquals (lengthOfFirstRepeat (s, 24000), (int) (0.9 * 9600), "held past the repeat: 90% of the echo time");
            s.asPlayed = false;
            s.gate = 0.5f;
            expectEquals (lengthOfFirstRepeat (s, 2400), 4800, "gate 50%");
        }

        beginTest ("Channels: echo channel, alternate, original off");
        {
            auto s = freeTime (50.0f, 4, 1.0f);
            s.channel = 2;
            s.alternate = 3;
            auto e = makeEngine (s);
            std::vector<int> channels;
            for (const auto& n : ons (run (e, playNote (60, 100, 0, 500), 1.0)))
                channels.push_back (n.message.getChannel());
            expect (channels == std::vector<int> { 1, 2, 3, 2, 3 });

            s.original = false;
            auto only = makeEngine (s);
            const auto events = run (only, playNote (60, 100, 0, 500), 1.0);
            bool originalHeard = false;
            for (const auto& ev : events)
                originalHeard = originalHeard || ev.message.getChannel() == 1;
            expect (! originalHeard, "only the repeats");
        }

        beginTest ("A note is never started twice; every note ends");
        {
            auto s = freeTime (100.0f, 4, 1.0f);
            s.asPlayed = false;
            s.gate = 1.0f;   // repeats as long as the echo time: they collide with the next
            auto e = makeEngine (s);
            std::vector<Event> input;
            for (int k = 0; k < 10; ++k)
                for (const auto& ev : playNote (60, 100, k * 4800, 6000))
                    input.push_back (ev);
            auto events = run (e, input, 3.0);
            expect (clean (events), "no doubled note-ons, all ended");
        }

        beginTest ("Everything else passes through");
        {
            auto e = makeEngine ({});
            const std::vector<Event> input { { 100, juce::MidiMessage::controllerEvent (1, 64, 127) },
                                             { 200, juce::MidiMessage::pitchWheel (1, 10000) } };
            const auto events = run (e, input, 0.1);
            expect (events.size() == 2 && events[0].message.isController() && events[1].message.isPitchWheel());
        }

        beginTest ("The event limit and bypass never leave a note hanging");
        {
            auto s = freeTime (300.0f, 16, 1.0f);
            auto e = makeEngine (s);
            std::vector<Event> input;
            for (int k = 0; k < 300; ++k)
                for (const auto& ev : playNote (40 + k % 40, 100, k * 100, 50))
                    input.push_back (ev);
            auto events = run (e, input, 1.0);
            juce::MidiBuffer released;
            e.releaseAll (released);
            for (const auto m : released)
                events.push_back ({ 48000, m.getMessage() });
            expect (clean (events), "balanced after release");
        }
    }
};

static EchoTests echoTests;
