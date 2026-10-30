# RKPlay: design

**Music player for intermissions and receptions: playlists streamed from disk, smooth crossfades, even loudness, ducking for announcements** · Utility family · standalone app first, also AU and VST3 · no latency

RK + a verb, as every RKFX plugin: to *play*. This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKPlay plays music from audio files for long stretches with nobody watching it: the band's intermission, dinner and dancing at a wedding reception, background music before a show. It is mainly a **standalone app** on a laptop, and also a plugin in Gig Performer next to the band's rig (and RKMix).

- **Playlists** of files or whole folders, several per show (Intermission, Dinner, First dance, Party), switched with a crossfade.
- **Hands-off playback:** auto-advance, shuffle without repeats, repeat, stop at the end, or play until a set time and fade out.
- **Smooth transitions:** silence at the start and end of each file is found and skipped, and songs **crossfade** with an equal-power curve.
- **Even loudness:** every file is measured once (EBU R128) and played at the same loudness, by gain alone (the user's archive standard: −18 LUFS, true peak under −0.5 dBTP, no limiting).
- **Playlists from `.m3u8` files** (relative paths), the user's archive format, and shows that survive moving to another Mac.
- **Ducking:** the music dips under an announcement mic (automatic, from an input) or at the press of a button.
- **Fade to stop / fade and pause** with one button; **Play now** and **Play next** for requests and special moments.
- **Live safety:** every file checked when a show loads, the next song always ready in memory, never a click, never waiting for the disk.
- **Pre-listen** (cue) on a second output, **8 one-shot pads** (fanfares, the couple's entrance), and a **history log**.

RKPlay with nothing loaded outputs silence. It never processes its input except to listen for ducking.

## Words used here

- **Show:** everything RKPlay holds: the playlists, the pads and every setting. A show is RKPlay's preset (and its Gig Performer state).
- **Playlist:** an ordered list of songs, or a linked folder.
- **Song:** one audio file in a playlist, with its analysis (loudness, music start and end, overview).
- **Queue:** songs chosen to play next, ahead of the playlist (requests).

## Playback

### Transport

| Control | What it does |
| --- | --- |
| **Play** | Starts the current song (from where it paused, or its music start). |
| **Pause** | Fades out over the **Pause fade** (default 1 s) and holds the position; Play fades back in. |
| **Next** | Crossfades into the next song now. |
| **Previous** | Crossfades into the song before (or back to this one's start if more than 5 s in). |
| **Fade out** | Fades out over the **Fade-out time** (default 5 s) and stops; Play starts the next song. |
| **Stop** | A short fade (100 ms) and stop; the position goes back to the song's start. |
| **Seek** | Click the overview to jump; the jump is a 50 ms crossfade, never a click. |

Every start, stop and jump fades. **No sound ever starts or stops without a fade.**

### Playlist modes (per playlist)

| Setting | Values | Default |
| --- | --- | --- |
| **Order** | In order, Shuffle | In order |
| **At the end** | Stop, Repeat, Go to playlist *n* | Repeat |
| **After each song** | Continue, Stop (wait for Play) | Continue |
| **Crossfade** | 0 to 15 s (0 = back to back, no gap) | 6 s |

- **Shuffle** is a "bag": every song plays once before any plays again, and the last song of one round never starts the next.
- **Stop after each song** suits a ceremony or a show where each piece is started by hand.

### Play until a time

**End at** (off, or a clock time, e.g. 21:30): RKPlay fades the music out at that time, either **exactly** or **at the end of the song** playing then (a setting). A countdown shows in the header. For "music until the band comes back".

### The queue

Right-click any song (in any playlist) or drag it to the queue:

- **Play next:** goes to the queue; after the current song, the queue plays before the playlist continues.
- **Play now:** crossfades into it at once (a special moment: the first dance, the cake). After it, the playlist continues where it was.

### Switching playlists

Choosing another playlist while music plays offers **Crossfade now** (default) or **After this song**. The old playlist remembers its position (and its shuffle bag), so going back continues it.

### As built (phase 3, feat/rkplay-playlists)

- **Up to 8 playlists** as tabs (click to show, double-click to rename, right-click: play now, play after this song, link to a folder or an .m3u8, rescan, unlink, export, delete; + adds one). The playing list's tab has a play mark and an accent line.
- **Each list** has its order, At the end (stop, repeat, or "Then:" another list), Stop after each, its crossfade and its gain. A list remembers its place (its cursor, and whether that song was played) and its shuffle bag while another plays; going back continues after the song it was on.
- **Shuffle**: a bag per round; the next round's order is made once (when its first song is chosen) and never starts with the song just played.
- **Three ways to play a song**: double-click, or **Go on from here**, moves the list there; **Play now** is a one-off request, after which the list goes on where it was; **Play next** adds it to the queue. The queue (right) plays before the list goes on; it isn't saved with the show.
- **End at** (right): a time and Fade out exactly then / After the song playing then; it acts once, then turns itself off. A countdown shows in the transport row.
- **The Music folder** (Settings): songs inside it are stored relative to it too. Choosing a new Music folder after the old one moved (another Mac, another user name) finds missing songs by their path inside it; **Relink missing songs** finds others by file name (several of one name: by size).
- **Linked lists** are read again when the show loads or on Rescan, keep each song's gain by its path, and take no songs by hand. Dropping an .m3u8 on a list adds its songs.
- **Settings** (a panel over the lower half): Fade out, Smart crossfade, Loudness, the Music folder and Relink.
- **Host parameter** `playlist` (1-8) is appended after `level` (the table above has it before; the IDs are what they are): another value crossfades into that list (stopped: makes it the one Play starts).

## Crossfades and smart transitions

- **Music start and end:** the analysis finds where the music really starts (the first moment above -50 dBFS, after any silence) and ends (the last moment above -50 dBFS). Playback starts at the music start; the crossfade is timed to the music end, so there is no gap even when a file ends with 10 s of silence.
- **Fade-out ends:** many songs fade out slowly. The **fade-out point** is where the level falls 20 dB below the song's loudness for good. With **Smart crossfade** on (default), the next song starts from the fade-out point, so a long fade doesn't leave the room quiet.
- **The curve** is equal-power (sine/cosine), so the loudness stays even through the crossfade. A crossfade never runs longer than half of either song.
- **Per-song trim** (optional, in the song's info): a start and end point set by hand overrides the detected ones.

## Loudness

- **Analysis:** each song's integrated loudness (EBU R128 / ITU-R BS.1770: K-weighting, gating), its peak (sample peak, 4x oversampled for true peak), music start and end, fade-out point, and a waveform overview (1,000 points), measured once on a background thread when the song is added. Measured as it plays: two channels (a mono file is on both sides). Checked against the archive's own measurements (`Tools/measure.py`): every archive song reads -18.0 ±0.05 LUFS, Green Flower Street -18.25 LUFS at -0.35 dBTP (the archive: -18.3, -0.3).
- **Cache:** results are kept in `Application Support/RKFX/RKPlay/analysis.json`, keyed by file path, size and modification date, so a show with 500 songs opens instantly and a changed file is measured again.
- **Level match** (on by default): each song plays at the **Target** loudness (-23 to -14 LUFS, default **-18 LUFS**, the user's archive standard in `~/Music/OFFLINE MUSIC ARCHIVE/LOUDNESS-TARGET.txt`). **Pure gain:** a song's gain never lifts its true peak above the **Peak ceiling** (default **-0.5 dBTP**, as the archive); a song too dynamic to reach the target plays as loud as that allows, never limited. Files already normalised to the archive standard play at 0 dB gain (within ±0.1 dB they are treated as exactly 0 dB, bit-exact; the archive measures -17.95 to -18.05 LUFS in RKPlay).
- **Lift quiet songs** (Off, up to 3 dB, up to 6 dB; default **up to 3 dB**): a song too dynamic to reach the target by gain alone is lifted further and its peaks limited, by at most that much. The limiter runs on the reading thread, seconds ahead of playback, so it looks ahead generously and moves its gain slowly (a mastering-style limiter: lookahead about 5 ms, release program-dependent, true-peak ceiling at the Peak ceiling) and adds no latency. The lift each song needs follows from its measured loudness and true peak, so playback guesses nothing (as built: not simulated; limiting a few dB of peaks moves integrated loudness by well under 0.1 dB). A song needing more than the cap is lifted by the cap and stays a little short. Songs that reach the target by gain alone are never limited (bit-exact). The song list marks limited songs with their limiting in dB. On the user's archive at -18 LUFS this touches one song (Green Flower Street, 0.3 dB); it matters for outside files (wedding requests, live recordings).
- **Make up the level at the desk or the amp, never by boosting songs** (as the archive notes say): RKPlay's output sits about 4 dB below commercial masters on purpose. The master **Level** (up to +6 dB) is a plain gain after everything.
- **Per-playlist gain** (±12 dB, default 0): a quieter Dinner list is the same target played 4 dB down, not a different target (a different target would make the dynamic songs uneven again).
- **Per-song gain** (±12 dB) adjusts on top: a karaoke or instrumental backing track a singer performs over is often best 2-3 dB lower.
- **Until analysed** (a song added a moment before it plays), it plays at 0 dB gain, and the window marks it.
- **Safety limiter** on the output (on by default, ceiling -0.3 dBFS): the zero-latency limiter RKMix's buses use (instant attack, release 100 ms, then a clip at the ceiling). A level-matched song alone never reaches it (its peaks stay under -0.5 dBTP); it is only for sums the analysis can't foresee: the middle of a crossfade between two loud songs, a pad over the music, the master Level pushed up. It can be switched off.

## Ducking

The music dips while someone speaks.

| Setting | Range | Default |
| --- | --- | --- |
| **Duck** | Off, Auto (from the input), Manual (button or host parameter) | Off |
| **Depth** | 0 to -30 dB | -12 dB |
| **Threshold** (Auto) | -60 to 0 dBFS | -35 dBFS |
| **Attack** | 50 to 1000 ms | 200 ms |
| **Hold** | 0 to 5 s | 1 s |
| **Release** | 0.2 to 5 s | 1.5 s |

- **Auto:** RKPlay's audio input is the **duck key**: in the standalone app, any input channel (the announcement mic split, or a send); in Gig Performer, RKPlay's input. The key is only listened to (a 100 Hz high-pass and an RMS detector), never passed to the output. In Gig Performer the mic still goes to the PA through its own path (RKMix).
- **Manual:** a **Duck** button (latching) and the host parameter, for a footswitch or a widget.
- The gain moves smoothly in dB, never pumping on single words (the Hold covers pauses between sentences).
- **Pads can duck the music too** (a setting per pad), so a fanfare plays over a quieter song.

## Pre-listen

- A **Cue** output (a second stereo output: headphones on the audio interface) plays any song from its start or from a point clicked on its overview, while the main output carries on. For checking a request before playing it.
- In the standalone app, the cue output is any output pair of the device (default: off); in a plugin, RKPlay's second output bus.
- Pre-listen never touches the main output, has its own level, and stops when the song is played for real.

## Pads

**8 one-shot pads**, each holding one file (a fanfare, applause, the couple's entrance music, a jingle):

- Click or trigger to play, again to stop (with a short fade); **Loop** per pad.
- Per pad: name, colour, level, **Duck the music** (on: the music ducks by the Depth while the pad plays), **Stop the music** (on: fade out the music when it starts).
- Pads play over the music and through the safety limiter. Each pad's file is loaded fully into memory (limit 60 s per pad; longer files stream like songs).
- Pads are part of the show.

## Live safety

- **Checked at load:** when a show loads, every file is checked (exists, readable, a supported format). Problem songs are marked in red, listed in a **Show check** panel, and skipped when their turn comes (never stopping the music).
- **Moved files:** **Relink** asks for a folder and finds missing files there by name (and size), for a show prepared on another computer or a music folder that moved.
- **Always ready:** the first 15 s of the **current song, the next song** (after shuffle and queue) and **every queued song** are loaded in memory before they are needed, so Play, Next and Play now start at once.
- **Streaming:** a background reading thread fills each playing song's buffer about 10 s ahead. The audio thread only copies from memory; it never waits for the disk, allocates or locks. If the disk can't keep up (a sleeping USB drive), the song fades out early rather than stuttering, the next song starts, and the window shows a warning.
- **Sample-rate conversion** (a 44.1 kHz file on a 48 kHz device) happens on the reading thread with a windowed-sinc interpolator, not on the audio thread.
- **Standalone extras for a long night:** while playing, the Mac is kept awake (an IOKit power assertion: no sleep, no App Nap); the show and the position are saved every 30 s, and after a crash or a power cut the app reopens the show at the song that was playing, stopped, with a note.
- **Panic** (right-click Stop): stops everything at once with a 20 ms fade, pads and pre-listen included.

## Files and formats

- **WAV, AIFF, FLAC, Ogg Vorbis** (JUCE's readers), **MP3, M4A/AAC, ALAC** (macOS Core Audio). Mono files play on both sides; multichannel files play their first two channels. DRM-protected files (old iTunes purchases) are refused with a clear message.
- **Adding songs:** drag files or folders from the Finder onto a playlist, or **Add files** / **Add folder**. Order by drag.
- **Linked folders:** a playlist can be a **folder** (with or without subfolders): it lists the folder's audio files and rescans when the show loads or on **Rescan**, so preparing for a gig is just copying files into a folder.
- **Linked `.m3u8` playlists:** a playlist can be an `.m3u` / `.m3u8` file (UTF-8, relative paths resolved from the file's own folder, `#EXTINF` titles read), the format of the user's archive (`~/Music/OFFLINE MUSIC ARCHIVE/Playlists/`). It is read again when the show loads or on **Rescan**, so set lists are edited in one place. Any playlist can also be **exported** as an `.m3u8` with paths relative to where it is saved.
- **Song names and tags:** the file's TITLE and ARTIST tags (FLAC Vorbis comments, ID3, MP4 atoms, read by RKPlay itself where JUCE doesn't expose them), otherwise the file name (`Artist - Title`, the archive's convention, is split at " - "). **BPM** and **KEY** tags, when present, are optional columns (handy for building a dance set). A song can be renamed in the show.

### Paths that survive a move

Gig Performer's file player keeps absolute paths and its markers inside opaque plugin state, with no relink: a restored backup on another Mac (or another user name) loses everything. RKPlay avoids that:

- **Music folder** (a show setting, e.g. `~/Music/OFFLINE MUSIC ARCHIVE/Library`): songs inside it are stored **relative** to it, so the show works wherever the folder is, on any Mac, under any user name, from an external drive. Songs outside it are stored absolute, and also by name.
- **Per-song data lives in the show, not in the file and not hidden:** start and end trims, gain, name. The show is readable JSON, so it can be fixed by hand if ever needed.
- **Relink** matches missing songs by file name, then checks size and length, and never forgets a song's data when it moves.

## The window

About **1100 × 740 px**, scaling as usual, readable from a few steps away:

- **Header:** the show (preset) name, menu, Save, Save as; the clock and the **End at** countdown; the Duck button and the ducking light.
- **Now playing (top, large):** title and artist in large type, elapsed and **remaining** time in large type, the overview with the playhead (click to seek), and **Next:** the next song's name. Big transport buttons: Previous, Play/Pause, Next, Fade out, Stop.
- **Left:** the playlists as tabs, the selected playlist's songs (name, artist, duration, loudness marker, status), its mode settings at the top; drag and drop to add and reorder.
- **Right:** the **queue**, then **pre-listen** (cue level, its own play/stop) and the **history** (time, song) of tonight.
- **Bottom:** the **8 pads**, the master **Level** and the output meter with the limiter light.

Colour coding: the playing song in the accent colour, queued songs marked, analysed songs with a small loudness bar, problems in red.

## Host parameters

Fixed, in this order (IDs permanent, append only):

| IDs | What |
| --- | --- |
| `play`, `pause`, `next`, `prev`, `fade`, `stop` | Transport triggers: act when switched on (a button or footswitch in Gig Performer), then reset |
| `playlist` | 1 to 8: switches playlist (crossfade now) |
| `duck` | Manual duck on/off |
| `pad1` … `pad8` | Pad triggers |
| `level` | Master level, -inf to +6 dB |

23 in all, plus the base's Bypass. Everything else (playlists, songs, settings) is in the show state. No Dry/Wet; **Bypass = silence** (RKMix's `bypassMutes`), faded.

## Standalone app

- **Audio:** any output device; main output pair and cue output pair chosen from its channels; the duck key from any input channel.
- **MIDI learn** for every host parameter (the pedal host's shared `MidiMap`, its own file under `Application Support/RKFX/RKPlay/`), for a MIDI footswitch or a controller's pads.
- **Program changes 0-7** switch playlists 1-8.
- **Keyboard:** Space = Play/Pause, → = Next, F = Fade out, D = Duck (held), 1-8 = pads. Shown in tooltips.
- **History:** each night's log is also written to `Application Support/RKFX/RKPlay/History/<date>.csv` (time, playlist, title, artist, file, how long it played), for the client or rights reporting.

## Decisions (2026-10-30)

- **Lift quiet songs** up to 3 dB by default (lookahead limiting on the reading thread), Off and up to 6 dB as choices.
- **Loudness:** -18 LUFS target, -0.5 dBTP peak ceiling, pure gain otherwise, as the user's offline music archive (`~/Music/OFFLINE MUSIC ARCHIVE`); every factory show uses it, quieter lists use playlist gain.
- **Accent:** sage `#6FA58A`.
- **Zones** (two rooms, two playlists) are not in v1; two RKPlay instances do it.
- **The input is only the duck key**, never passed to the output.
- **Mainly a standalone app**, also AU and VST3.

## Identity

- **AU code** `Ply1`, bundle ID `com.rkmusic.rkplay`, a music effect (`aumf`) with MIDI in: a stereo input (the duck key, never passed through) and two stereo outputs (main, cue). AU, VST3, Standalone.
- **Utility family**, its own accent (proposal: **sage `#6FA58A`**), and an RKMusic app icon ("PLAY") from `tools/icons`, as RKBoard and RKMix have.
- **Not an RKBoard pedal or an RKMix insert:** it is a source, not an effect.
- **No latency.** CPU is tiny (a crossfade is two streams; analysis runs on a low-priority background thread and pauses while the audio device is loaded heavily).

## Architecture

- **Reading:** a `juce::TimeSliceThread` "disk" thread runs the streams: for each active song a `Stream` holding the `AudioFormatReader`, the resampler, and a lock-free ring buffer (about 10 s at the device rate); a separate low-priority thread runs the analysis.
- **Preload:** the first 15 s of each "ready" song is decoded into memory (a `Preload` block), so a stream starts reading from memory while the disk catches up behind it.
- **Audio thread:** a small player graph: up to 3 song voices (current, incoming in a crossfade, and a seek's short crossfade), the cue voice, 8 pad voices; per-voice gain ramps (song gain from the analysis, fades, crossfade curve), the ducker, the master level, the limiter. Commands from the message thread (play, next, seek...) reach it through a lock-free FIFO; state goes back (positions, ends, underruns) the same way.
- **Message thread** owns the show: playlists, queue, shuffle bags, the "what plays next" decision, which it makes early (at least 20 s before a crossfade) so the next song is preloaded in time.
- **Shared code:** the zero-latency limiter (from RKMix's `MixBusDsp`) moves into `core/dsp` (or is shared from RKMix's folder); MIDI learn from `host/MidiMap`. The loudness meter (BS.1770) is new in `core/dsp` and could later serve a meter plugin.

## Build phases

One stacked branch per phase; the user tries each in the standalone app.

1. **Player core** (`feat/rkplay`): one playlist, files added by drag and drop, streaming with preload, transport with fades, crossfades with a fixed length, safety checks, the main window's now-playing area; state and presets.
2. **Analysis** (`feat/rkplay-analysis`): loudness, peaks, music start and end, fade-out point, the overview, the cache; level matching, smart crossfades, the safety limiter.
3. **Playlists** (`feat/rkplay-playlists`): several playlists, modes (order, shuffle, end, after each song), linked folders, the queue (Play next, Play now), switching, End at, relink.
4. **Ducking, pre-listen, pads** (`feat/rkplay-extras`): ducking (auto and manual), the cue output and pre-listen, the 8 pads.
5. **Standalone** (`feat/rkplay-standalone`): keep awake, autosave and crash recovery, MIDI learn, program changes, keyboard shortcuts, history files, the app icon.

Before merging: the manual (`docs/manuals/RKPlay.md`), factory presets, the website entry, the README last.

## Factory presets

A factory show can't hold music files, so the factory presets are **empty shows set up for an occasion**: named playlists with their modes, crossfades, ducking and pad settings. Load one, drop music into its playlists, Save as.

| Factory show | Set up |
| --- | --- |
| **Intermission** | One playlist "Intermission", in order, repeat, 6 s crossfade, End at off; Fade out 8 s |
| **Wedding Reception** | Playlists "Arrival", "Dinner" (shuffle, 8 s crossfade, playlist gain -4 dB), "First Dance" (stop after each song), "Party" (shuffle, 3 s crossfade); Duck Auto; pads "Entrance", "Fanfare" with Duck the music |
| **Background** | One linked-folder playlist, shuffle, repeat, 10 s crossfade, playlist gain -4 dB |
| **Ceremony** | One playlist, in order, stop after each song, no crossfade, 3 s fade-out |

## Tests

- **Streaming:** a song plays bit-exact against its file at the same sample rate (gain 0 dB, no fades in the middle); at another rate, a sine stays clean (the resampler's noise under -90 dB); a slow reader (an artificial delay) never blocks the audio thread and leads to an early fade, not a glitch.
- **Fades and crossfades:** every start, stop, pause, seek and crossfade is click-free (no sample step above a threshold); an equal-power crossfade of two uncorrelated noises keeps the power within 0.5 dB.
- **Analysis:** EBU R128 reference signals (1 kHz sine at -23 dBFS reads -23.0 LUFS ±0.1; the EBU Tech 3341 test cases); music start and end and the fade-out point on generated files with silence and fades.
- **Playlists:** shuffle plays every song once per round with no repeat at the boundary; the queue's order; Play now returns to the playlist; End at fades at the time (with an injected clock); missing files are skipped.
- **Ducking:** depth, attack, hold and release on a keyed tone burst.
- **State:** a show round-trips (playlists, modes, pads, per-song gain and trims, queue not saved); the analysis cache is used when a file is unchanged and redone when it changed.
- **Validation:** auval (`aumf Ply1 RKMu`), pluginval at strictness 10.

## Open questions

- **Window size** once phase 1 is on screen.
- **Several outputs for zones** (dinner room and terrace with different playlists) are left out of v1: one RKPlay per zone works meanwhile.
- **Streaming services** (Spotify, Apple Music) can't be played by any plugin (DRM); only files.
