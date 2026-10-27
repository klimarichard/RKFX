# RKGraphEQ

**Graphic EQ, 10, 15 or 31 bands** · Filter and EQ family (lavender) · zero latency

RKGraphEQ is a classic graphic equalizer: a row of sliders, each boosting or cutting one fixed frequency band. It's quick to use live, great for shaping a whole instrument or a monitor mix, and for notching out feedback with the 31-band layout. A response curve above the sliders shows the combined effect.

For the header, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKGraphEQ has no Dry/Wet: blending an EQ'd signal with the original causes phase coloration, so the EQ always runs at 100%.

## Band count

The **Bands** switch (bottom row) picks the layout:

| Bands | Spacing | Frequencies |
| --- | --- | --- |
| **10** | 1 octave | 31.5, 63, 125, 250, 500 Hz, 1, 2, 4, 8, 16 kHz |
| **15** | 2/3 octave | 25, 40, 63, 100, 160, 250, 400, 630 Hz, 1, 1.6, 2.5, 4, 6.3, 10, 16 kHz |
| **31** | 1/3 octave | 20 Hz to 20 kHz, the standard 31 ISO frequencies |

Each layout keeps **its own settings**: switching from 10 to 31 bands and back loses nothing, and Gig Performer mappings stay fixed. Switching crossfades over 20 ms, so it never clicks. Only the selected layout affects the sound and uses CPU.

Default: 10 bands.

## Sliders

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Band slider** | -12 to +12 dB | 0 dB | Boosts (up) or cuts (down) the band at the frequency shown below the slider. |

Sliders work like the knobs: drag up or down (hold Shift for fine adjustment), use the mouse wheel, **double-click** to reset to 0 dB, or click the value above the slider to type an exact number. In the 31-band layout the values are shown rounded to whole dB to fit.

Each band is exactly one band-spacing wide, whatever its gain (constant Q). A small boost and a big boost affect the same range of frequencies, and neighboring sliders join into a smooth curve, so the sliders' positions look like the sound.

Bands at 0 dB are switched off internally, so a mostly flat EQ uses almost no CPU.

## Flat

The **Flat** button (bottom row) resets every band of the current layout to 0 dB at once. The other layouts keep their settings.

## The response curve

The curve shows the actual combined response of all bands, including how neighboring bands add up, from 20 Hz to 20 kHz. The center line is 0 dB; grid lines are 3 dB apart, up to ±12 dB. It's for display only, and redraws only when a slider moves.

## Starting points

| Use | Settings |
| --- | --- |
| Smile curve for a fuller PA sound | 10 bands: 63 Hz +3, 125 Hz +2, 500 Hz -2, 1 kHz -2, 8 kHz +2, 16 kHz +3 |
| Ring out a vocal monitor | 31 bands: raise the gain until feedback starts, then pull down the slider at the ringing frequency by 6 to 9 dB; repeat for the next ring |
| Acoustic guitar through a PA | 15 bands: 100 Hz -4, 250 Hz -2, 2.5 kHz +2, 10 kHz +2 |
| Bass guitar growl | 10 bands: 63 Hz +2, 250 Hz -3, 1 kHz +3, 2 kHz +2 |
| Telephone effect | 10 bands: 31.5 to 250 Hz all -12, 4 kHz to 16 kHz all -12, 1 kHz +6 |

## Notes

- The sound is accurate across the range; on the very top band (16 kHz in 10 and 15 bands, 20 kHz in 31 bands) at 44.1 or 48 kHz, its upper side is slightly gentler than an analog EQ, which can't be heard as anything but natural.
- Boosting many bands at once makes the output louder; lower Output to compensate.
- Gig Performer tip: map a few key sliders (for example 2.5 kHz for presence) to widgets for quick changes during a show.
