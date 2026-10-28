# RKLimit

**Peak limiter** · Dynamics family (bright yellow) · zero latency, or about 0.1 ms with oversampling

RKLimit is a safety net: whatever comes in, nothing goes out above the **ceiling**. Put it at the end of a PA or in-ear mix to protect speakers and ears from sudden peaks (a dropped mic, a feedback burst, a too-hot backing track), or push it with **Input** to make a signal louder and denser.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKLimit has no Dry/Wet: blending in the unlimited signal would defeat its protection.

## How the signal flows

1. **Input** raises the level going in.
2. The **limiter** turns peaks down instantly so they sit just under the ceiling, and turns the level back up smoothly afterwards (**Release**).
3. A **safety clipper** catches anything the limiter can't (very sudden peaks), gently rounding it into the ceiling. With **oversampling** it also catches peaks that happen *between* digital samples, which can otherwise come out louder after digital-to-analog conversion.
4. **Output** sets the final level (the ceiling applies before it, so normally leave Output at 0 dB).

There's no lookahead, so RKLimit adds no latency (unless you choose oversampling). Signals that stay below the ceiling pass through completely untouched.

## Gain reduction meter

The meter on the left shows how much the limiter is turning the signal down, from 0 to -24 dB. As a safety net it should rarely move; when using it for loudness, a few dB on the peaks is typical.

## Limiter

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Input** | 0 to 24 dB | 0 dB | Gain before the limiter. Raise it to push the signal into the limiter for more loudness; leave it at 0 dB for pure protection. |
| **Ceiling** | -12 to 0 dBFS | -1 dBFS | The absolute maximum output level. Nothing goes above it. -1 dBFS leaves a little safety margin for converters; lower values cap in-ear or PA levels. |
| **Release** | 1 to 500 ms | 50 ms | How fast the level comes back up after a peak. Short is loud and dense but can distort bass; longer is smoother and more transparent. Dimmed when Auto is on. |
| **Auto** (release) | Off, on | Off | Program-dependent release: quick after short peaks, slower during sustained loud passages, avoiding pumping. A good set-and-forget choice. |
| **Link** | 0 to 100% | 100% | Stereo link. 100%: both sides are turned down together, keeping the stereo image stable. Lower values limit each side more on its own. |

## Oversampling (bottom row)

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Oversampling** | Off, 2x, 4x | Off | Runs the safety clipper at 2 or 4 times the sample rate to catch peaks between samples ("true peaks"). Off adds no latency at all; 2x adds 4 samples and 4x adds 5 samples (about 0.1 ms), reported to Gig Performer, which compensates automatically. Meant for setting up, not for switching during a song. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **PA Safety Net** | Input 0 dB, Ceiling -1 dBFS, Auto release |
| **In-Ear Protection** | Input 0 dB, Ceiling -6 dBFS, Auto release |
| **Loud Backing Tracks** | Input 6 dB, Ceiling -1 dBFS, Release 80 ms |
| **Transparent Peak Catcher** | Input 0 dB, Ceiling -0.3 dBFS, Release 20 ms, Oversampling 4x |
| **Drum Smash** | Input 12 dB, Ceiling -2 dBFS, Release 10 ms, Link 50% |
| **Dense Loudness** | Input 8 dB, Ceiling -1 dBFS, Auto release, Oversampling 4x |

## Notes

- As the last plugin before your outputs, RKLimit guarantees the ceiling; anything after it (including Output) can raise the level again.
- Short release times with a lot of gain reduction can distort low bass notes: any limiter without lookahead does this. Use Auto or a longer release if the low end sounds fuzzy.
- Gig Performer tip: put RKLimit on the in-ear mix output with "In-Ear Protection" for peace of mind on stage.
