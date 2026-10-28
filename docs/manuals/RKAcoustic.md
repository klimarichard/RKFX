# RKAcoustic

**Acoustic preamp** · Filter and EQ family (amethyst) · zero latency

RKAcoustic is a preamp for instruments with a piezo pickup: violin, viola, acoustic guitar, upright bass. A piezo sits under the bridge or saddle, so it hears the strings but hardly the wooden body, and it adds a hard, nasal edge (the "quack"). RKAcoustic puts the body back, takes the quack out, and adds what a stage preamp has: a low cut, EQ, a notch against feedback, a phase switch and gentle compression.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKAcoustic has no Dry/Wet: a preamp shapes the whole signal.

## How it works

The signal runs through these steps in order:

1. **High-Pass** removes rumble, stage thumps and handling noise below the instrument.
2. **Phase** flips the polarity.
3. **Body** adds the resonances of the instrument's wooden body that the pickup misses, for the chosen voicing.
4. **Quack** cuts the piezo's hard peak, at a frequency that suits the voicing.
5. **EQ:** Low, Mid (sweepable), Treble and Air.
6. **Notch** cuts one narrow band deeply, against feedback.
7. **Compress** evens out the level.

Put RKAcoustic **first** on the board or in the rackspace: every effect after it sounds better when the pickup sounds like the instrument. The body voicings are starting points; every instrument and pickup is different, so trust your ears.

## Pickup

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Flat / Violin / Viola / Guitar / Bass** | Flat, violin, viola, guitar, upright bass | Guitar | The instrument: sets the body resonances and where Quack cuts. **Violin**: body around 280 and 470 Hz, quack at 3 kHz. **Viola**: a little lower (230 and 380 Hz, quack at 2.6 kHz). **Guitar**: body at 100 and 200 Hz, quack at 1.6 kHz. **Bass** (upright): 65 and 110 Hz, quack at 900 Hz. **Flat**: no body, quack at 2.5 kHz. |
| **Phase** | Off, on | Off | Flips the polarity. On stage, if the instrument starts to feed back (boom or howl), try it: one of the two settings usually feeds back less. Also try it when you blend with a microphone or a second pickup and the sound gets thin. |
| **Body** | 0 to 100% | 50% | How much of the body resonance is added (and how much of the boxy region is cut). 0% leaves the pickup as it is. |
| **Quack** | 0 to 100% | 30% | How much of the piezo's hard peak is cut, up to 12 dB. |
| **High-Pass** | Off, 20 to 300 Hz | Off | Low cut, 24 dB/oct. About 80 Hz for violin, 60 to 80 Hz for guitar, 30 to 40 Hz for upright bass. Higher values clean up a crowded mix or a loud stage. |
| **Notch** | Off, 40 Hz to 2 kHz | Off | A narrow, deep cut (-30 dB) against feedback. When the instrument starts to feed back, sweep it slowly until the howl goes away. Body feedback on guitar is usually between 100 and 250 Hz, on violin between 250 and 600 Hz. |
| **Compress** | 0 to 100% | 0% | Gentle compression for an even level: 0% is off, 100% is 4:1 from -30 dB with a soft knee. A little makeup gain (up to about 6 dB) keeps the level up. |

## EQ

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Low** | -12 to +12 dB | 0 dB | Low shelf at 100 Hz: warmth, or less boom. |
| **Mid** | -12 to +12 dB | 0 dB | Bell at **Mid Freq**. Cut around 300 to 500 Hz for less boxiness; boost around 1 to 2 kHz to cut through a band. |
| **Mid Freq** | 200 Hz to 5 kHz | 800 Hz | Where Mid works. |
| **Treble** | -12 to +12 dB | 0 dB | High shelf at 4 kHz: presence, or less edge. |
| **Air** | 0 to +12 dB | 0 dB | High shelf at 12 kHz: sparkle and bow or pick detail, above the harsh region. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Violin Stage** | Violin, Body 60%, Quack 40%, High-Pass 80 Hz, Air +2 dB, Compress 20% |
| **Warm Viola** | Viola, Body 65%, Quack 35%, High-Pass 70 Hz, Treble -1 dB, Compress 20% |
| **Strummed Guitar** | Guitar, Body 50%, Quack 40%, High-Pass 70 Hz, Mid -2 dB at 400 Hz, Compress 35% |
| **Upright Bass** | Bass, Body 60%, Quack 40%, High-Pass 35 Hz, Compress 30% |
| **Loud Stage** | Guitar, Body 30%, Quack 40%, High-Pass 100 Hz, Low -3 dB, Notch 200 Hz, Compress 20% |
| **Violin into Effects** | Violin, Body 40%, Quack 50%, High-Pass 120 Hz, Air +3 dB, Compress 40%: a tighter, even signal for drive, wah and delays |

## Notes

- Feedback on a loud stage: first raise High-Pass and lower Body (body resonances are where feedback starts), then try Phase, then sweep Notch to the howl.
- Before distortion or an octave pedal, more High-Pass and some Compress help them track and stay clean.
- Gig Performer tip: map Body or Air to a widget to go from an intimate to a cutting sound between songs.
