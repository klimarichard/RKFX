# RKChorus

**Chorus** · Modulation family (light sky blue) · zero latency

RKChorus thickens a sound by mixing it with slightly delayed copies whose timing (and therefore pitch) gently wavers, like several players playing the same part. With one voice it's a classic pedal chorus; with four voices per side it becomes a lush ensemble. A mono source comes out as wide stereo.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md).

## How the signal flows

1. The input goes into a short delay line.
2. One to four **voices** read from it, each with its delay moved slowly up and down by an LFO. The voices are spread evenly through the LFO cycle, and the right channel moves opposite to the left, which creates the stereo width.
3. **Tone** brightens or darkens the chorus voices, **Width** sets their stereo spread.
4. **Dry/Wet** blends with the original, then **Output** sets the level.

The original (dry) signal is never delayed, so RKChorus adds no latency.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Voices** | 1 to 4 per side | 1 | Number of chorus voices. 1 is a classic pedal chorus; 3 to 4 is a rich ensemble or string-machine sound. The level is balanced automatically, so adding voices doesn't make it louder. Changing the count crossfades, so it never clicks. |
| **Rate** (Sync off) | 0.05 to 10 Hz | 0.8 Hz | How fast the voices waver. Slow (0.2 to 1 Hz) is smooth and lush; fast (3 Hz and up) becomes a warbling, vibrato-like chorus. |
| **Rate** (Sync on) | 1/1 to 1/32, dotted (D) and triplet (T) | 1/4 | Length of one modulation cycle as a note value. |
| **Depth** | 0 to 100% | 40% | How far the timing wavers: more depth = more pitch movement and a stronger effect. |
| **Delay** | 5 to 30 ms | 12 ms | The base delay of the voices. Short (5 to 10 ms) is tight and shimmery; longer (15 to 30 ms) is broader, closer to a doubling effect. |
| **Width** | 0 to 100% | 100% | Stereo spread of the chorus voices. 0% is mono, 100% is full width. |
| **Tone** | -100% (dark) to +100% (bright) | 0% | Tilts the chorus voices' tone around 1 kHz: negative is warmer and softer (vintage, analog-style), positive is brighter and more shimmery. The dry signal is not affected. |
| **Sync** | Off, on | Off | Off: the rate is in Hz. On: the rate follows the song tempo as a note value, locked to the beat when the host transport runs. |

Plus the standard **Dry/Wet** (default 50%, the classic chorus balance), **Output** and **Bypass**.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Classic Pedal Chorus** | Voices 1, Rate 0.8 Hz, Depth 45%, Delay 10 ms, Dry/Wet 50% |
| **Lush Ensemble** | Voices 4, Rate 0.3 Hz, Depth 60%, Delay 18 ms, Tone -30%, Dry/Wet 60% |
| **Subtle Vocal Widening** | Voices 2, Rate 0.5 Hz, Depth 20%, Delay 14 ms, Width 100%, Dry/Wet 25% |
| **Electric Piano Shimmer** | Voices 2, Rate 1.2 Hz, Depth 40%, Delay 8 ms, Tone +20%, Dry/Wet 45% |
| **Bass Chorus** | Voices 1, Rate 0.6 Hz, Depth 35%, Delay 10 ms, Width 60%, Tone +30%, Dry/Wet 35% |
| **Warbly Lo-Fi** | Voices 1, Rate 4 Hz, Depth 70%, Delay 6 ms, Tone -60%, Dry/Wet 50% |

## Notes

- Changing Delay while playing bends the pitch briefly, like turning the knob on an analog chorus; it's smooth, not a click.
- Gig Performer tip: map Dry/Wet to a widget to bring the chorus in for a chorus section of the song, or Depth for more or less movement.
