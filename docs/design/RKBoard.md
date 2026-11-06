# RKBoard: design notes

**The RKFX pedalboard.** How the board works is described in `plugins/RKBoard/BoardProcessor.h` and in the manual (`docs/manuals/RKBoard.md`). This file keeps design decisions and ideas for later. Undo, Lock, named scenes, scene morph and the remote are designed in `RKBoard Live Control.md`.

## Ideas for later

### Linked boards (an external plugin in the middle of the board)

**The goal:** to put a third-party plugin (an amp sim, a reverb) in the middle of an RKBoard chain.

**Why not a send + return (FX loop) pedal** (discussed 2026-11-06, set aside): RKBoard runs the whole board in one `processBlock` call. A loop in the middle of the chain would need the external plugin's output for block N while RKBoard is still computing block N, and the host can only run that plugin after RKBoard returns. Wired in the host, the loop (RKBoard's send output → the plugin → RKBoard's return input) is a cycle in the graph. Hosts refuse it, or break it with a delay of one buffer. At best the return comes back one host buffer late, and that delay changes with the buffer size. A send-only tap (an aux output) or a return-only input would work without a cycle, but neither was needed.

**What works today:** two instances in the host, `RKBoard (part 1) → external plugin → RKBoard (part 2)`, with no added latency. Gig Performer links their scenes (one widget or a script drives both boards' Scene parameter).

**The feature for later:** RKBoard instances in the same process that find each other (a shared registry of instances) and can be **linked**. A linked board follows the other's scene changes, and perhaps shares its Assign slots, so the two halves act as one board without host-side wiring. The link would be saved with each board (by a board name or link ID) and re-established when the session loads.

**Standalone:** RKBoard can't host third-party plugins there, so "external" would mean hardware: a send to spare outputs of the audio interface and a return from its inputs, like a real pedalboard's FX loop. The interface's round trip is fixed and could be measured and compensated for. This is a separate idea, not part of linking.
