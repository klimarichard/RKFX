#include <juce_audio_basics/juce_audio_basics.h>

#include "StrumEngine.h"

#include <vector>

/** RKStrum: a chord comes out in strum order and spacing after the window,
    alternating directions, the velocity ramp, a quick chord still sounding,
    notes released after they sound, and other events at once. */
class StrumTests : public juce::UnitTest
{
public:
    StrumTests() : juce::UnitTest ("RKStrum engine") {}

    void runTest() override
    {
        using Direction = StrumEngine::Direction;
        constexpr int window = 480, spacing = 960;   // 10 ms, 20 ms at 48 kHz

        beginTest ("A chord, spread low to high after the window");
        {
            StrumEngine e;
            e.prepare (48000.0);
            e.setSettings ({});
            const auto c = run (e, { { on (67), 0 }, { on (60), 30 }, { on (64), 60 } }, 4000);
            expect (notes (c.ons) == std::vector<int> { 60, 64, 67 }, "low to high, whatever order they came in");
            expect (times (c.ons) == std::vector<int> { window, window + spacing, window + 2 * spacing });
        }

        beginTest ("Up, alternate, and the velocity ramp");
        {
            StrumEngine e;
            e.prepare (48000.0);
            StrumEngine::Settings s;
            s.direction = Direction::alternate;
            s.ramp = -0.5f;
            e.setSettings (s);
            auto c = run (e, { { on (60), 0 }, { on (64), 0 }, { on (67), 0 } }, 4000);
            expect (notes (c.ons) == std::vector<int> { 60, 64, 67 }, "first chord down");
            expect (c.ons.back().velocity == 50 && c.ons.front().velocity == 100, "the last at half velocity");
            c = run (e, { { off (60), 0 }, { off (64), 0 }, { off (67), 0 }, { on (60), 100 }, { on (64), 100 }, { on (67), 100 } }, 4000);
            expect (notes (c.ons) == std::vector<int> { 67, 64, 60 }, "the next one up");
        }

        beginTest ("Releases: after the note sounds, or briefly when the chord was quick");
        {
            StrumEngine e;
            e.prepare (48000.0);
            e.setSettings ({});
            auto c = run (e, { { on (60), 0 }, { on (64), 0 }, { off (60), 100 }, { off (64), 100 } }, 6000);
            expectEquals ((int) c.offs.size(), 2, "both end");
            for (const auto& o : c.offs)
            {
                const auto start = o.note == 60 ? window : window + spacing;
                expectEquals (o.time, start + 960, "20 ms after it sounded");
            }

            c = run (e, { { on (60), 0 } }, 2000);
            c = run (e, { { off (60), 10 } }, 100);
            expect (times (c.offs) == std::vector<int> { 10 }, "a sounding note ends at once");
        }

        beginTest ("Other events pass at once; release ends what sounds");
        {
            StrumEngine e;
            e.prepare (48000.0);
            e.setSettings ({});
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 5);
            e.process (midi, 64);
            expectEquals (midi.getNumEvents(), 1);

            run (e, { { on (60), 0 } }, 2000);
            juce::MidiBuffer released;
            e.releaseAll (released);
            expectEquals (released.getNumEvents(), 1);
        }
    }

private:
    struct Event { int note, time, velocity; };
    struct Collected { std::vector<Event> ons, offs; };

    static juce::MidiMessage on (int note) { return juce::MidiMessage::noteOn (1, note, (juce::uint8) 100); }
    static juce::MidiMessage off (int note) { return juce::MidiMessage::noteOff (1, note); }

    /** Runs @p length samples (64-sample blocks); events at their sample. Times relative to this run. */
    static Collected run (StrumEngine& e, std::initializer_list<std::pair<juce::MidiMessage, int>> events, int length)
    {
        Collected c;
        for (int at = 0; at < length; at += 64)
        {
            juce::MidiBuffer midi;
            for (const auto& [m, time] : events)
                if (time >= at && time < at + 64)
                    midi.addEvent (m, time - at);
            e.process (midi, juce::jmin (64, length - at));
            for (const auto metadata : midi)
            {
                const auto m = metadata.getMessage();
                if (m.isNoteOn())
                    c.ons.push_back ({ m.getNoteNumber(), at + metadata.samplePosition, m.getVelocity() });
                else if (m.isNoteOff())
                    c.offs.push_back ({ m.getNoteNumber(), at + metadata.samplePosition, 0 });
            }
        }
        return c;
    }

    static std::vector<int> notes (const std::vector<Event>& v) { std::vector<int> r; for (const auto& e : v) r.push_back (e.note); return r; }
    static std::vector<int> times (const std::vector<Event>& v) { std::vector<int> r; for (const auto& e : v) r.push_back (e.time); return r; }
};

static StrumTests strumTests;
