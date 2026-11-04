#include <juce_audio_basics/juce_audio_basics.h>

#include "CueEngine.h"

#include <map>
#include <vector>

/** RKCue: MIDI files played from keys. Timing at the file's tempo map, the
    host's and a fixed tempo; One-shot, Gate and Toggle; Track Pitch,
    Transpose and Keep Drums; Poly copies sharing a note; the per-slot and
    total limits; Thru and the trigger channel; Loop; the host buttons; and
    no hanging notes after any kind of stop. */
class CueTests : public juce::UnitTest
{
public:
    CueTests() : juce::UnitTest ("RKCue engine") {}

    void runTest() override
    {
        constexpr int beat = 24000;   // a quarter note at 120 BPM, 48 kHz

        beginTest ("A MIDI file's tempo map");
        {
            const auto song = CueEngine::Song::read (makeFile (true), 16);
            expect (song != nullptr);
            expectEquals ((int) song->tempoMap.size(), 2);
            expectWithinAbsoluteError (song->tempoAt (1.0), 120.0, 1.0e-6);
            expectWithinAbsoluteError (song->tempoAt (2.5), 60.0, 1.0e-6);
            expectWithinAbsoluteError (song->lengthSeconds(), 1.0 + 2.0, 1.0e-6, "2 beats at 120, 2 at 60");
        }

        beginTest ("One-shot: the file plays from the trigger's own sample, at the file's tempo");
        {
            Rig rig;
            const auto r = rig.run ({ on (36, 17) }, 5 * beat);
            const auto ons = r.onTimes();
            expectEquals ((int) ons.size(), 4);
            for (int k = 0; k < 4; ++k)
                expectWithinAbsoluteError (ons[(size_t) k], 17 + k * beat, 1, "note " + juce::String (k));
            expect (r.notesOn() == std::vector<int> { 60, 62, 64, 65 });
            expect (r.balanced(), "every note ended");
            expectEquals (rig.engine.getCopies (0), 0, "the copy ended with the file");
            expect (r.passed.empty(), "the trigger key is taken in");
        }

        beginTest ("The file's tempo changes are followed");
        {
            Rig rig (makeFile (true));
            const auto ons = rig.run ({ on (36, 0) }, 6 * beat).onTimes();
            expectEquals ((int) ons.size(), 4);
            expectWithinAbsoluteError (ons[2], 2 * beat, 1);
            expectWithinAbsoluteError (ons[3], 2 * beat + 2 * beat, 2, "beat 3 at 60 BPM");
        }

        beginTest ("Host and Fixed tempo");
        {
            Rig host;
            host.settings.tempoSource = CueEngine::TempoSource::host;
            host.settings.hostBpm = 60.0;
            host.settings.hostHasBpm = true;
            expectWithinAbsoluteError (host.run ({ on (36, 0) }, 3 * beat).onTimes()[1], 2 * beat, 2, "60 BPM from the host");

            Rig noHost;
            noHost.settings.tempoSource = CueEngine::TempoSource::host;
            noHost.settings.hostBpm = 60.0;
            noHost.settings.hostHasBpm = false;
            expectWithinAbsoluteError (noHost.run ({ on (36, 0) }, 2 * beat).onTimes()[1], beat, 2, "no host tempo: the file's");

            Rig fixed;
            fixed.settings.tempoSource = CueEngine::TempoSource::fixed;
            fixed.settings.fixedBpm = 240.0;
            expectWithinAbsoluteError (fixed.run ({ on (36, 0) }, beat).onTimes()[1], beat / 2, 2, "240 BPM");
        }

        beginTest ("One-shot retrigger restarts; key-up does nothing");
        {
            Rig rig;
            const auto r = rig.run ({ on (36, 0), off (36, 100), on (36, beat / 4) }, beat / 2);
            expectEquals ((int) r.onTimes().size(), 2, "the first note twice");
            expect (r.offTimeOf (60, 1) == beat / 4, "the first run's note ends at the restart");
            expect (r.events[2].on && r.events[2].time == beat / 4, "then the new note-on");
            expectEquals (rig.engine.getCopies (0), 1);
        }

        beginTest ("Gate: key-up stops the copy; Toggle: a second press stops it");
        {
            Rig gate;
            gate.settings.slots[0].mode = CueEngine::Mode::gate;
            const auto g = gate.run ({ on (36, 0), off (36, beat + 10) }, 4 * beat);
            expectEquals ((int) g.onTimes().size(), 2);
            expect (g.balanced());
            expectEquals (g.events.back().time, beat + 10, "its note ends on the key-up");

            Rig toggle;
            toggle.settings.slots[0].mode = CueEngine::Mode::toggle;
            const auto t = toggle.run ({ on (36, 0), off (36, 50), on (36, beat + 10), off (36, beat + 60) }, 4 * beat);
            expectEquals ((int) t.onTimes().size(), 2);
            expect (t.balanced());
            expectEquals (toggle.engine.getCopies (0), 0);

            const auto again = toggle.run ({ on (36, 0) }, 100);
            expectEquals ((int) again.onTimes().size(), 1, "the next press starts it again");
        }

        beginTest ("Track Pitch, Transpose and Keep Drums");
        {
            Rig rig (makeFile (false, true));
            auto& s = rig.settings.slots[0];
            s.low = 48; s.high = 72; s.root = 60;
            s.trackPitch = true;
            s.transpose = 2;
            const auto r = rig.run ({ on (65, 0) }, beat / 2);
            expect (r.notesOn (1) == std::vector<int> { 60 + 5 + 2 }, "key - root + transpose");
            expect (r.notesOn (10) == std::vector<int> { 38 }, "the drum stays");

            Rig off_;
            off_.settings.slots[0].low = 48;
            off_.settings.slots[0].high = 72;
            const auto plain = off_.run ({ on (65, 0) }, beat / 2);
            expect (plain.notesOn() == std::vector<int> { 60 }, "Track Pitch off: as written");

            Rig high;
            high.settings.slots[0].transpose = 24;
            high.settings.slots[0].trackPitch = true;
            high.settings.slots[0].low = 0;
            high.settings.slots[0].high = 127;
            high.settings.slots[0].root = 0;
            expect (high.run ({ on (60, 0) }, beat / 2).notesOn().empty(), "beyond 127: dropped");
        }

        beginTest ("Velocity scaling and the slot's channel");
        {
            Rig rig;
            rig.settings.slots[0].channel = 5;
            const auto full = rig.run ({ on (36, 0, 64) }, 100);
            expectEquals (full.events[0].velocity, juce::roundToInt (100.0f * 64.0f / 127.0f));
            expectEquals (full.events[0].channel, 5);

            Rig none;
            none.settings.slots[0].velocity = 0.0f;
            expectEquals (none.run ({ on (36, 0, 20) }, 100).events[0].velocity, 100, "0 %: as in the file");

            const auto stopped = rig.run ({ off (36, 0) }, 4 * beat);
            expect (! stopped.events.empty() && stopped.events.back().channel == 5, "the note-offs go to the same channel");
        }

        beginTest ("Poly: copies overlap; a note two copies share ends with the last one");
        {
            Rig rig (makeFile (false, false, true));   // one long note: 60 for 1.5 beats
            rig.settings.slots[0].poly = true;
            const auto r = rig.run ({ on (36, 0), on (36, beat / 2) }, 3 * beat);
            expectEquals ((int) r.onTimes().size(), 2);
            const auto offs = r.offTimes (60);
            expectEquals ((int) offs.size(), 1, "one note-off");
            expectWithinAbsoluteError (offs.front(), beat / 2 + beat * 3 / 2, 2, "when the second copy ends it");
        }

        beginTest ("Limits: 8 copies per slot, 32 in all");
        {
            Rig rig (makeFile (false, false, true));
            rig.settings.slots[0].poly = true;
            std::vector<Rig::In> presses;
            for (int k = 0; k < 10; ++k)
                presses.push_back (on (36, k * 10));
            const auto r = rig.run (presses, 200);
            expectEquals (rig.engine.getCopies (0), 8);
            auto all8 = r;
            all8.merge (rig.run ({}, 3 * beat));
            expect (all8.balanced (true), "stolen copies end their notes");

            Rig all (makeFile (false, false, true));
            presses.clear();
            for (int s = 0; s < 16; ++s)
            {
                all.settings.slots[(size_t) s].poly = true;
                all.load (s, makeFile (false, false, true));
                for (int k = 0; k < 3; ++k)
                    presses.push_back (on (36 + s, s * 30 + k * 10));
            }
            all.run (presses, 1000);
            int total = 0;
            for (int s = 0; s < 16; ++s)
                total += all.engine.getCopies (s);
            expectEquals (total, 32);
        }

        beginTest ("Thru, and the trigger channel");
        {
            Rig rig;
            const auto r = rig.run ({ on (50, 0), off (50, 10), cc (7, 20) }, 100);
            expectEquals ((int) r.passed.size(), 3, "other keys and controllers pass");

            Rig closed;
            closed.settings.thru = false;
            const auto c = closed.run ({ on (50, 0), off (50, 10), on (36, 20) }, 100);
            expect (c.passed.empty(), "Thru off: nothing else");
            expectEquals ((int) c.onTimes().size(), 1, "the slot still plays");

            Rig channel2;
            channel2.settings.triggerChannel = 2;
            const auto t = channel2.run ({ on (36, 0), on (36, 10, 100, 2) }, 100);
            expectEquals ((int) t.passed.size(), 1, "channel 1 passes");
            expectEquals ((int) t.onTimes().size(), 1, "channel 2 triggers");
        }

        beginTest ("Thru changed while a key is down still ends it");
        {
            Rig rig;
            rig.run ({ on (50, 0) }, 64);
            rig.settings.thru = false;
            const auto r = rig.run ({ off (50, 0) }, 64);
            expectEquals ((int) r.passed.size(), 1, "the note-off of a passed note always passes");
        }

        beginTest ("Loop plays round until stopped");
        {
            Rig rig;
            rig.settings.slots[0].loop = true;
            auto r = rig.run ({ on (36, 0) }, 8 * beat - 100);
            expectEquals ((int) r.onTimes().size(), 8);
            expectWithinAbsoluteError (r.onTimes()[4], 4 * beat, 2, "no gap at the loop");
            expectEquals (rig.engine.getCopies (0), 1);
            rig.engine.stop (0);
            r.merge (rig.run ({}, 64));
            expect (r.balanced(), "Stop ends it");
        }

        beginTest ("Host buttons: Play starts at the root, untransposed; release stops a Gate slot");
        {
            Rig rig;
            auto& s = rig.settings.slots[0];
            s.low = 48; s.high = 72; s.root = 60;
            s.trackPitch = true;
            s.mode = CueEngine::Mode::gate;
            rig.engine.play (0);
            auto r = rig.run ({}, 100);
            expect (r.notesOn() == std::vector<int> { 60 });
            expectEquals (r.events[0].time, 0);
            rig.engine.release (0);
            r.merge (rig.run ({}, 64));
            expect (r.balanced(), "released");
            expectEquals (rig.engine.getCopies (0), 0);
        }

        beginTest ("No hanging notes: Stop All, a new file, releaseAll");
        {
            for (int how = 0; how < 3; ++how)
            {
                Rig rig;
                rig.settings.slots[0].loop = true;
                auto r = rig.run ({ on (36, 0) }, 100);
                juce::MidiBuffer released;
                if (how == 0)
                    rig.engine.stopAll();
                else if (how == 1)
                    rig.engine.setSong (0, CueEngine::Song::read (makeFile(), 16));
                else
                    rig.engine.releaseAll (released);
                r.add (released, 0);
                r.merge (rig.run ({}, 64));
                expect (r.balanced(), "case " + juce::String (how));
                expectEquals (rig.engine.getCopies (0), 0);
            }
        }

        beginTest ("A key that becomes a trigger while held ends where it was passed");
        {
            Rig rig;
            rig.engine.setSong (1, nullptr);
            rig.settings.slots[1].low = rig.settings.slots[1].high = 50;
            rig.run ({ on (50, 0) }, 64);                     // slot 2 empty: passes
            rig.load (1, makeFile());
            const auto r = rig.run ({ on (50, 0) }, 64);      // now a trigger
            expectEquals ((int) r.passed.size(), 1, "a note-off for the passed note");
            expect (r.passed[0].isNoteOff());
        }
    }

private:
    struct Out { bool on; int time, channel, note, velocity; };

    struct Result
    {
        std::vector<Out> events;
        std::vector<juce::MidiMessage> passed;   // what went through (not from a file)
        std::map<int, int> outstanding;          // channel * 128 + note: ons - offs (from the files)

        void add (const juce::MidiBuffer& midi, int at)
        {
            for (const auto metadata : midi)
            {
                const auto m = metadata.getMessage();
                const auto time = at + metadata.samplePosition;
                if ((m.isNoteOn() || m.isNoteOff()) && m.getChannel() != 16)
                {
                    events.push_back ({ m.isNoteOn(), time, m.getChannel(), m.getNoteNumber(), m.getVelocity() });
                    outstanding[m.getChannel() * 128 + m.getNoteNumber()] += m.isNoteOn() ? 1 : -1;
                }
                else
                    passed.push_back (m);
            }
        }

        void merge (const Result& o)
        {
            events.insert (events.end(), o.events.begin(), o.events.end());
            passed.insert (passed.end(), o.passed.begin(), o.passed.end());
            for (const auto& [key, count] : o.outstanding)
                outstanding[key] += count;
        }

        std::vector<int> onTimes() const
        {
            std::vector<int> t;
            for (const auto& e : events)
                if (e.on)
                    t.push_back (e.time);
            return t;
        }

        std::vector<int> offTimes (int note) const
        {
            std::vector<int> t;
            for (const auto& e : events)
                if (! e.on && e.note == note)
                    t.push_back (e.time);
            return t;
        }

        int offTimeOf (int note, int nth) const
        {
            auto t = offTimes (note);
            return nth <= (int) t.size() ? t[(size_t) nth - 1] : -1;
        }

        std::vector<int> notesOn (int channel = 0) const
        {
            std::vector<int> n;
            for (const auto& e : events)
                if (e.on && (channel == 0 || e.channel == channel))
                    n.push_back (e.note);
            return n;
        }

        /** Every note-on has its note-off (shared notes: @p allowShared lets
            a note sounded by several copies be ended once). */
        bool balanced (bool allowShared = false) const
        {
            for (const auto& [key, count] : outstanding)
                if (allowShared ? count < 0 : count != 0)
                    return false;
            if (allowShared)
            {
                // Each (channel, note): after the last note-off, nothing sounds.
                std::map<int, bool> sounding;
                for (const auto& e : events)
                    sounding[e.channel * 128 + e.note] = e.on;
                for (const auto& [key, on] : sounding)
                    if (on)
                        return false;
            }
            return true;
        }
    };

    struct Rig
    {
        struct In { int time; juce::MidiMessage message; };

        Rig() : Rig (makeFile()) {}

        explicit Rig (const juce::MidiFile& file)
        {
            settings = CueEngine::defaultSettings();
            engine.prepare (48000.0);
            load (0, file);
        }

        void load (int slot, const juce::MidiFile& file)
        {
            engine.setSong (slot, CueEngine::Song::read (file, 16));
            juce::MidiBuffer settle;
            engine.setSettings (settings);
            engine.process (settle, 1);   // the stop the change asks for
        }

        Result run (std::vector<In> in, int length)
        {
            Result r;
            for (int at = 0; at < length; at += 64)
            {
                juce::MidiBuffer midi;
                for (auto& e : in)
                    if (e.time >= at && e.time < at + 64)
                        midi.addEvent (e.message, e.time - at);
                engine.setSettings (settings);
                engine.process (midi, juce::jmin (64, length - at));
                r.add (midi, now + at);
            }
            now += length;
            return r;
        }

        CueEngine engine;
        CueEngine::Settings settings;
        int now = 0;
    };

    /** Played keys come in on channel 16, which the files never use, so what passes is told from what the files play. */
    static Rig::In on (int key, int time, int velocity = 100, int channel = 16)
    {
        return { time, juce::MidiMessage::noteOn (channel, key, (juce::uint8) velocity) };
    }

    static Rig::In off (int key, int time) { return { time, juce::MidiMessage::noteOff (16, key) }; }
    static Rig::In cc (int controller, int time) { return { time, juce::MidiMessage::controllerEvent (16, controller, 64) }; }

    /** Four quarter notes on channel 1 (60, 62, 64, 65), half a beat each, at
        120 BPM; @p tempoChange: 60 BPM from beat 2; @p drum: plus a channel-10
        snare (38) at the start; @p longNote: only 60, 1.5 beats. */
    static juce::MidiFile makeFile (bool tempoChange = false, bool drum = false, bool longNote = false)
    {
        constexpr int ppq = 96;
        juce::MidiFile file;
        file.setTicksPerQuarterNote (ppq);

        juce::MidiMessageSequence tempoTrack;
        tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0);   // 120 BPM
        if (tempoChange)
            tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent (1000000), 2 * ppq);
        file.addTrack (tempoTrack);

        juce::MidiMessageSequence notes;
        if (longNote)
        {
            notes.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            notes.addEvent (juce::MidiMessage::noteOff (1, 60), 3 * ppq / 2);
            notes.addEvent (juce::MidiMessage::endOfTrack(), 2 * ppq);
        }
        else
        {
            const int keys[] = { 60, 62, 64, 65 };
            for (int k = 0; k < 4; ++k)
            {
                notes.addEvent (juce::MidiMessage::noteOn (1, keys[k], (juce::uint8) 100), k * ppq);
                notes.addEvent (juce::MidiMessage::noteOff (1, keys[k]), k * ppq + ppq / 2);
            }
            if (drum)
            {
                notes.addEvent (juce::MidiMessage::noteOn (10, 38, (juce::uint8) 100), 0);
                notes.addEvent (juce::MidiMessage::noteOff (10, 38), ppq / 4);
            }
            notes.addEvent (juce::MidiMessage::endOfTrack(), 4 * ppq);
        }
        notes.sort();
        file.addTrack (notes);
        return file;
    }
};

static CueTests cueTests;
