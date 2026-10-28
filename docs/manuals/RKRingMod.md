# RKRingMod

**Ring modulator** · Modulation family (azure) · zero latency

RKRingMod multiplies your sound by a second tone, the **carrier**. Every note comes out moved to two new pitches, the sum and the difference of the note and the carrier, so the sound turns metallic, bell-like or clangorous, from subtle shimmer to wild sci-fi. With the carrier following the notes you play, it stays musical: the classic fusion-lead and synth-violin ring-mod sound. At very low carrier rates it becomes a choppy tremolo.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md). Dry/Wet blends the original back in; a little of it keeps the note's pitch clear.

## Carrier

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Free / Tracked / Synced** | Free, tracked, synced | Free | Where the carrier's pitch comes from. **Free**: the Frequency knob. **Tracked**: the note you play, shifted by Interval, so the result follows the melody. **Synced**: the host tempo, one carrier cycle per Division (low rates: a rhythmic chop). |
| **Sine / Tri / Square** | Sine, triangle, square | Sine | The carrier's wave. **Sine**: the pure ring-mod sound. **Triangle**: a little richer. **Square**: harsh and buzzy, many more partials. |
| **Frequency** (Free) | 1 Hz to 4 kHz | 440 Hz | The carrier's pitch. Below about 20 Hz it is heard as a tremolo with polarity flips; above, as new pitches. |
| **Interval** (Tracked) | -24 to +24 semitones | +7 | How far the carrier sits from the note you play. Whole octaves (12, -12) and fifths (7) sound most consonant; in-between values (e.g. 12.5) clang. 0 gives the octave above plus a deep growl. |
| **Bass / Guitar / Violin** (Tracked) | Bass, guitar, violin | Guitar | The range of notes it listens for. Like RKOctave and RKSynth, it follows single notes; with chords the carrier stays where it was. Between notes it holds the last pitch. |
| **Division** (Synced) | 1/1 to 1/32, dotted and triplet | 1/16 | One carrier cycle per note value, locked to the beat while the host plays. |

The carrier glides smoothly to new pitches, so moving Frequency or playing new notes never steps.

## LFO

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Depth** | 0 to 100% | 0% | Wobbles the carrier's pitch, up to an octave either way. |
| **Rate** | 0.05 to 10 Hz | 1 Hz | How fast it wobbles. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Metallic Bells** | Free, Sine, 620 Hz, Dry/Wet 60% |
| **Tracked Fifth** | Tracked, Sine, Interval +7, Dry/Wet 70% |
| **Fusion Lead Clang** | Tracked (Violin), Triangle, Interval +12.5, Dry/Wet 50% |
| **Robot Voice** | Free, Square, 90 Hz |
| **Wobbling Sci-Fi** | Free, Sine, 300 Hz, LFO Depth 60%, Rate 0.5 Hz, Dry/Wet 80% |
| **Synced Chop** | Synced, Square, 1/16 |

## Notes

- Tracked mode needs a clean signal: place RKRingMod before distortion, chorus, delay and reverb (or after RKAcoustic, a compressor or RKOctave).
- After a drive, a ring modulator sounds even wilder; before it, the drive smooths the clang into a fuzzy, synth-like tone.
- Gig Performer tip: map Frequency or Dry/Wet to an expression pedal for sweeps.
