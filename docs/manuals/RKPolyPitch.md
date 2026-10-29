# RKPolyPitch

**Polyphonic pitch shifter, whammy style** · Modulation family (indigo blue) · **Whammy mode delays your sound** (see below)

RKPolyPitch shifts the pitch of whatever you play, chords included, like a Whammy or a Boss XS-1: sweep it with an expression pedal (an octave up, a dive bomb two octaves down), jump with a footswitch, add a harmony under or above you, or thicken your sound with a detuned double. It works from violin to bass.

For the header, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). There's no Dry/Wet: the modes are the mix.

## Your sound and the delay

RKFX's first rule is no latency, and RKPolyPitch keeps it wherever it can:

- **Harmony and Detune:** your own sound passes through **untouched, with no delay**. The shifted voices follow a few milliseconds behind, which sounds like a second player.
- **Whammy:** your sound is **replaced** by the shifted one, so the shifter's delay is the delay of everything you hear. **This is the one place in RKFX where the sound you play comes out late.** The display shows how much:

| Range | Delay | With Low Latency |
| --- | --- | --- |
| Violin | about 7 ms | about 5 ms |
| Guitar | about 14 ms | about 11 ms |
| Bass | about 27 ms | about 22 ms |

The lower the instrument, the longer the shifter has to listen to keep low notes clean. 7 ms is like standing 2.4 m from an amp, which most players don't notice; 14 ms can be felt in tight rhythm parts but is fine for sweeps and leads. **Low Latency** cuts the delay by about a fifth, at the cost of a slightly grainier sound on low notes and a few cents less accuracy at extreme shifts. RKPolyPitch doesn't tell the host about this delay, so nothing else in your rig is delayed to match.

The pitch stays accurate within a few cents at every shift from -24 to +24 semitones, for single notes and chords.

## The display

The shift right now (large), what the current mode does to your own sound (orange in Whammy: the delay), and the treadle from **Heel** to **Toe** with the pedal's position.

## Mode

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Whammy / Harmony / Detune** | | Whammy | **Whammy:** only the shifted sound. **Harmony:** your sound plus a shifted voice. **Detune:** your sound plus two voices a few cents either side, a thick doubling. Switching crossfades. |
| **Violin / Guitar / Bass** | | Guitar | The lowest notes to expect. Choose the instrument: a lower range keeps low notes clean but delays Whammy more. |
| **Low Latency** | Off, on | Off | Less delay, slightly grainier low notes (see above). |

## Pedal

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Heel** | -24 to +24 semitones | 0 | The shift with the pedal at the heel (0%). |
| **Toe** | -24 to +24 semitones | +12 | The shift with the pedal at the toe (100%). |
| **Pedal** | 0 to 100% | 0% | Where the pedal is. Map it to an expression pedal. In between, the shift moves evenly in semitones, like a Whammy's treadle. For a fixed shift (Harmony), set Heel and Toe the same. |
| **Glide** | 0 to 500 ms | 30 ms | Smooths the pedal, so a stepped controller still sweeps smoothly. |

## Momentary

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Momentary** | footswitch | | While held, the shift moves to Toe; released, back to where the pedal is. Map a momentary footswitch to it for octave jumps and dives. |
| **Rise** | 0 to 2 s | 50 ms | How fast it moves to Toe. |
| **Fall** | 0 to 2 s | 150 ms | How fast it comes back. |

## Voice

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Fine** | -50 to +50 cents | 0 | A slight detuning on top of the shift. |
| **Detune** | 2 to 30 cents | 10 cents | Detune mode: how far the two voices are from your pitch. |
| **Voice Level** | -60 to +6 dB | -3 dB | Harmony and Detune: the level of the added voice(s). |
| **Width** | 0 to 100% | 60% | Detune: spreads the two voices left and right. |
| **Tone** | 1 to 20 kHz | 20 kHz | A low-pass on the shifted sound, to soften the top of big upward shifts. |

## In a rig

- **Gig Performer:** map **Pedal** to an expression pedal and **Momentary** to a momentary footswitch.
- **RKBoard:** the **PolyPitch** pedal, with Pedal on an Assign slot for the expression pedal.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Octave Up Sweep** | Whammy, Heel 0, Toe +12: the classic sweep |
| **Dive Bomb** | Whammy, Heel 0, Toe -24, Glide 80 ms |
| **Octave Jump** | Whammy, Toe +12, Rise 30 ms, Fall 100 ms: use the Momentary footswitch |
| **Octave Down Harmony** | Harmony, Heel and Toe -12, Voice Level -2 dB: a bass octave under chords |
| **Fifth Above** | Harmony, Heel and Toe +7, Voice Level -6 dB |
| **Detune Double** | Detune, 10 cents, Width 80% |
| **Bass Octave Down** | Harmony, Bass range, Heel and Toe -12, Tone 2 kHz |

## Notes

- Big upward shifts (two octaves) and dive bombs sound grainier than an octave, as on every pedal of this kind.
- If you play tight rhythm parts in Whammy mode and feel the delay, try Low Latency, or play the part in Harmony mode with Voice Level up.
