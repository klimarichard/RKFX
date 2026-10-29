# RKTrigger: design

**Drum trigger: hits to MIDI** · MIDI family · audio passes through (or is muted)

RK + a verb, as every RKFX plugin: to *trigger* a sound. This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKTrigger turns hits into MIDI notes: a piezo on a drum or a practice pad, a drum mic, a contact mic on a cajón, or a knock on a violin's body. Each hit sends a note with a velocity from how hard it was, so a sampler or drum instrument in Gig Performer plays a kick, a clap, a sample or a loop start. Typical uses:

- **Hybrid kit:** add a sampled kick or snare layer under an acoustic drum.
- **A pad or cajón as a drum kit:** two piezos, two sounds.
- **Body tap:** a knock on the violin or guitar body triggers a percussion sample, or a footswitch-free cue.

Up to **two triggers**: the left and right inputs separately (Dual), or both summed into one (Mono).

## Signal flow (per trigger)

```
input ─► key filter (high-pass, low-pass) ─► rectify, peak envelope ─┐
                                                                      ├─► threshold ─► scan (peak of the hit) ─► velocity ─► note on
                                                                      │                         └─► mask (retrigger guard) ─► note off after Length
                              the other trigger's hits (crosstalk) ──┘
input ───────────────────────────────────────────────────────────────► audio out: Thru or Mute
```

## Detection

- **Key filter:** a high-pass (Off, or 20 to 500 Hz) and a low-pass (Off, or 1 to 16 kHz) on what the trigger listens to, not on the sound, so a kick piezo ignores cymbal ring or a snare mic ignores the kick's boom.
- **Threshold** (−60 to 0 dB): a hit starts when the filtered signal's peak crosses it.
- **Scan** (1 to 10 ms, default 2 ms): after the crossing, the trigger waits this long and takes the **highest peak** in that window as the hit's strength. The note goes out at the end of the scan. That's the only delay RKTrigger adds to the MIDI, and it is what makes velocity follow the real hit rather than the first sample over the threshold.
- **Mask** (5 to 200 ms, default 40 ms): after a hit, nothing retriggers for this long. A drum's own ringing, or a piezo's bounce, doesn't fire twice.
- **Dynamic retrigger:** during the Mask, a new hit is accepted anyway if it's clearly **louder** than the decaying previous one (above **Retrigger** dB, default 6 dB over the envelope), so fast rolls and flams still come through.
- **Crosstalk** (Dual mode, 0 to 100%, default 50%): when both triggers fire within 5 ms, the weaker one is dropped if it's below that share of the stronger one. A hit on the kick then doesn't also fire the snare piezo next to it.

## Velocity and notes

- **Velocity:** the hit's peak between the Threshold (→ **Velocity Min**) and the Threshold + **Dynamics** dB (→ **Velocity Max**, Dynamics 6 to 48 dB, default 30 dB), with a **Curve** (−100 to +100%, 0 = linear in dB). Min = Max gives a fixed velocity.
- **Note** (0 to 127, defaults 36 for trigger 1 (kick, C1), 38 for trigger 2 (snare, D1)) and **Channel** (1 to 16; default 10, the General MIDI drum channel), per trigger.
- **Note Length** (5 to 500 ms, default 50 ms): the note-off follows after this time. Most drum samplers ignore it, but some instruments need a note-off to stop.

## Audio

- **Thru** (default): the audio passes through untouched, for a drum mic that also goes to the PA.
- **Mute:** the audio is silenced (smoothly), for a piezo or contact mic that should only trigger, never be heard.

## Controls

Per trigger (`t1_`, `t2_`; trigger 2 is dimmed in Mono mode):

| Control | ID | Range | Default (1 / 2) |
| --- | --- | --- | --- |
| Threshold | `tN_threshold` | −60 to 0 dB | −30 dB |
| High-Pass | `tN_hpf` | Off, 20 to 500 Hz | Off |
| Low-Pass | `tN_lpf` | 1 to 16 kHz, Off | Off |
| Scan | `tN_scan` | 1 to 10 ms | 2 ms |
| Mask | `tN_mask` | 5 to 200 ms | 40 ms |
| Retrigger | `tN_retrigger` | Off, 3 to 24 dB | 6 dB |
| Dynamics | `tN_dynamics` | 6 to 48 dB | 30 dB |
| Curve | `tN_curve` | −100 to +100% | 0% |
| Velocity Min | `tN_vel_min` | 1 to 127 | 1 |
| Velocity Max | `tN_vel_max` | 1 to 127 | 127 |
| Note | `tN_note` | 0 to 127 | 36 / 38 |
| Channel | `tN_channel` | 1 to 16 | 10 |
| Length | `tN_length` | 5 to 500 ms | 50 ms |

Global:

| Control | ID | Range | Default |
| --- | --- | --- | --- |
| Mode | `mode` | Mono, Dual | Mono |
| Crosstalk | `crosstalk` | 0 to 100% (Dual only) | 50% |
| Audio | `audio` | Thru, Mute | Thru |

Plus **Bypass** (which ends any sounding note and passes the audio through).

## The window

- **Per trigger:** a level meter of the filtered signal with the Threshold as a line, a flash on each hit with its velocity, and the note's name ("C1 · ch 10").
- **Hit history:** a strip of the last few seconds of hits as bars, with height for velocity, so you can see double-triggers and missed soft hits while setting the Threshold and Mask.
- Sections: Trigger 1 and Trigger 2 side by side (Detection, Velocity, Note), Mode, Crosstalk and Audio in the bottom row.

## Latency

The audio has **zero latency** (Thru). Each note leaves at the end of its **Scan** time: **2 ms** by default (1 ms at the shortest). That's well under what drummers notice, and it buys a velocity that follows the real hit.

## In a rig

- **Gig Performer:** RKTrigger on the piezo's or drum mic's input; its MIDI wired to a sampler or drum plugin.
- **Format:** an audio effect with MIDI output, like RKFollow (same core changes). AU type `aumf`, AU code `Trg1`.
- **RKBoard:** not a pedal (MIDI out).

## Identity

- **MIDI family**, accent **deep orchid `#9E3C7A`**.

## Factory presets

| Preset | Idea |
| --- | --- |
| Kick Piezo | Mono, High-Pass Off, Low-Pass 2 kHz, Threshold −30 dB, Mask 50 ms, note 36 |
| Snare Piezo | Mono, High-Pass 100 Hz, Threshold −32 dB, Mask 30 ms, Retrigger 6 dB (rolls), note 38 |
| Kick and Snare | Dual, the two above on L and R, Crosstalk 50% |
| Cajón | Dual: bass (L, Low-Pass 300 Hz) note 36, slap (R, High-Pass 1 kHz) note 38, Audio Thru |
| Body Tap | Mono, High-Pass 200 Hz, Threshold −20 dB, Mask 150 ms, fixed velocity 100, note 60: a knock on the instrument as a cue |
| Pad Only | Mono, Audio Mute, Mask 25 ms, Dynamics 36 dB |

## CPU and safety

- Per trigger: two SVF stages, a peak envelope and a few comparisons per sample. Far under 0.1% of a core.
- Never a hanging note: every note-on has its scheduled note-off. Bypass, preset changes and Note or Channel changes end sounding notes first.

## Tests

- **One hit, one note:** a decaying 60 Hz burst crosses the threshold once and gives exactly one note-on and one note-off after Length, even with the ringing above the threshold for longer than the Mask.
- **Velocity:** hits at the Threshold and at Threshold + Dynamics give Velocity Min and Max; the note leaves Scan ms after the crossing.
- **Mask and retrigger:** a second hit inside the Mask is dropped when softer and accepted when 6 dB louder; outside the Mask, always accepted.
- **Key filter:** with High-Pass 100 Hz, a 50 Hz kick on the snare channel doesn't trigger.
- **Crosstalk:** a strong hit on L with a 20% copy on R gives one note (L); equal hits give two.
- **Audio:** Thru is bit-exact; Mute is silent and fades without clicks.
- **Validation:** auval (`aumf Trg1 RKMu`), pluginval at strictness 10, presets.
