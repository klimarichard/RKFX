# RKHold: design

**Note freeze / sustainer** · Delay family · zero latency on the dry signal

RK + a verb, as every RKFX plugin: to *hold* a note. This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

Press a footswitch while a note sounds, and RKHold keeps that note going, as a clean, steady drone, for as long as you like. Then play over it. Hold the open D on the violin and play a melody above it; hold a chord on a guitar and solo over it; hold a sung note as a pad behind the next phrase.

It isn't a reverb freeze (RKVerb's Freeze holds the reverb's wash) and it isn't a looper (RKLoop replays what you played, in time). RKHold holds the **sound of the note itself**, without a rhythm, until you let it go.

- **Layers:** each press can add a new layer on top of the held ones (up to four), so a chord builds up note by note. Or each press replaces what's held.
- **Octave:** a held note can sound an octave down (a bass drone under you) or up.
- The dry signal passes through untouched: RKHold adds no latency.

## How a note is held

### Capture

RKHold keeps the last **1 second** of the input in a history buffer. Pressing **Hold** takes the last **250 ms** of it: the note must already be sounding when you press (the usual way to use a freeze), and its attack is left out.

### Note mode (single notes)

For a pitched single note (violin, voice, a single guitar note), RKHold uses the shared **`core/dsp/PitchTracker`**, already running on the input, so it knows the note's period at the moment you press:

- The captured 250 ms is cut at the tracker's cycle marks into **grains of two periods** each, with a Hann window (as RKVoice's PSOLA does).
- Playback lays grains out **one period apart**, so they overlap in phase and sound as one continuous note, with no throb or comb filtering. Each new grain is taken from a **different place** in the capture (chosen at random among the cycle marks, never twice in a row). The note keeps the small movements of the real note instead of a frozen, buzzing single cycle.
- **Octave:** laying the grains out at half the period gives the octave up, and at twice the period the octave down, with the note's own timbre (PSOLA's formant-keeping shift).

### Texture mode (chords, double stops, noise)

When there's no single pitch (a guitar chord, a double stop, breath, a cymbal), the tracker can't help. RKHold then uses **granular freeze**:

- Grains of **80 ms** (Hann), taken from random places in the capture, overlapping four at a time at random offsets.
- That gives a smooth, slightly shimmering hold of any sound: the classic freeze pedal effect. It sounds a little less exact than Note mode on a single note, which is why Note mode exists.

**Mode: Auto** (default) uses Note mode when the tracker reports a steady pitch at the moment of pressing, and Texture mode otherwise. **Note** and **Texture** force one or the other.

### Fades

- A held layer fades in over **Attack** (10 ms to 2 s) and out over **Release** (50 ms to 10 s) when Hold is released (Momentary), switched off (Latch) or the layer is cleared. A long Attack gives a swell; a long Release lets the drone die away naturally.

## Layers

- **Layers: Replace** (default): each new Hold fades the previous one out (over Release) as the new one fades in. One held sound at a time.
- **Layers: Add:** each new Hold adds a layer; up to **4** sound at once. A fifth press replaces the oldest. **Clear** fades all layers out.
- Each layer is placed in the stereo field by **Width** (alternating left and right as they're added), so a built-up chord spreads out.

## Controls

| Control | ID | Range | Default |
| --- | --- | --- | --- |
| Hold | `hold` | Off, on (the footswitch) | Off |
| Hold Mode | `hold_mode` | Momentary, Latch | Latch |
| Mode | `mode` | Auto, Note, Texture | Auto |
| Range | `range` | Violin, Voice, Guitar, Bass | Violin |
| Layers | `layers` | Replace, Add | Replace |
| Clear | `clear` | momentary button | |
| Octave | `octave` | −1, 0, +1 | 0 |
| Attack | `attack` | 10 ms to 2 s | 80 ms |
| Release | `release` | 50 ms to 10 s | 600 ms |
| Movement | `movement` | 0 to 100% | 40% |
| Tone | `tone` | 1 to 20 kHz (low-pass on the hold) | 12 kHz |
| Width | `width` | 0 to 100% | 50% |
| Hold Level | `hold_level` | −60 to +6 dB | 0 dB |
| Dry Level | `dry_level` | −60 to +6 dB | 0 dB |

- **Hold Mode:** **Latch**: one press holds, the next releases (or, with Layers: Add, adds a layer; Clear releases all). **Momentary**: held while the switch is down, released when it comes up (a sustain pedal).
- **Movement:** how much the held sound keeps changing. 0% always plays the grains in the same order (steady, organ-like). Higher values pick grains more freely and add a slow, slight detune (a few cents) between overlapping grains, so the hold breathes like a real sustained note.
- **Hold Level and Dry Level** are the mix (as in RKVoice and RKOctave). There's no Dry/Wet; Output and Bypass as usual. Dry Level down to −60 dB leaves only the hold, for example when the dry signal already goes to the PA from another channel.

**Footswitch use:** in Gig Performer, map a footswitch to **Hold** (and one to **Clear** for Add mode). On **RKBoard** the same through an Assign slot; the standalone app's MIDI learn reaches it too.

## The window

- **Hold display:** up to four layer slots, each showing what it holds (the note name, with middle C = C3, or "texture"), its level as it fades, and its octave. A big **Hold** button (the footswitch on screen) and **Clear**.
- **Sections:** Capture (Mode, Range, Hold Mode, Layers), Sound (Octave, Movement, Tone, Width), Envelope (Attack, Release), Mix (Hold Level, Dry Level).

## Identity

- **AU code** `Hld1`, bundle ID `com.rkmusic.rkhold`.
- **Delay family** (with RKDelay and RKLoop: it plays back what it has heard), accent **sea green `#2E9C7A`**.
- An **RKBoard** pedal ("Hold").

## Latency and CPU

- The dry signal is **not delayed**. The hold starts the moment you press, with the Attack fade, from audio already captured.
- Per layer, Note mode overlaps two to three grains, and Texture mode four. That's at most 16 interpolated reads per sample with all four layers, plus one tracker. Aim: under 0.3% of a core.
- Memory: the 1 s history and four 250 ms captures per channel, allocated in prepare().

## Safety

- Holding silence holds silence. If Hold is pressed with nothing above the tracker's threshold, nothing is captured, and no layer starts.
- All fades are smooth (no clicks on hold, release, clear, layer replace, or octave change, which crossfades).
- Bypass fades the hold out with the usual bypass fade; switching back on starts with no layers (as every RKFX plugin starts fresh).
- Preset changes keep the held layers (so you can change the sound under a drone), but Clear and Bypass end them.

## Factory presets

| Preset | Idea |
| --- | --- |
| Violin Drone | Note mode, Latch, Replace, Attack 300 ms, Release 2 s, Movement 40% |
| Octave Drone | Note mode, Octave −1, Hold Level −3 dB: a bass note under you |
| Chord Builder | Auto, Add, Width 80%: press on each note to build a chord |
| Sustain Pedal | Momentary, Attack 20 ms, Release 400 ms: a note sustains while the switch is down |
| Texture Pad | Texture mode, Movement 70%, Tone 6 kHz, Attack 1 s, Release 4 s: an ambient freeze of anything |
| Vocal Pad | Range Voice, Note mode, Movement 60%, Tone 8 kHz, Release 3 s |

## Tests

- **Note mode holds the pitch:** a held A (440 Hz) plays at 440 Hz within 2 cents for 10 s; level steady within 1 dB (no throb).
- **Octave:** −1 and +1 give 220 Hz and 880 Hz.
- **Texture mode** holds a chord's spectrum (energy at each chord tone within 3 dB of the original) and stays steady.
- **Auto** picks Note mode on a steady tone and Texture mode on a chord or noise.
- **Layers:** Add builds up to four layers, the fifth replaces the oldest; Replace crossfades; Clear fades all out.
- **Momentary vs Latch:** the hold follows the switch in Momentary; toggles in Latch.
- **No clicks:** hold, release, clear, replace and octave change on sustained tones produce no step beyond the signal's own.
- **Dry untouched:** with Hold Level at −60 dB (or nothing held), output equals the dry input (scaled by Dry Level).
- **Silence:** Hold on silence starts no layer.
- **Presets, board, validation:** RKFXPresetTests, RKBoardTests, auval (`aufx Hld1 RKMu`), pluginval at strictness 10.

## Open questions

- **Capture length:** 250 ms is long enough for natural variation and short enough that the note's attack is usually left out. Alternatively it could be a control (100 ms to 1 s).
- **MIDI-triggered hold:** holding when a MIDI note arrives (a keyboard or a MIDI footswitch) would need MIDI input (AU music effect). Recommendation: not in v1; the Hold parameter mapped to a footswitch covers it.
