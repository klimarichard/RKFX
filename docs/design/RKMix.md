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

- **Link** (between each pair): a linked pair is one stereo channel with one set of controls, those of its left (odd) input. The right input's own settings are kept but unused while linked, as in RKPatch. A pair has **one name**.
- **Per channel:** a name, a colour (one of 8 strip colours), and a **signal light**.
- **Channels shown** (1 to 32, default 16) only changes the view. A hidden channel that reaches the main or a bus is counted in a note, as RKPatch's hidden-routes note does.

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
- **EQ coefficients** are computed only when a value changes (a scene glide recomputes them every 32 samples while it runs), never per block for steady settings. This is RKPatch's lesson.

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
- **Stereo sends** to a stereo Aux are placed by the bus's **Pan** mode: **Own** (each send's own pan, `chN_spanB`: every in-ear mix its own stage picture), **Follow** (the channel's pan, the default) or **Mirror** (the channel's pan, left and right swapped). See "As built: Own send pans".
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
- **The pan mode** (stereo Aux) applies at every tap, not only post-fader.
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
| Pan (stereo Aux only) | Own, Follow, Mirror | Follow |

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

The 32 outputs are fed by the main pair, the mono sum, the buses' outputs, the channels' direct outs and the cue output. **Several sources on one output add up**, and the routing page marks that output in the accent colour with a note ("Out 3: Bus 1 + Bus 4"). An output nothing feeds is silent. Outputs have names (as in RKPatch) for the routing page.

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

### As built: Own send pans and recall scope (feat/rkmix-sendpan, 2026-10-31)

- **Own send pans:** a pan setting per channel × bus (`chN_spanB`, appended, -100..100), smoothed per send in the engine and read only when the bus is a stereo Aux in Own mode (otherwise it snaps to its target, so a later switch to Own doesn't glide from a stale value). A mono channel pans with the constant-power law, a pair balances, exactly as the channel's own pan. **Own replaced Centre** (choice index 0, key `own`): Own with every send pan at centre is Centre, and `MixSettings::fromVar` reads a saved `"centre"` as `"own"`, so old mixes sound the same. Bus-to-Aux sends (Group and FX returns) still use the sending bus's pan.
- **UI:** a pan slider under each send to an Own bus on the Sends page; with Sends on fader for an Own bus, the strips' pan sliders show the send pans (tinted the bus's colour).
- **Recall scope, per scene** (`MixScenes::Scope`): category bits (faders & mutes, pans, channel processing, bus & main processing, inserts, group & main assigns) and a bit per bus for the sends to it (levels and send pans, from channels and buses). Every scene value is classified once by ID (`classify`, tested for every ID); a recall applies only what is in scope, and recall safe still wins. Scenes always **store** everything. Scopes live beside the scenes (storing a scene keeps its scope), are copied with a scene, saved per scene as a key list (`"scope": ["faders", "sends3"]`, absent = everything). The Scenes page edits the scope of any scene (following the active one); scene buttons with a partial scope get a dot and a tooltip.
- **Tests:** Own at centre equals the old Centre; a send pan hard right; Follow ignores it; per bus; a pair's balance; no clicks; kept in the session; "centre" loads as Own. Every scene value is outside an empty scope and inside the full one; IDs land in their categories; a sends-to-bus-1 scene changes only those sends (and pans) while faders, other sends, pans and inserts stay; scope kept in the session, kept when a scene is first stored, copied with the scene.

### As built: the standalone app (feat/rkmix-standalone, 2026-10-30)

- **RKMix.app** (Standalone format; the RKMusic icon "MIX" in the steel accent, from tools/icons). The MIDI output device is the standalone wrapper's (MIDI out from RKTrigger and RKFollow inserts, and MIDI thru).
- **Shared with RKBoard** through the pedal host: `rkfx::MidiMap` (the map; each app names its targets) and `host/Tempo.h` (tap tempo, MIDI clock follower, the standalone play head).
- **MIDI learn** targets: Assign 1-32 (an expression pedal, glided over about 30 ms), Scene (the controller's value picks it), Tap, Talk (momentary or toggle). Learn from the Assign page's MIDI button per row, the scene bar's, TALK's and Tap's right-click menus; the mappings are the rig's (Application Support/RKFX/RKMix/midi-map.json), the same for every mix.
- **Program changes 0-15** recall scenes 1-16 (standalone only; in a host, Gig Performer routes program changes).
- **Tempo** (saved with the mix): the field, Tap and Clock (follow MIDI clock, kept in the rig's settings.json) in the Master section; the inserts' play head in the standalone app.
- **Tests:** program change to scene, a learned CC gliding an Assign, a momentary Talk switch, the rig's mappings shared between instances, the tempo kept with the mix.

### As built: DCAs and mute groups (feat/rkmix-dca, 2026-11-03)

- **8 DCAs, 8 mute groups.** Host parameters (appended after Talk): `dca_fader_n` (fader law), `dca_mute_n`, `mgrp_n`. Membership is a mix setting per channel and bus (`chN_dcaD`, `busN_dcaD`, `chN_mgrpG`, `busN_mgrpG`, appended); its `ownerBits` are 0, so a change rebuilds nothing.
- **No engine change.** `readLevels()` (every block) gives each channel and bus fader + the sum of its DCAs' levels (a DCA at the bottom: silence; capped at +10 dB) and ORs its mute with its DCAs' mutes and its active mute groups. So a DCA works exactly as the strip's own fader (post-fader sends and Post-Fader direct outs follow, pre-fader taps don't, glides 20 ms) and a mute group as its Mute (silent at every tap). A linked pair: its left channel's membership.
- **Scenes:** DCA faders, mutes and mute groups are in the Faders & mutes category; membership in the assigns one (renamed "Assigns & DCAs", key unchanged). Recall safe applies to membership (owned by the strip), never to the DCA faders.
- **Names and colours:** `dcaNames`, `dcaColours`, `muteGroupNames` in the extra state.
- **Window:** a DCA layer between Buses and User 1 (`GroupStrip`: 8 DCAs then 8 mute groups); `GroupPanel` (members grid) in the top panel; Spill puts a DCA's members (up to 15) on the faders with the DCA last; a strip's number row turns warning-coloured while a group mutes it; DCA and mute group submenus on the strip's right-click menu. Scene buttons 34 px so six layer buttons fit.

### As built: undo and the show lock (feat/rkmix-undo, 2026-11-03)

- **MixUndo** (processor, message thread) polls every mix setting and the host parameters except assign*, scene, talk, bypass, 10 times a second, plus the current scene. A step is open while changes keep coming less than 500 ms apart; it stores each control's value before the step and its latest (an index per control, so a big recall is linear). Closing drops controls that came back. Undo / redo set the values back (and `MixScenes::restoreCurrent`, which stores and applies nothing and cancels glides, plus the Scene parameter, whose listener then sees no change). After applying, the poll's snapshot is retaken, so an undo isn't recorded. 200 steps. `setExtraState` clears it. Polling (not listeners) because host automation can come from any thread.
- **Show lock** (`RKMixProcessor::Lock`: off, faders, everything; not saved). The window lays `LockShield`s over the top panel, the master section (faders mode: all but Clear solo, Talk and Back to faders), the preset bar and Bypass, and in Everything also the faders area and the scene bar; with faders, the strips' pans stop taking clicks, names aren't editable, insert slots and right-click menus do nothing. Lock opens a menu; locked, held 1 s it unlocks.

### As built: ganging, copy and paste (feat/rkmix-gang, 2026-11-03)

- **Gang** (editor only, not saved): strip ids as the user pages' (channel, kUserPageBus + bus), Cmd/Shift-click a name toggles one (the first adds the selected strip too). `MixGui::Fader` gained `onGestureStart` / `onUserMove (travel)`, called only for moves by hand (drag, wheel, double-click, typed value); the editor records each other ganged fader's parameter and value at the start and sets start + travel (normalised, clamped), so offsets survive a clamp within a gesture. Mutes: `GangSwitch` (a ToggleSwitch that calls back after a click) copies the state to the others.
- **Insert copy** (feat/rkmix-insert-copy): `copyInsert (slot)` = {kind insert, plugin, from, on, preset (`getPresetVar`)}; `pasteInsert` inserts the plugin fresh at the target strip's channel count, loads the preset, sets on/off (refuses a slot the strip doesn't have). The insert menu: Copy insert (filled), Paste (any slot; same slot of each ganged strip).
- **Copy / paste** (processor): `Section` input, delay, hpf, gate, eq, comp, limit, whole. A copy is a var {kind channel|chain, section, from, values by field suffix, inserts for whole (`stripToVar`)}. Channel sections never hold routing (pan, Main, keys, direct out, as channel presets). Chain sections: eq (EQ with HPF/LPF), comp, limit, whole. Pasting sets only what differs; inserts via `stripFromVar` (slots that don't exist on the target are skipped). The editor pastes a channel copy onto every ganged channel when the target is ganged.

### As built: the monitor remote (feat/rkmix-remote, 2026-11-03)

- **Server** (`MixWebServer`, since 2026-11-06 the suite's `rkfx::RemoteServer` in `host/remote/`, shared with RKBoard's remote): `juce::StreamingSocket` listener thread + a thread per connection; HTTP GET (Connection: close) or a WebSocket upgrade (RFC 6455: own SHA-1, unmasked server frames, masked client frames, ping/pong/close, continuation, 1 MB cap). `forEachConnection` under a lock; writes under a per-connection lock.
- **Remote** (`MixRemote`): settings `Application Support/RKFX/RKMix/remote.json` {enabled, port, useIp, secret}; a bus's key = first 12 hex of SHA-1 (secret + ":bus:" + n). Routes: `/m/<key>` (the page, `remote/monitor.html` embedded at configure time into `MixRemotePageData.cpp`, {{KEY}} and {{TITLE}} replaced, title HTML-escaped), `/manifest/<key>.json` (standalone display, start_url = the page), `/icon.png` (drawn at start), `/ws/<key>`. A pusher thread every 50 ms sends each connection its bus's mix JSON when it differs from the last sent, meters every 150 ms from `takeRemotePeaks` (raised beside the window's peaks; each open WebSocket counts in `windowsOpen` so the engine measures). Messages {t: send|pan|fader|mute, k: chN|busN, v}: validated against the connection's bus (its sends, Own pans, returns that `canSend` into it, its fader and mute), queued (latest per parameter) and applied on the message thread at 30 Hz. Only `begin()` serves: the plugin and app call it (wrapperType set), tests and the snapshot tool don't.
- **Page:** plain HTML/JS, no libraries. Rows per channel (leaders) and returns, relative sideways drags (pointer events, touch-action pan-y, 6 px before a drag takes over), a dragged control ignores incoming values, reconnects every 2 s, a screen lock. "used" = named or a send above the bottom.
- **QR** (`MixQr`, now `rkfx::RemoteQr` in `host/remote/`): byte mode, level M, versions 1-10, all 8 masks with the standard penalty. Checked against macOS CoreImage's detector.
- **Window:** a Remote tab: on/off, port, links by IP, New links, status; a grid of QR codes for Aux buses with a name or an output, with the phones connected (click copies the link).
- Next: a full console remote (iPad) on the same server and protocol, with a PIN; OSC as a thin layer for TouchOSC and hardware.

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
- **Fader area:** **16 strips per layer**. Layer buttons: **Inputs 1–16**, **Inputs 17–32**, **Buses**, **User 1**, **User 2**. A linked pair takes one strip. A strip shows the name and colour, a small EQ curve and gain-reduction lights, the two insert names, Solo, Mute, the Pan knob, the fader with its meter, and clicking its name selects it into the top panel.
- **User pages (added 2026-10-30, feat/rkmix-userpages):** two pages of up to 16 strips the user picks, channels (a linked pair once) and buses mixed, in the order picked. Right-click a User button for a three-column chooser (inputs 1–16, 17–32, buses; it reopens after each tick so several go in a row; Clear), or right-click any strip: "On User 1/2". Clicking an empty page opens the chooser. Saved with the mix (`userPages`, as "ch3"/"bus2"), cleared by Reset; scenes leave them alone. A follower channel picked before its pair was linked shows as the pair.
- **Master section (right, always visible):** main and mono faders with meters, the cue output's level, the Solo mode/PFL-AFL switches with the solo indicator (and Clear solo), and the scene bar.

Meters (peak plus a hold, −60 to 0 dBFS) are measured **only while a window is open** and only for the strips on screen, at about 15 fps. This is RKPatch's other CPU lesson.

## Identity

- **AU code** `Mix1`, bundle ID `com.rkmusic.rkmix`, a music effect (`aumf`) with MIDI in, as RKBoard (inserts with MIDI input, program change for scenes), and MIDI out (`PluginConfig::midiOutput`, see MIDI out). AU, VST3, Standalone.
- **Utility family**, accent proposal **steel `#6B8CA6`** (not used by another plugin).
- **One bus of 32 discrete channels** each way (`PluginConfig::channels = 32`). RKPatch's Step 0 confirmed in Gig Performer that one 64-channel bus works as AU and VST3, so no new step 0 is needed.
- **Not an RKBoard pedal**: the board's chain is stereo. RKBoard can't be an insert either.
- **Host bypass = silence** on every output (unlike RKPatch's straight-through: raw inputs on the PA and in-ears would be worse than nothing). It fades out and in over 10 ms. **Core change:** a `PluginConfig` option for bypass to mute rather than pass the dry signal (e.g. `bypassMutes`).

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

## Input patch (designed 2026-11-04, built 2026-11-09)

Today channel *n* always takes input *n*. The input patch lets **any input feed any channel**, so if the wiring changes (a stage box sends the vocal on In 7 instead of In 3, or a venue's multicore is in a different order), the mix stays as it is and only the patch changes.

### What it does

- **Every channel has a Source:** Off or In 1–32. The default is its own number (channel 3 takes In 3), so a mix that never touches the patch works as it does today.
- **A linked pair has two sources,** one per side: the left side uses the odd channel's Source, the right side the even channel's. This is the one place where a follower channel's own setting is used while linked. Linking or unlinking doesn't change either source. A pair whose sources are swapped (In 8 left, In 7 right) is allowed.
- **One input can feed several channels** (a split): the same vocal mic on channel 3 for the PA and on channel 20 with its own EQ and compressor for the singer's in-ear. Each channel processes its own copy.
- **Off** makes the channel silent (as an input with nothing plugged in): the signal light stays dark, the strip still processes silence, and an RKTrigger insert on it gets silence.
- **An input that no channel uses** goes nowhere. The patch page shows it as unused.

### What follows the patch

| Thing | Follows | Why |
| --- | --- | --- |
| The channel strip, its sends, direct out, meters, signal light | the channel's Source | they belong to the channel |
| Gate and comp **Key** "Ch *n*" | channel *n*'s Source, after channel *n*'s trim | a key means "the kick channel", wherever the kick mic is plugged in. The key list is renamed from "In *n*" to "Ch *n*" with the channel's name. Existing mixes need no migration, because the default patch maps channel *n* to In *n* |
| **Talkback source** | the raw input (In *n*) | the talkback mic is a physical input, often not a channel at all |

### Scenes, presets, undo

- **Structure, not scene:** a scene never changes the patch (the suffix `_src` joins the structure list in `MixScenes`), as with tap points and keys.
- **Saved with the mix.** A mix saved before the patch existed loads with the default patch (no `chN_src` = its own number).
- **Channel presets and copy/paste** never hold the Source. It's routing, as pan and keys are.
- **Undo** covers it for free: MixUndo polls every mix setting.
- **Reset** puts the default patch back.
- **Factory presets** keep the default patch.

### The window

- **A Patch page** in the top panel, beside the Routing page. It shows 32 rows in two columns of 16. Each row has the channel number, name and colour, then its **Source** box. A Source that isn't the channel's own number is drawn in the accent colour.
- **Input signal lights:** across the top of the Patch page, a row of 32 lights shows which raw inputs carry signal. You can see where a mic actually arrives ("the vocal is lighting In 7") and patch it there. These are measured only while the page is open, as the window's meters are.
- **Buttons:** **Default** (channel *n* ← In *n*, after a confirmation) and **Swap** (pick two channels to exchange their sources: the usual fix when two cables are swapped).
- **The strip:** the Source box also sits in the channel page's Trim row, so a channel can be repatched where you're already working. A repatched channel shows its source under the number on the fader strip ("← 7"), so it's visible from the faders.
- **The console remote:** the iPad's Setup page gets the same list (Source per channel, the input lights). Source joins the structure message per channel, and the raw input peaks are sent only while that page is open.

### Engine

- **No new processing.** Each block the engine builds a table of 32 source pointers from the settings, either `in[source]` or a shared silent buffer for Off. Where it now passes `in + ch` (processChannel, keySignal, the pair's right side), it passes the table instead. A split reads one input twice, which is fine because `in` is read-only.
- **Setting** `ch<n>_src`: a choice, Off / In 1–32, stored by key (`off`, `1` … `32`), appended. A change rebuilds the render plan as a structure change, because the plan's channel activity can depend on it.
- **CPU:** none measurable (32 pointers per block). The input lights cost 32 peak scans per block, only while the Patch page is open.

### Virtual soundcheck with the patch (added 2026-11-09)

The user asked whether a playback track could reach more than one channel. With the patch it does, with no playback-only routing:

- **Playback replaces the raw input** (In *n*), as it always did (`MixPlayback::process` writes into the inputs before the engine). Each channel then takes its Source from those inputs, so a file mapped to In 7 reaches every channel patched from In 7, exactly as the mic would at the gig. A split set up in a virtual soundcheck is one the live console can run, and the other way round.
- **Live / PB is per input** (`inputOnPlayback`, unchanged). A channel's **PB** switch (`setChannelOnPlayback`) switches its Source input, a linked pair both sides' inputs; a channel shows PB when its input (either side's) is on Playback. So a split channel switches with its source. A channel on Off has no input to switch.
- **The file mapping** still maps files to inputs; each item names the channels that input reaches ("In 7 > Ch 3 Vocal, 20 Vocal IEM").
- **The recorder** still records the raw inputs (so a recording plays back through any patch). Files are named after the channel each input feeds (a linked pair's, else the first named channel, else the first; "In *n*" for an input no channel takes). Inputs *n* / *n* + 1 (*n* odd) record as one stereo file when a linked pair takes them as its left and right; with the default patch this is today's rule. `session.json` gains `patch` (each channel's Source, 0 Off). Patch changes are allowed while recording: the files are the inputs, fixed when Record was pressed.
- **Export** runs a copy of the engine with the patch in its settings, so it exports what the console plays.

### As built (2026-11-09)

- `MixEngine::Channel::source`: `kOwnInput` (-1, the engine's default for tests and old callers), 0 Off, *n* In *n*. `sourceOf (ch, in)` picks the pointer (Off: a silent buffer allocated in `prepare`); `processChannel` passes `{ sourceOf (ch), sourceOf (ch + 1) }` to the strip, `keySignal` reads `sourceOf (key)`. Talkback and Monitor EQ's measuring read `in` directly. No new processing; a Source change switches at once (no crossfade: a setup action, said in the manual).
- Setting `ch<n>_src`, choice Off / In 1–32 (keys `off`, `1` … `32`), default *n*, appended at the end of the settings. It's a channel field (`ChannelField::source`), not a strip field, so presets, copy/paste and Reset strip skip it. `_src` is structure in `MixScenes`.
- The key choices now read "Ch *n*" (keys unchanged); the window's and the iPad's lists show the channel's name ("Key: Ch 7 Kick").
- **Patch page** (`MixPatchPanel.cpp`, a tab between Routing and Talk / Osc; tabs narrowed to 82 px): input lights (raw inputs as they arrive, Live or Playback; `watchInputs` counts watchers, peaks measured only while one watches), "-" for an unused input, "x2" for a split; the 32 channels in **4 columns of 8** (as the Routing page's outputs, not 2 × 16); Default (confirmation) and Swap (click two channels).
- **The Source box sits in the Channel page's header** beside the colours (a pair: L and R boxes), since the Input section has no room under the trim. The fader strip shows "3 ← 7", "5/6 ← 10/9", "← Off".
- **The iPad:** a **Patch** tab of its own (`remote/patch.js`; the Setup page is full): the lights (`{"t":"inputs","on"}`, then `{"type":"inputs","p":[dB × 32]}` ten times a second while any iPad shows it, separate peaks from the window's), the Source boxes, Default (tap twice) and Swap. The structure message carries each channel's `src`; the Mix page's strips show the arrow too.

### Tests

- The default patch is bit-exact with today's engine (every existing summing test passes unchanged).
- Swap two channels' sources: each channel's output is the other input's signal.
- A split: two channels fed from one input, each processed separately.
- Off: the channel is silent at every tap and in its direct out.
- A linked pair: each side follows its own Source, including swapped sides.
- A key "Ch *n*" follows channel *n*'s Source.
- Talkback ignores the patch.
- A scene recall leaves the patch alone, and undo puts a patch change back.
- A mix without `_src` loads with the default patch.

## 32 scenes, names, scene markers (built 2026-11-09)

The user asked for scene names, 32 scenes, and recorder markers that mean a scene recall, followed in the virtual soundcheck and in export.

- **32 scenes** (`MixScenes::kNumScenes`). The host parameter `scene` is stretched to 1–32 (same ID): the user confirmed no Gig Performer mapping uses it, so the changed normalisation breaks nothing of theirs. Program changes 0–31, OSC `/rkmix/scene` 1–32, the iPad's `{"t":"scene","n"}` 0–31. Older mixes' 16 scenes load into 1–16.
- **Names:** `MixScenes::getName/setName` (40 characters, one line), saved in the scenes' var (`names`), not undone (names never are). Version bumps on a rename, so the window and the iPad follow. `markerLabel (n)`: "Scene n" or "Scene n Name", the recorder's marker at a recall. The structure message carries `sceneNames`.
- **The scene bar** keeps 16 buttons (narrower, 28 px) with a bank button before them ("1-16" / "17-32", lit in the accent when the active scene is in the other bank; a recall from anywhere switches the bank to it) and the active scene's number and name after them. Right-click: Recall, Rename... (an AlertWindow), Copy to (all 32, by name), Recall scope.... The Scenes page's scene list shows names, with the picked scene's name editable beside it. The iPad: the same bank button and the name.
- **Scene markers:** `MixScenes::sceneOfMarker`: "Scene N", optionally followed by a non-alphanumeric character and anything ("Scene 3 Hallelujah", "Scene 3: x"; not "Scene 3b"), N 1–32. Drawn in the accent on the position bar.
- **Playback follows them** (`MixRecorderSession::pollSceneMarkers`, 30 Hz on the message thread; `followScenes` in recorder.json, default on, "Scenes follow markers" on the playback view). The scene marker in effect at the play position (the last at or before it) is tracked with the recording's folder; when it changes (playing past one, any jump, a loop seam, a rename) or Play starts, its scene is recalled if the console is on another. The first look after a load records it without recalling. A scene recalled by hand stays until the marker in effect changes. Up to 33 ms late: fine for a soundcheck.
- **Export replays them** (`RKMixProcessor::exportMix (regions)`): `MixScenes::startReplay` (the mix as it is now, the stored scenes with the current one captured) and `replay` (what `recall` would do: store the current, a never-stored scene starts as the mix, else `applyTo` with the scope and recall-safe strips) give the scene values after each scene marker up to the last region's end. Each is turned into engine settings and levels by `readSettings` / `readLevels` under a **thread-local override** of the parameters' values (`RKMixProcessor::valueOf` / `normalisedOf` hide `ProcessorBase::valueOf`; only the message thread ever sets it, so the audio thread never sees it). The glide is sampled: up to 8 steps 25 ms or more apart, continuous values interpolated, switches at once (the engine's own 20 ms smoothing joins the steps). Changes before the first run keep only the last (its final state). `MixExport::render` starts each run in the state of the last step at or before its preroll start, and applies later steps at their exact samples (blocks split there) up to the region's end; inserts' on/off and scene-controlled knobs are set on the export's copies. Memory: about 40 KB per step.
- **Tests:** marker parsing; export changes at the marker's sample (silence from it on, sound before), a region starting on a marker, the next region's preroll in the earlier scene, the glide, Follow off; the console's scene and faders untouched; playback recalls on a jump, not on load, not before the first scene marker, not when off, a hand-picked scene kept; names saved and in the markers; the Scene parameter reaching 32.

## Key from a bus (designed 2026-11-04, not built)

Today a gate or compressor key can only be another input. With a bus key, the detector can listen to a **bus**. The main use is ducking the band under speech: the "Band" group's compressor is keyed from the "Speech" group, so the band goes down by itself whenever someone talks into any of the speech mics. A single input can't do that once there's more than one speech mic.

### Where a key can be set

| Section | Key choices |
| --- | --- |
| Channel gate and comp (today's Key) | Self, Ch 1–32, **Bus 1–16** |
| Bus comp (new Key) | Self, Ch 1–32, Bus 1–16 except itself |
| Main comp (new Key) | Self, Ch 1–32, Bus 1–16 |

- The mono sum's compressor gets no key: it is the main's sum, and the main's key already covers it.
- Names are shown in the list ("Bus 3 Speech", "Ch 5 Kick"), as everywhere else.
- A key carries no audio, so it can never make a routing loop, and a key from any bus is allowed. The only choice left out is a bus keying itself, which is Self.

### What the key hears

- **A bus key** is the bus's signal **after its inserts and mute, before its fader** (pre-fader, follows mute). Muting the speech group stops the ducking. Riding the speech fader doesn't change how much the band ducks.
- **A stereo bus** keys from its louder side, sample by sample, as a stereo channel's detector does.
- **A channel key** ("Ch *n*") is as today: that channel's Source after its trim.

### Timing

RKMix processes channels first, then Groups in ascending order, then FX buses, Aux buses, and finally the main. A key from a bus that is already done in this block is **on time**. Any other bus key has to use that bus's signal from the **previous block**:

| Keyed section | On time | One block late |
| --- | --- | --- |
| Main comp | every bus | — |
| Bus comp of a Group | lower-numbered Groups | higher Groups, FX, Aux buses |
| Bus comp of an FX bus | every Group, lower-numbered FX buses | higher FX, Aux buses |
| Bus comp of an Aux bus | Groups, FX, lower-numbered Aux buses | higher Aux buses |
| Channel gate or comp | — | every bus |

- **"One block late"** is the host's block: 2.7 ms at 128 samples and 48 kHz, 10.7 ms at 512. With a compressor attack of 10 ms or more (ducking) it isn't audible. On a gate opened by a bus, the first few milliseconds would be cut, so the manual recommends a channel key for gates.
- **The Key box marks a late key** with a small "late" label. Its tooltip gives the delay at the current block size and sample rate.
- **Insert latency is not compensated** for a key: a key from a bus with latent inserts arrives late by that latency too. Making up for it would delay the keyed signal itself, and a key is never worth added latency. The manual says so.
- **The common case is on time:** a ducked "Band" Group keyed by a lower-numbered "Speech" Group, or the main keyed by any bus. Factory preset Small Band doesn't use a bus key. The manual's example puts Speech on a lower Group than Band.

### Engine

- **Key signal per bus, only when used:** after a bus's inserts and mute, if any key in the mix points at it, the engine writes the bus's mono key (louder side) into a ring of `maxBlock` samples. On-time readers read this block's samples. Late readers read the newest *n* samples from before this block was written (*n* = this block's length), so a block-size change never reads a gap.
- **No key used = no cost:** the render plan lists which buses feed a key and which readers are late. Unused buses write nothing.
- **Bus and main comp** get a `key` pointer as the channel strip's comp has (`BusDsp` processComp gains a detector input). nullptr means Self, as on channels.
- **Settings:** `ch<n>_gate_key` / `ch<n>_comp_key` gain the choices `bus1` … `bus16` (appended keys, so saved mixes load unchanged). New settings `bus<n>_comp_key` and `main_comp_key` (default Self). All are structure, not scene, joining the `_comp_key` handling already in `MixScenes`. Copy/paste and channel presets leave keys out, as today.
- **Bus type changes:** if a bus key's source changes type (and so its order), only its timing changes. The Key box's late mark follows.
- **Memory:** 16 rings of `maxBlock` floats, allocated in `prepare()`.

### Tests

- A Group keyed by a lower Group ducks in the same block (no lag, checked by the sample position of the first gain reduction).
- A channel keyed by a bus lags by exactly one block, for 64, 128 and 512 sample blocks, and when the block size changes between blocks.
- Muting the key bus stops the ducking. Moving its fader doesn't change it.
- A stereo bus key follows its louder side.
- A bus never appears as its own key, and every other combination works.
- Mixes saved before bus keys existed load with the same keys.
- With no bus key in the mix, the output is bit-exact with today's engine.

## Deferred to v2 (decided 2026-11-04)

Talked about but not planned for now:

- **Console remote phase 7:** an iPhone layout of the full console, an OSC layer (TouchOSC, hardware), a native wrapper only if needed.
- **Virtual soundcheck:** record every input with one button, then switch each channel between live and playback.
- **Output processing:** per-output delay, EQ and limiter for delay speakers and wedges.
- **Ringing-out help:** a spectrum view (RTA) on a bus that highlights feedback, with one-click narrow cuts.
- **Gain finder:** hold during soundcheck, and the trim sets itself so peaks land around −18 dBFS.
- **Speech automixer:** gain sharing across chosen mic channels (the Dugan method).
- **Scenes that follow RKPlay or a set list:** song-named scenes, MIDI program changes out on recall, more than 16 scenes, set lists of scenes.
- **Meters:** loudness (LUFS) and phase correlation on the main. Also a clip memory on each channel: a light that latches when the channel clips, with the time ("clipped at 21:43"), until cleared.
- **From "Not in v1":** mid/side input pairs, a comp-before-EQ switch, multithreaded channel processing.

## Open questions

- **Accent colour** (`#6B8CA6` proposed) and window size once phase 1 is on screen.

## The console remote (as built)

Branch `feat/rkmix-console-remote`. The whole console on an iPad, a web app on the monitor remote's server (`MixRemote` owns a `MixConsole`).

- **Pages:** `/console` (console.html), `/c/<file>` (rk.js, console.js, mix.js, strip.js, inserts.js, setup.js), `/manifest/console.json`. Every file in `plugins/RKMix/remote/`, and the shared `host/remote/rk.js`, is embedded as binary data (`RKMix_RemoteFiles`, `MixRemotePage::find`). The demo tool `RKFXRemoteDemo_RKMix [mix.json] [port] [pin]` serves them from disk off a headless RKMix with a test signal (its settings in a temp file).
- **Sign-in:** WebSocket `/ws/console`, connection tag `MixConsole::kTag`. `{"t":"hello","pin"|"token","name"}`; a right PIN gives a token (kept in localStorage; up to 16 devices in remote.json `console.devices`). Disconnect or a new PIN forgets devices. 5 wrong PINs per address: 30 s wait. Nothing else is handled before sign-in.
- **Values:** every control is a parameter by ID: host parameters, mix settings, and insert parameters as `ins:<flat slot>:<param id>` (re-resolved when the rack's version changes; `bypass` never). `{"t":"sub","ids":[...]}` replaces the subscription; each tick (MixRemote's 30 Hz timer) sends `{"type":"v","p":{id:[normalised, text(, default, steps, choices)]}}` for what changed. `{"t":"set","id","v"}` sets a normalised value (setValueNotifyingHost), so undo and scenes take it. The page ignores echoes for a control while a finger holds it (and 400 ms after).
- **Structure** (`"type":"s"`, rebuilt every third tick, sent when it differs): channels (name, colour, stereo, hidden right of a pair, group-muted, soloed, H G E C, inserts), buses (type, stereo, Own), main/mono outputs and inserts, DCAs and mute groups with members, user pages, scene, undo/redo text, anything soloed, talk source set, sample rate.
- **Curves:** `{"t":"curves","k":["eq:ch3","comp:ch3","eq:bus2"]}`: 128 log-spaced dB values (StripDsp/BusDsp::responseDb) or 121 transfer points, sent when they change.
- **Meters:** 20 Hz from the push thread (`takeRemoteMeters`: channels, buses L/R, main, mono, gate and comp GR, chain comp and limit GR), in dB. Monitor pages still get theirs every third time (the peaks held between).
- **Commands:** solo, clearSolo, scene, undo, redo, name, colour, userPage, copy/paste (Section names), insAdd/insRemove/insOn/insCopy/insPaste/insParams; plugin types at sign-in.
- **Threads:** connections queue messages; the message thread handles them in `tick()` and fills each console's outbox; the push thread sends it (woken by `onOutbox`). A console that stops reading (500 waiting) is let go.
- **Graphics:** rk.js ports the suite's palette (utility-tinted background and panels, accent #6B8CA6), font heights (JUCE height = 1.193 em of the system font) and the paint code of Knob, ToggleSwitch, SegmentedSwitch, TextButton, MixGui::Fader/Meter/OptionList, the linear slider, GainReductionMeter, ResponseCurve and TransferCurve to one canvas, scaled so every iPad is 850 logical pixels high. Each finger is captured by the component under it.
