# Shimmer: design

**A shimmer mode for RKVerb and RKDelay** · zero latency on the dry signal

This adds pitch-shifted feedback to RKVerb and RKDelay: the tail (or the repeats) is shifted up an octave, or another interval, and fed back in, so each pass climbs higher and the sound blooms into a shimmering pad. Both plugins share one new pitch shifter in the core. This is a design, not a manual: it says what the feature does and how, so it can be built and reviewed against it.

## Why it fits zero latency

A pitch shifter needs some time to work (here about 25 ms), which is why a plain pitch shifter was ruled out of RKFX. In a reverb or delay, the shifter only sits in the **feedback**, where the sound is already late: the dry signal passes untouched, and the wet signal's own delay simply absorbs the shifter's. In RKDelay the shifter's delay is even subtracted from the loop, so the repeats stay in time (see [RKDelay](#rkdelay)).

## The shifter (core)

`core/dsp/PitchShifter.h`: a granular delay-line shifter. It works on any material (chords, a whole reverb tail), with no pitch tracking. RKVoice's PSOLA shifter isn't reused, because it only handles single notes.

- **How it works:** a short delay line read by **four taps**. Each tap glides through a 50 ms window at the speed that gives the wanted pitch: faster than real time for upward shifts, slower for downward ones. When a tap reaches the end of its window it jumps back. Each tap is faded in and out with a raised-cosine window, a quarter of a window apart, so a jump is never heard and the four together keep a steady level.
- **Four taps rather than the usual two** smooth the level ripple and the roughness that two crossfading taps cause, which matters most on clean delay repeats.
- **Reads** use the existing Catmull-Rom `DelayLine`.
- **Latency:** on average half the window, about **25 ms**. It's exposed as `latencySamples()` so RKDelay can compensate.
- **Intervals:** any ratio. The plugins offer a fixed list (see below).

## RKVerb

```
input ─► pre-delay ─► reflections ─► diffusion ─► FDN (8 lines) ─┬─► filters ─► width ─► wet
                                        ▲                       │
                                        └── soft clip ◄─ × Shimmer ◄─ shifter ◄─ tail (mono)
```

- The **tail** (the sum of the FDN's lines) goes through the shifter and is added back into the FDN's input, after the diffusers. Each trip around comes back shifted again: one octave, then two, and so on. The reverb's own damping darkens each new layer, so the climb fades away naturally, as in the classic shimmer pedals.
- **Shimmer** (0 to 100%) sets how much shifted tail goes back in. At 0% the shifter isn't run at all, so the reverb is exactly as today.
- **Safety:** the feed is limited so that the reverb's loop gain plus the shimmer stays below 1 (max feed 0.6 × what the decay leaves room for), with a soft clipper as the last guard. An **Octave down** shimmer also passes a fixed 80 Hz high-pass, so low energy can't pile up.
- **Freeze:** while frozen, the shimmer feed is muted, as the input is, so the frozen sound doesn't keep growing.
- **CPU:** one mono shifter. That's four interpolated reads and four window values per sample, small next to the FDN.

## RKDelay

```
input ─► delay line ─► character ─► filters ─┬─► wet
              ▲                              │
              ├──── × (1 − Shimmer) ◄────────┤  unshifted repeats, written now
              └── written 25 ms early ◄─ × Shimmer ◄─ shifter ◄─┘  shifted repeats
```

- The feedback is split: **(1 − Shimmer)** of it goes back as today, and **Shimmer** of it goes through the shifter first. Each repeat is then shifted again, so the echoes climb: the first repeat at the original pitch, the next an octave up, the next two octaves up (with Shimmer at 100%), each one darker through the loop filters.
- **In time:** the shifted feedback is written into the delay line **the shifter's latency earlier** than the normal write position, so its trip around the loop still takes exactly the delay time. Both kinds of repeat stay on the beat. This needs one new `DelayLine::addAt (samplesAgo, x)`. When the delay time is shorter than the shifter's latency (under about 30 ms), the shifted repeats arrive slightly late; at those times shimmer is a texture, not an echo.
- **Stereo and ping-pong:** one shifter per channel, so the shifted repeats follow the stereo mode like the others.
- **Safety:** the total feedback is unchanged (it's only split), and the existing DC blocker, soft clipper and loop filters stay in the loop, so it can't run away. An Octave down shimmer gets the same fixed 80 Hz high-pass in its path.
- At 0% the shifters aren't run.

## Controls

Appended to each plugin, after its existing parameters. With the defaults below, old sessions and presets load with shimmer off and sound unchanged.

| Control | ID | Range | Default |
| --- | --- | --- | --- |
| Shimmer | `shimmer` | 0 to 100% | 0% |
| Interval | `shimmer_interval` | Octave up, Fifth up, Octave + fifth up, Two octaves up, Octave down | Octave up |

**In the windows:** a small **Shimmer** group (the Shimmer knob and an Interval selector) in each plugin's layout. The window grows if there's no room. Interval is dimmed while Shimmer is 0%.

## Factory presets

| Plugin | Preset | Idea |
| --- | --- | --- |
| RKVerb | Shimmer Pad | Hall, Decay 6 s, Size 80%, Shimmer 50%, Octave up, Damping 40%: the classic ambient shimmer |
| RKVerb | Fifth Heaven | Hall, Decay 8 s, Shimmer 40%, Fifth up |
| RKVerb | Shimmer Plate | Plate, Decay 4 s, Shimmer 30%, Octave up: brighter and shorter, for a violin or vocal line |
| RKDelay | Shimmer Echo | Digital, 1/4 dotted sync, Feedback 55%, Shimmer 60%, Octave up: climbing repeats |
| RKDelay | Crystal Fifths | Tape, 1/8 sync, Feedback 50%, Shimmer 50%, Fifth up, Ping-Pong |
| RKDelay | Sub Drop | Analog, 1/4 sync, Feedback 45%, Shimmer 40%, Octave down: repeats falling into darkness |

## Tests

- **Shifter:** a sine at 440 Hz comes out at 880 Hz (Octave up), 659 Hz (Fifth up) and 220 Hz (Octave down), measured by zero crossings and spectrum peak; the level stays within 1 dB of the input; the latency matches `latencySamples()` within a window.
- **Off is unchanged:** at Shimmer 0%, both plugins' output is identical to the same settings without the new parameters (bit-exact).
- **RKVerb:** with shimmer, an impulse's tail has clear energy an octave above the input after one second; with Freeze on, the shimmer level stops growing; at the longest decay and 100% Shimmer the output stays bounded (no runaway in 60 s).
- **RKDelay:** with Shimmer 100% and Octave up, repeat 1 is at the input pitch, repeat 2 an octave up, repeat 3 two octaves up, and **each repeat arrives at its delay time within 1 ms** (compensation). At Feedback 100% it stays bounded.
- **No clicks:** sweeping Shimmer and switching Interval on a sustained tone produce no steps beyond the signal's own.
- **Presets, board, validation:** RKFXPresetTests, RKBoardTests, auval (`aufx Vrb1 RKMu`, `aufx Dly1 RKMu`), pluginval at strictness 10.

## Open questions

- **Interval list:** is Octave up / Fifth up / Octave + fifth / Two octaves / Octave down the right set? A Fourth up or a "Detuned" (±10 cents, a chorus-like widen) could be added.
- **Shimmer tone:** the shifted layers are darkened by the reverb's damping (RKVerb) or the loop filters (RKDelay). A dedicated Shimmer Tone knob could brighten or darken just the shimmer. Recommendation: not in v1.
