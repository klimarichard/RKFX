# RKDelay

**Stereo delay** · Delay family (green) · zero latency

RKDelay repeats the sound as echoes. It has three characters (clean **digital**, warm **analog** and wobbly **tape**), stereo, mono and **ping-pong** modes, tempo sync, filters that shape the repeats, modulation, **ducking**, which keeps the echoes out of the way while you play, and **Shimmer**, which makes each repeat climb in pitch.

For the header, knobs, presets, Dry/Wet, Output and Bypass, see [Common Controls](Common%20Controls.md). A mono input gives a stereo output, so ping-pong works on mono sources.

## How the signal flows

1. The input goes into a delay line (up to 4 seconds).
2. Each repeat passes through the **character** (digital, analog or tape) and the **Low Cut** and **High Cut** filters.
3. **Feedback** sends the repeat back into the delay for more echoes. A soft limiter in that loop means even 100% feedback sustains without ever running away. With **Shimmer** up, part of each repeat is pitch-shifted on its way back, so the echoes climb.
4. **Ducking** turns the echoes down while the input is loud.
5. **Dry/Wet** blends the echoes with the original, then **Output** sets the level.

The original signal is never delayed, so RKDelay adds no latency.

## Delay

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Digital / Analog / Tape** | Digital, analog, tape | Digital | The character of the repeats. **Digital**: clean, exact copies; only the cut filters shape them. **Analog**: like a bucket-brigade pedal, each repeat gets darker and gently saturated, so the echoes melt away. **Tape**: wow and flutter make the repeats waver slightly in pitch, with tape-style saturation and a slight loss of highs. Switching glides, so it never clicks. |
| **Mono / Stereo / Ping-Pong** | Mono, stereo, ping-pong | Stereo | **Mono**: one delay for both sides. **Stereo**: left and right have their own delays (see R Offset). **Ping-pong**: the echoes bounce from left to right and back. |
| **Time** (Sync off) | 1 ms to 2 s | 375 ms | Time between echoes. Changing it while echoes are ringing glides smoothly, with the tape-like pitch bend of a real delay. |
| **Time** (Sync on) | 1/1 to 1/32, dotted (D) and triplet (T) | 1/8 D | Time as a note value, following the song tempo. Dotted eighths are the classic "U2"-style rhythmic delay. Times longer than 4 s (slow tempos with long notes) are limited to 4 s. |
| **Sync** | Off, on | Off | Off: Time in ms. On: Time as a note value. |
| **Feedback** | 0 to 100% | 35% | How many repeats: 0% is a single echo; higher values give longer trails; 100% sustains (self-oscillating textures) without running away. |
| **R Offset** | -50% to +50% | 0% | Makes the right side's time shorter or longer than the left's, for a wider, more complex stereo echo. Dimmed in Mono mode. |

## Feedback Tone

These filters sit inside the feedback loop, so every repeat is filtered again: the echoes get progressively thinner (Low Cut) or darker (High Cut).

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Low Cut** | 20 Hz to 1 kHz | 100 Hz | Removes lows from the echoes, keeping them from muddying the bass. |
| **High Cut** | 1 to 20 kHz | 8 kHz | Removes highs from the echoes, so they sit behind the original. |

## Modulation

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Depth** | 0 to 100% | 0% | Gently moves the delay time up and down, adding a chorus-like shimmer to the repeats. Works in every mode, on top of the tape wow and flutter. |
| **Rate** | 0.1 to 5 Hz | 0.6 Hz | Speed of the modulation. Dimmed while Depth is 0. |

## Shimmer

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Shimmer** | 0 to 100% | 0% | How much of the feedback is pitch-shifted. The first echo is always at your pitch; at 100% the next is an octave up (with Octave up), the next two octaves up, each a little darker through the Feedback Tone filters. In between, shifted and unshifted repeats sound together. At 0% it's switched off entirely. |
| **Interval** | Octave up, Fifth up, Octave + fifth up, Two octaves up, Octave down | Octave up | How far each repeat climbs (or falls, with Octave down). Dimmed while Shimmer is 0%. |

**The repeats stay on the beat.** The pitch shifter needs about 20 ms to work, so RKDelay sends the shifted repeats back that much earlier: they still arrive at the delay time (within a few milliseconds, inaudible as timing), synced or free. Only at very short times (under about 30 ms) do they come slightly late, where shimmer becomes a texture rather than an echo. Feedback stays as safe as without shimmer: the feedback is only split between shifted and unshifted repeats.

## Bottom row

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Ducking** | 0 to 100% | 0% | Turns the echoes down while you play and lets them come up in the gaps, so a vocal or solo stays clear while the delay fills the spaces. |

Plus the standard **Dry/Wet** (default 30%), **Output** and **Bypass**. When Bypass is switched on, the echoes stop at once; switching back on starts fresh.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Rhythmic Guitar Delay** | Digital, Stereo, Sync 1/8 D, Feedback 35%, Low Cut 150 Hz, High Cut 6 kHz, Dry/Wet 30% |
| **Warm Analog Lead** | Analog, Stereo, Time 420 ms, Feedback 45%, Mod Depth 20%, Ducking 50%, Dry/Wet 35% |
| **Slapback** | Tape, Mono, Time 110 ms, Feedback 10%, High Cut 5 kHz, Dry/Wet 30% (try Digital for a cleaner slap) |
| **Vintage Tape Echo** | Tape, Mono, Time 330 ms, Feedback 50%, Low Cut 200 Hz, High Cut 5 kHz, Dry/Wet 35% |
| **Wide Ping-Pong Keys** | Digital, Ping-Pong, Sync 1/4, Feedback 40%, High Cut 7 kHz, Dry/Wet 25% |
| **Clear Vocal Throw** | Analog, Stereo, Sync 1/4, Feedback 30%, Ducking 80%, Dry/Wet 30% |
| **Ambient Wash** | Tape, Ping-Pong, Time 600 ms, Feedback 90 to 100%, Mod Depth 40%, Dry/Wet 50% |
| **Shimmer Echo** | Digital, Stereo, Sync 1/4 D, Feedback 55%, Shimmer 60%, Octave up, Dry/Wet 35%: climbing repeats |
| **Crystal Fifths** | Tape, Ping-Pong, Sync 1/8, Feedback 50%, Shimmer 50%, Fifth up, Dry/Wet 35% |
| **Sub Drop** | Analog, Stereo, Sync 1/4, Feedback 45%, Shimmer 40%, Octave down, Dry/Wet 35%: repeats falling into darkness |

## Notes

- High feedback in Digital mode with the filters wide open repeats almost forever; lower Feedback or the High Cut for trails that fade.
- With Shimmer, a lower High Cut keeps the higher octaves from getting shrill.
- Gig Performer tip: map Shimmer to an expression pedal to bring the climbing repeats in for a solo, Feedback for dub-style swells, or Dry/Wet to a widget to throw echoes on single words.
