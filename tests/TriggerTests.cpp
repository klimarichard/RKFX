#include <juce_audio_basics/juce_audio_basics.h>

#include "TriggerEngine.h"

#include <cmath>
#include <functional>
#include <vector>

namespace
{
    constexpr double kRate = 48000.0;

    struct Event { int time; juce::MidiMessage message; };

    /** A drum-like hit at @p start: a sine at @p hz decaying with @p decayMs,
        optionally faded in over @p attackMs (no beater click). */
    float hit (int i, int start, double hz, float amplitude, double decayMs = 80.0, double attackMs = 0.0)
    {
        if (i < start)
            return 0.0f;
        const auto t = (double) (i - start) / kRate;
        const auto fadeIn = attackMs > 0.0 ? 1.0 - std::exp (-t * 1000.0 / attackMs) : 1.0;
        return amplitude * (float) (fadeIn * std::sin (2.0 * juce::MathConstants<double>::pi * hz * t) * std::exp (-t * 1000.0 / decayMs));
    }

    std::vector<Event> run (TriggerEngine& e, juce::AudioBuffer<float>& audio)
    {
        std::vector<Event> events;
        juce::MidiBuffer midi;
        for (int start = 0; start < audio.getNumSamples(); start += 256)
        {
            const auto n = juce::jmin (256, audio.getNumSamples() - start);
            float* io[] = { audio.getWritePointer (0, start), audio.getNumChannels() > 1 ? audio.getWritePointer (1, start) : nullptr };
            midi.clear();
            e.process (io, audio.getNumChannels(), n, midi);
            for (const auto m : midi)
                events.push_back ({ start + m.samplePosition, m.getMessage() });
        }
        return events;
    }

    juce::AudioBuffer<float> mono (double seconds, const std::function<float (int)>& signal)
    {
        juce::AudioBuffer<float> b (1, (int) (seconds * kRate));
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (0, i, signal (i));
        return b;
    }

    std::vector<Event> ons (const std::vector<Event>& events)
    {
        std::vector<Event> result;
        for (const auto& e : events)
            if (e.message.isNoteOn())
                result.push_back (e);
        return result;
    }

    TriggerEngine makeEngine (const TriggerEngine::Settings& s)
    {
        TriggerEngine e;
        e.setSettings (s);
        e.prepare (kRate);
        return e;
    }
}

/** RKTrigger DSP: one hit gives one note (however long it rings), velocity
    and scan timing, mask and retrigger, key filter, crosstalk, audio thru
    and mute. */
class TriggerTests : public juce::UnitTest
{
public:
    TriggerTests() : juce::UnitTest ("RKTrigger engine") {}

    void runTest() override
    {
        beginTest ("One hit, one note, however long it rings");
        for (const auto decayMs : { 30.0, 150.0, 400.0 })
        {
            auto e = makeEngine ({});
            auto audio = mono (1.5, [&] (int i) { return hit (i, 4800, 60.0, 0.8f, decayMs); });
            const auto events = run (e, audio);
            const auto notes = ons (events);
            expectEquals ((int) notes.size(), 1, "decay " + juce::String (decayMs) + " ms");

            int offs = 0, offAt = -1;
            for (const auto& ev : events)
                if (ev.message.isNoteOff()) { ++offs; offAt = ev.time; }
            expectEquals (offs, 1);
            if (! notes.empty())
                expectWithinAbsoluteError (offAt - notes[0].time, (int) (0.05 * kRate), 2, "note off after Length");
        }

        beginTest ("The note leaves Scan ms after the hit, with its velocity");
        {
            TriggerEngine::Settings s;
            s.triggers[0].scanMs = 2.0f;
            auto e = makeEngine (s);
            auto audio = mono (0.5, [&] (int i) { return hit (i, 4800, 200.0, 0.5f); });
            const auto notes = ons (run (e, audio));
            expect (notes.size() == 1);
            if (! notes.empty())
            {
                // The crossing is within the first cycle's rise (under 1.25 ms at 200 Hz).
                const auto delayMs = (notes[0].time - 4800) * 1000.0 / kRate;
                expect (delayMs >= 2.0 && delayMs < 3.5, "sent " + juce::String (delayMs, 2) + " ms after the hit starts");
            }

            TriggerEngine::Trigger t;
            t.thresholdDb = -30.0f;
            t.dynamicsDb = 30.0f;
            expectEquals (TriggerEngine::velocityFor (t, -30.0f), 1);
            expectEquals (TriggerEngine::velocityFor (t, 0.0f), 127);
            t.velocityMin = t.velocityMax = 100;
            expectEquals (TriggerEngine::velocityFor (t, -12.0f), 100);
        }

        beginTest ("Mask: a softer hit inside it is dropped, a much louder one gets through");
        {
            auto count = [] (float secondAmplitude, int gapMs)
            {
                auto e = makeEngine ({});
                const auto second = 4800 + gapMs * 48;
                auto audio = mono (1.0, [&] (int i) { return hit (i, 4800, 120.0, 0.2f, 40.0) + hit (i, second, 120.0, secondAmplitude, 40.0); });
                return (int) ons (run (e, audio)).size();
            };
            expectEquals (count (0.1f, 20), 1, "softer, inside the mask");
            expectEquals (count (0.8f, 20), 2, "12 dB louder, inside the mask");
            expectEquals (count (0.2f, 120), 2, "equal, after the mask");
        }

        beginTest ("The key filter keeps the wrong drum out");
        {
            TriggerEngine::Settings s;
            s.triggers[0].highPassHz = 200.0f;
            auto filtered = makeEngine (s);
            // A kick's body without the beater click (which a high-pass rightly lets through).
            auto audio = mono (0.5, [&] (int i) { return hit (i, 4800, 30.0, 0.5f, 80.0, 3.0); });
            expectEquals ((int) ons (run (filtered, audio)).size(), 0, "30 Hz under a 200 Hz high-pass");

            auto open = makeEngine ({});
            auto again = mono (0.5, [&] (int i) { return hit (i, 4800, 30.0, 0.5f, 80.0, 3.0); });
            expectEquals ((int) ons (run (open, again)).size(), 1, "without the filter");
        }

        beginTest ("Crosstalk: a weak copy on the other trigger is dropped");
        {
            auto notes = [] (float rightShare)
            {
                TriggerEngine::Settings s;
                s.mode = TriggerEngine::Mode::dual;
                s.crosstalk = 0.5f;
                auto e = makeEngine (s);
                juce::AudioBuffer<float> audio (2, (int) (0.5 * kRate));
                for (int i = 0; i < audio.getNumSamples(); ++i)
                {
                    audio.setSample (0, i, hit (i, 4800, 100.0, 0.8f));
                    audio.setSample (1, i, hit (i, 4848, 100.0, 0.8f * rightShare));   // 1 ms later
                }
                return ons (run (e, audio));
            };
            const auto echo = notes (0.2f);
            expect (echo.size() == 1 && echo[0].message.getNoteNumber() == 36, "one note, the left trigger's");
            expectEquals ((int) notes (1.0f).size(), 2, "equal hits: both");
        }

        beginTest ("Audio: Thru is untouched, Mute is silent without a click");
        {
            auto thru = makeEngine ({});
            auto audio = mono (0.3, [&] (int i) { return hit (i, 2000, 100.0, 0.5f); });
            const auto original = audio;
            run (thru, audio);
            bool identical = true;
            for (int i = 0; i < audio.getNumSamples(); ++i)
                identical = identical && juce::exactlyEqual (audio.getSample (0, i), original.getSample (0, i));
            expect (identical);

            TriggerEngine::Settings s;
            auto muted = makeEngine (s);
            auto tone = mono (0.3, [] (int i) { return 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / kRate); });
            juce::MidiBuffer midi;
            float* io[] = { tone.getWritePointer (0) };
            muted.process (io, 1, 4800, midi);
            s.mute = true;
            muted.setSettings (s);
            float* rest[] = { tone.getWritePointer (0, 4800) };
            muted.process (rest, 1, tone.getNumSamples() - 4800, midi);

            float maxStep = 0.0f, tail = 0.0f;
            for (int i = 1; i < tone.getNumSamples(); ++i)
                maxStep = juce::jmax (maxStep, std::abs (tone.getSample (0, i) - tone.getSample (0, i - 1)));
            for (int i = 4800 + 2400; i < tone.getNumSamples(); ++i)
                tail = juce::jmax (tail, std::abs (tone.getSample (0, i)));
            expect (maxStep < 0.02f, "fades: max step " + juce::String (maxStep));
            expect (tail < 1.0e-6f, "silent after the fade");
        }

        beginTest ("Never a hanging note");
        {
            auto e = makeEngine ({});
            auto audio = mono (0.2, [&] (int i) { return hit (i, 2000, 100.0, 0.5f); });
            auto events = run (e, audio);
            juce::MidiBuffer released;
            e.release (released, 0);
            int on = 0, off = 0;
            for (const auto& ev : events) { on += ev.message.isNoteOn() ? 1 : 0; off += ev.message.isNoteOff() ? 1 : 0; }
            for (const auto m : released) off += m.getMessage().isNoteOff() ? 1 : 0;
            expectEquals (on, off);
        }
    }
};

static TriggerTests triggerTests;
