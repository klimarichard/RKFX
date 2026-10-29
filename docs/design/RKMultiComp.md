# RKMultiComp: design

**Multiband compressor** · Dynamics family · zero latency

The name RKMultiComp was chosen on purpose (an exception to the RK + verb pattern). This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKMultiComp splits the signal into **three bands** (switchable to **four**) and compresses each band on its own, then adds them back together. A boomy low end can be held in place without flattening the top, a harsh upper-mid range can be tamed only when it gets loud, and a whole mix can be glued together without the bass pumping the cymbals.

A graphic at the top shows the frequency range from 20 Hz to 20 kHz, the crossover frequencies as vertical lines, each band's threshold as a horizontal line, and each band's live level and gain reduction.

Like every RKFX plugin it has **zero latency**: no lookahead, no FFT and no linear-phase filters.

## Signal flow

```
input ──► crossover tree (Linkwitz-Riley, 24 dB/oct, phase-compensated)
            │
            ├─► Low ──────► compressor ─► gain ─┐
            ├─► Mid ──────► compressor ─► gain ─┤   (4 bands: Low-Mid and High-Mid)
            └─► High ─────► compressor ─► gain ─┤
                                                 ▼
input ──► the same allpass phase shift ──► Dry ─► Dry/Wet ──► Output, Bypass (base processor)
```

Solo and band bypass act on each band before the sum. Each band's level detector reads that band's own signal (no external sidechain in v1).

## Crossovers

### Bands and crossover frequencies

| Mode | Bands | Crossovers in use |
| --- | --- | --- |
| 3 bands (default) | Low, Mid, High | Low (150 Hz), High (3.5 kHz) |
| 4 bands | Low, Low-Mid, High-Mid, High | Low (150 Hz), Mid (1 kHz), High (3.5 kHz) |

Switching to 4 bands **inserts the Mid crossover inside the Mid band**: the 3-band Mid band becomes Low-Mid (keeping its settings), and High-Mid appears with its own settings. Low and High never change. Switching back removes High-Mid and the Mid crossover. Settings are never lost, and host mappings stay fixed (as with RKGraphEQ's independent band sets).

| Crossover | ID | Range | Default |
| --- | --- | --- | --- |
| Low | `xover_low` | 40 Hz to 1 kHz | 150 Hz |
| Mid (4 bands only, dimmed in 3-band mode) | `xover_mid` | 200 Hz to 5 kHz | 1 kHz |
| High | `xover_high` | 600 Hz to 12 kHz | 3.5 kHz |

**Ordering:** the crossovers in use are always kept at least half an octave apart, going upwards. Low stays where it is set, and the next crossover up is pushed above it if needed. The graphic shows the frequencies actually in use. The knob values are never rewritten, so nothing jumps when a knob is moved back.

### Topology: a flat sum at zero latency

Each split is the existing `core/dsp/LinkwitzRiley4` (zero-latency, 4th order). An LR4 split's low and high bands add up to a 2nd-order **allpass** at the crossover frequency: the level is flat, but the phase is shifted. With more than one split, a band that didn't pass through a given split has to get that split's allpass instead, so that every band ends up with the same total phase shift and they still add up flat:

- **3 bands:** input → split at Low → *low band* → allpass at High → **Low**; *upper band* → split at High → **Mid**, **High**.
- **4 bands:** input → split at Mid → *lower half* → allpass at High → split at Low → **Low**, **Low-Mid**; *upper half* → allpass at Low → split at High → **High-Mid**, **High**.

Every band then carries AP(Low) · AP(High) (and AP(Mid) with 4 bands), so with all compressors idle the output is a flat allpass of the input.

`StateVariableFilter` gets an **allpass output** (input − 2 × the unity-gain band-pass, Q = 0.707), and `LinkwitzRiley4` gets a matching `allpass (x)` helper, so the compensation uses the same filter core as the split.

**Crossover changes** are smoothed (the frequency glides, and coefficients are updated every 32 samples), so sweeping a crossover live never clicks. **Changing the band count** crossfades the two topologies over 20 ms (as RKParaEQ does for a mode change). The compressor state of the bands that stay is kept.

### Dry/Wet without comb filtering

Because the bands are phase-shifted, mixing them with the raw input would cause dips around the crossovers. The **dry signal goes through the same allpass chain** (two or three 2nd-order allpasses per channel), so Dry/Wet works like it does in RKCompress: parallel compression with no notches.

**Bypass** (base processor) still passes the original, unshifted input. The 10 ms bypass crossfade blends the phase-shifted and the original signal for that moment only, which is inaudible.

## Compression per band

Each band runs the RKCompress engine from `core/dsp/Dynamics.h` (`LevelDetector`, the soft-knee `compressorCurve`, and `GainSmoother` in the log domain), without RKCompress's sidechain. Nothing new is invented for the dynamics, so a band at the same settings behaves exactly like RKCompress.

Per-band controls (`<band>` = `low`, `lmid`, `hmid`, `high`). In 3-band mode the middle band uses the `lmid_` parameters and is labelled **Mid**.

| Control | ID | Range | Default (Low / Mid / High-Mid / High) |
| --- | --- | --- | --- |
| Threshold | `<band>_threshold` | −60 to 0 dB | −24 / −24 / −24 / −24 dB |
| Ratio | `<band>_ratio` | 1:1 to 20:1 | 3:1 / 2.5:1 / 2.5:1 / 2:1 |
| Attack | `<band>_attack` | 0.1 to 100 ms | 20 / 10 / 5 / 3 ms |
| Release | `<band>_release` | 10 ms to 2 s | 200 / 150 / 100 / 80 ms |
| Gain | `<band>_gain` | −12 to +24 dB | 0 dB |
| Solo | `<band>_solo` | Off, on | Off |
| Band on | `<band>_on` | Off, on | On |

- **Solo**: hear only the soloed bands (more than one can be soloed). It's a setup tool, so it is clearly marked when active, like Sidechain Listen in RKCompress.
- **Band off**: the band passes uncompressed and with no gain change, but it stays in the sum, so switching it off never leaves a hole in the spectrum.
- The slower attacks and releases in the low band are on purpose: a fast release at 60 Hz follows the waveform itself and distorts.

Global controls:

| Control | ID | Range | Default |
| --- | --- | --- | --- |
| Bands | `bands` | 3, 4 | 3 |
| Offset | `offset` | −24 to +24 dB | 0 dB |
| Knee | `knee` | 0 to 12 dB | 6 dB |
| Detection | `detect` | Peak, RMS | RMS |
| Auto release | `release_auto` | Off, on | Off |
| Stereo link | `link` | 0 to 100% | 100% |
| Auto gain | `gain_auto` | Off, on | Off |

- **Offset** moves every band's threshold together (negative = more compression). It's the one control to map to a Gig Performer widget: "more squeeze for the chorus" without touching the band balance. The graphic moves all the threshold lines with it.
- **Auto gain** adds half of each band's gain reduction at 0 dBFS back to that band (the RKCompress auto makeup rule), on top of that band's Gain.
- **Auto release** is RKCompress's program-dependent release (50 ms to 1.5 s), applied in each band.

Plus the RKFX common controls: **Dry/Wet** (`mix`, default 100%, phase-matched as above), **Output** and **Bypass**. Every control is a host parameter with a permanent ID, registered in the order above: per-band parameters (Low, Low-Mid, High-Mid, High), then crossovers, then globals, then the standard controls.

## The graphic

A display across the top of the window, on the same frequency axis and grid style as RKParaEQ's response curve (logarithmic, 20 Hz to 20 kHz). The vertical axis is **level in dBFS, −60 to 0 dB**, the same scale as the thresholds.

- **Crossovers**: vertical lines at the crossover frequencies in use, labelled with their frequency. With 3 bands the Mid line is hidden.
- **Thresholds**: in each band's region, a horizontal line at that band's threshold (including Offset), labelled with its value. A band that is off draws its line dimmed; a soloed band gets a highlighted region.
- **Live level per band**: each band's region is filled from the bottom up to the band's current level (the detector level), in a dim tint. The part between the output level and the input level, where the compressor is taking level away, is drawn in the accent colour, so gain reduction can be read straight off the graphic: a band is working when its column crosses its threshold line and turns accent-coloured at the top. The gain reduction in dB is also printed under each band.
- **Selected band**: clicking in a band's region selects it for the knob row below (see [The window](#the-window)); the selected region is outlined.

**Cost**: the static parts (grid, crossover and threshold lines, labels) are drawn into a cached image that is redrawn only when a parameter changes, as in RKParaEQ. The live columns repaint at about 30 fps, limited to the display area, from per-band atomics the audio thread already has (the detector level and gain reduction of each band, as RKCompress's meter uses). Nothing is analysed for the display, so it adds no audio-thread work.

**Not an FFT analyser.** The "spectrum" is the per-band level columns, not a continuous spectrum curve. A real analyser would need an FFT running on the message thread while the window is open. It wouldn't affect latency, but it would cost CPU on stage and goes against the lightweight goal (see [Decisions](#decisions-2026-10-29)).

### Dragging

- **Threshold lines** drag up and down and set that band's Threshold. The line shows the threshold including Offset; the stored Threshold is the line's value minus Offset. **Crossover lines** drag sideways and set that crossover. Nothing else in the graphic can be dragged. A click anywhere else in a band's region selects that band.
- Each line has a hit zone of a few pixels, and the cursor changes over it. Shift-drag is fine (as on the knobs), and double-click resets to the default, as on the knobs.
- A drag is one host gesture (begin, change, end), so it automates and undoes like turning the knob, and the knob follows.
- **Cost**: dragging only sets parameters. The audio thread sees the same smoothed parameter changes it gets from a knob, so the DSP doesn't change. On the GUI side, the cached static image is redrawn once per parameter change, at most once per display frame (about 30 fps) during a drag, and only while the mouse button is down. When nothing is being dragged, the graphic costs exactly the same as a display-only one.

## The window

About **900 × 600 px** (like RKParaEQ), scaling as usual:

1. The common header (presets).
2. **The graphic**, full width, about 230 px high.
3. **Band tabs**: Low · Mid · High (or Low · Low-Mid · High-Mid · High), each with a small gain reduction meter and its On and Solo switches. A tab and a click in the graphic select the same band.
4. **The selected band's knobs**: Threshold, Ratio, Attack, Release, Gain. One row only, so the window stays the size of the other plugins. Every band's parameters still exist as host parameters, whether shown or not.
5. **Crossover row**: Low, Mid (dimmed in 3-band mode), High.
6. **Bottom row**: Bands (3 | 4), Offset, Knee, Detection, Auto release, Stereo link, Auto gain on the left; Dry/Wet, Output, Bypass on the right, as on every plugin.

The selected band is a GUI setting (saved with the editor's state, not a parameter), so it doesn't mark a preset as modified.

## Identity

- **AU code** `Mbc1`, bundle ID `com.rkmusic.rkmulticomp`.
- **Dynamics family**, accent **dark gold `#B8860B`** (distinct from RKCompress's golden yellow and the other yellows).
- **RKBoard**: a pedal like the others ("MultiComp").

## Factory presets

Each one is a starting point in the manual.

| Preset | Bands | Idea |
| --- | --- | --- |
| Gentle Glue | 3 | Low ratios (1.5–2:1), thresholds around −20 dB, Auto gain: evens out a whole mix or a backing track |
| Bass Tighten | 3 | Low crossover 120 Hz, Low 4:1 with slow attack: the low end stays in place while the pick or fingers stay lively |
| Vocal Even | 4 | Low-Mid controls chestiness (200–1k), High-Mid tames harshness (2.5–5k) only when loud, High left open for air |
| Piezo Tame | 4 | For acoustic instruments through a pickup (with RKAcoustic): Low catches body boom, High-Mid catches piezo quack |
| Violin Smooth | 3 | High crossover 2.5 kHz, High band 3:1 with fast attack: takes the edge off bow scratch on loud passages |
| Drum Punch | 3 | Low and Mid with slow attack (30 ms) for punch, High lightly compressed; Dry/Wet 50% |

## CPU and safety

- 3 bands: two LR4 splits, one compensation allpass, two allpasses on the dry path, three compressors per channel. 4 bands: three splits, two compensation allpasses, three dry allpasses, four compressors. That's a few dozen SVF stages in total, well under 1% of one core at 48 kHz (aim: under 0.5%, measured with RKFXBench).
- Allocation-free after prepare(). Bands that are off still split (to keep the sum flat), but skip their compressor.
- No feedback paths, so nothing can run away. Gain is smoothed, crossovers glide, and the band count crossfades.

## Tests

- **Flat sum**: with every band off (and with ratio 1:1), the output's magnitude matches the input within 0.01 dB from 20 Hz to 20 kHz, in both 3- and 4-band modes, at 44.1, 48, 96 and 192 kHz.
- **Phase-matched dry**: at Dry/Wet 50% with idle compressors, the magnitude is flat (no comb dips at the crossovers).
- **Band isolation**: a tone in one band compresses that band only. The other bands' gain reduction stays at 0 dB.
- **Same as RKCompress**: a single band at given settings produces the same gain reduction as RKCompress at the same settings, on a tone inside the band.
- **Crossover ordering**: crossovers set out of order or too close are used half an octave apart, going upwards.
- **No clicks**: sweeping crossovers, switching 3 ↔ 4 bands, and toggling Solo and Band On on a sustained tone produce no discontinuity above the usual click threshold.
- **Presets**: all factory presets load (RKFXPresetTests), and the plugin is added to `RKFX_ENGINE_PLUGINS`. auval (`aufx Mbc1 RKMu`) and pluginval at strictness 10 pass.

## Decisions (2026-10-29)

- **Dragging in the graphic: yes**, a deliberate exception to RKFX's display-only graphics, as long as it costs little. Only the threshold lines (up and down) and the crossover lines (sideways) can be dragged; see [Dragging](#dragging).
- **A real spectrum analyser: not in v1.** The level columns show what the compressor reacts to, which is what matters for setting it.
- **External sidechain per band: not in v1.** It's rarely needed live, and the base processor already supports sidechains if it's wanted later.
