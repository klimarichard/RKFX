# RKUtility

**Utility** · Utility family (light gray) · zero latency

RKUtility is the toolbox plugin: gain, balance, stereo width, bass mono, polarity, channel routing and mid/side, mute, and a fine per-channel delay for aligning microphones. Use it to fix and prepare signals anywhere in a rackspace.

For the header, presets and Bypass, see [Common Controls](Common%20Controls.md). RKUtility has no Dry/Wet, and its **Gain** takes the place of the usual Output control.

## How the signal flows

Channel mode and swap → polarity → channel delay → width → bass mono → pan / balance → gain → mute

With everything at its default, the signal passes through completely untouched. All changes glide, and switching modes, swapping, flipping polarity or muting never clicks.

## Level

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Gain** | -inf to +24 dB | 0 dB | Overall level. Turn fully left for -inf (silence). |
| **Pan** | -100% (left) to +100% (right) | 0% | Balance: turning towards one side fades the other; the center leaves both at full level. |
| **Mute** | Off, on | Off | Silences the output, with a 10 ms fade so it never clicks. Lights orange while on. |

## Stereo

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Channel mode** (Stereo, L, R, M/S Enc, M/S Dec) | Stereo, left only, right only, mid/side encode, mid/side decode | Stereo | **Stereo**: unchanged. **L**: the left input on both sides (use one mic of a stereo pair, or one side of a guitar rig). **R**: the right input on both sides. **M/S Enc**: turns left/right into mid (left output) and side (right output), for mid/side processing with the next plugins. **M/S Dec**: turns mid/side back into left/right. |
| **Width** | 0% (mono) to 200% | 100% | Stereo width: 0% folds to mono, 100% is unchanged, above 100% widens. Check mono compatibility when widening. |
| **Bass Mono** | Off, or 40 to 300 Hz | Off | Makes everything below the frequency mono, keeping the highs' stereo. Tightens the low end and keeps it centered on a PA. Turn fully left for Off. |
| **Swap L/R** | Off, on | Off | Exchanges left and right. |

## Polarity & Delay

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Ø L / Ø R** | Normal, inverted | Normal | Flips the polarity of one side. Fixes a mic or cable wired out of phase, or two mics on the same source (snare top and bottom) cancelling each other. |
| **Delay L / Delay R** | 0 to 20 ms, in 0.01 ms steps | 0 ms | Delays one side, to time-align two microphones (or a DI and a mic) on the same source so they don't cancel each other. |

Below the delay knobs, RKUtility shows each delay in milliseconds, in samples (at the current sample rate) and as the equivalent distance (sound travels about 34.3 cm per millisecond). For example, a mic 34 cm further from the source needs about 1 ms on the closer mic.

The delay is an intentional effect, so RKUtility still reports zero latency.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Mono Check** | Width 0% |
| **Bass Mono 120 Hz** | Bass Mono 120 Hz |
| **Stereo Widen** | Width 140% |
| **Left Input Only** | Channel mode L |
| **Mid-Side Encode** | Channel mode M/S Enc |
| **Flip Polarity** | Ø L and Ø R inverted |
| **Swap Sides** | Swap L/R on |

## Notes

- To align two mics: flip polarity on one and adjust its delay until the sound is at its thinnest, then flip polarity back. That's where they line up best.
- Gig Performer tip: map Mute to a footswitch for a silent tuning or talking moment, or Gain to a fader widget as a channel level.
