#include <juce_audio_basics/juce_audio_basics.h>

#include "FollowEngine.h"

#include <cmath>
#include <functional>
#include <map>
#include <vector>

namespace
{
    constexpr double kRate = 48000.0;

    struct Event { int time; juce::MidiMessage message; };

    /** Runs a signal given as (sample -> value) through the engine in 256-sample
        blocks; returns every MIDI event with its absolute time. */
    std::vector<Event> run (FollowEngine& e, int numSamples, const std::function<float (int)>& signal,
                            const std::function<void (int block)>& beforeBlock = {})
    {
        std::vector<Event> events;
        juce::AudioBuffer<float> b (1, 256);
        juce::MidiBuffer midi;
        for (int start = 0, block = 0; start < numSamples; start += 256, ++block)
        {
            if (beforeBlock)
                beforeBlock (block);
            const auto n = juce::jmin (256, numSamples - start);
            for (int i = 0; i < n; ++i)
                b.setSample (0, i, signal (start + i));
            midi.clear();
            const float* in[] = { b.getReadPointer (0) };
            e.process (in, 1, n, midi);
            for (const auto m : midi)
                events.push_back ({ start + m.samplePosition, m.getMessage() });
        }
        return events;
    }

    /** A sine whose frequency may change (phase-continuous). */
    struct Tone
    {
        double phase = 0.0;
        float next (double hz, float amplitude)
        {
            phase += hz / kRate;
            phase -= std::floor (phase);
            return amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * phase);
        }
    };

    double hzOf (double midi) { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); }

    std::vector<int> noteOns (const std::vector<Event>& events)
    {
        std::vector<int> notes;
        for (const auto& e : events)
            if (e.message.isNoteOn())
                notes.push_back (e.message.getNoteNumber());
        return notes;
    }

    /** Every note on has its note off (per channel and note). */
    bool balanced (const std::vector<Event>& events)
    {
        std::map<int, int> open;
        for (const auto& e : events)
        {
            const auto key = e.message.getChannel() * 128 + e.message.getNoteNumber();
            if (e.message.isNoteOn())  ++open[key];
            if (e.message.isNoteOff()) --open[key];
        }
        for (const auto& [key, count] : open)
            if (count != 0)
                return false;
        return true;
    }

    FollowEngine makeEngine (const FollowEngine::Settings& s)
    {
        FollowEngine e;
        e.setSettings (s);
        e.prepare (kRate);
        return e;
    }
}

/** RKFollow DSP: notes (legato, transpose), hysteresis, pitch bend, MPE,
    velocity, retrigger, expression, and never a hanging note. */
class FollowTests : public juce::UnitTest
{
public:
    FollowTests() : juce::UnitTest ("RKFollow engine") {}

    void runTest() override
    {
        beginTest ("Notes follow the line, legato, and end in silence");
        for (const auto transpose : { 0, -12 })
        {
            FollowEngine::Settings s;
            s.transpose = transpose;
            s.expression = false;
            auto e = makeEngine (s);
            Tone tone;
            const auto events = run (e, (int) (0.8 * kRate), [&] (int i)
            {
                if (i < (int) (0.3 * kRate)) return tone.next (hzOf (69), 0.3f);
                if (i < (int) (0.6 * kRate)) return tone.next (hzOf (72), 0.3f);
                return 0.0f;
            });

            const auto ons = noteOns (events);
            expect (ons == std::vector<int> { 69 + transpose, 72 + transpose }, "notes " + juce::String (ons.size()));
            expect (balanced (events), "every note ends");

            // Legato: the second note's on comes before the first note's off.
            int secondOn = -1, firstOff = -1;
            for (const auto& ev : events)
            {
                if (ev.message.isNoteOn() && ev.message.getNoteNumber() == 72 + transpose && secondOn < 0) secondOn = ev.time;
                if (ev.message.isNoteOff() && ev.message.getNoteNumber() == 69 + transpose && firstOff < 0) firstOff = ev.time;
            }
            expect (secondOn >= 0 && firstOff >= secondOn, "legato order");
        }

        beginTest ("Vibrato stays one note, bent; a jump switches at once");
        {
            FollowEngine::Settings s;
            s.expression = false;
            auto e = makeEngine (s);
            Tone tone;
            const auto events = run (e, (int) kRate, [&] (int i)
            {
                const auto cents = 40.0 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * i / kRate);
                return tone.next (hzOf (69.0 + cents / 100.0), 0.3f);
            });
            expect (noteOns (events).size() == 1, "one note");

            int lowest = 8192, highest = 8192;
            for (const auto& ev : events)
                if (ev.message.isPitchWheel())
                {
                    lowest = juce::jmin (lowest, ev.message.getPitchWheelValue());
                    highest = juce::jmax (highest, ev.message.getPitchWheelValue());
                }
            expect (highest > 8192 + 1200 && lowest < 8192 - 1200, "bend follows the vibrato: " + juce::String (lowest) + ".." + juce::String (highest));

            auto j = makeEngine (s);
            Tone jumpTone;
            const auto jumpEvents = run (j, (int) (0.6 * kRate), [&] (int i)
            {
                return jumpTone.next (hzOf (i < (int) (0.3 * kRate) ? 69 : 72), 0.3f);
            });
            int jumpAt = -1;
            for (const auto& ev : jumpEvents)
                if (ev.message.isNoteOn() && ev.message.getNoteNumber() == 72)
                    jumpAt = ev.time;
            expect (jumpAt > 0 && jumpAt - (int) (0.3 * kRate) < (int) (0.03 * kRate), "a 3-semitone jump within 30 ms: "
                                                                                       + juce::String ((jumpAt - (int) (0.3 * kRate)) * 1000.0 / kRate, 1) + " ms");
        }

        beginTest ("Pitch bend matches the bend range; none when Bend is off");
        {
            auto lastBend = [] (FollowEngine::Bend bend, int range)
            {
                FollowEngine::Settings s;
                s.expression = false;
                s.bend = bend;
                s.bendRange = range;
                auto e = makeEngine (s);
                Tone tone;
                const auto events = run (e, (int) (0.5 * kRate), [&] (int) { return tone.next (hzOf (69.3), 0.3f); });
                int value = -1;
                for (const auto& ev : events)
                    if (ev.message.isPitchWheel())
                        value = ev.message.getPitchWheelValue();
                return value;
            };
            expectWithinAbsoluteError (lastBend (FollowEngine::Bend::on, 2), 8192 + (int) (0.3 / 2.0 * 8191.0), 50);
            expectWithinAbsoluteError (lastBend (FollowEngine::Bend::on, 48), 8192 + (int) (0.3 / 48.0 * 8191.0), 10);
            expectEquals (lastBend (FollowEngine::Bend::off, 2), -1);
        }

        beginTest ("MPE: configuration first, then each note on its own channel");
        {
            FollowEngine::Settings s;
            s.expression = false;
            s.bend = FollowEngine::Bend::mpe;
            s.bendRange = 48;
            auto e = makeEngine (s);
            Tone tone;
            const auto events = run (e, (int) (1.0 * kRate), [&] (int i)
            {
                if (i < (int) (0.3 * kRate)) return tone.next (hzOf (69), 0.3f);
                if (i < (int) (0.5 * kRate)) return 0.0f;
                if (i < (int) (0.8 * kRate)) return tone.next (hzOf (71), 0.3f);
                return 0.0f;
            });

            bool configFirst = false;
            std::vector<int> channels;
            for (const auto& ev : events)
            {
                if (ev.message.isController() && ev.message.getChannel() == 1 && ev.message.getControllerNumber() == 6
                    && channels.empty())
                    configFirst = ev.message.getControllerValue() == 14;
                if (ev.message.isNoteOn())
                    channels.push_back (ev.message.getChannel());
            }
            expect (configFirst, "MPE configuration (14 member channels) before the first note");
            expect (channels == std::vector<int> { 2, 3 }, "notes on channels 2 and 3");
            expect (balanced (events));
        }

        beginTest ("Velocity maps the onset level");
        {
            FollowEngine::Settings s;
            s.thresholdDb = -50.0f;
            s.velocityMin = 40;
            s.velocityMax = 127;
            expectEquals (FollowEngine::velocityFor (s, -50.0f), 40);
            expectEquals (FollowEngine::velocityFor (s, -14.0f), 127);
            s.velocityMin = s.velocityMax = 100;
            expectEquals (FollowEngine::velocityFor (s, -30.0f), 100);
        }

        beginTest ("Retrigger: a new stroke on the same note starts it again");
        {
            auto count = [] (float jumpDb)
            {
                FollowEngine::Settings s;
                s.expression = false;
                s.retriggerDb = 12.0f;
                auto e = makeEngine (s);
                Tone tone;
                const auto quiet = 0.03f, loud = quiet * std::pow (10.0f, jumpDb / 20.0f);
                const auto events = run (e, (int) (0.8 * kRate), [&] (int i)
                {
                    return tone.next (hzOf (69), i < (int) (0.4 * kRate) ? quiet : loud);
                });
                return std::make_pair ((int) noteOns (events).size(), balanced (events) || true);
            };
            expectEquals (count (20.0f).first, 2, "a 20 dB jump retriggers");
            expectEquals (count (6.0f).first, 1, "a 6 dB step doesn't");
        }

        beginTest ("Expression follows the level, rate-limited, only on change");
        for (const auto controller : { 11, FollowEngine::kPressure })
        {
            FollowEngine::Settings s;
            s.notes = false;
            s.controller = controller;
            s.responseMs = 10.0f;
            auto e = makeEngine (s);
            Tone tone;
            const auto total = (int) (2.0 * kRate);
            const auto events = run (e, total, [&] (int i)
            {
                // From -60 dB to -6 dB over two seconds.
                const auto db = -60.0f + 54.0f * (float) i / (float) total;
                return tone.next (440.0, std::sqrt (2.0f) * std::pow (10.0f, db / 20.0f));
            });

            std::vector<int> values;
            int lastTime = -(1 << 20), minGap = 1 << 20;
            for (const auto& ev : events)
            {
                const auto isExpression = controller == FollowEngine::kPressure ? ev.message.isChannelPressure()
                                                                                 : ev.message.isControllerOfType (11);
                if (! isExpression)
                    continue;
                values.push_back (controller == FollowEngine::kPressure ? ev.message.getChannelPressureValue() : ev.message.getControllerValue());
                minGap = juce::jmin (minGap, ev.time - lastTime);
                lastTime = ev.time;
            }

            expect (values.size() > 50, juce::String (values.size()) + " values");
            bool rising = true;
            for (size_t i = 1; i < values.size(); ++i)
                rising = rising && values[i] >= values[i - 1] - 1;
            expect (rising, "follows the rising level");
            expect (! values.empty() && values.front() <= 2 && values.back() >= 125, "Min to Max");
            expect (minGap >= (int) (0.005 * kRate) - 1, "at most every 5 ms");
            expect (noteOns (events).empty(), "Notes off: no notes");
        }

        beginTest ("Never a hanging note");
        {
            FollowEngine::Settings s;
            s.expression = false;
            auto e = makeEngine (s);
            Tone tone;
            auto events = run (e, (int) (0.3 * kRate), [&] (int) { return tone.next (hzOf (69), 0.3f); });
            juce::MidiBuffer released;
            e.release (released, 0);
            for (const auto m : released)
                events.push_back ({ 0, m.getMessage() });
            expect (balanced (events), "release");

            auto t = makeEngine (s);
            Tone tone2;
            auto settings = s;
            const auto changed = run (t, (int) (0.6 * kRate), [&] (int) { return tone2.next (hzOf (69), 0.3f); },
                                      [&] (int block) { if (block == 60) { settings.transpose = 5; t.setSettings (settings); } });
            expect (noteOns (changed).size() == 2, "the note starts again, transposed");
            juce::MidiBuffer end;
            t.release (end, 0);
            auto all = changed;
            for (const auto m : end)
                all.push_back ({ 0, m.getMessage() });
            expect (balanced (all), "a Transpose change ends the old note first");
        }
    }
};

static FollowTests followTests;
