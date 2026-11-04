# RKCue: design

**MIDI file sampler: keys play MIDI files** · MIDI family (lilac) · MIDI effect: MIDI in, MIDI out · no latency

RK + a verb, as every RKFX plugin: to *cue* a phrase. This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKCue is a sampler whose samples are MIDI files. Load a MIDI file into a **slot** and give the slot a key, or a range of keys. A NoteOn on that key plays the file out of RKCue's MIDI output, into whatever instruments are wired after it in Gig Performer.

- **Drum-pad style:** one key fires one file as it was written, for example a fill, a riser, an arpeggio or an intro phrase. With 16 slots on 16 keys, that's an MPC for phrases.
- **Melodic-sampler style:** a slot covers a key range with a **root key**. With **Track Pitch** on, the other keys play the file transposed. Hold C3 and the bass riff plays in C; press F3 and it plays in F.
- Both styles can be mixed in one RKCue, and zones can overlap (two slots on one key play together, as layers).

It isn't RKConduct, where the file follows your taps beat by beat. Once triggered, an RKCue file plays at its tempo on its own, and the key only starts it, or also stops it (see **Play Mode**).

## Slots

There are **16 slots**. Each holds one Standard MIDI file (.mid). All of the file's tracks play, merged; the track list is shown for information, with each track's note count.

| Setting | Range | Default | What it does |
| --- | --- | --- | --- |
| **File** | Load / Clear, or drop a .mid file on the pad | empty | The file. It is stored inside RKCue's state, as in RKConduct, so a saved gig always has it. |
| **Low Key / High Key** | C-2 to G8 | slot *n*: C1 + *n* − 1, both | The zone. A single key when Low = High. |
| **Root Key** | C-2 to G8 | = Low Key | The key that plays the file untransposed. |
| **Track Pitch** | Off, on | Off | On: a key *k* in the zone transposes the file by *k* − Root semitones. Off: every key in the zone plays the file as written. |
| **Transpose** | −24 to +24 semitones | 0 | Added to every note, on top of Track Pitch. |
| **Keep Drums** | Off, on | On | Channel-10 notes (the GM drum channel) are never transposed, so a drum track inside a phrase doesn't turn into other drums. |
| **Play Mode** | One-shot, Gate, Toggle | One-shot | See below. |
| **Loop** | Off, on | Off | At the end, starts again from the beginning without a pause. |
| **Poly** | Off, on | Off | Off: a new trigger restarts the slot (the old run's notes end first). On: each trigger starts a new copy, up to 8 per slot. Needed to play a transposed riff as a chord. |
| **Velocity** | 0 to 100 % | 100 % | How much the trigger's velocity scales the file's velocities. 100 %: file velocity × trigger / 127. 0 %: the file's velocities as written. |
| **Channel** | As recorded, 1 to 16 | As recorded | Where the slot's notes go out. Send each slot to a different instrument in Gig Performer with a channel filter there. |
| **Name** | text | the file's name | Shown on the pad. |

A note transposed beyond 0 to 127 is dropped.

### Play Mode

| Mode | Key down | Key up | Same key again while playing |
| --- | --- | --- | --- |
| **One-shot** | starts | nothing | restarts (or, with Poly, adds a copy) |
| **Gate** | starts | stops: the copy started by that key ends its notes | (key is up between presses) |
| **Toggle** | starts | nothing | stops |

With **Loop** off a run ends at the end of the file in every mode. With Loop on, a One-shot slot loops until **Stop** (the slot's or **Stop All**).

When a run is stopped, its sounding notes get their note-offs at once. Notes in the file that haven't been reached aren't played. A sounding note's note-off always goes to the channel and transposed note its note-on went to, even if settings changed in between.

### Trigger keys and the rest of the keyboard

A NoteOn or NoteOff on a key in some slot's zone (a slot that has a file) is **taken in**. Every other key, and all other MIDI (controllers, pitch bend, aftertouch), passes through to the output with **Thru** on (default), so RKCue can sit between the keyboard and the instruments. Thru off: only RKCue's own notes go out.

**Trigger Channel**: Any (default) or 1 to 16. Only notes on that channel trigger slots; others pass through.

## Tempo

**Tempo Source** (one setting for the whole plugin):

| Source | A file plays at |
| --- | --- |
| **File** (default) | its own tempo map, tempo changes included |
| **Host** | the host's tempo (Gig Performer's global tempo). A change of tempo is followed at once, by every running copy. If the host gives no tempo, the file's own. |
| **Fixed** | the **Tempo** setting, 20 to 300 BPM (default 120) |

With Host and Fixed, the file's events are placed in beats (quarter notes, from the file's ticks) and the file's tempo map is ignored.

**Start** is immediate, on the trigger's own sample: RKCue adds no latency and doesn't wait for a beat. A quantized start (next beat / bar of the host) is an open question, below.

## Polyphony and voices

- A **copy** (a voice) is one run of one slot: its slot, its transposition, its velocity scale, its play position and the notes it has sounding.
- **32 copies** in total. A trigger beyond that ends the oldest copy first (stealing). A slot's Poly limit is 8.
- Every copy's note-on is remembered with the channel and note it went out on (a 16 × 128 counter per copy), so each note-off finds its note-on. If two copies sound the same note on the same channel, the note-off of the first doesn't cut the second: RKCue keeps a per-channel per-note count across copies and sends the note-off only when the count gets back to 0. (The note-on of the second copy is sent again as usual.)

## Host parameters

Every slot setting is a host parameter (`s1_low` … `s16_channel`, see As built), so factory presets can hold layouts and a widget can change a slot. The globals and buttons:

| Parameter | ID | Range | Default |
| --- | --- | --- | --- |
| Tempo Source | `tempo_source` | File, Host, Fixed | File |
| Tempo | `tempo` | 20 to 300 BPM | 120 |
| Thru | `thru` | Off, on | On |
| Trigger Channel | `trig_channel` | Any, 1 to 16 | Any |
| Stop All | `stop_all` | button | |
| Play 1 … Play 16 | `play_1` … `play_16` | button: on = key down at the root key, velocity 100; off = key up | |
| Stop 1 … Stop 16 | `stop_1` … `stop_16` | button | |

So a footswitch or a Gig Performer button can fire a slot with no keyboard at all. IDs are permanent; only append.

## The window

About 900 × 560, MIDI family background.

- **Top: keyboard map.** The whole 128-key range with each slot's zone drawn as a coloured bar above the keys, its root key marked. Drag a zone's ends to change Low and High; drag the marker to change the root. Keys light as notes come in.
- **Left: 16 pads**, 4 × 4. Each shows the slot's number and name, and lights while a copy plays (with a thin progress line). Click a pad to select it; a small ▶ on the pad plays it (as Play *n*). Drop a .mid file from Finder onto a pad to load it.
- **Right: the selected slot.** File name, **Load** and **Clear**, **Learn** (the next key you press sets Low = High = Root to that key; hold Shift and press two keys to learn a range), and the settings from the table above. Below: the file's track list (name, notes) and the file's length in bars and its tempo.
- **Header:** presets and Bypass (Common Controls), Tempo Source, Tempo, Thru, Trigger Channel and **Stop All**.

## Presets

A preset is a **folder** (`config.presetFolders`, as in RKConduct): `preset.json` with the settings and every slot's settings, and the slots' MIDI files next to it (`01 <name>.mid`, …). Copied to another computer, it plays the same. A slot whose file is missing from the folder loads empty, and the header names the missing files. A preset with an empty slot clears that slot.

### Factory presets

Factory presets hold only the **layout** (zones, modes and so on). Loading one keeps the files that are loaded. They are the manual's starting points:

| Factory preset | Layout |
| --- | --- |
| **16 Pads from C1** | Slot *n* on the single key C1 + *n* − 1, One-shot (the default layout) |
| **16 Pads, Gate** | The same keys, Gate: a phrase plays while the key is held |
| **8 Pads, Toggle Loops** | Slots 1 to 8 on C1 to G1, Toggle, Loop: start and stop loops from the keys |
| **Riff on the Keyboard** | Slot 1 on C2 to C4, root C3, Track Pitch, Poly: play a riff in any key, or as chords |
| **Split: Riff + Pads** | Slot 1 as above but C3 to C5 (root C4), slots 2 to 9 single pads from C1 |

## Bypass

Bypassed, MIDI passes through unchanged. Every running copy is stopped, with its note-offs, at the moment of bypass.

## Engine

`CueEngine`, GUI-free:

- **Files** are read on the message thread into a `CueSong`: every event with its time in beats (from the file's ticks and time format) and, for **File** tempo, its time in seconds from the file's tempo map. Note-on/note-off pairs are matched at load (a note-on with no note-off ends at the end of the file). The song is swapped into its slot under a SpinLock, as in RKConduct; the old one is deleted on the message thread.
- **Processing** never allocates: 32 preallocated voices, an output `MidiBuffer` with reserved space.
- Per block: incoming MIDI is handled in order, sample by sample position, interleaved with the voices' events. A voice's position advances per sample range in beats (`bpm / 60 / sampleRate`) or seconds (File), so a trigger at sample 17 plays the file's first event at sample 17, and Host tempo changes take effect from the next block.
- **Loop:** at the end the voice ends its sounding notes, then wraps; events at beat 0 play on the same sample.
- The file reader and song structure are shared with RKConduct by moving them to **`core/midi/MidiSong.h`** (RKConduct's `makeSong` becomes a thin wrapper; its behaviour doesn't change, checked by its tests).

## CPU and memory

Event-driven only, no audio. Expected well under 0.05 % CPU with 32 copies playing dense files. Memory: the 16 songs (a few KB to a few hundred KB each) and their stored file data in the state.

## Tests

- One-shot: trigger plays every event at the right sample (File, Host and Fixed tempo, tempo changes in the file), retrigger restarts with note-offs first.
- Gate: key up ends the copy's notes on that sample; Toggle: second press stops.
- Track Pitch: transposition by key − root; Transpose added; Keep Drums leaves channel 10 alone; notes out of range dropped.
- Poly: two keys give two copies; the shared per-note count never cuts a note another copy still sounds; stealing at 32.
- Loop wraps without a gap; Stop, Stop All, Bypass, clearing or replacing a slot's file, and changing a zone while playing never leave a hanging note (every note-on gets a note-off).
- Thru and Trigger Channel: non-zone notes and other MIDI pass through; zone notes are taken in.
- State and folder presets round-trip all 16 files and settings; a missing file loads the slot empty.
- RKConduct's tests still pass after the move to `core/midi/MidiSong.h`.
- auval and pluginval (AU, VST3) as for every plugin.

## Identity

- Plugin code **Cue1**, bundle id `com.rkmusic.rkcue`, `MIDI_EFFECT`.
- Accent **lilac #B66BD0** (MIDI family; next to plum, pink, rose, orchid, magenta, berry, deep orchid and light pink).
- Not in RKBoard (a MIDI effect, as RKConduct).
- Manual `docs/manuals/RKCue.md` and the factory presets before merge.

## Decisions (2026-11-04)

1. **No quantized start:** a trigger always starts on its own sample.
2. **No per-track settings:** the slot's single Channel is enough.
3. **No choke groups.**
4. **16 slots.**

## As built (2026-11-04)

- **Slot settings are host parameters**, not extra state: 11 per slot (`s<n>_low`, `_high`, `_root`, `_pitch`, `_transpose`, `_drums`, `_mode`, `_loop`, `_poly`, `_velocity`, `_channel`), after the globals (`tempo_source`, `tempo`, `thru`, `trig_channel`, `stop_all`) and before `play_1..16`, `stop_1..16`: 213 parameters plus Bypass. That lets factory presets hold layouts with no core change, while the files (and the slots' names) are the extra state: base64 in the session, files in folder presets (`NN <name>.mid`).
- **Keyboard map:** all 128 keys, no scrolling. Zones of slots without a file aren't drawn (except the selected slot's, dimmed). Overlapping zones are spread over up to 4 lanes.
- **Learn:** click, then a key (Low = High = Root); Shift-click, then two keys (a range, Root = the first).
- **Engine:** `plugins/RKCue/CueEngine` steps every copy per sample in beats; File tempo reads the tempo map with a per-copy cursor. A copy remembers, per file (channel, note), where its note went (16 × 128 × 2 bytes), and a shared count per (out channel, note) holds back a note-off while another copy sounds the note. Incoming notes are remembered as taken in or passed through, so a key's note-off always goes where its note-on went (Thru or a zone changed while held). A passed key that becomes a trigger while held gets its note-off.
- **Core:** `core/midi/MidiSong.h` (shared reader, tempo map, header-only); RKConduct uses it. `Transport::hasBpm` says whether the host gave a tempo (Host falls back to the file's).
- **Tests:** `tests/CueTests.cpp` (engine; mutation-checked: shared note count, Keep Drums, passed note-offs) and `tests/CuePresetTests.cpp` (folder presets, session, factory layouts keep files). auval and pluginval (strictness 10, with editor tests) pass.

## Open questions (answered above)

1. **Quantized start:** with Host tempo, should a trigger optionally wait for the host's next beat or bar (Start: Immediate / 1/4 / Bar)? Only meaningful while Gig Performer's transport/tempo is the band's clock.
2. **Per-track settings** inside a slot (track on/off and channel, as RKConduct has), or is the slot's single Channel enough for v1?
3. **Choke groups:** should starting one slot stop others in the same group (for example, only one intro phrase at a time)? Toggle + Stop covers most of it.
4. **16 slots enough?** The engine has no real limit; 16 keeps the pads and the parameter list tidy.
