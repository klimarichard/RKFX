# RKSustain

**Sustain, sostenuto, hold and latch (MIDI)** · MIDI family (light pink) · MIDI effect: MIDI in, MIDI out, no delay

RKSustain holds notes after you release the keys. It adds a sustain pedal to instruments that don't respond to one, gives you a real sostenuto (only the notes you hold when you press the pedal ring on), lets pads ring until your next chord without any pedal, or latches notes on and off. The pedal can be your keyboard's sustain pedal or a Gig Performer footswitch. Put it between your MIDI input and the instrument.

For the header, presets and Bypass, see [Common Controls](Common%20Controls.md). As a MIDI effect it has no Dry/Wet and no Output; bypassed, MIDI passes through unchanged (and any note it was holding ends first).

## Mode

| Mode | What it does |
| --- | --- |
| **Sustain** | Like a sustain pedal: while the pedal is down, notes keep sounding after you release their keys. Lifting the pedal ends them (a key still held keeps its note). |
| **Sostenuto** | Only the notes you are holding when you press the pedal keep sounding; notes you play afterwards end normally. Hold a bass note or a chord, press the pedal, and play freely above it. |
| **Hold** | Every note keeps sounding after you release it, until you play again after all keys were up: the next chord replaces the last. No pedal needed; pressing the pedal ends everything. Ideal for pads and drones under a melody on another instrument. |
| **Latch** | Each key switches its note: press once to start it, press again to end it. Pressing the pedal ends all of them. |

A note that is being held and played again restarts (it ends and starts again), as on a piano.

## Pedal

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Pedal** | Off, on | Off | The pedal as a parameter: map a Gig Performer footswitch or widget to it. |
| **From MIDI** | Off, on | On | Also take the pedal from a MIDI controller (your keyboard's pedal). |
| **Controller** | 0 to 127 | 64 | That controller's number: 64 is the sustain pedal, 66 the sostenuto pedal. RKSustain takes it in (it doesn't pass it on), so the instrument doesn't sustain on its own as well. |

Everything else (other controllers, pitch bend) passes through.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**.

| Factory preset | Settings |
| --- | --- |
| **Sustain Pedal** | Sustain, pedal from controller 64 |
| **Sostenuto Pedal** | Sostenuto, pedal from controller 64 |
| **Hold Until Next Chord** | Hold |
| **Latch Notes** | Latch |
| **Footswitch Sustain** | Sustain, pedal from the Pedal parameter only |

## Notes

- Put RKSustain last, just before the instrument, so it holds what the other MIDI effects make.
- With RKArpeggiate, use the arpeggiator's own Latch rather than RKSustain's Hold.
