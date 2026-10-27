# RKGate

**Gate and expander** · Dynamics family (amber yellow) · zero latency

RKGate silences or turns down a signal when it falls below a threshold: open mics between vocal lines, amp hiss between guitar phrases, tom spill between hits. It works as a classic **gate** (open or closed) or a gentler **expander** (turns quiet parts down gradually). An external sidechain and a key filter decide exactly what opens it.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKGate has no Dry/Wet.

## How the signal flows

1. A **detector** listens to the input, or to the external sidechain (the "key").
2. The **key filter** (high-pass and low-pass) shapes what the detector hears, not the sound.
3. The **gate** or **expander** decides how much to turn the signal down.
4. **Attack, Hold and Release** shape the opening and closing in time.
5. The gain change is applied to the signal, then **Output** sets the level.

There's no lookahead, so RKGate adds no latency. At the shortest attack times, a very sharp transient can lose a fraction of its first millisecond while the gate opens.

## Gain reduction meter

The meter on the left shows how much the signal is being turned down right now, from 0 to -48 dB. A fully closed gate fills it completely.

## Gate

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Gate / Expander** | Gate, expander | Gate | **Gate**: the signal is either fully open or turned down by the Range. **Expander**: below the threshold the signal is turned down gradually, more the quieter it gets, which sounds more natural on vocals and room mics. |
| **Threshold** | -80 to 0 dB | -50 dB | The level where the gate opens. Set it just above the noise or spill you want to remove, below the quietest notes you want to keep. |
| **Ratio** | 1:1.5 to 1:10 | 1:2 | Expander only (dimmed in gate mode): how strongly quiet parts are turned down. At 1:2, a signal 5 dB below the threshold comes out 10 dB below it. |
| **Range** | 0 to -80 dB, or Closed | Closed | How far the signal is turned down when the gate is closed. **Closed** (fully left) is complete silence; -10 to -20 dB just softens noise and keeps things natural. |
| **Hysteresis** | 0 to 12 dB | 3 dB | Gate only (dimmed in expander mode): the gate closes this much below the threshold where it opened. This stops it chattering open and closed when the level hovers around the threshold. |

## Timing

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Attack** | 0.05 to 50 ms | 0.5 ms | How fast the gate opens. Short keeps drum and pick attacks intact; longer makes the opening softer (good against clicks on sustained sounds). |
| **Hold** | 0 to 500 ms | 50 ms | How long the gate stays fully open after the level drops, before it starts closing. Keeps short gaps between words or notes from being chopped. |
| **Release** | 5 ms to 2 s | 100 ms | How fast the gate closes. Short is tight (drums); longer lets notes and reverb tails fade naturally. |

## Key

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Int / Ext** | Internal, external | Int | What opens the gate. **Int**: the signal itself. **Ext**: the plugin's sidechain input, so another signal controls the gate (for example a kick drum mic opening a gate on a bass synth). With nothing connected, Ext keeps the gate closed. |
| **High-Pass** | Off, or 20 Hz to 5 kHz | Off | Removes lows from what the detector hears. Example: on a snare mic, so kick drum spill doesn't open the gate. Turn fully left for Off. |
| **Low-Pass** | Off, or 500 Hz to 20 kHz | Off | Removes highs from what the detector hears. Example: on a tom mic, so cymbals don't open the gate. Turn fully right for Off. |
| **Listen** | Off, on | Off | For setting up only: the output becomes what the detector hears, so you can tune the key filter by ear. The switch lights orange while on. Don't forget to switch it off! |
| **Link** | Off, on | On | Stereo link. On: both channels open and close together, following the louder one, so the stereo image stays stable. Off: each side is gated on its own. |

### Using the sidechain in Gig Performer

RKGate has a second stereo input, labeled Sidechain. Wire the signal that should open the gate to the plugin's sidechain input pins and set the key to **Ext**.

## Starting points

| Use | Settings |
| --- | --- |
| Vocal mic between lines | Expander, Threshold -45 dB, Ratio 1:2, Range -15 dB, Attack 1 ms, Hold 80 ms, Release 200 ms |
| Guitar amp hiss | Gate, Threshold -55 dB, Range -30 dB, Attack 0.5 ms, Hold 50 ms, Release 150 ms, Hysteresis 4 dB |
| Tom mic spill | Gate, Threshold -30 dB, Range Closed, Attack 0.1 ms, Hold 60 ms, Release 120 ms, Key Low-Pass 2 kHz |
| Snare without kick spill | Gate, Threshold -28 dB, Range -25 dB, Attack 0.1 ms, Hold 40 ms, Release 80 ms, Key High-Pass 200 Hz |
| Rhythmic gated pad (pad gated by a drum loop) | Gate, Key Ext (drum loop into the sidechain), Threshold -30 dB, Range Closed, Attack 0.5 ms, Hold 20 ms, Release 60 ms |

## Notes

- If the gate chatters (flutters open and closed), raise Hysteresis or Hold.
- If note endings are cut off, lower the Threshold or lengthen Hold and Release, or switch to Expander mode.
- Gig Performer tip: map Range to a widget to change how hard the gate works between songs, or Threshold for quick adjustment to a different stage volume.
