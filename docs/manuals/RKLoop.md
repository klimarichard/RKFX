# RKLoop

**Live looper, eight loops** · Delay family (mint green) · zero latency

RKLoop records phrases and plays them back as loops while you keep playing over them. It has **eight loops**: use them as **parts** of a song (verse, chorus, solo; one plays at a time and you switch between them) or as **layers** that play together on top of each other. With **Sync** on, every loop stays in step with Loop 1. Add overdubs, take the last one back (undo), play loops backwards or an octave down. A few footswitches run it all, like a looper pedal. A good take can be saved as a WAV file, even from a footswitch in the middle of a song.

For the header, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKLoop has no Dry/Wet: your playing always passes through, and **Loop Level** sets the loops.

## The window

- **Loops:** one tile per loop, with its number, its role (Part or Layer), its length and how far it has played. A playing loop is lit, a recording one shows in orange, and a loop waiting to start (see Sync and switching) blinks. Click a tile to select that loop.
- **Quick save** and **Export** (top right of the Loops section): save loops as WAV files (see Saving loops). A short message next to them says what was saved, or what went wrong.
- **Selected loop:** the state of the selected loop, the footswitches, which act on it, and its role.

## Playing a loop

The footswitches act on the **selected** loop:

| You press | When the loop is | It does |
| --- | --- | --- |
| **Record** | empty | Starts recording. |
| **Record** (shown as Play) | recording | Ends the recording: the loop plays from the start, seamlessly. |
| **Record** (shown as Overdub) | playing | Starts overdubbing: what you play is added to the loop. |
| **Record** (shown as Play) | overdubbing | Ends the overdub; the loop keeps playing. |
| **Record** (shown as Play) | stopped | Plays the loop. |
| **Record** | waiting to start | Cancels the start. |
| **Stop** | a **part** | Stops the whole song: every loop. |
| **Stop** | a **layer** | Stops just that layer. |
| **Undo** / **Redo** | playing or stopped | Takes the loop's last overdub out, or puts it back (about 0.2 s, while the loop keeps playing). |
| **Clear** | any | Erases the loop. |
| **◀ Prev** / **Next ▶** | | Selects the previous or next loop (around from 8 to 1). |

## Parts and layers

Each loop has a role, set with **Part / Layer** below the footswitches (for the selected loop):

- **Parts** are song sections: one part plays at a time. Choosing a recorded part (with Next, Prev, a click or the Loop parameter) while another part plays **switches at the end of the playing part's loop**, so the change lands on its downbeat; the tile blinks until then. Choose the playing part again to cancel. Recording a new part takes over from the one playing.
- **Layers** play on top of the part and of each other. Each layer is on (Record, shown as Play) or off (Stop) on its own.

A recording or a stopped loop can always be selected for overdubbing, undoing or clearing; only parts switch what plays.

## Sync

With **Sync** on (the default), **Loop 1 sets the time**:

- Record Loop 1 first; its length is the reference.
- While anything plays, any other loop starts recording or playing on **Loop 1's next downbeat** (its tile blinks while it waits). With nothing playing, it starts at once, and that is the downbeat.
- When you end a recording, its length becomes the nearest **quarter, half, or whole multiple** of Loop 1 (1x, 2x, 3x...). If you end it a little early, it keeps recording up to that point; a little late, the extra is dropped and the loop plays on from exactly where it should. Every loop stays in step.

With Sync off, or before Loop 1 exists, loops are free: they start at once and are exactly as long as you play them. Part switches still happen at the end of the playing part.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Loop** | 1 to 8 | 1 | The selected loop (also a host parameter, for a Gig Performer widget). |
| **Part / Layer** | Part, layer | Part | The selected loop's role. |
| **Sync** | Off, on | On | Keeps the loops in step with Loop 1 (see Sync). |
| **Reverse** | Off, on | Off | Plays every loop backwards (overdubs then go in backwards too). |
| **Half Speed** | Off, on | Off | Plays every loop at half speed, an octave down. Overdubbing pauses while it is on. |
| **Loop Level** | 0 to 100% | 100% | The loops' volume under your playing. |

Each loop can be up to **30 seconds** (stereo); reaching that while recording closes the loop.

## Saving loops

The loops live only in memory: they are gone when the session closes (see Notes). To keep a take, save it as a WAV file. **Export** opens a menu:

| Item | What it does |
| --- | --- |
| **Export Loop n...** | Saves the selected loop to a file you choose. |
| **Export all loops...** | Saves every loop that isn't empty into a new folder, inside a folder you choose. |
| **Quick save all** | Saves every loop that isn't empty, with no dialog, into a new folder in `Music/RKMusic Plugins/RKFX/Loops`. |
| **Show the Loops folder** | Opens that folder in the Finder. |
| **24-bit** / **32-bit float** | The file format (remembered on this computer). |

Right-click a loop's tile for **Export this loop...**. The **Quick save** button does the same as Quick save all, and is also a host parameter (**Save Loops**), so a footswitch can save everything in one press, in the middle of a song, without opening the window.

- **What is saved:** the loop exactly as it is stored: forward and at normal speed (Reverse and Half Speed are ignored), with the overdubs you hear now (an undone overdub is left out), and not affected by Loop Level. The files are at the session's sample rate, stereo (mono when RKLoop runs in mono), and the loop's start and end are smoothed, so the file loops cleanly.
- **Loop points:** each file carries a loop marker over the whole file, so samplers (and Logic) loop it straight away.
- **Names:** a single loop is called e.g. `RKLoop 2026-11-06 21-14 Loop 3 Layer 120 BPM.wav` (the BPM is the host's tempo when you saved, if it gives one). Export all and Quick save make a folder `RKLoop 2026-11-06 21-14` with `Loop 1 Part.wav`, `Loop 3 Layer.wav` and so on.
- **While a loop is recording or overdubbing** it can't be exported (its menu item says why). Quick save waits for it to finish, up to 5 seconds, then saves the others and says which one it skipped.
- **24-bit or float:** 24-bit is the usual choice. A loop with many overdubs can go above full scale; 32-bit float keeps that without clipping.
- Saving never interrupts the sound: the loops keep playing, and a problem (a full disk, a folder that's gone) only shows a message.

## Footswitches in Gig Performer and RKBoard

Record, Stop, Undo, Clear, Next, Previous and Save Loops are host parameters, so footswitches can press them; Loop selects a loop directly.

- **Gig Performer:** map a **momentary** button (on while held) to each. Every press acts once; a latching button would act only on every second press.
- **RKBoard:** right-click a footswitch (or Quick save, or the Sync, Reverse, Half Speed or role switch) in RKLoop's controls and assign it to an Assign slot, then map a momentary Gig Performer button to that slot (or, in the standalone app, MIDI learn the slot for a footswitch).

A typical footswitch set: **Record**, **Stop**, **Next**, **Undo**. Next both selects the next part and, while a part plays, switches to it at the end of the loop.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**.

| Factory preset | Settings |
| --- | --- |
| **Looper** | Defaults: eight parts, Sync on; Loop 1 alone is a classic single looper |
| **Song Parts** | Eight parts, Sync on |
| **Synced Layers** | Eight layers, Sync on |
| **Parts and Layers** | Loops 1 to 4 parts, 5 to 8 layers, Sync on |
| **Free Loops** | Eight parts, Sync off |
| **Backing Loop** | Loop Level 70%, so you stand out over the loops |
| **Reverse Loop** | Reverse on |
| **Octave Down Loop** | Half Speed on |

## Notes

- The loops themselves are not saved with the song or preset: a new session starts empty. Changing the sample rate also clears them. To keep a loop, save it as a WAV file (see Saving loops).
- Memory: all eight loops (and their undo copies) are reserved when RKLoop loads, about 180 MB at 48 kHz.
- Put RKLoop late on the board (after drive and modulation, before or after delay and reverb as you like): what you loop is what RKLoop hears.
- In a host that bypasses the plugin, the loops are kept; Bypass mutes them.
