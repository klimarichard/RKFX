// RKMix's console remote: a send's own EQ (docs/design/RKMix Send EQ.md).
// An EQ button under each Aux and FX send (the Channel page's sends) and
// above each fader while the faders show the sends to a bus; lit when the
// EQ is on. Tapping it opens the EQ over the console: the curve with a handle
// per band (as the channel EQ's), On, Flat and the eight knobs.
"use strict";

(() => {
const { Palette, J, Component, TextButton, Knob, ToggleSwitch, EqEditor, app, store, Colour, stripColour } = RK;
const { curves } = C;
const locked = () => !!(C.isLocked && C.isLocked("everything"));
const eqId = (ch, bus, f) => `ch${ch + 1}_seq${bus + 1}_${f}`;

// The button: "EQ", lit while the send's EQ is on.
class SendEqButton extends Component {
  constructor(ch, bus) { super(); this.takesTouches = true; this.set(ch, bus); }
  set(ch, bus) { this.ch = ch; this.bus = bus; store.want(eqId(ch, bus, "on")); this.repaint(); }
  paint(g) {
    const on = store.on(eqId(this.ch, this.bus, "on"));
    g.setColour(on ? Palette.accent : Palette.knobBody); g.fillRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, 4);
    g.setColour(on ? Palette.onAccent(Palette.accent) : Palette.textDim); g.setFont(12, true);
    g.drawText("EQ", 0, 0, this.w, this.h, J.centred);
  }
  pointerDown() { return !locked(); }
  pointerUp(e) { if (!e.cancelled && this.hitTest(e.x, e.y)) C.openSendEq(this.ch, this.bus); }
}

// The EQ over the console (a sheet, as an insert's).
class SendEqSheet extends Component {
  constructor(ch, bus) {
    super(); this.takesTouches = true; this.ch = ch; this.bus = bus;
    const id = f => eqId(ch, bus, f), eqOn = () => store.on(id("on"));
    this.on = this.add(new ToggleSwitch(id("on"), Palette.accent, "On")); this.on.locked = locked;
    this.flat = this.add(new TextButton("Flat", () => { if (!locked()) for (const [f, n] of [["lc", 0], ["lsg", 0.5], ["mg", 0.5], ["hsg", 0.5]]) store.set(id(f), n); }));
    this.closeButton = this.add(new TextButton("Close", () => this.close()));
    this.curve = this.add(new EqEditor(Palette.accent, locked));
    this.curve.setHandles([
      { label: "C", freq: id("lc"), gain: null, minHz: 20, maxHz: 400, offZone: 0.05, on: eqOn },
      { label: "L", freq: id("lsf"), gain: id("lsg"), minHz: 40, maxHz: 500, on: eqOn },
      { label: "M", freq: id("mf"), gain: id("mg"), minHz: 200, maxHz: 8000, on: eqOn },
      { label: "H", freq: id("hsf"), gain: id("hsg"), minHz: 2000, maxHz: 16000, on: eqOn },
    ]);
    this.knobs = [["lc", "Low Cut"], ["lsf", "Low"], ["lsg", "Gain"], ["mf", "Mid"], ["mg", "Gain"], ["mq", "Q"], ["hsf", "High"], ["hsg", "Gain"]]
      .map(([f, label]) => { const k = this.add(new Knob(id(f), label)); k.locked = locked; return k; });
    for (const f of ["on", "lc", "lsf", "lsg", "mf", "mg", "mq", "hsf", "hsg"]) store.extra.add(id(f));
    store.endWants();
    this.curveKey = `seq:ch${ch + 1}:bus${bus + 1}`;
    curves.want(this.curveKey, d => { this.curve.points = d; this.curve.repaint(); });
  }
  area() { const w = Math.min(this.w - 16, 760), h = Math.min(this.h - C.kHeader - 16, 470); return { x: (this.w - w) / 2, y: C.kHeader + 8, w, h }; }
  resized() {
    const a = this.area();
    this.closeButton.setBounds(a.x + a.w - 12 - 84, a.y + 10, 84, 34);
    this.flat.setBounds(a.x + a.w - 12 - 84 - 6 - 70, a.y + 10, 70, 34);
    this.on.setBounds(a.x + a.w - 12 - 84 - 6 - 70 - 6 - 64, a.y + 10, 64, 34);
    const kh = 92, ch = a.h - 56 - kh - 40;
    this.curve.setBounds(a.x + 12, a.y + 56, a.w - 24, ch);
    const kw = Math.floor((a.w - 24) / 8);
    this.knobs.forEach((k, i) => k.setBounds(a.x + 12 + i * kw + (kw - 72) / 2, a.y + 56 + ch + 10, 72, kh));
  }
  paint(g) {
    g.setColour(Colour.hex(0x99000000)); g.fillRect(0, 0, this.w, this.h);
    const a = this.area(), s = C.state.structure;
    g.setColour(Palette.tintedPanel.darker(0.15)); g.fillRoundedRectangle(a.x, a.y, a.w, a.h, 6);
    g.setColour(Palette.outline); g.drawRoundedRectangle(a.x + 0.5, a.y + 0.5, a.w - 1, a.h - 1, 6, 1);
    if (s) { g.setColour(stripColour(s.bus[this.bus].c)); g.fillRoundedRectangle(a.x + 12, a.y + 12, 5, 30, 2); }
    g.setColour(Palette.text); g.setFont(17, true);
    g.drawFittedText(C.channelName(this.ch) + " in " + C.busName(this.bus) + ": send EQ", a.x + 26, a.y + 10, a.w - 300, 34, J.centredLeft, 1);
    g.setColour(Palette.textDim); g.setFont(12);
    g.drawText("Only this channel in this mix. Drag a handle sideways for its frequency, up and down for its gain; double-tap it for 0 dB.",
               a.x + 16, a.y + a.h - 30, a.w - 32, 22, J.centredLeft);
  }
  pointerDown() { return true; }   // nothing under it gets a touch
  close() { app.pop(this); curves.drop(this.curveKey); store.extra.clear(); store.endWants(); }
}

C.SendEqButton = SendEqButton;
C.openSendEq = (ch, bus) => {
  if (locked()) return;
  if (C.isStereo(ch - 1) && ch % 2 === 1) ch -= 1;   // a pair: its left channel's
  app.push(new SendEqSheet(ch, bus));
};
})();
