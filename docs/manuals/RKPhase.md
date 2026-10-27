# RKPhase

**Phaser** · Modulation family (pale blue) · zero latency

RKPhase is a classic phaser: it sweeps a set of notches up and down through the sound, giving the swirling, "whooshing" movement known from the famous orange and script pedals and from 70s electric piano and string-machine sounds. The sweep can come from an LFO, from your playing dynamics (envelope), or both.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md). A mono input gives a stereo output, so Stereo Phase works on mono sources.

## How the signal flows

1. The input passes through a chain of **allpass stages**. On their own they don't change the tone, only the timing (phase) of different frequencies.
2. When this phase-shifted signal is mixed with the original (**Dry/Wet**), frequencies where the two are opposite cancel out: those are the notches.
3. The stages' frequency is swept around the **Center** by the **LFO** and the **Envelope**; their movements add together.
4. **Feedback** sends part of the output back into the chain for a more resonant, vocal sweep (a soft limiter keeps it under control).

Every two stages make one notch: 4 stages give 2 notches, 12 stages give 6.

## Phaser

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Stages** | 2, 4, 6, 8, 12 | 4 | Number of allpass stages. 2 is subtle, 4 is the classic pedal sound, 8 to 12 are deep and dramatic. Changing it crossfades, so it never clicks. |
| **Center** | 100 Hz to 5 kHz | 800 Hz | The middle of the sweep. Low settings give a darker, throatier phase; high settings a brighter, airier one. |
| **Feedback** | -95% to +95% | +30% | Resonance. Positive values make the notches sharper and the sweep more vocal and "singing"; negative values give a different, hollower character. |

## LFO

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Depth** | 0 to 100% | 70% | How far the LFO sweeps, up to two octaves either side of Center. At 0% the LFO is off (its other controls are dimmed). |
| **Rate** (Sync off) | 0.02 to 10 Hz | 0.5 Hz | Speed of the sweep. Slow (0.1 to 0.5 Hz) is lush; faster (2 to 6 Hz) is a bubbly, Leslie-like swirl. |
| **Rate** (Sync on) | 1/1 to 1/32, dotted (D) and triplet (T) | 1/2 | Length of one sweep as a note value, locked to the beat when the host transport runs. |
| **Stereo Phase** | 0 to 180° | 90° | Offsets the right channel's sweep. 0° sweeps both sides together; 90° to 180° makes the movement travel across the stereo field. |
| **Sync** | Off, on | Off | Off: Rate in Hz. On: Rate as a note value following the song tempo. |

## Envelope

The envelope moves the sweep with your playing dynamics, like an auto-wah: play harder and the notches move further.

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Amount** | -100% to +100% | 0% | How far the envelope moves the sweep, up to about 4 octaves. Positive moves it up when you play louder, negative moves it down. At 0% the envelope is off (its other controls are dimmed). |
| **Sensitivity** | -24 to +24 dB | 0 dB | Matches the envelope to your source: raise it for quiet sources, lower it for hot ones. |
| **Attack** | 1 to 100 ms | 5 ms | How fast the sweep follows when you play harder. |
| **Release** | 20 ms to 1 s | 150 ms | How fast the sweep returns as notes fade. |

The envelope listens to whichever stereo channel is louder, so both sides move together.

Plus the standard **Dry/Wet** (default 50%, the deepest notches), **Output** and **Bypass**. At 100% Wet there are no notches, only a subtly phase-shifting signal.

## Starting points

| Sound | Settings |
| --- | --- |
| Classic orange-pedal phase (guitar) | 4 stages, Center 700 Hz, Feedback +20%, Depth 80%, Rate 0.4 Hz |
| Electric piano swirl | 4 stages, Center 1 kHz, Feedback +40%, Depth 70%, Rate 0.6 Hz, Stereo Phase 120° |
| Deep string-machine phase | 8 stages, Center 600 Hz, Feedback +50%, Depth 90%, Rate 0.15 Hz |
| Funky envelope phaser | 4 stages, Center 400 Hz, Feedback +50%, Depth 0%, Env Amount +80%, Attack 5 ms, Release 150 ms |
| Synced pulsing phase | 6 stages, Center 900 Hz, Feedback +30%, Depth 70%, Sync on, 1/8 |
| Fast rotary-style swirl | 4 stages, Center 1.2 kHz, Feedback +10%, Depth 50%, Rate 5 Hz, Stereo Phase 180° |

## Notes

- High Feedback can boost the resonant frequencies; lower Output if needed.
- Gig Performer tip: map Rate to a widget to speed the phaser up during a song, or Env Amount to switch between a sweeping and a dynamic, playing-controlled phaser.
