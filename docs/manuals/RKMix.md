# RKMix

**Mixing console** · 32 inputs, 32 outputs · every RKFX effect as an insert · plugin (AU, VST3) and standalone app

RKMix is a live mixing console in one plugin: 32 input channels with a full channel strip, 16 buses that can be groups, monitor mixes or effect returns, a stereo main with a mono sum, and any RKFX effect in insert slots. It has solo, talkback, a test oscillator, 16 scenes and 32 assignable controls. Use it in Gig Performer as the heart of a rig (one plugin for the whole band, with in-ear mixes), or as a standalone app on an audio interface.

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

- **Top panel:** the selected channel or bus in full, with page tabs: **Channel** (or **Bus**, or **Main**), **Sends**, **Routing**, **Talk / Osc**, **Scenes**, **Assign**.
- **Master section (top right, always there):** solo settings, Clear solo, TALK, OSC, and in the standalone app the tempo.
- **Fader area:** 16 strips at a time. Click a strip's name to show it in the top panel.
- **Main and Mono strips** (right): click one to open the Main page.
- **Bottom row:** the strip pages (**Inputs 1-16**, **Inputs 17-32**, **Buses**, **User 1**, **User 2**), the scene bar and Bypass.

Each strip shows its name and colour, the H G E C lights (high-pass, gate, EQ, compressor on; G and C light up while they reduce), its insert slots, the pan, Mute, S (solo), the fader with its level and the meter. Right-click a strip for its colour and its user pages.

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

- **Aux and FX buses:** a **Send** knob each. An Aux send also has a **tap**, the point where it takes the channel: **Default** (the bus's own setting, marked with a dot), **Input**, **Pre-EQ**, **Pre-Comp**, **Pre-Ins**, **Pre-Fader** or **Post-Fader**. For example, the singer's in-ear can take the vocal Pre-Comp while the other mixes take it after the compressor. FX sends are always after the fader.
- **Group buses:** an **Assign** switch each (after the fader and pan).
- **Direct out** (the page's header): the channel straight to an output as well, for a multitrack recorder or a broadcast feed, at a tap: **Input** (as it comes in, after the trim), **Pre-EQ** (after the gate), **Pre-Comp** (after the EQ), **Pre-Inserts** (after the compressor), **Pre-Fader** (after the inserts) or **Post-Fader**. A direct out is never panned. Before Post-Fader it ignores mute and solo, so a recording keeps going when you mute the channel in the room; Post-Fader follows the fader and mute.

A muted channel is silent in every send.

**Sends on fader:** on an Aux or FX bus's page, **Sends on fader** turns every channel fader into that channel's send to the bus, the way monitor mixes are set on a digital console. **Back to faders** (in the master section) returns.

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
| **Pan** (stereo Aux) | **Centre**, **Follow** (the channels' pans), **Mirror** (left and right swapped: the band's view of the stage) | Follow |
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
| **Band with In-Ears** | Lead and backing vocals, guitar, bass, keys (5/6), kick, snare, overheads (9/10); a "Drums" group, a "Reverb" FX bus, and four stereo in-ear mixes (IEM Vocals, Guitar, Bass, Keys) on Out 3/4 to 9/10, each with its player up front and a little reverb; User 1 holds the vocals and the in-ear buses |
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

Everything else is in the mix itself. A scene that moves a fader moves its host parameter, so Gig Performer widgets follow.

## Notes

- **Bypass** silences every output (a console has no "dry" signal).
- RKMix adds no latency of its own. An insert that has some (RKPolyPitch, RKLimit's lookahead) delays its path, and RKMix lines up every other path and output with it, so everything stays in time.
- The bus and main **low-pass** filters go down to 1 kHz.
- Gig Performer tip: map footswitches to **Scene** (or use program changes in the standalone app), an expression pedal to an **Assign** driving an insert's volume or wah, and a button to **Talk**.
