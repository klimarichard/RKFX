# RKRide

**Automatic level rider** · Dynamics family (honey) · zero latency

RKRide does what a sound engineer's hand does on a vocal fader: it slowly turns a phrase up when it's too quiet and down when it's too loud, so the voice or instrument sits at a steady level. It works over hundreds of milliseconds, not on single peaks, so it doesn't change the shape of notes the way a compressor does. It only moves the level, like a fader.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKRide has no Dry/Wet: blending the ridden signal with the original would only make the riding weaker, and Max Boost and Max Cut already set how far it goes.

## Where it goes

First on the channel, **before** a compressor: RKRide evens out the phrases, and the compressor then only deals with the peaks inside them. Good for:

- a singer who moves towards and away from the mic, or sings much softer in verses than in choruses;
- a violin or acoustic instrument whose soft passages disappear in the mix;
- announcements between songs, from voices of very different loudness.

## How it works

1. RKRide measures how loud the signal **sounds**: low rumble and handling noise count less, the presence range counts more, averaged over about 200 ms.
2. It works out how far that is from the **Target**, and moves the gain towards it, by at most **Max Boost** up or **Max Cut** down.
3. It moves at the **Speed**: slowly, like a hand on a fader. Turning down is twice as fast as turning up, as an engineer would ride.
4. In a **pause** (below the **Floor**) it holds the gain where it was, so it doesn't turn up the room noise and the next phrase doesn't start too loud.
5. If the pause lasts longer than **Return**, the gain drifts back to **0 dB**, so after a long silence (between songs, a long instrumental) every phrase starts from neutral.

There's no lookahead, so it adds no latency. It's slow on purpose: catching single peaks is the compressor's job.

## The display

- **Ride graph**, the last 6 seconds, scrolling from right to left:
  - **Upper lane:** the input level (shaded; gray while below the Floor), the **Target** (a colored line; in Relative mode it moves with the music) and the **Floor** (dotted).
  - **Lower lane, Gain:** the range Max Boost / Max Cut allow (a light band) and the gain RKRide is applying (a white line; up is louder).
- **Fader**, on the right: the gain right now, from -18 to +18 dB, moving like a real fader.

The display moves only while audio is playing.

## Rider

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Target** | -40 to 0 dB | -18 dB | The level RKRide steers towards. Set it where the voice sits well in the mix. |
| **Max Boost** | 0 to 18 dB | 6 dB | The most it will turn up. |
| **Max Cut** | 0 to 18 dB | 6 dB | The most it will turn down. |
| **Speed** | 100 ms to 3 s | 500 ms | How quickly it follows (going down, twice this fast). Slower is more natural; faster follows single words. |
| **Floor** | -70 to -20 dB | -50 dB | Below this level, RKRide considers it a pause and holds. Set it just above the noise between phrases (stage, breaths, amp hiss). |
| **Return** | 1 to 10 s | 3 s | How long a pause lasts before the gain drifts back to 0 dB. |

## Mode (bottom row)

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Absolute / Relative** | | Absolute | **Absolute:** the Target is a fixed level. **Relative:** the target follows the music on RKRide's sidechain input, so the voice stays a set amount above it: louder in a loud chorus, softer in a quiet bridge. While the music is silent (between songs), the fixed Target is used. |
| **Above Music** | -12 to +12 dB | +3 dB | Relative mode only (dimmed otherwise): how far above the music the voice is kept. |

Plus **Output** and **Bypass**.

### Using Relative mode in Gig Performer

RKRide has a second stereo input, labeled Sidechain. Wire the backing track (or the band submix) to it and switch to **Relative**. The main input is the voice or instrument being ridden. On **RKBoard** (where it's the **Ride** pedal) there is no sidechain, so it always works in Absolute mode.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Vocal Rider** | Target -18 dB, Max Boost 6 dB, Max Cut 6 dB, Speed 500 ms, Floor -50 dB, Return 3 s |
| **Gentle Vocal** | Max Boost 3 dB, Max Cut 3 dB, Speed 1 s: a light touch for a steady singer |
| **Violin Phrases** | Target -20 dB, Max Boost 8 dB, Max Cut 4 dB, Speed 300 ms: soft passages come forward |
| **Above the Track** | Relative, Above Music +3 dB, Max Boost 8 dB, Max Cut 8 dB, Speed 700 ms: a voice over a backing track |
| **Announcements** | Target -16 dB, Max Boost 10 dB, Max Cut 10 dB, Speed 200 ms, Floor -45 dB: talking between songs |
| **Bass Evener** | Target -20 dB, Max Boost 4 dB, Max Cut 4 dB, Speed 800 ms, Floor -55 dB |

## Notes

- Setting the Target: sing or play a typical passage and watch the graph. The Target line should run through the middle of the shaded level, and the white gain line should move gently around 0 dB.
- If the gain keeps sitting at the top or bottom of its band, the Target is too far from your level: move the Target rather than widening Max Boost or Max Cut.
- If quiet stage noise still gets turned up, raise the Floor.
- Gig Performer tip: map **Target** to a widget to bring a voice forward for a solo, or **Above Music** in Relative mode.
