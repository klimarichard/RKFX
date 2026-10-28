# RKVibrato

**Pitch vibrato** · Modulation family (powder blue) · zero latency

RKVibrato makes the pitch waver up and down, like a singer's or violinist's vibrato, a whammy-bar shimmer or a warped-tape wobble. Unlike a chorus, there's no dry signal mixed in: you only hear the pitch moving. A **Rise** time lets the vibrato fade in after each new note, just like a string player who starts a note straight and then adds vibrato.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKVibrato has no Dry/Wet (mixing the dry signal in would turn it into a chorus). A mono input gives a stereo output, so Stereo Phase works on mono sources.

## How it works

The sound passes through a very short delay whose length moves smoothly up and down; a delay that's getting shorter raises the pitch, one that's getting longer lowers it. RKVibrato calculates that movement so the pitch swing is always exactly what **Depth** says, at any **Rate**.

The delay adds a few milliseconds of offset while Depth is up, as in any vibrato pedal; that's part of how vibrato works and isn't reported as latency.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Sine / Triangle** | Sine, triangle | Sine | The shape of the pitch movement. **Sine**: smooth and natural. **Triangle**: the pitch moves at a steady speed and turns sharply at the top and bottom, a more mechanical, "warbly" sound. |
| **Rate** (Sync off) | 0.1 to 15 Hz | 5 Hz | Speed of the vibrato. 4 to 7 Hz is a natural singing or string vibrato; slow rates give a seasick wobble; fast rates a nervous flutter. |
| **Rate** (Sync on) | 1/1 to 1/32, dotted (D) and triplet (T) | 1/16 | Speed as a note value, locked to the beat when the host transport runs. |
| **Sync** | Off, on | Off | Off: Rate in Hz. On: Rate as a note value. |
| **Depth** | 0 to 100% | 30% | How far the pitch moves: 100% is ±1 semitone; 30% is a natural, gentle vibrato. The swing is the same at every rate. |
| **Stereo Phase** | 0 to 180° | 0° | Offsets the right channel's vibrato. 0°: both sides together. Higher values make the two sides waver against each other, a wide, shimmering stereo effect. |
| **Rise** | 0 to 2 s | 0 s | Fades the vibrato in after each new note. 0 s: always at full depth. 0.3 to 0.8 s: the natural "straight first, then vibrato" of a singer or string player. New notes are recognized by how suddenly the level rises, so soft and loud playing behave the same. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Classic Vibrato** | Sine, Rate 5 Hz, Depth 30% |
| **Violin Vibrato** | Sine, Rate 5.5 Hz, Depth 25%, Rise 0.4 s |
| **Singer Vibrato** | Sine, Rate 5 Hz, Depth 60%, Rise 0.6 s |
| **Warped Tape** | Triangle, Rate 1.2 Hz, Depth 40% |
| **Synced Pitch Wobble** | Sine, Sync on, 1/8, Depth 50% |
| **Stereo Shimmer** | Sine, Rate 6 Hz, Depth 20%, Stereo Phase 180° |

## Notes

- For a classic guitar "vibrato pedal" sound, use Sine, 5 to 7 Hz, Depth 40 to 60%.
- Gig Performer tip: map Depth to an expression pedal for a hand-controlled vibrato, or Rate to bring it from slow to fast.
