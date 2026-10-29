# RKFollow

**Pitch and expression to MIDI** · MIDI family (orchid) · the audio passes through untouched

RKFollow listens to a single line (a violin, a voice, a wind instrument, a single guitar or bass line) and turns it into MIDI, so any synth or sampler in Gig Performer plays along with you:

- **Notes:** each note you play becomes a MIDI note, with a velocity from how hard you started it.
- **Pitch bend:** your vibrato, slides and intonation come through as pitch bend, so the synth bends with you, or each note on its own channel with **MPE**.
- **Expression:** how loud you play becomes a MIDI controller (CC 11 by default), so a synth layer swells and fades with your bow or breath.

Your audio passes through **untouched**: no latency, no processing. For the header and presets, see [Common Controls](Common%20Controls.md). RKFollow has no Dry/Wet and no Output, only **Bypass**, which ends any sounding note.

## Setting it up in Gig Performer

1. Put RKFollow on the violin's (or voice's) audio, where you'd put any effect. The audio goes on to the PA as before.
2. Wire RKFollow's **MIDI output** to the MIDI input of a synth or sampler.
3. Play, and set the **Threshold** just above the noise: watch the Level meter (the white mark is the Threshold).
4. Set **Bend Range** to match the synth's pitch bend range (often ±2 semitones).

## How it follows you

- RKFollow finds the pitch of what you play and sends the **nearest semitone** as a note. Vibrato, scoops and slides don't change the note; they become pitch bend. A move of up to two semitones only changes the note once you've stayed on the new one for about 40 ms; a bigger jump changes it at once.
- **When notes start:** the pitch has to be measured first, so a note starts **15 to 30 ms** into a bowed or sung note, up to about 100 ms on a very sudden attack. That's quick enough for pads, strings and leads doubling you, but not for tight percussive synth sounds. Expression has no such delay.
- **Legato:** when you change note without a break, the new note starts before the old one ends, so mono synths glide or play legato.
- **Ending:** a note ends when you stop (6 dB under the Threshold) or the pitch can't be followed any more.
- **Single notes only.** Double stops and chords confuse the pitch tracking: RKFollow is for a single line.

## The display

- The **note** being sent, large, with the velocity and channel under it.
- The **bend** as a needle from -50 to +50 cents around the note.
- **Level:** what RKFollow hears, with the Threshold (white mark) and, when Expression is on, its Floor and Ceiling (gray marks).
- The **expression** value being sent (0 to 127).

## Tracking

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Notes** | Off, on | On | Switch off to send only expression (for example, to shape a synth you play from a keyboard with your violin's dynamics). |
| **Violin / Voice / Guitar / Bass** | | Violin | The range of notes to look for. Pick the instrument (Violin covers viola too). |
| **Threshold** | -70 to -20 dB | -50 dB | Below this level nothing is tracked. Set it just above the noise between notes. |
| **Transpose** | -24 to +24 semitones | 0 | Shifts the notes sent, for example -12 to play a bass synth an octave below your violin. |

## Notes

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Velocity Min** | 1 to 127 | 40 | The velocity of a note started at the Threshold. |
| **Velocity Max** | 1 to 127 | 127 | The velocity of a note started 36 dB above it. In between, it follows how hard you start the note. Set Min and Max the same for a fixed velocity. |
| **Retrigger** | Off, 6 to 24 dB | 12 dB | A new bow stroke or tongued note on the same pitch starts a new note when the level jumps by this much. Off: the same pitch is one long note. |

## Pitch Bend

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Off / On / MPE** | | On | **Off:** clean semitones, no bend (good for pads, keys and organ sounds). **On:** the synth follows your vibrato and slides. **MPE:** each note on its own channel (2 to 15), for MPE synths; a new note's bend never bends the previous note's release. |
| **Bend Range** | 2, 12, 24, 48 | 2 | Must match the synth's pitch bend range. MPE synths usually use 48. |
| **Glide** | 0 to 100 ms | 0 ms | Smooths the bend, for synths that sound steppy with fast vibrato. |
| **Channel** | 1 to 16 | 1 | The MIDI channel (not used in MPE mode). |

## Expression

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **On** | Off, on | On | Sends your loudness as a controller. |
| **Controller** | CC 0 to 127, Pressure | CC 11 | Which controller: CC 11 (Expression) for volume swells, CC 1 (Mod wheel) or CC 2 (Breath) for synths that map them to vibrato or brightness, CC 74 for brightness on many synths, or Channel Pressure. |
| **Floor** | -70 to -10 dB | -50 dB | Playing this quietly (or softer) sends Min. |
| **Ceiling** | -40 to 0 dB | -10 dB | Playing this loud (or louder) sends Max. |
| **Min**, **Max** | 0 to 127 | 0, 127 | The range of values sent. |
| **Curve** | -100 to +100% | 0% | 0% is even across your dynamics; positive rises quickly at first (the synth opens up early), negative saves the top for your loudest playing. |
| **Response** | 5 to 500 ms | 50 ms | How smoothly it follows: short follows every accent, long gives slow swells. |

Values are only sent when they change, at most every 5 ms, so the MIDI connection never floods.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Violin to Lead** | Violin, Bend On (±2), Expression CC 11 |
| **Violin to Strings** | Bend Off (clean notes for a pad), Velocity 60 to 110, Expression CC 11 with Response 200 ms (slow swells) |
| **Violin MPE** | Bend MPE (±48), for MPE synths |
| **Octave Down Bass** | Transpose -12, Bend Off, Retrigger 18 dB: a bass synth an octave under the violin |
| **Voice to Synth** | Voice, Threshold -45 dB, Bend On, Glide 20 ms |
| **Expression Only** | Notes off, Expression CC 11, Floor -55 dB, Ceiling -12 dB, Response 80 ms |

## Notes

- If notes flicker between two pitches on soft passages, raise the Threshold a little.
- With Bend On, a synth's own glide (portamento) on top of RKFollow's bend can make slides sound doubled; turn one of them down.
- RKFollow isn't an RKBoard pedal: the board's pedals pass audio only.
