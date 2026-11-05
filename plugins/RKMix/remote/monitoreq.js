// RKMix's console remote: Monitor EQ (docs/design/RKMix Monitor EQ.md). The
// window's Monitor EQ view on the iPad, for any Aux bus, with big buttons
// for walking the mic positions. RKMix does the measuring; this page sends
// commands ({t:"meq", a:...}) and shows the state it is sent ({type:"meq"}).
// A run started here stops if this iPad is gone for more than 2 seconds.
"use strict";

(() => {
const { Palette, J, Component, TextButton, PopupMenu, app, drawPanel, Colour } = RK;
const { state, net } = C;
const eqColour = Colour.hex(0xffa77ce8);   // RKGraphEQ's lavender
const kRange = 15;
const centres = [20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000,
                 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000];
const sideNames = ["Both", "Left", "Right", "Each ear"];

let meq = null;          // the latest state from RKMix
let selectedBus = -1;    // the bus this page measures

function send(a, extra) { net.send(Object.assign({ t: "meq", a }, extra || {})); }
function option(o) { send("opt", { o }); }
function S() { return state.structure; }
function auxBuses() { const out = []; for (let b = 0; b < 16; b++) if (C.busType(b) === 1) out.push(b); return out; }
function inputName(ch) { const s = S(), n = s && s.ch[ch] ? s.ch[ch].n : ""; return "In " + (ch + 1) + (n ? " - " + n : ""); }
function bandLabel(hz) { return hz >= 1000 ? (hz / 1000) + "k" : String(hz); }
function running() { return meq && meq.step !== "idle" && meq.step !== "done"; }
function mine() { return meq && (meq.bus === selectedBus || meq.step === "idle"); }

// A button that shows a value and opens a menu.
class MenuButton extends TextButton {
  constructor(items) { super("", null); this.items = items; }
  paint(g) {
    const a = this.enabled ? 1 : 0.4;
    g.setColour(Palette.knobBody.withMultipliedAlpha(a)); g.fillRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, 4);
    const cx = this.w - 14, cy = this.h / 2;
    g.setColour(Palette.textDim.withMultipliedAlpha(a)); g.fillPolygon([cx - 5, cy - 3, cx + 5, cy - 3, cx, cy + 3]);
    g.setColour(Palette.text.withMultipliedAlpha(a)); g.setFont(15);
    g.drawFittedText(this.text, 10, 0, this.w - 34, this.h, J.centredLeft, 1);
  }
  pointerDown() { if (!this.enabled) return false; PopupMenu.show(this.items(), this, { minWidth: this.w }); return false; }
}

// A horizontal slider over a range (not a parameter): the value shown, sent as it moves.
class ValueSlider extends Component {
  constructor(min, max, step, format, onChange) {
    super(); Object.assign(this, { min, max, step, format, onChange }); this.value = min; this.takesTouches = true; this.dragging = false;
  }
  setValue(v) { if (!this.dragging) this.value = v; }
  set(v) {
    v = Math.min(this.max, Math.max(this.min, Math.round(v / this.step) * this.step));
    if (v !== this.value) { this.value = v; this.onChange(v); this.repaint(); }
  }
  paint(g) {
    const a = this.enabled ? 1 : 0.4, tw = 74, w = this.w - tw - 10, cy = this.h / 2;
    const p = (this.value - this.min) / (this.max - this.min), x = 12 + p * (w - 24);
    g.setColour(Palette.knobTrack.withMultipliedAlpha(a)); g.drawLine(12, cy, w - 12, cy, 4);
    g.setColour(Palette.accent.withMultipliedAlpha(a)); g.drawLine(12, cy, x, cy, 4);
    g.setColour(Palette.text.withMultipliedAlpha(a)); g.fillEllipse(x - 10, cy - 10, 20, 20);
    g.setFont(15); g.drawText(this.format(this.value), w + 10, 0, tw, this.h, J.centredRight);
  }
  pointerDown(e) { if (!this.enabled) return false; this.dragging = true; this.lastX = e.x; this.drag = this.value; return true; }
  pointerMove(e) {
    const w = this.w - 84 - 24;
    this.drag += (e.x - this.lastX) / Math.max(60, w) * (this.max - this.min); this.lastX = e.x;
    this.set(this.drag);
  }
  pointerUp() { this.dragging = false; this.repaint(); }
}

// The 31 bands: measured, target, correction, with it.
class Graph extends Component {
  paint(g) {
    drawPanel(g, 0, 0, this.w, this.h, Palette.panel);
    const L = 40, T = 30, R = 14, B = 26, pw = this.w - L - R, ph = this.h - T - B, cw = pw / 31;
    const x = b => L + (b + 0.5) * cw;
    const y = db => T + (kRange - Math.max(-kRange, Math.min(kRange, db))) / (2 * kRange) * ph;
    g.setFont(12);
    for (const db of [-12, -6, 0, 6, 12]) {
      g.setColour(Palette.outline.withAlpha(db === 0 ? 1 : 0.5)); g.drawHorizontalLine(y(db), L, L + pw);
      g.setColour(Palette.textDim); g.drawText(String(db), 0, y(db) - 8, L - 6, 16, J.centredRight);
    }
    for (let b = 2; b < 31; b += 3) g.drawText(bandLabel(centres[b]), x(b) - 22, T + ph + 4, 44, 16, J.centred);
    if (!meq) return;

    // The target, dashed
    const ctx = g.ctx; ctx.save(); ctx.setLineDash([5, 5]);
    g.setColour(Palette.textDim); g.strokePolyline(meq.t.flatMap((v, b) => [x(b), y(v)]), 1.5);
    ctx.restore();

    const data = meq.n && mine();
    if (data) {
      const bit = (s, b) => s && s[b] === "1";
      for (let b = 0; b < 31; b++) {
        const c = meq.c[b], top = Math.min(y(0), y(c)), h = Math.max(1, Math.abs(y(c) - y(0)));
        g.setColour(eqColour.withAlpha(bit(meq.lc, b) ? 0.35 : 0.7)); g.fillRect(x(b) - cw * 0.32, top, cw * 0.64, h);
        if (bit(meq.nl, b) || bit(meq.or, b)) { g.setColour(Palette.textDim); g.drawRect(x(b) - cw * 0.32, y(0) - 3, cw * 0.64, 6); }
        if (bit(meq.nd, b)) { g.setColour(Palette.textDim); g.drawText("-", x(b) - 6, y(0) - 18, 12, 12, J.centred); }
      }
      // Runs of bands with data
      const runs = []; let run = [];
      for (let b = 0; b < 31; b++) { if (bit(meq.nd, b)) { if (run.length) runs.push(run); run = []; } else run.push(b); }
      if (run.length) runs.push(run);
      if (meq.res) {
        g.setColour(Palette.text.withAlpha(0.55));
        for (const r of runs) g.strokePolyline(r.flatMap(b => [x(b), y(meq.n[b] + meq.c[b])]), 1.5);
      }
      g.setColour(Palette.accent);
      for (const r of runs) g.strokePolyline(r.flatMap(b => [x(b), y(meq.n[b])]), 2.5);
      for (const r of runs) for (const b of r) g.fillEllipse(x(b) - 3.5, y(meq.n[b]) - 3.5, 7, 7);
    }

    // Legend
    let lx = L;
    const key = (colour, text) => {
      g.setColour(colour); g.fillRect(lx, 12, 12, 3); lx += 18;
      g.setColour(Palette.textDim); g.setFont(12); g.drawText(text, lx, 4, 200, 18, J.centredLeft); lx += g.textWidth(text) + 16;
    };
    key(Palette.accent, "Measured"); key(Palette.textDim, "Target"); key(eqColour, "Correction");
    if (data && meq.res) key(Palette.text.withAlpha(0.55), "With it");
    if (data && meq.ear > 0) key(Palette.warning, `L/R up to ${meq.ear} dB`);
  }
}

class MonitorEqPage extends Component {
  constructor() {
    super();
    this.rows = [];
    const row = (label, c) => { this.add(c); c.label = label; return c; };

    this.bus = row("Bus", new MenuButton(() => {
      const list = auxBuses().map(b => ({ text: C.busName(b), ticked: b === selectedBus, action: () => { selectedBus = b; app.dirty = true; } }));
      return list.length ? list : [{ header: true, text: "No Aux buses" }];
    }));
    this.speaker = this.add(new TextButton("Speaker", () => option({ type: 0 })));
    this.inEar = this.add(new TextButton("In-ear", () => option({ type: 1 })));
    this.source = row("Source", new MenuButton(() => {
      const o = meq.o, items = [{ text: "Measure (coupler)", ticked: !o.useFile, action: () => option({ useFile: false }) }];
      if (o.hasFile) items.push({ text: "File: " + o.fileName, ticked: o.useFile, action: () => option({ useFile: true }) });
      items.push({ separator: true }, { header: true, text: "Load files on the computer" });
      return items;
    }));
    this.input = row("Mic on", new MenuButton(() => {
      const items = [];
      for (let ch = 0; ch < 32; ch++) items.push({ text: inputName(ch), ticked: ch === meq.o.input, action: () => option({ input: ch }) });
      return items;
    }));
    this.mic = row("Mic", new MenuButton(() => {
      const o = meq.o, items = meq.mics.map((n, i) => ({ text: n, ticked: !o.useCal && i === o.mic, action: () => option({ mic: i }) }));
      if (o.hasCal) items.push({ separator: true }, { text: "Calibration: " + o.calName, ticked: o.useCal, action: () => option({ useCal: true }) });
      return items;
    }));
    this.target = row("Target", new MenuButton(() => meq.targets.map(([id, n]) => ({ text: n, ticked: id === meq.o.target, action: () => option({ target: id }) }))));
    this.bass = row("Bass", new ValueSlider(0, 8, 0.5, v => v.toFixed(1) + " dB", v => option({ bass: v })));
    this.level = row("Level", new ValueSlider(-60, -12, 1, v => v + " dBFS", v => option({ level: v })));
    this.sides = row("Sides", new MenuButton(() => sideNames.map((n, i) => ({ text: n, ticked: i === meq.o.sides, action: () => option({ sides: i }) }))));
    this.positions = row("Positions", new ValueSlider(1, 8, 1, v => String(v), v => option({ positions: v })));
    this.strength = this.add(new ValueSlider(0, 100, 5, v => v + " %", v => option({ strength: v / 100 })));

    this.graph = this.add(new Graph());
    const big = (text, f, bold) => { const b = this.add(new TextButton(text, f)); b.fontHeight = 18; b.bold = !!bold; return b; };
    this.start = big("Start", () => { if (selectedBus >= 0) send("start", { bus: selectedBus }); }, true);
    this.next = big("Next", () => send("next"), true);
    this.finish = big("Finish", () => send("finish"));
    this.stop = big("Stop", () => send("stop"));
    this.apply = big("Apply", () => send("apply"), true);
    this.undo = big("Undo", () => send("undo"));
    this.next.toggle = true;
  }
  // Lock everything (the iPad's Lock): looking only.
  find(x, y) { return C.isLocked && C.isLocked("everything") ? null : super.find(x, y); }
  shown() { send("sub", { on: true }); }
  hidden() { send("sub", { on: false }); }

  update() {
    if (!meq) return;
    // The bus: the one being measured, else the first Aux bus.
    const aux = auxBuses();
    if (running() || meq.step === "done") { if (meq.bus >= 0 && selectedBus < 0) selectedBus = meq.bus; }
    if (!aux.includes(selectedBus)) selectedBus = aux.length ? (aux.includes(meq.bus) ? meq.bus : aux[0]) : -1;

    const o = meq.o, inEar = o.type === 1, file = inEar && o.useFile, run = running(), m = mine();
    this.bus.text = selectedBus >= 0 ? C.busName(selectedBus) : "No Aux bus";
    this.speaker.toggle = !inEar; this.inEar.toggle = inEar;
    this.source.text = o.useFile ? "File: " + o.fileName : "Measure (coupler)";
    this.input.text = inputName(o.input);
    this.mic.text = o.useCal ? "Calibration: " + o.calName : meq.mics[o.mic];
    const t = meq.targets.find(([id]) => id === o.target); this.target.text = t ? t[1] : "";
    this.bass.setValue(o.bass);
    this.level.max = o.maxLevel; this.level.setValue(o.level);
    this.sides.text = sideNames[o.sides];
    this.positions.setValue(o.positions);
    this.strength.setValue(Math.round(o.strength * 100));

    const shown = [this.bus];
    if (inEar) shown.push(this.source);
    if (!file) shown.push(this.input, this.mic);
    shown.push(this.target);
    if (inEar && o.target === 4) shown.push(this.bass);
    if (!file) { shown.push(this.level); if (inEar) shown.push(this.sides); shown.push(this.positions); }
    for (const c of [this.source, this.input, this.mic, this.target, this.bass, this.level, this.sides, this.positions]) c.setVisible(shown.includes(c));
    this.rows = shown;
    for (const c of [this.bus, this.speaker, this.inEar, this.source, this.input, this.sides, this.positions]) c.enabled = !run;

    this.start.text = file ? "Use file" : meq.res && m ? "Measure again" : "Start";
    this.start.enabled = !run && selectedBus >= 0;
    this.next.enabled = run && m && meq.step === "waiting";
    this.finish.enabled = run && m && meq.step === "waiting" && meq.pos > 0;
    this.stop.enabled = run && m;
    this.apply.enabled = m && meq.apply;
    this.undo.enabled = meq.undo;
    this.resized();
  }

  resized() {
    const lw = 380, rowH = 46, gap = 8;
    this.left = { x: 12, y: 12, w: lw, h: this.h - 24 };
    let yy = 24;
    const place = (c, label) => { c.setBounds(12 + 100, yy, lw - 112, rowH - 8); yy += rowH; };
    place(this.bus);
    this.speaker.setBounds(112, yy, (lw - 112) / 2 - 3, rowH - 8); this.inEar.setBounds(112 + (lw - 112) / 2 + 3, yy, (lw - 112) / 2 - 3, rowH - 8);
    this.typeY = yy; yy += rowH;
    for (const c of this.rows.slice(1)) place(c);
    const sY = this.h - 12 - rowH - 8;
    this.strength.setBounds(112, sY, lw - 112, rowH - 8); this.strengthY = sY;

    const rx = 12 + lw + 12, rw = this.w - rx - 12, bh = 64;
    const by = this.h - 12 - bh;
    const bw = (rw - 5 * gap) / 6;
    [this.start, this.next, this.finish, this.stop, this.apply, this.undo].forEach((b, i) => b.setBounds(rx + i * (bw + gap), by, bw, bh));
    this.statusY = by - 52;
    this.graph.setBounds(rx, 12, rw, this.statusY - 12 - 10);
    this.right = { x: rx, w: rw };
  }

  paint(g) {
    g.fillAll(Palette.tintedBackground);
    if (!this.left) return;
    drawPanel(g, this.left.x, this.left.y, this.left.w, this.left.h, Palette.tintedPanel, "Monitor EQ");
    g.setFont(14); g.setColour(Palette.textDim);
    const label = (c, text) => g.drawText(text, 24, c.y, 86, c.h, J.centredLeft);
    for (const c of this.rows) label(c, c.label);
    g.drawText("Type", 24, this.typeY, 86, 38, J.centredLeft);
    label(this.strength, "Strength");
    if (!meq) { g.drawText("Waiting for RKMix…", this.right.x, this.statusY, this.right.w, 40, J.centredLeft); return; }

    // The status: the step's instruction, or what went wrong (warning colour), and the progress.
    const m = mine(), warn = m && !!meq.msg;
    const text = !m ? `Monitor EQ is measuring ${C.busName(meq.bus)}.` : warn ? meq.msg : meq.desc;
    g.setColour(warn ? Palette.warning : Palette.text); g.setFont(17, true);
    g.drawFittedText(text, this.right.x, this.statusY, this.right.w, 36, J.centredLeft, 2);
    if (m && ["floor", "settling", "measuring"].includes(meq.step)) {
      g.setColour(Palette.knobTrack); g.fillRect(this.right.x, this.statusY + 40, this.right.w, 5);
      g.setColour(Palette.accent); g.fillRect(this.right.x, this.statusY + 40, this.right.w * meq.prog, 5);
    }
  }
}

let page = null;
const previous = C.onSignedIn;
C.onSignedIn = () => { if (previous) previous(); if (page && state.page === "meq") page.shown(); };
C.listeners.structure.add(() => { if (page) page.update(); });
C.onMonitorEq = msg => { meq = msg; if (page) page.update(); app.dirty = true; };
C.addPage("meq", "Monitor EQ", () => (page = new MonitorEqPage()));
})();
