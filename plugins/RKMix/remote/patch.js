// RKMix's console remote: the input patch (docs/design/RKMix.md, Input patch).
// Which input each channel takes, as the window's Patch page: a light per
// input lit by its signal (sent {type:"inputs"} only while this page shows,
// asked with {t:"inputs",on}), a Source box per channel, Default (tap twice)
// and Swap (then tap two channels).
"use strict";

(() => {
const { Palette, J, Component, TextButton, ChoiceBox, app, store, stripColour, drawPanel } = RK;
const { net, num } = C;

let page = null;
let levels = new Array(32).fill(-60);   // dB, from RKMix
const S = () => C.state.structure;
const locked = () => !!(C.isLocked && C.isLocked("everything"));
const srcId = ch => `ch${ch + 1}_src`;
const sourceOf = ch => { const s = S(); return s && s.ch[ch] && s.ch[ch].src !== undefined ? s.ch[ch].src : ch + 1; };   // 0 Off, n In n

// A channel's row: its number, colour and name; tap it while swapping.
class PatchRow extends Component {
  constructor(ch) { super(); this.ch = ch; this.takesTouches = true; }
  paint(g) {
    const s = S(); if (!s) return;
    const ch = this.ch, hid = s.ch[ch].hid, lead = hid ? ch - 1 : ch;
    const swapPick = page && page.swapFirst === ch;
    if (swapPick) { g.setColour(Palette.accent.withAlpha(0.3)); g.fillRoundedRectangle(0, 0, this.w, this.h, 4); }
    g.setColour(Palette.textDim); g.setFont(13, true); g.drawText(String(ch + 1), 0, 0, 26, this.h, J.centredRight);
    g.setColour(stripColour(s.ch[lead].c)); g.fillRoundedRectangle(32, 6, 4, this.h - 12, 2);
    const linked = s.ch[ch].st || hid;
    const name = (s.ch[lead].n || "Ch " + (lead + 1)) + (linked ? (hid ? " R" : " L") : "");
    g.setColour(sourceOf(ch) !== ch + 1 ? Palette.accent.brighter(0.4) : Palette.text); g.setFont(13);
    g.drawFittedText(name, 42, 0, this.w - 42, this.h, J.centredLeft, 1, 0.7);
  }
  pointerDown() { if (page) page.rowTapped(this.ch); return false; }
}

class PatchPage extends Component {
  constructor() {
    super();
    this.swapping = false; this.swapFirst = -1; this.armedDefault = false;
    this.rows = []; this.boxes = [];
    for (let ch = 0; ch < 32; ch++) {
      this.rows.push(this.add(new PatchRow(ch)));
      const box = this.add(new ChoiceBox(srcId(ch), v => v === 0 ? "Off" : "In " + v));
      box.separatorBefore = 1; box.locked = locked;
      this.boxes.push(box);
    }
    this.defaultButton = this.add(new TextButton("Default", () => this.defaultPressed()));
    this.swapButton = this.add(new TextButton("Swap", () => {
      if (locked()) return;
      this.swapping = !this.swapping; this.swapFirst = -1; this.update();
    }));
  }
  shown() {
    store.beginWants(); C.collectWants(this); if (C.extraWants) C.extraWants(); store.endWants();
    net.send({ t: "inputs", on: true });
  }
  hidden() { net.send({ t: "inputs", on: false }); this.swapping = false; this.swapFirst = -1; levels.fill(-60); }

  defaultPressed() {
    if (locked()) return;
    if (!this.armedDefault) { this.armedDefault = true; setTimeout(() => { this.armedDefault = false; this.update(); }, 3000); this.update(); return; }
    this.armedDefault = false;
    for (let ch = 0; ch < 32; ch++) store.set(srcId(ch), (ch + 1) / 32);
    this.update();
  }
  rowTapped(ch) {
    if (!this.swapping || locked()) return;
    if (this.swapFirst < 0) this.swapFirst = ch;
    else if (ch !== this.swapFirst) {
      const a = sourceOf(this.swapFirst), b = sourceOf(ch);
      store.set(srcId(this.swapFirst), b / 32); store.set(srcId(ch), a / 32);
      this.swapping = false; this.swapFirst = -1;
    }
    this.update();
  }
  update() {
    this.defaultButton.text = this.armedDefault ? "Tap again" : "Default";
    this.swapButton.toggle = this.swapping;
    this.repaint(); app.dirty = true;
  }

  resized() {
    this.lights = { x: 8, y: 8, w: this.w - 16, h: 96 };
    this.channels = { x: 8, y: 112, w: this.w - 16, h: this.h - 120 };
    this.swapButton.setBounds(this.channels.x + this.channels.w - 108, this.channels.y + 26, 100, 40);
    this.defaultButton.setBounds(this.channels.x + this.channels.w - 216, this.channels.y + 26, 100, 40);
    const gx = this.channels.x + 8, gy = this.channels.y + 74, gw = this.channels.w - 16, gh = this.channels.h - 82;
    const cw = gw / 4, rh = gh / 8;
    for (let ch = 0; ch < 32; ch++) {
      const x = gx + Math.floor(ch / 8) * cw, y = gy + (ch % 8) * rh;
      this.rows[ch].setBounds(x, y, cw - 112, rh - 4);
      this.boxes[ch].setBounds(x + cw - 108, y + 2, 100, Math.min(36, rh - 8));
    }
  }
  paint(g) {
    g.fillAll(Palette.tintedBackground);
    if (!this.lights) return;
    const s = S();
    // The inputs: a light each, lit by the signal; dim when no channel takes it, "x2" when split.
    drawPanel(g, this.lights.x, this.lights.y, this.lights.w, this.lights.h, Palette.tintedPanel, "Inputs (signal as it arrives)");
    const users = new Array(32).fill(0);
    for (let ch = 0; ch < 32; ch++) { const src = sourceOf(ch); if (src > 0) users[src - 1]++; }
    const lw = (this.lights.w - 16) / 32;
    for (let i = 0; i < 32; i++) {
      const cx = this.lights.x + 8 + i * lw + lw / 2, cy = this.lights.y + 40, db = levels[i];
      g.setColour(Palette.knobTrack.darker(0.3)); g.fillEllipse(cx - 9, cy - 9, 18, 18);
      if (db > -50) {
        const amount = Math.max(0.25, Math.min(1, 0.25 + (db + 50) / 44 * 0.75));
        g.setColour((db > -1 ? Palette.warning : Palette.accent.brighter(0.5)).withAlpha(amount)); g.fillEllipse(cx - 8, cy - 8, 16, 16);
      }
      g.setColour(users[i] ? Palette.text : Palette.knobTrack); g.setFont(12, true);
      g.drawText(String(i + 1), cx - lw / 2, cy + 12, lw, 16, J.centred);
      g.setColour(users[i] > 1 ? Palette.accent.brighter(0.4) : Palette.textDim); g.setFont(11);
      g.drawText(users[i] === 0 ? "-" : users[i] > 1 ? "x" + users[i] : "", cx - lw / 2, cy + 28, lw, 14, J.centred);
    }
    drawPanel(g, this.channels.x, this.channels.y, this.channels.w, this.channels.h, Palette.tintedPanel, "Channels: the input each takes");
    g.setFont(14); g.setColour(this.swapping ? Palette.accent.brighter(0.4) : Palette.textDim);
    g.drawText(this.swapping ? (this.swapFirst < 0 ? "Swap: tap the first channel" : "Swap: now the channel to swap with Ch " + (this.swapFirst + 1))
                             : "One input can feed several channels. A key \"Ch n\" follows channel n's input.",
               this.channels.x + 12, this.channels.y + 26, this.channels.w - 240, 40, J.centredLeft);
    // a box not on its own input: an accent frame
    for (let ch = 0; ch < 32; ch++)
      if (sourceOf(ch) !== ch + 1) { const b = this.boxes[ch]; g.setColour(Palette.accent.brighter(0.4)); g.drawRoundedRectangle(b.x - 1.5, b.y - 1.5, b.w + 3, b.h + 3, 5, 1.5); }
  }
}

const previous = C.onSignedIn;
C.onSignedIn = () => { if (previous) previous(); if (page && C.state.page === "patch") page.shown(); };
C.onInputs = msg => { levels = msg.p; if (page) page.repaint(); app.dirty = true; };
C.addPage("patch", "Patch", () => (page = new PatchPage()));
})();
