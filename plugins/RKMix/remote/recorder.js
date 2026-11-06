// RKMix's console remote: the recorder and the virtual soundcheck
// (docs/design/RKMix Recorder.md). Record and Stop (Stop asks again), the
// time, the dropouts and the disk, Marker; for a recording loaded on the
// computer, the transport, its markers (tap the bar to locate), the loop,
// and the inputs' source. This page sends {t:"rec"|"pb", a:...} and shows the
// state it is sent ({type:"rec"}). Locked, only "All live" still works.
"use strict";

(() => {
const { Palette, J, Component, TextButton, app, drawPanel } = RK;
const { net } = C;

let rec = null;          // the latest state from RKMix
let page = null;
const send = (t, a, extra) => net.send(Object.assign({ t, a }, extra || {}));
const locked = () => !!(C.isLocked && C.isLocked("faders"));
const hms = s => { s = Math.max(0, Math.floor(s)); return Math.floor(s / 3600) + ":" + String(Math.floor(s / 60) % 60).padStart(2, "0") + ":" + String(s % 60).padStart(2, "0"); };

// The recording's position: markers, the loop, the play head; tap to locate.
class PositionBar extends Component {
  constructor() { super(); this.takesTouches = true; }
  xOf(t) { const len = rec && rec.pb.len ? rec.pb.len : 1; return 8 + (t / len) * (this.w - 16); }
  paint(g) {
    g.setColour(Palette.knobTrack); g.fillRoundedRectangle(0, 22, this.w, this.h - 22, 6);
    const pb = rec && rec.pb; if (!pb || !pb.loaded) return;
    if (pb.loop) { g.setColour(Palette.accent.withAlpha(0.3)); g.fillRect(this.xOf(pb.ls), 22, this.xOf(pb.le) - this.xOf(pb.ls), this.h - 22); }
    g.setFont(13);
    for (const [t, label] of pb.markers) {
      const x = this.xOf(t);
      g.setColour(Palette.textDim); g.drawVerticalLine(x, 16, this.h); g.drawText(label, x + 3, 0, 120, 18, J.centredLeft);
    }
    g.setColour(Palette.accent); g.fillRect(this.xOf(pb.pos) - 2, 18, 4, this.h - 18);
  }
  pointerDown(e) {
    if (locked() || !rec || !rec.pb.loaded) return false;
    send("pb", "locate", { at: Math.max(0, Math.min(1, (e.x - 8) / (this.w - 16))) * rec.pb.len });
    return false;
  }
}

class RecorderPage extends Component {
  constructor() {
    super();
    this.record = this.add(new TextButton("Record", () => this.recordPressed()));
    this.record.fontHeight = 22; this.record.bold = true;
    this.marker = this.add(new TextButton("Marker", () => { if (!locked()) send("rec", "marker"); }));
    this.play = this.add(new TextButton("Play", () => { if (!locked() && rec) send("pb", rec.pb.playing ? "pause" : "play"); }));
    this.stop = this.add(new TextButton("Stop", () => { if (!locked()) send("pb", "stop"); }));
    this.prev = this.add(new TextButton("◀ Marker", () => { if (!locked()) send("pb", "prev"); }));
    this.next = this.add(new TextButton("Marker ▶", () => { if (!locked()) send("pb", "next"); }));
    this.loop = this.add(new TextButton("Loop", () => { if (!locked() && rec) send("pb", "loop", { on: !rec.pb.loop }); }));
    this.allPb = this.add(new TextButton("All to playback", () => { if (!locked()) send("pb", "allPb"); }));
    this.allLive = this.add(new TextButton("All live", () => send("pb", "allLive")));   // always: the one change that is always safe
    for (const b of [this.play, this.stop, this.prev, this.next, this.loop, this.allPb, this.allLive, this.marker]) b.fontHeight = 16;
    this.bar = this.add(new PositionBar());
    this.armedStop = false;
  }
  shown() { send("rec", "sub", { on: true }); }
  hidden() { send("rec", "sub", { on: false }); }

  // Stop asks again: a stray tap never ends the gig's recording.
  recordPressed() {
    if (locked() || !rec) return;
    if (!rec.on) { send("rec", "start"); return; }
    if (this.armedStop) { send("rec", "stop"); this.armedStop = false; }
    else { this.armedStop = true; setTimeout(() => { this.armedStop = false; this.update(); }, 3000); }
    this.update();
  }

  update() {
    if (!rec) return;
    const pb = rec.pb, loaded = !!pb.loaded;
    this.record.text = rec.on ? (this.armedStop ? "Tap again to stop" : "Stop") : "Record";
    this.record.toggle = rec.on; this.record.onColour = Palette.warning; this.record.colour = Palette.warning.withAlpha(0.35);
    this.marker.enabled = rec.on;
    this.play.text = loaded && pb.playing ? "Pause" : "Play"; this.play.toggle = loaded && pb.playing;
    this.loop.toggle = loaded && pb.loop;
    for (const b of [this.play, this.stop, this.prev, this.next, this.loop, this.allPb]) b.enabled = loaded;
    this.allLive.toggle = false; this.allLive.enabled = rec.src.length > 0;
    this.repaint();
  }

  resized() {
    const lw = 380, gap = 10;
    this.left = { x: 12, y: 12, w: lw, h: this.h - 24 };
    this.record.setBounds(24, 48, lw - 24, 80);
    this.marker.setBounds(24, 140, lw - 24, 56);
    const rx = 12 + lw + 12, rw = this.w - rx - 12;
    this.right = { x: rx, y: 12, w: rw, h: this.h - 24 };
    const bw = (rw - 24 - 4 * gap) / 5;
    [this.prev, this.play, this.stop, this.next, this.loop].forEach((b, i) => b.setBounds(rx + 12 + i * (bw + gap), 86, bw, 64));
    this.bar.setBounds(rx + 12, 170, rw - 24, 70);
    this.allPb.setBounds(rx + 12, 262, 200, 48);
    this.allLive.setBounds(rx + 12 + 210, 262, 160, 48);
  }

  paint(g) {
    g.fillAll(Palette.tintedBackground);
    if (!this.left) return;
    drawPanel(g, this.left.x, this.left.y, this.left.w, this.left.h, Palette.tintedPanel, "Recorder");
    drawPanel(g, this.right.x, this.right.y, this.right.w, this.right.h, Palette.tintedPanel, "Playback (virtual soundcheck)");
    if (!rec) { g.setColour(Palette.textDim); g.setFont(16); g.drawText("Waiting for RKMix…", 24, 220, 300, 30, J.centredLeft); return; }

    // The time, the dropouts, the disk, a problem.
    g.setColour(rec.on ? Palette.warning : Palette.textDim); g.setFont(40, true);
    g.drawText(hms(rec.t), 24, 210, this.left.w - 24, 50, J.centredLeft);
    g.setFont(16); g.setColour(rec.drops > 0 ? Palette.warning : Palette.textDim);
    g.drawText("Dropouts: " + rec.drops, 24, 266, 300, 24, J.centredLeft);
    g.setColour(Palette.textDim);
    g.drawText("Disk: " + (rec.left >= 60 ? Math.floor(rec.left / 60) + " h " + (rec.left % 60) + " min" : rec.left + " min") + " left", 24, 292, 300, 24, J.centredLeft);
    if (rec.status) { g.setColour(Palette.warning); g.drawFittedText(rec.status, 24, 320, this.left.w - 24, 44, J.topLeft, 2); }

    // The recording loaded on the computer.
    const pb = rec.pb, x = this.right.x + 12;
    g.setFont(17, true); g.setColour(Palette.text);
    if (!pb.loaded) {
      g.setFont(16); g.setColour(Palette.textDim);
      g.drawFittedText("Load a recording on the computer (Recorder page, Playback). Then play it here into the inputs set to PB.", x, 40, this.right.w - 24, 44, J.topLeft, 2);
    } else
      g.drawFittedText(pb.name + "   " + hms(pb.pos) + " / " + hms(pb.len) + (pb.locating ? "   locating" : "") + (pb.under ? "   underruns: " + pb.under : ""),
                       x, 40, this.right.w - 24, 36, J.centredLeft, 1);
    g.setFont(15); g.setColour(rec.src.length ? Palette.accent : Palette.textDim);
    g.drawText(rec.src.length ? rec.src.length + " inputs on Playback (PB)" : "Every input is Live", x + 390, 262, 300, 48, J.centredLeft);
  }
}

const previous = C.onSignedIn;
C.onSignedIn = () => { if (previous) previous(); if (page && C.state.page === "rec") page.shown(); };
C.onRecorder = msg => { rec = msg; if (page) page.update(); app.dirty = true; };
C.addPage("rec", "Recorder", () => (page = new RecorderPage()));
})();
