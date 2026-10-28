# RKSynth

**Instrument synth** · Modulation family (cobalt) · zero latency

RKSynth turns the notes you play into an analog-style synthesizer: play a bass and get a synth bass line, play a guitar or a violin and get a synth lead. It follows your pitch, plays a saw, square or triangle wave at that pitch (or an octave or two below), and shapes it with a resonant filter that opens on every note, the classic synth "bwow".

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md). Dry/Wet blends your instrument under the synth (100% = synth only).

## How it works

- RKSynth listens for the pitch of each note. **It plays single notes**: chords and double stops confuse it (as with every guitar or bass synth that needs no special pickup).
- A note sounds as soon as its pitch is known, two cycles into the note. That is about 50 ms on a bass's low E, about 25 ms on a guitar's low E, and under 10 ms higher up. This is not latency (the sound isn't delayed once it plays); it is how long the lowest notes take to reveal their pitch. On bass, play the low notes a touch early, or use the octave above with **Octave -1**.
- When you play notes legato (without a pause), the synth glides from one to the next over the **Glide** time. After a pause, a note starts right on its pitch.
- When the note ends (the level falls below the **Threshold**), the synth stops.
- On every new note (after a pause, or a fresh attack within a phrase) the filter envelope opens the filter and lets it close again over the **Decay** time.

## Oscillator

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Bass / Guitar / Violin** | Bass (35 to 400 Hz), Guitar (60 Hz to 1.1 kHz), Violin (125 Hz to 2.7 kHz) | Guitar | The range of notes RKSynth listens for. Choose your instrument: the right range tracks more reliably. |
| **Saw / Square / Tri** | Saw, square, triangle | Saw | The wave. **Saw**: bright and buzzy, the classic synth bass and lead. **Square**: hollow, woody, a little like a clarinet. **Triangle**: soft and round, almost a flute. |
| **-2 / -1 / 0 / +1** | Octaves | 0 | Plays the synth one or two octaves below what you play, or one above. |
| **Sub** | 0 to 100% | 0% | Adds a square wave one octave below the synth, for weight. |
| **Glide** | 1 ms to 1 s | 10 ms | How long the synth takes to slide to a new note when you play legato. Short values just smooth the change; long ones give the classic portamento slides. |
| **Dynamics** | 0 to 100% | 70% | How much the synth's level follows your playing. 0%: every note at the same level, like a keyboard synth. 100%: as soft or loud as you play. |
| **Threshold** | -70 to -20 dB | -50 dB | Below this level the synth stays silent. Raise it if string noise or the end of a note makes the synth sputter; lower it if soft notes don't sound. |

## Filter

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Cutoff** | 50 Hz to 10 kHz | 1.2 kHz | The filter's resting position (24 dB/oct low-pass). Lower is darker and rounder. |
| **Resonance** | 0 to 100% | 30% | Emphasis at the cutoff: higher values give the squelchy, vocal synth sound. |
| **Env Amount** | 0 to 100% | 50% | How far the filter opens on each note, up to 4 octaves above the cutoff. 0% leaves the filter still. |
| **Decay** | 20 ms to 2 s | 300 ms | How quickly the filter closes again after each note starts. Short: a plucky "bwow"; long: a slow sweep. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Synth Bass** | Bass, Saw, Sub 50%, Cutoff 400 Hz, Resonance 40%, Env Amount 60%, Decay 250 ms |
| **Funky Filter Bass** | Bass, Square, Cutoff 250 Hz, Resonance 60%, Env Amount 90%, Decay 150 ms, Dynamics 50% |
| **Deep Sub Bass** | Bass, Triangle, Octave -1, Sub 30%, Cutoff 300 Hz, Env Amount 10%, Dynamics 40% |
| **Saw Lead** | Guitar, Saw, Glide 30 ms, Cutoff 1.5 kHz, Resonance 35%, Env Amount 50%, Decay 400 ms |
| **Violin Synth Lead** | Violin, Saw, Glide 40 ms, Cutoff 2 kHz, Env Amount 30%, Decay 600 ms, Dynamics 90% |
| **Soft Synth Pad** | Guitar, Triangle, Glide 120 ms, Cutoff 2.5 kHz, Env Amount 0%, Dynamics 100%, Dry/Wet 60% |

## Notes

- Place RKSynth early, on a clean signal: after a compressor or RKAcoustic is fine, after distortion, chorus, delay or reverb it can't track. Effects that shape the synth (chorus, delay, reverb, even distortion) go after it.
- On bass, the neck pickup and playing with the fingers track best; mute the strings you aren't playing.
- For a synth bass under a band, try Dry/Wet around 70% to keep some of the real bass's attack, or 100% for a pure synth line.
- Gig Performer tip: map Cutoff or Env Amount to an expression pedal for filter sweeps while you play.
