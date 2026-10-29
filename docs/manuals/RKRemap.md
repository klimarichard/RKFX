# RKRemap

**Patch bay: 64 inputs to 64 outputs** · Utility family (slate) · zero latency, no processing unless you ask for it

RKRemap is a patch bay for Gig Performer. Instead of a knot of crossing cables between your audio interface's inputs, your plugins and your outputs, every cable goes straight (**In 1 → RKRemap 1**, **RKRemap 1 → Out 1**) and the routing happens inside RKRemap, on one labelled page.

- **64 inputs and 64 outputs.** Each output picks its source; one input can feed as many outputs as you like.
- **Stereo pairs** that move as one, and **sum to mono** where a pair has to go to a single output.
- **Per output:** trim, mute and polarity. **Per channel:** a name and a signal light.
- **Views:** show only as many inputs and outputs as your rig uses.

A route is an exact copy of its input (bit for bit) unless you set a trim, polarity or mono sum: no processing, no delay. For the header and presets, see [Common Controls](Common%20Controls.md). RKRemap has no Dry/Wet and no Output knob (the trims are per output), only **Bypass**.

## Setting it up in Gig Performer

1. Insert RKRemap and wire it **straight**: your interface's input *n* to RKRemap's input *n*, RKRemap's output *n* to the interface's output *n* (or to the plugin that should get it).
2. A new RKRemap passes **In *n* → Out *n***, so inserting it changes nothing until you change a route.
3. Set **Inputs shown** and **Outputs shown** (bottom row) to what your rig uses, name the channels, and pick each output's source.

## Inputs

One row per shown input: its number, its **name** (double-click to type one, for example "Violin pickup"), and a **signal light**: dim when there is signal (above -60 dBFS), bright when it's strong (above -20 dBFS). **Link** (on the first row of each pair: 1/2, 3/4 ...) makes the pair a stereo input: it then appears in the source menus as one choice ("In 5/6").

## Outputs

One row per shown output (a linked pair shares one row of controls):

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Name** | | | Double-click to name it ("FOH Vocal", "IEM L"). |
| **Source** | Off, In 1 ... In 64, linked pairs | In *n* (the same number) | What the output plays. The menu shows the inputs' names. |
| **Trim** | -24 to +12 dB | 0 dB | The output's level. Double-click to reset. |
| **M** (mute) | off, on | off | Silences the output. |
| **Ø** (polarity) | normal, inverted | normal | Flips the output's polarity (a reversed cable, a mic facing the other way). |
| **Mono** | off, on | off | On a linked output pair whose source is a linked input pair: both sides get the mono sum. |
| **Link** | off, on | off | Makes the output pair a stereo pair, with one set of controls (the left output's). The right output shows "linked with ...". |

### How pairs and mono sums work

| Source | Output | What comes out |
| --- | --- | --- |
| a single input | a single output | that input |
| a single input | a linked pair | that input on both sides |
| a linked input pair | a linked pair | left to left, right to right (or the mono sum on both, with **Mono**) |
| a linked input pair | a single output | the mono sum |

The **mono sum** is the average of left and right, as RKUtility's Width at 0%: centred sounds keep their level, a sound on one side only comes out 6 dB lower, and anything out of phase between the sides cancels (as it would on a mono PA).

Example: a stereo synth bass on inputs 5/6 (linked) goes **In 5/6 → Out 5/6** (linked) in stereo to the in-ear mix, and **In 5/6 → Out 3** in mono to the front of house.

Note that RKRemap **routes**: each output plays one source. Mixing several sources into one output (an in-ear mix of vocal, violin and synth) is a mixer's job; RKRemap feeds the mixer's channels.

## Switching while playing

Every control is a host parameter, so a Gig Performer widget or a rackspace variation can change a route, mute an output or flip a source live. A route change crossfades over 10 ms, and trim, mute and polarity glide, so nothing clicks. A route that isn't changing is never touched.

## Views (bottom row)

**Inputs shown** and **Outputs shown** (1 to 64 each) set how many rows the window lists. They only change the view: hidden channels keep routing. If a hidden output still carries an input that is shown, an orange note says so ("3 hidden outputs carry shown inputs"), so nothing plays unnoticed. Hidden outputs fed by hidden inputs (such as the straight-through default beyond your channels) aren't counted.

Names and views are saved with the session **and in presets**, so a preset is a whole patch: routes, names and views.

## Bypass

As in every RKFX plugin, **Bypass passes each input to the output with the same number** (In *n* → Out *n*). In a patch bay that is a different routing, not silence: **don't map Bypass to a footswitch or a widget in a live rig.**

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your rig and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Straight 16** | In 1-16 to Out 1-16, 16 shown each way |
| **Stereo Pairs 16** | Inputs and outputs linked in pairs, 1/2 to 1/2 ... 15/16 to 15/16 |
| **IEM and FOH** | An example rig with names: Vocal (In 1) to FOH Vocal (Out 1) and IEM Vocal (Out 7); Violin (In 2) to FOH Violin (Out 2) and IEM Violin (Out 8); Synth Bass (In 5/6, linked) in stereo to IEM Synth (Out 5/6) and in mono to FOH Synth Bass (Out 3) |
| **Recorder Split** | In 1-8 to Out 1-8 ("PA") and also to Out 9-16 ("Rec"), for a recorder |

## Notes

- RKRemap isn't an RKBoard pedal: the board's chain is stereo.
- With up to 64 outputs, the outputs list scrolls; the signal lights of hidden rows aren't drawn.
