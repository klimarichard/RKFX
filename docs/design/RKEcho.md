# RKEcho: design

**MIDI echo** · MIDI family · MIDI in, MIDI out

RK + a verb, as every RKFX plugin: to *echo* what you play. This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKEcho repeats the notes you play, in time with the song, like a delay pedal, but on MIDI. Because the repeats are notes and not sound, they can do what an audio delay can't:

- **Climb or fall:** each repeat transposed by an interval (rising fifths, octaves) or by scale steps in a key (each repeat a third higher, in tune with the song).
- **Go to another instrument:** the repeats on another MIDI channel, so a piano line echoes on a bell or a synth, or alternate between two instruments (a MIDI ping-pong).
- **Stay clean:** each repeat is a fresh note at its own velocity, never a darker, noisier copy.

The notes you play pass through at once (MIDI adds no latency); only the repeats are scheduled.

## How it works

- **Each note-on** you play schedules its repeats: one every **Time** (synced to the song's tempo or free in ms), up to **Repeats** times.
- **Velocity:** each repeat's velocity is the previous one's × **Feedback** (10 to 100%). Repeats stop early once the velocity falls below **Min Velocity**. With Feedback at 100% the repeats keep the played velocity.
- **Transpose:** each repeat is shifted from the previous one by **Step**, in semitones (−12 to +12) or, with **Scale Steps** on, in steps of the **Key** and **Scale** (RKHarmonize's scale logic, `core/dsp/Scales.h`): Step +2 in C major turns C into E, then G, then B. Repeats stop when they'd leave the MIDI range.
- **Length:** a repeat lasts as long as the played note did (**As played**), or a fixed share of the echo time (**Gate**, 10 to 100%). When a repeat is due before the played note has ended, its length so far is used and capped at the echo time.
- **Swing:** every second repeat is delayed by Swing (0 to 50% of the echo time).
- **Channels:** repeats go to the played note's own channel (**Same**) or to **Echo Channel** (1 to 16). With **Alternate** set to another channel, repeats alternate between the two: a MIDI ping-pong between two instruments.
- **Original:** the played notes pass through (On, default), or only the repeats sound (Off), for a pure "echo instrument".
- **Retrigger safety:** if a repeat would start a note that is still sounding (same channel and note), that note is ended first, so no synth gets a doubled note-on.
- **Everything else** (controllers, pitch bend, sustain pedal, program changes) passes through unchanged. Repeats aren't held by the sustain pedal (they have their own length).

### Limits

- Up to **256** scheduled events. If more are needed (a fast passage with many repeats), the oldest pending repeats are dropped, never the note-offs of sounding ones.
- **Repeats** 1 to 16; **Time** synced (1/1 to 1/32, dotted and triplet, `rkfx::SyncDivision`) or 10 ms to 2 s.

## Controls

| Control | ID | Range | Default |
| --- | --- | --- | --- |
| Time | `time` | 10 ms to 2 s | 375 ms |
| Sync | `sync` | Off, on | On |
| Division | `division` | 1/1 to 1/32, D and T | 1/8 D |
| Repeats | `repeats` | 1 to 16 | 4 |
| Feedback | `feedback` | 10 to 100% | 70% |
| Min Velocity | `vel_min` | 1 to 127 | 10 |
| Step | `step` | −12 to +12 | 0 |
| Scale Steps | `scale_steps` | Off, on | Off |
| Key | `key` | C to B | C |
| Scale | `scale` | as RKHarmonize | Major |
| Length | `length` | As played, Gate | As played |
| Gate | `gate` | 10 to 100% | 50% |
| Swing | `swing` | 0 to 50% | 0% |
| Original | `original` | Off, on | On |
| Echo Channel | `channel` | Same, 1 to 16 | Same |
| Alternate | `alternate` | Off, 1 to 16 | Off |

Plus **Bypass**: bypassed, MIDI passes through unchanged; as bypass engages, every sounding repeat ends and pending repeats are dropped (the MIDI family's rule, `releaseMidi()`).

## The window

- **Echo display:** the next repeats as a row of bars along a time line, one per repeat: height for velocity, labelled with their note (middle C = C3) when a note is played, so you can see the climb and the decay before you hear it.
- **Sections:** Time (Sync, Time or Division, Repeats, Swing), Repeats (Feedback, Min Velocity, Length, Gate), Pitch (Step, Scale Steps, Key, Scale), Routing (Original, Echo Channel, Alternate).

## In a rig

- **Gig Performer:** between a keyboard's MIDI input and a synth, like RKArpeggiate. For repeats on another instrument, wire RKEcho's MIDI output to both synths' inputs (each listening to its channel), with Echo Channel set.
- **Format:** a MIDI effect, AU MIDI processor (`aumi`) and VST3, AU code `Ech1`, like the other MIDI effects. Not on RKBoard.

## Identity

- **MIDI family**, accent **rose `#D2457C`**.

## Factory presets

| Preset | Idea |
| --- | --- |
| Dotted Eighths | 1/8 D, 4 repeats, Feedback 65% |
| Rising Fifths | 1/8, 5 repeats, Step +7, Feedback 80% |
| Octave Cascade | 1/16, 4 repeats, Step +12, Gate 40%, Feedback 75% |
| Thirds in Key | 1/8, 6 repeats, Scale Steps on, Step +2, C major |
| Ping-Pong Instruments | 1/4, 6 repeats, Echo Channel 2, Alternate 3 |
| Echoes Only | Original off, 1/4 D, 3 repeats, Echo Channel 2: an echo layer on a second instrument |

## CPU and safety

- A sorted queue of at most 256 events, checked once per block: negligible.
- No hanging notes: every scheduled note-on has its note-off scheduled with it; bypass and preset changes end sounding repeats; dropped events are always pending note-ons (their note-offs go with them).

## As built (2026-10-29)

- **Owners:** each (channel, note) has one owner token at a time. A note-on that takes a note (a repeat, or a played note) ends the current owner first; a scheduled note-off only sends if its token still owns the note; a played note's note-off is dropped when a repeat has taken the note over. Tested with repeats as long as the echo time colliding with every next one (and checked to fail without the rule).
- **Times** are rounded to whole samples when scheduled (a swing of 20% was otherwise one sample late in floating point).
- **As played:** a repeat lasts as long as the played note did, capped at 90% of the echo time; a repeat due before the played note has ended lasts the cap.
- **Window 980 × 500.**

## Tests

- **Timing:** repeats at Time (and at the synced division for a given tempo), within one sample, swing on every second repeat.
- **Velocity:** each repeat × Feedback; repeats stop below Min Velocity; Feedback 100% keeps the velocity for all Repeats.
- **Transpose:** Step +7 gives 60, 67, 74…; Scale Steps +2 in C major gives C, E, G, B; repeats stop at the edge of the MIDI range.
- **Length:** As played copies the played length; Gate gives the share of the echo time; a repeat due before the note-off is capped.
- **Channels:** Echo Channel and Alternate route as set; Original off removes the played notes only.
- **Retrigger safety:** fast repeated notes never send two note-ons for the same note without a note-off between.
- **Never hanging:** bypass, preset change and the 256-event limit leave every note ended.
- **Validation:** auval (`aumi Ech1 RKMu`), pluginval at strictness 10, presets.
