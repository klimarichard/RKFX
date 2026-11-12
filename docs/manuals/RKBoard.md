# RKBoard

**Pedalboard** · every RKFX effect built in · plugin (AU, VST3) and standalone app

RKBoard puts RKFX effects on one board, like pedals (as many as you like, up to 64): in a chain, or split into two parallel paths that are mixed back together. Each pedal is exactly the RKFX plugin of the same name (same sound, same controls, same presets), so a sound shaped in a single plugin moves to the board unchanged. Use it inside Gig Performer for gigs, or as a standalone app to shape your tone at home; board presets are the same files in both.

For knobs, presets and the standard controls inside each pedal, see [Common Controls](Common%20Controls.md).

## The window

- **Header (top):** the board preset (name, menu, Save, Save as), the **Assign** and **Remote** buttons, **Undo**, **Redo** and **Lock**, and the board's **In** and **Out** levels (-24 to +12 dB).
- **Scene bar:** the tempo, scenes 1 to 16 in two rows of eight with their names (see Scenes), and the **Morph** section (see Scene morph).
- **The board:** the pedals, flowing left to right. A board too long for one row continues on the next, like text, with a cable from the end of one row to the start of the next; the window grows by a row. The split, both paths and the merge always stay together on one row.
- **Below the board:** the controls of the selected pedal (its own plugin window), the split's or merge's settings, the assignable parameters, or the remote's page.

## The board

```
In -> [chain] -> SPLIT 1 -+- [path A] -+- MERGE 1 -> [chain] -> SPLIT 2 ... -> [chain] -> Out
                          +- [path B] -+
```

A board is a chain of pedals with up to **four splits** in it. Each split sends the signal down two paths, A and B, and its merge adds them back together. Pedals can go anywhere: on a chain (before, between or after the splits) or on any split's paths. A new board has one split. A split only takes part while one of its paths holds a pedal; with both paths empty the signal passes it untouched (it is then drawn small).

- **To add a split:** click the **+** at the end of a chain and choose **Insert a split here**, or right-click a pedal on a chain and choose **Insert a split after it**. The pedals after that point follow the new merge.
- **To remove a split:** right-click its SPLIT or MERGE node and choose **Remove this split**, or use the button in its settings. Its pedals stay: path A's, then path B's, join the chain in its place.

With several splits, the nodes are numbered (SPLIT 1, MERGE 1, SPLIT 2...).

Each pedal is a tile in its family color, with its name, an LED and a footswitch:

| Action | Result |
| --- | --- |
| Click the tile | Shows the pedal's controls below the board |
| Click the footswitch | Bypass on / off (the LED is off while bypassed) |
| Drag the tile | Moves the pedal: within its section, or to another section or path |
| Right-click the tile | Duplicate (a copy with the same settings, right after it), Remove, Trails (delay and reverb), **Assign On/Off to** (see Assignable parameters), and on a chain **Insert a split after it** |
| Click a **+** | Adds a pedal at the end of that section, from a menu grouped by family (on a chain also: **Insert a split here**). A new pedal starts bypassed in every scene; switch it on where you want it |
| Click SPLIT or MERGE | Shows its settings below the board |
| Right-click SPLIT or MERGE | **Remove this split** |

Pedals only use processing power while switched on.

## Split and merge

Every split has its own settings. Like a pedal's knobs, they can be assigned (right-click one: **Assign to**) and **scene-controlled**, so a scene can, for example, bring in a wet delay path by raising the merge's Level B.

Click a **SPLIT** node:

| Split mode | What it does |
| --- | --- |
| **Full** | Both paths get the complete signal. With one path empty this is a wet/dry rig: the empty path is the dry signal. |
| **Frequency** | Lows go to path A, highs to path B, split at the **Crossover** (50 Hz to 5 kHz, 24 dB/octave). With nothing changed on the paths they add back to the original sound. Distort only the highs and keep the bass clean, for example. |
| **Stereo** | The left channel goes to path A, the right channel to path B. Choosing Stereo pans path A left and path B right in the merge (and back to the centre when you leave Stereo), so the stereo image comes out as it went in. |

Click a **MERGE** node:

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Level** (A, B) | -24 to +12 dB | 0 dB | Each path's level in the mix. |
| **Pan** (A, B) | -100% (left) to +100% (right) | 0% | Balance of each path. The centre leaves the path as it is. |
| **Invert** (B) | Off, on | Off | Flips path B's polarity. |

An empty path passes its signal through. If one path is slower than the other (a drive pedal with oversampling adds a few samples), the faster path is delayed to match, so they never smear when mixed.

## Assignable parameters

RKBoard has 32 host parameters, **Assign 1** to **Assign 32**, for Gig Performer widgets, expression pedals and footswitches. Each one turns one knob of one pedal, or switches one pedal on and off.

- **To assign a knob:** right-click it in a pedal's controls, choose **Assign to**, then a number. The menu shows what the other numbers already drive. Switches and buttons in a pedal's controls (an on/off switch, a mode switch, RKLoop's footswitches) are assigned the same way.
- **To assign a pedal's on/off switch:** right-click the pedal on the board, choose **Assign On/Off to**, then a number. The pedal is on while the parameter is at 50% or more (a Gig Performer button: lit = on), and the parameter follows when the pedal is switched another way (the footswitch on the tile, a scene).
- **To remove it:** right-click the knob or the pedal again and choose **Remove**.
- Click **Assign** in the header to see all 32, each with:

| Control | What it does |
| --- | --- |
| **Min** | The knob's value with the parameter at 0 (heel). |
| **Max** | The knob's value with the parameter at 100% (toe). |
| **Inv** | Swaps the two ends. For an on/off switch (no Min and Max): the pedal is on at 0% and off at 100%. |
| **×** | Removes the assignment. |

Assigning a knob never moves it. Turning the knob yourself moves its Assign parameter too, so a Gig Performer widget stays in step. Each knob has one Assign number; an assignment follows its pedal when you move the pedal, and goes away when you remove it.

## Scenes

A board has 16 **scenes**. A scene remembers:

- whether each pedal is on or bypassed (a newly added pedal is bypassed in all of them), and
- the value of every **scene-controlled** knob. Right-click a knob (a pedal's, or a split's or merge's) and tick **Scene controlled** (up to 48 knobs per board). All other knobs are the same in every scene.

Click a scene button to switch. Pedals switch on and off at once; scene-controlled knobs glide to their new values, so nothing clicks. Changes you make while a scene is selected are stored in that scene. Right-click a scene button to copy it to another scene (with its name), then change what should differ.

**Names:** double-click a scene button (or right-click it and choose **Rename...**) to name it: "Intro", "Verse", "Solo" (up to 24 characters; empty for none). The button shows the number and the name under it, shortened to fit; the tooltip shows it whole. Names are saved with the board.

Scenes can also be switched by the host parameter **Scene** (1 to 16, for a Gig Performer widget), and in the standalone app by MIDI program changes 0 to 15. The Scene parameter's text includes the name ("2 Verse"), so a Gig Performer widget showing it reads like the setlist. If a widget still shows an old name after a rename, reload the rackspace.

## Scene morph

An expression pedal blends the board from one scene to another: clean to lead in one sweep, or a verse's delay swelling into the chorus's.

The **Morph** section at the right of the scene bar has **From** (the first button, the current scene unless you choose another), **To** (the second, the next scene unless you choose another) and the **Morph** position (0 to 100%). Morph is a host parameter, so in Gig Performer map an expression pedal to it; in the app, right-click **Morph** for MIDI learn.

- **Knobs:** every scene-controlled knob moves from its value in From (at 0%) to its value in To (at 100%), along the same smooth glide as a scene change. Knobs that aren't scene-controlled don't move (they are the same in every scene).
- **Pedals on and off** can't be blended: a pedal on in both scenes stays on, off in both stays off. A pedal **on in only one** of them is on anywhere between 0% and 100%, and at either end as that scene has it. To fade such a pedal in, make its Dry/Wet (or level) scene-controlled.
- **At 0%** the board is exactly scene From, **at 100%** exactly scene To, and To becomes the current scene (the scene bar follows). A heel-to-toe sweep is a slow scene change; rocking back to the heel returns to From.
- **While morphing** (between 0% and 100%) the scene bar reads "morphing 2 → 3": changes you make then are stored in no scene.
- **Choosing a scene** (a button, the Scene parameter, a program change) makes it From and ends a morph in progress: Morph goes back to 0%, and the next move of the pedal blends from that scene.
- A knob that is also on an **Assign** parameter follows its Assign parameter, not the morph. To morph a knob, make it scene-controlled and leave it unassigned.

**Example** (the factory preset **Morph Swell**): the Delay is off in scene 1 and on in scene 2, with its Dry/Wet scene-controlled at 0% in scene 1 and 35% in scene 2. As the pedal moves from the heel, the Delay switches on at 0% Dry/Wet and swells to 35%; the Distort's Gain and the Verb's Dry/Wet rise with it.

## Undo and redo

**Undo** and **Redo** in the header (or **Cmd+Z** and **Cmd+Shift+Z**) step back and forward through your changes; their tooltips say what they undo. Changes less than half a second apart are one step (a whole knob turn, a scene change with its glide).

- **Recorded:** every pedal's knobs and switches, the splits' settings, pedals on and off, In and Out, assignments (with their range and invert), scene-controlled knobs, trails, scene names and copies; pedals added, removed, duplicated or moved, and splits inserted or removed. A removed pedal comes back exactly as it was, with its assignments and scenes.
- **Scene changes:** undoing one brings back the scene before it and the board as it was, without changing what either scene stores.
- **Not recorded:** loading a board (it starts a new history), the Morph position, the tuner, tempo and tap, and MIDI learn.

Changes from a Gig Performer widget, an Assign parameter or the remote are recorded too.

## Lock

**Lock** protects the board from a stray click during the show. Click it and choose:

- **Footswitches and scenes only:** the scene buttons, the pedals' footswitches, **Tap**, **Tuner** and **Morph** still work; knobs, dragging, adding and removing pedals, the routing, presets, Undo and right-click menus don't.
- **Everything:** nothing can be changed. A pedal can still be clicked to look at its controls.

Locked parts are covered by a faint hatch. **Hold Lock for a second** to unlock. The lock is for the window only: host parameters, MIDI and the remote still work. It isn't saved.

## Trails

Delay and reverb pedals have a **Trails** option (right-click the tile). With trails on, bypassing the pedal stops it taking in new sound, but the echoes or reverb already playing ring out naturally, also when a scene switches the pedal off. With trails off, the sound stops as usual.

## Tempo in a host

In a host, tempo-synced delays and modulation follow the host's tempo. The **BPM** field at the left of the scene bar shows the tempo the pedals play at (dimmed: it can't be typed in the plugin).

To follow MIDI clock instead, right-click the BPM field and tick **Follow MIDI clock**, then connect a clock source to RKBoard's MIDI input. For example, in Gig Performer, switch on **MIDI clock** in RKTracks and connect its MIDI output to RKBoard's MIDI input: the delays then follow the song's tempo, section tempos and tempo changes included.

- While clock arrives, the pedals play at its tempo and the BPM field shows it brightly. When the clock stops (half a second without ticks) or before it starts, they follow the host's tempo again.
- A MIDI **Start** puts beat 1 on its sample, so synced LFOs (tremolo, auto-wah, …) start with the song.
- The setting is saved with the host session (the gig), not in board presets.

## Standalone app

The standalone app has a few things a host would otherwise provide. In the plugin they are hidden: Gig Performer handles controllers and tempo there.

**Audio and MIDI settings** (Options at the top of the window) choose your audio interface and MIDI inputs. Use a small buffer size (64 or 128 samples) for a low delay through the interface.

### Tempo

The **BPM** field (20 to 300) and **Tap** sit at the left of the scene bar. Tap at least twice; RKBoard averages your last few taps, and a pause of two seconds starts over. Tempo-synced delays and modulation follow this tempo. Board presets store their tempo and apply it when loaded.

Right-click **Tap** and tick **Follow MIDI clock** to take the tempo from incoming MIDI clock instead (a drum machine, a sequencer or RKTracks); the BPM field then shows it, dimmed. A MIDI **Start** puts beat 1 on its sample, so synced LFOs start with the song.

### MIDI learn

Footswitches and expression pedals connect through MIDI learn. Right-click what you want to control, choose **MIDI learn**, then press the switch or move the pedal:

| What | Where to right-click | How it reacts |
| --- | --- | --- |
| A pedal on / off | its tile | a switch |
| An assignable parameter | its row in the Assign view | an expression pedal: smoothed, so it never zippers |
| Scene selector | a scene button | the controller's value picks the scene (0 = scene 1, up to 15 = scene 16) |
| Tap tempo | **Tap** | a switch |
| Tuner | **Tuner** | a switch |
| Morph | **Morph** (or its slider) | an expression pedal |

For switches, the same menu chooses **toggle** (each press switches on or off) or **momentary** (on while held). **Clear MIDI** removes the mapping. MIDI program changes 0 to 15 always select scenes 1 to 16.

Mappings belong to your rig, not to a sound: they stay the same whatever board you load, and are kept in `~/Library/Application Support/RKFX/RKBoard/midi-map.json`.

### Tuner

Click **Tuner** (or a learned footswitch) to show the tuner: the note, its octave, and how many cents it is off (the marker turns green within 3 cents). Set the **Reference** pitch (430 to 450 Hz, A4) and whether the output is **muted while tuning**. The tuner listens to a copy of the input, so it never adds delay or changes the sound.

## Remote (phone and iPad)

A phone on a mic stand or an iPad on the pedalboard can run the board: RKBoard serves a web page on the local network. Click **Remote** in the header:

| Control | What it does |
| --- | --- |
| **Remote on** | Starts serving the page. |
| **This board** | This board's name in the remote's list (default "Board"). Saved with the host session. |
| **PIN** | 4 to 8 digits. **New PIN** makes a random one; a new PIN signs every device out. |
| **Port** | 8081 by default (RKMix's remote uses 8080, so both can run in one gig). |
| **Address by IP** | Uses this computer's IP address in the link, for networks that don't find `.local` names. |
| **QR code** | Scan it with the phone's camera: it opens the page and signs in (the PIN is in the link). |
| **Connected** | The devices on the page; **Disconnect** lets one go (it must type the PIN again). |

On the phone or iPad, **Add to Home Screen** (Safari's share menu) makes it a full-screen app. A device that signed in once is remembered. Five wrong PINs make it wait 30 seconds.

The page's tabs, along the bottom:

- **Scenes:** big buttons with the scene names (the current one lit), and the **Morph** fader with its From and To.
- **Board:** the pedals as tiles in signal order, in their family colours (A1, B1... on a split's paths). Tap a tile's **footswitch** to switch it on or off; tap the tile for its knobs (**Done** closes them).
- **Assign:** the Assign parameters in use, as big faders (an expression pedal on the screen), and the on/off switches.
- **Tuner** (the app): the note, its octave, cents and Hz, big; and the tuner on or off.
- **Tempo:** the BPM, and a big **Tap** (in the app; in a host the tempo is the host's).

**Lock** on the page locks the knobs, the Assign faders and Morph's From and To; scenes, footswitches, the Morph fader, Tap and the tuner still work. **Fine** makes faders and knobs move a quarter as far, for small changes. Both are the device's own.

The remote can do what the window can, as clicks in the window would (Undo records them, an edit goes into the current scene). It can't add, remove or move pedals. The window's Lock doesn't apply to it.

**Several RKBoards** (one per Gig Performer rackspace) share one remote: the page lists them by name, and each device remembers its board. The header's name switches between them. The settings (on, PIN, port, devices) belong to the computer, shared by the plugin and the app.

## Board-only pedals

Two pedals exist only on the board, made for an expression pedal through an Assign parameter. You can place as many as you like.

### Volume (Utility, light gray)

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Position** | 0 to 100% | 100% | The pedal. Changes are smoothed, so fast moves never zipper. |
| **Taper** | Linear, Audio | Audio | **Audio** swells evenly to the ear (about -18 dB at the middle), like a real volume pedal. **Linear** follows the position directly. |
| **Minimum** | -inf to -20 dB | -inf | The level at the heel. Above -inf the pedal never fully mutes. |

Where you put it decides the sound: before a drive it works like a guitar's volume knob (backing off cleans up the distortion); after delay and reverb it swells while the tails ring on.

| Factory preset | Settings |
| --- | --- |
| **Volume Pedal** | Audio taper, mutes at the heel |
| **Clean-Up** | Minimum -30 dB, for in front of a drive |
| **Linear Taper** | Linear taper |

### Wah (Filter and EQ, violet)

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Position** | 0% (heel) to 100% (toe) | 50% | The pedal. The sweep follows a classic wah curve: more change towards the toe. |
| **Classic / Vocal / Bass** | | Classic | **Classic**: the familiar resonant wah. **Vocal**: wider and more vowel-like. **Bass**: an octave lower, keeping the low end. |
| **Heel Freq** | 200 to 800 Hz | 400 Hz | Where the sweep starts. |
| **Toe Freq** | 1 to 4 kHz | 2.2 kHz | Where it ends. |
| **Resonance** | 0 to 100% | 60% | How sharp and vocal the peak is. |
| **Auto Engage** | Off, on | Off | Moving the pedal switches the wah on; resting at the heel for half a second switches it off. No footswitch needed. |

| Factory preset | Settings |
| --- | --- |
| **Classic Wah** | Defaults |
| **Auto Engage** | Classic, Auto Engage on |
| **Vocal Wah** | Vocal, Resonance 45% |
| **Bass Wah** | Bass, Heel 250 Hz, Toe 1.6 kHz, Resonance 50% |
| **Narrow Sweep** | Heel 500 Hz, Toe 1.5 kHz, Resonance 80% |

## Starting points

These boards are built in as **factory presets**: open the board's preset menu and choose **Factory**. Load one, then change it to your sound and use **Save as** to keep your version.

| Factory preset | Board |
| --- | --- |
| **Clean Ambient** | Compress, Chorus (Dry/Wet 35%), Delay (25%, trails), Verb (30%, trails) |
| **Crunch** | Gate, Saturate, Distort, Delay (15%, trails) |
| **Wet Dry Rig** | Compress, Distort, then a full split: path A dry, path B Delay and Verb fully wet (trails), path B at -6 dB |
| **Bass Split** | Frequency split at 250 Hz: Compress on the lows (A), Distort on the highs (B) |
| **Wah and Volume** | Wah (auto engage), Distort, Volume, Delay (20%, trails); Assign 1 drives the Wah, Assign 2 the Volume |
| **Scenes Starter** | Compress, Distort, Delay, Verb. Scene 1 "Clean" (Distort and Delay off); scene 2 "Crunch" (Distort on); scene 3 "Lead" (Distort and Delay on) |
| **Morph Swell** | Compress, Distort, Delay, Verb. Scene 1 "Verse": Delay off (Dry/Wet 0%), Verb 15%, Gain 6 dB; scene 2 "Chorus": Delay on at 35%, Verb 35%, Gain 24 dB. Put an expression pedal on **Morph** to swell from one to the other (see Scene morph) |

## Board presets

Board presets live in `Presets/RKBoard` in the RKFX preset library, shared by the plugin and the standalone app. A board preset holds every pedal (as that plugin's own preset), the routing, the assignments and the scenes (with their names, and Morph's From and To when you chose them). A gig saved in Gig Performer is self-contained: it never depends on the preset file.

Each pedal also keeps its own preset menu, so you can load any RKFX preset or factory preset into a pedal on the board, or save a pedal's settings as a preset for the single plugin.

## Host parameters

| Parameters | What they do |
| --- | --- |
| **Input**, **Output** | The board's In and Out levels. |
| **Assign 1** to **Assign 32** | Knobs and on/off switches, see Assignable parameters. An assignment belongs to a pedal, not a place: it follows the pedal when you move it. |
| **Scene** | 1 to 16; its text includes the scene's name ("2 Verse"). |
| **Morph** | 0 to 100%: blends scene From into scene To, see Scene morph. |

## Notes

- RKBoard adds no latency, except a few samples when a drive pedal uses oversampling (as in the single plugins).
- To use the vocoder pedal, send MIDI into RKBoard: its carrier synth plays from it.
- Gig Performer tip: map an expression pedal to an Assign parameter driving a Wah or Volume pedal, and footswitches to Assign parameters switching pedals on and off, or to the Scene parameter.
