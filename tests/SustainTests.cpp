#include <juce_audio_basics/juce_audio_basics.h>

#include "SustainEngine.h"

#include <vector>

/** RKSustain: sustain from the pedal's controller (swallowed) or the Pedal
    parameter, sostenuto catching only the held notes, hold until the next
    chord, latch toggling, retriggering, and releasing everything. */
class SustainTests : public juce::UnitTest
{
public:
    SustainTests() : juce::UnitTest ("RKSustain engine") {}

    void runTest() override
    {
        using Mode = SustainEngine::Mode;

        beginTest ("Sustain: the pedal holds released notes; lifting it ends them");
        {
            SustainEngine e;
            e.prepare();
            e.setSettings ({});
            auto out = run (e, { on (60), pedalCC (127), off (60) });
            expect (has (out, true, 60) && ! has (out, false, 60), "held by the pedal");
            expect (! hasController (out), "the pedal's controller is swallowed");
            out = run (e, { on (64), pedalCC (0) });
            expect (has (out, false, 60) && ! has (out, false, 64), "60 ends; 64 is still held by its key");
        }

        beginTest ("The Pedal parameter works like the controller");
        {
            SustainEngine e;
            e.prepare();
            SustainEngine::Settings s;
            s.controller = -1;
            s.pedalParameter = true;
            e.setSettings (s);
            auto out = run (e, { on (60), off (60) });
            expect (! has (out, false, 60), "held");
            s.pedalParameter = false;
            e.setSettings (s);
            out = run (e, {});
            expect (has (out, false, 60), "released with the parameter");
        }

        beginTest ("Sostenuto holds only the notes held when the pedal went down");
        {
            SustainEngine e;
            e.prepare();
            SustainEngine::Settings s;
            s.mode = Mode::sostenuto;
            e.setSettings (s);
            auto out = run (e, { on (48), pedalCC (127), on (60), off (48), off (60) });
            expect (! has (out, false, 48), "the bass note rings");
            expect (has (out, false, 60), "the melody note doesn't");
            out = run (e, { pedalCC (0) });
            expect (has (out, false, 48));
        }

        beginTest ("Hold: notes ring until the next chord replaces them");
        {
            SustainEngine e;
            e.prepare();
            SustainEngine::Settings s;
            s.mode = Mode::hold;
            e.setSettings (s);
            auto out = run (e, { on (60), on (64), off (60), off (64) });
            expect (! has (out, false, 60) && ! has (out, false, 64), "ringing");
            out = run (e, { on (62) });
            expect (has (out, false, 60) && has (out, false, 64) && has (out, true, 62), "the next chord replaces them");
        }

        beginTest ("Latch toggles; a sustained note played again restarts");
        {
            SustainEngine e;
            e.prepare();
            SustainEngine::Settings s;
            s.mode = Mode::latch;
            e.setSettings (s);
            auto out = run (e, { on (60), off (60) });
            expect (has (out, true, 60) && ! has (out, false, 60));
            out = run (e, { on (60) });
            expect (has (out, false, 60), "the second press ends it");

            SustainEngine sustain;
            sustain.prepare();
            sustain.setSettings ({});
            run (sustain, { on (60), pedalCC (127), off (60) });
            out = run (sustain, { on (60) });
            expect (has (out, false, 60) && has (out, true, 60), "restarted");

            juce::MidiBuffer released;
            sustain.releaseAll (released);
            expect (has (released, false, 60));
        }
    }

private:
    static juce::MidiMessage on (int note) { return juce::MidiMessage::noteOn (1, note, (juce::uint8) 100); }
    static juce::MidiMessage off (int note) { return juce::MidiMessage::noteOff (1, note); }
    static juce::MidiMessage pedalCC (int value) { return juce::MidiMessage::controllerEvent (1, 64, value); }

    static juce::MidiBuffer run (SustainEngine& e, std::initializer_list<juce::MidiMessage> events)
    {
        juce::MidiBuffer midi;
        int position = 0;
        for (const auto& m : events)
            midi.addEvent (m, position++);
        e.process (midi);
        return midi;
    }

    static bool has (const juce::MidiBuffer& midi, bool noteOn, int note)
    {
        for (const auto metadata : midi)
        {
            const auto m = metadata.getMessage();
            if (m.getNoteNumber() == note && (noteOn ? m.isNoteOn() : m.isNoteOff()))
                return true;
        }
        return false;
    }

    static bool hasController (const juce::MidiBuffer& midi)
    {
        for (const auto metadata : midi)
            if (metadata.getMessage().isController())
                return true;
        return false;
    }
};

static SustainTests sustainTests;
