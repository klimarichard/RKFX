#include <juce_audio_basics/juce_audio_basics.h>

#include "ConductEngine.h"

#include <vector>

/** RKConduct: reading a MIDI file into beats, tapping quarter notes while a
    1/16 pattern plays at the tapped tempo, waiting for a late tap, skipping
    on an early one, count-in, double triggers, tracks on/off and remapped,
    tap keys taken in while the keyboard plays on, stop, and loop. */
class ConductTests : public juce::UnitTest
{
public:
    ConductTests() : juce::UnitTest ("RKConduct engine") {}

    void runTest() override
    {
        constexpr int beat = 24000;          // a quarter note at 120 BPM, 48 kHz
        constexpr int sixteenth = beat / 4;

        beginTest ("A MIDI file in beats: tracks, names, notes, length, tempo");
        {
            const auto song = ConductEngine::makeSong (makeFile());
            expect (song != nullptr);
            expectEquals ((int) song->tracks.size(), 2, "the tempo track isn't listed");
            expectEquals (song->tracks[0].name, juce::String ("Arp"));
            expectEquals (song->tracks[0].notes, 8);
            expectEquals (song->tracks[1].notes, 1);
            expectWithinAbsoluteError (song->lengthBeats, 2.0, 1.0e-9);
            expectWithinAbsoluteError (song->tempoBpm, 100.0, 1.0e-6);
        }

        beginTest ("Tapping quarter notes: the sixteenths in between at the tapped tempo");
        {
            auto e = makeEngine();
            const auto r = run (*e, { 0, beat }, 2 * beat);
            const auto arp = onsOf (r, 1);
            expectEquals ((int) arp.size(), 8);
            expect (! e->isPlaying(), "the file ended (the next tap starts it again)");
            for (int k = 0; k < (int) arp.size(); ++k)
                expectWithinAbsoluteError (arp[(size_t) k], k * sixteenth, 1, "sixteenth " + juce::String (k));
        }

        beginTest ("Slower taps: the next beat's sixteenths spread out");
        {
            auto e = makeEngine();
            const auto slow = 28800;         // 100 BPM
            const auto r = run (*e, { 0, slow, 2 * slow }, 3 * slow);
            const auto arp = onsOf (r, 1);
            expectEquals ((int) arp.size(), 8);
            expectWithinAbsoluteError (arp[4], slow, 1, "beat 2 starts on the tap");
            expectWithinAbsoluteError (arp[5] - arp[4], slow / 4, 2, "at the tapped tempo");
        }

        beginTest ("A late tap: the player waits at the tap point");
        {
            auto e = makeEngine();
            const auto r = run (*e, { 0 }, 2 * beat);
            expectEquals ((int) onsOf (r, 1).size(), 4, "only the first beat");
            expect (e->isWaiting());
            expectWithinAbsoluteError (e->getPosition(), 1.0, 1.0e-9);
        }

        beginTest ("An early tap: skips to the tap point, ending what sounds");
        {
            auto e = makeEngine();
            const auto r = run (*e, { 0, beat / 2 }, beat);   // half way through the first beat
            const auto arp = onsOf (r, 1);
            expectEquals (arp[2], beat / 2, "beat 2's first note is on the tap");
            expectEquals (notesOn (r, 1)[2], 64, "the 3rd and 4th sixteenths are skipped");
        }

        beginTest ("Count-in taps, and the tempo they set");
        {
            auto e = makeEngine (2);
            const auto r = run (*e, { 0, 30000, 60000 }, 60000 + beat);
            const auto arp = onsOf (r, 1);
            expectWithinAbsoluteError (arp.front(), 60000, 1, "starts on the third tap");
            expectWithinAbsoluteError (arp[1] - arp[0], 30000 / 4, 2, "at the count-in's tempo (96 BPM)");
        }

        beginTest ("A double trigger is ignored");
        {
            auto e = makeEngine();
            const auto r = run (*e, { 0, 480 }, beat / 2);   // 10 ms apart
            expectEquals ((int) onsOf (r, 1).size(), 2, "no skip");
        }

        beginTest ("Tracks: off, and sent to another channel; tap keys taken in, the keyboard plays on");
        {
            auto e = makeEngine();
            ConductEngine::Settings s;
            s.trackOn.fill (true);
            s.trackChannel.fill (0);
            s.trackOn[1] = false;           // the bass
            s.trackChannel[0] = 5;          // the arpeggio to channel 5
            e->setSettings (s);
            const auto r = run (*e, { 0, beat }, beat + 100, { { 62, 50 } });
            expect (onsOf (r, 2).empty(), "no bass");
            expect (notesOn (r, 1) == std::vector<int> { 62 } && ! onsOf (r, 5).empty(), "the arpeggio on channel 5 (channel 1: only the played key)");
            expect (std::find (r.notesOn.begin(), r.notesOn.end(), 36) == r.notesOn.end(), "the tap key is taken in");
            expect (std::find (r.notesOn.begin(), r.notesOn.end(), 62) != r.notesOn.end(), "the other key passes");
        }

        beginTest ("Stop ends the notes and goes back; the end loops when set");
        {
            auto e = makeEngine();
            run (*e, { 0 }, 100);
            e->stop();
            const auto r = run (*e, {}, 64);
            expect (! e->isPlaying() && ! r.offs.empty(), "notes ended");
            expectEquals (e->getPosition(), 0.0);

            auto looping = makeEngine();
            ConductEngine::Settings s;
            s.trackOn.fill (true);
            s.trackChannel.fill (0);
            s.loop = true;
            looping->setSettings (s);
            const auto l = run (*looping, { 0, beat, 2 * beat, 3 * beat }, 4 * beat);
            expectEquals ((int) onsOf (l, 1).size(), 16, "two times round");
        }
    }

private:
    struct On { int channel, time, note; };
    struct Result
    {
        std::vector<On> ons;
        std::vector<int> notesOn;
        std::vector<int> offs;
    };

    static juce::MidiFile makeFile()
    {
        constexpr int ppq = 96;
        juce::MidiFile file;
        file.setTicksPerQuarterNote (ppq);

        juce::MidiMessageSequence tempoTrack;
        tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent (600000), 0);   // 100 BPM
        file.addTrack (tempoTrack);

        juce::MidiMessageSequence arp;
        arp.addEvent (juce::MidiMessage::textMetaEvent (3, "Arp"), 0);
        for (int k = 0; k < 8; ++k)
        {
            arp.addEvent (juce::MidiMessage::noteOn (1, 60 + k, (juce::uint8) 100), k * ppq / 4);
            arp.addEvent (juce::MidiMessage::noteOff (1, 60 + k), k * ppq / 4 + ppq / 8);
        }
        arp.addEvent (juce::MidiMessage::endOfTrack(), 2 * ppq);
        file.addTrack (arp);

        juce::MidiMessageSequence bass;
        bass.addEvent (juce::MidiMessage::noteOn (2, 36, (juce::uint8) 100), 0);
        bass.addEvent (juce::MidiMessage::noteOff (2, 36), 2 * ppq - 1);
        file.addTrack (bass);
        return file;
    }

    static std::unique_ptr<ConductEngine> makeEngine (int countIn = 0)
    {
        auto e = std::make_unique<ConductEngine>();
        ConductEngine::Settings s;
        s.trackOn.fill (true);
        s.trackChannel.fill (0);
        s.countIn = countIn;
        e->setSettings (s);
        e->prepare (48000.0);
        e->setSong (ConductEngine::makeSong (makeFile()));
        juce::MidiBuffer settle;
        e->process (settle, 1);   // the song change takes effect
        return e;
    }

    /** Taps the C1 key at @p taps (samples) for @p length samples; @p notes are other keys (note, time). */
    static Result run (ConductEngine& e, std::initializer_list<int> taps, int length,
                       std::initializer_list<std::pair<int, int>> notes = {})
    {
        Result r;
        for (int at = 0; at < length; at += 64)
        {
            juce::MidiBuffer midi;
            for (const auto t : taps)
                if (t >= at && t < at + 64)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), t - at);
            for (const auto& [note, t] : notes)
                if (t >= at && t < at + 64)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), t - at);
            e.process (midi, juce::jmin (64, length - at));
            for (const auto metadata : midi)
            {
                const auto m = metadata.getMessage();
                if (m.isNoteOn())
                {
                    r.ons.push_back ({ m.getChannel(), at + metadata.samplePosition, m.getNoteNumber() });
                    r.notesOn.push_back (m.getNoteNumber());
                }
                else if (m.isNoteOff())
                    r.offs.push_back (at + metadata.samplePosition);
            }
        }
        return r;
    }

    static std::vector<int> onsOf (const Result& r, int channel)
    {
        std::vector<int> times;
        for (const auto& o : r.ons)
            if (o.channel == channel)
                times.push_back (o.time);
        return times;
    }

    static std::vector<int> notesOn (const Result& r, int channel)
    {
        std::vector<int> notes;
        for (const auto& o : r.ons)
            if (o.channel == channel)
                notes.push_back (o.note);
        return notes;
    }
};

static ConductTests conductTests;
