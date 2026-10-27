# RKFX

A suite of separate, lightweight audio effect plugins by RKMusic, built for live performance in [Gig Performer](https://gigperformer.com/) on any source: voice, guitar, keys, drums, violin and more.

Goals, in priority order:

1. **Zero latency.** No lookahead, no FFT, no block-based processing. The only exception is the optional oversampling in RKSaturate and RKDistort (4 or 5 samples, about 0.1 ms, reported to the host).
2. **Lightweight.** Minimal CPU and memory, fast loading, no heavy graphics.
3. **Live-safe.** No clicks, no runaway feedback, no broken sessions after updates.
4. **Consistent.** Same look, same controls, same behavior in every plugin.

Each effect is its own plugin, so a rackspace loads only what it uses.

## Plugins

| Plugin | Effect | Status |
| --- | --- | --- |
| [RKFreqShift](docs/manuals/RKFreqShift.md) | Frequency shifter, four independent shifts with feedback | Available |
| [RKFilter](docs/manuals/RKFilter.md) | Low-pass, high-pass, band-pass and auto-wah, with a tempo-synced LFO | Available |
| [RKParaEQ](docs/manuals/RKParaEQ.md) | Seven-band parametric EQ with response curve | Available |
| [RKGraphEQ](docs/manuals/RKGraphEQ.md) | Graphic EQ (10, 15 or 31 bands) with response curve | Available |
| RKVocode | Channel vocoder | Planned |
| [RKCompress](docs/manuals/RKCompress.md) | Compressor with external sidechain, transfer curve and gain reduction meter | Available |
| [RKGate](docs/manuals/RKGate.md) | Gate and expander with external sidechain and key filter | Available |
| RKLimit | Limiter | Planned |
| RKTransient | Transient shaper | Planned |
| RKDeEss | De-esser | Planned |
| RKSwell | Auto swell | Planned |
| [RKSaturate](docs/manuals/RKSaturate.md) | Tape, tube and transformer saturation | Available |
| [RKDistort](docs/manuals/RKDistort.md) | Overdrive, distortion and fuzz with cab filter | Available |
| [RKChorus](docs/manuals/RKChorus.md) | Chorus, 1 to 4 voices per side | Available |
| [RKDouble](docs/manuals/RKDouble.md) | Artificial double tracking with random drift | Available |
| [RKFlange](docs/manuals/RKFlange.md) | Flanger with positive and negative feedback | Available |
| [RKPhase](docs/manuals/RKPhase.md) | Phaser, 2 to 12 stages, LFO and envelope | Available |
| [RKTremolo](docs/manuals/RKTremolo.md) | Tremolo, auto-pan and harmonic tremolo | Available |
| RKRotary | Rotary speaker | Planned |
| RKVibrato | Vibrato | Planned |
| [RKDelay](docs/manuals/RKDelay.md) | Digital, analog and tape delay with ping-pong and ducking | Available |
| RKVerb | Reverb | Planned |
| RKUtility | Gain, pan, width, mono, polarity, mid/side | Planned |

## Documentation

- [Common Controls](docs/manuals/Common%20Controls.md): layout, knobs, Dry/Wet, Output, Bypass, presets and Gig Performer use, shared by every plugin.
- One manual per plugin in [docs/manuals](docs/manuals).

## Formats and requirements

- macOS, AU and VST3.
- Mono and stereo; sample rates 44.1 to 192 kHz.
- RKCompress and RKGate have a second stereo input (Sidechain) for an external key signal.
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
                                                               #   Dly1, Sat1, Dst1, ...)
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 \
    --validate-in-process ~/Library/Audio/Plug-Ins/VST3/RKFreqShift.vst3
```

Every release build of every plugin must pass the tests, auval and [pluginval](https://github.com/Tracktion/pluginval) at strictness 10. Benchmark results can be logged with `RKFXBench --record benchmarks/history.csv`.

## Repository layout

```
CMakeLists.txt     top level: fetches JUCE, adds core, plugins and tests
core/              shared code for every plugin
  dsp/             filters (state-variable, cascades, matched bell/shelf),
                   crossover, tone tilt, Hilbert transformer, oscillators, LFO,
                   random modulator, envelope follower, modulated delay line,
                   2x/4x oversampler,
                   dynamics (level detection, gain computer, gain smoothing)
  safety/          smoothing, DC blocker, NaN guard, soft clipper
  gui/             look and feel, knob, fader, switches, response curve,
                   gain reduction meter
  plugin/          base processor and editor, parameters, presets, output stage
plugins/<Name>/    one folder per plugin (presets/ holds its factory presets)
tests/             DSP and format tests, CPU benchmark
docs/manuals/      user manuals
benchmarks/        recorded CPU benchmark results
```

## License

GPL-3.0, see [LICENSE](LICENSE). Built with [JUCE](https://juce.com/), used under the GPL.
