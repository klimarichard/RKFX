# RKFX

A suite of separate, lightweight audio effect plugins by RKMusic, built for live performance in [Gig Performer](https://gigperformer.com/) on any source: voice, guitar, keys, drums, violin and more.

Goals, in priority order:

1. **Zero latency.** No lookahead, no FFT, no block-based processing. The exceptions: the optional oversampling in RKSaturate, RKDistort, RKPreamp, RKLimit and RKRotary (4 or 5 samples, about 0.1 ms, reported to the host), and RKPolyPitch's Whammy mode, where the shifted sound replaces yours and trails it by 5 to 27 ms (shown on screen; its Harmony and Detune modes leave your sound undelayed).
2. **Lightweight.** Minimal CPU and memory, fast loading, no heavy graphics.
3. **Live-safe.** No clicks, no runaway feedback, no broken sessions after updates.
4. **Consistent.** Same look, same controls, same behavior in every plugin.

Each effect is its own plugin, so a rackspace loads only what it uses. **RKBoard** puts them all together on one pedalboard, **RKMix** is a 32-channel mixing console with them as inserts, and **RKPlay** is a music player for intermissions and receptions; all three work as a plugin or a standalone app.

## Plugins

| Plugin | Effect | Status |
| --- | --- | --- |
| [RKFreqShift](docs/manuals/RKFreqShift.md) | Frequency shifter, four independent shifts with feedback | Available |
| [RKFilter](docs/manuals/RKFilter.md) | Low-pass, high-pass, band-pass and auto-wah, with a tempo-synced LFO | Available |
| [RKParaEQ](docs/manuals/RKParaEQ.md) | Seven-band parametric EQ with response curve; any gain band can be dynamic (cut when loud, lift when soft) | Available |
| [RKGraphEQ](docs/manuals/RKGraphEQ.md) | Graphic EQ (10, 15 or 31 bands) with response curve | Available |
| [RKVocode](docs/manuals/RKVocode.md) | Channel vocoder with sidechain or MIDI-played carrier | Available |
| [RKCompress](docs/manuals/RKCompress.md) | Compressor with external sidechain, transfer curve and gain reduction meter | Available |
| [RKMultiComp](docs/manuals/RKMultiComp.md) | Multiband compressor, 3 or 4 bands, with phase-matched Dry/Wet and a band display with draggable thresholds and crossovers | Available |
| [RKRide](docs/manuals/RKRide.md) | Automatic level rider: keeps a voice or instrument at a steady level (or a set amount above the music), holds in pauses | Available |
| [RKGate](docs/manuals/RKGate.md) | Gate and expander with external sidechain and key filter | Available |
| [RKLimit](docs/manuals/RKLimit.md) | Peak limiter with absolute ceiling | Available |
| [RKTransient](docs/manuals/RKTransient.md) | Level-independent transient shaper | Available |
| [RKDeEss](docs/manuals/RKDeEss.md) | Split-band or wideband de-esser | Available |
| [RKSwell](docs/manuals/RKSwell.md) | Auto swell (bowed, violin-like attacks) | Available |
| [RKSaturate](docs/manuals/RKSaturate.md) | Tape, tube and transformer saturation | Available |
| [RKDistort](docs/manuals/RKDistort.md) | Overdrive, distortion and fuzz with cab filter | Available |
| [RKChorus](docs/manuals/RKChorus.md) | Chorus, 1 to 4 voices per side | Available |
| [RKDouble](docs/manuals/RKDouble.md) | Artificial double tracking with random drift | Available |
| [RKFlange](docs/manuals/RKFlange.md) | Flanger with positive and negative feedback | Available |
| [RKPhase](docs/manuals/RKPhase.md) | Phaser, 2 to 12 stages, LFO and envelope, plus a Uni-Vibe mode | Available |
| [RKTremolo](docs/manuals/RKTremolo.md) | Tremolo, auto-pan and harmonic tremolo | Available |
| [RKRotary](docs/manuals/RKRotary.md) | Rotary speaker with horn, drum and drive | Available |
| [RKVibrato](docs/manuals/RKVibrato.md) | Pitch vibrato with rise | Available |
| [RKDelay](docs/manuals/RKDelay.md) | Digital, analog and tape delay with ping-pong, ducking and shimmer (repeats that climb in pitch) | Available |
| [RKVerb](docs/manuals/RKVerb.md) | Room, hall and plate reverb with shimmer and freeze | Available |
| [RKUtility](docs/manuals/RKUtility.md) | Gain, pan, width, bass mono, polarity, mid/side, mic alignment delay | Available |
| [RKRemap](docs/manuals/RKRemap.md) | Patch bay: 64 inputs to 64 outputs with names, stereo and mid/side pairs, mono sums, trim, mute and polarity, for straight cables in Gig Performer (about 0.1% of a CPU core) | Available |
| [RKOctave](docs/manuals/RKOctave.md) | Analog-style octave divider, one and two octaves down | Available |
| [RKAcoustic](docs/manuals/RKAcoustic.md) | Preamp for piezo pickups: body, quack cut, EQ, notch, phase, compression | Available |
| [RKPreamp](docs/manuals/RKPreamp.md) | Guitar amp preamp: four voicings, amp-style tone stack, presence, bright, cab | Available |
| [RKSynth](docs/manuals/RKSynth.md) | Monophonic synth played by your bass, guitar or violin: saw, square, triangle, sub, resonant filter envelope | Available |
| [RKRingMod](docs/manuals/RKRingMod.md) | Ring modulator: free, pitch-tracked or tempo-synced carrier, with LFO | Available |
| [RKLoop](docs/manuals/RKLoop.md) | Live looper, eight loops as song parts or layers, synced to Loop 1: overdub, undo/redo, reverse, half speed, footswitch control | Available |
| [RKHold](docs/manuals/RKHold.md) | Note freeze: a footswitch holds the note you're playing as a drone (or a chord, layer by layer), an octave down or up if you like | Available |
| [RKPolyPitch](docs/manuals/RKPolyPitch.md) | Polyphonic pitch shifter, whammy style: pedal sweeps, momentary jumps, harmony and detune, chords included (Whammy mode delays your sound by a few ms, stated on screen) | Available |
| [RKVoice](docs/manuals/RKVoice.md) | Harmonizer: up to four harmony voices on a voice or instrument, placed by a held MIDI chord (1st, 2nd, 3rd above or below) or a key and scale; the dry signal untouched | Available |

### MIDI effects

The MIDI family: MIDI effects (MIDI in, MIDI out, no audio) for keyboards and MIDI controllers in front of an instrument, and two plugins that turn audio into MIDI:

| Plugin | Effect | Status |
| --- | --- | --- |
| [RKHarmonize](docs/manuals/RKHarmonize.md) | Harmonizer: up to two harmony notes in a key and scale | Available |
| [RKArpeggiate](docs/manuals/RKArpeggiate.md) | Tempo-synced arpeggiator with latch, swing and octaves | Available |
| [RKStrum](docs/manuals/RKStrum.md) | Strummer: chords spread out like a guitar strum, down, up or alternating | Available |
| [RKSustain](docs/manuals/RKSustain.md) | Sustain, sostenuto, hold until the next chord, and latch, from a pedal or a footswitch | Available |
| [RKConduct](docs/manuals/RKConduct.md) | MIDI file player conducted by taps: tap the beats (or bars, in any time signature), the notes in between follow your tempo | Available |
| [RKEcho](docs/manuals/RKEcho.md) | MIDI echo: repeats in time that can climb (fifths, octaves, thirds in a key) and go to another instrument or bounce between two | Available |
| [RKFollow](docs/manuals/RKFollow.md) | Pitch and expression to MIDI: a violin, voice or any single line plays a synth, with pitch bend (or MPE) and your dynamics as a controller; the audio passes through untouched | Available |
| [RKTrigger](docs/manuals/RKTrigger.md) | Drum trigger: hits on a piezo, drum mic or instrument body to MIDI notes, one or two triggers, with crosstalk rejection | Available |

### RKBoard

[RKBoard](docs/manuals/RKBoard.md) is a pedalboard holding the plugins above as pedals, as many as you like (each one the real plugin, with its presets), plus board-only Wah and Volume pedals. Pedals run in a chain with up to four splits, each into two parallel paths (full, frequency or stereo split) that are mixed back together. It has 32 assignable host parameters for widgets, expression pedals and footswitches (knobs or pedal on/off), 16 scenes, and trails for delays and reverbs. As a standalone app it adds tap tempo and MIDI clock, MIDI learn for footswitches and expression pedals, and a tuner. Board presets are shared by the plugin and the app. Its app icon is the RKMusic name strip (RK over a warm white band).

### RKMix

[RKMix](docs/manuals/RKMix.md) is a live mixing console: 32 inputs (linkable into stereo pairs) and 32 outputs. Each channel has a full strip (trim, polarity, delay, high-pass, gate and compressor with key inputs, 4-band EQ) and 2 insert slots for any RKFX effect; 16 buses are Group, Aux (in-ear and monitor mixes, each send at its own tap point) or FX returns, each with an EQ, glue compressor, limiter and 4 inserts; the stereo main and its mono sum have the same. Channels also have direct outs at any tap point (for a recorder). It has solo (cue PFL/AFL or in place), talkback, a sine and pink-noise oscillator, MIDI out from RKTrigger and RKFollow inserts, 16 scenes, 32 assignable host parameters, two user pages of chosen strips, and latency alignment across every path. As a standalone app it adds tap tempo and MIDI clock, MIDI learn and program-change scene recall.

### RKPlay

[RKPlay](docs/manuals/RKPlay.md) is a music player for intermissions, wedding receptions and background music, mainly as a standalone app. Up to 8 playlists (in order or shuffled, linked to a folder or an .m3u8), a queue for requests, crossfades that skip silence and long fade-outs, and End at (stop at a time of day). Every song is measured once (EBU R128) and played at the same loudness (-18 LUFS by default) by gain alone; quiet, dynamic songs can be lifted a little with a lookahead limiter. It ducks under an announcement mic or a button, has 8 pads for fanfares and entrances, and pre-listen on a cue output. As a standalone app it adds an OSC remote (TouchOSC), MIDI learn, keyboard shortcuts, crash recovery and a history of what played.

## Documentation

- [Common Controls](docs/manuals/Common%20Controls.md): layout, knobs, Dry/Wet, Output, Bypass, presets and Gig Performer use, shared by every plugin.
- One manual per plugin in [docs/manuals](docs/manuals).

## Formats and requirements

- macOS, AU and VST3; RKBoard, RKMix and RKPlay also as standalone apps. RKMix has 32 inputs and 32 outputs and MIDI out (an AU music effect, `aumf Mix1 RKMu`). The MIDI effects are AU MIDI processors and VST3 effects with MIDI in and out; RKFollow and RKTrigger are AU music effects and VST3 effects with audio in and out and MIDI out. RKPlay is an AU music effect (`aumf Ply1 RKMu`) with a stereo input (only listened to, for ducking) and two stereo outputs (main and cue).
- Mono and stereo (RKRemap: 64 channels each way); sample rates 44.1 to 192 kHz.
- Note names use middle C (MIDI note 60) = C3, as Yamaha instruments and Cubase do.
- RKCompress, RKGate and RKVocode have a second stereo input (Sidechain) for an external key or carrier signal.
- RKVocode also accepts MIDI (to play its carrier), so it registers as a music effect (AU type `aumf`).
- Presets are plain JSON files in `~/Music/RKMusic Plugins/RKFX/Presets/<Plugin>/`.
- Every plugin comes with factory presets (the starting points from its manual), built in and shown under **Factory** in the preset menu. Their source files are in `plugins/<Plugin>/presets/` and are compiled in automatically.

## Building

Requirements: macOS with the Xcode Command Line Tools, and CMake 3.22 or newer (for example `brew install cmake`). JUCE 8.0.13 is downloaded automatically on the first configure.

```sh
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

Builds install the AU and VST3 into `~/Library/Audio/Plug-Ins` automatically. Restart the host to pick up a rebuilt plugin.

### Tests and validation

```sh
ctest --test-dir build --output-on-failure                     # DSP, preset format and factory preset tests
./build/tests/RKFXBench_artefacts/Release/RKFXBench            # CPU benchmark, 48 kHz / 64 samples
auval -v aufx Fsh1 RKMu                                        # AU validation (per plugin code:
                                                               #   Fsh1, Flt1, Peq1, Geq1, Cmp1, Gat1,
                                                               #   Trm1, Chr1, Dbl1, Flg1, Phs1,
                                                               #   Dly1, Sat1, Dst1, Vrb1, Lim1, Trn1,
                                                               #   Des1, Swl1, Rot1, Vib1, Utl1;
                                                               #   RKVocode: auval -v aumf Voc1 RKMu,
                                                               #   RKMix: auval -v aumf Mix1 RKMu,
                                                               #   RKPlay: auval -v aumf Ply1 RKMu)
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 \
    --validate-in-process ~/Library/Audio/Plug-Ins/VST3/RKFreqShift.vst3
```

Every release build of every plugin must pass the tests, auval and [pluginval](https://github.com/Tracktion/pluginval) at strictness 10. Benchmark results can be logged with `RKFXBench --record benchmarks/history.csv`.

To check a GUI without a host, `cmake --build build --target RKFXSnapshots` builds a snapshot tool per plugin; `build/tools/RKFXSnapshot_<Name>_artefacts/Release/RKFXSnapshot_<Name> out.png [scale]` saves that plugin's window as a PNG.

`cmake --build build --target RKIconRender` builds the icon renderer for the RKMusic plugins (RKModular, RKSampler, RKTracks, RKBoard, RKMix, RKPlay): `build/tools/RKIconRender_artefacts/Release/RKIconRender tools/icons/fonts <out dir>` writes each plugin's `icon_big.png` and `icon_small.png`, to copy into its resources folder (see `tools/icons/RenderIcons.cpp`).

## Repository layout

```
CMakeLists.txt     top level: fetches JUCE, adds core, plugins, tests and tools
core/              shared code for every plugin
  dsp/             filters (state-variable, cascades, matched bell/shelf),
                   crossover, Linkwitz-Riley split, tone tilt, Hilbert transformer, oscillators, LFO,
                   random modulator, envelope follower, modulated delay line,
                   2x/4x oversampler,
                   dynamics (level detection, gain computer, gain smoothing),
                   peak limiter, loudness meter (EBU R128, true peak)
  safety/          smoothing, DC blocker, NaN guard, soft clipper
  gui/             rk_gui, the RKMusic look: palette, look and feel, knob,
                   fader, switches, response curve, gain reduction meter
                   (standalone target, also used by RKTracks)
  plugin/          base processor and editor, parameters, presets, output stage
plugins/<Name>/    one folder per plugin (presets/ holds its factory presets)
host/              the pedal host shared by RKBoard and RKMix: every plugin's
                   processor as a pedal or insert, the Wah and Volume pedals,
                   MIDI learn, tap tempo and MIDI clock
plugins/RKBoard/   the pedalboard
plugins/RKMix/     the mixing console (engine, strip, buses, inserts, scenes)
plugins/RKPlay/    the music player (streaming engine, analysis, shows, remote)
tests/             DSP and format tests, CPU benchmark
tools/             snapshot tool: renders a plugin's window to PNG; icons/: the RKMusic icon renderer and its fonts
docs/manuals/      user manuals
docs/design/       designs written before building (RKVoice, RKMix, RKPlay, ...)
benchmarks/        recorded CPU benchmark results
```

## License

GPL-3.0, see [LICENSE](LICENSE). Built with [JUCE](https://juce.com/), used under the GPL.
