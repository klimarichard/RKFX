# RKFX

A suite of separate, lightweight audio effect plugins by RKMusic, built for live performance in [Gig Performer](https://gigperformer.com/) on any source: voice, guitar, keys, drums, violin and more.

Goals, in priority order:

1. **Zero latency.** No lookahead, no FFT, no block-based processing. The only exception is the optional oversampling in RKSaturate, RKDistort, RKPreamp, RKLimit and RKRotary (4 or 5 samples, about 0.1 ms, reported to the host).
2. **Lightweight.** Minimal CPU and memory, fast loading, no heavy graphics.
3. **Live-safe.** No clicks, no runaway feedback, no broken sessions after updates.
4. **Consistent.** Same look, same controls, same behavior in every plugin.

Each effect is its own plugin, so a rackspace loads only what it uses. **RKBoard** puts them all together on one pedalboard, as a plugin or a standalone app.

## Plugins

| Plugin | Effect | Status |
| --- | --- | --- |
| [RKFreqShift](docs/manuals/RKFreqShift.md) | Frequency shifter, four independent shifts with feedback | Available |
| [RKFilter](docs/manuals/RKFilter.md) | Low-pass, high-pass, band-pass and auto-wah, with a tempo-synced LFO | Available |
| [RKParaEQ](docs/manuals/RKParaEQ.md) | Seven-band parametric EQ with response curve | Available |
| [RKGraphEQ](docs/manuals/RKGraphEQ.md) | Graphic EQ (10, 15 or 31 bands) with response curve | Available |
| [RKVocode](docs/manuals/RKVocode.md) | Channel vocoder with sidechain or MIDI-played carrier | Available |
| [RKCompress](docs/manuals/RKCompress.md) | Compressor with external sidechain, transfer curve and gain reduction meter | Available |
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
| [RKPhase](docs/manuals/RKPhase.md) | Phaser, 2 to 12 stages, LFO and envelope | Available |
| [RKTremolo](docs/manuals/RKTremolo.md) | Tremolo, auto-pan and harmonic tremolo | Available |
| [RKRotary](docs/manuals/RKRotary.md) | Rotary speaker with horn, drum and drive | Available |
| [RKVibrato](docs/manuals/RKVibrato.md) | Pitch vibrato with rise | Available |
| [RKDelay](docs/manuals/RKDelay.md) | Digital, analog and tape delay with ping-pong and ducking | Available |
| [RKVerb](docs/manuals/RKVerb.md) | Room, hall and plate reverb with freeze | Available |
| [RKUtility](docs/manuals/RKUtility.md) | Gain, pan, width, bass mono, polarity, mid/side, mic alignment delay | Available |
| [RKOctave](docs/manuals/RKOctave.md) | Analog-style octave divider, one and two octaves down | Available |
| [RKAcoustic](docs/manuals/RKAcoustic.md) | Preamp for piezo pickups: body, quack cut, EQ, notch, phase, compression | Available |
| [RKPreamp](docs/manuals/RKPreamp.md) | Guitar amp preamp: four voicings, amp-style tone stack, presence, bright, cab | Available |

### RKBoard

[RKBoard](docs/manuals/RKBoard.md) is a pedalboard holding the plugins above as pedals, as many as you like (each one the real plugin, with its presets), plus board-only Wah and Volume pedals. Pedals run in a chain or split into two parallel paths (full, frequency or stereo split) that are mixed back together. It has 32 assignable host parameters for widgets, expression pedals and footswitches (knobs or pedal on/off), 16 scenes, and trails for delays and reverbs. As a standalone app it adds tap tempo and MIDI clock, MIDI learn for footswitches and expression pedals, and a tuner. Board presets are shared by the plugin and the app.

## Documentation

- [Common Controls](docs/manuals/Common%20Controls.md): layout, knobs, Dry/Wet, Output, Bypass, presets and Gig Performer use, shared by every plugin.
- One manual per plugin in [docs/manuals](docs/manuals).

## Formats and requirements

- macOS, AU and VST3; RKBoard also as a standalone app.
- Mono and stereo; sample rates 44.1 to 192 kHz.
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
                                                               #   RKVocode: auval -v aumf Voc1 RKMu)
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 \
    --validate-in-process ~/Library/Audio/Plug-Ins/VST3/RKFreqShift.vst3
```

Every release build of every plugin must pass the tests, auval and [pluginval](https://github.com/Tracktion/pluginval) at strictness 10. Benchmark results can be logged with `RKFXBench --record benchmarks/history.csv`.

To check a GUI without a host, `cmake --build build --target RKFXSnapshots` builds a snapshot tool per plugin; `build/tools/RKFXSnapshot_<Name>_artefacts/Release/RKFXSnapshot_<Name> out.png [scale]` saves that plugin's window as a PNG.

## Repository layout

```
CMakeLists.txt     top level: fetches JUCE, adds core, plugins, tests and tools
core/              shared code for every plugin
  dsp/             filters (state-variable, cascades, matched bell/shelf),
                   crossover, Linkwitz-Riley split, tone tilt, Hilbert transformer, oscillators, LFO,
                   random modulator, envelope follower, modulated delay line,
                   2x/4x oversampler,
                   dynamics (level detection, gain computer, gain smoothing)
  safety/          smoothing, DC blocker, NaN guard, soft clipper
  gui/             rk_gui, the RKMusic look: palette, look and feel, knob,
                   fader, switches, response curve, gain reduction meter
                   (standalone target, also used by RKTracks)
  plugin/          base processor and editor, parameters, presets, output stage
plugins/<Name>/    one folder per plugin (presets/ holds its factory presets)
plugins/RKBoard/   the pedalboard: hosts every plugin's processor as a pedal
                   (pedals/ holds the board-only Wah and Volume)
tests/             DSP and format tests, CPU benchmark
tools/             snapshot tool: renders a plugin's window to PNG
docs/manuals/      user manuals
benchmarks/        recorded CPU benchmark results
```

## License

GPL-3.0, see [LICENSE](LICENSE). Built with [JUCE](https://juce.com/), used under the GPL.
