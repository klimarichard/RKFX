# RKPolyPitch: design

**Polyphonic pitch shifter, whammy style** · Modulation family · **the one exception to zero latency** (Whammy mode)

The name RKPolyPitch was chosen on purpose (an exception to the RK + verb pattern, like RKMultiComp). This is a design, not a manual: it says what the plugin does and how, so it can be built and reviewed against it.

## What it does

RKPolyPitch shifts the pitch of whatever you play, chords included, like a Whammy or a Boss XS-1:

- **Whammy:** your sound is replaced by the shifted one, swept with an expression pedal between two settings (unison to an octave up, a dive bomb two octaves down) or jumped with a footswitch.
- **Harmony:** a shifted voice added to your own sound (an octave down under a chord, a fifth above a melody).
- **Detune:** two voices a few cents either side of your sound, a thick, chorus-like doubling.

It works on chords as well as single notes (no pitch tracking), from violin to bass.

## The latency exception

RKFX's first rule is zero latency, and RKPolyPitch keeps it wherever it can:

- **Harmony and Detune:** your own sound passes through **untouched, with no latency**. The shifted voices trail it by a few milliseconds, as the harmony voices of RKVoice do. That reads as a second player, not as lag.
- **Whammy:** your sound is **replaced** by the shifted one, so the shift's delay is the delay of everything you hear. **This is the one place in RKFX where the sound you play comes out late** (apart from the 0.1 ms of optional oversampling elsewhere). The manual says so plainly, with the numbers below.

The delay depends on the instrument range, because the shifter's window must cover about two cycles of the lowest note (shorter windows make low notes grainy and warbly):

| Range | Lowest note | Window | Delay |
| --- | --- | --- | --- |
| Violin | G (196 Hz) | 14 ms | about 7 ms |
| Guitar | low E (82 Hz) | 28 ms | about 14 ms |
| Bass | low E (41 Hz) | 50 ms | about 25 ms |

The display shows the current delay. For comparison, 7 ms is like standing 2.4 m from an amp, which most players don't notice; 14 ms is noticeable for tight rhythm parts, fine for pedal sweeps and leads. RKPolyPitch doesn't report this delay to the host: in a live rig, delaying everything else to match would add latency to the whole rackspace.

## The shifter

The shared **`core/dsp/PitchShifter`** (the shimmer shifter: two pairs of Hann-faded taps with different window lengths, so no frequency is cancelled), with its windows set per Range. It gets one change: **the two window lengths become a setting** (`prepare (rate, windowMs, secondWindowMs)`), defaulting to today's 50 and 35 ms so RKVerb and RKDelay are unchanged. RKPolyPitch uses the Range window and 0.7 × it.

- **Any material:** no pitch tracking, so chords, double stops and noise all shift. Quality is best for shifts of up to an octave; two octaves (and dive bombs) get grainier, as on every pedal of this kind.
- **Glide:** the shift moves smoothly (the ratio is smoothed per sample), so pedal sweeps are continuous.
- **Stereo:** one shifter per channel, run in step, so the stereo image stays put.
- **Detune** uses two shifters per channel (+cents and −cents) and, as their ratios are near 1, a short window (10 ms for every Range): its voices trail by about 5 ms.

## The pedal

The shift is set by two positions and the **Pedal** between them, the way a Whammy's treadle works:

- **Heel** (−24 to +24 semitones, default 0) and **Toe** (−24 to +24, default +12).
- **Pedal** (0 to 100%, default 0%): map it to an expression pedal in Gig Performer or on RKBoard. Between heel and toe the shift moves in semitones (linearly), so the middle of an octave sweep is a tritone, like a Whammy.
- **Glide** (0 to 500 ms, default 30 ms): smooths the Pedal, so a stepped MIDI controller still sweeps smoothly.
- **Momentary** (off, on): a footswitch parameter. While on, the shift moves to Toe over **Rise** (0 to 2 s); released, back to the Pedal's position over **Fall** (0 to 2 s). This is the XS-1's momentary mode: hit the switch for an octave jump or a dive.
- **Fine** (−50 to +50 cents) on top, for slight detuning in Whammy and Harmony.

## Modes

| Mode | What you hear |
| --- | --- |
| Whammy (default) | Only the shifted sound (the latency exception) |
| Harmony | Your sound (untouched) + the shifted voice at **Voice Level** |
| Detune | Your sound (untouched) + two voices at ± **Detune** cents (2 to 30, default 10), at **Voice Level**, spread by **Width** |

- **Voice Level** (−60 to +6 dB, default −3 dB) in Harmony and Detune; **Width** (0 to 100%) spreads the Detune voices left and right; **Tone** (1 to 20 kHz low-pass) on the shifted sound, to soften the top of large upward shifts.
- Switching modes crossfades over 20 ms.

## Controls

| Control | ID | Range | Default |
| --- | --- | --- | --- |
| Mode | `mode` | Whammy, Harmony, Detune | Whammy |
| Range | `range` | Violin, Guitar, Bass | Guitar |
| Heel | `heel` | −24 to +24 st | 0 |
| Toe | `toe` | −24 to +24 st | +12 |
| Pedal | `pedal` | 0 to 100% | 0% |
| Glide | `glide` | 0 to 500 ms | 30 ms |
| Momentary | `momentary` | Off, on | Off |
| Rise | `rise` | 0 to 2 s | 50 ms |
| Fall | `fall` | 0 to 2 s | 150 ms |
| Fine | `fine` | −50 to +50 cents | 0 |
| Detune | `detune` | 2 to 30 cents | 10 |
| Voice Level | `voice_level` | −60 to +6 dB | −3 dB |
| Width | `width` | 0 to 100% | 60% |
| Tone | `tone` | 1 to 20 kHz | 20 kHz |

Plus **Output** and **Bypass**. No Dry/Wet: the modes are the mix (Whammy has no dry by design; Harmony and Detune have Voice Level).

## The window

- **Pedal display:** a treadle drawn from heel to toe with the current position, the shift now in semitones (large), the Heel and Toe values at its ends, and the **delay** of the current Range in Whammy mode ("Delay: about 14 ms"), or "your sound: no delay" in Harmony and Detune.
- **Sections:** Mode (Mode, Range), Pedal (Heel, Toe, Pedal, Glide), Momentary (switch, Rise, Fall), Voice (Fine, Detune, Voice Level, Width, Tone).

## Identity

- **AU code** `Ppt1`, bundle ID `com.rkmusic.rkpolypitch`.
- **Modulation family** (with RKOctave and RKVoice), accent **indigo blue `#5468D4`**.
- An **RKBoard** pedal ("PolyPitch"): Pedal to an expression pedal through an Assign slot, Momentary to a footswitch.

## Factory presets

| Preset | Idea |
| --- | --- |
| Octave Up Sweep | Whammy, Heel 0, Toe +12: the classic sweep |
| Dive Bomb | Whammy, Heel 0, Toe −24, Glide 80 ms |
| Octave Jump | Whammy, Momentary, Toe +12, Rise 30 ms, Fall 100 ms |
| Octave Down Harmony | Harmony, Heel −12 (Pedal 0), Voice Level −2 dB: a bass octave under chords |
| Fifth Above | Harmony, Heel +7, Voice Level −6 dB |
| Detune Double | Detune, 10 cents, Width 80% |
| Bass Octave Down | Harmony, Range Bass, Heel −12, Tone 2 kHz |

## CPU and safety

- Two tap pairs per shifter, one shifter per channel (two in Detune). Aim: under 0.3% of a core.
- No feedback paths. Pedal, glide and mode changes are smoothed; the ratio is limited to ±24 semitones.

## Tests

- **Pitch:** a 440 Hz tone at every whole shift from −24 to +24 comes out within 1% (zero crossings and spectrum peak); a two-note chord keeps both notes, each shifted.
- **Delay:** for each Range, an impulse-like burst comes out at the stated delay (within a few ms); in Harmony and Detune the dry signal is bit-exact (Voice Level −60 dB: output equals input).
- **Pedal:** Pedal 50% between 0 and +12 gives +6 semitones; Glide smooths a step; Momentary rises to Toe over Rise and falls back over Fall.
- **No clicks:** pedal sweeps, Momentary, mode and Range changes on a sustained chord produce no step beyond the signal's own.
- **Shimmer unchanged:** RKVerb and RKDelay's shimmer outputs are identical before and after the PitchShifter change (their windows stay 50 and 35 ms).
- **Validation:** auval (`aufx Ppt1 RKMu`), pluginval at strictness 10, presets, RKBoardTests.

## Decisions (2026-10-29)

- **Ranges:** Violin, Guitar, Bass (no Keys range).
- **Low Latency** switch (`low_latency`, Off by default, appended after the controls above): shortens the shifter's windows by a third, so the Whammy delay drops by a third (violin about 5 ms, guitar about 9 ms, bass about 17 ms) at the cost of grainier low notes. The display shows the delay that applies.
