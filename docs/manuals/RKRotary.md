# RKRotary

**Rotary speaker** · Modulation family (deep sky blue) · zero latency, or about 0.1 ms with drive oversampling

RKRotary recreates the swirling sound of a Leslie-style rotary speaker: a spinning horn for the highs and a spinning drum for the lows, each throwing the sound around the room. The essential organ effect, and lovely on electric piano, guitar and vocals. Switching between slow and fast gives the classic speed-up and slow-down, with the heavy drum lagging behind the light horn.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md). RKRotary always gives a stereo output, from a mono or stereo input.

## How it works

1. The input is summed to mono (a rotary cabinet has one input) and can be driven through a warm tube-style **Drive**, like the cabinet's amplifier.
2. A crossover at about 800 Hz sends the highs to the **horn** and the lows to the **drum**.
3. As each rotor spins, its sound moves towards and away from two virtual microphones on opposite sides: the pitch wavers (Doppler effect), the level rises and falls, and the horn gets brighter when it points at a mic.
4. **Balance**, **Mic Distance** and **Width** set how the two rotors and mics are mixed into stereo.

## Rotors

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Slow / Fast / Brake** | Slow, fast, brake | Slow | The rotor speed. **Slow** ("chorale"): gentle, slow swirl. **Fast** ("tremolo"): fast, intense swirl. **Brake**: both rotors slow down and stop. Meant to be mapped to a footswitch. |
| **Slow Rate** | 0.3 to 1.5 Hz | 0.8 Hz | Horn speed in Slow mode (turns per second). The drum turns a little slower than the horn, like a real cabinet. |
| **Fast Rate** | 4 to 8 Hz | 6.5 Hz | Horn speed in Fast mode. |
| **Horn Accel** | 0.2 to 3 s | 0.8 s | How quickly the light horn changes speed. |
| **Drum Accel** | 1 to 8 s | 4 s | How quickly the heavy drum changes speed. Being slower than the horn creates the characteristic "wind-up" where highs and lows spin at different speeds for a moment. |

## Sound

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Drum/Horn** (balance) | -100% (drum) to +100% (horn) | 0% | The mix of the two rotors. Towards the horn: brighter, more swirl; towards the drum: warmer, rounder. |
| **Mic Distance** | 0% (close) to 100% (distant) | 50% | Close mics give a strong, dramatic swirl; distant mics a softer, more blended sound, as if heard from across the room. |
| **Width** | 0 to 100% | 80% | Stereo spread of the two mics. 0% is mono. |
| **Drive** | 0 to 100% | 0% | Overdrives the cabinet's amplifier for the growling, gritty rock-organ sound. At 0% it's completely off (and uses no CPU). |

## Drive Oversampling (bottom row)

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Drive Oversampling** | Off, 2x, 4x | 2x | Runs the drive at a higher rate, so heavy drive stays smooth instead of harsh. It adds 4 samples (2x) or 5 samples (4x) of latency (about 0.1 ms), reported to Gig Performer; the latency stays the same whether Drive is used or not, so turning Drive up never shifts the timing. Off adds no latency. Meant for setting up, not for switching during a song. |

Plus the standard **Dry/Wet** (default 100%), **Output** and **Bypass**.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Slow Chorale** | Slow, defaults |
| **Fast Tremolo** | Fast, defaults |
| **Driven Rock Organ** | Fast, Drive 40%, Mic Distance 30%, Width 90% |
| **Distant Room** | Slow, Mic Distance 90%, Width 60% |
| **Guitar Rotary** | Slow, Slow Rate 1.0 Hz, Drum/Horn +40%, Drive 15% |
| **Stopped** | Brake (rotors standing still: a static, slightly hollow cabinet tone) |
| **Quick Spin-Up** | Fast, Horn Accel 0.3 s, Drum Accel 1.5 s |

## Notes

- The speed-up and slow-down are part of the sound: switch Slow/Fast in the music, not only between songs.
- Gig Performer tip: map **Speed** to a footswitch (or to the organ's half-moon switch), and Drive to an expression pedal.
