# RKFilter

**Multimode filter with auto-wah and LFO** · Filter and EQ family (violet) · zero latency

RKFilter is a resonant low-pass, high-pass or band-pass filter whose cutoff can move by itself: following how loud you play (auto-wah), following a tempo-synced LFO, or both. It works on any source: guitar, bass, keys, drums, vocals.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md).

## How the signal flows

1. An envelope follower listens to the input and measures how loud it is.
2. The filter's cutoff is set by the Cutoff knob, then moved up or down by the envelope and the LFO together.
3. Dry/Wet blends with the original, then Output sets the level.

In stereo, the envelope follows whichever channel is louder, so both sides open together. The LFO can run offset between left and right for a stereo sweep.

## Filter

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Mode (LP / HP / BP)** | Low-pass, high-pass, band-pass | LP | **LP** keeps the lows and removes highs above the cutoff (darker). **HP** keeps the highs and removes lows below the cutoff (thinner). **BP** keeps only a band around the cutoff (nasal, wah-like, telephone). |
| **Slope (12 / 24 / 36 / 48)** | 12, 24, 36, 48 dB/oct | 12 | How steeply frequencies beyond the cutoff are removed. 12 is gentle and natural, 48 is very steep and synth-like. In band-pass mode the slope is split between both sides (24 = 12 dB/oct on each side), so higher values give a narrower band. |
| **Cutoff** | 20 Hz to 20 kHz | 1 kHz | Where the filter starts working. The starting point for the envelope and LFO movement. |
| **Resonance** | 0 to 100% | 20% | Emphasis at the cutoff frequency. Higher values give a sharper, more vocal, whistling peak. Stays stable even at 100% (it never self-oscillates). In band-pass mode it narrows the band. |

Changing Mode or Slope crossfades over 10 ms, so it never clicks, even while playing.

Resonance sounds the same at every slope: only the first filter stage is resonant, the extra stages just make the slope steeper.

## Envelope (auto-wah)

The envelope moves the cutoff with your playing dynamics: play harder and the filter opens further.

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Amount** | -100% to +100% | 0% | How far the envelope moves the cutoff, up to about 4 octaves. Positive values sweep **up** when you play louder (classic auto-wah). Negative values sweep **down** (a closing, "reverse" wah). At 0% the envelope is off and its other knobs are dimmed. |
| **Sensitivity** | -24 to +24 dB | 0 dB | Matches the envelope to your source. Raise it for quiet sources (soft vocals, clean guitar) so they still open the filter; lower it for hot sources (drums, high-gain guitar) so they don't keep it fully open. |
| **Attack** | 1 to 100 ms | 5 ms | How quickly the filter opens when you play. Short = snappy "quack", longer = smoother, softer sweep. |
| **Release** | 20 ms to 1 s | 150 ms | How quickly the filter closes again as the note fades. |

## LFO

The LFO moves the cutoff back and forth automatically.

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Shape** | Sine, Tri, Sqr, Up, Down, S&H | Sine | The movement. **Sine** and **Tri** sweep smoothly, **Sqr** jumps between two positions, **Up** and **Down** are ramps (saw up, saw down), **S&H** (sample and hold) jumps to a random position every cycle. Square and S&H are slightly softened so they never click. |
| **Sync** | Off, on | Off | Off: the rate is set in Hz. On: the rate follows the host tempo as a note value, and when the host transport runs, the LFO locks to the beat, so it lands in time. |
| **Depth** | -100% to +100% | 0% | How far the LFO moves the cutoff, up to about 4 octaves in each direction. Negative values invert the movement. At 0% the LFO is off and its other knobs are dimmed. |
| **Rate** (Sync off) | 0.05 to 20 Hz | 1 Hz | Speed of the LFO. |
| **Rate** (Sync on) | 1/1 to 1/32, dotted (D) and triplet (T) | 1/4 | Length of one LFO cycle as a note value. 1/4 = one cycle per beat. |
| **Stereo Phase** | 0 to 180° | 0° | Offsets the right channel's LFO. 0° moves both sides together; 180° moves them in opposite directions for a wide stereo sweep. |

The envelope and LFO add together, so you can combine an auto-wah with a slow LFO drift.

## Starting points

| Sound | Settings |
| --- | --- |
| Classic funk auto-wah (guitar, clavinet) | BP, 12, Cutoff 400 Hz, Resonance 60%, Env Amount +80%, Attack 5 ms, Release 120 ms |
| Envelope bass filter | LP, 24, Cutoff 200 Hz, Resonance 50%, Env Amount +70%, Sensitivity +6 dB |
| Tempo-synced filter pump on pads | LP, 24, Cutoff 800 Hz, Resonance 30%, LFO Depth +60%, Sync on, 1/8, Shape Sine |
| Random stepped filter | LP, 24, Cutoff 1 kHz, Resonance 50%, LFO Depth +70%, Shape S&H, Sync on, 1/16 |
| Telephone / lo-fi vocal | BP, 24, Cutoff 1.5 kHz, Resonance 20% |
| Wide stereo sweep | LP, 12, Cutoff 1.2 kHz, LFO Depth +50%, Rate 0.2 Hz, Stereo Phase 180° |

## Notes

- High Resonance boosts the cutoff frequency strongly (up to about +20 dB at 100%). With a loud, bright source, lower Output or Resonance.
- Gig Performer tip: map Cutoff to an expression pedal for a manual wah, or map Env Amount and LFO Depth to widgets to bring the movement in and out during a song.
- When the host isn't playing, a synced LFO keeps running at the tempo's rate; it locks to the beat as soon as the transport starts.
