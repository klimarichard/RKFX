# RKFollow: design

**Pitch and expression to MIDI** · MIDI family · audio passes through untouched

RK + a verb, as every RKFX plugin: the synth *follows* you. This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKFollow listens to a single line (a violin, a voice, a wind instrument, a single guitar or bass line) and turns it into MIDI, so any synth or sampler in Gig Performer plays along with you:

- **Notes:** each note you play becomes a MIDI note, with a velocity from how hard you started it.
- **Pitch bend:** vibrato, slides and intonation come through as pitch bend, so the synth bends with you instead of jumping in semitones. Optionally as **MPE**, one channel per note.
- **Expression:** how loud you play becomes a MIDI controller (CC 11 by default), so a synth layer swells and fades with your bow or breath, like a breath controller.

The audio passes through **untouched** (no latency, no processing), so RKFollow can sit right on the instrument's channel. Its MIDI goes to any synth's MIDI input. Expression can also run on its own (Notes off), to shape a synth played from a keyboard with your violin's dynamics.

## Signal flow

```
input ──────────────────────────────────────────────► audio out (unchanged)
  │
  ├─► mono sum ─► pitch tracker ─► note (semitone, hysteresis) ─► note on/off, pitch bend ─┐
  │                    │                                                                     ├─► MIDI out
  └─► level (RMS) ─────┴─► onset / velocity ─► gate ─────────────────────────────────────────┤
                       └─► expression (floor..ceiling, curve, smoothing) ─► CC ─────────────┘
```

## Pitch tracking

- The shared **`core/dsp/PitchTracker`** (zero latency, a report per cycle), as in RKOctave, RKSynth and RKVoice, with the same **Range** choices: Violin (default), Voice, Guitar, Bass.
- **Note:** the tracked frequency rounded to the nearest semitone (A = 440 Hz, MIDI note 69), with RKVoice's **hysteresis**: a move of up to 2 semitones only changes the note once the pitch has been more than 70 cents away for 40 ms, so vibrato and scoops don't flip notes. Bigger moves (a new note) switch at once.
- **No pitch** (below the Threshold, noise, breath): no note.

## Notes

- **Note on:** once the tracker has confirmed a pitch (two cycles that agree, as in RKVoice) and the level is above the **Threshold**. The note is the semitone plus **Transpose** (−24 to +24, so a violin can drive a bass synth an octave down).
- **Velocity:** from the level in the note's first 10 ms. A level at the Threshold gives **Velocity Min**, and 36 dB above it gives **Velocity Max**, in between on a dB scale. Velocity Min = Max gives a fixed velocity.
- **Note changes (legato):** when the sung or played note changes without a break, the new note-on is sent **before** the old note-off. Mono synths then glide or play legato, as with a keyboard player's overlapping fingers.
- **Note off:** when the level falls 6 dB below the Threshold, or the pitch is lost for 2.5 periods (as in RKVoice).
- **Retrigger:** a new bow stroke or tongued note on the **same** pitch starts a new note when the level jumps by the **Retrigger** amount (Off, or 6 to 24 dB, default 12 dB) within 20 ms.
- **Never a hanging note:** Bypass, a preset change, a Range or Transpose change, and silence all send the note-off first.

### Pitch bend

**Bend** sets how the pitch between semitones comes through:

| Bend | What the synth does |
| --- | --- |
| Off | Plays clean semitones (a quantized line, good for pads and keys sounds) |
| On (default) | Follows vibrato, slides and intonation with pitch bend around the current note |
| MPE | As On, but each note gets its own channel (2 to 16, the MPE lower zone, with an MPE configuration message), so a bend on a new note never bends the previous note's release tail |

- **Bend Range** must match the synth: ±2 (default for On), ±12, ±24 or ±48 semitones (default for MPE, the MPE standard).
- The bend is the tracked pitch minus the current note, sent at most every 2 ms and only when it changes by a cent or more. A slide beyond the Bend Range moves to a new note (legato, as above).
- **Glide** (0 to 100 ms, default 0) smooths the bend, for synths that sound steppy with a fast bend.

### MIDI out

- **Channel** 1 to 16 (default 1). In MPE mode notes rotate over channels 2 to 15, with 1 as the master channel.

## Expression (CC)

The loudness of what you play, as a controller:

| Control | Range | Default |
| --- | --- | --- |
| Expression | Off, on | On |
| Controller | CC 0 to 127, or Channel Pressure | CC 11 (Expression) |
| Floor | −70 to −10 dB | −50 dB (the level that sends the minimum) |
| Ceiling | −40 to 0 dB | −10 dB (the level that sends the maximum) |
| Min, Max | 0 to 127 | 0, 127 |
| Curve | −100% (slow start) to +100% (fast start) | 0% (linear in dB) |
| Response | 5 to 500 ms | 50 ms (smoothing: short follows every accent, long swells) |

- The level is the same RMS the notes use. Between Floor and Ceiling it maps to Min..Max through the Curve.
- **Sent only when it changes**, at most every 5 ms, so it never floods the MIDI connection.
- On the note channel (or the MPE master channel). With **Notes Off** (see below), only expression is sent.
- Common choices: CC 11 (Expression) for volume swells, CC 1 (Mod wheel) or CC 2 (Breath) for synths that map them to vibrato or brightness, CC 74 for brightness on many synths.

## Controls

| Control | ID | Range | Default |
| --- | --- | --- | --- |
| Notes | `notes` | Off, on | On |
| Range | `range` | Violin, Voice, Guitar, Bass | Violin |
| Threshold | `threshold` | −70 to −20 dB | −50 dB |
| Transpose | `transpose` | −24 to +24 semitones | 0 |
| Velocity Min | `vel_min` | 1 to 127 | 40 |
| Velocity Max | `vel_max` | 1 to 127 | 127 |
| Retrigger | `retrigger` | Off, 6 to 24 dB | 12 dB |
| Bend | `bend` | Off, On, MPE | On |
| Bend Range | `bend_range` | 2, 12, 24, 48 | 2 |
| Glide | `glide` | 0 to 100 ms | 0 ms |
| Channel | `channel` | 1 to 16 | 1 |
| Expression | `expr` | Off, on | On |
| Controller | `expr_cc` | 0 to 127, Pressure | 11 |
| Floor | `expr_floor` | −70 to −10 dB | −50 dB |
| Ceiling | `expr_ceiling` | −40 to 0 dB | −10 dB |
| Min | `expr_min` | 0 to 127 | 0 |
| Max | `expr_max` | 0 to 127 | 127 |
| Curve | `expr_curve` | −100 to +100% | 0% |
| Response | `expr_response` | 5 to 500 ms | 50 ms |

Plus **Bypass** (which ends any sounding note). No Dry/Wet and no Output: the audio is passed through as it is.

## The window

- **Note display:** the current note's name, large (with the octave), the bend as a cents needle (−50 to +50 around the note, or the slide in semitones), and the last velocity.
- **Level meter** with marks at the Threshold, and at the Expression Floor and Ceiling.
- **Expression bar:** the controller value being sent (0 to 127).
- **Sections:** Tracking (Notes, Range, Threshold, Transpose), Notes (Velocity Min/Max, Retrigger), Bend (Bend, Bend Range, Glide, Channel), Expression (on, Controller, Floor, Ceiling, Min, Max, Curve, Response).

## Latency

The audio has **zero latency**. A note starts once the tracker has confirmed its pitch: **15 to 30 ms** into a bowed or sung note, up to about 100 ms on an instant, full-level attack (the same as RKVoice, which shares the tracker). That's the time a synth layer under you needs to enter, and is fine for pads, strings and leads that double you. It isn't fast enough to play percussive synth sounds tightly from the violin. Expression has no tracking delay, only its Response time.

## In a rig

- **Gig Performer:** RKFollow on the violin's (or voice's) audio channel; its MIDI output wired to the synth's MIDI input. The violin goes on to the PA as before.
- **Format:** an audio effect with MIDI output. AU type `aumf` (music effect), like RKVocode, AU code `Flw1`. VST3 has a MIDI event output. (To verify in Gig Performer with both formats; VST3 is the safe one.)
- **RKBoard:** not a pedal. The board's pedals only pass audio.

## Identity

- **MIDI family**, accent **orchid `#CC6BA8`**.

## Core changes

- **`PluginConfig::midiOutput`** (an audio plugin that also sends MIDI) and a matching `MIDI_OUTPUT` option in `rkfx_add_plugin` (NEEDS_MIDI_OUTPUT, AU `aumf`).
- **`ProcessorBase::sendMidi (message, sampleInChunk)`** for processDsp(): the base places it in the host's MIDI buffer at the right position (the chunk offset added). It is cleared of the input MIDI first, as these plugins take no MIDI input.
- **Bypass** for such plugins calls `releaseMidi()` first, as MIDI effects already do, so notes end when bypassed.

## Factory presets

| Preset | Idea |
| --- | --- |
| Violin to Lead | Violin, Bend On ±2, Glide 0, Expression CC 11 |
| Violin to Strings | Bend Off (clean notes for a pad), Velocity 60 to 110, Expression CC 11 with Response 200 ms (slow swells) |
| Violin MPE | Bend MPE ±48, for MPE synths |
| Octave Down Bass | Range Violin, Transpose −12, Bend Off, Retrigger 18 dB |
| Voice to Synth | Range Voice, Threshold −45 dB, Bend On ±2, Glide 20 ms |
| Expression Only | Notes Off, Expression CC 11, Floor −55, Ceiling −12, Response 80 ms |

## CPU and safety

- One tracker, a few envelopes, and MIDI only on change: about RKSynth's cost without its oscillators (aim: under 0.2% of a core).
- No hanging notes (see Notes), MIDI rate-limited, allocation-free after prepare() (MIDI written into the host's buffer).

## Tests

- **Notes:** a sine sweep through semitones gives the right note numbers (with Transpose), each on, then off; legato order (new on before old off).
- **Hysteresis:** ±40 cent vibrato on A 440 Hz stays one note, with the bend following it; a 3-semitone jump switches at once.
- **Bend:** a note 30 cents sharp sends the matching bend for ±2 and ±48 ranges; no bend messages when Bend is Off.
- **MPE:** consecutive notes on rotating channels 2..15, configuration message first.
- **Velocity:** soft and loud onsets map to Min and Max; Min = Max gives a fixed velocity.
- **Retrigger:** a 12 dB jump on the same pitch gives off then on; a 6 dB swell doesn't (at 12 dB).
- **Expression:** a level ramp from Floor to Ceiling sends Min to Max, monotonic, rate-limited, only on change; Channel Pressure when chosen.
- **Never hanging:** bypass mid-note, silence mid-note, Range change mid-note all end with every note off.
- **Audio untouched:** output equals input, bit for bit.
- **Validation:** auval (`aumf Flw1 RKMu`), pluginval at strictness 10, presets.

## As built (2026-10-29)

- **Pitch measured over ~12 ms of cycles, not one cycle.** The tracker gives each cycle's length in whole samples: fine for RKVoice's shifter, but too coarse for MIDI (at 2 kHz, a sample is about 50 cents, so notes would flicker and the bend would be off). RKFollow measures the time across the cycles of the last 12 ms (at least two), which puts the pitch within about 2 cents.
- **First notes and jumps are confirmed.** A note's first cycles read flat or sharp while the tracker's filter settles, which sent a wrong note (a semitone off) for ~40 ms before the right one. A first note or a jump now needs three cycles agreeing within 1.5% on it, and the tracker's own (filter) pitch agreeing too. Measured: a note starts 28 ms into an A 440 Hz; a jump of three semitones lands 16 ms after it is played.
- **Core:** as designed. `sendMidi()` also drops anything sent while bypassed, so nothing can start after the bypass note-offs.
- **Note names** with middle C = C3 (Yamaha, Cubase), the suite's convention: A 440 Hz shows as A3.
- **Window 1000 × 630.** CPU measured 0.08% of one core (RKFXBench).

## Decisions (2026-10-29)

- **Name:** RKFollow.
- **No polyphony** in v1: chords and double stops would need spectral analysis (FFT, latency), outside RKFX's rules.
