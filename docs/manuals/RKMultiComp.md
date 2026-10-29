# RKMultiComp

**Multiband compressor** · Dynamics family (dark gold) · zero latency

RKMultiComp splits the sound into three frequency bands (or four) and compresses each band on its own. A boomy low end can be held in place without flattening the top, a harsh upper-mid range can be tamed only when it gets loud, and a whole mix can be evened out without the bass making the cymbals pump. Each band is the same compressor as [RKCompress](RKCompress.md).

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md).

## How the signal flows

1. **Crossovers** split the signal into bands: Low, Mid and High (or Low, Low-Mid, High-Mid and High in 4-band mode).
2. Each band has its own **compressor**: when the band gets louder than its threshold, it is turned down by its ratio, with its own attack and release.
3. Each band's **Gain** sets its level after compression.
4. The bands are added back together. With every band at rest they add up to exactly the original sound (the same level at every frequency).
5. **Dry/Wet** blends with the original (parallel compression), then **Output** sets the level.

There's no lookahead and no linear-phase filtering, so RKMultiComp adds no latency. The crossovers shift the phase a little around each crossover frequency (all bands the same way, and the dry signal too), which is inaudible on its own and keeps Dry/Wet free of comb-filter dips.

## The display

Frequency runs from 20 Hz on the left to 20 kHz on the right (like RKParaEQ), and level from -60 dB at the bottom to 0 dB at the top.

- **White vertical lines:** the crossovers, labeled with their frequency.
- **Colored horizontal lines:** each band's threshold (including Offset), labeled in dB. A band that is switched off draws its line in gray.
- **Columns:** each band's level, live. When the column rises above the threshold line, the part the compressor is taking away is drawn in solid color at the top, and the band's gain reduction is printed under its name.
- **Outline:** the selected band, whose knobs are shown below. A soloed band is lightly tinted.

**You can drag the lines:** drag a threshold line up or down to set that band's threshold, or drag a crossover line sideways to move it. Hold Shift for fine adjustment, and double-click a line to reset it, as on the knobs. Clicking anywhere else in a band selects it. The display only redraws the columns while you play; the rest is redrawn only when a setting changes.

## Bands

Below the display, a **tab per band** shows its name and a slim gain reduction bar along its bottom edge. Click a tab (or a band in the display) to show that band's knobs.

| Control | Range | Default (Low / Mid / High-Mid / High) | What it does |
| --- | --- | --- | --- |
| **On** | Off, on | On | Switched off, the band passes uncompressed and with no Gain change, but it stays in the sound. |
| **Solo** | Off, on | Off | Hear only the soloed bands (more than one can be soloed). For setting up: it lights orange. |
| **Threshold** | -60 to 0 dB | -24 dB | Where compression starts in this band. |
| **Ratio** | 1:1 to 20:1 | 3:1 / 2.5:1 / 2.5:1 / 2:1 | How strongly the band is turned down above the threshold. |
| **Attack** | 0.1 to 100 ms | 20 / 10 / 5 / 3 ms | How fast the band reacts once it goes over. |
| **Release** | 10 ms to 2 s | 200 / 150 / 100 / 80 ms | How fast it recovers. Dimmed when Auto Release is on. |
| **Gain** | -12 to +24 dB | 0 dB | The band's level after compression: makeup, or a tone shaping tool (turn a band down for a duller sound). |

The low band reacts more slowly by default on purpose: a fast release on deep notes follows the waveform itself and distorts.

## Crossovers

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Low** | 40 Hz to 1 kHz | 150 Hz | Between Low and Mid (or Low-Mid). |
| **Mid** | 200 Hz to 5 kHz | 1 kHz | Between Low-Mid and High-Mid. 4-band mode only (dimmed with 3 bands). |
| **High** | 600 Hz to 12 kHz | 3.5 kHz | Between Mid (or High-Mid) and High. |

The crossovers always stay in order, at least half an octave apart: if one is set at or below the one under it, it is used half an octave above that one (the display shows where it actually is). Moving a crossover glides smoothly, so it can be swept while playing.

## Bottom row

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Bands** | 3, 4 | 3 | Switching to **4** splits the Mid band in two at the Mid crossover: Low-Mid keeps the Mid band's settings and High-Mid has its own. Switching back to **3** joins them again. Nothing is lost either way, and the switch crossfades so it never clicks. |
| **Offset** | -24 to +24 dB | 0 dB | Moves every band's threshold together. Negative = more compression everywhere. |
| **Knee** | 0 to 12 dB | 6 dB | How gradually compression starts around the thresholds (all bands). |
| **Link** | 0 to 100% | 100% | Stereo link: at 100% both channels of a band are turned down together; at 0% each side on its own. |
| **Detection** | Peak, RMS | RMS | How the band levels are measured, as in RKCompress. RMS is smooth and musical; Peak reacts to every transient. |
| **Auto Release** | Off, on | Off | Program-dependent release in every band (fast after short peaks, slow during sustained loud passages). |
| **Auto Gain** | Off, on | Off | Adds back half of the reduction a full-scale signal would get, per band, on top of each band's Gain. |

Plus **Dry/Wet** (default 100%), **Output** and **Bypass**. Turning Dry/Wet down blends the uncompressed signal back in (parallel compression). The dry signal gets the same phase shift as the bands, so the blend sounds smooth at every setting.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Gentle Glue** | 3 bands; thresholds -20 dB; ratios Low 1.8:1, Mid 1.5:1, High 1.5:1; Knee 9 dB; Auto Release; Auto Gain. Evens out a whole mix or a backing track. |
| **Bass Tighten** | 3 bands, Low crossover 120 Hz; Low -26 dB 4:1, attack 30 ms; Mid -22 dB 2:1; High -20 dB 1.5:1; Auto Gain. The low end stays in place, the pick or fingers stay lively. |
| **Vocal Even** | 4 bands at 200 Hz / 2.5 kHz / 5 kHz; Low -26 dB 2:1; Low-Mid -24 dB 3:1 (chestiness); High-Mid -28 dB 4:1, attack 2 ms (harshness, only when loud); High off (air left open); Knee 9 dB. |
| **Piezo Tame** | 4 bands at 250 Hz / 1 kHz / 3.5 kHz; Low -28 dB 4:1 (body boom); Low-Mid 1.5:1; High-Mid -26 dB 3:1, attack 3 ms (piezo quack); High 1.5:1. Pairs well with [RKAcoustic](RKAcoustic.md). |
| **Violin Smooth** | 3 bands at 200 Hz / 2.5 kHz; Low 1.5:1; Mid -22 dB 2:1; High -28 dB 3:1, attack 1 ms, release 60 ms. Takes the edge off bow scratch in loud passages. |
| **Drum Punch** | 3 bands; Low and Mid with 30 ms attack (punch through), Low -28 dB 4:1, Mid -26 dB 3:1; High -22 dB 2:1; Peak; Auto Gain; Dry/Wet 50%. |

## Notes

- Start with one band at a time: Solo it, find the threshold where its column just crosses the line on loud notes, then un-solo and set the others.
- 2 to 4 dB of reduction per band is typical for gentle control; more is a deliberate effect.
- Gig Performer tip: map **Offset** to a widget for "more squeeze in the chorus" without changing the band balance, or **Dry/Wet** for adjustable parallel compression.
- On **RKBoard** it's the **MultiComp** pedal.
