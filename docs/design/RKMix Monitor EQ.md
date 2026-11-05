# RKMix Monitor EQ: design

**Measure a wedge or an in-ear mix with pink noise, and get a 31-band RKGraphEQ curve for its Aux bus** · zero latency, free when not measuring

RKMix sends pink noise out of one Aux bus (a wedge or an in-ear mix) and listens on one input channel: a mic in front of the wedge, or an ear simulator (coupler) holding the in-ear. It compares the two band by band. The difference against a target curve becomes a correction, which RKMix writes into an RKGraphEQ insert on that bus, adding one if there isn't one. For in-ears without a coupler, a published measurement file can be loaded instead. It runs from the console and from the iPad remote, so the user can walk the mic positions with the iPad in hand. This is a design, not a manual: it says what the feature does and how, so it can be built and reviewed against it.

Two **types** share one engine and one panel: **Speaker** (wedges, side fills) and **In-ear**. The type sets the targets, the limits and the level; everything else is the same.

## Suite rules, and what idle costs

The measurement must not change RKMix when it isn't running:

- **Zero latency, always.** Nothing is added to any signal path; the reported latency never changes because of this feature. The analysis runs beside the audio, never in a path anyone listens to.
- **No FFT, no block analysis.** Per-sample band filters at RKGraphEQ's 31 ISO centres, so the bands match its sliders exactly.
- **Idle = one atomic read per block.** The engine reads a single "measuring" state once per block. When it is off, no noise is generated, no analysis filter runs, no measuring state is touched; each bus only compares its number with -1. The CPU benchmark at 48 kHz / 64 samples is recorded before and after; the idle figure must not move.
- **No allocation on the audio thread.** All state (noise generator, 2 × 31 band filters, sums) is a fixed-size member, set up in `prepare`.
- **No locks.** The audio thread keeps running sums per band, published under a sequence counter (a seqlock of relaxed atomics); the message thread reads them on a timer (about 10 Hz) and does all the maths (averaging, mic curve, target, limits).
- **Even while measuring it is cheap:** one pink noise generator and 124 biquads in double precision (31 bands × 2 sections, on the noise and on the mic), mono.

## Signal flow while measuring

```
pink noise ─► level ─► [Aux bus input, replacing its sends] ─► bus DSP and inserts (incl. RKGraphEQ) ─► fader at 0 dB ─► output ─► wedge / in-ear
     │                                                                                                                         │
     └─► 31 band filters ─► sum of squares per band (reference)                                          mic / coupler ◄───────┘
                                                                                                                │
                                        input (straight from the interface, before trim) ─► 31 band filters ─► sum of squares per band
```

- **Where the noise goes in:** at the bus's input, replacing everything sent to it, so the noise passes the bus's EQ, compressor and inserts. The existing RKGraphEQ curve is therefore part of what is measured, and the result is a change on top of it (measure, apply, measure again to check).
- **Taking over the bus:** on Start the bus's own signal fades out and its fader and mute fade to 0 dB (unity) over 50 ms; the noise then fades in over 1 s. On Stop the noise fades out over 50 ms and the bus fades back to its own signal and fader over 50 ms. The measurement's own Level sets the loudness. Other buses and the main carry on as they were.
- **Sides:** a stereo bus gets the noise on both sides, or (In-ear) on the left or right side only, the other side silent, so each ear is measured on its own.
- **Other inserts on the bus** (a limiter, a compressor) are left in. On steady pink noise they change the level of all bands about equally, which doesn't change the curve. The panel warns if the bus has a dynamics insert.
- **Mic tap:** the input straight from the interface, before trim, EQ and inserts.
- **Band filters:** each band is two identical 2nd-order band-passes in series (Q 2.78, so the pair is −3 dB at the band edges, 1/6 octave each side). Their overlap smooths the result a little, the same for noise and mic, so it cancels. Bands above 0.45 × the sample rate (20 kHz at 44.1 kHz) are not measured.
- **Stopping at once:** Stop, closing the panel, a scene recall, and the remote that started it disconnecting for more than 2 s.

## Measuring

1. **Noise floor:** 3 s with the bus taken over but no noise. The input's band levels are stored as the floor.
2. **Each position:** 5 s of noise, the first 0.5 s ignored so the room settles. Per band: the input's level minus the noise's level, in dB.
3. **Positions:** 1 to 8, averaged as power, not dB. Speaker suggests 4: around ear height where the performer stands, pointed at the wedge, 50 cm to 1 m away. In-ear suggests 2 (seated twice in the coupler), so a bad fit shows.
4. **Signal check:** a band whose input level was less than 10 dB above the floor is **no data** and gets no correction. An input that clips stops the position with a warning.

The pink noise is Paul Kellet's refined filter on white noise (the one RKMix's oscillator uses), its own generator, seeded at each Start. Its exact spectrum doesn't matter: the reference is measured from the same noise.

## Mic curves

The measured response minus the mic's own response is the wedge's response. Choices:

- **Flat (measurement mic)**
- **Shure SM58**, **Shure SM57**, **sE Electronics V7:** 31 band values each, read from the manufacturer's published typical response charts. Approximate (units vary by about ±2 dB), which is fine for a 1/3-octave graphic EQ. They are shown to the user for review before they ship.
- **Load calibration file…:** the plain text file that comes with measurement mics and couplers (miniDSP UMIK, Dayton, IEC 711 clones): lines of frequency and dB; `*` and `"` lines are comments; a sensitivity line is ignored. Interpolated in log frequency to the 31 bands. Stored with the RKMix settings (the values, not the path).

**Compensation is capped at +6 dB.** A band where the curve is more than 6 dB down (a dynamic mic above about 12–15 kHz) is **low confidence**: its correction is halved and drawn faded.

Speaker reminds the user to keep cardioid mics 50 cm to 1 m away: closer, their proximity effect raises the bass and the EQ would cut bass the wedge doesn't have.

## In-ear

An in-ear's sound depends on the sealed space between the earpiece and the eardrum, so a mic in front of it says nothing useful. In-ear works two ways:

- **Coupler measurement:** an IEC 711-style ear simulator (a cavity the size of an ear canal with a measurement mic in it, the cheap clones are fine) on an input, with its calibration file as the mic curve. The flow is the same as for a wedge, with no room and no reflections.
- **Load measurement file…:** no hardware. A published coupler measurement of the in-ear model (frequency and dB, the same file reader as the calibration file) is used as the response; no noise is played. Universal fits only roughly: custom moulds and fit change it, so the result is a starting point.

What is different from Speaker:

- **Targets:** flat in a coupler sounds dull and thin, so the in-ear targets are what a speaker in a room gives at the eardrum:
  - **Diffuse field:** the ISO 11904-2 diffuse-field response (a broad rise of about 10 dB at 2.5–3 kHz).
  - **Speakers in a room:** diffuse field with a gentle downward tilt above 1 kHz (−1 dB per octave) and a bass shelf below 150 Hz (4th order, gone by 1 kHz), **Bass** 0 to +8 dB (default +4).
- **Leave the top alone.** Peaks above about 8 kHz move with how deep the earpiece sits, so correcting them exactly fits the coupler, not the ear. Above 8 kHz the correction is the response's broad trend only (the average over the three bands either side) and limited to ±3 dB.
- **No wedge range:** an in-ear covers the whole range, so no bands are dropped at the ends (only no-data bands).
- **Limits:** cuts to −10 dB, boosts to +6 dB (a sealed in-ear has headroom in the bass that a wedge doesn't).
- **Level:** default −40 dBFS, at most −20 dBFS; In-ear never goes louder. The panel says, every time, not to wear the in-ears while measuring.
- **Sides:** Left, Right or Both; Left and Right are measured and corrected together (the bus has one RKGraphEQ for both sides, so the two ears' responses are averaged; a big difference between them, over 3 dB in some band, is shown as a warning about the fit).

## From response to EQ

1. **Targets:** Speaker: **Flat**, **Gentle tilt** (0 dB up to 1 kHz, then −1 dB per octave: −1 at 2 kHz, −4 at 16 kHz), **Vocal wedge** (Gentle tilt plus a 2nd-order roll-off below 150 Hz). In-ear: see above.
2. The response is **normalised** to its median between 250 Hz and 4 kHz (0 dB there), and the target the same way. The median, not the mean, so a bump or dip inside that range doesn't shift everything else.
3. **Correction** per band = target − response.
4. **Smoothing:** each band is averaged with its neighbours (1-2-1), so single-band spikes from reflections are tamed.
5. **Wedge range (Speaker):** the bands at the bottom or top where the response is more than 12 dB below the normalised level (the wedge doesn't go there) get 0. The wedge's real range is found, not assumed.
6. **Limits:** Speaker cuts down to −12 dB and boosts up to +4 dB. A dip deeper than 8 dB that is narrower than three bands is treated as a null (cancellation) and not boosted at all. In-ear: see above.
7. **Strength:** 0 to 100% (default 75%), scaling the whole correction.
8. The current RKGraphEQ gains are added (the correction is on top of what was measured), and the sum is clamped to RKGraphEQ's range.

All of it is message-thread code with no audio dependencies, so it is unit tested with made-up responses.

## Applying it

- **Apply** finds the first RKGraphEQ in the bus's insert slots. If there is none, it inserts one in the first free bus slot, set to 31 bands; if all 4 slots are taken, Apply is disabled with a note saying so.
- It sets the 31-band gains and switches the plugin to 31 bands. Other settings (output gain, on/off) stay.
- **Undo** puts back the gains (and removes a GraphEQ that Apply inserted). One step only.
- The result is saved with the scene like any insert setting.

## Console panel

Under an Aux bus's insert area, a **Monitor EQ…** button opens the panel for that bus:

- Type (Speaker / In-ear), Input (channel picker, or Load measurement file… for In-ear), Mic curve, Target (and Bass for In-ear), Level, Sides (In-ear), Positions (1–8), Strength.
- **Start** runs the noise floor, then position 1; **Next position**, then **Finish**.
- **Display:** 31 columns, live while measuring (response, faded for no data and low confidence), then the suggested correction overlaid on the current GraphEQ curve. Redrawn on the 10 Hz timer only while the panel is open.
- **Apply**, **Undo**, **Stop**.

## iPad remote

The same flow on the remote, following the existing remote patterns (state pushed to the page, commands back):

- A **Monitor EQ** view for the remote's bus (and from the full console remote, for any Aux bus).
- State: step (idle, floor, position *n* of *N*, done), the 31 band values, the suggested correction.
- Commands: start, next, finish, stop, apply, undo, and the panel's settings.
- **Safety:** if the remote that started a measurement disconnects for more than 2 s, the noise stops.

## As built

**Phase 1 (engine), 2026-11-05:** `plugins/RKMix/MixMeasure.h/.cpp`, owned by `MixEngine` (`getMeasure()`).
- Level is the noise's **RMS**, −60 to −12 dBFS (pink noise peaks about 12 dB above its RMS, so −12 keeps the peaks below 0 dBFS). Speaker default −20, In-ear default −40, at most −20 (set by the panel).
- `start (bus, input, sides)` takes the bus over (the floor: silent, steady); `setNoise (on, dB)`; `stop()`; `isBusy()` until the bus is handed back. One run at a time.
- `read()` returns running sums since start (samples, steady samples, 31 reference and 31 input sums of squares, the input's peak, steady, noise on); the panel windows them by the difference of two reads.
- Engine: the taken-over bus's input becomes `own × (1 − takeover) + noise` (per side), its mute × fader `g` becomes `g + (1 − g) × takeover`; everything else in the bus (EQ, compressor, inserts, limiter, routing) is as usual.
- Idle cost, measured A/B against the build before (old and new binaries alternated 5 times, 48 kHz / 64 samples): 0.308 / 0.310 %, 1.936 / 1.938 %, 2.080 / 2.090 %, i.e. no difference. Measuring adds about 0.2 % (benchmark case "Monitor EQ measuring Aux 1").
- Tests: tests/MixMeasureTests.cpp (takeover level, a known −18 dB path measured within 0.5 dB in every band, floor, sides, stop and hand-back).

**Phase 2 (maths), 2026-11-05:** `plugins/RKMix/MixMonitorEq.h/.cpp`, namespace `monitoreq`.
- `readResponseFile`, `micCurve`, `target`, `windowBetween` (two reads of the sums), `measuredResponse` (floor, positions, mic), `fileResponse`, `maxDifferenceDb` (left against right), `suggest` (response, options, current gains).
- Speaker targets: Gentle tilt (default), Flat, Vocal wedge. In-ear: Speakers in a room (default), Diffuse field.
- The SM58, SM57 and V7 curves and the diffuse-field values are approximate, read from published charts; still to be reviewed by the user before release.
- The floor's power is taken off each position's input before the ratio.
- Tests: tests/MixMonitorEqTests.cpp.
- Stagg SPM-435 (the user's in-ears): no published measurement found (2026-11-05: AutoEq, about 25 squig.link databases, reviews, retailers). So it needs a coupler, or a generic target.

**Phase 3 (console), 2026-11-05:**
- `MixMonitorEqSession` (on the processor, `getMonitorEq()`): the steps, the options (saved with the mix as `monitorEq`), Apply and Undo. A Start right after a run waits for the bus to be handed back. Start refuses a bus with no output. Stops: Stop, a scene recall, the bus's type changing, the audio restarting; a clip (0.98) ends the position (or, during the floor, the run).
- In-ear sides: Both, Left, Right, Each ear (positions alternate left and right; the coupler is moved between earpieces; the noise only on that side).
- Apply on an existing GraphEQ in 10 or 15 bands: the suggestion starts from its curve read at the 31 centres; a GraphEQ switched off counts as flat and Apply switches it on.
- `MixMonitorEqView`: the Aux bus page's **Monitor EQ** button swaps the processing for it. Closing it stops a run it started (a remote's run carries on).
- A band needs at least -100 dBFS as well as 10 dB over the floor (a digitally silent floor and filter tails).
- Tests: tests/MixMonitorEqSessionTests.cpp (in RKBoardTests): a +6 dB bell found, cut, applied into an inserted GraphEQ, measured again (mostly gone), undone; an existing 10-band GraphEQ; full slots; positions, Next, Finish, a scene recall; Each ear; a file; options saved.

## Build order

1. **Engine:** `MixMeasure` (pink noise, band analysis, the seqlock sums, the takeover fades), the bus taken over in `MixEngine`. Tests, and the idle benchmark against before.
2. **The maths** (mic curves, file reader, targets, limits for both types), with unit tests.
3. **Console panel** and Apply / Undo, with the auto-inserted RKGraphEQ.
4. **iPad remote.**
5. **Manual** (docs/manuals/RKMix.md section), then README last.

## Not in this version

- **Feedback ring-out** (raise the gain until a band rings, notch it): a later mode of the same panel.
- Measuring the main PA, or several monitors at once.
- Phase or delay measurement.
