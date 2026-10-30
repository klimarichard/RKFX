# RKMix: design

**Mixing console: 32 inputs, 16 flexible buses, a stereo main and a mono sum, 32 outputs** · Utility family · zero latency unless an insert adds some

RK + a verb, as every RKFX plugin: to *mix*. This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKMix is a small digital mixing console in one plugin, mainly for Gig Performer, with a standalone app for use straight on an audio interface. Every cable goes straight in and out, **In 1 → RKMix input 1** … **RKMix output 32 → Out 32**. The mix happens inside: channel strips, group buses, monitor mixes, effect buses and the main mix.

- **32 input channels**, linkable in pairs (1/2 … 31/32) into stereo channels.
- **A fixed strip on every channel:** trim, polarity, delay, high-pass, gate, 4-band EQ, compressor. Each section can be switched off, and then costs nothing.
- **2 insert slots per channel** that hold real RKFX plugins (as RKBoard's pedals), **4 per bus**, 4 on the main, 2 on the mono sum.
- **16 flexible buses**, each a **Group**, an **Aux** (monitor mix) or an **FX** bus, mono or stereo.
- **Main stereo L/R plus a mono sum**, each on any of the 32 outputs.
- **Solo** either to a cue output (PFL/AFL, the main mix is never touched) or in place, selectable.
- **16 scenes** with a glide, **32 assignables**, and host parameters for every channel and bus fader and mute.
- **MIDI out from audio-to-MIDI inserts:** RKTrigger and RKFollow in a channel's insert slots send their MIDI out of RKMix (a kick mic triggering a drum sampler), with a warning when two of them can send the same note on the same channel.
- **Latency is aligned inside:** when an insert adds latency, the other paths are delayed to match at every point where signals meet.

RKMix with nothing switched on (sections off, faders at 0 dB, centred, all channels to the main) is a plain summing mixer.

## Signal flow

```
input n ─► Trim ─► Polarity ─► Delay ─► HPF ─► Gate ─► EQ ─► Comp ─► Insert 1 ─► Insert 2 ─┬─► Mute ─► Fader ─► Pan ─┬─► Main L/R
                                                                                           │                         ├─► Group buses (assigned, post-fader)
                                                                                           ├─► Aux sends ────────────┼─► Aux buses (send level, tap point per send)
                                                                                           └─► PFL                   ├─► FX buses (send level, always post)
                                                                                                                     └─► AFL

Group bus ─► Insert 1-4 ─► Mute ─► Fader ─► Pan ─┬─► Main L/R (switch)
                                                 ├─► a later Group (assigned)
                                                 ├─► Aux and FX buses (sends)
                                                 └─► its own output(s)
FX bus    ─► Insert 1-4 ─► Mute ─► Fader ─► Pan ─┬─► Main L/R (switch, on by default)
                                                 ├─► Aux buses (sends: reverb in the in-ears)
                                                 └─► its own output(s)
Aux bus   ─► Insert 1-4 ─► Mute ─► Fader ──────────► its own output(s)
Main L/R  ─► Insert 1-4 ─► Mute ─► Fader ─► Balance ─► main output pair
                                    └─► Mono sum (L+R)/2 ─► Insert 1-2 ─► Mute ─► Fader ─► mono output
```

The drawing shows Aux sends at their default, Pre-Fader. Each send can tap the strip earlier or later instead (see Send tap points).

**Processing order** is fixed: channels, then Group buses in ascending number, then FX buses, then Aux buses, then the main and the mono sum. A Group can feed only a **higher-numbered** Group, so the routing can never loop. FX and Aux buses never feed Groups, and Aux buses never feed the main.

**Mute** silences the channel's fader path *and* its pre-fader sends. On a live console a muted channel must be silent in the in-ears too.

## Inputs and channels

- **Link** (between each pair): a linked pair is one stereo channel with one set of controls, those of its left (odd) input. The right input's own settings are kept but unused while linked, as in RKRemap. A pair has **one name**.
- **Per channel:** a name, a colour (one of 8 strip colours), and a **signal light**.
- **Channels shown** (1 to 32, default 16) only changes the view. A hidden channel that reaches the main or a bus is counted in a note, as RKRemap's hidden-routes note does.

## The fixed strip

Every section has an **On** switch. An off section is skipped entirely (bit-exact, no CPU). Switching a section on or off crossfades over **20 ms**.

| Section | Controls | Range | Default |
| --- | --- | --- | --- |
| Trim | Gain | −24 to +24 dB (snaps to exactly 0 dB) | 0 dB |
| Polarity | Ø (a pair: left and right separately) | normal, inverted | normal |
| Delay | Time | 0 to 20 ms (0.01 ms steps) | 0, off |
| HPF | Frequency, Slope | 20–400 Hz; 12 or 24 dB/oct | 80 Hz, 12, off |
| Gate | Threshold, Range, Attack, Hold, Release, Key | −80 to 0 dB; 0 to −80 dB; 0.1–50 ms; 0–500 ms; 5–2000 ms; Self or input *n* | −50 dB, −80, 1 ms, 50 ms, 150 ms, Self, off |
| EQ | Low (shelf or bell), Low Mid (bell), High Mid (bell), High (shelf or bell): Frequency, Gain, Q each | 20 Hz–20 kHz; ±15 dB; Q 0.3–10 | 100 / 250 / 2.5k / 8k Hz, 0 dB, off |
| Comp | Threshold, Ratio, Knee, Attack, Release, Makeup, Key | −60 to 0 dB; 1:1–20:1; 0–12 dB; 0.1–100 ms; 10–2000 ms; 0–+24 dB; Self or input *n* | −20 dB, 3:1, 6 dB, 10 ms, 150 ms, 0 dB, Self, off |

- **The DSP is the suite's own:** the HPF from `core/dsp/StateVariableFilter.h` / `FilterCascade.h`, the EQ bands from `MatchedBiquad.h` (as RKParaEQ), the gate and compressor from `core/dsp/Dynamics.h` (the engines RKGate and RKCompress use). It is reused at the level of the DSP classes, not as whole plugins, so a channel strip costs a few filters and two detectors.
- **Key** feeds the gate's or the compressor's detector from another input (after that input's Trim), for a kick-keyed bass gate or music ducking under a voice. Because the key is taken from the raw trimmed input, it doesn't depend on processing order.
- **A stereo channel** runs one detector on the louder side, so both sides gate and compress together.
- **EQ coefficients** are computed only when a value changes (a scene glide recomputes them every 32 samples while it runs), never per block for steady settings. This is RKRemap's lesson.

## Fader, pan, routing

| Control | Range | Default |
| --- | --- | --- |
| Fader | −∞ to +10 dB (the suite's fader law, `core/gui/Fader.h`) | 0 dB |
| Mute | off, on | off |
| Pan (mono channel) / Balance (stereo channel) | L100 – C – R100 | C |
| Main | off, on (assign to Main L/R) | on |
| Group *k* | off, on, per Group bus | off |
| Send *k* | −∞ to +10 dB, per Aux and FX bus | −∞ |
| Send tap | per send (channel × Aux bus): Bus default, Input, Pre-EQ, Pre-Comp, Pre-Inserts, Pre-Fader, Post-Fader | Bus default |

- **Pan law:** a mono channel panned centre into a stereo bus is **−3 dB per side** (constant power). A stereo channel's Balance turns one side down and leaves the other at unity.
- **Into a mono bus,** a mono channel goes as it is and a stereo channel as (L + R) / 2.
- **Stereo sends** to a stereo Aux follow the channel's pan (**Pan follows**, a switch per Aux bus, default on, so an in-ear mix keeps the stage picture).
- **Sends at −∞ are skipped** (no multiply-add).

### Send tap points

Every send to an **Aux bus** taps the channel at its own point. The bus sets the default and each send can override it. For example, the singer's in-ear (Bus 1) gets the vocal **Pre-Comp** (natural dynamics in the ear), while every other Aux gets it at the bus default, Pre-Fader (after the compressor).

| Tap | Taken after | Before |
| --- | --- | --- |
| Input | Trim, Polarity, Delay | HPF |
| Pre-EQ | HPF, Gate | EQ |
| Pre-Comp | EQ | Comp |
| Pre-Inserts | Comp | Insert 1 |
| Pre-Fader | Inserts | Mute, Fader |
| Post-Fader | Fader, Pan | — |

- **Mute applies at every tap:** a muted channel is silent in every send, whatever its tap point.
- **Pan follows** (stereo Aux) applies the channel's pan at every tap, not only post-fader.
- **The bus's default** (Pre-Fader or Post-Fader in the bus settings, or any tap) is used by every send left at *Bus default*. A send with its own tap shows a small tap label next to its knob, so overrides are visible.
- **FX sends** stay Post-Fader (no tap choice); Group assigns are always post-fader.
- **Cost:** the strip keeps a copy of the signal only at the tap points some send actually uses (a copy per used tap per block). A strip whose sends all use one tap costs what it did before.
- **Latency:** a tap before the inserts has less latency than one after them. The alignment at each bus (see Latency) delays it to match, so a pre-insert send stays in time with the rest of the in-ear mix.
- **Tap points are structure, not scene values** (see Scenes): they can change latency alignment, so they are set up once, not switched per song.

## Buses

Each of the 16 buses has:

| Setting | Values | Default |
| --- | --- | --- |
| Type | Group, Aux, FX | Aux |
| Format | Mono, Stereo | Stereo |
| Name, colour | text, 8 colours | "Bus *k*" |
| Output | Off, Out *n*, or Out *n*/*n+1* (stereo) | Off |
| Main | off, on (Group and FX only) | Group off, FX on |
| Default tap (Aux only) | any send tap point (see Send tap points) | Pre-Fader |
| Pan follows (stereo Aux only) | off, on | on |

and like a channel: **4 insert slots**, Mute, Fader, Pan/Balance (into the main), and sends to Aux and FX buses (Group and FX buses). A Group bus also has assign switches to **higher-numbered** Groups.

- **Group:** fed by the channels assigned to it, post-fader and post-pan at unity (no send knob). Use it for "Drums" or "Backing vocals" with one fader and shared compression.
- **Aux:** fed by per-channel send levels, each at its own tap point (the bus's default unless overridden). Never feeds the main. Use it for monitor and in-ear mixes, a recorder feed or a broadcast feed.
- **FX:** fed by per-channel send levels, always post-fader. Returns into the main (and can be sent on to Aux buses, so the singer hears the reverb). Its inserts hold the effect (RKVerb, RKDelay), usually at 100% wet.
- **Changing a bus's type or format** doesn't erase the sends or assigns that the new type doesn't use. They are kept and come back if the type is changed back. The window shows the change as a structural one (see Scenes).

### Sends on fader

Selecting an **Aux or FX bus** in the mixer view switches every channel's fader to show its **send to that bus** (the fader caps turn the bus's colour). This is how monitor mixes are set on a digital console. Pressing the bus again returns to the channel faders.

## Main and mono

- **Main L/R:** 4 inserts, Mute, Fader, Balance, output pair (default Out 1/2).
- **Mono sum:** (L + R) / 2 of the main after its inserts and fader, then 2 inserts (an RKFilter low-pass for a sub, for example), Mute, Fader, output (default Off).

## Outputs

The 32 outputs are fed by the main pair, the mono sum, the buses' outputs, the channels' direct outs and the cue output. **Several sources on one output add up**, and the routing page marks that output in the accent colour with a note ("Out 3: Bus 1 + Bus 4"). An output nothing feeds is silent. Outputs have names (as in RKRemap) for the routing page.

### Direct outs (added 2026-10-30, feat/rkmix-directouts)

- **Every channel can also go straight to an output** (a linked pair to an output pair), as well as everywhere else it goes: for a multitrack recorder, a broadcast feed, or an in-ear that needs one source alone.
- **Tap point:** any send tap (Input, Pre-EQ = after the gate, Pre-Comp = after the EQ, Pre-Inserts = after the compressor, Pre-Fader = after the inserts, Post-Fader). Default Input.
- **Never panned.** Before Post-Fader it **ignores mute and solo** (a recorder keeps recording when the channel is muted in the room); Post-Fader follows the fader and mute (in-place solo included).
- **Latency:** lined up with every output (delayed to the total), whatever the tap.
- **Structure, not scene:** the output and tap don't change with a scene.
- **Settings** `ch<n>_direct_out` (an output target) and `ch<n>_direct_tap`, appended. Linking or unlinking a pair moves its direct out between one output and a pair (as a bus's format does).
- **Window:** the channel's Sends page header, "Direct out [Out 9] at [Pre-EQ]"; the Routing page lists "<channel> direct" on its output.

## Solo

A global **Solo mode**:

- **Cue** (default, safe live): solo sends the soloed channels and buses to the **cue output** (any output pair, default Off), and the main mix is never touched. A global **PFL/AFL** switch picks the tap: **PFL** is after the inserts and before mute and fader, **AFL** is after the fader and pan. With nothing soloed, the cue output carries a chosen **cue source**: the main, any Aux bus, or silence.
- **In place** (studio): soloing mutes everything not soloed in the main and in every bus (it acts like a mute everywhere else). Each channel and bus has a **Solo safe** switch (on by default for FX buses, so reverb stays). While in-place solo is active, the window's solo indicator flashes.

Solo is never saved (in presets, scenes or the session) and clears when a mix is loaded. A solo change fades over 10 ms.

### As built (feat/rkmix-solo, 2026-10-30)

- **Solo buttons** (S) sit next to Mute on every channel and bus strip; the **Master section** (top right) holds Cue | In place, PFL | AFL, the cue output, the cue source with nothing soloed (Silence, Main, any bus), Clear solo (lit while anything is soloed, flashing while soloing in place), TALK and OSC.
- **In place, soloing only buses:** the channels still feed buses (the soloed group hears its sources) but don't reach the main straight; soloing channels mutes the other channels everywhere. Solo safe: a channel's switch on the channel page (Output); a bus's Auto / On / Off in its page's header (Auto = on for FX buses).
- **Cue alignment** with latent inserts: channel solos and bus AFL arrive with every other output; a bus's PFL comes at the bus's own latency (a few ms early at most).
- **Talkback** goes only into **Aux** buses (switches per bus, all on by default; the others' switches are dimmed), and the main when switched; its Latch setting makes TALK a switch instead of held. **The oscillator's pink noise** is Paul Kellet's refined filter (not Voss-McCartney), at the same RMS as the sine; tested to have equal power in constant-Q bands from 100 Hz to 6.4 kHz (-3 dB/octave) within 1 dB.
- **Talk / Osc page** (a fourth tab): talkback source, level, latch, destinations; oscillator on, sine or pink, frequency, level, destinations, straight-to output.
- **Never saved:** solo (cleared on load); Talk (host parameter) and OSC are forced off when a mix loads (their setup is kept).
- **Tests:** PFL past fader and mute with the main untouched, AFL, the cue source, in place with solo safe, soloing a group in place, talkback to Aux not main and high-passed, the sine's level, pink noise's slope, solo and talk/osc not saved, solo through the processor.

## Talkback

- **Talk source:** any input (a dedicated talkback mic, usually not on a channel's fader), after its Trim, through a fixed 100 Hz high-pass. Default Off.
- **Destinations:** any of the 16 buses and the main, chosen by switches (default: every Aux bus, not the main). Talkback is added at the bus's input, so it passes the bus's inserts and fader like the rest of the mix.
- **Level** −∞ to +10 dB (default 0 dB), and **Talk** button: momentary (hold) or latch (click), a setting.
- **Talk** is a host parameter, an assignable target and MIDI-learnable, so a Gig Performer widget or a footswitch can work it. It fades in and out over 10 ms.
- **Never saved on:** Talk is always off when a mix loads.

## Oscillator

- **Signal:** Sine (20 Hz–20 kHz, default 1 kHz) or **Pink noise** (−3 dB/octave, the usual Voss-McCartney generator).
- **Level** −60 to 0 dBFS, default −18 dBFS. Mono signal, the same on both sides of a stereo destination.
- **Destinations:** any buses, the main, or one output or output pair directly (to check a speaker or a cable), chosen by switches. Default: none.
- **On** switch, fading over 10 ms. Not a host parameter, but an assignable target.
- **Never saved on:** the oscillator is always off when a mix loads, and the master section shows a lit **OSC** indicator while it runs.

## Inserts

- **What fits:** any RKBoard pedal (stereo RKFX audio plugins and the board's Volume and Wah), hosted the same way RKBoard does: the plugin's own `ProcessorBase` instance and its own editor, with its presets and factory presets, and the sidechain bus disabled. MIDI-input plugins (RKVoice, RKVocode, RKSynth) receive the mix's MIDI input.
- **Per slot:** plugin (or empty), On (bypass), and an **Edit** button opening the plugin's editor in the top panel (see The window).
- **A mono channel or mono bus** runs its inserts in mono. Effects whose point is stereo (ping-pong delay, chorus spread) belong on a stereo FX bus; the slot menu marks them "stereo effect".
- **Insert on/off** fades over 20 ms (the plugin is fed silence and idles while off, as RKBoard's bypass does).

## MIDI out

RKMix has a **MIDI output**. It carries what its **audio-to-MIDI inserts** send: **RKTrigger** (a kick or snare mic triggering a drum sampler) and **RKFollow** (a voice or violin playing a synth). Both pass their audio through unchanged, so they sit in any channel's or bus's insert slots like any other effect.

- **Channel and notes come from the plugin itself:** RKTrigger's channel and note per trigger, RKFollow's channel. RKMix doesn't add a second Channel Out setting. The slot shows what the plugin sends ("MIDI → Ch 10 · C1 (36), D1 (38)") and clicking it opens the plugin's editor.
- **Merging:** after each insert runs, RKMix collects the MIDI it sent (into a buffer allocated in `prepare()`) and merges it into the output at the same sample positions. Sources are independent; RKMix doesn't filter or change their messages.
- **Conflict warning:** two sources **conflict** when they can send the **same note, or the same controller, on the same channel**, or when one sends a **channel-wide message** (pitch bend, channel pressure) on a channel the other uses: a bend would bend the other source's notes too. Kick on C1 (36) and snare on D1 (38), both on channel 10, is fine; two triggers on channel 10, note 36, conflict; RKFollow can send any note (and bends), so it conflicts with any other source on its channel, and in **MPE** mode with any source on any channel. Each conflicting slot shows a warning mark, and the routing page lists the conflicts ("Ch 10, C1 (36): Kick › Trigger, Kick Sub › Trigger"). It only warns, never blocks: doubling a note can be intended.
- **What each plugin can send** comes from a new core hook, `ProcessorBase::getMidiOutputFootprint()`: per channel, the notes and controllers the plugin can currently send (a 128-bit note mask and a 128-bit controller mask) and whether it sends channel-wide messages (pitch bend, channel pressure). RKTrigger reports its triggers' channels and notes. RKFollow reports every note, its expression controller (or pressure) and pitch bend when Bend is on, plus the RPN controllers (100, 101, 6) it uses to set the bend range, on its channel; in MPE mode, on all 16 channels. The footprint is re-read on the message thread when an insert's parameters change.
- **No hung notes:** switching a MIDI insert off (by hand, by a scene or by an assignable), removing it, changing its channel or note, or loading a mix first sends note-offs for what it started (the core's `releaseMidi`).
- **Mute and solo don't stop MIDI:** inserts come before Mute, so a muted kick channel still triggers its sample, which is usually the point (the kick mic is off in the PA). To stop a trigger, switch its insert off.
- **Timing:** MIDI is sent as soon as the insert produces it, never delayed by latency alignment, for the fastest triggering.
- **RKMix bypassed:** silence on every output and no MIDI out (note-offs first).
- **MIDI in** reaches the MIDI-input inserts (RKVoice, RKVocode, RKSynth) but is **not passed to the output** unless the **MIDI thru** setting is on (default off), so Gig Performer wiring doesn't double notes.
- **Scenes** can switch MIDI inserts on and off, but their channel and note parameters can't be scene-controlled (a choice must never glide through values).
- **Pure MIDI effects** (RKHarmonize, RKArpeggiate, RKStrum, RKSustain, RKEcho, RKConduct) have no audio and aren't hosted in v1. They stay as their own plugins in Gig Performer, after RKMix's MIDI output.
- **RKBoard** hides RKTrigger and RKFollow in its pedal menu (the board has no MIDI output): the registry marks them `sendsMidi`.
- **Standalone:** a MIDI output device, chosen in the audio/MIDI settings.

### As built (feat/rkmix-midi, 2026-10-30)

- RKMix is built with MIDI out (`NEEDS_MIDI_OUTPUT`, `PluginConfig::midiOutput`); still a music effect (`aumf`).
- RKTrigger and RKFollow joined the pedal host (`RKFX_HOST_MIDI_PLUGINS`; the MIDI plugins are now configured before the host) in a **MIDI** family of the insert menu, flagged `sendsMidi`. RKBoard hides them and refuses to add them (its tests check it).
- **Core:** `rkfx::MidiFootprint` (`core/plugin/MidiFootprint.h`) and `ProcessorBase::getMidiOutputFootprint()`. RKTrigger reports trigger 1 (Mono) or both (Dual): channel and note. RKFollow reports every note on its channel (if Notes is on), its expression controller (or pressure: channel-wide), pitch bend (channel-wide) and the RPN controllers 100, 101, 6 that set its bend range, on every channel in MPE mode.
- The rack tracks each MIDI insert's sounding notes; leaving play (removed, prepared again for mono/stereo, a mix loaded) or a channel/note change sends note-offs for them; RKMix's bypass ends them all (`releaseMidi`).
- **Window:** a MIDI insert's slot shows a MIDI tag, in the warning colour while it clashes; the insert view's header says what it sends and what it clashes with; the routing page has a **MIDI out** box: MIDI thru, the MIDI inserts with what they send, the clashes.
- **Tests:** a hit on input 1 through an RKTrigger insert leaves RKMix as a note-on (Ch 10, C1) at the hit; two triggers on one note conflict, on two notes they don't, RKFollow on their channel clashes with both; removing a sounding trigger sends its note-off (checked to fail without); MIDI thru off and on. auval (Mix1, Brd1, Trg1, Flw1) and pluginval pass.
- **Still open:** Step 0 below (the user's check of MIDI out from an audio effect in Gig Performer), and the MIDI output device in the standalone app (phase 8).

### Step 0 for MIDI

RKTrigger's and RKFollow's MIDI output from an audio effect hasn't been checked in Gig Performer yet. Before the MIDI phase, the user loads **RKTrigger as AU and VST3** in Gig Performer and checks that its MIDI output can be wired to an instrument and triggers it. If one format can't send MIDI from an audio effect, the design is revisited for that format.

## Latency

- Most RKFX plugins have **zero latency**. A few report some (RKPolyPitch, RKLimit's lookahead), and the fixed strip has none (the channel Delay is the user's own alignment, not latency).
- **Path latency:** a channel's latency is the sum of its inserts' latencies, and a bus's latency is the latest of its inputs plus its own inserts. At every **summing point** (each bus, the main, the cue output) each input is delayed so it arrives with the latest one. The Pre and Post taps are aligned too, so a pre-fader monitor mix doesn't smear.
- **RKMix reports** the main path's latency to the host (as RKBoard does with `setLatencySamples`). Aux and cue outputs are aligned to the same total, so every output is in time with every other.
- **Latency only changes with the structure** (adding, removing or changing an insert), never with a scene. With no latent insert anywhere, RKMix has no latency and no alignment buffers run.
- Alignment buffers are allocated in `prepare()` for the largest latency the slots can report, and are skipped when a path's delay is 0.

## Scenes and assignables

- **16 scenes**, like RKBoard's. A scene holds the **mix values**: every strip value and section switch, faders, mutes, pans, sends, group and main assigns, insert on/off, and the insert knobs marked **Scene controlled** (right-click, up to 64, as RKBoard). It does **not** hold the **structure**: input links, bus types and formats, send tap points, outputs, which plugin sits in which slot, the talkback and oscillator setup, names and colours. So a scene change never changes latency or routing topology.
- **Glide** (per mix, 0 to 5 s, default 100 ms): continuous values glide, switches and choices jump at the start (sections on/off and mutes crossfade over their usual 20 ms).
- **Recall safe** per channel and bus: excluded from scene recall (a talkback mic, a channel being sound-checked).
- **32 assignables** as RKBoard's: any control in the mix or in an insert, with min/max and invert, set by right-clicking a control ("Assign to…"). Two-way: moving the control moves the host parameter.
- **Standalone:** MIDI learn (RKBoard's `MidiMap`, its own file under Application Support/RKFX/RKMix/), program change 0–15 selects scenes, and tempo/tap/MIDI clock for tempo-synced inserts (RKBoard's `BoardPlayHead`).

### As built (feat/rkmix-scenes, 2026-10-30)

- **What a scene holds** is decided by parameter ID (`MixScenes::isSceneValue`): every fader and mute, and every mix setting except links, bus types and formats, outputs, tap points, keys, pan modes, solo and solo-safe settings, recall safe, talkback, oscillator and MIDI settings. Plus each insert's on/off and up to 64 scene-controlled insert knobs.
- **Edits go into the active scene** because the current scene is stored as it is whenever another is recalled (and when the session is saved); a scene never recalled starts as the mix was when it was first left.
- **Glide** runs on the message thread (60 steps a second, the engine's 20 ms smoothing in between); switches and choices change at once. The Scene host parameter recalls (from any thread, done on the message thread).
- **Recall safe** per channel and bus is on the Scenes page; it covers the strip's own values, faders, sends and inserts.
- **The scene bar** is the bottom row (click: recall, right-click: copy); **right-click any control** (the console's or an insert's) for Assign to / Remove / Scene controlled; the **Assign page** lists the 32 with min, max (percent of the control's travel), Inv and remove. Removing an insert removes its assignments and scene knobs.
- **Tests:** a scene's fader, pan, insert on/off and a scene-controlled insert knob recall; a link (structure) stays; a recall-safe channel keeps its fader; edits stay in their scene; the session keeps scenes and the current one; the Scene parameter recalls. An Assign moves a fader (range, invert), a pan, a mute (on at 50%), an insert knob; the fader moves its Assign; kept in the session; gone with its insert.

### As built: the standalone app (feat/rkmix-standalone, 2026-10-30)

- **RKMix.app** (Standalone format; the RKMusic icon "MIX" in the steel accent, from tools/icons). The MIDI output device is the standalone wrapper's (MIDI out from RKTrigger and RKFollow inserts, and MIDI thru).
- **Shared with RKBoard** through the pedal host: `rkfx::MidiMap` (the map; each app names its targets) and `host/Tempo.h` (tap tempo, MIDI clock follower, the standalone play head).
- **MIDI learn** targets: Assign 1-32 (an expression pedal, glided over about 30 ms), Scene (the controller's value picks it), Tap, Talk (momentary or toggle). Learn from the Assign page's MIDI button per row, the scene bar's, TALK's and Tap's right-click menus; the mappings are the rig's (Application Support/RKFX/RKMix/midi-map.json), the same for every mix.
- **Program changes 0-15** recall scenes 1-16 (standalone only; in a host, Gig Performer routes program changes).
- **Tempo** (saved with the mix): the field, Tap and Clock (follow MIDI clock, kept in the rig's settings.json) in the Master section; the inserts' play head in the standalone app.
- **Tests:** program change to scene, a learned CC gliding an Assign, a momentary Talk switch, the rig's mappings shared between instances, the tempo kept with the mix.

## Parameters

Fixed host parameters, in this order (IDs permanent, append only):

| IDs | Count | What |
| --- | --- | --- |
| `ch_fader_1` … `ch_fader_32` | 32 | channel faders (a linked pair uses its left channel's) |
| `ch_mute_1` … `ch_mute_32` | 32 | channel mutes |
| `bus_fader_1` … `bus_fader_16` | 16 | bus faders |
| `bus_mute_1` … `bus_mute_16` | 16 | bus mutes |
| `main_fader`, `main_mute`, `mono_fader`, `mono_mute` | 4 | main and mono |
| `assign1` … `assign32` | 32 | assignables |
| `scene` | 1 | scene 1–16 |
| `talk` | 1 | talkback on/off |

134 in all, plus the base's Bypass. Everything else (about 1,000 values: strips, sends, buses, routing) lives in the mix state, not in host parameters, as RKBoard's board does. A scene recalling a fader moves its host parameter, so Gig Performer widgets follow.

No Dry/Wet and no Output knob (`hasMix` and `hasOutput` off).

## State and presets

- **The mix** (everything above but solo) is RKMix's extra state, JSON, with the inserts' states embedded as RKBoard embeds its pedals' (`getPresetVar`/`loadPresetVar`), and `PluginConfig::extraInPresets` on, so a **preset is a whole console**.
- **Channel presets:** a channel strip (fixed strip, inserts, name, colour; not its routing) can be saved and loaded on its own from the channel view ("Violin pickup", "Lead vocal"), in `Presets/RKMix/Channels`.

## The window

About **1280 × 880 px**, scaling as usual, in three areas:

- **Top panel (about 400 px):** the **selected channel or bus** in full: trim/polarity/delay, HPF, gate (with gain-reduction meter), EQ with its curve (`core/gui/ResponseCurve.h`), comp (transfer curve and gain reduction), inserts, and its routing (main, groups, sends as a row of small knobs). The panel shows an insert's own editor instead when its **Edit** button is pressed (scaled to fit, as on RKBoard), or the **Routing page** (bus setup table: type, format, name, colour, output, main; then the outputs map) or the **Scenes/Assign page**.
- **Fader area:** **16 strips per layer**. Layer buttons: **Inputs 1–16**, **Inputs 17–32**, **Buses**. A linked pair takes one strip. A strip shows the name and colour, a small EQ curve and gain-reduction lights, the two insert names, Solo, Mute, the Pan knob, the fader with its meter, and clicking its name selects it into the top panel.
- **Master section (right, always visible):** main and mono faders with meters, the cue output's level, the Solo mode/PFL-AFL switches with the solo indicator (and Clear solo), and the scene bar.

Meters (peak plus a hold, −60 to 0 dBFS) are measured **only while a window is open** and only for the strips on screen, at about 15 fps. This is RKRemap's other CPU lesson.

## Identity

- **AU code** `Mix1`, bundle ID `com.rkmusic.rkmix`, a music effect (`aumf`) with MIDI in, as RKBoard (inserts with MIDI input, program change for scenes), and MIDI out (`PluginConfig::midiOutput`, see MIDI out). AU, VST3, Standalone.
- **Utility family**, accent proposal **steel `#6B8CA6`** (not used by another plugin).
- **One bus of 32 discrete channels** each way (`PluginConfig::channels = 32`). RKRemap's Step 0 confirmed in Gig Performer that one 64-channel bus works as AU and VST3, so no new step 0 is needed.
- **Not an RKBoard pedal**: the board's chain is stereo. RKBoard can't be an insert either.
- **Host bypass = silence** on every output (unlike RKRemap's straight-through: raw inputs on the PA and in-ears would be worse than nothing). It fades out and in over 10 ms. **Core change:** a `PluginConfig` option for bypass to mute rather than pass the dry signal (e.g. `bypassMutes`).

## Architecture

- **Message thread owns the mix;** the audio thread reads an immutable **render plan** (per channel: active sections and coefficients; per summing point: the list of sources with gains and alignment delays), rebuilt only when something changes and swapped atomically. Steady state reads no parameters and allocates nothing.
- **Inserts** are owned and swapped as on RKBoard. Lesson from RKBoard: the chain lock is a non-reentrant `SpinLock`, so never set an Assign parameter (its listener takes the lock) while holding it.
- **Gliding values** (fader, pan, send, trim, EQ during a scene glide) are smoothed per sample. A summing point whose sources are all steady is a plain multiply-add per source.
- **Code shared with RKBoard** (pedal hosting, assignables, scenes, MIDI learn, play head) moves to a shared library (e.g. `core/host/`) rather than being copied. This refactor is the first commit of phase 4 (inserts), phases 5, 7 and 8 build on it, and it must leave RKBoard's tests passing.

## Build phases

As RKBoard: one stacked branch per phase, the user tests in Gig Performer between phases.

1. **Console core** (`feat/rkmix`): 32 inputs with links, names and colours; Trim, Polarity, Delay, Mute, Fader, Pan; Main L/R and the mono sum with outputs; host parameters; mixer window with layers and meters; state and presets.
2. **Fixed strip** (`feat/rkmix-strip`): HPF, gate, EQ, comp, keys, the channel view, channel presets.
3. **Buses** (`feat/rkmix-buses`): 16 buses with types, formats, sends, sends on fader, groups to later groups, outputs, the routing page.
4. **Inserts** (`feat/rkmix-inserts`): the shared pedal-hosting refactor, insert slots, hosted editors, latency alignment.
5. **MIDI out** (`feat/rkmix-midi`, after MIDI's Step 0): MIDI output, RKTrigger and RKFollow as inserts, the merge, footprints and conflict warnings, note-offs, MIDI thru.
6. **Solo, talkback, oscillator** (`feat/rkmix-solo`): cue PFL/AFL, solo in place, solo safe, talkback, sine/pink-noise oscillator.
7. **Scenes and assignables** (`feat/rkmix-scenes`): scenes, glide, recall safe, scene-controlled insert knobs, assignables.
8. **Standalone** (`feat/rkmix-standalone`): MIDI learn, program change, tempo/tap/clock, MIDI output device.

Before merging: the manual (`docs/manuals/RKMix.md`), factory presets, the website entry, the README last.

## Factory presets

| Preset | Idea |
| --- | --- |
| Empty 16 | 16 mono channels to the main, nothing on: a starting point |
| Stereo Pairs 32 | 16 linked pairs to the main |
| Small Band | Named channels (kick, snare, overheads pair, bass, guitar, keys pair, 3 vocals), Group "Drums" and "Vocals", 4 stereo in-ear Auxes, FX "Reverb" (RKVerb) and "Delay" (RKDelay), useful strip starting points (HPF on vocals, gate on toms) |
| Solo Performer | Violin pickup, vocal mic, backing tracks pair, click; main to Out 1/2, an in-ear Aux with the click (not in the main), FX reverb |
| Recorder Feed | A band mix to Out 1/2 and a post-fader stereo Aux of the same mix to Out 31/32 for a recorder |

## CPU and memory

- **Budget:** 32 mono channels with every fixed section on, 16 stereo buses with 32 active sends, no inserts: **under 2% of a core** at 128-sample blocks, 48 kHz (measured with windows closed). A fixed strip is about 10 biquads and 2 envelope detectors per sample.
- **Inserts dominate:** each costs what the plugin costs alone (0.03–0.45% for RKFX plugins). Everything runs on the host's one thread for the plugin. The manual says so and suggests heavy effects on FX buses (one reverb on an FX bus, not 16 on channels).
- **Memory:** the 20 ms channel delays (32 × 20 ms), alignment buffers sized to the slots' maximum reported latency, the summing buffers (16 stereo buses, main, mono, cue), and the inserts' own.
- **Not in v1:** multithreaded processing of channels inside RKMix. The plan's order (channels, then buses) would allow it later.

## Tests

- **Summing:** unity routes bit-exact at default settings; channels to the main, pan law (−3 dB centre), balance, mono and stereo into mono buses; the mono sum is (L + R) / 2.
- **Fixed strip:** each section off is bit-exact; HPF/EQ responses match RKParaEQ's/RKFilter's; gate and comp match RKGate's/RKCompress's engines; keys from another input; a linked pair gates and compresses both sides together.
- **Buses:** Group assigns post-fader; every Aux tap point takes the signal at its place in the strip (e.g. Pre-Comp is uncompressed while the same channel's Pre-Fader send is compressed), per-send overrides beat the bus default, every tap follows mute, a pre-insert tap is latency-aligned with post-insert sends; FX always post; pan follows; groups feed only higher groups (the GUI and the state loader refuse otherwise); several sources on one output add up.
- **Solo:** cue PFL/AFL taps, cue source with nothing soloed, the main untouched in Cue mode; solo in place mutes the rest except solo-safe; solo never saved.
- **Talkback and oscillator:** reach only their chosen destinations; sine frequency and level; pink noise slope (−3 dB/octave within ±1 dB, 50 Hz–10 kHz); both off after loading a mix saved with them on.
- **Bypass:** every output silent, with a fade, and no MIDI out.
- **MIDI out:** an RKTrigger insert's notes leave RKMix at their sample positions, merged with a second source's; conflicts found for the same channel and note (and controller), none for different notes on one channel, RKFollow conflicting with anything on its channel (on every channel in MPE mode), a pitch-bending source conflicting with a trigger on its channel; note-offs when a MIDI insert is switched off, removed, re-channelled or the mix is loaded or bypassed; a muted channel still sends MIDI; MIDI in reaches the output only with MIDI thru; RKBoard's menu hides `sendsMidi` plugins.
- **Latency:** a latent insert on one channel: every summing point aligned (impulse on two channels arrives together at the main, in an Aux pre and post, and at the cue output); reported latency; zero latency with no latent insert.
- **No clicks:** fader, mute, section switches, insert on/off, solo and scene changes on a sustained tone produce no step beyond the signal's own.
- **Scenes:** recall restores values not structure; recall safe; glide; host fader parameters follow a scene.
- **State:** session, presets, channel presets, factory presets, Reset; the right input of a linked pair keeps its settings.
- **CPU:** the budget above, in the benchmark.
- **Validation:** auval (`aumf Mix1 RKMu`), pluginval at strictness 10, RKBoardTests still pass after the shared-hosting refactor.

## As built: phases 1-4 (2026-10-30)

Branches, stacked: `docs/rkmix-design` → `feat/rkmix` (console core) → `feat/rkmix-strip` → `feat/rkmix-buses` → `feat/rkmix-inserts`. Not merged; the manual, factory presets and the README come before the merge.

- **Window 1280 × 900.** Instead of a "Channels shown" setting, the fader area has **layers**: Inputs 1-16, Inputs 17-32, Buses (bottom row). The top panel has pages: **Channel** (or **Bus** when a bus is selected), **Sends**, **Routing**. Strips show H/G/E/C section lights (G and C light up while they reduce), a pan slider, the insert slots, Mute, the fader and a meter (pre-fader for channels, post-fader for buses and the main).
- **Settings:** about 3,700 mix settings, held as parameters of a never-processing processor (as RKBoard's routing), so the suite's knobs and switches work with them; a change rebuilds only its channel, bus or the main. The host sees 134 parameters plus Bypass, as decided.
- **Fader law:** -inf (the bottom) to +10 dB, 0 dB at three quarters of the travel.
- **The fixed strip's DSP** is the suite's, made cheap for 32 channels: RKGate's gate and RKCompress's compressor logic (curves, hysteresis, hold, log-domain timing from `core/dsp/Dynamics.h`) with the gain computed every 8 samples and interpolated (`MixDynamics.h`); the HPF is Butterworth biquads (12 or 24 dB/oct); the EQ bands the matched bells and shelves, all active bands run in one loop with their state in registers. 32 channels with every section on and 16 stereo buses with 32 sends: **1.9% of a core** (64-sample blocks); the console with nothing on: 0.3%.
- **The gate's gain-reduction meter** isn't on the channel page (no room); the strip's G lights while the gate reduces. The compressor's is.
- **Shelf Q:** the Low and High bands' Q applies only as bells (a shelf is 0.707; its Q knob dims).
- **Strip presets** (the fixed strip, inserts, name and colour; not pan, Main, keys or sends) are in the `RKMix Channels` preset folder; the channel page's Strip presets menu saves, loads, resets.
- **Bus Main defaults on** for Group and FX buses (a group usually feeds the main; the spec said Group off).
- **Send tap points** are chosen on the Sends page in a short list per Aux send; the bus's default tap is marked with a dot. A tap change isn't crossfaded (it is structure, like the spec says).
- **Bus-to-bus sends** take the source bus after its fader and pan (Group → Aux/FX, FX → Aux); a mono bus into a stereo bus is panned by its Pan.
- **Unfed buses are skipped** (no CPU) unless an insert on them is on (a reverb's tail).
- **Inserts:** 2 per channel, 4 per bus, 4 on the main, 2 on the mono sum; every RKBoard pedal (stereo RKFX plugins, Volume, Wah). A click on a slot opens the plugin's own editor over the console (scaled to fit) with On, Remove and Close; right-click switches it, replaces or removes it. On/off is the plugin's own Bypass (its 10 ms crossfade; off costs nothing).
- **Pedal host:** RKBoard's registry and its Volume and Wah pedals moved to `host/` (`rkfx_pedal_host`), which RKBoard and RKMix both link; RKBoard's own presets register on top. RKMix is its own `juce_add_plugin` target, like RKBoard.
- **Latency alignment** as designed: channels lined up with the slowest (pre-insert taps too), each bus's channel part and bus inputs, the main's, and every output at the same total, reported to the host. Limits: 512 samples for a channel's inserts, 4096 for a path (RKFX plugins report at most 63 each). Lines cost about 4.6 MB, allocated in prepare. With no latency anywhere nothing is delayed (bit-exact).
- **Core changes:** `PluginConfig::bypassMutes` (Bypass fades to silence); a plugin with it has no dry delay and may report up to 65,536 samples of latency (`ProcessorBase::kMaxMutedLatency`).
- **A bus's Output list follows its format** (added at the user's request): a mono bus offers single outputs, a stereo bus output pairs (and Off). Switching the format keeps the place: Out 5/6 becomes Out 5, Out 6 becomes Out 5/6. (The engine still sums a stereo source on a single output, for older states.)
- **Snapshot tool:** several clicks, separated by semicolons.
- **Validation:** RKFXTests, RKBoardTests (RKMix's state, inserts and RKBoard after the refactor), RKFXPresetTests pass; auval (`aumf Mix1 RKMu`, `aumf Brd1 RKMu`) and pluginval at strictness 10 pass for RKMix and RKBoard.
- **To look at in Gig Performer:** the bus layer's faders are short (4 insert rows); the window's overall size at 1280 × 900.

## Bus and main processing, mirrored pan (decided 2026-10-30, after phase 4)

Every bus, the main and the mono sum get a fixed EQ, compressor and limiter, each with its own On switch, all **off by default** (a bus stays a bit-exact sum until one is switched on):

    bus inputs -> EQ -> Comp -> inserts -> mute, fader -> Limit -> output (and the main, other buses)
    main       -> EQ -> Comp -> inserts -> mute, fader, balance -> Limit -> main output
    mono sum   -> EQ -> Comp -> inserts -> mute, fader -> Limit -> mono output

The limiter sits **after the fader**, so on an in-ear mix it protects the ears even with the fader pushed up. On the mono sum, the EQ's low-pass makes a sub feed without an insert.

| Section | Controls | Defaults |
| --- | --- | --- |
| EQ | HPF (20-400 Hz, 12/24 dB/oct, own On), 6 bands: Low (shelf or bell), 4 bells, High (shelf or bell), each Freq, Gain (±15 dB), Q; LPF (1-20 kHz, 12/24 dB/oct, own On) | 80 / 160 / 400 / 1k / 2.5k / 6k Hz bands, flat; HPF 40 Hz, LPF 16 kHz |
| Comp ("glue") | Threshold, Ratio, Knee, Attack, Release (or Auto), Makeup; RMS detector, stereo linked | -20 dB, 2:1, 6 dB, 30 ms, Auto, 0 dB |
| Limit | Ceiling (-20 to -0.1 dBFS), Release (10-1000 ms) | -1 dBFS, 100 ms |

- **The EQ** uses the channel EQ's filters (matched bells and shelves, Butterworth cut filters); its curve is drawn like the channels'.
- **The limiter** is zero-latency: a peak detector with an instant attack and the release set, then a clip at the ceiling, so the ceiling is never exceeded (it matters for ears). No lookahead: nothing is added to the latency.
- **The window:** the bus page gets EQ, Comp and Limit rows; what a bus feeds (its own sends) moves to the Sends tab, which then shows "what this strip feeds" for channels and buses. Clicking the Main or Mono strip opens a **Main page** with both chains.
- **Mirrored pan:** an Aux bus's "Pan follows" becomes **Pan: Centre / Follow / Mirror**. Mirror swaps left and right (the band's view of the stage, facing the audience; a stereo channel's balance too). FX buses always follow. (RKMix isn't released, so the on/off setting is replaced, no migration.)
- **CPU (as built):** about 1.5% of a core with every section of all 16 stereo buses, the main and the mono sum on (8 filters, compressor and limiter each; 64-sample blocks); a section that is off costs nothing. (The first estimate, 0.2-0.3%, was too low.)
- **As built (feat/rkmix-busdyn):** `BusDsp` (HPF, 6 bands, LPF, compressor with Auto release, zero-latency limiter with its clip), sharing the channel strip's filter code (`MixFilters.h`). A cut filter's slope change dips the filter out and back in. The bus page: setup (type, format, output, Main and pan or default tap and Pan), then EQ with its curve, Compressor and Limiter with their gain-reduction meters. A bus's own sends moved to its Sends tab. Clicking the Main or Mono strip opens the Main page (Main | Mono). Defaults: band frequencies 80 / 160 / 400 / 1k / 2.5k / 6k Hz, HPF 40 Hz, LPF 16 kHz. Tests: the limiter's ceiling with the fader at +10 dB, the EQ display, a mono sub low-pass, Mirror (a pair's sides and balance), the session.

## Decisions (2026-10-30)

- **Host:** both, plugin first; the standalone works on an audio interface as RKBoard's does.
- **Strip:** the full fixed console strip (trim, polarity, delay, HPF, gate, 4-band EQ, comp), each section switchable, plus **2 insert slots per channel and 4 per bus**.
- **Buses:** **16 flexible buses**, each typed **Group, Aux or FX**; Groups feed only later Groups; FX and Aux never feed Groups.
- **Solo:** both Cue (PFL/AFL to a cue output) and In place, selectable.
- **Main:** stereo plus a mono sum.
- **Latency:** aligned internally, reported to the host.
- **Host parameters:** channel/bus/main/mono faders and mutes, plus 32 assignables and Scene; the rest in the mix state.
- **Scenes and assignables:** 16 scenes with a glide, 32 assignables, MIDI learn in the standalone.

- **Bypass:** silence on every output.
- **Mono channels** run inserts in mono.
- **Send tap point per send** (channel × Aux bus), the bus supplying the default: so the singer's in-ear can take the vocal before the compressor while the other mixes take it after.
- **Talkback** and an **oscillator** (sine and pink noise) are in v1 (phase 6).
- **MIDI out** from audio-to-MIDI inserts (RKTrigger, RKFollow): each plugin's own channel and notes (no separate Channel Out), a warning only for the same note or controller on the same channel; pure MIDI effects not hosted in v1. RKTrigger's MIDI out in Gig Performer is checked first (MIDI's Step 0).
- **Not in v1:** mid/side input pairs, a comp-before-EQ switch. (Direct outs were here too; the user asked for them after phase 8, see Outputs.)

## Open questions

- **Accent colour** (`#6B8CA6` proposed) and window size once phase 1 is on screen.
