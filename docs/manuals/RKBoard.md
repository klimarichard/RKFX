# RKBoard

**Pedalboard** · every RKFX effect built in · plugin (AU, VST3) and standalone app

RKBoard puts RKFX effects on one board, like pedals (as many as you like, up to 64): in a chain, or split into two parallel paths that are mixed back together. Each pedal is exactly the RKFX plugin of the same name (same sound, same controls, same presets), so a sound shaped in a single plugin moves to the board unchanged. Use it inside Gig Performer for gigs, or as a standalone app to shape your tone at home; board presets are the same files in both.

For knobs, presets and the standard controls inside each pedal, see [Common Controls](Common%20Controls.md).

## The window

- **Header (top):** the board preset (name, menu, Save, Save as), the **Assign** button, and the board's **In** and **Out** levels (-24 to +12 dB).
- **Scene bar:** scenes 1 to 16 (see Scenes).
- **The board:** the pedals, flowing left to right. A board too long for one row continues on the next, like text, with a cable from the end of one row to the start of the next; the window grows by a row. The split, both paths and the merge always stay together on one row.
- **Below the board:** the controls of the selected pedal (its own plugin window), the split's or merge's settings, or the assignable parameters.

## The board

```
In -> [pre] -> SPLIT -+- [path A] -+- MERGE -> [post] -> Out
                      +- [path B] -+
```

Pedals can go anywhere: before the split (**pre**), on **path A** or **path B**, or after the merge (**post**). All of them can also form one plain chain. The split and merge only take part while path A or path B holds a pedal; with both paths empty, the board is a simple chain and the signal passes the split untouched.

Each pedal is a tile in its family color, with its name, an LED and a footswitch:

| Action | Result |
| --- | --- |
| Click the tile | Shows the pedal's controls below the board |
| Click the footswitch | Bypass on / off (the LED is off while bypassed) |
| Drag the tile | Moves the pedal: within its section, or to another section or path |
| Right-click the tile | Duplicate (a copy with the same settings, right after it), Remove, Trails (delay and reverb), and **Assign On/Off to** (see Assignable parameters) |
| Click a **+** | Adds a pedal at the end of that section, from a menu grouped by family |
| Click SPLIT or MERGE | Shows its settings below the board |

Pedals only use processing power while switched on.

## Split and merge

Click the **SPLIT** node:

| Split mode | What it does |
| --- | --- |
| **Full** | Both paths get the complete signal. With one path empty this is a wet/dry rig: the empty path is the dry signal. |
| **Frequency** | Lows go to path A, highs to path B, split at the **Crossover** (50 Hz to 5 kHz, 24 dB/octave). With nothing changed on the paths they add back to the original sound. Distort only the highs and keep the bass clean, for example. |
| **Stereo** | The left channel goes to path A, the right channel to path B. Choosing Stereo pans path A left and path B right in the merge (and back to the centre when you leave Stereo), so the stereo image comes out as it went in. |

Click the **MERGE** node:

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

- whether each pedal is on or bypassed, and
- the value of every **scene-controlled** knob. Right-click a knob and tick **Scene controlled** (up to 48 knobs per board). All other knobs are the same in every scene.

Click a scene button to switch. Pedals switch on and off at once; scene-controlled knobs glide to their new values, so nothing clicks. Changes you make while a scene is selected are stored in that scene. Right-click a scene button to copy it to another scene, then change what should differ.

Scenes can also be switched by the host parameter **Scene** (1 to 16, for a Gig Performer widget), and in the standalone app by MIDI program changes 0 to 15.

## Trails

Delay and reverb pedals have a **Trails** option (right-click the tile). With trails on, bypassing the pedal stops it taking in new sound, but the echoes or reverb already playing ring out naturally, also when a scene switches the pedal off. With trails off, the sound stops as usual.

## Standalone app

The standalone app has a few things a host would otherwise provide. In the plugin they are hidden: Gig Performer handles controllers and tempo there.

**Audio and MIDI settings** (Options at the top of the window) choose your audio interface and MIDI inputs. Use a small buffer size (64 or 128 samples) for a low delay through the interface.

### Tempo

The **BPM** field (20 to 300) and **Tap** sit at the left of the scene bar. Tap at least twice; RKBoard averages your last few taps, and a pause of two seconds starts over. Tempo-synced delays and modulation follow this tempo. Board presets store their tempo and apply it when loaded.

Right-click **Tap** and tick **Follow MIDI clock** to take the tempo from incoming MIDI clock instead (a drum machine or a sequencer); the BPM field then shows it, dimmed.

### MIDI learn

Footswitches and expression pedals connect through MIDI learn. Right-click what you want to control, choose **MIDI learn**, then press the switch or move the pedal:

| What | Where to right-click | How it reacts |
| --- | --- | --- |
| A pedal on / off | its tile | a switch |
| An assignable parameter | its row in the Assign view | an expression pedal: smoothed, so it never zippers |
| Scene selector | a scene button | the controller's value picks the scene (0 = scene 1, up to 15 = scene 16) |
| Tap tempo | **Tap** | a switch |
| Tuner | **Tuner** | a switch |

For switches, the same menu chooses **toggle** (each press switches on or off) or **momentary** (on while held). **Clear MIDI** removes the mapping. MIDI program changes 0 to 15 always select scenes 1 to 16.

Mappings belong to your rig, not to a sound: they stay the same whatever board you load, and are kept in `~/Library/Application Support/RKFX/RKBoard/midi-map.json`.

### Tuner

Click **Tuner** (or a learned footswitch) to show the tuner: the note, its octave, and how many cents it is off (the marker turns green within 3 cents). Set the **Reference** pitch (430 to 450 Hz, A4) and whether the output is **muted while tuning**. The tuner listens to a copy of the input, so it never adds delay or changes the sound.

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
| **Scenes Starter** | Compress, Distort, Delay, Verb. Scene 1: clean (Distort and Delay off); scene 2: crunch (Distort on); scene 3: lead (Distort and Delay on) |

## Board presets

Board presets live in `Presets/RKBoard` in the RKFX preset library, shared by the plugin and the standalone app. A board preset holds every pedal (as that plugin's own preset), the routing, the assignments and the scenes. A gig saved in Gig Performer is self-contained: it never depends on the preset file.

Each pedal also keeps its own preset menu, so you can load any RKFX preset or factory preset into a pedal on the board, or save a pedal's settings as a preset for the single plugin.

## Host parameters

| Parameters | What they do |
| --- | --- |
| **Input**, **Output** | The board's In and Out levels. |
| **Assign 1** to **Assign 32** | Knobs and on/off switches, see Assignable parameters. An assignment belongs to a pedal, not a place: it follows the pedal when you move it. |
| **Scene** | 1 to 16. |

## Notes

- RKBoard adds no latency, except a few samples when a drive pedal uses oversampling (as in the single plugins).
- To use the vocoder pedal, send MIDI into RKBoard: its carrier synth plays from it.
- Gig Performer tip: map an expression pedal to an Assign parameter driving a Wah or Volume pedal, and footswitches to Assign parameters switching pedals on and off, or to the Scene parameter.
