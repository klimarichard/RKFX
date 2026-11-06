# RKBoard Live Control: design

**Undo and redo, Lock, named scenes, scene morph, and a phone and iPad remote for RKBoard** · nothing in the audio path changes except the morph's knob values

RKMix already has undo, a show lock, scenes and two remotes; RKBoard has none of them. This brings RKBoard to the same level, reusing RKMix's code where it fits, and adds two things RKMix doesn't have: **scene names** and **scene morph** (an expression pedal blends from one scene to another). This is a design, not a manual: it says what the feature does and how, so it can be built and reviewed against it.

## Suite rules

- **Zero latency, no change to the sound.** Undo, Lock, names and the remote work on the message thread. Morph only sets knob values, through the same parameter path the scene glide uses.
- **Live-safe:**
  - Undo never rebuilds what it doesn't have to: a knob undo sets a value, and only a structural undo (a pedal added, removed or moved) swaps the chain, the way an edit already does (prepared on the message thread, swapped in under the short spin lock).
  - The remote can't do more than the window could.
- **Consistent with RKMix:** the same Lock levels and hold-to-unlock, the same undo grouping (changes less than half a second apart are one step), the same remote look (the suite's knobs, faders and meters in `rk.js`).

## 1. Undo and redo

**What's recorded:**

- **Values:** every pedal's knobs and switches, every split's settings, bypasses, scene-controlled flags, trails, assignments (slot, range, invert), In and Out.
- **Structure:** pedals added, removed, duplicated or moved; splits inserted or removed; a pedal preset loaded into a pedal.
- **Scene recalls**, as in RKMix: undoing a recall brings back the board and the scene before it, and stores nothing into either scene.
- **Not recorded:** loading a board preset (it starts a new history, as in RKMix), the tuner, tempo and tap, MIDI learn (it belongs to the rig, not the board).

**How:** unlike RKMix (where the structure isn't undoable), a board's structure is small (at most 64 pedals and 4 splits), so each structural step stores a **snapshot of the board's layout** (sections as ID lists, the split settings, the pedal types) together with the full state of every pedal it removed. Undoing a structural step:

- re-creates only what's missing (a removed pedal is created and prepared on the message thread from its saved state, with its **same ID**, so its assignments, scenes and MIDI learn come back with it);
- keeps the pedal objects that are still there (no reloading, no glitch);
- swaps the chain in once.

Value steps are recorded by polling, as `MixUndo` does: a 10 Hz timer compares the tracked parameters with the last values. Polling works for changes from anywhere (the window, a Gig Performer widget, an Assign slot, the remote) without audio-thread listeners. The tracked set changes as pedals come and go, so the poller is rebuilt with the chain, keyed by owner (pedal ID or routing owner) and parameter ID.

**Where to share code:** `MixUndo`'s grouping and descriptions ("Undo Delay: Time", "Undo 12 changes") are generic. Move the step and grouping logic into `host/` (shared by RKBoard and RKMix) and keep a small adapter in each plugin for what it tracks and how a scene recall is undone.

**UI:** **Undo** and **Redo** in the header (tooltips say what they undo), Cmd+Z and Cmd+Shift+Z.

## 2. Lock

As RKMix's Lock (`MixLock.cpp`), with RKBoard's two levels:

- **Footswitches and scenes only:** scene buttons, pedals' footswitches, Tap, Tuner and morph still work. Knobs, dragging, adding and removing, the routing, presets, and right-click menus don't.
- **Everything:** nothing changes; selecting a pedal to look at its controls still works.

**Hold Lock for a second** to unlock. Undo is off while locked. The lock is for the window only (host parameters, MIDI and the remote still work) and isn't saved. The remote has its own Lock (as RKMix's remotes have).

## 3. Named scenes

- **Double-click a scene button** (or right-click, **Rename...**) to name it: "Intro", "Verse", "Solo". The button shows the number and, below it, the name (shortened to fit; full in the tooltip).
- Names are saved with the board (board presets and the host state) and copied with **Copy to**.
- The **Scene** host parameter's text shows the name: "2 Verse". Gig Performer shows a parameter's text on a widget's label, so a scene widget reads like the setlist. Check that Gig Performer refreshes that text after a rename; if it caches it, document that a rename shows after the rackspace is reloaded.
- Markers in RKMix's recorder and the remote use the names too.
- **RKMix** gets the same (its scene bar and its Scene parameter) as a small follow-up, with the same code.

## 4. Scene morph

An expression pedal blends from one scene to another: clean to lead in one sweep, a verse's delay swelling into a chorus's.

- A **Morph** section in the scene bar: **From** (a scene, default the current one), **To** (a scene, default the next one), and the **Morph** position (0 to 100%).
- **Morph** is a host parameter (and assignable through MIDI learn in the standalone app), so an expression pedal drives it.
- **Knobs:** every scene-controlled knob (pedals' and splits') sits at `from + (to − from) × morph`, in normalised value. That's the same path as the scene glide, so it's already smoothed against zipper noise. Knobs that aren't scene-controlled don't move (they're the same in all scenes).
- **Bypasses** can't be blended, so: a pedal on in both scenes stays on; off in both stays off. A pedal **on in only one** of them is on whenever morph is between 0% and 100%, and at either end it's as that scene has it. A pedal coming in on the way has its own knobs to fade it in: a scene-controlled Dry/Wet or level, which is how a board is normally built for this. The manual says so with an example (Delay off in scene 1, on in scene 2 with Dry/Wet scene-controlled at 0% in scene 1 and 35% in scene 2).
- **At 0%** the board is exactly scene From, **at 100%** exactly scene To: morph then hands over. Reaching 100% makes To the current scene (the scene bar follows), and From becomes To's predecessor. A heel-to-toe sweep therefore acts like a slow scene change.
- **Edits during a morph** (morph strictly between 0% and 100%) aren't stored into either scene. The scene bar shows "morphing 2 → 3" and the knobs' scene marks are dimmed, so it's clear nothing is being stored.
- **Recalling a scene** sets morph's From to it and ends a morph in progress. Gig Performer users with a widget on Morph get a sensible state: the widget moves the board again from wherever the scene left it.
- Assign slots, which drive knobs too, win over morph for their knob (an assigned knob follows its Assign parameter). The manual says not to make a knob both assigned and scene-controlled if it should morph.

## 5. The remote (phone and iPad)

One web page served by RKBoard on the local network, drawn with the suite's own controls, for a player standing at the board: a phone on a mic stand or an iPad on the pedalboard.

**Pages:**

- **Scenes:** big buttons with their names, the current one lit, and the Morph fader (multi-touch, as RKMix's faders).
- **Board:** the pedals as tiles in signal order, in their family colours. Tap the footswitch for bypass; tap the tile for its main knobs (the pedal's controls, as the console remote shows an insert's).
- **Assign:** the 32 Assign parameters that are in use, as faders (an expression pedal on the screen).
- **Tuner** (standalone app): the note and the cents, big.
- **Tempo:** the BPM and a big Tap button.

**Access:** the board is one player's, so it's PIN-protected like RKMix's console remote (a QR code with the PIN, remembered on the device), not open like the monitor pages. Lock and Fine as on RKMix's remotes.

**Server and code, shared with RKMix:**

- `MixWebServer` (HTTP and WebSocket, `juce_core` only) and `MixQr` are generic: move them to `host/remote/` (renamed `RemoteServer`, `RemoteQr`), used by both plugins. RKMix keeps its behaviour; its tests are the check.
- `rk.js` (the controls) is shared too; RKBoard's page is its own `board.html` / `board.js` in `plugins/RKBoard/remote/`.

**Several RKBoards in one Gig Performer** (one per rackspace, or a two-part board as in RKBoard.md's linked-boards note): one server per process, through a `juce::SharedResourcePointer`. Each instance registers under a short name (default "Board", editable, saved with the board), and the start page lists them. That avoids a port per instance. The default port is **8081**, so it never clashes with RKMix's 8080 in the same gig; it can be changed.

**Protocol:** as RKMix's console remote. The state is pushed when the board's counter (`getBoardCounter`) or a value changes; messages are `{t:"scene", n}`, `{t:"bypass", id, on}`, `{t:"param", owner, id, v}`, `{t:"assign", slot, v}`, `{t:"morph", v}`, `{t:"tap"}`. Each is applied on the message thread, into undo and the current scene as a click in the window would be.

## Tests

- Undo: a knob, a bypass, an assignment, a move, a removal (the pedal comes back with its ID, settings, assignments and scenes), a split removed and restored; grouping; a scene recall undone; a board load clears the history.
- Lock: both levels block what they should (as RKMix's lock tests).
- Names: saved, copied with Copy to, shown in the Scene parameter's text.
- Morph: 0% equals From and 100% equals To exactly; the halfway values; the bypass rule; edits mid-morph stored nowhere; reaching 100% hands over; a recall ends the morph.
- Remote: the shared server's tests still pass for RKMix; RKBoard's protocol (each message, the PIN, two instances listed).
- Benchmark: RKBoard idle must not move.

## Build order

1. **Named scenes** (smallest, and morph and the remote use them).
2. **Undo and redo** (with the shared `host/` code; RKMix moved onto it with no change in behaviour).
3. **Lock.**
4. **Scene morph.**
5. **Remote:** move the server to `host/remote/`, then RKBoard's page.
6. **Manuals** (`docs/manuals/RKBoard.md`; RKMix's for names), then README.

## Not in this version

- Linked boards (see `RKBoard.md`): a remote listing several instances is the first step towards it.
- Morph across more than two scenes, or a morph curve.
- Remote editing of the board's structure (adding and moving pedals).
