# RKPlay Web Remote: design

**A phone and iPad web app for RKPlay, served by RKPlay itself, on the same server and controls as RKMix's and RKBoard's remotes** · nothing in the audio path changes

RKPlay can already be played from TouchOSC (`PlayRig`, OSC on port 8000). That works, but it needs an app, a layout file and two addresses typed by hand, and a TouchOSC layout can only show a page of songs at a time. A web remote needs nothing installed: scan the QR code and the page opens. It also has room for what OSC can't do well: the whole song list with search, the queue to reorder, and every playlist. This is a design, not a manual: it says what the feature does and how, so it can be built and reviewed against it.

## Suite rules

- **No change to the sound.** The remote runs on the message thread and calls the same processor methods the window does (`play()`, `queueNext()`, `switchPlaylist()` ...). Nothing new on the audio thread.
- **Live-safe.**
  - A stray tap never changes the music. Tapping a song only selects it; **Play next** or **Play now** plays it, as on the OSC remote.
  - **Stop** and **Panic** need **Unlock** first, and lock again after one press (as in the TouchOSC layouts).
  - The remote controls **playback**, not the show's setup (as the OSC remote): no adding or removing songs, no playlist settings, no loudness settings.
- **Consistent with the suite.** Same server (`host/remote/RemoteServer`), same QR code (`RemoteQr`), same controls and look (`rk.js`), same PIN and device handling as RKBoard and RKMix's console remote.
- **OSC stays.** The web remote is a second way in, next to OSC and MIDI. Any of them can be on at the same time, and they all show the same state.

## What it does

### Pages

The page adapts to the screen. On a **phone** (portrait) the pages are tabs along the bottom: **Play, Songs, Queue, Lists, Pads**. On an **iPad** (landscape) Play and Pads sit on the left and Songs on the right, with Queue and Lists as tabs over the song list.

**Play**

- The playing song: title and artist (large), elapsed and remaining time, a progress bar. The remaining time and the bar count to an end set by hand, as in the window.
- **Next:** what plays after this song (the queue's first, or the playlist's next song), and the playing playlist's name.
- Transport: **Play/Pause, Previous, Next, Fade out**, all large. **Stop** and **Panic** behind **Unlock**.
- **Level:** a suite fader (`rk.js`) with its dB value. **Duck** (a toggle, lit while on) and the duck amount in dB.
- **End at** and **Switch at**, when set: the time and the countdown ("Ends 23:30, in 1:12:05"). The remote can set End at (a time picker) or turn it off, as OSC's `/endat` can. Switch at is shown only (it's part of the show's setup).
- The window's **note** ("Skipped: file missing"), as a line that fades after a few seconds.
- A **connection light**: green while connected; grey, with "Reconnecting...", when the Wi-Fi drops. The page reconnects by itself.

**Songs**

- The whole playlist, scrolled freely (not a page at a time): title, artist, length. The playing song is marked ▶; a missing one is greyed out with "(missing)".
- A **playlist picker** at the top: the playing playlist by default, but any playlist can be browsed (for a request that's in another list).
- **Search:** filters by title and artist across **all** playlists, shown grouped by playlist. This is the main reason for the web remote: someone asks for a song, you find it in two seconds.
- **Tap** a song to select it. A bar appears with **Play next** (onto the queue), **Play now** (a one-off), and **Go on from here** (jump: the playlist goes on from it). Same three as OSC's `/selected/next | now | jump`.
- **Follow:** the list scrolls to the playing song when it changes, except for 30 s after you scroll by hand (as the OSC remote's page).

**Queue**

- The requests waiting, in order. **Drag** to reorder, **swipe or ×** to remove, **Clear** (asks once). OSC only shows the first five; this shows all of them and can change them (`moveQueued`, `removeQueued`, `clearQueue`).

**Lists**

- Every playlist as a big button, the playing one lit, with its song count. Tap one and choose **Now** (crossfade into it) or **After this song**. A pending switch is shown ("Switching to Dinner after this song", with **Cancel**).

**Pads**

- The eight pads as big buttons with their names, lit while playing, with a progress ring. Tap to start, tap again to stop (as `triggerPad`).

### Access

- **PIN-protected**, like RKBoard and RKMix's console remote. RKPlay plays to a whole room, so a guest on the venue Wi-Fi mustn't be able to change the music. The QR code carries the PIN (in the URL fragment, never sent to the server), and a device that signed in once keeps a token. Five wrong PINs from one address make it wait 30 seconds.
- **Several devices at once** (the DJ's phone and the venue manager's iPad): they all see the same state, and a change on one shows on the others.
- **Port 8082** by default (RKMix uses 8080, RKBoard 8081, so all three can run in one gig). It can be changed.
- **Off by default.**

### Where it's set up

In **Settings > Remote**, a new **Web remote** section above OSC: on/off, the link and its QR code, the port, the PIN, the connected devices with **Disconnect**, and "Use the IP address in the link" (for networks where `.local` names don't work), as RKBoard's Remote panel.

**In the plugin too.** RKMix's and RKBoard's remotes work in the plugin, so RKPlay's should: RKPlay in a Gig Performer rackspace can be played from a phone. The plugin's Settings then gets a Remote page that has only the Web remote section (OSC, MIDI, history and recovery stay the standalone app's, as now). The settings (on, port, PIN, devices) are the computer's, shared by the plugin and the app, in `Application Support/RKFX/RKPlay/remote.json`.

## How

### Code

- **`plugins/RKPlay/PlayRemote.h/.cpp`**: the hub, modelled on `BoardRemoteHub`. It owns the `RemoteServer` (one per process through a `juce::SharedResourcePointer`, so two RKPlays in one host share the port and the page lists them by name, as RKBoard does). Incoming messages are queued and handled on the message thread (a 20 Hz tick); a push thread sends what waits.
- **`plugins/RKPlay/remote/play.html` and `play.js`**: the page, using `host/remote/rk.js` for the fader, the buttons and the palette. Song lists are long, so they're drawn as normal HTML rows (scrolling, search field, drag) rather than on the canvas; the transport, fader and pads use `rk.js`.
- **PIN and device code: copied** from RKBoard's `BoardRemote` for now (the user's choice, 2026-11-10). Moving it into a shared `host/remote/RemoteAccess` used by RKMix, RKBoard and RKPlay is left for later.
- **The rig's song-list selection isn't reused.** `PlayRig`'s page and selection are for TouchOSC (one shared selection for every OSC device). The web remote keeps its selection per device, on the page itself, and sends the song with the command.

### Protocol

JSON over a WebSocket at `/ws/play`, as RKBoard's: `hello` (PIN or token), then `open` (an instance, when there are several).

**From RKPlay:**

- `{"type":"now", state, title, artist, elapsed, remaining, progress, next, playlist, duck, duckDb, level, levelDb, endAt, switchAt, pendingSwitch, note}`: when something changes, and four times a second while playing. The page moves the clock and the bar smoothly between messages.
- `{"type":"lists", lists:[{name, count, linked}], playing, version}`: when the show changes (`getShowVersion`).
- `{"type":"songs", playlist, version, songs:[{title, artist, length, missing}]}`: one playlist's songs, sent when the page asks for it (`{"t":"songs", playlist}`) and again when that playlist changes. A big playlist (thousands of songs) is sent once, not every tick.
- `{"type":"queue", items:[{id, title, artist, length}]}`, `{"type":"pads", pads:[{name, playing, progress}]}`: when they change.
- `{"type":"search", query, results:[{playlist, index, title, artist, length}]}`: the answer to a search (at most 200 results; the search runs in RKPlay, so the page never needs every playlist at once).

**To RKPlay:**

- Transport: `{"t":"play"}`, `pause`, `playpause`, `next`, `prev`, `fade`, `stop`, `panic`.
- `{"t":"level", v}` (normalised, as the fader), `{"t":"duck", on}`, `{"t":"pad", n}`.
- `{"t":"switch", playlist, now}`, `{"t":"cancelSwitch"}` (RKPlay already clears a pending switch when the playing playlist is chosen: `switchPlaylist (active, false)`).
- Songs: `{"t":"queueNext" | "playNow" | "jump", playlist, index, file}`. **`file`** (the song's path, as RKPlay sent it) is checked before acting: if the list changed since the page got it and that index is now another song, RKPlay looks for the file in the playlist; if it's gone, nothing plays and the page says "That song has moved; try again". So an edit in the window between the tap and the press never plays the wrong song.
- Queue: `{"t":"queueMove", from, to}`, `{"t":"queueRemove", id}`, `{"t":"queueClear"}`.
- `{"t":"endAt", time:"23:30"}` or `{"t":"endAt", off:true}`.
- `{"t":"search", query}`, `{"t":"songs", playlist}`.

Stop and Panic's Unlock is on the page only (the server trusts a signed-in device, as RKMix's and RKBoard's remotes do).

### The window

- The window shows nothing new except the Web remote section in Settings, and a small **remote light** in the header while a device is connected (so you know someone else can change the music).

## Tests

- **Protocol:** each command does what the same click in the window does (play, next, queue next, play now, jump, switch now and after, queue move and remove and clear, level, duck, pad, End at).
- **The file check:** a song moved or removed between the page's list and the command: the right song plays, or nothing does.
- **State messages:** `now`, `lists`, `songs`, `queue` and `pads` are sent when they change and not otherwise; a playlist's songs are sent again only when that playlist's version moves.
- **Search:** title and artist, case and accents ignored ("cafe" finds "Café"), across playlists, at most 200 results.
- **Access:** wrong PIN, lockout, token remembered, Disconnect forgets the device (through the shared `RemoteAccess`, whose tests then cover all three plugins).
- **RKMix and RKBoard:** their remote tests pass unchanged after the move to `RemoteAccess`.
- **The page:** checked in headless Chromium (Playwright in the session scratchpad, nothing installed system-wide) against an `RKFXRemoteDemo_RKPlay` tool that serves a demo show, in phone and iPad sizes, as was done for RKBoard.

## Build order

1. **`RemoteAccess`**: move the PIN and device code into `host/remote/`, with RKMix and RKBoard on it, all tests passing.
2. **`PlayRemote` hub and protocol**, with its tests (no page yet).
3. **The page:** Play and Pads first (usable for a show), then Songs with search, then Queue and Lists.
4. **Settings > Remote** (the Web remote section, the QR code, devices), in the app and the plugin; the header light.
5. **`RKFXRemoteDemo_RKPlay`** and the Playwright check on phone and iPad sizes.
6. **Manual** (`docs/manuals/RKPlay.md`: a "Remote control from a phone (web)" section before the OSC one), then README.

## Not in this version

- **A guest request page** (an open page where guests suggest songs, which wait for your OK). Easy on this server later, but it's a different feature with its own questions (moderation, spam).
- **Editing the show** from the remote (adding songs, playlist settings).
- **Pre-listening** on the remote (it plays on the Mac's cue output, which the person with the phone usually can't hear).
- **Seeking** within a song (OSC can't either; too easy to do by accident).
- **Album art.**

## As built (2026-11-10)

Built as designed, with these differences:

- **The page is all canvas** (`rk.js`), song lists included: only the rows on screen are drawn, so a long playlist scrolls smoothly (with momentum). The search field is `rk.js`'s text input; a search runs on Enter.
- **iPad layout:** Play alone on the left; Songs, Queue, Lists and **Pads** as tabs on the right (Play and the pads together didn't fit). The page scales to about 1000 points wide on an iPad held sideways, 400 on a phone.
- **Settings:** a third page, **Web remote**, between Show and Remote (in the plugin too); the OSC page is unchanged.
- **The light** is in the bottom bar, right of the pads ("● Remote (n)", click for the settings), not in the header: the transport row is full.
- **Players' names** in the page's list are the show (preset) names; RKPlay has no remote name of its own.
- **Unlock** locks again after one press of Stop or Panic, or after 5 seconds.
- `RKFXRemoteDemo_RKPlay` serves the page from a headless RKPlay with a made-up show; with `RKPLAY_DEMO_SNAPSHOT=<png>`, SIGUSR1 saves the window's Web remote page. The page was checked with Playwright Chromium (in the session scratchpad) at phone and iPad sizes.
- **Untested on a real phone or iPad, and in Gig Performer.**
