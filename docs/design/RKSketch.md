# RKSketch: design

**A small MIDI file builder: draw notes and drums for up to 128 bars, hear them, drag the .mid out, without opening a DAW** · MIDI family · standalone app only · no audio processing

RK + a verb, as every RKFX plugin: to *sketch* (a song idea, a backing part, a click track with a count-in, a file for RKConduct). This is a design, not a manual: it says what the app does and how, so it can be built and reviewed against it.

## What it does

RKSketch is a quick, focused editor for **short Standard MIDI Files**: a bass line for a rehearsal, a pad part for a live rig, a drum groove, a song file for RKConduct, a melody to send to a bandmate. It opens in a second, starts with an empty 8-bar song, and every edit is one step away from a finished `.mid`.

- **Up to 16 tracks**, each on its own MIDI channel, **up to 128 bars**, tempo and time-signature changes on any bar.
- **Piano roll** for melodic tracks and a **drum grid** for drum tracks (named GM drum rows).
- **Velocity and controller lanes** (mod wheel, sustain, expression, pitch bend, any CC).
- **Bar tools** for the whole song: insert, delete, duplicate and repeat bars, so a 4-bar idea becomes a 64-bar part in a few clicks.
- **Hear it** with the Mac's built-in General MIDI sounds (no setup), or send it to any MIDI output (a synth, an IAC bus into Gig Performer).
- **Play it in:** step entry and real-time recording from a MIDI keyboard, with a count-in and metronome.
- **The .mid is the document.** Save writes a plain Standard MIDI File that any DAW, RKConduct or a phone opens; RKSketch keeps its own settings inside the same file, so nothing is lost when it is opened again.
- **Drag the file out** of the window straight into Finder, a DAW, Mail or RKConduct.

What it is **not**: no audio tracks, no plugins, no mixer, no notation (score) view, no arranger clips. Those are what the DAW is for.

## Words used here

- **Song:** everything in one file: tracks, length, tempo and time-signature map, key, markers.
- **Track:** one part on one MIDI channel, melodic or drums.
- **Bar range:** a selection of whole bars across all tracks (for the bar tools).
- **Grid:** the snap value for drawing and moving notes (1/1 to 1/32, straight, triplet or dotted).

## The song

| Setting | Values | Default |
| --- | --- | --- |
| **Length** | 1 to 128 bars | 8 |
| **Tempo** | 20 to 300 BPM, 0.01 steps; a change on any bar (a jump, or a ramp across bars) | 120 |
| **Time signature** | 1 to 16 over 2, 4, 8 or 16; a change on any bar | 4/4 |
| **Key** | any major or minor key, or none | none |
| **Markers** | a name on any bar (Intro, Verse, Chorus...) | none |
| **Count-in** | 0, 1 or 2 bars of click written into the file on its own track | 0 |

- **Key** highlights the scale's rows in the piano roll and is written as a key-signature event; it never changes notes.
- **Markers** are written as marker meta events, so a DAW and RKConduct show them.
- **Count-in** (for a backing file a band starts to): a click track (GM side stick and hi-hat by default) placed before bar 1; the bar numbers stay 1-based for the music.
- **Tempo ramps** are written as tempo events every 1/16 note across the ramp.

## Tracks

| Setting | Values | Default |
| --- | --- | --- |
| **Name** | text (written as the track name) | "Track n" |
| **Kind** | Melodic, Drums | Melodic (Drums for a new drum track) |
| **Channel** | 1 to 16 | the first free one (10 for drums) |
| **Sound** | a GM program (128, by family), or none (no program change written) | Acoustic Grand Piano |
| **Bank** | none, or MSB / LSB (for an external synth) | none |
| **Volume, Pan** | CC 7 and CC 10 at the start, or not written | not written |
| **Mute, Solo** | for listening only; a muted track is still saved | off |
| **Colour** | the suite's track colours | by number |

- Up to **16 tracks**; two tracks can share a channel (e.g. a left-hand and a right-hand piano part).
- **Export only** marks a track to leave out of the file (a guide part or a click for you only).
- A track list on the left: name, colour, kind icon, mute, solo, an activity light while playing. Drag to reorder; right-click to duplicate, delete, or change the kind.

## Editing

### Piano roll (melodic tracks)

- **Draw:** click to place a note of the current length (the grid, or the last note drawn), drag right to make it longer.
- **Select:** click, Shift-click, drag a rectangle, Cmd-A; select all notes of one pitch by clicking its key.
- **Move and resize:** drag a note (Alt-drag copies); drag its end to resize; arrow keys move by the grid (up and down by a semitone, Shift by an octave).
- **Delete:** double-click, Backspace, or right-drag across notes to erase.
- **Snap** to the grid, off with Cmd held. The grid also sets how notes are quantized.
- **Hear while editing:** a note sounds when it is drawn, clicked or moved (can be turned off).
- The keyboard on the left shows note names with middle C = C3; the key's scale rows are shaded.
- **Ghost notes:** the other tracks shown faintly behind (off, one chosen track, or all).

### Drum grid (drum tracks)

- One row per drum, named by the GM drum map (Kick, Snare, Closed Hat...); only the rows in use plus the common kit are shown, the full 47 on request. A custom row list (with names) can be saved for an external kit.
- **Click a cell** to add a hit, click again to remove it; drag across cells to paint.
- Each hit has a velocity (three quick levels by Alt-click: soft, medium, accent, or any value in the lane).
- **Flam and roll** on a cell (two quick hits, or a roll across the cell at 1/32 or 1/64).

### Lanes under the editor

- **Velocity** bars for the notes on screen; drag to set, draw a line across several for a ramp.
- **Controllers:** one lane at a time, chosen from Mod wheel (1), Expression (11), Sustain (64, drawn as on and off), Pitch bend, Channel pressure, or any CC number. Draw points, lines or a smooth curve; a curve is written as points at the grid.

### Note tools (on the selection, or the whole track)

| Tool | What it does |
| --- | --- |
| **Quantize** | to the grid, with a strength (100% by default) and swing (50 to 75%) |
| **Transpose** | semitones, octaves, or in the key (scale steps) |
| **Velocity** | set, scale (%), add, compress toward a value, random ± |
| **Length** | set, scale, legato (each note to the next), staccato (50%) |
| **Humanize** | small random timing (± ms) and velocity |
| **Reverse, Invert** | in time, or pitch around a note |
| **Chord** | stamp a chord on each selected note (major, minor, 7th, sus... in root position or an inversion) |

### Bar tools (all tracks, on a bar range)

Select bars in the ruler (drag across bar numbers).

| Tool | What it does |
| --- | --- |
| **Duplicate** | the range is copied after itself |
| **Repeat** | the range copied *n* times after itself |
| **Insert** | *n* empty bars before the range |
| **Delete** | the bars removed, the rest moves up |
| **Clear** | notes and controllers removed, bars kept |
| **Copy / Paste** | bars from one place to another (paste replaces or merges) |

Tempo, time-signature and marker events move with their bars. The song never goes past 128 bars; a tool that would is refused with a message.

### Undo

Every edit is undoable (Cmd-Z, Shift-Cmd-Z), without limit until the file is closed.

## Playing it in

- **MIDI input:** any keyboard or controller (all inputs, or one chosen); plays the selected track's sound while not recording (so it doubles as a little sound module).
- **Step entry** (a toggle): each note or chord played on the keyboard is placed at the cursor with the current length, and the cursor moves on by that length. Space (or the sustain pedal) adds a rest; Backspace removes the last step.
- **Record** (real time): from the cursor or a loop, with a count-in of 0, 1 or 2 bars and the metronome. **Replace** or **Merge** (overdub, each pass on top while looping). Input quantize off or to the grid. Pitch bend, mod wheel and sustain are recorded into their lanes.
- **Computer keyboard as a piano** (A to K for a white-key octave, Z and X for the octave) when no MIDI keyboard is around.

## Listening

- **Transport:** Play / Stop (Space), Return to start, a **loop** on a bar range (set by the ruler), the cursor follows the playhead (or not).
- **Metronome** (on / off, level) on its own, never written to the file (the Count-in track is).
- **Sound:** **Built-in** (default), the Mac's General MIDI sound set (Apple's DLS synth, hosted inside RKSketch: 16 channels, drums on channel 10, no setup), or **MIDI out** to a chosen port, or both. Each track's program and bank are sent on Play, so an external synth is set up too.
- **Output** in the app's audio settings (as every RKFX app: device, rate, buffer).
- Stopping sends note-offs and resets sustain and pitch bend on every channel; **Panic** (all notes off on all ports) in the transport.

## Files

- **Save** writes a Standard MIDI File, **type 1** (a tempo track plus one track per RKSketch track), **480 ticks per quarter note**. In it: track names, the tempo, time-signature and key maps, markers, program and bank at the start of each track, the notes and controllers.
- **RKSketch's own settings** (track kinds, colours, mute and solo, the grid, the drum row names, the loop, the view) are kept in one sequencer-specific meta event (FF 7F) in the tempo track, as JSON. Every other program ignores it; RKSketch reads it back. The file is the only thing to keep.
- **Export as...** writes a copy with other choices: type 0 (one track, for phones and some hardware), without the count-in, without the RKSketch event, or only the selected tracks or bar range.
- **Drag out:** a file icon in the header can be dragged into Finder, a DAW, Mail or RKConduct; it carries the current song (saved first to a temporary copy if it has unsaved changes).
- **Open** any .mid (type 0 or 1): type 0 is split by channel into tracks, channel 10 becomes a drum track, anything past 128 bars is cut with a warning, events RKSketch doesn't edit (SysEx, other meta events) are kept and written back unchanged.
- **Drop** a .mid on the window or the Dock icon to open it.
- **Templates:** a new song starts empty, or from a template (a saved song chosen as a start, e.g. "Band: drums, bass, keys"). Factory templates are built in.
- **Autosave** every 30 s to a recovery copy; after a crash, RKSketch offers it at the next start. Recent files in the File menu.

## The window

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ [▶][■][⟲][●]  Bar 5.3.1  120.00 BPM  4/4  C major   Grid 1/16  [⇩ .mid]  ▤ │ header
├───────────────┬──────────────────────────────────────────────────────────────┤
│               │ 1    2    3    4   [Verse]   6    7    8                   │ ruler (bars, markers, loop, tempo)
│ ● Drums    MS │ ▪▪ ▪ ▪▪▪  ▪▪ ▪ ▪▪▪  ▪▪ ▪ ▪▪▪  ▪▪ ▪ ▪▪▪                     │ overview: every track,
│ ● Bass     MS │ ─ ─  ──   ─ ─  ──   ─ ─  ──   ─ ─  ──                      │ the whole song,
│ ● Keys     MS │ ═══  ═══  ═══  ═══                                         │ bar ranges selected here
│ + track       ├──────────────────────────────────────────────────────────────┤
│               │ C4 ▕                                                        │
│ Bass          │    ▕   ███     ███                                          │ editor: piano roll or
│ Ch 2          │ C3 ▕ ███    ███   ██                                        │ drum grid of the
│ Fingered Bass │    ▕                                                        │ selected track
│ Vol – Pan –   │ C2 ▕                                                        │
│               ├──────────────────────────────────────────────────────────────┤
│               │ Velocity ▾  ▌▌ ▌ ▌▌▌ ▌                                      │ lane
└───────────────┴──────────────────────────────────────────────────────────────┘
```

- **Header:** transport, position, tempo, time signature, key (click to edit at the cursor's bar), grid, the drag-out file icon, a settings menu (audio and MIDI, sound source, preferences).
- **Overview:** every track as a strip of small note blocks across the whole song (up to 128 bars always fit, scrolling only when the window is narrow); click a track's strip to edit it, drag across bars for the bar tools.
- **Editor:** zoom with Cmd-scroll (time) and Alt-scroll (pitch); the lane's height can be dragged.
- The suite's look (core `LookAndFeel`, dark), a default window of 1200 × 760, resizable. Note names middle C = C3.

### Keyboard shortcuts

Space play / stop, Return to start, R record, L loop on the selection, 1 to 6 grid (1/1 to 1/32), T triplet, D draw, S select, Q quantize, Cmd-D duplicate, Cmd-↑↓ transpose an octave, Up / Down select the track, Cmd-S save, Shift-Cmd-E export, Cmd-Z undo.

## Identity

- **App:** RKSketch.app in /Applications, bundle `com.rkmusic.rksketch`; registered as an editor for `.mid` / `.midi` (Open With, not the default unless the user chooses).
- **Icon:** the RKMusic name strip as the other apps (from `tools/icons`).
- **Standalone only.** As a plugin it would only duplicate the DAW's own editor; it can become an AU / VST3 MIDI generator later (it plays the song in time with the host) if there's a use.

## Architecture

- A plain JUCE GUI application (`juce_add_gui_app`), not the plugin wrapper: `JUCEApplication`, a `DocumentWindow`, `AudioDeviceManager` for audio and MIDI. Uses core `gui` (look and feel, knobs, switches) and `PresetFormat`-style JSON; it doesn't need `ProcessorBase`.
- **Model** (`SketchSong`, message thread): tracks of notes (start and length in ticks, pitch, velocity, release velocity) and controller points, the tempo / time-signature / key / marker maps, the length. Every edit is an `UndoableAction` on a `juce::UndoManager`.
- **Playback:** each edit builds a new immutable `PlaySnapshot` (all events sorted in ticks, with the tempo map) handed to the audio thread through an atomic pointer swap (the old one freed on the message thread). The audio callback walks the snapshot by sample, sends events sample-accurately to the built-in synth and to a `MidiOutput` (with its timestamped buffer), and the metronome clicks are generated directly.
- **Built-in sound:** Apple's DLS synth (`AudioUnit aumu dls  appl`) hosted with JUCE's `AudioUnitPluginFormat`; its output is the app's audio. If it fails to load, MIDI out still works and the header says so.
- **Files:** `SketchMidiFile` reads and writes through `juce::MidiFile` (type 1) with its own type 0 split and merge, the FF 7F settings event, and pass-through of unknown events.
- **Tick maths** in one place (`TempoMap`: ticks ↔ bars.beats.ticks ↔ seconds), with tempo ramps; used by the ruler, playback and export, so they never disagree.

## Build phases

One stacked branch per phase; the user tries each in the app.

1. **Editor and file** (`feat/rksketch`): the app shell and window, song settings (length, one tempo, one time signature), melodic tracks, the piano roll with velocity lane, undo, Save / Open / drag out, type 1 export.
2. **Playback** (`feat/rksketch-play`): transport, loop, metronome, the built-in GM sound, MIDI out, audition while editing, Panic.
3. **Drums and controllers** (`feat/rksketch-drums`): drum tracks and the drum grid, controller and pitch-bend lanes, ghost notes.
4. **Song tools** (`feat/rksketch-tools`): tempo and time-signature changes and ramps, key, markers, count-in, the bar tools, the note tools.
5. **Playing in** (`feat/rksketch-record`): MIDI input, step entry, real-time record with count-in, the computer keyboard; templates, autosave and recovery, export options, the icon.

Before merging: the manual (`docs/manuals/RKSketch.md`), factory templates, the website entry, the README last.

## Factory templates

| Template | Set up |
| --- | --- |
| **Empty** | one melodic track, Acoustic Grand Piano, 8 bars, 120 BPM, 4/4 |
| **Band** | Drums (ch 10), Bass (ch 2, Fingered Bass), Keys (ch 3, Electric Piano 1), 16 bars |
| **Piano (two hands)** | Right hand and Left hand on channel 1, Acoustic Grand Piano |
| **Drum groove** | one drum track, 4 bars, looped, a basic rock beat to start from |
| **Click track** | one drum track with a quarter-note click, a 1-bar count-in, 64 bars |
| **Strings** | Violins, Violas, Cellos, Basses (ch 1 to 4, GM string programs), 8 bars, 80 BPM |

## Tests

- **Files:** a song round-trips through Save and Open with every event identical (ticks, pitches, velocities, controllers, maps, settings); a file from another program keeps its unknown events; type 0 splits by channel and merges back; files over 128 bars are cut at the bar line with note-offs placed.
- **Tempo map:** ticks ↔ seconds and bars.beats.ticks across tempo jumps, ramps and time-signature changes, against hand-computed values; the 128-bar limit with any signature.
- **Bar tools:** insert, delete, duplicate, repeat move notes, controllers and map events correctly, including notes that cross the range's edges (cut or kept whole, a setting); undo restores the exact song.
- **Note tools:** quantize with strength and swing, transpose in a key, legato, velocity ramps.
- **Playback:** events land on the right sample at several rates and buffer sizes; a snapshot swap during playback never drops or doubles a note; Stop leaves no hanging notes (every note-on has its note-off).
- **Compatibility:** files open in Logic, Cubase and RKConduct (checked by hand once per phase that changes export).

## Open questions

- **The name:** RKSketch is the proposal; RKCompose, RKWrite or RKDraft would also fit the suite.
- **Notes that cross a bar range's edge** in Delete and Clear: cut at the edge, or kept whole? (Proposed: cut, as DAWs do.)
- **A plugin version** later, as a MIDI generator inside Gig Performer, if a song file should play from the rig without RKConduct.
- **A chord track** (chord symbols per bar that generate a bass or pad part) is left for after v1.
