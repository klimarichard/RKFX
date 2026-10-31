# RKPlay

**Music player** · playlists streamed from disk, crossfades, even loudness, ducking · standalone app first, also a plugin (AU, VST3) · no latency

RKPlay plays music from audio files for long stretches with nobody watching it: the band's intermission, dinner and dancing at a wedding reception, background music before a show. Every song plays at the same loudness, songs crossfade without gaps, the music dips under announcements, and it keeps going on its own until you stop it or until a set time. Use it as a standalone app on a laptop, or as a plugin in Gig Performer next to the band's rig.

For the preset menu and Bypass, see [Common Controls](Common%20Controls.md).

## The window

- **Now playing (top):** the song's title and artist in large type, elapsed and remaining time, the song's waveform with the playhead (click it to jump), and **Next:** what plays after it.
- **Transport:** **|<** (previous), **Play/Pause**, **>|** (next), **Fade out**, **Stop**, **Duck** and **Settings**.
- **Left:** the playlists as tabs, the shown playlist's settings, and its songs.
- **Right:** the **Queue**, **Pre-listen** and **End at**.
- **Bottom:** the **Level** knob and the **8 pads**.

A show is everything RKPlay holds: the playlists, the pads and every setting. A show is RKPlay's preset, and in Gig Performer it is saved with the gig.

## Playing

| Button | What it does |
| --- | --- |
| **Play / Pause** | Play starts the song (or goes on where it paused). Pause fades out over 1 s and holds the place. |
| **>\|** | Crossfades into the next song now. |
| **\|<** | Crossfades into the song before, or back to the start of this one after its first 5 s. |
| **Fade out** | Fades out (over the Fade-out time, 5 s unless set otherwise) and stops. Play then starts the next song. |
| **Stop** | A short fade and stop; the song goes back to its start. Right-click: **Panic** (everything stops at once: songs, pads and pre-listen). |

Click the waveform to jump within the song. Every start, stop and jump fades, so there is never a click.

## Playlists

A show has up to **8 playlists** (Dinner, Party, First Dance...), shown as tabs. The playing one has a play mark.

- **Click** a tab to show its songs, **double-click** to rename it, **+** to add a playlist.
- **Right-click** a tab: play this playlist now (crossfade) or after the current song, rename, link to a folder or an .m3u8, rescan, unlink, export as .m3u8, delete.
- **Adding songs:** drag files, folders or .m3u8 playlists from the Finder onto the list, or use **Add files...** and **Add folder...**. **Clear** empties the list.
- RKPlay plays WAV, AIFF, FLAC, Ogg Vorbis, MP3 and M4A (AAC, ALAC). A mono file plays on both sides.

Each playlist has its own settings, above its songs:

| Setting | What it does | Default |
| --- | --- | --- |
| **In order / Shuffle** | Shuffle plays every song once, in a random order, before any plays again (and never the same song twice in a row between rounds) | In order |
| **At the end** | Stop, Repeat, or **Then:** another playlist | Repeat |
| **Stop after each** | Waits for Play after every song (a ceremony, songs started by hand) | Off |
| **Crossfade** | How long songs overlap, 0 to 15 s (0: back to back, no gap) | 6 s |
| **Gain** | The whole list louder or quieter, ±12 dB (a quieter Dinner list) | 0 dB |

A playlist remembers its place (and its shuffle order) while another plays, so going back to it continues after the song it was on.

### Songs

**Double-click** a song to play it from there. **Right-click** a song:

- **Play now:** crossfades into it at once (the first dance, the cake). Afterwards the playlist goes on where it was.
- **Play next:** adds it to the **Queue**. Queued songs play after the current song, before the playlist goes on (requests).
- **Go on from here:** the playlist continues from this song.
- **Pre-listen:** plays it on the cue output only (see Pre-listen).
- Move up or down, remove, its **gain** (±12 dB, for a backing track a singer performs over), show in Finder.

The playing song is in the accent colour. A song RKPlay can't find or read is shown in red and skipped when its turn comes; the music never stops for it. The Queue (right) shows the requests in order; right-click one to move or remove it. The queue is not saved with the show.

### Linked playlists

A playlist can be **linked to a folder** or to an **.m3u8 playlist** (right-click its tab). RKPlay reads it again when the show loads and on **Rescan**, so preparing for a gig is just copying files into a folder, or editing the .m3u8 in one place. A linked list takes no songs by hand, but keeps each song's gain. Any list can be **exported** as an .m3u8 (with paths relative to where you save it).

### Switching playlists

Right-click another tab and choose **Play this playlist now** (a crossfade) or **after the current song**. The host parameter **Playlist** and program changes do the same (see the Standalone app).

## End at

The **End at** panel (right) stops the music at a time of day, for "music until the band comes back": turn it on, type the time (for example 21:30) and choose **Fade out exactly then** or **After the song playing then**. A countdown shows under it. It acts once, then turns itself off.

## Loudness

RKPlay measures every song once (its loudness, peaks, where the music really starts and ends, and the waveform) in the background, and keeps the results, so a show with hundreds of songs opens at once. Then it plays every song at the same loudness:

- **Target** (Settings): every song plays this loud. Default **-18 LUFS**.
- **By gain alone:** a song is turned up or down, never compressed. A song already at the target plays untouched, bit for bit.
- **Peak ceiling -0.5 dBTP:** a song is never turned up so far that its peaks would clip.
- **Lift quiet songs** (Off, up to 3 dB, up to 6 dB; default **up to 3 dB**): a very dynamic song that can't reach the target by gain alone is lifted further, and its few highest peaks are limited by at most that much (gently, several seconds ahead of playback). The song list shows the gain each song gets (a lifted song in orange, "lift"), and "measuring" until it is measured.
- **Level match** can be switched off: every song then plays at its own level (plus its gains).
- A song added a moment before it plays, before it is measured, plays at its own level.

RKPlay's output sits a few dB below commercial records on purpose. Set the volume at the desk or the amplifier, or with the **Level** knob (up to +6 dB).

**Safety limiter** (Settings, on by default): a limiter at -0.3 dBFS on the output, for what the measurements can't foresee (two loud songs crossing, a pad over the music, the Level pushed up). A song alone never reaches it.

## Crossfades

- RKPlay skips the silence at the start and end of each file, so there are no gaps.
- **Smart crossfade** (Settings, on): many songs end with a long fade-out; the next song comes in where the fade-out has gone quiet, so the room doesn't go quiet.
- The crossfade keeps the loudness even all the way through. It never takes more than half of either song.

## Ducking

The music dips while someone speaks.

- **Duck button** (and the host parameter **Duck**): dips the music until pressed again.
- **Duck from the input** (Settings): the music dips while RKPlay's input (an announcement mic, split from the desk) is above the threshold. The input is only listened to; it never goes to RKPlay's output.
- **Pads** can duck the music too (see Pads).

The Duck button lights, and shows the dip in dB, whenever the music is dipped.

| Setting | Range | Default |
| --- | --- | --- |
| **Depth** | 0 to -30 dB | -12 dB |
| **Threshold** (input) | -60 to 0 dB | -35 dB |
| **Attack** | 0.05 to 1 s | 0.2 s |
| **Hold** (after the voice stops: pauses between sentences) | 0 to 5 s | 1 s |
| **Release** | 0.2 to 5 s | 1.5 s |

## Pads

**8 pads** at the bottom play short sounds over the music: a fanfare, applause, the couple's entrance.

- **Click** a pad to play it, click again to stop. Click an empty pad (or drop a file on it) to load a sound, up to a minute long. Pad sounds are level matched like songs.
- **Right-click** a pad: load, rename, level, **Loop**, **Duck the music while it plays**, **Fade out the music when it starts** (for entrance music that replaces the song), colour, clear.
- Pads are part of the show.

## Pre-listen

Right-click a song and choose **Pre-listen** to hear it on the **cue output** (headphones) while the music plays on. The Pre-listen panel shows it with its waveform (click to jump), **Stop** and its own level. It stops by itself when that song is played for real.

The cue output is RKPlay's second stereo output: in Gig Performer, the plugin's outputs 3/4; in the standalone app, the two outputs after the main pair (choose them in the audio settings). The panel says when the cue output is switched off.

## Settings

**Settings** opens over the lower half of the window, with two pages:

- **Show:** Fade-out time (0.5 to 30 s), Smart crossfade, the Loudness settings, Ducking, and the **Music folder**.
- **Remote** (standalone app): OSC, MIDI, the keyboard shortcuts and the history.

### The Music folder

Choose the folder your music lives in (for example `~/Music/OFFLINE MUSIC ARCHIVE/Library`). Songs inside it are kept **relative to it**, so the show works wherever the folder is: on another Mac, under another user name, on an external drive. If songs are missing after a move, choose the folder again in its new place, or use **Relink missing songs...** and pick a folder: RKPlay finds them by file name (and checks the size). A song's gain and name stay with it.

## Standalone app

### Remote control from a phone (OSC)

RKPlay can be played from **TouchOSC** (or any OSC app) on a phone or tablet on the same network:

1. In Settings > Remote, turn on **OSC remote**. RKPlay listens on port **8000** and shows the Mac's address.
2. In TouchOSC, set the connection's host to that address, send port 8000, receive port 9000.
3. Type the phone's address into RKPlay (port 9000) so the phone gets feedback: song names, times, button lights.

The remote controls playback only, not the show's setup. The commands:

| Address | What it does |
| --- | --- |
| `/rkplay/play`, `/rkplay/pause`, `/rkplay/playpause`, `/rkplay/next`, `/rkplay/prev`, `/rkplay/fade`, `/rkplay/stop`, `/rkplay/panic` | The transport (on a button's press) |
| `/rkplay/duck` 1 or 0 (no value: toggle) | Duck |
| `/rkplay/pad/1` ... `/rkplay/pad/8` | A pad |
| `/rkplay/playlist/1` ... `/rkplay/playlist/8`; `/rkplay/playlist/after` n | Switch playlist now; after this song |
| `/rkplay/level` 0 to 1 (a fader), `/rkplay/level/db` dB | The Level |
| `/rkplay/endat` "21:30", `/rkplay/endat/off` | End at |
| `/rkplay/hello` | Send all the feedback again (also **Send everything again** in Settings) |

RKPlay sends back, whenever something changes: `/rkplay/play`, `/pause`, `/duck`, `/pad/n` and `/playlist/n` light their buttons; `/title`, `/artist`, `/elapsed`, `/remaining`, `/progress` (0 to 1), `/next`, `/state`, `/playlist/name`, `/playlist/n/name`, `/pad/n/name`, `/duck/db`, `/endat` and `/level` fill labels and faders.

### MIDI

- **MIDI learn:** right-click a transport button, Duck, a pad or the Level knob, choose **MIDI learn**, then press the footswitch or move the controller. Footswitches act as they go down. Learned controls belong to your rig, the same for every show.
- **Program changes 0-7** switch to playlists 1-8.

### Keyboard

| Key | Does |
| --- | --- |
| **Space** | Play / Pause |
| **→** / **←** | Next / Previous |
| **F** | Fade out |
| **S** | Stop |
| **D** | Duck |
| **1** to **8** | Pads |
| **Esc** | Stop pre-listening |

The keys work in the plugin too, while its window has the focus.

### For a long night

- The Mac doesn't go to sleep while music plays.
- The show is saved every 30 seconds. After a crash or a power cut, RKPlay opens it again on the next start, with the song that was playing ready to play (stopped), and says so.
- **History:** every song played for a second or more is written to that night's file (time, playlist or Request, title, artist, how long it played). **Show the history...** in Settings > Remote shows tonight's file in the Finder. Handy for the client, or for rights reporting.

## Starting points

These shows are built in as **factory presets**: open the preset menu and choose **Factory**. They hold no music: load one, drop your songs into its playlists, and **Save as**. All play at -18 LUFS.

| Factory show | What it sets up |
| --- | --- |
| **Intermission** | One playlist "Intermission": in order, repeat, 6 s crossfade; Fade out 8 s |
| **Wedding Reception** | "Arrival" (shuffle), "Dinner" (shuffle, 8 s crossfade, -4 dB), "First Dance" (stop after each song, no crossfade), "Party" (shuffle, 3 s crossfade); Duck from the input on; pad 1 "Entrance" (fades out the music) and pad 2 "Fanfare" (ducks the music), waiting for their sounds |
| **Background** | One playlist "Background": shuffle, repeat, 10 s crossfade, -4 dB. Right-click its tab to link it to your music folder |
| **Ceremony** | One playlist "Ceremony": in order, stop after each song, no crossfade, stop at the end; Fade out 3 s |

## Host parameters

| Parameter | What it does |
| --- | --- |
| **Play**, **Pause**, **Next**, **Previous**, **Fade Out**, **Stop** | The transport: turning one on (a button or footswitch widget) acts once |
| **Level** | The master level, -60 (silence) to +6 dB |
| **Playlist** | 1 to 8: crossfades into that playlist (stopped: the one Play starts) |
| **Duck** | Dips the music while on |
| **Pad 1** … **Pad 8** | Play (or stop) a pad |

Everything else is in the show.

## Notes

- **Bypass** silences RKPlay (with a fade). RKPlay never passes its input through.
- RKPlay adds no latency, and uses very little CPU: the files are read and prepared ahead on a background thread, so playback never waits for the disk. If a disk can't keep up (a sleeping USB drive), the song fades out rather than stuttering, and the next one starts.
- The measurements are kept in `~/Library/Application Support/RKFX/RKPlay/analysis.json`; a file that changes is measured again. The app's other files (OSC settings, learned MIDI, the autosave, the history) are in the same folder.
- Gig Performer tip: put RKPlay on its own outputs into the mixer (or RKMix), map widgets to Play, Fade Out and Duck, and feed the announcement mic into RKPlay's input for automatic ducking.
