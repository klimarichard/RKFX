# RKVoices: design

**Harmonizer (audio, steered by MIDI)** · Modulation family · dry signal at zero latency

Working name; to be settled before the first build. This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKVoices adds up to four harmony voices to a single voice or instrument (singing, violin, sax, a single guitar line). Each voice is a pitch-shifted copy of the input, so it sounds like the player, not like a synth. Where the harmony goes is set in one of two ways:

- **Chord mode** (the reason it takes MIDI): hold a chord on a keyboard, or let RKTracks play one from a MIDI file, and each voice takes a chord tone chosen by its position relative to the note you sing: *1st above*, *2nd above*, *1st below* and so on.
- **Scale mode**: a key and scale, as in RKHarmonize, and each voice sits a diatonic interval away (a 3rd above, a 5th below). No MIDI needed.

The dry signal passes through untouched, so for the player RKVoices has no latency at all; the harmony voices trail it by a few milliseconds (see [Latency](#latency)).

## Signal flow

```
input (mono sum) ──► pitch tracker ──► sung note (quantized, with hysteresis)
     │                    │                       │
     │                    └─ cycle marks          ├─► Chord mode: rank in the held chord ──┐
     │                                            └─► Scale mode: interval in the key ─────┤
     │                                                                                      ▼
     ├────────────────► PSOLA shifter × 4 (one per voice) ◄── target note per voice, glided
     │                         │
     ▼                         ▼
   Dry level ───────────► mix (voice level, pan) ──► Output, Bypass (base processor)
```

## Pitch tracking

- The shared **`core/dsp/PitchTracker`** (zero latency, one report per cycle, as in RKOctave and RKSynth). Its cycle marks are also the grain positions the shifter needs, so tracking and shifting share one analysis.
- A new **`Range::voice`** (about 70 Hz to 1.1 kHz: a low male voice to a soprano's top) next to bass, guitar and violin. The plugin's **Range** control picks one.
- **Sung note**: the tracked frequency rounded to the nearest semitone (A4 = 440 Hz, from the host's tuning where available), with **hysteresis**: the note changes only once the pitch has been more than 70 cents from the current note for 40 ms. Vibrato and scoops then don't make the voices jump between chord tones.
- **No pitch** (silence, breath, s and t sounds, a note too noisy to track): the voices fade out over 20 ms and fade back in when the pitch is found again. The dry signal is never touched.

## Chord mode

### The held chord

- The chord is every MIDI note currently held, on any channel or the one set in **MIDI channel**. The **sustain pedal** (CC 64) keeps released notes in the chord while it's down, as on a piano.
- **Chord tones**: by default the held notes' **pitch classes in every octave** (holding C–E–G means every C, E and G), so there is always a chord tone above and below the sung note. **As voiced** uses only the exact held notes; a voice with no held note in its direction is silent.

### Voice positions

Each voice has a **Position**: Off, *3rd below*, *2nd below*, *1st below*, *1st above*, *2nd above*, *3rd above*. Counted from the sung note, strictly above or below it:

| Position | Holding C–E–G, singing D | Singing E (a chord tone) |
| --- | --- | --- |
| 1st above | E | G |
| 2nd above | G | C (an octave up) |
| 3rd above | C (an octave up) | E (an octave up) |
| 1st below | C | C |
| 2nd below | G (below) | G (below) |
| 3rd below | E (below) | E (below) |

- **A sung chord tone is skipped**: when you sing E, *1st above* is G, not a second E in unison with you.
- With a chord of one or two notes, voices land on the same note names in different octaves (holding only C, *1st above* and *1st below* are the C above and the C below). That's allowed; the voices simply double in octaves.
- **Defaults**: Voice 1 *1st above*, Voice 2 *1st below*, Voices 3 and 4 Off: a close three-part chord.

### No chord held

**No chord** sets what the voices do when no MIDI note is held (and the sustain pedal is up):

| Setting | The voices |
| --- | --- |
| **Silent** (default) | fade out (20 ms) until a chord is held again |
| Latch | keep the last chord that was held |
| Scale | follow Scale mode's key, scale and intervals until a chord is held again |

### Chord changes

When the held chord changes during a sung note, each voice moves to its new target with the **Glide** time (0 to 200 ms, default 30 ms), so a chord change mid-note sounds like singers moving, not a splice.

## Scale mode

The same key, scale and voice logic as RKHarmonize (`HarmonizeEngine::harmonyNote`, shared rather than copied, so both plugins harmonize alike): Key (C to B), Scale (Major … Chromatic) and, per voice, an **Interval** (Off, octave / 6th / 5th / 4th / 3rd below, 3rd / 4th / 5th / 6th / octave above). Default: Voice 1 a 3rd above, the others Off.

## Pitch shifting

**PSOLA** (pitch-synchronous overlap-add), the standard method for a single voice:

- Each cycle the tracker reports becomes a **grain**: two periods of the input, with a Hann window centred on the cycle mark.
- A voice lays the grains out at its **target period** (1 / target frequency) and overlaps them. Higher notes overlap the grains more closely, lower notes spread them out. A grain is reused or skipped as needed to keep up with the input.
- Because each grain keeps the input's own waveform, the voice keeps the player's **formants** (the character of the voice or instrument): up to about a 5th each way it still sounds like the player, and further it starts to sound thinner (up) or hollow (down).
- Gain and pan per voice, smoothed. The voices are the wet signal; **Dry** is the input's level in the mix (0 dB default; turn it down to hear only the harmony, for example when the dry voice already goes to the PA elsewhere).
- Optional per-voice **Detune** (±20 cents) and **Humanize** (a few ms of slowly wandering delay) so the voices don't sound locked to each other. Both default to a small amount (5 cents, 3 ms).

## Latency

RKVoices reports **zero latency** to the host: the dry signal isn't delayed, and nothing looks ahead. Each harmony voice trails the dry signal by the time PSOLA needs to collect a grain, about **one to two periods of the sung note**:

| Sung note | Period | Harmony delay |
| --- | --- | --- |
| E2 (82 Hz), a low male voice | 12 ms | 15 to 25 ms |
| A3 (220 Hz), a tenor | 4.5 ms | 6 to 9 ms |
| A4 (440 Hz), a soprano or a violin's A string | 2.3 ms | 3 to 5 ms |

That is like a second singer standing 2 to 8 metres away: heard as harmony, not as an echo. At the start of a note the voices also wait for the tracker's first two cycles (the same as RKSynth), so they enter a moment after the note's attack; for most material that reads as a natural, slightly behind backing part.

The MIDI chord adds no latency: a chord change applies from the next sample, with the Glide.

## Controls

| Control | Range | Default |
| --- | --- | --- |
| Mode | Chord, Scale | Chord |
| Range | Voice, Bass, Guitar, Violin | Voice |
| Chord tones | Any octave, As voiced | Any octave |
| No chord | Silent, Latch, Scale | Silent |
| MIDI channel | Omni, 1 to 16 | Omni |
| Key, Scale | as RKHarmonize | C, Major |
| Voice 1–4 Position (Chord mode) | Off, 3rd below … 3rd above | 1st above, 1st below, Off, Off |
| Voice 1–4 Interval (Scale mode) | as RKHarmonize | 3rd above, Off, Off, Off |
| Voice 1–4 Level | −60 to +6 dB | −3 dB |
| Voice 1–4 Pan | 100% L to 100% R | −30%, +30%, −60%, +60% |
| Detune | 0 to 20 cents | 5 cents |
| Humanize | 0 to 10 ms | 3 ms |
| Glide | 0 to 200 ms | 30 ms |
| Dry | −60 to +6 dB | 0 dB |

Plus the RKFX common controls (presets, Output, Bypass). No Dry/Wet: Dry and the voice levels are the mix, as in RKOctave. Every control is a host parameter, with permanent IDs.

**The window**: the common header, then a strip showing the **sung note**, the **held chord** and each voice's **current note** (so you can see what the harmony is doing on stage), then Mode with its settings, then four voice columns (Position or Interval, Level, Pan), then Detune, Humanize, Glide and Dry.

## In a rig

- **Gig Performer**: an audio effect with a MIDI input, like RKVocode: put it after the vocal or violin input and connect a MIDI In (a keyboard, or RKTracks' MIDI out for chords from the song) to it.
- **RKBoard**: a pedal like the others, played from the board's MIDI input.
- **With RKHarmonize**: both share the scale logic, so a keyboard part through RKHarmonize and a voice through RKVoices in Scale mode harmonize the same way.

## Limits (v1)

- **Single notes only.** A chord on a guitar or piano can't be split into notes and shifted without spectral (FFT) processing, which would add 40 ms or more and break the RKFX no-lookahead rule.
- **Noisy or breathy notes** harmonize less cleanly, because the tracker has less to go on; the voices then fade rather than glitch.
- **No formant control** in v1 (shifting formants independently of pitch, for a gender change, needs spectral processing).
- **No MIDI out** of the harmony notes in v1 (a later option: send them to a synth too).

## CPU and safety

- Four voices of PSOLA plus one tracker: a few hundred operations per sample, well inside the RKFX budget (aim: under 1% of one core at 48 kHz, like the other pitch plugins).
- Allocation-free after prepare(); grain buffers sized for the lowest note of the widest range (two periods at 35 Hz).
- No runaway: no feedback paths. Voices fade on lost pitch, on Bypass and on preset changes.

## Tests

- **Chord ranks**: the table above, plus two-note and one-note chords, notes at the top and bottom of the MIDI range, As voiced with nothing above or below (voice silent), the sustain pedal holding released notes.
- **No chord**: Silent fades out; Latch keeps the last chord; Scale follows the key.
- **Hysteresis**: a sung note with ±60-cent vibrato never changes target; a real step of a semitone does, within 40 ms.
- **Shifting accuracy**: a sine and a sawtooth shifted by −12 … +12 semitones come out within 5 cents of the target, with the input's harmonic balance kept (formants).
- **Latency**: the harmony's delay against the dry signal is within the table's range for E2, A3 and A4; the dry path is sample-identical to the input.
- **Lost pitch**: noise and silence fade the voices out, with no clicks (maximum step).
- **Scale mode**: matches `HarmonizeEngine::harmonyNote` for every key, scale and interval.

## Open questions

- The name (RKVoices, RKChoir, RKHarmony …).
- Whether Range needs a separate *Voice* entry, or the *Guitar* range (60 Hz to 1.1 kHz) covers singing well enough.
- Whether four voices are right, or two are enough for most songs (each voice is a column on the screen and a few host parameters).
