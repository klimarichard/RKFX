# RKMix Recorder and Virtual Soundcheck: design

**Record every input to its own WAV file, and play a recording back into the channels in place of the band** · zero latency, nothing on the audio thread but copies into FIFOs

Two halves that use the same files:

- **Recorder:** each input (as it arrives, before the trim) to its own file, plus the main mix if you like, for the whole gig. Markers at every scene recall.
- **Virtual soundcheck:** load a recording and switch channels from **Live** to **Playback**. RKMix plays the files into those channels exactly where the live inputs would arrive, so trims, EQs, compressors, inserts, sends and scenes can be set without the band: last night's show, or the afternoon's line check, mixed again.

This is a design, not a manual: it says what the feature does and how, so it can be built and reviewed against it.

## Suite rules

- **Zero latency.** Recording copies the inputs; playback replaces them sample for sample. No path is delayed, and the latency plan doesn't change.
- **No disk, no allocation, no locks on the audio thread.** Recording pushes samples into preallocated lock-free FIFOs that a background thread writes to disk. Playback reads from rings a background thread fills ahead (as RKPlay's `PlayStream` does).
- **Live-safe:**
  - A disk that can't keep up never stops the audio: the recorder drops what it can't write, counts the dropouts and shows them.
  - Playback that runs dry plays silence for those channels, never garbage.
  - **A live show can't end up on Playback by mistake:** the input source isn't saved with the mix or in scenes, every session starts Live, and Lock blocks switching it.
- **Free while off.** Not recording and not playing back costs one atomic read per block.

## Recording

### What is recorded

- **The inputs** you choose (saved with the mix; a new mix records all 32), **before the trim and polarity**, as they come in. The trim, and everything after it, then work the same on playback as live.
- One file per channel: a **mono file** for a single input, a **stereo file** for a linked pair (`05-06 Keys.wav`, left = the odd input, right = the even one). Both sides of a pair are in one file, so they can never drift apart.
- **Pairs as two mono files** (a setting on the Recorder page, off by default, remembered per machine): for a DAW or a mixing workflow that prefers mono tracks, a linked pair is recorded as `05 Keys L.wav` and `06 Keys R.wav` instead.
- **Linking and unlinking are disabled while recording** (see Links while recording), so the files always match the channels as they were when Record was pressed.
- **Main L/R** (optional, default on): a stereo file of the main output after its limiter, for listening back.
- Optionally any **Aux bus** output (an in-ear mix, for the musician to hear what they had).

### Files

- WAV, **24-bit** (default) or **32-bit float**, at the session's sample rate.
- A folder per recording: `~/Music/RKMusic Plugins/RKFX/Recordings/<date> <time> <mix name>/` (the root folder can be changed in the Recorder page and is remembered per machine), containing `01 Lead Vox.wav` (input number and channel name), `05-06 Keys.wav` (a pair), `Main.wav`, and `session.json`:
  - the sample rate and start time; for each file, the input each of its channels came from (`"05-06 Keys.wav": [5, 6]`), so mono and stereo files, and pairs split into two mono files, all load the same way;
  - the names and colours when recording started;
  - the **markers** (sample position, label: "Scene 3", "Marker 2", ...);
  - the dropout count and where they happened.
- **Size:** a mono 24-bit track at 48 kHz is about 518 MB an hour, so a 3-hour gig with 24 inputs is about 37 GB. The page shows the free space and the time left at the current track count, refuses to start with less than 10 minutes left, and stops cleanly (files closed) at 1 minute left.
- **Long files:** a mono file only reaches WAV's 4 GB limit after about 8 hours at 48 kHz / 24-bit, but a stereo float main at 96 kHz gets there sooner. Use JUCE's RF64 support in `WavAudioFormat` (confirm the writer switches to RF64 automatically past 4 GB in JUCE 8.0.13; if not, start a new file at 2 GB and note the join in `session.json`).
- **A crash or a power cut leaves readable files:** each writer's header is brought up to date every 10 s (flush), so at most the last 10 s are lost.

### On the audio thread

- `MixRecorder::push (file, channels, numSamples)` for each recorded file (one or two input channels at once), called in `processBlock` at the point the inputs arrive, before the channel strips run. The main (and Aux) pushes happen after their limiters.
- Use `juce::AudioFormatWriter::ThreadedWriter`, one per file, mono or stereo (its `write` takes several channels at once, so a pair needs nothing special). Each has its own lock-free FIFO and is written by one shared `juce::TimeSliceThread`. FIFO size: **4 s per channel** (at 48 kHz float, 768 KB per channel, about 26 MB for 32 inputs and the main), allocated when recording is armed, never in the callback.
- **The file layout is fixed when Record is pressed:** which inputs are recorded, and which are pairs (stereo files). The audio thread reads that layout, never the live link settings.
- `ThreadedWriter::write` returns false when its FIFO is full. The recorder then counts a dropout (an atomic), and the page and `session.json` say where.
- **Start and stop are sample-exact and shared:** the audio thread starts every track on the same sample (the first block after Record is pressed) and stops them all on the same block, so the files stay aligned to the sample.

### Controls

- A **Recorder** page (with Talk / Osc, Scenes...): Record / Stop, the elapsed time, the free space and time left, the dropout count, the track list (tick inputs, Main, Aux buses), the format, the folder.
- **Markers:** every scene recall writes one ("Scene 3 Verse" once scenes have names); a **Marker** button (and a host parameter) adds "Marker n".
- Host parameters **Record** (on = recording; a Gig Performer button) and **Marker** (acts once).
- **REC** in the master section, blinking red while recording, so it's visible from every page.
- A change of sample rate stops the recording cleanly.

### Links while recording

Linking or unlinking a pair while recording would leave a file that no longer matches its channel, so it's blocked:

- **Link** on the Channel page is dimmed while recording, with the tooltip "Stop recording to link or unlink". The console remote's Link control is dimmed the same way, and a link message from any remote is refused.
- **Loading a mix** from the preset menu is dimmed while recording ("Stop recording to load a mix"): a mix can bring other links with it. Strip presets and scenes are fine, since they never change links.
- **A session restored by the host** while recording (Gig Performer reloading the gig, `setStateInformation`) can't be refused. The take then ends and a new one starts in a new folder on the same block (no samples lost between them), with the new mix's links. Both takes' `session.json` note the split.
- **The console remote** (iPad): Record / Stop (Stop with a confirmation), elapsed time, dropouts, Marker.

## Virtual soundcheck (playback)

### Loading

On the Recorder page, **Playback: Load...** picks a recording folder (it reads `session.json`; a folder of plain WAVs without one is accepted too, mapped by the number at the start of each name, else in order). It shows the length, the markers, and how each file maps to inputs (editable: drag a file to another input).

- **Each channel of a file feeds one input:** a mono file one input, a stereo file two neighbouring inputs (left to the odd one, right to the even one). A stereo file can only be dropped on an odd input, so its two sides land on a pair.
- The mix's links don't have to match the recording: a stereo file still plays into two inputs that are now unlinked (each side into its own channel), and two mono files into a pair that is now linked.
- A plain WAV without `session.json`: a stereo file takes two inputs, from the number its name starts with.

- **Sample rate:** a recording at another rate than the session's is refused in this version, with a message.
- The input names stored in the recording are offered (**Use the recording's names**) but not applied by default.

### The input source

Each input has a source, **Live** or **Playback**:

- On the Channel page's Input section, a **Live / PB** switch. On the strip, a **PB** badge in the accent colour over the channel number.
- **All to playback** / **All live** on the Recorder page.
- **Playback replaces the raw input**, before the trim: everything after it runs as live, including the gate and compressor keys and RKTrigger and RKFollow inserts (whose MIDI then plays the drum samples and synths, as at the gig).
- A channel on Playback with no file mapped plays silence.
- **Never saved, never in scenes, never undone.** Every session and every loaded mix starts all Live. **Lock** (both levels) blocks the switch, and **All live** stays available while locked: the one change that is always safe.

### Transport

- **Play / Pause / Stop**, a position bar with the markers (click to locate), **previous / next marker**, and **Loop** between two markers (rehearse one song's mix over and over).
- **Marker list** (added 2026-11-08): the loaded recording's markers in a scrollable list beside the file mapping; click to locate, double-click to rename, × / Delete to delete (Undo delete for the last one), **Add marker here** at the play position. Edits are written into `session.json` at once (a plain-WAV folder gets one with only `markers`; `scan` reads markers for both kinds). Not while locked, nor for the folder being recorded into (the recorder rewrites its `session.json`). Plus **-1 min / -10 s / +10 s / +1 min** jumps.
- **Export** (added 2026-11-08, `MixExport`): the main mix between markers into a stereo WAV, offline on a low-priority thread. The processor's `exportMix()` snapshots the engine settings (built from the parameters on the message thread, not the audio thread's copy), the levels without solo/talk/osc, a second `InsertRack` built from `rack.toVar()`, and the tempo. A fresh `MixEngine` per region (inserts prepared again) runs from 2 s before the region; the main comes from `OutputTaps::main`, `getLatency()` samples late, so the file starts on the marker's sample. Inputs silent from the region's end; tail up to 3 s, cut under -70 dB, 20 ms fade. Written to a temporary file and moved into place. Export = first selected marker to the one after the last; Export each = one per marker; nothing selected = around the play position. Tested sample-equal to the live console (with RKLimit's latency on the main too); about 80x real time for 15 channels with gate/EQ/comp and 4 inserts.
- All tracks share one playback position and are read by one disk thread, so they stay sample-aligned.
- **Streaming:** generalise `PlayStream` (one song, two channels, a ring filled by a `TimeSliceClient`) into a multi-track reader: N mono rings of 3 s each, filled ahead by one `TimeSliceThread`, all advanced by the same position. A locate drops the rings, refills them on the disk thread, and starts once 0.5 s is ready (the transport shows "locating"). Every start, stop and locate fades over 5 ms.
- A ring that runs dry plays silence for that block and counts an underrun (shown).

### Recording while playing back

Allowed: the recorder records what reaches the inputs, Live or Playback. A virtual soundcheck can then be recorded too (e.g. the main, to compare two mixes). The page warns when a recorded input is on Playback.

## Tests

- Recording: a known signal on every input comes back sample-exact from the files (24-bit within rounding, float bit-exact), all files the same length and aligned to the sample; the main file matches the main output.
- Pairs: a linked pair records as one stereo file (left = odd input) and, with Pairs as two mono files on, as two mono files; `session.json` maps both. Link is refused while recording (the window, a remote message); loading a mix is refused; a host state restore splits the take on one block with no samples lost.
- Playback mapping: a stereo file into a pair that is now unlinked, two mono files into a pair that is now linked, a plain WAV folder without `session.json`.
- Markers at the right samples; `session.json` read back.
- Dropouts: a writer that can't keep up (a test hook that stalls the disk thread) counts dropouts, and the audio thread never blocks.
- Playback: files played into channels give, sample for sample, the same channel output as feeding the same signal live; locate, loop and marker jumps are sample-accurate and click-free; a dry ring plays silence.
- Safety: a loaded mix and a reloaded session start all Live; Lock blocks Live → PB but not All live; scenes and undo never touch the source.
- Benchmark: idle must not move; recording 33 tracks and playing back 32 at 48 kHz / 64.

## Build order

1. **Recorder engine:** the ThreadedWriters, the push points, the sample-exact start and stop, dropouts, files and `session.json`, crash-safe headers. Tests.
2. **Recorder page,** the REC indicator, markers, the host parameters.
3. **Playback engine:** the multi-track stream (from `PlayStream`), the input source switch, the transport. Tests.
4. **Playback UI:** loading, the mapping, the PB switch and badge, the transport.
5. **Console remote:** recorder controls, then playback transport.
6. **Manual** (a Recorder section in `docs/manuals/RKMix.md`), then README.

## Not in this version

- Resampling a recording at another sample rate.
- Punch-in, or editing the files.
- Recording or playing back the insert outputs, or any tap other than the raw input.
- Timecode sync with another recorder.
