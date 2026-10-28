# RKOctave

**Octave down** · Modulation family (royal blue) · zero latency

RKOctave adds one or two octaves below what you play, like a classic analog octave pedal: a violin becomes a viola or a cello, a guitar gets a bass line under it, a bass gets a deep sub. It reacts instantly, with no delay at all, so it plays as tightly as the dry sound.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKOctave has no Dry/Wet: **Direct** and the two octave levels are the mix.

## How it works

- RKOctave listens for the pitch of the note you play (its lowest frequency, the fundamental) and counts its cycles.
- Every second cycle makes one cycle **one octave down** (**Octave 1**); every fourth makes one cycle **two octaves down** (**Octave 2**).
- **Natural** keeps the sound of your instrument: it is your own signal, folded down an octave. **Synth** replaces it with a pure, smooth tone at the same pitch, like an analog synth's sub-oscillator, and follows your playing's volume.
- Below the **Threshold**, the octaves fade out, so noise and the end of a note never turn into growling low notes.

**Play one note at a time.** Like every analog octave pedal, RKOctave follows single notes. Chords and double stops confuse it and make it warble; that can be a sound, but not a clean one.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Bass / Guitar / Violin** | Bass, guitar, violin | Guitar | The range of notes RKOctave listens for. **Bass**: 35 to 400 Hz. **Guitar**: 60 Hz to 1.1 kHz (also drop-C tunings). **Violin**: 125 Hz to 2.7 kHz (E7), including the low C string of a 5-string violin or a viola. Choose the instrument you play: the right range tracks more cleanly. For viola, use Violin; for cello, Guitar. |
| **Natural / Synth** | Natural, synth | Natural | **Natural**: your own sound an octave down, a little gritty, like classic octave pedals. **Synth**: a round, pure sub tone. |
| **Direct** | 0 to 100% | 100% | The level of your original signal. At 0% you hear only the octaves. |
| **Octave 1** | 0 to 100% | 80% | Level of the octave below. |
| **Octave 2** | 0 to 100% | 0% | Level of two octaves below. |
| **Tone** | 200 Hz to 8 kHz | 1.5 kHz | Brightness of the octaves (24 dB/oct low-pass). Lower is rounder and darker, higher keeps more of the instrument's character. It doesn't change the Direct sound. |
| **Threshold** | -70 to -20 dB | -50 dB | Below this level the octaves fade out. Raise it if noise or note tails make the octaves growl; lower it if soft notes get no octave. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Classic Octave** | Guitar, Natural, Direct 100%, Octave 1 80% |
| **Violin to Viola** | Violin, Natural, Direct 100%, Octave 1 70%, Tone 2.5 kHz |
| **Violin to Cello** | Violin, Natural, Direct 70%, Octave 1 70%, Octave 2 50%, Tone 1.2 kHz |
| **Synth Sub** | Guitar, Synth, Direct 100%, Octave 1 70%, Tone 600 Hz |
| **Bass Sub** | Bass, Synth, Direct 100%, Octave 1 60%, Tone 300 Hz |
| **Octave Only** | Guitar, Natural, Direct 0%, Octave 1 100% |

## Notes

- Place RKOctave **first**, or right after a tuner or compressor: it tracks best on a clean signal. After distortion, reverb, chorus or delay, it gets confused.
- A distortion *after* RKOctave turns it into an octave fuzz.
- Use the neck pickup on guitar, and play cleanly: string noise and open strings ringing on make it warble.
- Gig Performer tip: map Octave 1 to a widget or an expression pedal to bring the lower voice in and out.
