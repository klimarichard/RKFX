# RKVoice

**Harmonizer** · Modulation family (periwinkle) · zero latency for your own sound

RKVoice adds up to four harmony voices to your voice or instrument (singing, violin, sax, a single guitar line). Each voice is a pitch-shifted copy of what you play, so it sounds like you, not like a synth. Where the harmony goes is up to you: **hold a chord** on a keyboard (or let RKTracks play one from a song) and each voice takes a note of that chord, or set a **key and scale** and each voice sits a 3rd, 5th or octave away, in key.

For the header, knobs, presets, Output and Bypass, see [Common Controls](Common%20Controls.md). RKVoice has no Dry/Wet: **Dry** and the voice levels are the mix.

## How it works

- RKVoice listens for the pitch of what you sing or play. **It harmonizes single notes**: a chord on a guitar or piano confuses it.
- **Your own sound has no latency at all**: it passes through untouched. Each harmony voice follows a few milliseconds behind it, one to two periods of the note you sing: about 3 to 5 ms on a high note, 6 to 9 ms in the middle of a voice, 15 to 25 ms on a low male note. That reads as a second singer standing a few metres away.
- **At the start of a note**, the voices come in once its pitch is clear: about 15 to 30 ms into a sung or bowed note. A hard, instant attack (a picked guitar note) can take longer, up to about 100 ms.
- The voices **keep your expression**: your vibrato, scoops and slides carry over into the harmony, a set interval away from what you actually sing.
- A little vibrato or a scoop doesn't make the harmony jump between notes: the sung note changes only once you've clearly moved to a new one.
- When the note ends (below the **Threshold**, or the pitch is lost), the voices fade out over 20 ms.

## Harmony

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Chord / Scale** | Chord, Scale | Chord | Where the voices get their notes. **Chord**: from the chord you hold on MIDI (each voice's **In a Chord** setting). **Scale**: from the key and scale (each voice's **In the Scale** setting). |
| **Chord Tones** | Any Octave, As Voiced | Any Octave | **Any Octave**: the notes of the held chord in every octave, so there is always a note above and below you. **As Voiced**: only the exact notes you hold; a voice with no held note in its direction stays silent, so the way you voice the chord on the keyboard sets the spacing. |
| **No Chord** | Silent, Latch, Scale | Silent | What the voices do when no chord is held. **Silent**: they stop until you hold a chord again. **Latch**: they keep the last chord. **Scale**: they follow the key and scale until a chord comes back. |
| **MIDI Ch** | Omni, 1 to 16 | Omni | Which MIDI channel the chord comes from. Omni takes every channel. |
| **Key**, **Scale** | C to B; Major … Chromatic | C, Major | The key and scale for Scale mode (and for No Chord: Scale). The same as in RKHarmonize: in a seven-note scale a 3rd is major or minor as the key requires; in pentatonic and blues the harmony moves to the nearest scale note; Chromatic keeps every interval fixed. |

The sustain pedal works as on a piano: notes you release while it's down stay in the chord until you lift it.

## Input

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Voice / Bass / Guitar / Violin** | Voice (65 Hz to 1.2 kHz), Bass (35 to 400 Hz), Guitar (60 Hz to 1.1 kHz), Violin (125 Hz to 2.7 kHz) | Voice | The range of notes RKVoice listens for. Choose your source: the right range tracks more reliably. |
| **Threshold** | -70 to -20 dB | -50 dB | Below this level the voices stay silent. Raise it if breaths, bleed or the end of a note make the harmony sputter; lower it if soft notes don't get a harmony. |
| **Dry** | -60 dB (off) to +6 dB | 0 dB | Your own sound in the mix. Turn it right down to hear only the harmony, for example when your dry voice already goes to the PA on its own channel. |

## Voices

Each of the four voices has:

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **In a Chord** | Off, 3rd below, 2nd below, 1st below, 1st above, 2nd above, 3rd above | Voice 1: 1st above · Voice 2: 1st below · Voices 3, 4: Off | Chord mode: which note of the held chord this voice sings, counted from the note you sing. *1st above* is the nearest chord note above you, *2nd above* the next one, and so on. |
| **In the Scale** | Off, octave / 6th / 5th / 4th / 3rd below, 3rd / 4th / 5th / 6th / octave above | Voice 1: 3rd above · others Off | Scale mode: the voice's interval in the key. |
| **Level** | -60 dB (off) to +6 dB | -3 dB | The voice's level. |
| **Pan** | 100% left to 100% right | -30%, +30%, -60%, +60% | Where the voice sits in the stereo picture (a mono input comes out in stereo). |

The setting a voice isn't using right now is dimmed: **In the Scale** in Chord mode (unless No Chord is set to Scale), **In a Chord** in Scale mode.

### Chord positions: an example

Holding **C, E and G**:

| Voice set to | You sing D | You sing E (a note of the chord) |
| --- | --- | --- |
| 1st above | E | G |
| 2nd above | G | C (an octave up) |
| 3rd above | C (an octave up) | E (an octave up) |
| 1st below | C | C |
| 2nd below | G (below) | G (below) |
| 3rd below | E (below) | E (below) |

When you sing a note of the chord, the voices skip it, so none of them doubles you in unison. With **1st above** and **1st below** (the default), every note you sing becomes a close three-part chord that moves with you as the chords change.

## Bottom row

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| **Detune** | 0 to 20 cents | 5 cents | Spreads the voices a few cents around the exact pitch (voices 1 and 2 by the full amount, up and down; 3 and 4 by half), so they don't sound locked together. More is a looser, choir-like sound. |
| **Humanize** | 0 to 10 ms | 3 ms | A few milliseconds of slowly wandering timing per voice, like singers who never start together exactly. |
| **Glide** | 0 to 200 ms | 30 ms | How long a voice takes to slide to its new note when the chord or your note changes. Short is crisp; long sounds like singers moving between notes. |

## The status line

Above the voices, RKVoice shows what it's doing: the note it hears you **singing**, the **chord** in use (or *no chord*), and the note each voice is singing (a dash when it's silent).

## Starting points

These starting points are built into the plugin as **factory presets**: open the preset menu and choose **Factory**. Load one, then adjust it to your sound and use **Save as** to keep your version.

| Factory preset | Settings |
| --- | --- |
| **Close Harmony** | The defaults: Chord mode, voice 1 *1st above* (pan -30%), voice 2 *1st below* (pan +30%) |
| **Two Above** | Chord mode, voices 1 and 2 *1st* and *2nd above* (pan -25%, +25%): a three-part harmony over the melody |
| **Choir** | Chord mode, four voices *1st above*, *1st below*, *2nd above*, *2nd below* (levels -5, -5, -7, -7 dB; pan -35%, +35%, -70%, +70%), Detune 10 cents, Humanize 6 ms, Glide 50 ms |
| **Thirds in the Key** | Scale mode (C major), voice 1 *3rd above*, centred |
| **Triads in the Key** | Scale mode (C major), voice 1 *3rd above* (pan -30%), voice 2 *5th above* (pan +30%) |
| **Octaves** | Scale mode, voice 1 *octave above* (-9 dB), voice 2 *octave below* (-6 dB), centred, Detune 3 cents |
| **Harmony Only** | Close Harmony with Dry off: only the voices, for when your own sound reaches the PA another way |

Set the **Key** to your song's key in the Scale-mode presets.

## In a rig

- **Gig Performer**: put RKVoice after your vocal or violin input and connect a **MIDI In** block to it: a keyboard you hold chords on, or RKTracks' MIDI output so the chords come from the song. Without MIDI, use Scale mode.
- **RKBoard**: RKVoice is a pedal like the others, and plays from the board's MIDI input.
- Place it on a clean signal: after a compressor, de-esser or EQ is fine; after distortion, chorus, delay or reverb it can't track. Reverb and delay after RKVoice give the voices the same space as you.

## Notes

- If the harmony comes in late or sputters, check the **Range** first, then raise the **Threshold** a little.
- In a loud room, bleed from the band can reach the vocal mic; a gate or a higher Threshold keeps the voices from following it.
- Gig Performer tip: map **Dry** or the voice levels to widgets to bring the harmony in for a chorus only, or a switch to **Chord / Scale** to change how a song section is harmonized.
