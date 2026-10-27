# RKFlange

**Flanger** · Modulation family (cornflower blue) · zero latency

RKFlange creates the classic "jet plane" sweep: the signal is mixed with a very short, moving delay of itself, which carves a series of notches into the sound that glide up and down. Feedback makes the sweep more resonant and metallic. It works on guitar, drums, keys, pads and vocals.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md). A mono input gives a stereo output, so Stereo Phase works on mono sources.

## How the signal flows

1. The input goes into a short delay line. Its delay time sits at **Manual** and is swept up and down by an LFO (**Rate**, **Depth**).
2. Part of the delayed signal is fed back into the delay (**Feedback**), through a soft limiter so it can never run away.
3. **Dry/Wet** mixes the delayed signal with the original. The flanging comes from that mix: at 50% the notches are deepest.
4. **Output** sets the level.

The original (dry) signal is never delayed, so RKFlange adds no latency.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Rate** (Sync off) | 0.02 to 10 Hz | 0.3 Hz | Speed of the sweep. Very slow (0.05 to 0.3 Hz) is the classic jet sweep; fast rates become a warbling, vibrato-like effect. |
| **Rate** (Sync on) | 1/1 to 1/32, dotted (D) and triplet (T) | 1/2 | Length of one sweep as a note value, locked to the beat when the host transport runs. |
| **Depth** | 0 to 100% | 60% | How wide the sweep is: at 100% the delay moves two octaves either side of Manual. At 0% the sweep stops and RKFlange is a fixed, tunable comb filter (Rate and Stereo Phase are then dimmed). |
| **Manual** | 0.1 to 10 ms | 2 ms | The center delay of the sweep, which sets where the notches sit. Short times give a high, airy sweep; longer times move it lower and toward a chorus-like sound. With Depth at 0, turning Manual by hand "plays" the flange. |
| **Feedback** | -95% to +95% | +50% | Resonance. Positive values give a ringing, metallic, "whooshing" flange; negative values a hollower, more nasal sound with different pitch emphasis. Near ±95% the sound sings strongly but stays under control. |
| **Stereo Phase** | 0 to 180° | 90° | Offsets the right channel's sweep. 0° sweeps both sides together (mono-like); 90° to 180° makes the sweep move across the stereo field. |
| **Sync** | Off, on | Off | Off: Rate in Hz. On: Rate as a note value following the song tempo. |

Plus the standard **Dry/Wet** (default 50%, the deepest flanging), **Output** and **Bypass**. At 100% Wet there's no flanging, only a gently pitch-wobbling delayed signal (vibrato).

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Classic Jet** | Rate 0.15 Hz, Depth 80%, Manual 2 ms, Feedback +70%, Stereo Phase 90° |
| **Subtle Guitar Flange** | Rate 0.4 Hz, Depth 50%, Manual 1.5 ms, Feedback +30%, Dry/Wet 40% |
| **Hollow Negative** | Rate 0.25 Hz, Depth 70%, Manual 3 ms, Feedback -70% |
| **Synced Drum Whoosh** | Sync on, 1/1, Depth 90%, Manual 1 ms, Feedback +60% |
| **Metallic Robot Comb** | Depth 0%, Manual 4 ms, Feedback +85% |
| **Wide Pad Swirl** | Rate 0.1 Hz, Depth 100%, Manual 3 ms, Feedback +40%, Stereo Phase 180° |

## Notes

- High Feedback boosts the resonant frequencies; lower Output if it gets loud.
- No "through-zero" flanging: that style needs the dry signal delayed, which would add latency.
- Gig Performer tip: map Manual to an expression pedal (with Depth at 0) for a hands-on, "pedal-swept" flange.
