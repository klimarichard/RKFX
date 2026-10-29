# RKRide: design

**Automatic level rider** · Dynamics family · zero latency

RK + a verb, as every RKFX plugin: to *ride* the fader. This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKRide does what a sound engineer's hand does on a vocal fader: it slowly turns a phrase up when it's too quiet and down when it's too loud, so the voice or instrument sits at a steady level. It works over hundreds of milliseconds, not on single peaks. It isn't a compressor: it doesn't change the shape of notes or squash transients, it only moves the overall level, like a fader.

Typical uses:

- A singer who moves towards and away from the mic, or sings much softer in verses than in choruses.
- A violin or acoustic instrument whose soft passages disappear in the mix.
- Spoken announcements between songs, from voices of very different loudness.

It goes **before** a compressor: RKRide evens out the phrases, and the compressor then deals with the peaks inside them, gently, because it gets a steady level.

## Signal flow

```
input ─┬──────────────────────────────► × gain ──► Output, Bypass (base processor)
       │                                   ▲
       └─► weighting ─► level (RMS) ─► gain computer ─► smoothing (Speed)
                                           ▲
                 sidechain (optional) ─► weighting ─► level ─┘  (Relative mode: the target follows the music)
```

### Level detection

- **Weighting:** a high-pass at 80 Hz (12 dB/oct) and a +4 dB high shelf at 2 kHz, roughly the K-weighting loudness meters use. Low rumble and handling noise count less, and presence counts more, so the level follows how loud the voice *sounds*.
- **Level:** RMS over about 200 ms (the short-term loudness of a syllable or a bowed note), stereo-linked (one gain for both channels, from their combined level).

### The gain computer

- **Wanted gain** = Target − level, limited to **Max Boost** upwards and **Max Cut** downwards.
- **Floor:** while the level is below the Floor (a pause, breaths, stage noise between phrases), the gain **holds** where it was instead of rising. Without this, every pause would make the rider turn up the room noise, and the next phrase would start too loud.
- **Return:** if the silence lasts longer than **Return** (default 3 s), the gain drifts back to **0 dB** (over about a second). 0 dB is the rider's neutral starting point, so after a long pause (between songs, a long instrumental) every phrase starts from it, not from wherever the last phrase left the gain. Short pauses between phrases still hold.
- **In Relative mode** the Target isn't a fixed level but **Above Music** dB over the sidechain's level (for example a backing track), measured with the same weighting. The voice then stays a fixed amount above the music: louder in a loud chorus, softer in a quiet bridge. While the sidechain is silent (between songs), the target falls back to the absolute Target.

### Smoothing

- The gain moves towards the wanted gain with the **Speed** time (100 ms to 3 s, default 500 ms), as a one-pole glide in dB. That's slow enough to sound like a hand on a fader, never like pumping.
- Going **down** is twice as fast as going up (a loud entry is pulled back sooner than a soft one is lifted), as engineers ride.
- The gain is applied smoothly per sample, so it never clicks.

## Controls

| Control | ID | Range | Default |
| --- | --- | --- | --- |
| Target | `target` | −40 to 0 dB | −18 dB |
| Max Boost | `max_boost` | 0 to 18 dB | 6 dB |
| Max Cut | `max_cut` | 0 to 18 dB | 6 dB |
| Speed | `speed` | 100 ms to 3 s | 500 ms |
| Floor | `floor` | −70 to −20 dB | −50 dB |
| Return | `return` | 1 to 10 s | 3 s |
| Mode | `mode` | Absolute, Relative | Absolute |
| Above Music (Relative only, dimmed otherwise) | `above` | −12 to +12 dB | +3 dB |

Plus the RKFX common controls: **Output** and **Bypass**. There's **no Dry/Wet**: blending a ridden signal with the original only makes the riding weaker, which Max Boost and Max Cut already do. Every control is a host parameter, with permanent IDs.

## The window

About **760 × 470 px** (like RKCompress):

- **Ride graph** (the main display): the last 6 seconds scrolling from right to left. It shows the input level (a dim filled area), the Target (a horizontal line; in Relative mode, a line that follows the music), the Floor (a dotted line), and the **gain** as a bright line around a 0 dB centre (up = boost, down = cut, up to the Max Boost / Max Cut range drawn as a band). You can see at a glance what the rider is doing and why.
- **Fader:** a vertical fader cap next to the graph, showing the current gain, moving like a real fader (display only).
- **Knobs:** Target, Max Boost, Max Cut, Speed, Floor; Mode and Above Music.

**Cost:** the graph keeps about 180 points (6 s at 30 per second) in a ring buffer filled from the audio thread's atomics, and repaints at 30 fps only while the plugin is processing and its window is open.

## In a rig

- **Gig Performer:** first on the vocal or instrument channel, before RKCompress. For Relative mode, wire the backing track (or the band submix) to the plugin's sidechain input.
- **RKBoard:** a pedal ("Ride"). The board doesn't feed sidechains, so on the board RKRide works in Absolute mode only (Relative behaves as Absolute).

## Identity

- **AU code** `Rid1`, bundle ID `com.rkmusic.rkride`.
- **Dynamics family**, accent **honey `#E3B450`**.
- Sidechain input (stereo, as RKCompress and RKGate).

## Factory presets

| Preset | Idea |
| --- | --- |
| Vocal Rider | Target −18 dB, ±6 dB, Speed 500 ms, Floor −50 dB |
| Gentle Vocal | ±3 dB, Speed 1 s: a light touch for a steady singer |
| Violin Phrases | Target −20 dB, Boost 8 / Cut 4 dB, Speed 300 ms: soft passages come forward |
| Above the Track | Relative, Above Music +3 dB, ±8 dB, Speed 700 ms: a voice over a backing track |
| Announcements | Target −16 dB, ±10 dB, Speed 200 ms, Floor −45 dB: talking between songs |
| Bass Evener | Target −20 dB, ±4 dB, Speed 800 ms, Floor −55 dB |

## CPU and safety

- Per sample: four SVF stages (weighting, two channels), one mean square, a log every few samples, one gain. Far under 0.1% of a core.
- Allocation-free after prepare(). No feedback paths. The gain is always inside ±18 dB and smoothed.
- Zero latency, no lookahead. RKRide is slow by design, so it doesn't need to see ahead.

## Tests

- **Converges:** a steady tone at −30 dB ends at the Target (−18 dB) within 0.3 dB; at −40 dB it stops at Max Boost; at 0 dB it stops at Max Cut.
- **Speed:** the gain covers about 63% of a step change in the Speed time going up, and in half of it going down.
- **Floor and Return:** after a phrase, a pause shorter than Return holds the gain where it was, and the next phrase starts from it. A pause longer than Return brings the gain back to 0 dB.
- **Relative:** with a sidechain 6 dB louder, the target rises 6 dB; with a silent sidechain it falls back to the absolute Target.
- **Weighting:** a 40 Hz tone counts clearly less than a 1 kHz tone of the same level.
- **No clicks:** a level step of 20 dB and every control swept while playing produce no step larger than the signal's own.
- **Presets, board, validation:** RKFXPresetTests, RKBoardTests, auval (`aufx Rid1 RKMu`), pluginval at strictness 10.

## As built (2026-10-29)

- **Window 900 × 600** (not 760 × 470): the ride graph needs the height to show both lanes readably. The graph's gain lane is scaled to the Max Boost / Max Cut range (a little more), so small rides are visible; the fader keeps a fixed ±18 dB scale. Mode and Above Music sit in the bottom row.
- The graph stands still while no audio is processed (the processor flags fresh meter data).
- The gain computer runs every 16 samples; the gain glides and is applied per sample.
- **CPU measured** (RKFXBench, 48 kHz, 64 samples, stereo): 0.03% of one core.

## Decisions (2026-10-29)

- **Long pauses return to 0 dB:** after Return (default 3 s) below the Floor, the gain drifts back to 0 dB, the neutral start for every phrase after a long silence. Short pauses hold.
