# RKCompress

**Compressor** · Dynamics family (golden yellow) · zero latency

RKCompress evens out the level of a signal: when it gets louder than the threshold, it is turned down by an amount set by the ratio. It's a clean, transparent compressor for vocals, bass, drums and any other source, with an external sidechain for ducking (for example, a vocal turning a backing track down), parallel compression via Dry/Wet, a transfer curve and a gain reduction meter.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md).

## How the signal flows

1. A **detector** listens to the input (or to the external sidechain) and measures its level.
2. If the level is above the threshold, the **gain computer** works out how much to turn it down, according to the ratio and knee.
3. **Attack and release** smooth that gain change in time.
4. The gain change is applied to the signal, then **Makeup** brings the level back up.
5. **Dry/Wet** blends with the original (parallel compression), then **Output** sets the level.

There's no lookahead, so the compressor adds no latency.

## Display

- **Transfer curve:** input level (left to right) against output level (bottom to top), from -60 to 0 dB. The diagonal gray line is "no compression"; the colored curve shows what RKCompress does, including the knee and makeup. A white dot moves along the curve showing the current input level.
- **GR meter:** how much the signal is being turned down right now, from 0 to -24 dB.

Both update about 30 times per second and only redraw their own small area.

## Compressor

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Threshold** | -60 to 0 dB | -18 dB | The level where compression starts. Lower = more of the signal gets compressed. |
| **Ratio** | 1:1 to 20:1 | 4:1 | How strongly the level above the threshold is reduced. 2:1 is gentle (every 2 dB above becomes 1 dB), 4:1 is firm, 10:1 and up acts almost like a limiter. 1:1 does nothing. |
| **Knee** | 0 to 12 dB | 6 dB | How gradually compression starts around the threshold. 0 dB is a hard knee (sudden, punchy); higher values ease in smoothly (more natural on vocals). |
| **Attack** | 0.1 to 100 ms | 10 ms | How fast the compressor reacts once the level goes over. Short attack catches peaks; longer attack lets the initial transient (pick, stick, consonant) through for more punch. |
| **Release** | 10 ms to 2 s | 150 ms | How fast the level recovers after it drops. Too short can cause distortion or "breathing"; too long keeps the signal turned down. Dimmed when Auto is on. |
| **Auto** (release) | Off, on | Off | Program-dependent release: recovers quickly (about 50 ms) after short peaks and slowly (up to about 1.5 s) during sustained loud passages. A good set-and-forget choice. |
| **Peak / RMS** | Peak, RMS | RMS | How the level is measured. **RMS** follows the average loudness (smooth, musical, good for vocals and bass). **Peak** reacts to every instant peak (tighter control of transients, good for drums). |

## Sidechain

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Int / Ext** | Internal, external | Int | What the detector listens to. **Int**: the signal being compressed. **Ext**: the plugin's sidechain input, so another signal controls the compression (ducking). If nothing is connected to the sidechain input, Ext does no compression at all. |
| **High-Pass** | Off, or 40 to 300 Hz | Off | Filters the lows out of what the detector hears (not out of the sound), so bass and kick drum don't make the compressor pump. Turn fully left for Off. |
| **Link** | 0 to 100% | 100% | Stereo link. 100%: both channels are turned down together, following whichever is louder, so the stereo image stays stable. 0%: each side is compressed on its own. |
| **Listen** | Off, on | Off | For setting up only: the output becomes what the detector hears (after Int/Ext and High-Pass). While on, the switch lights orange and the transfer curve shows **SIDECHAIN LISTEN**. Don't forget to switch it off! |

### Using the sidechain in Gig Performer

RKCompress has a second stereo input, labeled Sidechain. In Gig Performer, wire the signal that should trigger the compression (for example, your vocal mic) to the plugin's sidechain input pins, and set the source to **Ext**. The main input carries the signal to be turned down (for example, a backing track or pad).

## Bottom row

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Makeup** | 0 to 24 dB | 0 dB | Turns the compressed signal back up to make up for the lost level. Dimmed when Auto is on. |
| **Auto** (makeup) | Off, on | Off | Sets the makeup automatically from Threshold, Ratio and Knee (half the reduction a full-scale signal would get, which matches the perceived loudness without overshooting). |

Plus **Dry/Wet** (default 100%), **Output** and **Bypass**. Turning Dry/Wet down blends the uncompressed signal back in: "parallel" or "New York" compression, which keeps the natural transients while adding density.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Smooth Vocal** | Threshold -20 dB, Ratio 3:1, Knee 9 dB, Attack 10 ms, Release Auto, RMS, Makeup Auto |
| **Bass Evener** | Threshold -24 dB, Ratio 4:1, Knee 6 dB, Attack 20 ms, Release 150 ms, RMS |
| **Parallel Drum Punch** | Threshold -30 dB, Ratio 8:1, Knee 0, Attack 5 ms, Release 80 ms, Peak, Makeup Auto, Dry/Wet 40% |
| **Acoustic Guitar** | Threshold -18 dB, Ratio 2.5:1, Knee 9 dB, Attack 15 ms, Release Auto, RMS, High-Pass 100 Hz |
| **Sidechain Ducking** | Source Ext (vocal into the sidechain), Threshold -35 dB, Ratio 3:1, Attack 10 ms, Release 300 ms, High-Pass 150 Hz |

## Notes

- Watch the GR meter: 3 to 6 dB of reduction is typical for gentle control; more is a deliberate effect.
- At very short attack times on low notes, any compressor without lookahead slightly distorts the waveform; raise Attack a little if bass sounds fuzzy.
- Gig Performer tip: map Threshold to a widget to push a vocal into more compression for loud song parts, or Dry/Wet for adjustable parallel compression.
