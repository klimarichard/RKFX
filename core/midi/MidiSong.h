#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace rkfx
{

/** A Standard MIDI File, ready to play: every track's events in beats
    (quarter notes), in time order, plus the file's tempo map. Shared by
    the plugins that play MIDI files (RKConduct, RKCue). Built on the
    message thread; read-only afterwards. */
struct MidiSong
{
    struct Event { double beat; int track; juce::MidiMessage message; };

    struct TrackInfo
    {
        juce::String name;
        int notes = 0;
    };

    /** A tempo from @c beat on. */
    struct Tempo { double beat; double bpm; };

    std::vector<Event> events;                  // by beat; at the same beat, note-offs first
    std::vector<TrackInfo> tracks;              // up to the maxTracks asked for
    std::vector<Tempo> tempoMap;                // never empty; the first at beat 0
    double lengthBeats = 0.0;
    double tempoBpm = 120.0;                    // the file's first tempo

    /** The tempo at @p beat (File tempo). */
    double tempoAt (double beat) const noexcept
    {
        auto bpm = tempoMap.front().bpm;
        for (const auto& t : tempoMap)
        {
            if (t.beat > beat)
                break;
            bpm = t.bpm;
        }
        return bpm;
    }

    /** The file's length in seconds at its own tempo map. */
    double lengthSeconds() const noexcept
    {
        double seconds = 0.0;
        for (size_t i = 0; i < tempoMap.size(); ++i)
        {
            const auto from = tempoMap[i].beat;
            const auto to = i + 1 < tempoMap.size() ? juce::jmin (tempoMap[i + 1].beat, lengthBeats) : lengthBeats;
            if (to > from)
                seconds += (to - from) * 60.0 / tempoMap[i].bpm;
        }
        return seconds;
    }

    /** Reads a Standard MIDI File (nullptr when it isn't one, or it is timed
        in SMPTE frames). Tracks beyond @p maxTracks are left out; tracks with
        only meta events (a tempo track) aren't listed. */
    static std::unique_ptr<MidiSong> read (const juce::MemoryBlock& data, int maxTracks)
    {
        juce::MemoryInputStream stream (data, false);
        juce::MidiFile file;
        if (! file.readFrom (stream))
            return nullptr;
        return read (file, maxTracks);
    }

    static std::unique_ptr<MidiSong> read (const juce::MidiFile& file, int maxTracks)
    {
        const auto ticksPerQuarter = (double) file.getTimeFormat();
        if (ticksPerQuarter <= 0.0)
            return nullptr;   // SMPTE time: not in beats

        auto s = std::make_unique<MidiSong>();
        auto tempoFound = false;

        for (int t = 0; t < file.getNumTracks() && (int) s->tracks.size() < maxTracks; ++t)
        {
            const auto* sequence = file.getTrack (t);
            TrackInfo info;
            const auto track = (int) s->tracks.size();

            for (int i = 0; i < sequence->getNumEvents(); ++i)
            {
                const auto& m = sequence->getEventPointer (i)->message;
                const auto beat = m.getTimeStamp() / ticksPerQuarter;

                if (m.isTrackNameEvent() && info.name.isEmpty())
                    info.name = m.getTextFromTextMetaEvent();
                if (m.isTempoMetaEvent())
                {
                    const auto bpm = juce::jlimit (5.0, 1000.0, 60.0 / m.getTempoSecondsPerQuarterNote());
                    s->tempoMap.push_back ({ beat, bpm });
                    if (! tempoFound)
                    {
                        s->tempoBpm = bpm;
                        tempoFound = true;
                    }
                }
                if (m.isEndOfTrackMetaEvent())
                    s->lengthBeats = juce::jmax (s->lengthBeats, beat);
                if (m.isMetaEvent())
                    continue;

                if (m.isNoteOn())
                    ++info.notes;
                s->events.push_back ({ beat, track, m });
                s->lengthBeats = juce::jmax (s->lengthBeats, beat);
            }

            if (info.notes > 0 || (! s->events.empty() && s->events.back().track == track))
            {
                if (info.name.isEmpty())
                    info.name = "Track " + juce::String (t + 1);
                s->tracks.push_back (info);
            }
        }

        // In time; at the same time, note-offs first (a repeated note restarts cleanly).
        std::stable_sort (s->events.begin(), s->events.end(), [] (const Event& a, const Event& b)
        {
            if (! juce::exactlyEqual (a.beat, b.beat))
                return a.beat < b.beat;
            return a.message.isNoteOff() && ! b.message.isNoteOff();
        });

        // The tempo map: the first tempo from the start (as RKConduct always
        // did), later changes in time; at the same beat the first one wins.
        std::stable_sort (s->tempoMap.begin(), s->tempoMap.end(), [] (const Tempo& a, const Tempo& b) { return a.beat < b.beat; });
        std::vector<Tempo> map;
        for (const auto& t : s->tempoMap)
        {
            if (map.empty() || t.beat - map.back().beat >= 1.0e-9)
                map.push_back (t);
        }
        if (map.empty())
            map.push_back ({ 0.0, s->tempoBpm });
        map.front().beat = 0.0;
        s->tempoBpm = map.front().bpm;
        s->tempoMap = std::move (map);
        return s;
    }
};

} // namespace rkfx
