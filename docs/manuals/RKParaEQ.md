# RKParaEQ

**Seven-band parametric EQ** · Filter and EQ family (deep purple) · zero latency

RKParaEQ shapes the tone of any source with a high-pass filter, five fully adjustable bands and a low-pass filter. A response curve at the top shows the combined effect of every band.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKParaEQ has no Dry/Wet: blending an EQ'd signal with the original causes phase coloration, so the EQ always runs at 100%.

## How the signal flows

High-pass → Low → Low Mid → Mid → High Mid → High → Low-pass → Output

With the default settings the EQ is completely transparent: the cut filters are off and every band is at 0 dB, so inserting it changes nothing until you turn a knob.

## The response curve

The curve shows how the whole EQ changes the level at each frequency, from 20 Hz on the left to 20 kHz on the right. The center line is 0 dB (no change); the grid lines are 6 dB apart, up to ±18 dB. It is for display only: use the knobs to make changes. It redraws only when a setting changes, so it costs no CPU while you play.

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

## Sound quality

- **Zero latency**, with no oversampling and no linear-phase tricks.
- **Highs stay natural.** Many digital EQs squeeze ("cramp") bands near the top of the audio range, so a high boost gets narrower and harsher than it should. RKParaEQ's bells and shelves follow the shape of an analog EQ almost to the top of the range, so an "air" boost at 12 to 16 kHz sounds smooth.
- **Smooth changes.** Every knob glides, and switching between Shelf and Bell fades the band out, switches, and fades it back in, so even live changes never click.

## Starting points

| Use | Settings |
| --- | --- |
| Clean up a vocal mic | HP on, 24, 100 Hz; Low Mid -3 dB at 300 Hz, Q 1.5; High shelf +3 dB at 10 kHz |
| Tame a boomy acoustic guitar | HP on, 12, 90 Hz; Low Mid -4 dB at 200 Hz, Q 2 |
| Kick drum punch | Low bell +4 dB at 60 Hz, Q 1.5; Mid -4 dB at 400 Hz, Q 2; High Mid +3 dB at 4 kHz |
| Bass guitar definition | HP on, 12, 35 Hz; Mid +3 dB at 800 Hz, Q 1.2; LP on, 12, 8 kHz |
| Remove a feedback ring | The matching band at the ringing frequency, -9 dB, Q 8 |
| Thin keyboard pad for the mix | HP on, 24, 200 Hz; High Mid -2 dB at 3 kHz, Q 1 |

## Notes

- Boosting several bands at once makes the output louder; lower Output to compensate.
- Gig Performer tip: map Mid Gain or High Gain to a widget for a quick "presence" control during a song, or map a band's On switch to bring a corrective cut in and out.
