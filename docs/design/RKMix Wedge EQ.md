# RKMix Wedge EQ: design

**Measure a wedge with pink noise and a mic, and get a 31-band RKGraphEQ curve for its Aux bus** · zero latency, free when not measuring

RKMix sends pink noise out of one Aux bus (the wedge), listens to a mic on one input channel, and compares the two band by band. The difference against a target curve becomes a correction, which RKMix writes into an RKGraphEQ insert on that bus (adding one if there isn't one). It runs from the console and from the iPad remote, so the user can walk the mic positions with the iPad in hand. This is a design, not a manual: it says what the feature does and how, so it can be built and reviewed against it.

## Suite rules, and what idle costs

The measurement must not change RKMix when it isn't running:

- **Zero latency, always.** Nothing is added to any signal path; the reported latency never changes because of this feature. The measuring itself is analysis beside the audio, never in a path anyone listens to.
- **No FFT, no block analysis.** Per-sample biquads, the same 1/3-octave band filters RKGraphEQ uses, so the bands match its sliders exactly.
- **Idle = one branch per block.** The engine checks a single `measuring` flag (atomic, read once per block). When it is off, no noise is generated, no analysis filter runs, no state is touched. The CPU benchmark at 48 kHz / 64 samples is recorded before and after; the idle figure must not move.
- **No allocation on the audio thread.** All state (noise generator, 2 × 31 filters, accumulators) is a fixed-size member, sized in `prepareToPlay`.
- **No locks.** The audio thread writes the band sums into a double-buffered array with an atomic sequence number; the message thread reads them on a timer (about 10 Hz) and does all the maths (averaging, mic curve, target, limits).
- **Even while measuring, it is cheap:** one pink noise generator and 62 biquads (31 on the noise, 31 on the mic), mono.

## Signal flow while measuring

```
pink noise ─► level ─► [Aux bus input, replacing its sends] ─► bus inserts (incl. RKGraphEQ) ─► bus output ─► wedge
     │                                                                                                   │
     └─► 31 band filters ─► mean square per band (reference)                                   mic ◄──── air
                                                                                               │
                                     input channel (pre-everything) ─► 31 band filters ─► mean square per band
```

- **Where the noise goes in:** at the bus's input, replacing everything sent to it, so the noise passes the bus's inserts. The existing RKGraphEQ curve is therefore part of what is measured, and the result is a change on top of it (measure, apply, measure again to check).
- **Bus fader and mute are bypassed** during the measurement (unity); the measurement's own Level sets the loudness. Other buses and the main carry on as they were.
- **Other inserts on the bus** (a limiter, a compressor) are left in. On steady pink noise they change the level of all bands about equally, which doesn't change the curve. The panel warns if the bus has a dynamics insert.
- **Mic tap:** the input channel straight from the interface, before gain, EQ and inserts. A stereo pair uses its left side.
- **Starting and stopping:** the noise fades in over 1 s and out over 50 ms; Stop, closing the panel, a lost remote or a scene recall all stop it at once.

## Measuring

1. **Noise floor:** 3 s with the noise off. The mic's band levels are stored as the floor.
2. **Each position:** 5 s of noise (the first 0.5 s ignored, so the room settles). Per band: the mic's level minus the noise's level, in dB.
3. **Positions:** 1 to 8, averaged as power, not dB. The panel suggests 4: around ear height where the performer stands, pointed at the wedge, 50 cm to 1 m away.
4. **Signal check:** a band whose mic level was less than 10 dB above the floor is **no data** and gets no correction.

The pink noise is Paul Kellet's filtered white noise (refined version), seeded once, so it is identical every run. Its exact spectrum doesn't matter: the reference is measured from the same noise.

## Mic curves

The measured response minus the mic's own response is the wedge's response. Choices:

- **Flat (measurement mic)**
- **Shure SM58**, **Shure SM57**, **sE Electronics V7:** 31 band values each, read from the manufacturer's published typical response charts. Approximate (units vary by about ±2 dB), which is fine for a 1/3-octave graphic EQ. They are shown to the user for review before they ship.
- **Load calibration file…:** the plain text file that comes with measurement mics (miniDSP UMIK, Dayton, others): lines of frequency and dB, `*` and `"` lines are comments, an optional sensitivity line is ignored. Interpolated in log frequency to the 31 bands. Stored with the RKMix settings (the values, not the path).

**Compensation is capped at +6 dB.** A band where the curve is more than 6 dB down (a dynamic mic above about 12–15 kHz) is **low confidence**: its correction is halved and drawn faded.

The panel reminds the user to keep cardioid mics 50 cm to 1 m away: closer, their proximity effect raises the bass and the EQ would cut bass the wedge doesn't have.

## From response to EQ

1. **Target curves:** Flat, Gentle tilt (−1 dB per octave above 1 kHz), Vocal wedge (Gentle tilt plus a 2nd-order roll-off below 150 Hz).
2. The response is **normalised** to its mean between 250 Hz and 4 kHz (0 dB there).
3. **Correction** per band = target − response.
4. **Smoothing:** each band is averaged with its neighbours (1-2-1), so single-band spikes from reflections are tamed.
5. **Wedge range:** the bands where the response is more than 12 dB below the normalised level at the bottom or top (the wedge simply doesn't go there) get 0. The wedge's real range is found, not assumed.
6. **Limits:** cuts down to −12 dB, boosts up to +4 dB. A dip deeper than 8 dB that is narrower than three bands is treated as a null (cancellation) and not boosted at all.
7. **Strength:** 0 to 100% (default 75%), scaling the whole correction.
8. The current RKGraphEQ gains are added (the correction is on top of what was measured), and the sum is clamped to RKGraphEQ's range.

All of it is message-thread code with no JUCE audio dependencies, so it is unit tested with made-up responses.

## Applying it

- **Apply** finds the first RKGraphEQ in the bus's insert slots. If there is none, it inserts one in the first free bus slot, set to 31 bands; if all 4 slots are taken, Apply is disabled with a note saying so.
- It sets the 31-band gains and switches the plugin to 31 bands. Other settings (output gain, on/off) stay.
- **Undo** puts back the gains (and removes a GraphEQ that Apply inserted). One step only.
- The result is saved with the scene like any insert setting.

## Console panel

Under the Aux bus's insert area, a **Wedge EQ…** button opens the panel for that bus:

- Mic input (channel picker), Mic curve, Target, Level (−40 to −6 dBFS, default −20), Positions (1–8), Strength.
- **Start** runs the noise floor, then position 1; **Next position**, then **Finish**.
- **Display:** 31 columns, live while measuring (response, faded for no data and low confidence), then the suggested correction overlaid on the current GraphEQ curve. Redrawn on the 10 Hz timer only while the panel is open.
- **Apply**, **Undo**, **Stop**.

## iPad remote

The same flow on the remote, following the existing remote patterns (state pushed to the page, commands back):

- A **Wedge EQ** view for the remote's bus (and from the full console remote, for any Aux bus).
- State: step (idle, floor, position *n* of *N*, done), the 31 band values, the suggested correction.
- Commands: start, next, finish, stop, apply, undo, and the panel's settings.
- **Safety:** if the remote that started a measurement disconnects for more than 2 s, the noise stops.

## Build order

1. Engine: pink noise, band analysis, the `measuring` flag, the noise replacing the bus input. Benchmark idle against before.
2. The maths (mic curves, calibration file, target, limits), with unit tests.
3. Console panel and Apply / Undo, with the auto-inserted RKGraphEQ.
4. iPad remote.
5. Manual (docs/manuals/RKMix.md section), then README last.

## Not in this version

- **Feedback ring-out** (raise the gain until a band rings, notch it): a later mode of the same panel.
- Measuring the main PA, or several wedges at once.
- Phase or delay measurement.
