# RKFX: Common Controls

Every RKFX plugin shares the same layout, knob behavior, standard controls and preset system. This page explains them once; each plugin's own manual covers only what is specific to it.

RKFX plugins are built for live use: they add no latency (except where a manual says so), they never click when you change a setting, and an update never breaks a saved Gig Performer rackspace.

## Window layout

- **Header (top):** the plugin name on the left, the preset controls on the right.
- **Main area:** the plugin's own controls, grouped in sections.
- **Bottom row:** global controls for the plugin on the left, and the standard controls (Dry/Wet, Output, Bypass) on the right, always in the same spot on every plugin.

The window can be resized by dragging its bottom-right corner; it keeps its shape and stays sharp at any size.

Each plugin family has its own accent color, used for switches that are on and as a faint tint in the background and panels, so you can tell plugins apart at a glance:

| Family | Color |
| --- | --- |
| Filter and EQ | Violet |
| Dynamics | Yellow |
| Drive | Red |
| Modulation | Light blue |
| Delay | Green |
| Reverb | Orange |
| Utility | Light gray |

## Knobs

| Action | Result |
| --- | --- |
| Drag up or down | Changes the value |
| Hold Shift while dragging | Fine adjustment (10 times slower) |
| Mouse wheel | Small steps (hold Shift for even smaller ones) |
| Double-click the knob | Back to the default value |
| Click the value under the knob | Type an exact value, then press Return (Escape cancels) |

When typing a value you can include units: `1.2k` or `1.2 kHz` for 1200 Hz, `1.5 s` for 1500 ms.

A dimmed knob belongs to a section that is currently switched off or has no effect in the current mode. It still works, so you can prepare a setting before switching the section on.

## Switches

- **On/off switches** light up in the plugin's color when on. Click to toggle.
- **Segmented switches** (for example Fine / Coarse) show the selected option in the plugin's color. Click an option to select it.

## Standard controls

| Control | Range | What it does |
| --- | --- | --- |
| **Dry/Wet** | 0 to 100% | Blends the original signal with the effect. 0% is only the original, 100% is only the effect. Uses an equal-power blend, so the middle doesn't dip in level. Only on plugins where blending makes musical sense (not on EQs, gates, limiters and similar). |
| **Output** | -24 to +12 dB, default 0 dB | Level after the effect. |
| **Bypass** | Off, on | Passes the original signal through untouched. Switching crossfades over about 10 ms, so it never clicks. While bypassed, the plugin uses almost no CPU. When switched back on, the effect starts fresh (no old echoes or tails). |

All changes glide over a few milliseconds, so turning a knob, moving a Gig Performer widget or switching a snapshot never produces zipper noise or clicks.

## Presets

The header shows the current preset name. A `*` after the name means you have changed something since the preset was loaded or saved.

- **Preset name button:** opens the preset menu:
  - **Factory:** the plugin's built-in presets, matching the starting points in its manual. They're part of the plugin, so they're always there and can't be changed or deleted; to keep a modified version, use **Save as**.
  - Your own presets, with sub-folders as sub-menus.
  - At the bottom:
    - **Reset to defaults:** every control back to its built-in default.
    - **Show presets folder:** opens the plugin's preset folder in Finder.
    - **Change library folder...:** moves where all RKFX plugins look for presets.
- **Save:** overwrites the current preset file, after asking. If the current settings don't come from a file of your own yet (for example a factory preset), works like Save as.
- **Save as:** saves the settings under a new name.

### Where presets live

```
~/Music/RKMusic Plugins/RKFX/
└── Presets/
    ├── RKFreqShift/
    │   ├── Default.json      ← loaded by every new instance, if present
    │   ├── Slow Phasing.json
    │   └── Vocals/           ← sub-folders show as sub-menus
    └── RKFilter/
        └── ...
```

- The library sits under Music rather than Documents, so iCloud's "Desktop & Documents" sync can never move your presets to the cloud.
- **Default.json:** save a preset with this name, and every new instance of that plugin starts with it.
- Preset files are plain, readable text (JSON) and can be copied, shared and backed up like any file. A preset holds only the settings that differ from the defaults.
- If a preset contains something the plugin doesn't understand (for example a value edited by hand), that value is skipped, the default is used instead, and a short note appears in the header. A preset never fails to load because of one bad value.

### Presets and Gig Performer

Gig Performer saves each plugin's complete settings inside the gig file, together with the preset name for display. A gig never depends on a preset file: moving, renaming or deleting preset files never changes a saved rackspace. Gig Performer's own rackspaces, variations and snapshots work as usual.

## Using RKFX in Gig Performer

Every control (including Bypass) is exposed as a parameter, so you can map it to panel widgets or MIDI controllers in Gig Performer and keep the plugin window closed during a show. Parameters are listed in the same order as on screen: the plugin's own controls first, then Dry/Wet, Output and Bypass.

## MIDI effects

The MIDI family (RKHarmonize, RKArpeggiate) works on MIDI, not audio: notes come in, notes go out, so they go between a MIDI input and an instrument in Gig Performer. They have the same header and presets, but no Dry/Wet and no Output, only **Bypass**: bypassed, MIDI passes through unchanged, and any note the effect was playing ends first, so nothing hangs.
