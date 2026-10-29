# RKRemap: design

**Patch bay: 64 inputs to 64 outputs** · Utility family · zero latency, no processing by default

RK + a verb, as every RKFX plugin: to *remap* inputs to outputs. This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKRemap is a patch bay for Gig Performer. Instead of a knot of crossing cables between the audio interface's inputs, the plugins and the outputs, every cable goes straight: **In 1 → RKRemap input 1**, **RKRemap output 1 → Out 1**, and the routing happens inside RKRemap, labelled and visible on one page.

- **Up to 64 inputs and 64 outputs.**
- **Each output picks its source:** any input, or none. One input can feed as many outputs as you like (a vocal mic to the FOH, the in-ear mixes and the recorder).
- **Stereo pairs:** inputs and outputs can be linked in pairs (1/2, 3/4, …) and routed as one.
- **Sum to mono:** a stereo input pair can go to a single output summed to mono, or to a linked output pair as mono, while also going stereo elsewhere. For example, a synth bass goes stereo to the in-ears and mono to the FOH.
- **Per output:** trim, mute and polarity. **Per channel:** a name and a signal light.
- **Views:** how many inputs and how many outputs the window shows, set independently. A 16-in, 24-out rig sees only that.

Unless a trim, polarity or mono sum is set, a route is a bit-exact copy: no processing, no latency.

## Step 0: confirm Gig Performer's handling of 64 channels

Before the design is built out, a test build of RKRemap with 64 inputs and 64 outputs (identity routing, no window) is loaded in Gig Performer as **AU and VST3**, to confirm:

- the plugin block shows 64 input and 64 output pins (Gig Performer allows up to 256 each);
- audio passes on every pin;
- the layout is offered by default (no channel-configuration dialog), and a saved gig reloads with all pins wired.

The planned layout is **one main bus of 64 discrete channels** in each direction. 64 is also the most a single VST3 bus can describe (its speaker arrangement is a 64-bit mask). If a format refuses it, the fallback is **32 stereo buses** each way, the same channels grouped by pairs, which Gig Performer shows as the same 64 pins. The test settles which.

## Routing

### Outputs pick their source

Each output has a **Source**:

| Source | What the output carries |
| --- | --- |
| Off | silence |
| In *n* | input *n* |
| In *n*/*n+1* (a linked input pair) | on a single output: the pair **summed to mono**; on a linked output pair: left to left, right to right (or mono, see below) |

- A **single input** sent to a **linked output pair** goes to both sides (a mono source centred in a stereo mix).
- **Mono** (a switch on a linked output pair whose source is a linked input pair) sums the pair to mono on both sides. The synth bass in the example goes **In 5/6 → Out 1/2** stereo for the in-ears, and **In 5/6 → Out 7** mono for the FOH (a single output always sums a pair).
- **The mono sum** is the average, (L + R) / 2, as RKUtility's Width at 0% does: centred sounds keep their level, a sound on one side only is 6 dB lower, and out-of-phase content cancels (as it would on a mono PA).

### Linking

- **Input pairs** (1/2, 3/4, … 63/64) can each be linked. A linked pair appears in the Source lists as "In 5/6", and keeps its single inputs available too ("In 5", "In 6").
- **Output pairs** can each be linked. A linked pair has one set of controls, those of its left output (Source, Trim, Mute, Polarity, Mono); the right output's own settings are kept but unused while linked.

### Per output

| Control | Range | Default |
| --- | --- | --- |
| Source | Off, In 1 … In 64, In 1/2 … In 63/64 | Off (see factory presets) |
| Trim | −24 to +12 dB | 0 dB |
| Mute | off, on | off |
| Polarity | normal, inverted | normal |
| Mono | off, on (linked pairs with a pair source) | off |

All are host parameters, so a Gig Performer widget or a rackspace variation can switch a route, mute an in-ear send or swap a source live.

### Switching never clicks

When an output's Source changes, the old route fades out as the new one fades in over **10 ms**. Mute, trim and polarity changes glide the same way (a polarity flip fades through silence). A steady route is never touched: no fade runs and the samples are copied as they are.

## Parameters

Per output *k* = 1 … 64: `out_k_source`, `out_k_trim`, `out_k_mute`, `out_k_polarity`, `out_k_mono`. Then per pair *p* = 1 … 32: `in_link_p`, `out_link_p`. 384 parameters in all, in that order (IDs permanent, append only).

The **Source** parameter's values are 0 = Off, 1–64 = In 1–64, 65–96 = In 1/2 … In 63/64. Choosing a pair that isn't linked behaves as its left input alone, and the window only offers linked pairs.

## Names and views (not parameters)

- **A name for every input and output** ("Violin pickup", "Vocal mic", "IEM L"), shown in the lists and the source menus ("In 5/6 · Synth Bass").
- **Inputs shown** and **Outputs shown** (1 to 64 each, defaults 16 and 16): how many the window lists. They only change the view; hidden channels keep routing. An output that is hidden but still has a source is counted in a note under the list ("3 hidden outputs routed"), so nothing plays unnoticed.

Names and view counts are part of a patch bay's setup, so they belong in **presets** as well as in the session. Today a plugin's extra state (RKConduct's MIDI file) is saved with the session only. **Core change:** `PluginConfig::extraInPresets`: when set, `getExtraState()` is also written to and read from preset files (as "extra", as in the host state).

## The window

About **1000 × 720 px**, scaling as usual:

- **Inputs column** (left): one row per shown input: number, name (click to edit), a signal light, and between each pair a **link** switch. A linked pair is drawn as one double-height row.
- **Outputs list** (right, scrolling when long): one row per shown output (or per linked pair): number, name, **Source** menu (inputs listed with their names, linked pairs after the singles), Trim (a small horizontal fader), Mute, Polarity, Mono (pairs only), a signal light, and the pair's link switch.
- **Top bar:** Inputs shown, Outputs shown, and the hidden-routes note.
- **Signal lights** light at −60 dBFS and turn accent-coloured at −20 dBFS and above, refreshed at about 15 fps, only for the rows on screen.

## Bypass

The RKFX standard Bypass passes each input to the output with the same number (In *n* → Out *n*). In a patch bay that is a different routing, not silence, so the manual warns against mapping Bypass to a control in a live rig. See the open questions.

## Identity

- **AU code** `Rmp1`, bundle ID `com.rkmusic.rkremap`, an effect (`aufx`).
- **Utility family**, accent **slate `#A3ADB8`**.
- **Not an RKBoard pedal**: the board's chain is stereo.

## Core changes

- **`PluginConfig::channels`** (default 0 = today's mono/stereo): a fixed number of discrete input and output channels (64 here). The base processor's buses, dry buffer and output stage then work on that many channels; mono-to-stereo handling is skipped.
- **`PluginConfig::extraInPresets`** (see Names and views).
- No Dry/Wet and no Output knob (`hasMix` and `hasOutput` off): trims are per output.

## Factory presets

| Preset | Idea |
| --- | --- |
| Straight 16 | In 1–16 → Out 1–16, 16 shown each way: a starting point |
| Stereo Pairs 16 | Inputs and outputs linked in pairs, 1/2 → 1/2 … 15/16 → 15/16 |
| IEM and FOH | Example rig with names: Vocal (In 1) to FOH (Out 1) and IEM L/R (Out 3/4); Violin (In 2) the same; Synth Bass (In 5/6, linked) stereo to the IEM pair and mono to FOH Bass (Out 2) |
| Recorder Split | In 1–8 to Out 1–8 (the PA) and also to Out 9–16 (a recorder), names "PA" and "Rec" |

## CPU and memory

- A copy of the inputs per block (64 channels, allocated in prepare(), since the outputs overwrite the same buffer), then one copy, or a multiply-add for summed, trimmed or fading routes, per output. Negligible: well under 0.1% of a core.
- Signal lights: one peak per channel per block.

## Tests

- **Bit-exact routes:** In *n* → Out *m* copies exactly; fan-out to several outputs; Off is silent.
- **Pairs:** linked In 5/6 → linked Out 1/2 is L→L, R→R; a single input to a linked output pair goes to both sides.
- **Mono:** a pair to a single output is (L + R) / 2; Mono on a linked output pair gives (L + R) / 2 on both; a hard-left signal comes out 6 dB lower, an out-of-phase pair cancels.
- **Trim, mute, polarity** as set; linked pairs follow their left output's controls.
- **No clicks:** source changes, mute, trim and polarity changes on a sustained tone produce no step beyond the signal's own; a steady route stays bit-exact (no fade running).
- **64 channels:** every input reachable from every output.
- **Views:** Inputs/Outputs shown don't change the audio; hidden routed outputs are counted.
- **Names and views in presets:** saved and loaded with presets and the session (and the core change keeps other plugins' presets unchanged).
- **Validation:** auval (`aufx Rmp1 RKMu`), pluginval at strictness 10, presets; and Step 0 in Gig Performer.

## Open questions

- **Bypass:** keep the RKFX standard (In *n* → Out *n*, with a warning), or make RKRemap's Bypass mute every output? Muting is safer on stage but breaks the suite's rule that bypass passes the input through. Recommendation: keep the standard and warn, or leave Bypass out of mappings.
- **Bus layout:** one 64-channel bus or 32 stereo buses, decided by Step 0.
- **Trim range:** −24 to +12 dB enough, or should it reach −∞ (off) like a fader?
