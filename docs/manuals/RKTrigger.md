# RKTrigger

**Drum trigger: hits to MIDI** · MIDI family (deep orchid) · the audio passes through, or is muted

RKTrigger turns hits into MIDI notes: a piezo on a drum or a practice pad, a drum mic, a contact mic on a cajón, or a knock on a violin's body. Each hit sends a note with a velocity from how hard it was, so a sampler or drum instrument in Gig Performer plays a kick, a clap, a sample or a loop start. There are up to **two triggers**: the left and right inputs separately (**Dual**), or both summed into one (**Mono**).

For the header and presets, see [Common Controls](Common%20Controls.md). RKTrigger has no Dry/Wet and no Output, only **Bypass**, which ends any sounding note.

## Setting it up in Gig Performer

1. Put RKTrigger on the piezo's or drum mic's input. For two triggers, bring both into one stereo input (left and right) and choose **Dual**.
2. Wire RKTrigger's **MIDI output** to a sampler or drum plugin.
3. Hit softly and hard while watching the display. Set the **Threshold** just above what the other drums and the stage put into the trigger, so only real hits cross it.
4. If one hit gives two notes, raise **Mask**; if fast rolls lose notes, lower Mask or Retrigger.

## How a hit is detected

1. **High-Pass** and **Low-Pass** filter what the trigger listens to (not the sound), so a kick piezo ignores cymbal ring and a snare mic ignores the kick's boom.
2. A hit starts when the level crosses the **Threshold**.
3. For the **Scan** time (2 ms) RKTrigger keeps listening and takes the loudest point as the hit's strength. Then the note goes out: that's the only delay it adds, and it makes the velocity follow the real hit.
4. For the **Mask** time after a hit, nothing triggers again, so the drum's ringing and the piezo's bounce don't fire twice. A clearly **louder** new hit (by **Retrigger** dB) still gets through, so fast rolls and flams aren't lost.
5. After the Mask, a new hit has to rise above the last hit's fading ring, so even a long-ringing drum doesn't fire again on its own.
6. In **Dual** mode, **Crosstalk** drops a hit on one trigger that's much weaker than a hit on the other at the same moment: hitting the kick doesn't also fire the snare piezo next to it.

## The display

For each trigger, the last few seconds scroll from right to left: the level (shaded) against the Threshold (white line), and each hit as a bar as tall as its velocity. Double-triggers and missed soft hits are easy to spot while you set it up. The note name and channel of each trigger are shown on the left. Note names have middle C (60) as C3, as in Yamaha instruments and Cubase, so the General MIDI kick (36) shows as C1.

## Trigger controls

Click **Trigger 1** or **Trigger 2** to show that trigger's controls. Trigger 2 is used in Dual mode only.

### Detection

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Threshold** | -60 to 0 dB | -30 dB | A hit starts above this level. |
| **High-Pass** | Off, 20 to 500 Hz | Off | Ignores low frequencies (a snare trigger ignoring the kick). Turn fully left for Off. |
| **Low-Pass** | 1 to 16 kHz, Off | Off | Ignores high frequencies (a kick trigger ignoring cymbals). Turn fully right for Off. |
| **Scan** | 1 to 10 ms | 2 ms | How long a hit is measured before the note goes out. Shorter is quicker; longer reads the velocity of slow-rising hits better. |
| **Mask** | 5 to 200 ms | 40 ms | How long after a hit nothing triggers again. |
| **Retrigger** | Off, 3 to 24 dB | 6 dB | How much louder a new hit inside the Mask must be to get through. |

### Velocity

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Dynamics** | 6 to 48 dB | 30 dB | The span from a hit at the Threshold (Velocity Min) to the hardest hit (Velocity Max). Smaller: you reach full velocity sooner. |
| **Curve** | -100 to +100% | 0% | Positive makes soft hits louder; negative saves high velocities for hard hits. |
| **Velocity Min**, **Max** | 1 to 127 | 1, 127 | The velocity range. Set them the same for a fixed velocity. |

### Note

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Note** | 0 to 127 | 36 (C1) / 38 (D1) | The note sent: 36 is the General MIDI kick, 38 the snare. You can type a number or a name (for example "C1"). |
| **Channel** | 1 to 16 | 10 | The MIDI channel (10 is the General MIDI drum channel). |
| **Length** | 5 to 500 ms | 50 ms | When the note-off follows. Most drum samplers ignore it; some instruments need it to stop. |

## Bottom row

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Mono / Dual** | | Mono | **Mono:** one trigger from both inputs summed. **Dual:** two triggers, the left input and the right input. |
| **Crosstalk** | 0 to 100% | 50% | Dual only: a hit weaker than this share of a simultaneous hit on the other trigger is dropped. 0% keeps everything. |
| **Thru / Mute** | | Thru | **Thru:** the audio passes through untouched (a drum mic that also goes to the PA). **Mute:** the audio is silenced, for a piezo or contact mic that should only trigger. |

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Kick Piezo** | Mono, Low-Pass 2 kHz, Threshold -30 dB, Mask 50 ms, note 36 |
| **Snare Piezo** | Mono, High-Pass 100 Hz, Threshold -32 dB, Mask 30 ms, Retrigger 6 dB (rolls), note 38 |
| **Kick and Snare** | Dual: the two above on left and right, Crosstalk 50% |
| **Cajon** | Dual: bass (left, Low-Pass 300 Hz) note 36, slap (right, High-Pass 500 Hz) note 38 |
| **Body Tap** | Mono, High-Pass 200 Hz, Threshold -20 dB, Mask 150 ms, fixed velocity 100, note 60 on channel 1: a knock on the instrument as a cue |
| **Pad Only** | Mono, Audio Mute, Mask 25 ms, Dynamics 36 dB |

## Notes

- RKTrigger adds no delay to the audio. The MIDI note leaves the Scan time (2 ms) after the hit starts.
- RKTrigger isn't an RKBoard pedal: the board's pedals pass audio only.
