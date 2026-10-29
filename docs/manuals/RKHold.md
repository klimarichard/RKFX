# RKHold

**Note freeze / sustainer** · Delay family (sea green) · zero latency

Press a footswitch while a note sounds, and RKHold keeps that note going as a steady drone for as long as you like. Then play over it: hold the open D on the violin and play a melody above it, hold a chord on a guitar and solo over it, hold a sung note as a pad behind the next phrase. Each press can add a layer, so a chord builds up note by note, and a held note can sound an octave down as a bass drone.

Your own sound passes through untouched. For the header, presets, Output and Bypass, see [Common Controls](Common%20Controls.md).

## How it holds

RKHold keeps the last second of what you play. Pressing **Hold** takes the last quarter of a second of it, so the note must already be sounding when you press (its attack is left out).

- **Note mode** (a single note: violin, voice, a single guitar note): RKHold finds the note's pitch and plays it back as a chain of small slices, two cycles each, cut exactly at the note's cycles and taken from different places in what it captured. The note keeps the small movements of a real sustained note, without a buzzing loop or a beat. It can sound an octave down or up, keeping the instrument's tone.
- **Texture mode** (chords, double stops, noise): small grains of 80 ms from random places, overlapping: the classic freeze-pedal sound. Works on anything.
- **Auto** (default) uses Note mode when it hears a steady pitch, and Texture mode otherwise.

Holding silence holds nothing: if you press with nothing sounding, no layer starts.

## The display

Four layer slots show what each holds (the note's name, with middle C as C3, or "texture") and its level as it fades. The big **Hold** and **Clear** buttons work like the footswitches.

## Capture

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Mode** | Auto, Note, Texture | Auto | How it holds (see above). Note forces Note mode (falls back to Texture without a clear pitch). |
| **Range** | Violin, Voice, Guitar, Bass | Violin | The range of notes Note mode looks for. |
| **Hold Mode** | Momentary, Latch | Latch | **Latch:** press to hold, press again to release (with Layers: Add, each press adds a layer and **Clear** releases them). **Momentary:** held while the switch is down, released when it comes up, like a sustain pedal. |
| **Layers** | Replace, Add | Replace | **Replace:** each new hold crossfades from the previous one. **Add:** each press adds a layer, up to four; a fifth replaces the oldest. |

## Sound

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Octave** | -1, 0, +1 | 0 | The held sound an octave down (a bass drone under you), as played, or an octave up. |
| **Movement** | 0 to 100% | 40% | How much the held sound keeps changing. 0%: steady, organ-like. Higher: the slices are picked more freely and drift a few cents, so the hold breathes. |
| **Tone** | 1 to 20 kHz | 12 kHz | A low-pass on the held sound, to soften it under your playing. |
| **Width** | 0 to 100% | 50% | With Layers: Add, spreads the layers left and right as they're added. |

## Envelope and Mix

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Attack** | 10 ms to 2 s | 80 ms | How fast a held layer fades in (long: a swell). |
| **Release** | 50 ms to 10 s | 600 ms | How fast it fades out when released or cleared (long: the drone dies away). |
| **Hold Level** | -60 to +6 dB | 0 dB | The level of the held sound. |
| **Dry Level** | -60 to +6 dB | 0 dB | The level of your own sound. Turn it down to hear only the hold, for example when your dry signal already goes to the PA elsewhere. |

Plus **Output** and **Bypass**. Bypass fades the hold out; switching back on starts with nothing held. Changing presets keeps the held layers, so you can change the sound under a drone.

## Footswitches

In Gig Performer, map a footswitch to **Hold**, and one to **Clear** if you build chords with Layers: Add. Set the footswitches to **momentary** (on while pressed): RKHold counts each press itself. On **RKBoard** (the **Hold** pedal), assign them through Assign slots or the standalone app's MIDI learn.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Violin Drone** | Note mode, Latch, Replace, Attack 300 ms, Release 2 s, Movement 40% |
| **Octave Drone** | Note mode, Octave -1, Hold Level -3 dB: a bass note under you |
| **Chord Builder** | Auto, Add, Width 80%: press on each note to build a chord |
| **Sustain Pedal** | Momentary, Attack 20 ms, Release 400 ms: a note sustains while the switch is down |
| **Texture Pad** | Texture mode, Movement 70%, Tone 6 kHz, Attack 1 s, Release 4 s: an ambient freeze of anything |
| **Vocal Pad** | Voice range, Note mode, Movement 60%, Tone 8 kHz, Release 3 s |

## Notes

- Press while the note is already sounding steadily, not on its attack.
- Note mode on a chord falls back to Texture mode; use Texture mode directly for chords.
