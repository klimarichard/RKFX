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

- **Top panel:** the selected channel or bus in full, with page tabs: **Channel** (or **Bus**, **Main**, **DCA**, **Mute group**), **Sends**, **Routing**, **Talk / Osc**, **Scenes**, **Assign**, **Recorder**, **Remote**; then **Undo**, **Redo** and **Lock**.
- **Master section (top right, always there):** solo settings, Clear solo, TALK, OSC, and in the standalone app the tempo. **REC** blinks at its top while the recorder runs; **PLAYBACK: all live** appears while any input plays a recording.
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

Each channel has **2 insert slots** (after the compressor), each bus and the main **4**, the mono sum **2**. Click an empty slot (**+**) to choose any RKFX effect, by family. Click a filled slot to open the effect's own window over the console, with **On**, **Remove** and **Close**; right-click a slot to switch it on or off, replace or remove it. **Copy an insert:** right-click a filled slot and choose **Copy insert**, then right-click any slot (empty or filled) on a channel, a bus, the main or the mono sum and choose **Paste**. The copy has the same settings and is on or off like the original; on a stereo strip it runs in stereo. A paste on a ganged strip goes into the same slot of every ganged strip. An effect that is off costs nothing. On a mono channel or mono bus the effect runs in mono.

## Sends and direct outs

The **Sends** page shows where the selected channel goes:

- **Aux and FX buses:** a **Send** knob each. Under the send to a stereo Aux bus whose **Pan** is **Own**, a **pan** slider places the channel in that mix (a balance for a stereo pair; double-click for centre). An Aux send also has a **tap**, the point where it takes the channel: **Default** (the bus's own setting, marked with a dot), **Input**, **Pre-EQ**, **Pre-Comp**, **Pre-Ins**, **Pre-Fader** or **Post-Fader**. For example, the singer's in-ear can take the vocal Pre-Comp while the other mixes take it after the compressor. FX sends are always after the fader.
- **Group buses:** an **Assign** switch each (after the fader and pan).
- **Direct out** (the page's header): the channel straight to an output as well, for a multitrack recorder or a broadcast feed, at a tap: **Input** (as it comes in, after the trim), **Pre-EQ** (after the gate), **Pre-Comp** (after the EQ), **Pre-Inserts** (after the compressor), **Pre-Fader** (after the inserts) or **Post-Fader**. A direct out is never panned. Before Post-Fader it ignores mute and solo, so a recording keeps going when you mute the channel in the room; Post-Fader follows the fader and mute.

A muted channel is silent in every send.

**Sends on fader:** on an Aux or FX bus's page, **Sends on fader** turns every channel fader into that channel's send to the bus, the way monitor mixes are set on a digital console. If the bus's **Pan** is **Own**, the strips' pans turn the bus's colour and set each channel's pan in that mix instead. **Back to faders** (in the master section) returns.

### Send EQ

Every send to an Aux or FX bus has its own small EQ, so a channel can sound different in one mix without changing the room or the other mixes: the bass and kick without their sub in the singer's in-ear, the guitar's low end out of the keyboard player's mix, a harsh hi-hat softened for the drummer.

- The **EQ** button under each Aux and FX send (Sends page), and above each fader while the faders show sends (**Sends on fader**), opens it. The button is lit when the EQ is on, with a small picture of its curve once it changes something.
- **On** switches it (it starts flat, so nothing changes until you move a band). **Flat** puts every band back to 0 dB and the low cut to Off.

| Band | What it is | Range | Default |
| --- | --- | --- | --- |
| **Low Cut** | High-pass, 12 dB/oct | Off, 20 to 400 Hz | Off |
| **Low** | Low shelf | 40 to 500 Hz, ±15 dB | 150 Hz, 0 dB |
| **Mid** | Bell, Q 0.5 to 4 | 200 Hz to 8 kHz, ±15 dB | 1 kHz, 0 dB, Q 1 |
| **High** | High shelf | 2 to 16 kHz, ±15 dB | 8 kHz, 0 dB |

- It works at every tap, from Input to Post-Fader, and has no latency. A stereo pair has one EQ for both sides.
- **Right-click** an EQ button: **Copy send EQ**, **Paste** (onto the ganged channels too, at the same bus), or **Paste to all sends to** this bus (the same cut on every channel in one mix).
- It costs nothing while it's off. Sixty send EQs on (six in-ear mixes of ten channels) take about 1% of one CPU core at 48 kHz.
- Scenes keep it with the sends to that bus (**Sends to B#** in a recall scope), Undo takes it back, and the mix saves it. Sends to Group buses, and sends from one bus to another (a reverb return into an in-ear mix), have no EQ.

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

## Monitor EQ (tuning wedges and in-ears)

Monitor EQ measures a wedge (or an in-ear mix) and suggests a 31-band **RKGraphEQ** curve for its Aux bus, so the monitor sounds even from low to high. RKMix plays **pink noise** out of the bus, listens with a mic on one of its inputs, compares the two band by band, and works out the EQ that brings the result close to a target curve. One click puts it into an RKGraphEQ on the bus.

Open it with **Monitor EQ** at the top of an **Aux** bus's page (select the bus on the Buses layer). It takes the place of the bus's processing; click Monitor EQ again to go back. It's also a tab on the iPad's console remote, which is handy for walking around with the mic (see [The console remote](#the-console-remote-ipad)).

When nothing is being measured, Monitor EQ does nothing at all: no sound, no CPU, no latency.

### Wedges (Speaker)

What you need: a mic on an input. A **measurement mic** (Behringer ECM8000, Superlux ECM999, a miniDSP UMIK, ...) is best. A stage mic works too: choose it as the **Mic** (**Shure SM58**, **Shure SM57**, **sE Electronics V7**) and RKMix takes its own sound out of the measurement. A measurement mic with a **calibration file** (a text file from its maker): **Load calibration file...** in the Mic list.

1. Set **Mic on** to the input the mic is plugged into, and the **Mic**. Set the input's gain on your interface so a loud voice doesn't clip.
2. Hold the mic **where the performer's head will be**, pointed at the wedge, **50 cm to 1 m** away. (Closer, a stage mic's bass rises by itself, and the EQ would take out bass the wedge doesn't have.)
3. Click **Start**. RKMix measures the room's own noise for 3 seconds (keep quiet), then plays pink noise into the wedge for about 5 seconds. The bus's own mix is replaced by the noise while it measures, and its fader and mute don't matter (**Level** sets how loud the noise is).
4. Move the mic a little (another spot around the head) and click **Next**. After the last position (**Positions**, 4 by default) it finishes by itself; **Finish** stops early with what is measured.
5. Look at the graph, set **Strength**, and click **Apply**.

### In-ears (In-ear)

An in-ear's sound depends on how it seals in the ear, so a mic in front of it says nothing useful. Two ways:

- **With a coupler** (an "ear simulator", an IEC 711-style block that holds the earpiece in a cavity the size of an ear canal, with a measurement mic inside; cheap ones cost around €100-250): plug it into an input, load its **calibration file** as the Mic, push the earpiece in as it sits in an ear, and measure as above. **Sides**: **Each ear** measures the left earpiece, then asks you to move the coupler to the right one (the noise plays on that side only); **Both**, **Left** or **Right** play on those sides.
- **With a measurement file** (no hardware): if someone has published a measurement of your in-ear model (a text file of frequency and dB), set **Source** to **Load measurement file...** and click **Use file**. No noise is played. A custom mould or a different fit changes the sound, so treat this as a starting point.

**Never wear the in-ears while measuring.** In-ear's Level starts at -40 dBFS and can't go above -20 dBFS.

### The graph and the targets

- **Measured** (the line with dots): the monitor's sound, band by band, set to 0 dB around the middle (250 Hz to 4 kHz). A band with **-** under it didn't hear enough over the room's noise: it gets no correction (raise the Level, or quieten the room).
- **Target** (dashed): what it should be.
- **Correction** (the lavender bars): what the EQ will add or take away, at the chosen Strength. A bar drawn faded is a band where the mic itself is weak (a stage mic's top end): it gets only half. A small box at 0 dB is a band left alone on purpose: a narrow, deep dip (a cancellation from a reflection: boosting it would only waste power) or the ends a wedge simply doesn't reach.
- **With it** (thin line): what the measured line becomes with the correction.

| Target | Type | What it is |
| --- | --- | --- |
| **Gentle tilt** (default) | Speaker | Flat up to 1 kHz, then 1 dB down per octave (-4 dB at 16 kHz). A wedge that measures dead flat sounds harsh up close; this sounds natural |
| **Flat** | Speaker | Flat everywhere |
| **Vocal wedge** | Speaker | Gentle tilt with the bass rolled off below 150 Hz: less mud and less feedback in a vocal wedge |
| **Speakers in a room** (default) | In-ear | What good speakers in a room sound like at the eardrum (the ear's natural rise around 3 kHz), with a gentle tilt and **Bass** (0 to +8 dB, default +4) |
| **Diffuse field** | In-ear | The ear's natural response alone, without the bass and the tilt |

Limits: a wedge's correction cuts down to -12 dB and boosts no more than +4 dB; an in-ear's cuts to -10 dB and boosts up to +6 dB, and above 8 kHz only follows the broad trend (within 3 dB), because the fine detail up there changes with every fit.

### Apply and Undo

- **Apply** writes the curve into the bus's **RKGraphEQ** (switching it to 31 bands and on). If the bus has none, Apply puts one in the first free insert slot; if all four slots are taken, it says so.
- The measurement is taken **through** the bus's EQ as it is, so the result is a change on top of it. Apply, then **Measure again** to check: the measured line should now sit close to the target.
- **Undo** puts back what Apply changed (or removes the RKGraphEQ it added). The main Undo button doesn't cover inserts, so use this one.

### Good to know

- Start with a low **Level** and raise it until the noise is clearly louder than the room. If the mic clips, that position is measured again.
- A measurement stops at once with **Stop**, when you close Monitor EQ (or select another strip), recall a scene, change the bus's type, or when the iPad that started it loses its connection for more than 2 seconds. The bus goes back to its own mix.
- The bus needs an **Output** set before it can be measured.
- Other inserts on the bus (a compressor, a limiter) stay in while measuring; they don't change the curve much, but a heavily compressing one is best switched off for the measurement.
- Monitor EQ tunes the **sound**. It doesn't hunt feedback (ringing out a wedge is a different job).
- The mic curves (SM58, SM57, V7) are typical curves from the makers' charts; real mics differ by a dB or two, which is fine for a 31-band EQ.
- It isn't on the musicians' phone pages: those have no PIN, so only the iPad console (with its PIN) and the window can play noise into a monitor.
- The settings (type, mic, target, level, files) are saved with the mix.

## Recorder and virtual soundcheck

The **Recorder** page records every input to its own WAV file for the whole gig, and plays a recording back into the channels in place of the band, so the mix can be set again without them: last night's show, or the afternoon's line check.

### Recording

- **Record** starts, **Stop** ends. The time, the free disk space and how long it lasts at the chosen tracks, and the **dropouts** are next to it. **REC** blinks in the master section on every page.
- **What is recorded** (saved with the mix; a new mix records every input and the main): tick the inputs (**All**, **None**), **Main** (the main L/R after its limiter, for listening back) and any **Aux buses** (their outputs, what the musician heard).
- **The inputs are recorded as they arrive**, before the trim and polarity, so everything after the trim works the same on playback as live.
- **One file per channel:** a mono file for a single input, one **stereo file for a linked pair** (`05-06 Keys.wav`, left = the odd input). **Pairs as two mono files** (this computer's setting) records a pair as `05 Keys L.wav` and `06 Keys R.wav` instead, for a DAW that prefers mono tracks.
- **Files:** WAV, **24-bit** or **32-bit float** (this computer's setting), at the session's sample rate, in a folder per recording: `Music/RKMusic Plugins/RKFX/Recordings/<date> <time> <mix name>/` (**Folder...** changes where; **Show** opens it). Each file is named by its input and the channel's name (`01 Lead Vox.wav`). A `session.json` beside them says which input each file came from, the names and colours, the markers and any dropouts.
- **Markers:** every scene recall writes one ("Scene 3"), and **Marker** adds "Marker 1", "Marker 2"...
- **Size:** a mono track at 48 kHz, 24-bit is about 518 MB an hour: a 3-hour gig with 24 inputs is about 37 GB. Record refuses to start with less than 10 minutes of space, and stops by itself (the files finished) with a minute left. Files past 4 GB become RF64 automatically.
- **Safe for the show:** the recording never touches the sound. If the disk can't keep up, RKMix drops what it can't write (counted as dropouts, every file still aligned) rather than stopping the audio. A crash or a power cut loses at most the last 10 seconds.
- **While recording:** **Link** is dimmed (the files match the channels as they were), and so is loading a mix from the preset menu. If the host reloads the session anyway, the recording carries on in a new folder from the next sample. A change of sample rate stops it.
- **Record** and **Marker** are host parameters, for Gig Performer buttons.

### Virtual soundcheck (playback)

1. On the Recorder page, choose **Playback (virtual soundcheck)**, then **Load...** a recording's folder: RKMix's own, or any WAV files whose names start with their input number (`05 Kick.wav`, `09-10 Overheads.wav`). A recording at another sample rate than the session's is refused.
2. Each file is shown with the input it feeds; change it in its menu (a stereo file goes onto a pair: In 1/2, In 3/4...). The mix's links don't have to match: a stereo file plays into two unlinked channels as well. The recorded main and buses are listed but **Off**. **Use the recording's names** names the channels as they were.
3. Switch inputs to **Playback**: **PB** in the Input section of the Channel page, or **All to playback**. A strip on Playback shows **PB** over its number. An input on Playback with no file is silent.
4. **Play** / **Pause**, **Stop** (back to the start), **◀ Marker** and **Marker ▶**, **-1 min**, **-10 s**, **+10 s** and **+1 min**, and the bar (click to go there). **Loop** plays the part between the markers around the play position over and over: one song's mix.
5. **Markers** (the list beside the files): every marker of the recording, in time order, the one at the play position highlighted.
   - **Click** a marker to go there.
   - **Double-click** its name to rename it ("Take 03" to "Hallelujah"). Enter keeps the new name, Esc leaves the old one.
   - **×**, or **Delete** on the selected marker, deletes it. **Undo delete** brings back the last one deleted.
   - **Add marker here** puts a new one ("Marker 1", "Marker 2"...) at the play position, also while playing.

   The changes are saved into the recording's `session.json` right away, so they are there the next time it is loaded. (A folder of plain WAV files gets a `session.json` that holds only its markers.) The bar, ◀ Marker / Marker ▶, Loop and the iPad follow at once. The markers can't be changed while locked, or in a recording that is still being recorded.

- The recording replaces the input **before the trim**: the gate, the compressor, the keys, the inserts (RKTrigger and RKFollow play their samples and synths as at the gig) all work as live. Every file stays aligned to the sample; starts, stops and jumps fade over 5 ms.
- **The source is never saved**, never in scenes and never undone: every session and every loaded mix starts with every input **Live**, so a show can't start on Playback by mistake. **Lock** blocks switching to Playback; **PLAYBACK: all live** in the master section stays available, also when locked.
- A virtual soundcheck can be recorded too (to compare two mixes): the page warns when a recorded input is on Playback.
- The iPad's **Recorder** page has Record and Stop (Stop asks again), the time, dropouts and Marker, and the playback transport with the markers and the loop. Recordings are loaded on the computer.

## MIDI out

RKMix sends MIDI from two kinds of inserts: **RKTrigger** (a kick or snare mic triggering a drum sampler) and **RKFollow** (a voice or violin playing a synth). Each uses its own MIDI channel and notes. Wire RKMix's MIDI output to the instrument in Gig Performer.

- When two inserts could send the same note or controller on the same channel, their slots show **MIDI** in the warning colour and the Routing page lists the clash. It only warns: doubling a note can be intended.
- Mute doesn't stop MIDI (a muted kick mic still triggers its sample). Switch the insert off to stop it.
- **MIDI thru** (Routing page, default off) passes RKMix's MIDI input on to its output.

## The monitor remote (phones and tablets)

Each musician sets their own in-ear mix on their phone, with nothing to install: a web page that RKMix serves on your network.

1. On the **Remote** page, click **Remote off** to switch it on. It shows the address it listens at (`http://<this computer's name>.local:8080`).
2. Every Aux bus in use (with a name or an output) gets a **QR code**. The musician points the phone's camera at their code and opens the link. Clicking a code copies its link. With many in-ear mixes the codes get smaller so they all fit: clicking one then also shows it large, for the phone to scan.
3. On the phone (drawn with the same faders, pans, meters and switches as RKMix's window): **My mix** (the bus's level and Mute) and a row per channel. **Drag a row sideways** to set that channel's level in the mix (the list scrolls when you drag up and down). In an **Own** bus each row also has its pan (double-tap it for the centre). FX and Group buses that feed the mix (the reverb) are rows too, marked "return". The thin bar under each fader is the channel's signal (orange above 0 dB). **Lock** (top right) stops pocket touches. **Fine** (next to Lock) is for small changes: while it's lit, the faders and pans move a quarter as far for the same finger travel. The phone remembers it. **EQ** (on each channel's row, lit when on) opens that channel's EQ in your mix only (its send EQ, see [Send EQ](#send-eq)): drag a handle on the curve sideways for its frequency and up and down for its gain (double-tap it for 0 dB). **C** is the low cut (drag it to the far left for Off), **L**, **M** and **H** the low shelf, mid and high shelf; a slider sets the mid's Q. **On**, **Flat** and **Close** are at the top. **Tap the star** (☆) on a row to keep that channel on top, under **Me** (your own vocal and instrument, say); the rest are under **Others**. Starred channels always show. Tap the star again to put it back. **Hide pans** (next to Fine, in an Own bus) takes the pans away for the gig, once they're set at the soundcheck; it's lit while they're hidden, and tapping it again brings them back. The phone remembers both, for each mix. **Show all channels** also shows the unnamed channels that aren't in the mix yet.
4. **iPhone and iPad:** Share → **Add to Home Screen**. The mix gets its own icon and opens full screen, like an app. Android: the browser's menu → Add to Home screen (or Install app).

A phone can only change its own mix: its sends, their EQs, its returns, its level and mute. Nothing else in the console, and no other musician's mix. Each mix's link has its own key. The links stay the same from mix to mix and gig to gig, so a home-screen icon keeps working. **New links** makes new ones (the old links stop working: everyone scans again).

- The phones and the computer must be on the **same network** (Wi-Fi or the computer's hotspot). The first time, macOS asks whether Gig Performer (or RKMix) may accept incoming connections and find devices on the local network: allow it.
- **Links by IP address:** if a phone can't open the `.local` name (some Android phones and some networks), the links use the computer's IP address instead. An IP address can change between gigs; the name doesn't.
- **Port:** 8080 unless another app already uses it (double-click to change). In **Gig Performer**, put RKMix in the **global rackspace**: only one RKMix can serve, and if another RKMix already took the port, the Remote page says so.
- The phone's screen still locks by itself: set Auto-Lock to Never (or longer) in the phone's settings for the show. The remote works while the window is locked. Its changes go into the undo history and the active scene, like any other change.

The remote settings belong to the computer (`~/Library/Application Support/RKFX/RKMix/remote.json`), not to the mix.

## The console remote (iPad)

The whole console on an iPad, from anywhere in the room: the same server as the monitor remote, protected by a PIN. It looks like RKMix's window, with buttons sized for fingers.

1. On the **Remote** page, switch the remote on. The **Console remote (iPad)** panel shows the **PIN** (4 digits, chosen at random the first time; double-click it to choose your own, 4 to 8 digits) and a **QR code**.
2. Scan the code with the iPad's camera (or type `http://<this computer's name>.local:8080/console`), type the PIN once. The iPad remembers it.
3. Share → **Add to Home Screen** opens it full screen like an app, in landscape. The home-screen app is separate from Safari, so it asks for the PIN once more.

Pages (tabs at the top):

- **Mix:** the strips as in the window: **Inputs 1-16**, **Inputs 17-32**, **Buses**, **DCA** (with **Spill**), **User 1**, **User 2**; Main and Mono always on the right, the **scenes** at the bottom, **Clear solo** and **TALK** (held, or tapped on and off when Latch is on). Put a finger anywhere on a fader and slide: it moves from where it was, never jumping to the finger. Several fingers move several faders. Pans slide sideways (double-tap: centre). An Aux or FX bus's **Sends** button (on the Buses layer) turns the channel faders into its sends, and the pans into its send pans in an Own bus; **Back to faders** ends it. While the faders show sends, the **EQ** button above each fader opens that send's EQ (the curve with its handles, On, Flat and the knobs). Tap a strip's name to select it, double-tap it to rename; tap its H G E C letters to open it on the Channel page. Tap an insert slot to add a plugin or open it.
- **Channel:** the selected channel in full (input, delay, high-pass, gate, output, EQ, compressor) with its sends, taps, send EQs (the **EQ** button under each Aux and FX send) and direct out; or a bus's, the main's or the mono's EQ, compressor and limiter, with a bus's setup and sends (or the main's routing). The **EQ curve** has a handle per band (and the high-pass, and the bus low-pass): drag it sideways for the frequency, up and down for the gain; double-tap it for 0 dB. Knobs: drag up and down, double-tap for the default. **Copy...** copies a section (or the whole strip), **Paste** puts it on the strip shown. The row at the bottom picks the strip (the Mix page's layer), the arrows step through every strip.
- **Setup:** solo and the cue output, talkback, the oscillator, which channels and buses each DCA and mute group holds, and which strips the user pages show (in the order tapped).
- **Monitor EQ:** measure any Aux bus's wedge or in-ear from where the performer stands, with big Start, Next, Finish, Stop, Apply and Undo buttons (see [Monitor EQ](#monitor-eq-tuning-wedges-and-in-ears)). Calibration and measurement files are loaded in the window; the iPad can then choose them.

An open insert shows its controls as knobs and switches, with **On**, **Copy**, **Paste**, **Replace...**, **Remove** (tap it twice) and **Close**. The plugin's own editor, with its curves and pads, is in RKMix's window.

**Lock** (top right) locks the iPad: the faders and mutes only, or everything. Hold it a second to unlock. It's the iPad's own lock, apart from the window's.

**Fine** (next to Lock) is for small changes: while it's lit, the faders, knobs, pans, the EQ curve's handles and the Monitor EQ sliders move a quarter as far for the same finger travel. The iPad remembers it.

- Everything the iPad changes is the same as a change in the window: it goes into the undo history (Undo and Redo are on the iPad too) and the active scene.
- **Disconnect** (beside each iPad on the Remote page) lets an iPad go: it must type the PIN again. **New PIN** does that for every iPad. After five wrong PINs an iPad waits 30 seconds.
- **Console on / off** switches the console remote off without stopping the phones' monitor mixes.
- Set the iPad's Auto-Lock to Never for the show.

## Names from Gig Performer

The phones and the iPad can show where the show is: Gig Performer's rackspace and variation (or song and song part), in a strip under a phone's header and in the middle of the iPad's header. Gig Performer sends them to RKMix by OSC, from its gig script.

1. On the **Remote** page, in **Names (Gig Performer, OSC)**, click **Names off** to switch them on. It listens on port **9100** (double-click to change), on this computer only.
2. In Gig Performer 5, open the **Global Rackspace script** editor, paste the script from `tools/gigperformer/RKMixNames.gpscript`, check that its port matches, and **Compile**. A script defines each callback only once: if yours already has `On Rackspace` (or `On Variation`, `On Song`, `On Songpart`, `GigLoaded`), put the script's `SendNames` line into yours. In the setlist it sends the song and song part; otherwise the rackspace and variation.
3. Change rackspace: the Remote page shows what came in, and every phone and iPad shows it within a moment.

What RKMix listens for (any sender, not only Gig Performer):

| Address | Arguments | |
|---|---|---|
| `/rkmix/title` | text | The first line (the rackspace, or the song). Empty text clears it. |
| `/rkmix/subtitle` | text | The second line (the variation, or the song part). |
| `/rkmix/now` | text, text | Both at once (the second optional). |
| `/rkmix/clear` | | Both gone. |
| `/rkmix/scene` | 1 to 16 | Recalls that scene (an integer, a float or text). |

- The names are only shown: they aren't saved with the mix, aren't in the scenes or the undo history, and are gone when the names are switched off. A phone that connects later gets the names at once.
- **From other computers** lets another computer on the network send (Gig Performer on another Mac); off, only this computer can.
- Like the web server, the settings belong to the computer (`remote.json`), and the first RKMix to start takes the port: put RKMix in Gig Performer's **global rackspace**.

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
- **Recall scope** (Scenes page, for each scene): what recalling that scene changes. Switch on any of **Faders & mutes**, **Pans**, **Channel processing** (trim, polarity, delay, high-pass, gate, EQ, compressor), **Bus & main processing** (their EQ, compressor, limiter), **Inserts** (on or off, and the scene-controlled knobs), **Assigns & DCAs** (group assigns, Main switches, DCA and mute group members), and the **Sends to** each bus (**B1** to **B16**: the send levels to that bus, their send EQs, and their pans in an Own bus). **Everything** is the default. For example, scenes that only change the singer's in-ear between songs: switch everything off except **Sends to** the singer's bus. A scene still stores everything, so widening its scope later loses nothing. Pick the scene in the list (it follows the active scene), or right-click a scene button and choose **Recall scope...**. **Copy to all scenes** gives every scene the same scope; copying a scene copies its scope too. A scene that recalls only part of the mix has a dot under its button, and its tooltip says what it recalls.
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
| **Band with In-Ears** | Lead and backing vocals, guitar, bass, keys (5/6), kick, snare, overheads (9/10); a "Drums" group, a "Reverb" FX bus, and four stereo in-ear mixes (IEM Vocals, Guitar, Bass, Keys) on Out 3/4 to 9/10, each with its player up front and a little reverb (IEM Vocals has its own pans: guitar left, keys and backing vocal right); User 1 holds the vocals and the in-ear buses; DCAs Vocals and Band (guitar, bass, keys, the Drums group), mute groups Band (every instrument) and Vocal mics |
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
| **Record**, **Marker** | The recorder: on = recording; Marker adds one marker per press |

Everything else is in the mix itself. A scene that moves a fader moves its host parameter, so Gig Performer widgets follow.

## Notes

- **Bypass** silences every output (a console has no "dry" signal).
- RKMix adds no latency of its own. An insert that has some (RKPolyPitch, RKLimit's lookahead) delays its path, and RKMix lines up every other path and output with it, so everything stays in time.
- The bus and main **low-pass** filters go down to 1 kHz.
- Gig Performer tip: map footswitches to **Scene** (or use program changes in the standalone app), an expression pedal to an **Assign** driving an insert's volume or wah, and a button to **Talk**.
