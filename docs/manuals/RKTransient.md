# RKTransient

**Transient shaper** · Dynamics family (mustard) · zero latency

RKTransient changes the *shape* of each note or hit instead of its level: make drums punchier or softer, tighten a boomy kick, bring out the pick attack of a guitar or soften it, add room and sustain or dry a sound up. It has no threshold: it reacts the same to soft and loud playing, so you set it once and it follows the dynamics.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md).

## How it works

RKTransient compares a fast and a slow measurement of the level:

- At the **start of a note**, the fast measurement jumps above the slow one: that's the **attack**.
- As the note **rings out**, the slow measurement stays above the fast one: that's the **sustain**.

**Attack** and **Sustain** then boost or cut those parts. Because only the *difference* matters, the effect is the same whether the source is quiet or loud, and steady sounds are left alone. The **Focus** filters choose which frequencies it listens to.

## Shape

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Attack** | -100% to +100% | 0% | Positive: louder, snappier starts (more punch, more pick, more stick). Negative: softer starts (smooths harsh hits, pushes a sound back in the mix). At 100% the start of a hit gets roughly 8 dB louder. |
| **Sustain** | -100% to +100% | 0% | Positive: longer, fuller tails (more room, more body, more ring). Negative: shorter, drier sounds (less room, less ring, tighter). |
| **Attack Time** | 1 to 50 ms | 10 ms | How long the "attack" part lasts. Short times shape only the very first click; longer times include more of the hit's body. |
| **Sustain Time** | 20 to 500 ms | 100 ms | How long the "sustain" part is followed as the note decays. Longer times affect more of the tail. |

At 0% Attack and 0% Sustain the signal passes through completely untouched.

## Focus

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **High-Pass** | Off, or 20 Hz to 5 kHz | Off | The detector ignores frequencies below this. Example: on a drum bus, so the kick doesn't trigger the shaping of the snare and hi-hats. Turn fully left for Off. |
| **Low-Pass** | Off, or 100 Hz to 20 kHz | Off | The detector ignores frequencies above this. Example: so cymbals don't trigger it. Turn fully right for Off. |
| **Listen** | Off, on | Off | For setting up only: the output becomes what the detector hears. The switch lights orange while on. Don't forget to switch it off! |
| **Link** | Off, on | On | Stereo link. On: both sides are shaped together, keeping the stereo image stable. Off: each side is shaped on its own. |

The Focus filters only change what the detector listens to; the whole sound is shaped.

Plus the standard **Dry/Wet** (default 100%; lower it to blend in the original for "parallel" shaping), **Output** and **Bypass**.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Punchier Drums** | Attack +60%, Attack Time 10 ms |
| **Tighter Kick** | Attack +40%, Sustain -50%, Sustain Time 80 ms, Focus Low-Pass 500 Hz |
| **Snare Crack** | Attack +70%, Sustain -20%, Focus High-Pass 200 Hz |
| **Roomier Drums** | Sustain +60%, Sustain Time 200 ms |
| **Softer Picking** | Attack -50%, Attack Time 15 ms |
| **Dry Up a Room Mic** | Sustain -70%, Sustain Time 150 ms |
| **Parallel Snap** | Attack +100%, Dry/Wet 50% |

## Notes

- Strong Attack boosts raise the peaks; follow RKTransient with RKLimit if you need to keep the peak level in check.
- On low, steady tones a tiny level ripple (about half a dB at full settings) can remain; it's inaudible in normal use.
- Gig Performer tip: map Attack to a widget to make the drums cut through more in louder sections.
