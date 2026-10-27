# RKDouble

**Artificial double tracking** · Modulation family (steel blue) · zero latency

RKDouble makes one performance sound like two: it adds a slightly delayed copy (the "double") whose timing drifts randomly, just like a second take played or sung by a real person never lines up perfectly. It's the classic studio "ADT" trick, great for vocals and guitars, and it can also spread a mono source across the stereo field.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKDouble has no Dry/Wet: the original always stays at full level, and **Double Level** sets how loud the copy is. A mono input gives a stereo output, so Spread works on mono sources.

## How the signal flows

1. The original (dry) signal passes through untouched, with no latency.
2. A copy is taken from a delay line at the **Delay** time, moved randomly back and forth by **Drift**. The small timing changes also cause tiny, natural pitch variations.
3. **Tone** colors the double, **Double Level** sets its volume.
4. **Spread** places the original and the double in the stereo field, then **Output** sets the level.

The double always comes slightly *after* the original, never before it, so the plugin adds no latency.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Delay** | 10 to 60 ms | 25 ms | How far behind the original the double plays. 15 to 30 ms sounds like a tight second take; 40 ms and above starts to sound like a slapback echo. |
| **Drift** | 0 to 100% | 40% | How much the double's timing wanders, up to about ±5 ms. 0% is a fixed delay (more like a short echo); more drift makes it sound like a real, slightly loose second performance. |
| **Drift Speed** | 0.1 to 2 Hz | 0.5 Hz | How quickly the timing wanders. The movement is random and never repeats in a regular cycle, unlike a chorus. Dimmed when Drift is 0. |
| **Tone** | -100% (dark) to +100% (bright) | -20% | Tilts the double's tone. A slightly darker double sits behind the original and sounds more natural. |
| **Spread** | 0 to 100% | 60% | Stereo placement. 0%: original and double both in the center. 100%: original fully left, double fully right (the classic wide double-tracked sound). In between, they're moved partly apart. |
| **Double Level** | -24 to 0 dB | -3 dB | Volume of the double. The original always stays at full level. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Natural Vocal Double** | Delay 25 ms, Drift 40%, Drift Speed 0.5 Hz, Tone -20%, Spread 40%, Level -4 dB |
| **Wide Rhythm Guitar** | Delay 20 ms, Drift 50%, Drift Speed 0.7 Hz, Tone -10%, Spread 100%, Level -1 dB |
| **Centered Thickening** | Delay 18 ms, Drift 35%, Spread 0%, Tone -30%, Level -8 dB |
| **Slapback-Style Double** | Delay 55 ms, Drift 15%, Tone -40%, Spread 30%, Level -6 dB |
| **Two Singers** | Delay 30 ms, Drift 80%, Drift Speed 1.2 Hz, Spread 70%, Level -2 dB |

## Notes

- With Spread at 100%, the original is only on the left. If your mix is often listened to in mono (some PAs, phones), keep Spread lower.
- Gig Performer tip: map Double Level to a widget to bring the double in for choruses and take it out for verses.
