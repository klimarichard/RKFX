# RKSwell

**Auto swell** · Dynamics family (ochre) · zero latency

RKSwell fades in every note automatically, removing its attack, like a violinist's bow or a guitarist rolling up the volume knob after each pick. Turn plucked guitar, piano or synth stabs into soft, bowed, pad-like swells, or use it as an automatic volume pedal for ambient playing.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKSwell has no Dry/Wet.

## How it works

- When you play after a pause, the sound starts silent and **swells in** over the **Swell Time**, following the chosen **Curve**.
- When the signal drops below the **Threshold**, it counts as silence: the sound is muted, and the next note swells in from zero again.
- With **Retrigger**, new notes played *during* a phrase (without silence in between) also swell in: the level dips quickly (a few milliseconds, never a click) and swells again. How hard you have to play for that is set by Retrigger; at 0% only notes after silence swell.

New notes are recognized by how suddenly the level rises, not by how loud they are, so soft and hard playing behave the same.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Linear / Exp / S-Curve** | Linear, exponential, S-curve | S-Curve | The shape of the swell. **Linear**: the volume rises evenly. **Exp** (exponential): starts very quietly and comes in late, like a slow volume-pedal roll. **S-Curve**: gentle start, smooth middle, gentle arrival; the most natural, bow-like shape. |
| **Threshold** | -60 to 0 dB | -40 dB | Below this level the signal counts as silence and the swell resets. Set it just above your instrument's noise and the tail you don't want to hear; lower it if quiet notes don't come through. |
| **Swell Time** | 10 ms to 3 s | 400 ms | How long a swell takes to reach full volume. Short times just soften the attack; long times give slow, ambient swells. |
| **Retrigger** | 0 to 100% | 50% | How readily a new note during a phrase starts a new swell. 100%: almost every new note swells. 50%: clear new attacks do. 0%: only a note after silence swells; then the phrase plays normally (like an automatic volume pedal). |
| **Link** | Off, on | On | Stereo link. On: both sides swell together. Off: each side on its own. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Bowed Violin** | S-Curve, Threshold -40 dB, Swell Time 400 ms, Retrigger 50% |
| **Slow Pad Swell** | S-Curve, Swell Time 1.5 s, Retrigger 30% |
| **Volume Pedal** | Linear, Swell Time 800 ms, Retrigger 0% |
| **Attack Softener** | S-Curve, Swell Time 60 ms, Retrigger 80% |
| **Ambient Guitar Swells** | Exp, Swell Time 1.2 s, Retrigger 60% |
| **Every Note Swells** | S-Curve, Swell Time 250 ms, Retrigger 100% |

## Notes

- Place RKSwell before delay and reverb: the swells blend into the echoes for classic ambient textures (RKDelay, then RKVerb).
- If notes cut out between phrases, lower the Threshold; if noise or string buzz comes through, raise it.
- Gig Performer tip: map Swell Time to an expression pedal to go from a subtle attack softener to long swells.
