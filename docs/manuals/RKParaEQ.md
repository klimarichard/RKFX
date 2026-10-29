# RKParaEQ

**Seven-band parametric EQ, with dynamic bands** · Filter and EQ family (deep purple) · zero latency

RKParaEQ shapes the tone of any source with a high-pass filter, five fully adjustable bands and a low-pass filter. A response curve at the top shows the combined effect of every band. Any of the five gain bands can also be made **dynamic**, so it moves by itself depending on how loud its frequency range is: a cut that only happens when a vocal gets harsh, or a lift that only happens when it's soft.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKParaEQ has no Dry/Wet: blending an EQ'd signal with the original causes phase coloration, so the EQ always runs at 100%.

## How the signal flows

High-pass → Low → Low Mid → Mid → High Mid → High → Low-pass → Output

With the default settings the EQ is completely transparent: the cut filters are off and every band is at 0 dB, so inserting it changes nothing until you turn a knob.

## The response curve

The curve shows how the whole EQ changes the level at each frequency, from 20 Hz on the left to 20 kHz on the right. The center line is 0 dB (no change); the grid lines are 6 dB apart, up to ±18 dB. It is for display only: use the knobs to make changes. It redraws only when a setting changes, so it costs no CPU while you play.

When a band is dynamic, two more things appear: a **white curve** showing the EQ as it is right now, with the dynamic bands moving, and a **bracket** at each dynamic band's frequency, from its Gain to Gain + Range, showing how far it can move. The white curve only redraws while a dynamic band is actually moving.

## The bands

Each band has an **On** switch in its title row. A band that's off has dimmed knobs and doesn't affect the sound; you can still set it up before switching it on. Switching a band on or off glides smoothly, so it never clicks.

### HP (high-pass) and LP (low-pass)

| Control | HP range | LP range | Default | What it does |
| --- | --- | --- | --- | --- |
| **On** | | | Off | Switches the cut filter on. |
| **Slope (12 / 24 / 36 / 48)** | | | 12 | How steeply frequencies are removed, in dB per octave. 12 is gentle; 48 is a "brick wall". |
| **Freq** | 20 Hz to 1 kHz | 1 to 20 kHz | 80 Hz (HP), 12 kHz (LP) | HP removes everything below this frequency (rumble, stage noise, boominess); LP removes everything above it (hiss, harshness, fizz). |

### Low and High

| Control | Low range | High range | Default | What it does |
| --- | --- | --- | --- | --- |
| **On** | | | On | Switches the band on. |
| **Shelf / Bell** | | | Shelf | **Shelf** raises or lowers everything below (Low) or above (High) the frequency, like a bass or treble knob. **Bell** affects only a band around the frequency. |
| **Freq** | 20 Hz to 1 kHz | 1 to 20 kHz | 100 Hz (Low), 8 kHz (High) | Where the band works. For a shelf, the point where the change is half way. |
| **Gain** | ±18 dB | ±18 dB | 0 dB | How much to boost (+) or cut (-). |
| **Q** | 0.3 to 10 | 0.3 to 10 | 0.71 | Width of the bell: low Q = wide and gentle, high Q = narrow and precise. Dimmed in Shelf mode, where it has no effect. |

### Low Mid, Mid and High Mid

Always bells.

| Control | Low Mid | Mid | High Mid | What it does |
| --- | --- | --- | --- | --- |
| **On** | On | On | On | Switches the band on. |
| **Freq** | 40 Hz to 2 kHz (default 250 Hz) | 200 Hz to 8 kHz (default 1 kHz) | 600 Hz to 16 kHz (default 3.5 kHz) | Center of the bell. |
| **Gain** | ±18 dB, default 0 dB | | | Boost or cut at the center. |
| **Q** | 0.3 to 10, default 1.0 | | | Width of the bell. |

The ranges overlap, so two bands can work close together, for example a wide boost with a narrow cut inside it.

## Dynamic bands

Each of the five gain bands (Low, Low Mid, Mid, High Mid, High) has a **Dynamic** switch at the bottom of its strip. When it's on, the band listens to its own frequency range and adds a moving amount of gain on top of its **Gain**:

- **Range below 0 dB (cut):** when the band's level goes **above** the Threshold, the band is turned down, by at most the Range. Harshness on loud notes, boom on one ringing low note, sibilance: tamed only when they appear.
- **Range above 0 dB (boost):** when the band's level falls **below** the Threshold, the band is turned up, by at most the Range. Presence on soft passages, without making the loud ones louder. The boost only works down to about 30 dB below the threshold, then fades out, so pauses, hiss and room noise between notes are never lifted.

A bell with Gain +3 dB and Range -6 dB moves between +3 and -3 dB. A dynamic band with Gain 0 dB is a plain "only when needed" cut or boost.

**Which band's dynamics you see:** click a gain band's strip (anywhere outside its knobs) to select it; it gets an outline, and the **dynamics row** under the strips shows its controls:

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Dynamic** | Off, on | Off | Switches the band's dynamics on (the same switch as at the bottom of the strip). Off, the band is a plain EQ band. |
| **Threshold** | -60 to 0 dB | -30 dB | The level of the band's frequency range where the band starts to move. |
| **Range** | -24 to +24 dB | -6 dB | How far the band can move: negative cuts when loud, positive boosts when soft, 0 does nothing. |
| **Ratio** | 1.5:1 to 10:1 | 3:1 | How quickly the band moves once past the threshold. 2:1 is gentle, 4:1 and up firm. |
| **Attack** | 1 to 100 ms | 10 ms | How fast the band moves away from its Gain. |
| **Release** | 10 ms to 2 s | 150 ms | How fast it comes back. |

Next to the knobs, two meters help you set it up: **Level** shows what the band hears, with a white mark at the Threshold (play loud and see where the bar reaches), and **Gain** shows how far the band has moved right now.

What each band listens to matches its shape: a bell hears a band around its frequency (as narrow as its Q), a low shelf everything below its frequency, a high shelf everything above. They all listen after the high-pass filter, so rumble you've filtered out doesn't trigger the Low band. Stereo is linked: both sides move together.

## Sound quality

- **Zero latency**, with no oversampling and no linear-phase tricks.
- **Highs stay natural.** Many digital EQs squeeze ("cramp") bands near the top of the audio range, so a high boost gets narrower and harsher than it should. RKParaEQ's bells and shelves follow the shape of an analog EQ almost to the top of the range, so an "air" boost at 12 to 16 kHz sounds smooth.
- **Smooth changes.** Every knob glides, and switching between Shelf and Bell fades the band out, switches, and fades it back in, so even live changes never click. Switching Dynamic on or off, or turning Range from a cut to a boost, fades the dynamic part out and in the same way.
- **Dynamic bands at zero latency.** There's no lookahead, so a very sudden peak can pass for up to the Attack time before the band moves, as with any RKFX compressor.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version. (“Remove a feedback ring” is a technique rather than a fixed setting, so it isn't a preset.)

| Factory preset | Settings |
| --- | --- |
| **Vocal Mic Cleanup** | HP on, 24, 100 Hz; Low Mid -3 dB at 300 Hz, Q 1.5; High shelf +3 dB at 10 kHz |
| **Boomy Acoustic Fix** | HP on, 12, 90 Hz; Low Mid -4 dB at 200 Hz, Q 2 |
| **Kick Punch** | Low bell +4 dB at 60 Hz, Q 1.5; Mid -4 dB at 400 Hz, Q 2; High Mid +3 dB at 4 kHz |
| **Bass Definition** | HP on, 12, 35 Hz; Mid +3 dB at 800 Hz, Q 1.2; LP on, 12, 8 kHz |
| Remove a feedback ring | The matching band at the ringing frequency, -9 dB, Q 8 |
| **Thin Keyboard Pad** | HP on, 24, 200 Hz; High Mid -2 dB at 3 kHz, Q 1 |
| **Vocal Harshness Tamer** | HP on, 12, 80 Hz; High Mid at 3.2 kHz, Q 2, Dynamic: Threshold -32 dB, Range -6 dB, Ratio 3:1, Attack 5 ms, Release 100 ms |
| **Soft De-Ess** | High shelf at 6.5 kHz, Dynamic: Threshold -36 dB, Range -8 dB, Ratio 4:1, Attack 1 ms, Release 60 ms |
| **Bass Boom Control** | Low bell at 110 Hz, Q 1.5, Dynamic: Threshold -24 dB, Range -6 dB, Ratio 3:1, Attack 20 ms, Release 200 ms |
| **Presence Lift** | High Mid at 2.5 kHz, Q 1, Dynamic: Threshold -36 dB, Range +4 dB, Ratio 2:1, Attack 20 ms, Release 300 ms |
| **Acoustic Body Control** | Low Mid -2 dB at 220 Hz, Q 1.4, Dynamic: Threshold -28 dB, Range -4 dB, Ratio 3:1, Attack 15 ms, Release 150 ms |

## Notes

- Boosting several bands at once makes the output louder; lower Output to compensate.
- Gig Performer tip: map Mid Gain or High Gain to a widget for a quick "presence" control during a song, or map a band's On switch to bring a corrective cut in and out.
- The thresholds of the dynamic presets depend on your input level: play loud, watch the Level meter, and set the Threshold just under where the loud notes reach.
