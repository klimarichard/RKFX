# RKCue

**MIDI file sampler: keys play MIDI files (MIDI)** · MIDI family (lilac) · MIDI effect: MIDI in, MIDI out · no latency

RKCue is a sampler whose samples are MIDI files. Put a MIDI file in a **slot**, give the slot a key, and pressing that key plays the file out of RKCue, into whatever instruments come after it in Gig Performer: a drum fill, a riser, an arpeggio, an intro phrase, a bass riff. Give a slot a range of keys instead, and the other keys play the file **transposed**, like a melodic sampler: hold C and the riff plays in C, press F and it plays in F.

Put RKCue between your MIDI input and the instruments. Keys that don't belong to a slot play on through RKCue as usual.

For the header, presets and Bypass, see [Common Controls](Common%20Controls.md). As a MIDI effect it has no Dry/Wet and no Output. Bypassed, MIDI passes through unchanged, and every file that is playing ends its notes first.

It isn't RKConduct: there, the file follows your taps beat by beat. An RKCue file, once started, plays on its own at its tempo.

## The window

- **Zones (top):** the whole MIDI keyboard, with each slot's keys drawn as a bar above it (the selected slot in full colour). Drag a bar's ends to change its Low and High key. With Track Pitch on, the dot is the Root key: drag it too. Keys light up as you play them. Click a bar or a key to select its slot.
- **Pads (left):** the 16 slots, slot 1 at the bottom left as on a pad controller. A pad shows its name and keys, and lights up while it plays, with a line for how far it has got. Click a pad to select it; click its **▶** to play it (held, like a key). **Drop a MIDI file from Finder onto a pad** to load it; drop several and they fill that pad and the ones after it.
- **Slot (right):** the selected slot's file and settings (below).
- **Bottom row:** Tempo Source, Tempo, Trigger Channel, Thru and **Stop All**.

## A slot

At the top: the slot's name (double-click it to rename), **Load**, **Clear**, **Learn** and **Stop**. Below it: the file's length (in beats, and in seconds at its own tempo), its tempo and its tracks with their note counts. All of a file's tracks play.

**Learn:** click it and press a key: the slot is now on that key (Low, High and Root). **Shift-click** Learn and press two keys: the slot covers the range between them, and the first key is the Root.

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Low Key, High Key** | C-2 to G8 | slot 1: C1, slot 2: C#1 ... slot 16: D#2 | The keys that play the slot. One key when both are the same. |
| **Root Key** | C-2 to G8 | the slot's key | The key that plays the file as written (with Track Pitch on). |
| **Track Pitch** | Off, on | Off | On: the other keys in the zone transpose the file, a semitone per key from the Root. Off: every key in the zone plays the file as written. |
| **Transpose** | -24 to +24 semitones | 0 | Moves every note of the file, on top of Track Pitch. |
| **Keep Drums** | Off, on | On | Channel 10 (the drum channel in General MIDI files) is never transposed, so the drums in a phrase stay the same drums. |
| **Play Mode** | One-shot, Gate, Toggle | One-shot | See below. |
| **Loop** | Off, on | Off | At the end, the file starts again without a pause, until it is stopped. |
| **Poly** | Off, on | Off | Off: pressing the slot again restarts it. On: each press starts another copy (up to 8), so you can play a transposed riff as a chord, or let a phrase ring while you start it again. |
| **Velocity** | 0 to 100 % | 100 % | How much the velocity of your key scales the file's velocities. 100 %: play softly and the file plays softly. 0 %: the file's own velocities. |
| **Channel** | As recorded, 1 to 16 | As recorded | Where the slot's notes go. Send each slot to a different instrument in Gig Performer (with a MIDI channel filter there). |

A note that would be transposed beyond the keyboard (above G8 or below C-2) is left out.

### Play Mode

| Mode | Key down | Key up | Pressed again while playing |
| --- | --- | --- | --- |
| **One-shot** | starts | nothing: the file plays to its end | starts it again (Poly: another copy) |
| **Gate** | starts | stops | (the key is up between presses) |
| **Toggle** | starts | nothing | stops |

Whatever stops a file (the end, the key, Stop, Stop All, loading another file into the slot, Bypass), the notes it is sounding end at once. Two copies sounding the same note don't cut each other short: the note ends when the last of them ends it.

## Tempo

| Tempo Source | A file plays at |
| --- | --- |
| **File** (default) | its own tempo, tempo changes included |
| **Host** | Gig Performer's tempo, and follows it when it changes. Without a host tempo, the file's own. |
| **Fixed** | the **Tempo** setting (20 to 300 BPM) |

A file starts on the exact moment of your key: RKCue doesn't wait for a beat, and adds no delay.

## Trigger keys and the rest of the keyboard

The keys of a slot that holds a file are **taken in**: they start the file and don't reach the instruments themselves. All other keys, and all other MIDI (sustain pedal, pitch bend, controllers), pass through when **Thru** is on (default). With Thru off, only the files' notes go out.

**Trigger Channel:** Any (default), or 1 to 16. Only notes on that channel play slots; the others pass through. Use it to play slots from one keyboard (or a pad controller) while another keyboard plays the instruments through the same RKCue.

## Gig Performer buttons

Every slot has a **Play** and a **Stop** parameter, and there is a **Stop All** parameter, so a footswitch or a Gig Performer button can fire a slot with no keyboard at all. Play works like the slot's Root key: pressed is key down (velocity 100), released is key up (which stops a Gate slot). Every slot setting is a parameter too, if you want to change a slot from a widget.

## Presets

A preset you save keeps the slots' files. Each RKCue preset is a **folder** holding the settings (`preset.json`) and a copy of every slot's MIDI file (`01 Drum Fill.mid`, `05 Bass Riff.mid` ...). Copy the folder to another computer and the preset plays the same files. A preset's empty slots are emptied when you load it. If a MIDI file has been taken out of the folder, its slot loads empty and the header names the missing file.

A saved Gig Performer gig keeps the files inside RKCue's settings, wherever the files were.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Factory presets hold only the settings: loading one keeps your files.

| Factory preset | Settings |
| --- | --- |
| **16 Pads from C1** | Each slot on one key, C1 to D#2, One-shot (the default) |
| **16 Pads, Gate** | The same keys; a file plays while its key is held |
| **8 Pads, Toggle Loops** | Slots 1 to 8 (C1 to G1) loop, started and stopped from their keys |
| **Riff on the Keyboard** | Slot 1 on C2 to C4, Root C3, Track Pitch and Poly: play the riff in any key, or as chords |
| **Split, Riff and Pads** | Slot 1 on C3 to C5 (Root C4) with Track Pitch and Poly; slots 2 to 9 single pads from C1 |

## Notes

- Choose trigger keys you don't need for playing, at the bottom of the keyboard, or play the slots from a pad controller on its own Trigger Channel.
- A file that doesn't end on a whole bar loops early. Make the file's length (its end-of-track) a whole number of bars when you build it.
- Standard MIDI files only (.mid), timed in beats; files timed in SMPTE frames aren't read.
- Up to 16 tracks per file are read; up to 32 copies play at once across all slots (the oldest makes way).
