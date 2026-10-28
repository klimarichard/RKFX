# RKDeEss

**De-esser** · Dynamics family (pale yellow) · zero latency

RKDeEss tames harsh "s", "sh" and "t" sounds (sibilance) in vocals, without dulling the voice the rest of the time. It can also calm squeaky strings, piercing cymbals or any narrow, spiky high-frequency harshness.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKDeEss has no Dry/Wet.

## How it works

1. A **detector** listens only to the high frequencies, above the **Frequency** setting, where sibilance lives.
2. When that band gets too loud (see **Detection** and **Threshold**), the de-esser turns it down, by up to the **Range**. Its timing is fixed and fast, tuned for sibilance.
3. In **Split-Band** mode only the frequencies above the setting are turned down; in **Wideband** mode the whole signal is.

When nothing needs de-essing, the signal passes through completely untouched.

## Gain reduction meter

The meter on the left shows how much is being turned down right now, from 0 to -24 dB. It should move on "s" sounds and stay still on vowels.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Relative / Absolute** | Relative, absolute | Relative | How the detector decides. **Relative**: compares the sibilance band with the overall level of the voice, so it reacts the same in a quiet verse and a loud chorus (recommended for live). **Absolute**: compares the band with a fixed threshold, like a classic de-esser. |
| **Split-Band / Wideband** | Split-band, wideband | Split-Band | **Split-Band**: only the highs above the Frequency are turned down; the body of the voice stays untouched (transparent). **Wideband**: the whole signal dips briefly on each "s" (a more classic, slightly more obvious sound, useful when the harshness is spread out). |
| **Frequency** | 2 to 12 kHz | 6 kHz | Where sibilance starts. Typical: 4 to 6 kHz for male voices, 6 to 9 kHz for female voices. Use **Listen** to find the spot that sounds the most "spitty". |
| **Threshold** (Relative) | -24 to +12 dB | 0 dB | How loud the sibilance may get, relative to the rest of the voice, before it's reduced. 0 dB catches typical "s" sounds; lower values catch more (and milder) sibilance; higher values only the worst. |
| **Threshold** (Absolute) | -60 to 0 dB | -24 dB | The fixed level the sibilance band must exceed. |
| **Range** | 0 to 24 dB | 8 dB | The most the sibilance can be turned down. 4 to 8 dB is natural; more starts to sound lispy. |
| **Listen** | Off, on | Off | For setting up only: the output becomes what the detector hears (just the highs), so you can tune Frequency by ear. The switch lights orange while on. Don't forget to switch it off! |
| **Link** | Off, on | On | Stereo link. On: both sides are de-essed together, keeping the stereo image stable. |

The knob shows the threshold for the selected detection mode; each mode remembers its own threshold.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Gentle Vocal** | Relative, Split-Band, Frequency 6 kHz, Threshold 0 dB, Range 6 dB |
| **Bright Female Vocal** | Relative, Split-Band, Frequency 7.5 kHz, Threshold -3 dB, Range 8 dB |
| **Male Vocal** | Relative, Split-Band, Frequency 5 kHz, Threshold 0 dB, Range 8 dB |
| **Harsh Sibilance Fix** | Relative, Split-Band, Frequency 6.5 kHz, Threshold -6 dB, Range 14 dB |
| **Classic Wideband** | Relative, Wideband, Frequency 6 kHz, Threshold 0 dB, Range 6 dB |
| **Cymbal Tamer** | Relative, Split-Band, Frequency 9 kHz, Threshold -6 dB, Range 6 dB |
| **Fixed Threshold** | Absolute, Split-Band, Frequency 6 kHz, Threshold -30 dB, Range 8 dB |

## Notes

- Put RKDeEss after EQ and compression on a vocal: those tend to bring sibilance up.
- If the voice starts to sound lispy or dull, lower the Range or raise the Threshold.
- Gig Performer tip: different singers (or songs) often need a different Frequency; save one preset per voice.
