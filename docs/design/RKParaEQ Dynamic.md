# RKParaEQ Dynamic: design

**Dynamic EQ bands in RKParaEQ** · Filter and EQ family · zero latency

This adds dynamics to RKParaEQ's five gain bands (Low, Low Mid, Mid, High Mid, High), so a band can move by itself depending on how loud its frequency range is. It isn't a new plugin: a band with **Dynamic** off is exactly today's band, and existing sessions and presets sound the same. This is a design, not a manual: it says what the feature does and how, so it can be built and reviewed against it.

## What it does

Each gain band gets a **Dynamic** switch. When it's on, the band listens to its own frequency range and adds a moving amount of gain to its static **Gain**:

- **Range below 0 dB (cut):** when the band's level goes **above** the Threshold, the band is turned down, by at most the Range. This compresses that range only when it gets loud: harshness on loud vocal notes, boom on a low note that rings, sibilance.
- **Range above 0 dB (boost):** when the band's level falls **below** the Threshold, the band is turned up, by at most the Range. This lifts a range only when it's quiet: presence on a soft vocal line, body on a quiet passage.
- **Range at 0 dB:** no dynamics.

Both directions even out the dynamics of that range (downward and upward compression), so one knob covers both. Boosting louder signals or cutting quieter ones (expansion) is left out of v1; see [Decisions](#decisions-2026-10-29).

The HP and LP cut filters have no gain, so they stay as they are.

## Signal flow

```
input ─► HP ─┬─► Low ─► Low Mid ─► Mid ─► High Mid ─► High ─► LP ─► Output, Bypass
             │    ▲        ▲        ▲        ▲          ▲
             └─► detectors (one per dynamic band, after HP, before the bands)
```

- **Where the detectors listen:** to the signal after the high-pass and before the five bands, all of them to the same point. The EQ's own cuts and boosts then don't move each other's thresholds, and rumble the HP removes doesn't trigger the Low band.
- **What each detector hears**, a filter matching the band's shape at its frequency:
  - **Bell:** band-pass at the band's frequency and Q (the unity-gain SVF band-pass, Q limited to 0.5 to 10, so a very wide bell doesn't hear the whole spectrum);
  - **Low shelf:** low-pass (12 dB/oct) at the shelf frequency;
  - **High shelf:** high-pass (12 dB/oct) at the shelf frequency.
- **Level:** RMS over about 10 ms (`Dynamics::LevelDetector`), stereo-linked (the louder channel counts, so the stereo image never shifts).

## The gain computer

For a dynamic band with Threshold *T*, Ratio *R* and Range *G* (all in dB), and detector level *L*:

- **Cut (G < 0):** over = *L* − *T*. The dynamic gain is −over × (1 − 1/R), never below *G*, and 0 below the threshold. This is RKCompress's curve with a floor, with a fixed 6 dB soft knee around the threshold.
- **Boost (G > 0):** under = *T* − *L*. The dynamic gain is under × (1 − 1/R), never above *G*, and 0 above the threshold.
- **The boost window:** a boost only acts on levels down to 30 dB below the threshold, and fades back to 0 over the next 10 dB. Silence, hiss, amp noise and room rumble between notes get no boost. This is built in, not a control.

The dynamic gain is then smoothed with **Attack** (moving away from 0) and **Release** (moving back to 0), in the log domain (`Dynamics::GainSmoother`, as in RKCompress; Auto release isn't offered here).

**Total gain** of the band = its static **Gain** + the dynamic gain. The shape and Q stay as set. So a bell at +3 dB with Range −6 dB moves between +3 and −3 dB.

## Filter updates

Today a band's coefficients are recalculated only while a knob glides, every 32 samples. A dynamic band's gain moves all the time, so:

- A dynamic band recalculates its coefficients **every 16 samples** (0.33 ms at 48 kHz), from the smoothed gain. The matched bell and shelf designs (`MatchedDesign`) are cheap enough for that, and the transposed direct form II `Biquad` handles coefficient changes at that rate without clicks when the gain moves smoothly.
- **Attack is at least 1 ms**, so the gain never jumps by more than a fraction of a dB between updates.
- A dynamic band is never skipped as "flat", even with Gain at 0 dB.
- **Switching Dynamic on or off** glides the dynamic gain to or from 0 over the release time, so it never clicks. Changing Range's sign (cut ↔ boost) does the same through 0.

## Controls

Appended per band (`<band>` = `low`, `lowmid`, `mid`, `highmid`, `high`), after every existing parameter, in band order. IDs are permanent; with the defaults below an old session or preset loads with Dynamic off and sounds unchanged, so no state migration is needed.

| Control | ID | Range | Default |
| --- | --- | --- | --- |
| Dynamic | `<band>_dyn` | Off, on | Off |
| Threshold | `<band>_threshold` | −60 to 0 dB | −30 dB |
| Range | `<band>_range` | −24 to +24 dB (0 = none) | −6 dB |
| Ratio | `<band>_ratio` | 1.5:1 to 10:1 | 3:1 |
| Attack | `<band>_attack` | 1 to 100 ms | 10 ms |
| Release | `<band>_release` | 10 ms to 2 s | 150 ms |

Range defaults to −6 dB rather than 0, so switching Dynamic on does something straight away (a gentle cut when loud).

## The window

Today's window is 900 × 630, with seven strips holding every knob. The dynamics controls go in **one row for the selected band**, as in RKMultiComp, so the strips stay as they are:

1. **Strips:** each gain band's title row gets a small **Dyn** switch next to **On**. A strip with Dynamic on gets an accent-coloured title. Clicking a gain band's title selects it (HP and LP can't be selected).
2. **The dynamics row**, a new section under the strips, titled after the selected band ("Mid dynamics"): **Dynamic**, **Threshold**, **Range**, **Ratio**, **Attack**, **Release**, and a **level meter**. The meter is a vertical bar showing the band's detector level against a mark at the Threshold, plus the current dynamic gain in dB. It's the tool for setting the threshold: play loud, and watch where the bar reaches. With Dynamic off, the row's knobs are dimmed but usable.
3. The window grows to about **900 × 770**.

The selected band is a GUI setting held by the processor while the plugin is loaded (as in RKMultiComp), not a parameter.

**On RKBoard** the pedal editor is scaled to fit its panel, so the taller window will show smaller there than today (roughly three quarters of its size instead of nearly full size).

## The response curve

- **The static curve**, as today: the EQ with every dynamic gain at 0. It is redrawn only when a setting changes.
- **The live curve:** when any band has Dynamic on, a second curve shows the EQ as it is right now, including the moving dynamic gains. It's drawn brighter, with the static curve dimmed under it, and repainted at about 30 fps, only while a dynamic gain is actually moving by more than 0.1 dB. When nothing moves, nothing is redrawn.
- **Range markers:** at each dynamic band's frequency, a thin vertical bracket spans from its static gain to its static gain + Range, showing how far that band can move.
- The curve stays **display-only**, as today (see [Decisions](#decisions-2026-10-29)).

**Cost:** the live curve needs the magnitude of up to five biquads at about 450 points per frame. That runs on the message thread, only while something moves and only while the window is open. The audio thread just publishes each band's current dynamic gain and detector level as atomics.

## Factory presets

The five existing presets stay unchanged. New ones (also the starting points in the manual):

| Preset | Idea |
| --- | --- |
| Vocal Harshness Tamer | High Mid bell 3.2 kHz, Q 2, dynamic, Range −6 dB, Threshold set for loud notes: harsh only when loud, untouched when soft |
| Soft De-Ess | High shelf 6.5 kHz, dynamic, Range −8 dB, Ratio 4:1, Attack 1 ms, Release 60 ms: a gentle de-esser inside the EQ |
| Bass Boom Control | Low bell 110 Hz, Q 1.5, dynamic, Range −6 dB, Attack 20 ms: one ringing low note stops jumping out |
| Presence Lift | High Mid bell 2.5 kHz, dynamic, Range +4 dB, Ratio 2:1: soft vocal lines come forward, loud ones stay as they are |
| Acoustic Body Control | Low Mid bell 220 Hz with a static −2 dB, dynamic, Range −4 dB: tames boom on a pickup or close mic when played hard |

## CPU and safety

- Per dynamic band and channel: one SVF detector stage, an RMS detector, the gain computer and smoother, and one coefficient design every 16 samples. That's on top of the biquad already running. With all five bands dynamic the aim is **under 0.3% of one core** at 48 kHz (measured with RKFXBench, as for RKMultiComp). Bands with Dynamic off cost exactly what they cost today.
- Allocation-free after prepare(). No feedback paths. The dynamic gain is capped by Range and smoothed, and the boost window keeps silence from being lifted.
- Zero latency: no lookahead, so a very fast transient can pass for up to the attack time before the band moves (as with every RKFX compressor).

## Tests

- **Unchanged when off:** with Dynamic off on every band, the output matches today's engine sample for sample (the existing ParaEQ tests also pass unchanged), and every existing factory preset loads cleanly.
- **Cut:** a tone at a dynamic bell's frequency, above the threshold, is reduced by the expected amount (ratio), and never by more than the Range. A tone below the threshold is untouched.
- **Boost:** a quiet tone below the threshold is lifted by the expected amount, never more than the Range. A tone above the threshold is untouched. Silence and a tone 45 dB under the threshold get no boost (the window).
- **Frequency selectivity:** a loud tone two octaves away from a dynamic bell doesn't move it.
- **Shelves:** low and high shelves react to their own side of the frequency only.
- **No clicks:** a sustained tone pumped over and under the threshold, Dynamic toggled, Range swept from −24 to +24 dB, and the band switched between shelf and bell produce no step larger than the signal's own.
- **Curve:** the live curve's magnitude matches the measured response with a dynamic band held at a known gain.
- **Board and presets:** RKBoardTests and RKFXPresetTests pass; auval (`aufx Peq1 RKMu`) and pluginval at strictness 10.

## Decisions (2026-10-29)

- **No dragging on the curve:** it stays display-only, as today.
- **Above/Below switch:** not in v1 (it can be appended later).
- **Per-band Listen:** not in v1. The level meter in the dynamics row covers setting the threshold.
