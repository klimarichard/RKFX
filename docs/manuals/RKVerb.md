# RKVerb

**Algorithmic reverb** · Reverb family (orange) · zero latency

RKVerb places a sound in a space: a small room, a big hall or a classic studio plate. It has pre-delay with tempo sync, damping, filters for the reverb, gentle modulation for lush tails, stereo width, **Shimmer**, which makes the tail bloom upwards into a shining pad, and **Freeze**, which holds the current reverb forever as a pad underneath you.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md). A mono input gives a stereo reverb.

## How the signal flows

1. **Pre-delay** waits a moment before the reverb starts, keeping the original clear.
2. **Early reflections** (the first echoes off nearby walls) give the space its character; they depend on the type.
3. A network of eight delay lines builds the dense **reverb tail**: its length is set by **Decay** and **Size**, its brightness by **Damping**, its movement by **Modulation**.
4. With **Shimmer** up, part of the tail is pitch-shifted (an octave up, for example) and sent back into the reverb, so each pass climbs higher.
5. **Low Cut** and **High Cut** shape the reverb, **Width** sets its stereo spread.
6. **Dry/Wet** blends with the original, then **Output** sets the level.

The original signal is never delayed, so RKVerb adds no latency. (The shimmer's pitch shifter needs about 20 ms to work, but it only acts on the tail, which is late anyway.)

## Reverb

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Room / Hall / Plate** | Room, hall, plate | Hall | The type of space. **Room**: short, dense early reflections; small and natural, good for making things sound "in a room". **Hall**: spacious and smooth, with a gradual build-up; the classic big reverb. **Plate**: bright and dense right from the start, with no distinct early echoes; the studio sound for vocals and snare. Switching glides smoothly. |
| **Decay** | 0.2 to 20 s | 2.5 s | How long the reverb takes to fade by 60 dB. The value is accurate: 2 s really fades in 2 s, at any size. |
| **Size** | 0 to 100% | 60% | The size of the space. Larger sizes spread the echoes further apart for a bigger, airier sound; smaller ones are tighter. Changing it while playing glides (with a slight pitch movement, like a real space changing). |
| **Pre-Delay** (Sync off) | 0 to 200 ms | 20 ms | Gap before the reverb starts. 10 to 40 ms keeps vocals and drums clear and upfront while the reverb blooms behind them. |
| **Pre-Delay** (Sync on) | 1/64 to 1/8, dotted (D) and triplet (T) | 1/16 | Pre-delay as a note value, following the song tempo (up to 1 second). |
| **Sync** | Off, on | Off | Off: pre-delay in ms. On: pre-delay as a note value. |

## Tone

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Damping** | 0 to 100% | 50% | How quickly the highs die away compared with the lows, like the soft surfaces of a real room. 0% keeps the tail bright; high values make it warm and dark. |
| **Low Cut** | 20 Hz to 1 kHz | 150 Hz | Removes lows from the reverb, so it doesn't muddy the mix. |
| **High Cut** | 1 to 20 kHz | 10 kHz | Removes highs from the reverb, softening sibilance and cymbal wash. |

## Space

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Modulation** | 0 to 100% | 20% | Slowly moves the reverb's echoes for a lush, chorus-like tail and to avoid metallic ringing on long decays. 0% is completely still. |
| **Width** | 0 to 100% | 100% | Stereo spread of the reverb: 0% is mono, 100% fully wide. |

## Shimmer

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Shimmer** | 0 to 100% | 0% | How much pitch-shifted tail goes back into the reverb. Each trip comes back shifted again, so the sound climbs (an octave, then two...) and the Damping darkens each new layer until it fades. Around 30 to 60% gives the classic ambient shimmer. At 0% it's switched off entirely. |
| **Interval** | Octave up, Fifth up, Octave + fifth up, Two octaves up, Octave down | Octave up | How far each trip is shifted. Octave up is the classic shimmer; Fifth up is more harmonic and "choral"; Octave down sinks into a dark drone. Dimmed while Shimmer is 0%. |

Shimmer is held in check whatever the settings: the shimmer path is limited, so even the longest Decay with Shimmer at 100% doesn't run away. While **Freeze** is on, the shimmer stops feeding, so a frozen pad stays as it is.

## Freeze (bottom row)

**Freeze** holds the reverb that's sounding now, indefinitely, and stops new sound from entering it: play a chord, press Freeze, and play over the sustained pad. Switching Freeze off lets the held sound fade out with the normal Decay. It's made to be mapped to a footswitch in Gig Performer.

Plus the standard **Dry/Wet** (default 25%), **Output** and **Bypass**.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Vocal Plate** | Plate, Decay 1.8 s, Size 50%, Pre-Delay 40 ms, Damping 40%, Low Cut 200 Hz, High Cut 8 kHz, Dry/Wet 20% |
| **Snare Plate** | Plate, Decay 1.2 s, Size 40%, Pre-Delay 10 ms, Damping 30%, Low Cut 250 Hz, Dry/Wet 25% |
| **Small Room** | Room, Decay 0.6 s, Size 40%, Pre-Delay 5 ms, Damping 60%, Dry/Wet 20% |
| **Concert Hall** | Hall, Decay 2.8 s, Size 80%, Pre-Delay 30 ms, Modulation 25% |
| **Ambient Pad Wash** | Hall, Decay 8 s, Size 100%, Pre-Delay 60 ms, Damping 30%, Low Cut 300 Hz, Modulation 60%, Dry/Wet 45% |
| **Synced Vocal Hall** | Hall, Decay 2.2 s, Pre-Delay synced to 1/16, Dry/Wet 22% |
| **Freeze Drone** | Hall, Decay 12 s, Size 100%, Modulation 40%, Dry/Wet 60%; press Freeze to hold a chord |
| **Dark Guitar Room** | Room, Decay 1 s, Size 60%, Damping 75%, High Cut 5 kHz |
| **Shimmer Pad** | Hall, Decay 6 s, Size 80%, Damping 40%, Shimmer 50%, Octave up, Dry/Wet 40% |
| **Fifth Heaven** | Hall, Decay 8 s, Size 80%, Shimmer 40%, Fifth up, Dry/Wet 40% |
| **Shimmer Plate** | Plate, Decay 4 s, Shimmer 30%, Octave up, Dry/Wet 30%: brighter and shorter, for a violin or vocal line |

## Notes

- Long decays with little Low Cut can build up a lot of low end; raise Low Cut for a clearer mix.
- More Damping keeps a shimmer softer (each octave layer is darker); less makes it glassier.
- Gig Performer tip: map Shimmer to an expression pedal to swell the shimmer in and out, Freeze to a footswitch, Dry/Wet to a widget for "more reverb" moments, or Decay for a longer tail at the end of a song.
