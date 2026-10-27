# RKDistort

**Overdrive, distortion and fuzz** · Drive family (deep red) · zero latency, or about 0.1 ms with oversampling

RKDistort turns a clean signal into anything from a touch-sensitive overdrive to a saturated distortion or a buzzing, sputtering fuzz. It's made for guitar and bass played straight into Gig Performer, but works on keys, synths, drums and vocals too. A simple **cab filter** shapes the sound like a speaker cabinet, so direct guitar or bass doesn't sound fizzy through a PA.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md).

## How the signal flows

1. **Tight** removes lows before the distortion, so palm mutes and low notes stay defined.
2. **Gain** drives the signal into the clipping stage (the **type**), oversampled for a clean, non-harsh result.
3. **Mid** and **Tone** shape the distorted sound.
4. The **cab filter** gives it a speaker-cabinet curve.
5. **Auto Gain** (optional) keeps the level steady as you change the gain.
6. **Dry/Wet** blends with the clean signal (parallel distortion), then **Output** sets the level.

## Drive

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Drive / Dist / Fuzz** | Overdrive, distortion, fuzz | Drive | The type. **Drive** (overdrive): soft clipping with a mid focus: the lows are held back going in and the fizz is rounded coming out; smooth and responsive to how hard you play. **Dist** (distortion): harder clipping with more sustain and compression; tighter and more aggressive. **Fuzz**: heavy, lopsided clipping, buzzy and thick; as notes fade they start to sputter and gate, like a classic fuzz pedal. Switching crossfades, so it never clicks. |
| **Gain** | 0 to 48 dB | 18 dB | Amount of drive. Lower settings clean up when you play softly or turn down your instrument; higher settings saturate everything. |
| **Tight** | Off, or 40 to 500 Hz | Off | Cuts lows before the distortion. Around 80 to 150 Hz tightens high-gain sounds and palm mutes; higher settings make it thinner and more focused. Turn fully left for Off. |

## Tone & Cab

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Cab** (Off, 1x12, 2x12, 4x12, Bass) | Off, small 1x12, medium 2x12, large 4x12, bass 1x15 | Off | A speaker-cabinet curve: removes the harsh top end and the deep lows, with each cabinet's typical resonances. **1x12**: compact, focused, a bit boxy. **2x12**: fuller and more balanced. **4x12**: big low resonance and a slight mid scoop, the classic rock sound. **Bass**: keeps the low end of a 15" bass cabinet. Use Off when you send RKDistort into an amp or a separate cabinet simulator. Changing it crossfades. |
| **Mid** | -12 to +12 dB around 800 Hz | 0 dB | Boost for a solo that cuts through the band; cut for a scooped, heavy rhythm sound. |
| **Tone** | -100% (dark) to +100% (bright) | 0% | Tilts the distorted sound darker or brighter. |

The cab filter is a fixed EQ curve, not an impulse-response cabinet simulation, so it's practically free on CPU.

## Bottom row

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Auto Gain** | Off, on | Off | Turns the output down as you add gain (about 0.5 dB per dB), so changing the gain mostly changes the character, not the volume. |
| **Oversampling** | Off, 2x, 4x | 4x | Processes the distortion at a higher rate so its harmonics don't fold back as harsh, inharmonic tones. 4x is cleanest for high gain; Off adds no latency at all. |

Plus **Dry/Wet** (default 100%; lower it for parallel distortion that keeps the clean attack, great on bass), **Output** and **Bypass**.

## Latency

Oversampling adds 4 samples at 2x and 5 at 4x (about 0.1 ms at 44.1 or 48 kHz), far below anything you can feel. RKDistort reports it to Gig Performer, which compensates automatically, and the dry signal is delayed to match, so Dry/Wet and Bypass stay aligned. With oversampling Off there's no latency. Changing Oversampling is meant for setting up, not during a song.

## Starting points

| Sound | Settings |
| --- | --- |
| Edge-of-breakup blues | Drive, Gain 12 dB, Tone +10%, Cab 2x12 |
| Classic overdrive lead | Drive, Gain 24 dB, Mid +4 dB, Tone 0%, Cab 2x12 |
| Modern high-gain rhythm | Dist, Gain 38 dB, Tight 120 Hz, Mid -4 dB, Cab 4x12 |
| Vintage fuzz | Fuzz, Gain 30 dB, Tone -20%, Cab 1x12 |
| Gritty bass (parallel) | Dist, Gain 28 dB, Tight 80 Hz, Cab Bass, Dry/Wet 40% |
| Distorted keys / organ | Drive, Gain 20 dB, Mid +3 dB, Cab 1x12, Dry/Wet 70% |
| Lo-fi vocal crunch | Dist, Gain 18 dB, Tight 300 Hz, Tone +20%, Cab 1x12, Dry/Wet 50% |

## Notes

- Without the cab filter, a direct high-gain guitar sounds fizzy through a PA; turn on a cab or use a separate cabinet simulator.
- Gig Performer tip: map Gain to an expression pedal for a "volume knob" style clean-up, or Mid to a widget for a solo boost.
