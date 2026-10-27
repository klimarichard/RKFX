# RKFreqShift

**Frequency shifter** · Modulation family (sky blue) · zero latency

RKFreqShift moves every frequency in the signal up or down by a fixed number of Hz. It is inspired by classic Bode frequency shifters and has four independent shifts mixed together.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md).

## Frequency shifting vs pitch shifting

A pitch shifter multiplies frequencies, so harmonies stay in tune (an octave up doubles everything). A frequency shifter *adds* the same number of Hz to every frequency. Shifting a note's 100 Hz, 200 Hz and 300 Hz harmonics up by 50 Hz gives 150, 250 and 350 Hz: no longer a clean harmonic series, so the sound becomes metallic, bell-like or inharmonic.

That is what makes it interesting:

- **Small shifts (below about 5 Hz)** sound like a slow, smooth phaser or stereo movement, without the regular sweep of an LFO.
- **Medium shifts (5 to 50 Hz)** give beating, wobbling, detuned textures.
- **Large shifts (hundreds of Hz and up)** give metallic, robotic, ring-modulator-like tones.
- **Feedback** shifts the signal again on every pass, creating rising or falling "barber pole" spirals.

## How the signal flows

1. The input is split into two versions 90° apart, which is what makes a clean shift possible.
2. Each of the four shifts (when on) moves the signal up, down or both, with optional feedback.
3. The four shifts are added together, each at its own level.
4. Low Cut and High Cut trim the shifted (wet) signal.
5. Dry/Wet blends with the original, then Output sets the level.

A mono input gives a stereo output, so Stereo Spread works on mono sources too.

## Shift controls

Each of the four strips (Shift 1 to Shift 4) has the same controls. By default only Shift 1 is on.

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **On** | Off, on | Shift 1 on, others off | Switches this shift on. When off, its knobs are dimmed and it uses no sound (you can still prepare its settings). |
| **Fine / Coarse** | Fine, Coarse | Coarse | Picks the range of the Frequency knob. Each range keeps its own setting, so switching back and forth never loses a value. |
| **Frequency (Coarse)** | 0 Hz to 5 kHz | 100 Hz | How far to shift. The knob is curved so the low end has plenty of detail (100 Hz sits a little before the middle). |
| **Frequency (Fine)** | 0 to 50 Hz | 10 Hz | How far to shift, for slow phasing and beating effects. Linear, so every part of the knob has equal resolution. |
| **Up/Down** | 0 to 100% | 0% | Direction. 0% shifts only up, 100% only down, 50% gives both at once (a ring-modulator-like sound, symmetrical around the original). |
| **Feedback** | 0 to 90% | 0% | Sends this shift's output back into its own input, so it shifts again on every pass, building spirals. Safe at every setting: a soft limiter in the loop keeps it from running away. |
| **Level** | 0 to 100% | 100% | Volume of this shift in the mix. |

Switching between Fine and Coarse glides the frequency over a few milliseconds, so it doesn't click even while playing.

## Global controls (bottom row)

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Spread** | 0 to 100% | 0% | Stereo width. The right channel is shifted slightly further (up to 5 Hz more) and starts slightly ahead (up to 90°), so the two sides move against each other. 0% keeps both sides identical. |
| **Low Cut** | 20 Hz to 1 kHz | 20 Hz | Removes lows from the shifted signal. Useful because downward shifts can push bass into rumble. |
| **High Cut** | 1 to 20 kHz | 20 kHz | Removes highs from the shifted signal, taming harsh upward shifts. |

Plus the standard **Dry/Wet** (default 100%), **Output** and **Bypass**.

## Starting points

| Sound | Settings |
| --- | --- |
| Slow stereo phasing on keys or pads | Shift 1, Fine, 0.3 Hz, Up/Down 0%, Spread 60%, Dry/Wet 50% |
| Wobbly detune on vocals | Shift 1, Fine, 4 Hz, Dry/Wet 30% |
| Barber-pole rising spiral | Shift 1, Fine, 2 Hz, Feedback 70%, Dry/Wet 50% |
| Metallic robot | Shift 1, Coarse, 300 Hz, Up/Down 50% |
| Inharmonic bells on guitar | Shifts 1 and 2 on, Coarse 150 Hz and 420 Hz, levels 70%, High Cut 8 kHz, Dry/Wet 40% |

## Notes

- At 0 Hz a shift doesn't move frequencies but still changes the signal's phase, so mixing it with the dry signal colors the tone slightly. This is normal for frequency shifters.
- With several shifts on at full level, the output gets louder; lower the Levels or Output to compensate.
- Gig Performer tip: map a widget to a shift's Fine Frequency or Up/Down for expressive live sweeps. Fine and Coarse are separate parameters, so map the one you use.
