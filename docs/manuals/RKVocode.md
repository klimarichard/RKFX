# RKVocode

**Channel vocoder** · Filter and EQ family (indigo violet) · zero latency

RKVocode makes an instrument talk: it takes the *shape* of your voice (the modulator) and imposes it on another sound (the carrier), from the classic robot voice to a synth choir that sings your words. The carrier can be a built-in oscillator (on a fixed note, or played polyphonically from a MIDI keyboard) or any other signal sent to the plugin's sidechain input.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKVocode has no Dry/Wet; use **Mod Mix** to blend in your voice.

## How it works

1. Your voice (the plugin's main input) is split into many frequency **bands**, and the level of each band is followed over time.
2. The **carrier** is split into the same bands, and each carrier band is turned up and down to match the voice. The result has the carrier's pitch and tone with your voice's vowels and consonants.
3. **Unvoiced** adds back the "s", "t" and "f" sounds, which have no pitch and would otherwise get lost.

The carrier's own spectrum is evened out band by band, so the result follows your voice clearly whatever carrier you use.

## Vocoder

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Bands** | 8, 16, 24, 32 | 16 | Number of bands. Fewer bands: a rougher, more robotic, vintage sound. More bands: clearer, more intelligible words. |
| **Low** | 50 to 500 Hz | 100 Hz | The lowest band. |
| **High** | 4 to 12 kHz | 8 kHz | The highest band. |
| **Formant** | -12 to +12 semitones | 0 | Shifts the vowel character without changing the pitch: up for smaller, brighter, "chipmunk" vowels; down for bigger, darker, giant-like ones. |
| **Attack** | 1 to 50 ms | 5 ms | How quickly each band follows your voice getting louder. Short for crisp consonants. |
| **Release** | 10 to 500 ms | 50 ms | How quickly each band follows your voice getting quieter. Longer is smoother and more legato; shorter is more articulate. |
| **Unvoiced** | 0 to 100% | 30% | Adds your voice's "s", "t", "sh" sounds (and noise shaped by them) for intelligibility. |
| **Mod Mix** | 0 to 100% | 0% | Blends your original voice into the output. |

## Carrier

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Sidechain / Internal** | Sidechain, internal | Internal | **Internal**: the built-in oscillator. **Sidechain**: any signal sent to the plugin's sidechain input (typically a synth or organ; rich, bright sounds work best). With nothing connected, there's no output. |
| **Saw / Pulse / Noise / Saw+N** | Saw, pulse, noise, saw plus noise | Saw | The internal oscillator's waveform. **Saw**: bright, classic vocoder. **Pulse**: hollower, more electronic. **Noise**: whispered, pitchless. **Saw+N**: saw with breath. |
| **MIDI / Fixed** | MIDI, fixed | Fixed | **Fixed**: the oscillator plays one note all the time (instant robot voice, no keyboard needed). **MIDI**: notes played on the plugin's MIDI input set the pitches; play chords for a vocoder choir. With no notes held, it's silent. |
| **Note** | C1 to C6 | C3 | The pitch in Fixed mode (C3 = middle C, as in Studio One). |
| **Voices** | 1 to 8 | 8 | How many MIDI notes can sound at once. Set 1 for monophonic lead lines. |
| **Glide** | 0 to 500 ms | 0 ms | Pitch glide between notes, for smooth, sliding lead lines. |

Unused carrier controls are dimmed (for example Note in MIDI mode, or the oscillator controls when the sidechain is the carrier).

### Using RKVocode in Gig Performer

- **Voice:** wire your microphone to the plugin's main input.
- **MIDI mode:** route your keyboard's MIDI to the plugin (it appears as an effect that accepts MIDI).
- **Sidechain mode:** wire the carrier instrument's audio to the plugin's sidechain input pins.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Robot Voice** | Internal, Saw, Fixed C3, 16 bands |
| **Classic Synth Vocoder** | Internal, Saw, MIDI, 16 bands, Unvoiced 40% |
| **Vocoder Choir** | Internal, Saw+N, MIDI, 24 bands, Release 150 ms |
| **Deep Robot** | Internal, Pulse, Fixed C2, Formant -3 |
| **Whisper** | Internal, Noise, 24 bands, Unvoiced 60% |
| **Sidechain Synth** | Sidechain carrier, 32 bands, Unvoiced 30% |
| **Bright Formant Lead** | Internal, Saw, MIDI, Voices 1, Glide 60 ms, Formant +7 |

## Notes

- Speak or sing close to the mic, with clear consonants: the vocoder can only follow what it hears.
- A compressor (RKCompress) before RKVocode keeps the voice level steady for a more even result.
- Gig Performer tip: map Formant to an expression pedal for dramatic vowel sweeps.
