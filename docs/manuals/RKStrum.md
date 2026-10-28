# RKStrum

**Strummer (MIDI)** · MIDI family (berry) · MIDI effect: MIDI in, MIDI out

RKStrum turns chords played on a keyboard into strummed chords: instead of all notes at once, they come out one after another, low to high like a guitar's downstroke (or high to low, or alternating), with the spacing, accents and looseness of a real strum. It makes keyboard guitars, harps, mandolins and plucked synths far more convincing. Put it between your MIDI input and the instrument in Gig Performer.

For the header, presets and Bypass, see [Common Controls](Common%20Controls.md). As a MIDI effect it has no Dry/Wet and no Output; bypassed, MIDI passes through unchanged.

## How it works

Notes that start within the **Chord Window** of each other count as one chord. When the window closes, the chord plays in strum order, each note **Speed** after the one before.

Because the order of a chord's notes isn't known until the chord is complete, its **first note waits for the window** (10 ms by default, about the time between the notes of a chord played by hand), and a single note is delayed by the same amount. Keep the window short; raise it only if your chords come out in two parts.

A note you release before the strum reaches it still sounds briefly (20 ms), so quick chords are never lost. Everything that isn't a note (sustain pedal, pitch bend, controllers) passes through at once.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Down / Up / Alternate** | | Down | **Down**: low to high, like a downstroke. **Up**: high to low. **Alternate**: down, then up, then down... for each new chord, like strumming a rhythm. |
| **Speed** | 2 to 100 ms | 20 ms | The time between one string and the next. 10 to 25 ms is a guitar strum; 50 to 100 ms a slow harp roll. |
| **Velocity Ramp** | -50 to +50% | 0% | How the velocity changes from the first note to the last: negative, the first string is strongest (a strum's accent); positive, the strum builds. |
| **Humanize** | 0 to 100% | 0% | Random timing on each string (up to half the Speed), so repeated strums don't sound mechanical. |
| **Chord Window** | 2 to 50 ms | 10 ms | Notes starting this close together are one chord (see above). |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**.

| Factory preset | Settings |
| --- | --- |
| **Guitar Downstroke** | Down, 18 ms, Velocity Ramp -10% |
| **Alternating Strum** | Alternate, 15 ms, Humanize 30% |
| **Slow Harp Roll** | Down, 70 ms, Velocity Ramp +10% |
| **Upstroke Accent** | Up, 12 ms, Velocity Ramp -25% |
| **Loose Human Strum** | Alternate, 25 ms, Humanize 70%, Velocity Ramp -15% |

## Notes

- For rhythm parts, Alternate with a little Humanize follows your playing: each new chord strums the other way.
- RKStrum after RKHarmonize strums the harmonized chord; before RKArpeggiate it makes little sense (the arpeggio already spreads the notes).
