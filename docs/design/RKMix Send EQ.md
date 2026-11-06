# RKMix Send EQ: design

**A small 4-band EQ on every channel's send to every Aux and FX bus** · zero latency, free while off, switched on per channel per bus

A channel's sound in an in-ear mix often wants to differ from the room's: the bass and kick without their sub in the singer's in-ear, the guitar's low end out of the keyboard player's mix, a hard-edged hi-hat softened for the drummer. Today that means a whole bus EQ (it changes every channel in the mix) or the channel EQ (it changes the room too). The send EQ shapes **one channel in one mix**. This is a design, not a manual: it says what the feature does and how, so it can be built and reviewed against it.

## Suite rules, and what it costs

- **Zero latency.** The bands are plain filters (biquads), with no latency and nothing to align: RKMix's latency plan doesn't change.
- **Free while off.** A send whose EQ is off takes exactly today's path. The block's check is one flag per active send, already known when `ActiveSends` is built.
- **No allocation on the audio thread.** The filter state for every channel × bus pair is a fixed-size member, set up in `prepare`.
- **The cost while on,** estimated from the recorded benchmark (RKParaEQ, 7 bands with 48 dB cuts: 1.7 µs per 64-sample block, 0.13% of a core): a 4-band stereo send EQ is roughly 0.03–0.04% of a core at 48 kHz / 64. Sixty of them (six in-ear mixes, ten channels each) are about 2–3%; all 512 (32 channels × 16 buses), unlikely in practice, about 15–20%. The first build step adds benchmark cases to measure this for real.

## The bands

Fixed shapes, for a quick tweak on a phone, not a studio EQ:

| Band | Shape | Range | Default |
| --- | --- | --- | --- |
| **Low cut** | High-pass, 12 dB/oct | Off, 20 to 400 Hz | Off |
| **Low** | Low shelf | 40 to 500 Hz, ±15 dB | 150 Hz, 0 dB |
| **Mid** | Bell, Q 0.5 to 4 | 200 Hz to 8 kHz, ±15 dB | 1 kHz, 0 dB, Q 1 |
| **High** | High shelf | 2 to 16 kHz, ±15 dB | 8 kHz, 0 dB |

- **On** switches the whole EQ (per channel per bus). Turning it on with defaults is flat: nothing changes until a band moves.
- Low cut **Off** (the bottom of its range) skips the filter.
- A band at 0 dB is skipped (the strip EQ does the same), so a send EQ that only cuts the lows costs one filter.
- Use the strip EQ's filters (`MixFilters.h`, the core's matched bell and shelf), so the send EQ sounds like the channel EQ and is drawn by the same curve code.

## Where it sits

```
channel ─► ... tap point ─┬─► [send EQ, this channel × this bus] ─► send gain, pan ─► bus
                          └─► (other sends at the same tap, without EQ: today's shared path)
```

- **After the tap, before the send's gain and pan.** It works at every tap point: Input, Pre-EQ, ... Post-Fader.
- **Aux and FX buses**, from channels. Group assigns have no send gain and stay as they are. Bus-to-bus sends (an FX return into an Aux mix) are not in this version.
- A **stereo pair** has one EQ (one set of settings) with two sides of filter state.
- **In the engine:** `sendAtTap` today adds the tapped signal straight into each bus (`addToBus`), and every send at the same tap shares that source. A send with EQ on copies the tapped signal into a scratch buffer (one per engine, mono or stereo, block-sized, allocated in `prepare`), filters it there, then calls `addToBus` with that buffer. Sends without EQ are untouched.
- **Mute and solo:** unchanged. The EQ sits before the gain stages; a muted channel is still silent in every send.

## Settings

Per channel (32) × bus (16), in `MixSettings`, named like the existing send settings (see `channelSendId` / `channelSendPanId` in `MixParams.h`): `on`, `lc` (low cut), `lsf`, `lsg`, `mf`, `mg`, `mq`, `hsf`, `hsg`. That's 9 × 512 = **4608 settings**.

- `ownerBits`: a send EQ setting belongs to its **channel** (a change rebuilds only that channel's send state, as a send level does today).
- **Presets:** only non-default values are written, so a mix without send EQs doesn't grow.
- **Scenes:** they store it, and the recall scope puts it in **Sends to B#** (with that bus's send levels and send pans). `MixScenes::classify` must classify every new ID; its test covers every ID already, so it fails until the new ones are classified. Glide: gains and frequencies glide like the strip EQ; On switches at once (with the filter state reset and a short crossfade, so turning it on never clicks).
- **Undo:** tracked like every mix setting (`MixUndo` polls them).
- **Copy and paste:** right-click a send EQ, **Copy send EQ** / **Paste**; a paste on a ganged strip goes to the same bus on every ganged strip. Plus **Paste to all sends to this bus** for the same cut on every channel in one mix.
- **Check in step 1:** 4608 more parameters in a processor that never processes. Measure the mix's load time, the memory, and `MixUndo`'s 10 Hz poll before and after. If any of them grows noticeably, fall back to a compact store for the send EQs (one atomic block per pair, outside the parameter list) and give the knobs a small adapter. Decide from the numbers, not in advance.

## On the console

- **Sends page:** under each Aux and FX send, next to its tap and pan, a small **EQ** button: lit when on, with a tiny curve drawn in it when it isn't flat. Click it for a popover with the curve, the four bands and On (the strip EQ's controls, smaller).
- **Sends on fader:** each strip gets the same small EQ button above its fader, for that bus's send.
- The H G E C lights don't change (they're the strip's).

## On the remotes

**Monitor remote (the musicians' phones):** next to each channel's name, an **EQ** button (lit when on). Tap it for a sheet with the curve, finger handles for the four bands (drag sideways for frequency, up and down for gain; Mid's Q with a pinch or a slider), On, and **Flat**. It works only on the sends to **that phone's own bus**, exactly what the phone may already change (its send levels and pans), so the permissions are the same as today (the pages have no PIN; Monitor EQ was kept off them for that reason, but this changes nothing outside the musician's own mix). Fine and Lock apply to the sheet too.

- Protocol: the send messages gain a type, e.g. `{t:"seq", ch, band, f, g, q}` and `{t:"seq", ch, on}`, applied like a send level (into undo and the active scene). The mix sent to a page (`MixRemote`'s mix JSON) carries each channel's send EQ for that bus.
- The curve drawing is the console remote's EQ curve with handles (`strip.js`), moved to `rk.js` if both pages need it.

**Console remote (iPad):** the same button on the Sends page and on sends on fader.

## Tests

- Off: the output is bit-identical to today's (one send, many sends, every tap).
- On and flat: identical within float rounding.
- Each band: the response at a few frequencies matches the filter's design (a low cut at 100 Hz is -3 dB there, 12 dB/oct below).
- A pair: both sides filtered alike; a mono bus sums after the EQ.
- Only the one send changes: the same channel's other sends, and the main, are untouched.
- Scenes: stored, recalled, scoped with Sends to B#, glided; Undo takes it back; presets keep it.
- The remote: a phone's message changes only its own bus; a message for another bus is refused.
- Benchmark: idle (no send EQ on) must not move; cases for 1, 60 and 512 send EQs on.

## Build order

1. **Engine:** the filter state, the scratch path in `sendAtTap`, the settings (and the parameter-count check above). Tests and benchmarks.
2. **Scenes, undo, presets, copy and paste.**
3. **Console:** the Sends page popover and sends on fader.
4. **Monitor remote,** then the console remote.
5. **Manual** (`docs/manuals/RKMix.md`: Sends and the remote sections), then the README line.

## Not in this version

- EQ on bus-to-bus sends (a Group or FX return into an Aux mix).
- A send compressor or limiter.
- More bands or other shapes.
