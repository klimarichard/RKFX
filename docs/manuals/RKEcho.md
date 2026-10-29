# RKEcho

**MIDI echo** · MIDI family (rose) · MIDI in, MIDI out

RKEcho repeats the notes you play, in time with the song, like a delay pedal, but on MIDI. Because the repeats are notes and not sound, they can climb (rising fifths, octaves, thirds in the key), go to another instrument, or bounce between two instruments, and each repeat is a fresh, clean note.

The notes you play pass through at once; RKEcho adds no delay to them. For the header and presets, see [Common Controls](Common%20Controls.md) (MIDI effects).

## Setting it up in Gig Performer

Put RKEcho between a keyboard's MIDI input and a synth, like RKArpeggiate. For repeats on another instrument, wire RKEcho's MIDI output to both synths and set **Echo Channel** (and **Alternate**) to the channels the other synths listen to.

## The display

The repeats of the last note you played, as bars along the echo times: height for velocity, labelled with their notes (middle C is C3). Before you play, it shows C3. The first bar is your own note (dimmed when Original is off).

## Time

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Sync** | Off, on | On | Repeats in time with the song's tempo (Time as a note value) or in milliseconds. |
| **Time** | 10 ms to 2 s, or 1/1 to 1/32 (dotted, triplet) | 1/8 D | The time between repeats. |
| **Repeats** | 1 to 16 | 4 | How many repeats. |
| **Swing** | 0 to 50% | 0% | Delays every second repeat by this share of the time, for a shuffled echo. |

## Repeats

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **As played / Gate** | | As played | **As played:** each repeat lasts as long as your note did (at most 90% of the echo time). **Gate:** a fixed share of the echo time. |
| **Feedback** | 10 to 100% | 70% | Each repeat's velocity is the previous one's times this. 100% keeps every repeat as loud as your note. |
| **Min Velocity** | 1 to 127 | 10 | Repeats stop once they'd be quieter than this. |
| **Gate** | 10 to 100% | 50% | With Gate, how long each repeat lasts. |

## Pitch

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Step** | -12 to +12 | 0 | Each repeat moves this far from the previous one: +7 climbs in fifths, +12 in octaves, -12 falls. |
| **Scale Steps** | Off, on | Off | Counts Step in steps of the **Key** and **Scale** instead of semitones: Step +2 in C major turns C into E, G, B, D... (thirds in the key). |
| **Key**, **Scale** | as RKHarmonize | C, Major | The key and scale for Scale Steps. |

Repeats stop when they'd go beyond the MIDI note range.

## Routing

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Original** | Off, on | On | Your own notes pass through. Off: only the repeats sound (an "echo instrument"). |
| **Echo Channel** | Same, 1 to 16 | Same | The channel the repeats go to: your note's own, or another instrument's. |
| **Alternate** | Off, 1 to 16 | Off | Repeats alternate between Echo Channel and this channel: a MIDI ping-pong between two instruments. |

Plus **Bypass**: MIDI passes through unchanged, and every sounding repeat ends first.

## Good to know

- Controllers, pitch bend, the sustain pedal and program changes pass through unchanged. Repeats aren't held by the sustain pedal; they have their own length.
- A note is never started twice: if a repeat lands on a note that's still sounding, that note is ended first, so no synth gets stuck or doubled notes.
- In a very fast, dense passage with many repeats, RKEcho keeps at most 256 events waiting; beyond that, the earliest waiting repeats are dropped (never a note's end).

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Dotted Eighths** | 1/8 D, 4 repeats, Feedback 65% |
| **Rising Fifths** | 1/8, 5 repeats, Step +7, Feedback 80% |
| **Octave Cascade** | 1/16, 4 repeats, Step +12, Gate 40%, Feedback 75% |
| **Thirds in Key** | 1/8, 6 repeats, Scale Steps, Step +2, C major, Feedback 80% |
| **Ping-Pong Instruments** | 1/4, 6 repeats, Echo Channel 2, Alternate 3, Feedback 75% |
| **Echoes Only** | Original off, 1/4 D, 3 repeats, Echo Channel 2: an echo layer on a second instrument |
