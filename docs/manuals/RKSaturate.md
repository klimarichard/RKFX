# RKSaturate

**Saturation** · Drive family (warm red) · zero latency, or about 0.1 ms with oversampling

RKSaturate adds warmth, weight and gentle grit: the musical distortion of tape, tube circuits and transformers. Use it lightly to make a sound fuller and more present, or push it for obvious drive. It works on anything: vocals, bass, drums, keys, guitar, a whole mix.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md).

## How the signal flows

1. **Drive** raises the level into the saturation curve.
2. The curve (the **character**) bends the waveform, adding harmonics. With **oversampling** on, this happens at 2 or 4 times the sample rate, so the extra harmonics don't fold back as harsh, unmusical tones.
3. **Tone** tilts the result darker or brighter.
4. **Auto Gain** turns the level back down to match the drive.
5. **Dry/Wet** blends with the original, then **Output** sets the level.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Tape / Tube / Transformer** | Tape, tube, transformer | Tape | The character. **Tape**: soft and smooth, rounds off transients and a touch of the highs; glues things together. **Tube**: asymmetric, adding even harmonics; warm, full and slightly "bloomy". **Transformer**: a firmer curve that drives the lows harder; adds weight and a little grit to bass, kick and low vocals. Switching crossfades, so it never clicks. |
| **Drive** | 0 to 24 dB | 6 dB | How hard the signal hits the curve. Up to about 6 dB is subtle color; 12 dB is clear saturation; 24 dB is thick and distorted. |
| **Tone** | -100% (dark) to +100% (bright) | 0% | Tilts the saturated sound: darker for smooth, vintage warmth, brighter for presence and edge. |
| **Auto Gain** | Off, on | On | Turns the output down as you add drive (about 0.75 dB per dB of drive), so you hear the change in character rather than just "louder". It's a fixed level match, not a compressor, so it never pumps. |
| **Oversampling** | Off, 2x, 4x | 2x | Processes the curve at a higher rate to keep harmonics from folding back as harsh, inharmonic tones (aliasing). **Off**: no latency, but strong drive on bright sounds gets gritty. **2x**: clean for most uses. **4x**: cleanest for heavy drive on bright sources. See latency below. |

Plus the standard **Dry/Wet** (default 100%), **Output** and **Bypass**. Lower Dry/Wet blends saturation in parallel, adding warmth while keeping the original's transients.

## Latency

Oversampling adds a tiny delay: 4 samples at 2x and 5 samples at 4x (about 0.1 ms at 44.1 or 48 kHz, far below anything you can feel when playing). RKSaturate reports it to Gig Performer, which compensates automatically. With oversampling **Off**, there's no latency at all.

The dry signal (for Dry/Wet and Bypass) is delayed by the same amount, so nothing shifts when you change the mix or bypass. Changing the Oversampling setting itself is meant for setting up a sound, not for switching during a song.

## Starting points

| Use | Settings |
| --- | --- |
| Subtle warmth on a vocal | Tape, Drive 4 dB, Tone -10%, Auto Gain on, 2x |
| Bass weight and growl | Transformer, Drive 10 dB, Tone +10%, Auto Gain on, 2x |
| Drum bus glue | Tape, Drive 8 dB, Tone 0%, Auto Gain on, Dry/Wet 60% |
| Warm electric piano | Tube, Drive 9 dB, Tone -20%, Auto Gain on, 2x |
| Gritty, pushed keys | Tube, Drive 18 dB, Tone +20%, Auto Gain on, 4x |
| Parallel crunch on drums | Transformer, Drive 20 dB, Dry/Wet 35%, 4x |

## Notes

- Saturation also gently compresses peaks; that's part of the sound.
- Gig Performer tip: map Drive to a widget to push a sound harder in loud song sections (with Auto Gain on, the level stays roughly steady).
