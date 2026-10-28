# RKArpeggiate

**Arpeggiator (MIDI)** · MIDI family (pink) · MIDI effect: MIDI in, MIDI out, no delay

RKArpeggiate plays the notes you hold one after another, in time with the song: up, down, up and down, in the order you played them, at random, or all together as a rhythmic chord, over up to four octaves. Latch keeps it running after you let go, so your hands are free. Put it between your MIDI input and an instrument in Gig Performer.

For the header, presets and Bypass, see [Common Controls](Common%20Controls.md). As a MIDI effect it has no Dry/Wet and no Output; bypassed, MIDI passes through unchanged (the note playing ends first).

## Timing

Each step is one **Rate** note value at Gig Performer's tempo. While the host's transport plays, the steps lock to its beat: a new arpeggio starts on the next step of the grid, so it's always in time with the band and the loops. When the transport doesn't play, an arpeggio starts the moment you press the keys and runs at the tempo.

## Pattern

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Up / Down / Up/Down / Played / Random / Chord** | | Up | **Up**: lowest to highest. **Down**: highest to lowest. **Up/Down**: up, then back down (the top and bottom aren't repeated). **Played**: in the order you pressed the keys. **Random**: a random held note each step (never the same twice in a row). **Chord**: all held notes together on every step. |
| **Latch** | Off, on | Off | On: the arpeggio keeps playing after you release the keys; pressing new keys (after all were up) replaces it. Bypass or Latch off stops it. |
| **Rate** | 1/1 to 1/32, dotted and triplet | 1/16 | The length of a step. |
| **Octaves** | 1 to 4 | 1 | Repeats the pattern in the octaves above (Chord: the chord moves up an octave each step). |
| **Gate** | 5 to 100% | 75% | How long each note sounds, as a share of its step: short is staccato, 100% legato. |
| **Swing** | 50 to 75% | 50% | Delays every second step: 50% is straight, about 66% a triplet shuffle, 75% a hard swing. |

## Velocity

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **As Played / Fixed** | | As Played | **As Played**: each note keeps the velocity you played it with. **Fixed**: every note at the Velocity below. |
| **Velocity** | 1 to 127 | 100 | The fixed velocity. |

The keys you play are taken in (only the arpeggio comes out); everything else (sustain pedal, pitch bend, controllers) passes through.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**.

| Factory preset | Settings |
| --- | --- |
| **Sixteenths Up** | Up, 1/16 |
| **Eighths Up and Down** | Up/Down, 1/8, 2 octaves |
| **Triplet Cascade** | Down, 1/8 triplets, 3 octaves, Gate 60% |
| **Swung Sixteenths** | As played, Swing 62%, Gate 55% |
| **Latched Pad Pulse** | Chord, 1/8, Gate 40%, Latch on |
| **Random Sparkle** | Random, 2 octaves, Gate 30%, fixed velocity 90 |

## Notes

- Gig Performer tip: map Latch to a footswitch, so the arpeggio holds while you play something else.
- With RKHarmonize before it, every held note brings its harmony into the arpeggio.
