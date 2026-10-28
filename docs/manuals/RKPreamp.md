# RKPreamp

**Amp preamp ("amp in a box")** · Drive family (rust red) · zero latency (4 or 5 samples with oversampling)

RKPreamp is the front half of a guitar amp: its gain stages and its tone stack. Four voicings cover the classic amp families, from a sparkling clean American amp to a modern high-gain lead. The Bass, Middle and Treble knobs work like an amp's, not like an EQ: they interact, just as on the real thing. Put a cabinet after it (a cab simulator, or the built-in cab filter) and you have an amp sound straight into the PA.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md). Dry/Wet blends in your clean signal, for a parallel drive that keeps the attack.

## How it works

1. A coupling filter sets how much low end goes into the gain stages (tighter for the higher-gain voicings).
2. **Bright** adds sparkle before the gain, like an amp's bright switch.
3. One to three **gain stages** (depending on the voicing) clip softly and a little unevenly, like tubes. Each stage is level-matched, so turning up **Gain** adds drive, sustain and compression rather than volume, and playing softer cleans the sound up.
4. The **tone stack** (Bass, Middle, Treble) shapes the driven sound, as in an amp.
5. **Presence** adds or takes away upper treble.
6. The **cab filter** (optional) makes it sound like a speaker cabinet.

The voicings are inspired by the amps they are named after; they are not exact copies.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Clean / Crunch / Top Boost / Hot-Rod** | Clean American, British Crunch, Top Boost, Hot-Rod | Crunch | The amp. **Clean**: one gain stage and a Fender-style tone stack; clean and full, breaking up only near the top of Gain. **Crunch**: two stages and a Marshall-style stack; rock rhythm and classic lead. **Top Boost**: two stages, chimey and bright, Vox-inspired. **Hot-Rod**: three stages with tight lows; modern high gain and singing leads. Switching dips the sound for a moment, like switching an amp's channel. |
| **Gain** | 0 to 10 | 5 | How hard the gain stages are driven: more distortion, sustain and compression. The level stays about the same. |
| **Bass** | 0 to 10 | 5 | Low end. As on an amp, its effect depends on Middle. |
| **Middle** | 0 to 10 | 5 | The mids. Low settings give the classic scooped sound; high settings push the guitar forward in a band. |
| **Treble** | 0 to 10 | 5 | Highs and upper mids. On an amp it also shifts the others: more treble makes the bass a little lighter. |
| **Presence** | 0 to 10 | 5 | Upper treble after the tone stack, -6 to +6 dB at 3.5 kHz; 5 is flat. |
| **Bright** | Off, on | Off | A treble boost before the gain stages, strongest at low Gain (as with an amp's bright switch, which fades as the amp is turned up). |
| **Cab** | Off, 1x12, 2x12, 4x12, Bass | Off | A speaker-cabinet curve (the same as in RKDistort), for going straight to a PA or headphones. Leave it off when the signal goes into a real amp or a cabinet simulator. |
| **Oversampling** | Off, 2x, 4x | 2x | Runs the gain stages at a higher rate, which keeps high notes free of harsh, inharmonic fizz. 2x adds 4 samples of latency (under 0.1 ms), 4x adds 5 samples. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Clean Sparkle** | Clean, Gain 3, Middle 4, Treble 6.5, Bright on |
| **Edge of Breakup** | Clean, Gain 7, Treble 6, Presence 5.5: clean when you play softly, gritty when you dig in |
| **Classic Crunch** | Crunch, Gain 5, Middle 6.5, Treble 6, Presence 6 |
| **Chimey Top Boost** | Top Boost, Gain 5, Bass 4.5, Treble 6.5, Bright on |
| **Hot-Rod Lead** | Hot-Rod, Gain 7, Middle 6, Treble 6, Presence 6 |
| **Scooped Rhythm** | Hot-Rod, Gain 6, Bass 6.5, Middle 2.5, Treble 6.5, Presence 6 |
| **Crunch to the PA** | Crunch, Gain 6, Middle 6, Treble 5.5, Cab 4x12 |

## Notes

- Effects before RKPreamp are "in front of the amp" (wah, compressor, octave, an overdrive as a boost); delay, reverb and chorus usually sound best after it.
- An overdrive in front (RKDistort, Overdrive type, low Gain, Tight on) tightens and pushes Crunch and Hot-Rod, the classic boosted-amp trick.
- Into a real guitar amp, use the Clean voicing with low Gain, or keep the Cab off and go into the amp's effects return.
- Gig Performer tip: map Gain to a widget for rhythm and lead levels of drive in one patch.
