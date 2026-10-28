# RKHarmonize

**Harmonizer (MIDI)** · MIDI family (magenta) · MIDI effect: MIDI in, MIDI out, no delay

RKHarmonize adds up to two harmony notes to every note you play on a keyboard (or any MIDI controller), in a key and scale, so the harmony follows the melody the way a second and third player would: a third above a C in C major is an E, above a D it is an F. Put it between your MIDI input and an instrument in Gig Performer.

For the header, presets and Bypass, see [Common Controls](Common%20Controls.md). As a MIDI effect it has no Dry/Wet and no Output; bypassed, MIDI passes through unchanged (and any harmony notes still sounding end first).

## Key

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Key** | C to B | C | The key's root note. |
| **Scale** | Major, Natural Minor, Harmonic Minor, Melodic Minor, Dorian, Phrygian, Lydian, Mixolydian, Major Pentatonic, Minor Pentatonic, Blues, Chromatic | Major | The scale the harmony stays in. In the seven-note scales the harmony moves by scale steps, so thirds are major or minor as the key requires. In pentatonic and blues it moves to the nearest note of the scale. **Chromatic** keeps every interval fixed (parallel harmony: a 3rd is always a major third). |

A note you play outside the scale keeps its distance from the scale note below it, so chromatic passing notes get a parallel harmony.

## Harmony

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Voice 1** | Off, Octave below, 6th, 5th, 4th or 3rd below, 3rd, 4th, 5th or 6th above, Octave above | 3rd above | The first harmony note. |
| **Voice 2** | (the same) | Off | The second. With Voice 1 a 3rd above and Voice 2 a 5th above, every note becomes a triad in the key. |
| **Velocity** | 0 to 100% | 80% | The harmony notes' velocity, as a share of the note played (so they sit a little behind the melody). |
| **Original** | Off, on | On | Plays the note you play too. Off: only the harmony sounds (for example, for a line an octave below on another instrument). |

When a harmony note lands on a note you are already playing, it isn't started twice, and releasing one key never cuts off a note another key still holds. Everything that isn't a note (sustain pedal, pitch bend, controllers) passes through.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**.

| Factory preset | Settings |
| --- | --- |
| **Thirds Above** | C major, Voice 1 a 3rd above |
| **Sixths Below** | C major, Voice 1 a 6th below |
| **Thirds and Fifths** | C major, 3rd and 5th above: triads |
| **Minor Thirds Below** | A minor, a 3rd below |
| **Octave Doubling** | An octave below, at 90% |
| **Blues Fourths** | E blues, a 4th above |

## Notes

- Set the Key and Scale to the song; with Gig Performer, store them per song or rackspace (or map Key to a widget).
- Harmonies of a single melody line sound best; chords get two more notes each and quickly crowd.
- Combine with RKArpeggiate (RKHarmonize first) for harmonized arpeggios.
