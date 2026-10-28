# RKConduct

**MIDI file player, conducted by taps (MIDI)** · MIDI family (plum) · MIDI effect: MIDI in, MIDI out

RKConduct plays a MIDI file, but you set the time: tap the beats (on two keys of your keyboard, a footswitch, or a Gig Performer button) and the file follows you. You don't have to tap every note: tap only every quarter note, and a 1/16 pattern in between still plays evenly, at the tempo you are tapping. Speed up, slow down, hold back: the music goes with you, like an orchestra with its conductor. Put it between your MIDI input and the instruments in Gig Performer.

For the header, presets and Bypass, see [Common Controls](Common%20Controls.md). As a MIDI effect it has no Dry/Wet and no Output; bypassed, MIDI passes through unchanged (and the file's notes end first).

## How it follows you

Each tap says "we are at the next beat now" (every **Tap Every**, a quarter note by default):

- **Between taps** the file plays at the tempo measured from your last taps, so the notes in between keep their spacing.
- **If you tap late**, the file waits at the beat for you (notes already playing keep sounding) and goes on with your tap. It never runs ahead of you.
- **If you tap early**, it jumps straight to that beat: the notes it hadn't reached yet are skipped (notes already playing end), and it takes your faster tempo from then on.
- **The first tap** starts the file at the beginning, at the file's own tempo (or the Tempo setting); from the second tap on, the tempo is yours. With **Count-in** taps, those come first and set the tempo before the music starts.
- A second tap sooner than a third of a beat after the last one is ignored (a bounced key or a double press).
- At the end the file stops (the next tap starts it again), or loops.

There is no delay in RKConduct itself; the only thing that takes a tap to catch up is a change of tempo, which the next beat follows.

## Song

Click **Load MIDI file** and choose a Standard MIDI file (.mid). The file is stored inside RKConduct's settings, so a saved Gig Performer gig always has it, wherever the file was.

**Time** (next to the Load button) is the song's time signature, 4/4 unless you change it: beats per bar, and the beat's note value (3/4, 6/8, 7/8...). It sets how bars are counted in the display and how long a bar is for **Tap Every: Bar**. (RKConduct doesn't read it from the file.)

Every track in the file is listed with its number of notes:

| Control | What it does |
| --- | --- |
| **On** (tick) | Plays the track, or leaves it out. |
| **As recorded / Channel 1 to 16** | The MIDI channel the track goes out on. Send each track to a different instrument in Gig Performer (with a MIDI channel filter there), for example the bass to one synth and the arpeggio to another. |

## Conduct

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Tap / Stop** | | | Tap the beat with the mouse, or map a Gig Performer button or footswitch to the Tap and Stop parameters. **Stop** ends the notes and goes back to the beginning. |
| **Tap Every** | 1/1, dotted 1/2, 1/2, dotted 1/4, 1/4, dotted 1/8, 1/8, 1/16, Bar | 1/4 | How far apart your taps are in the music: every quarter note, every half note, a whole note (1/1)... **Bar** taps once per bar of the Time setting (three quarter notes in 3/4). Dotted quarters suit 6/8. |
| **File Tempo** | Off, on | On | Starts at the tempo stored in the MIDI file. Off: at the Tempo below. |
| **Tempo** | 20 to 300 BPM | 120 | The starting tempo, until your taps give one. |
| **Count-in** | 0 to 8 | 0 | Taps before the music starts; they set the tempo for the first beat. 4 = one bar of 4/4 counted in. |
| **Loop** | Off, on | Off | Starts again from the beginning at the end, without a pause. |
| **Tap Key 1, Tap Key 2** | Off, any note | C1, D1 | Either key taps (the two are there so you can alternate fingers or hands). These keys are taken in; the rest of the keyboard plays on through RKConduct. |
| **Tap CC** | Off, CC 0 to 127 | Off | A MIDI controller that taps (a footswitch sending CC 64, for example), on each press. |

The window shows the bar and beat, the tempo you are tapping, and whether the file is waiting for your tap.

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. (Presets hold the settings, not the MIDI file.)

| Factory preset | Settings |
| --- | --- |
| **Tap Quarter Notes** | Tap every 1/4 |
| **Tap Half Notes** | Tap every 1/2 |
| **Tap Every Bar** | Tap every 1/1 |
| **Tap Eighths** | Tap every 1/8 |
| **Count In a Bar** | Tap every 1/4, 4 count-in taps |
| **Footswitch Taps** | Tap every 1/4, no tap keys, taps from CC 64 |
| **Waltz, Tap Every Bar** | 3/4, tap once a bar |

## Notes

- Choose tap keys you never need to play, at the bottom of the keyboard.
- Tapping half notes or whole bars leaves your hands freer, but a tempo change then takes longer to follow; quarter notes are the best balance for most music.
- The file's markers aren't used; set the time signature with Time.
