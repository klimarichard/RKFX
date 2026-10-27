# RKTremolo

**Tremolo, auto-panner and harmonic tremolo** · Modulation family (cyan blue) · zero latency

RKTremolo makes the volume pulse in rhythm: the classic amp tremolo on guitar and electric piano, a stereo auto-panner on keys and pads, or the warmer "harmonic" tremolo of vintage brownface amps, where lows and highs pulse against each other. It can lock to the song tempo.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKTremolo has no Dry/Wet: Depth already sets how much of the effect you hear. A mono input gives a stereo output, so auto-panning works on mono sources.

## How the signal flows

- **Classic:** the volume follows the LFO (the low-frequency oscillator), between full level and the level set by Depth.
- **Harmonic:** a crossover splits the sound into lows and highs; their volumes move in opposite directions and are then added back together. The result is a softer, more swirling pulse with a slight phaser-like color.

The volume changes are always slightly smoothed, so even a hard square wave at full depth never clicks.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Classic / Harmonic** | Classic, harmonic | Classic | The tremolo type, see above. |
| **Shape** (Sine, Tri, Sqr, Up, Down) | Sine, triangle, square, saw up, saw down | Sine | The shape of the pulse. **Sine** is smooth and classic, **Tri** a little more pronounced, **Sqr** a choppy on/off "stutter", **Up** swells in and drops, **Down** drops in and fades (good for rhythmic, "pumping" effects). |
| **Sync** | Off, on | Off | Off: the rate is set in Hz. On: the rate follows the song tempo as a note value, and when the host transport runs, the pulse locks to the beat. |
| **Depth** | 0 to 100% | 50% | How deep the volume dips. 0% does nothing; 100% dips all the way to silence. |
| **Rate** (Sync off) | 0.1 to 20 Hz | 4 Hz | Pulses per second. |
| **Rate** (Sync on) | 1/1 to 1/32, dotted (D) and triplet (T) | 1/8 | Length of one pulse as a note value. |
| **Smoothing** | 0 to 100% | 30% | Softens the edges of the square and saw shapes. 0% is as sharp as possible (still click-free), 100% rounds them off strongly. Dimmed for Sine and Tri, which are already smooth. |
| **Stereo Phase** | 0 to 180° | 0° | Offsets the right channel's pulse. 0°: both sides pulse together. 90°: a circling stereo movement. **180°: auto-pan**, the sound moves from side to side. |
| **Crossover** | 200 Hz to 2 kHz | 600 Hz | Harmonic mode only (dimmed in Classic): where lows and highs are split. Lower values move more of the body into the "high" side. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Classic Amp Tremolo** | Classic, Sine, Depth 60%, Rate 5 Hz |
| **Surf Tremolo** | Classic, Tri, Depth 85%, Rate 7 Hz |
| **Electric Piano Auto-Pan** | Classic, Sine, Depth 70%, Rate 3 Hz, Stereo Phase 180° |
| **Brownface Harmonic** | Harmonic, Sine, Depth 70%, Rate 4.5 Hz, Crossover 600 Hz |
| **Synced Stutter** | Classic, Sqr, Depth 100%, Sync on, 1/16, Smoothing 10% |
| **Rhythmic Pumping Pad** | Classic, Down, Depth 80%, Sync on, 1/4, Smoothing 40% |

## Notes

- Gig Performer tip: map Depth to an expression pedal to bring the tremolo in and out, or Rate to speed it up at the end of a song.
- When the host isn't playing, a synced tremolo keeps running at the tempo's rate and locks to the beat as soon as the transport starts.
