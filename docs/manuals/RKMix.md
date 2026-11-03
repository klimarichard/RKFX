# RKMix

**Mixing console** · 32 inputs, 32 outputs · every RKFX effect as an insert · plugin (AU, VST3) and standalone app

RKMix is a live mixing console in one plugin: 32 input channels with a full channel strip, 16 buses that can be groups, monitor mixes or effect returns, a stereo main with a mono sum, and any RKFX effect in insert slots. It has 8 DCAs and 8 mute groups, solo, talkback, a test oscillator, 16 scenes and 32 assignable controls. Use it in Gig Performer as the heart of a rig (one plugin for the whole band, with in-ear mixes), or as a standalone app on an audio interface.

For knobs, presets and the controls inside each insert, see [Common Controls](Common%20Controls.md).

## The signal flow

```
input -> trim, polarity, delay -> HPF -> gate -> EQ -> compressor -> 2 inserts -> mute, fader, pan -> main
                                                          (sends and direct outs tap in along the way)
Group buses -> main (and later groups, Aux and FX buses)
FX buses    -> main (and Aux buses)
Aux buses   -> their own outputs (in-ear and wedge mixes, a recorder)
main        -> EQ -> comp -> 4 inserts -> fader, balance -> limiter -> main output
               mono sum (L + R) / 2 -> EQ -> comp -> 2 inserts -> fader -> limiter -> mono output
```

Every section has its own **On** switch. A section that is off is skipped completely: it costs nothing and leaves the sound untouched.

## The window

- **Top panel:** the selected channel or bus in full, with page tabs: **Channel** (or **Bus**, **Main**, **DCA**, **Mute group**), **Sends**, **Routing**, **Talk / Osc**, **Scenes**, **Assign**, **Remote**; then **Undo**, **Redo** and **Lock**.
- **Master section (top right, always there):** solo settings, Clear solo, TALK, OSC, and in the standalone app the tempo.
- **Fader area:** 16 strips at a time. Click a strip's name to show it in the top panel.
- **Main and Mono strips** (right): click one to open the Main page.
- **Bottom row:** the strip pages (**Inputs 1-16**, **Inputs 17-32**, **Buses**, **DCA**, **User 1**, **User 2**), the scene bar and Bypass.

Each strip shows its name and colour, the H G E C lights (high-pass, gate, EQ, compressor on; G and C light up while they reduce), its insert slots, the pan, Mute, S (solo), the fader with its level and the meter. Right-click a strip for its colour, its user pages, and its DCAs and mute groups. A strip whose number is on orange is silenced by a DCA's Mute or a mute group (its own Mute may be off).

## Channels

**Link** (the Link button on the Channel page) joins two neighbouring inputs (1/2, 3/4...) into one stereo channel with one set of controls. A linked pair takes one strip.

**Double-click** the name field at the top of the Channel page to name a channel. **Strip presets** saves and loads a channel's sound (the strip, its inserts, name and colour, not its routing), for example "Lead vocal" or "Violin pickup".

| Section | Controls | Range | Default |
| --- | --- | --- | --- |
| **Input** | Trim, Ø (polarity; a pair has one per side) | -24 to +24 dB | 0 dB, normal |
| **Delay** | Time: line up a mic with another (a close and a far mic) | 0 to 20 ms | 0, off |
| **High-pass** | Freq, slope 12 or 24 dB/oct: removes rumble and handling noise | 20 to 400 Hz | 80 Hz, 12, off |
| **Gate** | Threshold, Range, Attack, Hold, Release, Key | -80 to 0 dB; Range down to closed | -50 dB, closed, 1 ms, 50 ms, 150 ms, off |
| **EQ** | Low (shelf or bell), Lo Mid, Hi Mid, High (shelf or bell): Freq, Gain, Q each | 20 Hz to 20 kHz, ±15 dB, Q 0.3 to 10 | 100 Hz, 250 Hz, 2.5 kHz, 8 kHz, flat, off |
| **Compressor** | Threshold, Ratio, Knee, Attack, Release, Makeup, Key | -60 to 0 dB, 1:1 to 20:1 | -20 dB, 3:1, 6 dB, 10 ms, 150 ms, 0 dB, off |
| **Output** | Pan (Balance on a pair), Main, Solo safe | | centre, Main on |

- **Key** lets the gate or compressor listen to another input instead of the channel itself: a bass gated by the kick, or music ducking under a voice.
- **Main** off takes the channel out of the main mix (it can still feed groups, sends and its direct out).
- The EQ curve and the compressor's curve and gain-reduction meter are drawn on the page; a shelf's Q knob is dimmed (a shelf has a fixed shape).

### Inserts

Each channel has **2 insert slots** (after the compressor), each bus and the main **4**, the mono sum **2**. Click an empty slot (**+**) to choose any RKFX effect, by family. Click a filled slot to open the effect's own window over the console, with **On**, **Remove** and **Close**; right-click a slot to switch it on or off, replace or remove it. An effect that is off costs nothing. On a mono channel or mono bus the effect runs in mono.

## Sends and direct outs

The **Sends** page shows where the selected channel goes:

- **Aux and FX buses:** a **Send** knob each. Under the send to a stereo Aux bus whose **Pan** is **Own**, a **pan** slider places the channel in that mix (a balance for a stereo pair; double-click for centre). An Aux send also has a **tap**, the point where it takes the channel: **Default** (the bus's own setting, marked with a dot), **Input**, **Pre-EQ**, **Pre-Comp**, **Pre-Ins**, **Pre-Fader** or **Post-Fader**. For example, the singer's in-ear can take the vocal Pre-Comp while the other mixes take it after the compressor. FX sends are always after the fader.
- **Group buses:** an **Assign** switch each (after the fader and pan).
- **Direct out** (the page's header): the channel straight to an output as well, for a multitrack recorder or a broadcast feed, at a tap: **Input** (as it comes in, after the trim), **Pre-EQ** (after the gate), **Pre-Comp** (after the EQ), **Pre-Inserts** (after the compressor), **Pre-Fader** (after the inserts) or **Post-Fader**. A direct out is never panned. Before Post-Fader it ignores mute and solo, so a recording keeps going when you mute the channel in the room; Post-Fader follows the fader and mute.

A muted channel is silent in every send.

**Sends on fader:** on an Aux or FX bus's page, **Sends on fader** turns every channel fader into that channel's send to the bus, the way monitor mixes are set on a digital console. If the bus's **Pan** is **Own**, the strips' pans turn the bus's colour and set each channel's pan in that mix instead. **Back to faders** (in the master section) returns.

## Buses

Click the **Buses** page, then a bus strip's name, for the **Bus** page:

| Setting | Values | Default |
| --- | --- | --- |
| **Type** | **Group**, **Aux**, **FX** | Aux |
| **Format** | Mono, Stereo | Stereo |
| **Output** | Off, an output (mono bus) or an output pair (stereo bus) | Off |
| **Main** (Group and FX) | on, off | on |
| **Pan into the main** (Group and FX) | L100 to R100 | centre |
| **Default tap** (Aux) | Input to Post-Fader | Pre-Fader |
| **Pan** (stereo Aux) | **Own** (each send's own pan: every in-ear mix gets its own stage picture), **Follow** (the channels' pans), **Mirror** (left and right swapped: the band's view of the stage) | Follow |
| **Solo safe** | Auto (on for FX buses), On, Off | Auto |

- **Group:** collects the channels assigned to it, after their faders, for one fader and shared processing ("Drums", "Backing vocals"). A group can feed later groups (by number) and send to Aux and FX buses.
- **Aux:** a monitor or in-ear mix, a recorder or a broadcast feed. Fed by sends, goes to its own output, never to the main.
- **FX:** an effect return. Put an RKVerb or RKDelay in its insert at 100% Dry/Wet, send channels to it, and it returns into the main (and can be sent on to Aux buses, so the singer hears the reverb).

Every bus also has an **EQ** (high-pass, 6 bands, low-pass), a **Compressor** ("glue", with Auto release) and a **Limiter** after its fader, each with its own On switch and off by default. The limiter never lets the bus past its **Ceiling** (-20 to -0.1 dBFS, default -1), even with the fader pushed up: switch it on for every in-ear mix.

A bus's **Sends** page shows what it feeds: later groups, and Aux and FX buses.

## Main and mono

Click the Main or Mono strip for the **Main** page (Main | Mono). The main pair has an **Output** (default Out 1/2), **Balance**, and the same EQ, Compressor and Limiter as a bus; the mono sum ((L + R) / 2 of the main, after its fader) has an Output (default Off) and its own EQ, Compressor and Limiter, for a centre or front-fill speaker.

## Routing

The **Routing** page sets the main and mono outputs and the main balance, and lists all 32 outputs with what feeds each one (the main, buses, direct outs; several sources on one output add up). **Double-click** an output's name to rename it ("Stage wedge left"). The **MIDI out** box lists the MIDI-sending inserts (below).

## Solo

Each strip has an **S** button. The master section sets how solo works:

- **Cue** (default, safe for live): soloed strips go to the **Cue output** (any output pair; set it to your headphones) and the main mix is never touched. **PFL** hears the channel before its fader and mute, **AFL** after the fader and pan. **Cue with no solo** sets what the cue output plays while nothing is soloed: Silence, the Main, or a bus.
- **In place** (rehearsal and studio): soloing silences everything else, everywhere. Strips with **Solo safe** on (FX buses by default) keep playing, so the reverb stays.

**Clear solo** lights while anything is soloed (and flashes while soloing in place). Solo is never saved.

## Talkback and the oscillator

The **Talk / Osc** page:

- **Talkback:** choose the **Source** input (a talkback mic), its **Level**, and which Aux buses (all by default) and the main it goes to. It passes a 100 Hz high-pass. Hold **TALK** to talk, or switch **Latch** on to make TALK a switch. TALK is also a host parameter, so a Gig Performer button or a footswitch can work it.
- **Oscillator:** a **Sine** (20 Hz to 20 kHz) or **Pink** noise at a **Level** (-60 to 0 dBFS, default -18), into chosen buses and the main, or **Straight to** one output, to check a speaker or a cable. **OSC** in the master section switches it on.

Talk and the oscillator are always off when a mix loads.

## MIDI out

RKMix sends MIDI from two kinds of inserts: **RKTrigger** (a kick or snare mic triggering a drum sampler) and **RKFollow** (a voice or violin playing a synth). Each uses its own MIDI channel and notes. Wire RKMix's MIDI output to the instrument in Gig Performer.

- When two inserts could send the same note or controller on the same channel, their slots show **MIDI** in the warning colour and the Routing page lists the clash. It only warns: doubling a note can be intended.
- Mute doesn't stop MIDI (a muted kick mic still triggers its sample). Switch the insert off to stop it.
- **MIDI thru** (Routing page, default off) passes RKMix's MIDI input on to its output.

## The monitor remote (phones and tablets)

Each musician sets their own in-ear mix on their phone, with nothing to install: a web page that RKMix serves on your network.

1. On the **Remote** page, click **Remote off** to switch it on. It shows the address it listens at (`http://<this computer's name>.local:8080`).
2. Every Aux bus in use (with a name or an output) gets a **QR code**. The musician points the phone's camera at their code and opens the link.
3. On the phone: **My mix** (the bus's level and Mute) and a row per channel. **Drag a row sideways** to set that channel's level in the mix (the list scrolls when you drag up and down). In an **Own** bus each row also has its pan (double-tap it for the centre). FX and Group buses that feed the mix (the reverb) are rows too, marked "return". The thin green bar is the channel's signal. **Lock** (top right) stops pocket touches. **Show all channels** also shows the unnamed channels that aren't in the mix yet.
4. **iPhone and iPad:** Share → **Add to Home Screen**. The mix gets its own icon and opens full screen, like an app. Android: the browser's menu → Add to Home screen (or Install app).

A phone can only change its own mix: its sends, its returns, its level and mute. Nothing else in the console, and no other musician's mix. Each mix's link has its own key. The links stay the same from mix to mix and gig to gig, so a home-screen icon keeps working. **New links** makes new ones (the old links stop working: everyone scans again).

- The phones and the computer must be on the **same network** (Wi-Fi or the computer's hotspot). The first time, macOS asks whether Gig Performer (or RKMix) may accept incoming connections and find devices on the local network: allow it.
- **Links by IP address:** if a phone can't open the `.local` name (some Android phones and some networks), the links use the computer's IP address instead. An IP address can change between gigs; the name doesn't.
- **Port:** 8080 unless another app already uses it (double-click to change). In **Gig Performer**, put RKMix in the **global rackspace**: only one RKMix can serve, and if another RKMix already took the port, the Remote page says so.
- The phone's screen still locks by itself: set Auto-Lock to Never (or longer) in the phone's settings for the show. The remote works while the window is locked. Its changes go into the undo history and the active scene, like any other change.

The remote settings belong to the computer (`~/Library/Application Support/RKFX/RKMix/remote.json`), not to the mix.

## Ganging, copy and paste

**Gang strips** to move them together: **Cmd-click** (or Shift-click) strip names. The first Cmd-click gangs the selected strip with the one you click; click again to take a strip out. Ganged strips have a bright frame. Moving one ganged fader by hand moves the others by the same travel, each from where it was, so their balance stays (one that reaches the bottom picks up again on the way back within the same move). Muting one mutes them all. Channels and buses can be ganged together; with **Sends on fader** the ganged faders move their sends. Press **Esc**, or right-click a strip and choose **Ungang all**, to end it. A gang belongs to the window: it isn't saved.

**Copy and paste a section:** right-click a section of the Channel page (Input, Delay, High-pass, Gate, EQ, Compressor) and choose **Copy Gate**, or **Copy whole strip** (every section and the inserts). On another channel, right-click and **Paste**. When that channel is ganged, the paste goes to every ganged channel: copy the vocal EQ once, paste it onto four backing vocals. A bus's, the main's and the mono sum's **EQ**, **Compressor** and **Limiter** copy and paste between each other the same way (right-click them on the Bus or Main page). The strip's right-click menu also has Copy whole strip and Paste. A paste never changes routing (pans, sends, keys, outputs), names or colours, and Undo takes it back in one step.


**Undo** and **Redo** (top right of the tabs, or Cmd+Z and Cmd+Shift+Z) take back the console's changes a step at a time: faders, mutes, pans, sends, every strip and bus setting, routing, DCAs and mute groups, and scene recalls. Changes less than half a second apart are one step, so a fader drag or a scene recall (with its glide) undoes in one go. **Undoing a scene recall** brings back the mix and the scene before it, and changes neither scene. A Gig Performer widget's moves are undone the same way. The buttons' tooltips say what they undo. Not undone: the inserts' own knobs, inserts added or removed, names and colours. Loading a mix starts a new history.

**Lock** (next to Undo) protects the console from a stray click during the show:

- **Faders and mutes only:** the faders (channels, buses, DCAs, main), Mute, S, the mute groups, Spill, the scenes, TALK and Clear solo still work. The top panel, pans, inserts, names, right-click menus and the solo setup don't.
- **Everything:** nothing can be changed; you can still switch pages to look.

Either way, Bypass and the preset menu are locked too, and Undo is off. **Hold Lock for a second** to unlock. The lock is for the window only (host parameters and the remote still work) and isn't saved: a reloaded session starts unlocked.


The **DCA** page (bottom row) shows the 8 DCAs and the 8 mute groups. Click a name for its members page: every input and bus, switched on or off. You can also right-click any strip and tick its **DCAs** or **Mute groups**. Double-click a name to rename it ("Drums", "Band").

- **DCA:** one fader that moves several faders at once. -6 dB on the DCA is -6 dB on every member, while their own faders stay where they are, so the balance between them is kept. The DCA's bottom silences its members, its **Mute** mutes them. A DCA sums nothing and adds no processing, and the in-ears don't change (pre-fader sends ignore it, as they ignore the channel's fader). A channel in two DCAs gets both. The total never goes past +10 dB.
- **Spill:** shows a DCA's members on the faders (up to 15), with the DCA at the right. Click Spill again, or any page button, to go back.
- **Mute group:** its big **MUTE** silences every member at once, everywhere: the main, groups, sends and the in-ears. "Band" for a speech between songs, "Vocal mics" during a playback. The members' own Mute buttons stay as they are, so when the group is off again everything plays as before.

DCA faders, DCA mutes and the mute groups are host parameters (map a Gig Performer button to **Mute Group 1**), and scenes recall them with **Faders & mutes**. Who is in which DCA and mute group is recalled with **Assigns & DCAs**.

## User pages

**User 1** and **User 2** show up to 16 strips you choose, channels, pairs and buses mixed, in the order you add them: for example the vocals next to their in-ear buses, or just what you touch during a song.

- **Right-click a User button** to choose: the list shows all inputs and buses (ticked = on the page) and opens again after each click, so you can tick several in a row; click outside it when done. **Clear** empties the page.
- **Right-click any strip** for **On User 1** and **On User 2**.
- Clicking an empty user page opens the list straight away.

The pages are saved with the mix.

## Scenes

The scene bar (bottom row) holds **16 scenes**. Click a number to recall it; right-click to copy one scene to another. Changes you make go into the active scene.

- A scene holds the **mix**: faders, mutes, pans, sends, every strip value and section switch, inserts on or off, and insert knobs you mark **Scene controlled** (right-click the knob in the insert's window; up to 64).
- A scene never changes the **structure**: links, bus types and formats, outputs, taps, which effect sits in which slot, names, user pages, talkback and oscillator setup.
- **Glide** (Scenes page, 0 to 5 s, default 100 ms): faders and knobs glide to the new scene; switches change at once.
- **Recall safe** (Scenes page) per channel and bus: scenes leave it alone (a talkback mic, a channel you are sound-checking).
- **Recall scope** (Scenes page, for each scene): what recalling that scene changes. Switch on any of **Faders & mutes**, **Pans**, **Channel processing** (trim, polarity, delay, high-pass, gate, EQ, compressor), **Bus & main processing** (their EQ, compressor, limiter), **Inserts** (on or off, and the scene-controlled knobs), **Assigns & DCAs** (group assigns, Main switches, DCA and mute group members), and the **Sends to** each bus (**B1** to **B16**: the send levels to that bus, and their pans in an Own bus). **Everything** is the default. For example, scenes that only change the singer's in-ear between songs: switch everything off except **Sends to** the singer's bus. A scene still stores everything, so widening its scope later loses nothing. Pick the scene in the list (it follows the active scene), or right-click a scene button and choose **Recall scope...**. **Copy to all scenes** gives every scene the same scope; copying a scene copies its scope too. A scene that recalls only part of the mix has a dot under its button, and its tooltip says what it recalls.
- Recall safe wins over the scope: a recall-safe channel or bus is never changed.

## Assignable controls

**Right-click any control**, the console's or an insert's, and choose **Assign to** one of the 32 **Assign** host parameters. In Gig Performer, map widgets, expression pedals and footswitches to them. Moving the control moves the parameter too. The **Assign** page lists them with **Min** and **Max** (the part of the control's travel used) and **Inv** (upside down, for an expression pedal).

## Standalone app

- **Audio/MIDI settings:** the audio interface, MIDI input and MIDI output devices.
- **Tempo** (master section): type it, **Tap**, or **Clock** to follow incoming MIDI clock. Tempo-synced inserts (delays, tremolos) follow it.
- **MIDI learn:** the **MIDI** button on each Assign row; right-click a scene button, TALK or Tap to learn a controller for it. Learned controllers belong to your rig, the same for every mix.
- **Program changes** 0-15 recall scenes 1-16.

## Starting points

These mixes are built in as **factory presets**: open the preset menu and choose **Factory**. Load one, change it to your band and use **Save as** to keep your version.

| Factory mix | What it sets up |
| --- | --- |
| **Duo** | Vocal 1 and 2 (high-pass 100 Hz, compressor -18 dB 3:1), Guitar (high-pass 80 Hz), Keys on 5/6 as a stereo pair; an FX bus "Reverb" (RKVerb at 100%) with sends from all four; the main limiter on |
| **Band with In-Ears** | Lead and backing vocals, guitar, bass, keys (5/6), kick, snare, overheads (9/10); a "Drums" group, a "Reverb" FX bus, and four stereo in-ear mixes (IEM Vocals, Guitar, Bass, Keys) on Out 3/4 to 9/10, each with its player up front and a little reverb (IEM Vocals has its own pans: guitar left, keys and backing vocal right); User 1 holds the vocals and the in-ear buses |
| **Recording Feed** | Inputs 1-16 straight to Out 17-32 (direct outs at Input) for a multitrack recorder, alongside the main on Out 1/2 |
| **Mono Fill** | The mono sum on Out 3 for a front fill or a centre speaker (high-pass 80 Hz, limiter), the main with a 30 Hz high-pass and limiter |

## Presets

A preset is a **whole console**: every setting, the inserts with their own settings, names, colours, scenes, assignments and user pages (not solo). Presets live in `Presets/RKMix` in the RKFX preset library, strip presets in `Presets/RKMix Channels`. A gig saved in Gig Performer keeps the whole mix in the gig file and never depends on a preset file.

## Host parameters

| Parameters | What they do |
| --- | --- |
| **Ch 1 Fader** … **Ch 32 Fader**, **Ch 1 Mute** … | Channel faders and mutes (a linked pair uses its left channel's) |
| **Bus 1 Fader** … **Bus 16 Fader**, **Bus 1 Mute** … | Bus faders and mutes |
| **Main Fader**, **Main Mute**, **Mono Fader**, **Mono Mute** | The main and the mono sum |
| **Assign 1** … **Assign 32** | See Assignable controls |
| **Scene** | 1 to 16 |
| **Talk** | Talkback on or off |
| **DCA 1 Fader** … **DCA 8 Fader**, **DCA 1 Mute** …, **Mute Group 1** … **Mute Group 8** | The DCAs and mute groups |

Everything else is in the mix itself. A scene that moves a fader moves its host parameter, so Gig Performer widgets follow.

## Notes

- **Bypass** silences every output (a console has no "dry" signal).
- RKMix adds no latency of its own. An insert that has some (RKPolyPitch, RKLimit's lookahead) delays its path, and RKMix lines up every other path and output with it, so everything stays in time.
- The bus and main **low-pass** filters go down to 1 kHz.
- Gig Performer tip: map footswitches to **Scene** (or use program changes in the standalone app), an expression pedal to an **Assign** driving an insert's volume or wah, and a button to **Talk**.
