# RKLoop

**Live looper** · Delay family (mint green) · zero latency

RKLoop records a phrase and plays it back as a loop while you keep playing over it; add layers (overdub), take the last one back (undo), play the loop backwards or an octave down. One footswitch does most of it, like a looper pedal.

For the header, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKLoop has no Dry/Wet: your playing always passes through, and **Loop Level** sets the loop.

## Playing it

| You press | When | It does |
| --- | --- | --- |
| **Record** | empty | Starts recording. |
| **Record** (shown as Play) | recording | Ends the recording: the loop plays from the start, seamlessly. |
| **Record** (shown as Overdub) | playing | Starts overdubbing: what you play is added to the loop. |
| **Record** (shown as Play) | overdubbing | Ends the overdub; the loop keeps playing. |
| **Record** (shown as Play) | stopped | Plays the loop from the start. |
| **Stop** | recording, playing, overdubbing | Stops (a recording becomes the loop first). The loop is kept. |
| **Undo** / **Redo** | playing, stopped (an overdub ends first) | Takes the last overdub out, or puts it back. Takes a moment (about 0.2 s for a one-minute loop), while the loop keeps playing. |
| **Clear** | any | Erases the loop. |

The status line shows the state, the position and the loop length; the bar shows where the loop is.

The loop's start and end get short fades, and stopping, starting and clearing fade too, so nothing clicks. A loop can be up to **60 seconds** (stereo); reaching that while recording closes the loop and plays it.

## Controls

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Reverse** | Off, on | Off | Plays the loop backwards (overdubs then go in backwards too). |
| **Half Speed** | Off, on | Off | Plays the loop at half speed, an octave down. Overdubbing pauses while it is on. |
| **Loop Level** | 0 to 100% | 100% | The loop's volume under your playing. |

## Footswitches in Gig Performer and RKBoard

Record, Stop, Undo and Clear are host parameters, so footswitches can press them:

- **Gig Performer:** map a **momentary** button (on while held) to each. Every press acts once; a latching button would act only on every second press.
- **RKBoard:** right-click a button (Record, Stop, Undo, Clear) in RKLoop's controls and assign it to an Assign slot, then map a momentary Gig Performer button to that slot (or, in the standalone app, MIDI learn the slot for a footswitch).

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**.

| Factory preset | Settings |
| --- | --- |
| **Looper** | Loop Level 100% |
| **Backing Loop** | Loop Level 70%, so you stand out over the loop |
| **Reverse Loop** | Reverse on |
| **Octave Down Loop** | Half Speed on |

## Notes

- The loop itself is not saved with the song or preset: a new session starts empty. Changing the sample rate also clears it.
- Put RKLoop late on the board (after drive and modulation, before or after delay and reverb as you like): what you loop is what RKLoop hears.
- In a host that starts and stops the plugin (Bypass), the loop is kept; Bypass mutes it.
