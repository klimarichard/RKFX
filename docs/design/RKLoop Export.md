# RKLoop Export: design

**Save a loop (or all of them) as WAV files** · no change to the audio path, nothing on the audio thread but a counter

RKLoop's loops live only in memory: the manual says they aren't saved with the session, so a good take is gone when the gig file closes. Export writes them to WAV files: one loop, every loop, or a quick save to a fixed folder from a footswitch, without a dialog, in the middle of a song. This is a design, not a manual: it says what the feature does and how, so it can be built and reviewed against it.

## Suite rules

- **Zero latency, no change to the sound.** Export reads the loop buffers; it never writes to them and never delays the audio thread.
- **No allocation, no locks, no disk on the audio thread.** The audio thread only bumps a counter when it writes into a loop (see Reading a loop safely). The copy and the file writing happen on the message thread and a background thread.
- **Live-safe.** Exporting while the loops play is normal use: the music never stutters, and a failed write (a full disk, a folder that's gone) only shows a message.

## What is exported

- **The loop exactly as stored**, `length` samples, at the session's sample rate, in the plugin's channel count (stereo, or mono in a mono slot). The buffer already has its 5 ms seam fades baked in (`LoopEngine::closeRecording` fades both ends), so the file loops cleanly in any sampler or DAW with no extra processing.
- **Forward, at normal speed.** Reverse and Half Speed are playback modes, not part of the loop, so they're ignored (noted in the manual).
- **The overdubs as they are now:** undone overdubs are left out, as you hear it.
- **Not the Loop Level,** and no normalising: the file is what was recorded.

## Formats and names

- WAV, **24-bit** (default) or **32-bit float** (a setting in the export menu, remembered per machine, not per preset).
- A **smpl chunk** with one loop from 0 to length − 1, so samplers that read it (and Logic) loop the file right away. JUCE's `WavAudioFormat` writes it from the metadata keys `NumSampleLoops`, `Loop0Start`, `Loop0End`, `Loop0Type`; confirm the key names against JUCE 8.0.13 when building.
- **File name:** `RKLoop <date> <time> Loop <n> <Part|Layer>.wav`, e.g. `RKLoop 2026-11-06 21-14 Loop 3 Layer.wav`. When a host tempo is known, ` <bpm> BPM` is added (the host's tempo at export time, rounded to 0.1). That says what tempo the loop was played at; it isn't a claim that the loop is a whole number of bars.
- **Export all:** one folder, `RKLoop <date> <time>/`, with `Loop 1 Part.wav` ... and only the loops that aren't empty.

## The commands

In RKLoop's window, an **Export** button next to the loop tiles opens a menu:

| Item | What it does |
| --- | --- |
| **Export Loop n...** | The selected loop, to a file you pick (a save dialog, starting in the last folder used). |
| **Export all loops...** | Every non-empty loop, into a new folder in a folder you pick. |
| **Quick save all** | Every non-empty loop, with no dialog, to `~/Music/RKMusic Plugins/RKFX/Loops/RKLoop <date> <time>/`. |
| **Show the Loops folder** | Opens that folder in the Finder. |
| **24-bit / 32-bit float** | The file format. |

Right-click a loop tile for **Export this loop...**.

**Quick save** is also a host parameter, **Save Loops** (a footswitch button, like Record and Stop: turning it on acts once). It's for the moment you've just played something good: one press and it's safe, no window needed. The window shows "Saved 4 loops" (or the error) for a few seconds next to the button.

An item is dimmed, with the reason in its tooltip, when there's nothing to export (every loop empty) or the loop is busy (see below).

## Reading a loop safely

The audio thread writes into a loop's buffer while **recording**, **overdubbing**, and during an **Undo/Redo swap** (`continueSwap`, a few hundred samples per audio sample until done). In **playing** and **stopped** with no swap running, the buffer isn't written.

- A loop can be exported when it is playing or stopped with no swap running (`swapCursor < 0`). While it's recording, overdubbing or swapping, its Export item is dimmed ("finish the overdub first"). Quick save waits for it: it retries every 100 ms for up to 5 s, then saves the others and says which one it skipped.
- To catch a write that starts during the copy, each loop gets an `std::atomic<uint32_t> writes`, bumped by the audio thread whenever it starts writing that loop (beginning a recording, an overdub or a swap). One relaxed increment, nothing else.
- The message thread reads `writes`, copies `length` samples into its own `juce::AudioBuffer` (allocated there; a 30 s stereo loop at 192 kHz is 46 MB, a normal one far less), then reads `writes` again. If it changed or the state isn't playing or stopped any more, the copy is thrown away and retried (as above). This is the same pattern as Monitor EQ's seqlock.
- The copy is a memcpy per channel (milliseconds even for the longest loop) and happens on the message thread. The **file writing** goes to a background thread (a `juce::ThreadPool` job owned by the processor), so the window never freezes. The processor waits for that job to finish in its destructor.

## In RKBoard and RKMix

RKLoop as a pedal or insert has the same window, so Export works there unchanged. **Save Loops** is a parameter like the footswitches, so it can be put on an RKBoard Assign slot or a footswitch through MIDI learn in the standalone app. The file dialog uses `juce::FileChooser::launchAsync` (never the blocking form, which isn't allowed in plugins on macOS).

## Tests

- A loop recorded from a known signal exports sample-exact (read the file back), with the right length, channel count and format; 24-bit within its rounding, float bit-exact.
- The smpl chunk is read back with JUCE's reader (start 0, end length − 1).
- Undo: an undone overdub isn't in the file; after Redo it is.
- A busy loop: overdubbing, the export is refused; a `writes` change during the copy is retried.
- Export all skips empty loops; names and folder as above; an unwritable folder gives an error and leaves the loops untouched.
- The Save Loops parameter acts once per press.
- Benchmark: RKLoop before and after, at 48 kHz / 64; the figure must not move.

## Build order

1. Engine: the `writes` counters and a `copyLoop (i, buffer)` that returns false when busy. Tests.
2. Writer: WAV with the smpl chunk, names, the Loops folder, the background job. Tests.
3. Window: the Export menu, the tile's right-click, the status line; the Save Loops parameter.
4. Manual (`docs/manuals/RKLoop.md`: an Export section, and the Notes line "the loops themselves are not saved" pointing to it), then the README line.

## Not in this version

- **Importing** a WAV into a loop (the natural other half: a backing loop prepared at home). It needs resampling to the session rate and the sync length rules, so it's its own small design.
- Keeping the loops in the gig file (state size: up to 8 × 30 s of audio inside Gig Performer's gig).
- A mixdown of every playing loop into one file.
