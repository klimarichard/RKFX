// RKMix's console remote: the Channel page. The selected channel in full
// (RKMixEditor::ChannelPanel: input, delay, high-pass, gate, output, EQ,
// compressor) with its sends (SendsPanel), or a bus's, the main's or the mono
// sum's EQ, compressor and limiter (ProcessingPanel) with a bus's sends. The
// EQ curve has a handle per band to drag with a finger. Below, the strips of
// the Mix page's layer to pick from.
"use strict";

(() => {
const { Palette, J, Component, TextButton, ToggleSwitch, SegmentedSwitch, Knob, LinearSlider, ChoiceBox, GainReductionMeter,
        ResponseCurve, TransferCurve, app, store, stripColour, drawPanel } = RK;
const { state, net, num } = C;
const accent = Palette.accent;
const PopupMenuShow = (items, anchor) => RK.PopupMenu.show(items, anchor, { minWidth: 200 });

const K = { knobW: 58, knobH: 80, switchW: 50, switchH: 30, title: 22, gap: 8, bottom: 56, procKnobW: 58, procKnobH: 76 };

function S() { return state.structure; }
function inner(a) { return { x: a.x + 8, y: a.y + K.title, w: a.w - 16, h: a.h - K.title - 2 }; }
function keyName(v) { return v === 0 ? "Self" : "Key: In " + v; }
const tapLabels = ["Default", "Input", "Pre-EQ", "Pre-Comp", "Pre-Ins", "Pre-Fader", "Post-Fader"];

function canSend(from, to) {
  const s = S(); if (!s) return false;
  const a = s.bus[from].t, b = s.bus[to].t;
  return from !== to && ((a === 0 && b !== 0) || (a === 2 && b === 1));
}
function canAssign(from, to) { const s = S(); return !!s && to > from && s.bus[from].t === 0 && s.bus[to].t === 0; }

// MixGui::OptionList: a choice as a vertical list, one option marked with a dot.
class OptionList extends RK.ParamComponent {
  constructor(id, labels) { super(id); this.labels = labels; this.marked = -1; }
  paint(g) {
    const n = this.labels.length, h = Math.max(1, Math.floor(this.h / n));
    const sel = Math.round(this.norm * Math.max(1, n - 1));
    g.setFont(10.5);
    this.labels.forEach((l, i) => {
      const y = i * h + 1, rh = h - 2;
      if (i === sel) { g.setColour(accent); g.fillRoundedRectangle(0, y, this.w, rh, 4); }
      g.setColour(i === sel ? Palette.onAccent(accent) : Palette.textDim);
      g.drawFittedText(l, 3, y, this.w - 6, rh, J.centred, 1);
      if (i === this.marked) { g.setColour(i === sel ? Palette.onAccent(accent) : accent); g.fillEllipse(this.w - 6, y + rh / 2 - 2, 4, 4); }
    });
  }
  pointerDown(e) {
    const n = this.labels.length, i = Math.max(0, Math.min(n - 1, Math.floor(e.y / Math.max(1, Math.floor(this.h / n)))));
    this.setNorm(i / (n - 1)); return false;
  }
}

// The EQ curve with a handle per band: drag sideways for the frequency, up
// and down for the gain (a shelf or bell), double-tap a handle for 0 dB.
class EqEditor extends ResponseCurve {
  constructor() { super(18, accent); this.takesTouches = true; this.handles = []; this.drag = new Map(); }
  // handles: [{ label, freq: id, gain: id|null, minHz, maxHz, on: () => bool }]
  setHandles(h) { this.handles = h; }
  hzOf(h) { return h.minHz * Math.pow(h.maxHz / h.minHz, store.value(h.freq)); }
  pos(h) {
    const x = this.xFor(this.hzOf(h));
    const y = h.gain ? this.yFor(store.value(h.gain) * 30 - 15) : this.yFor(0);
    return [x, y];
  }
  paint(g) {
    super.paint(g);
    for (const h of this.handles) {
      const [x, y] = this.pos(h), on = h.on(), held = [...this.drag.values()].some(d => d.h === h);
      g.setColour((held ? Palette.text : accent).withAlpha(on ? 1 : 0.35));
      g.fillEllipse(x - 10, y - 10, 20, 20);
      g.setColour(held ? Palette.background : Palette.onAccent(accent).withAlpha(on ? 1 : 0.6));
      g.setFont(11, true); g.drawText(h.label, x - 10, y - 10, 20, 20, J.centred);
    }
  }
  pointerDown(e) {
    let best = null, bestD = 34;
    for (const h of this.handles) { const [x, y] = this.pos(h), d = Math.hypot(x - e.x, y - e.y); if (d < bestD) { bestD = d; best = h; } }
    if (!best || (C.isLocked && C.isLocked("everything"))) return false;
    if (e.taps >= 2 && best.gain) { store.set(best.gain, 0.5); return false; }
    store.hold(best.freq, true); if (best.gain) store.hold(best.gain, true);
    this.drag.set(e.id, { h: best, x: e.x, y: e.y, f: store.value(best.freq), gn: best.gain ? store.value(best.gain) : 0, k: RK.fine.scale() });
    this.repaint();
    return true;
  }
  pointerMove(e) {
    const d = this.drag.get(e.id); if (!d) return;
    const h = d.h;
    // frequency: the curve's x is log(hz / 20) / log(1000)
    const startHz = h.minHz * Math.pow(h.maxHz / h.minHz, d.f);
    const hz = 20 * Math.pow(1000, Math.log(startHz / 20) / Math.log(1000) + (e.x - d.x) * d.k / this.w);
    store.set(h.freq, Math.log(Math.max(h.minHz, Math.min(h.maxHz, hz)) / h.minHz) / Math.log(h.maxHz / h.minHz));
    if (h.gain) store.set(h.gain, d.gn - (e.y - d.y) * d.k * (2 * this.range / this.h) / 30);
  }
  pointerUp(e) {
    const d = this.drag.get(e.id); if (!d) return;
    this.drag.delete(e.id);
    store.hold(d.h.freq, false); if (d.h.gain) store.hold(d.h.gain, false);
    this.repaint();
  }
}

// ---------------------------------------------------------------- the page
class StripPage extends Component {
  constructor() {
    super();
    this.prev = new TextButton("◀", () => this.step(-1));
    this.next = new TextButton("▶", () => this.step(1));
    C.listeners.structure.add(() => { if (this.parent) this.structureChanged(); });
    C.listeners.meters.add((m, dt) => { if (this.parent) this.meters(m, dt); });
    C.onSelect = () => { if (this.parent) this.rebuild(); };
  }
  shown() { this.rebuild(); }
  hidden() { for (const k of this.curveKeys || []) C.curves.drop(k); this.curveKeys = []; }

  // The strips to pick from: the Mix page's layer, then the main and the mono.
  choices() {
    const s = S(); if (!s) return [];
    const L = C.mix ? C.mix.layer : 0, out = [];
    if (L === 2) for (let b = 0; b < 16; b++) out.push(["bus", b]);
    else if (L >= 4) for (const id of s.up[L - 4] || []) out.push(id >= C.kUserPageBus ? ["bus", id - C.kUserPageBus] : ["ch", id]);
    else { const base = L === 1 ? 16 : 0; for (let ch = base; ch < base + 16; ch++) if (!s.ch[ch].hid) out.push(["ch", ch]); }
    out.push(["main", 0], ["mono", 0]);
    return out;
  }
  step(d) {
    const s = S(); if (!s) return;
    // every channel (pairs once), every bus, the main, the mono
    const all = [];
    for (let ch = 0; ch < 32; ch++) if (!s.ch[ch].hid) all.push(["ch", ch]);
    for (let b = 0; b < 16; b++) all.push(["bus", b]);
    all.push(["main", 0], ["mono", 0]);
    const sel = state.selected;
    let i = all.findIndex(([k, n]) => k === sel.kind && n === sel.index);
    i = (Math.max(0, i) + d + all.length) % all.length;
    state.selected = { kind: all[i][0], index: all[i][1] };
    this.rebuild();
  }
  selection() {
    const s = S(), sel = state.selected;
    if (!s) return sel;
    if (sel.kind === "ch" && s.ch[sel.index].hid) return { kind: "ch", index: sel.index - 1 };
    if (sel.kind === "dca" || sel.kind === "mg") return { kind: "ch", index: 0 };
    return sel;
  }
  structureChanged() {
    const s = S(), sel = this.selection();
    const key = JSON.stringify([sel, this.choices(), sel.kind === "ch" ? s.ch[sel.index].st : 0, s.bus.map(b => [b.t, b.st, b.own])]);
    if (key !== this.key) this.rebuild();
  }

  rebuild() {
    const s = S(); if (!s) return;
    const sel = this.selection();
    state.selected = sel;
    this.key = JSON.stringify([sel, this.choices(), sel.kind === "ch" ? s.ch[sel.index].st : 0, s.bus.map(b => [b.t, b.st, b.own])]);
    this.removeAll();
    this.sections = []; this.dimRules = []; this.extra = {}; this.labels = [];
    this.add(this.prev); this.add(this.next);
    this.chips = this.choices().map(([kind, index]) => {
      const name = kind === "ch" ? C.channelName(index) : kind === "bus" ? C.busName(index) : kind === "main" ? "Main" : "Mono";
      const b = this.add(new TextButton(name, () => { state.selected = { kind, index }; this.rebuild(); }));
      b.fontHeight = 12; b.toggle = kind === sel.kind && index === sel.index;
      b.stripColour = kind === "ch" ? s.ch[index].c : kind === "bus" ? s.bus[index].c : -1;
      return b;
    });
    store.beginWants();
    if (sel.kind === "ch") this.buildChannel(sel.index); else this.buildChain(sel.kind === "bus" ? sel.index : sel.kind === "main" ? 16 : 17);
    // Copy a section (the window's right-click menu), paste what was copied
    const key = sel.kind === "ch" ? "ch" + (sel.index + 1) : sel.kind === "bus" ? "bus" + (sel.index + 1) : sel.kind;
    const sections = sel.kind === "ch" ? [["input", "Input"], ["delay", "Delay"], ["hpf", "High-pass"], ["gate", "Gate"], ["eq", "EQ"], ["comp", "Compressor"], ["whole", "Whole strip"]]
                                       : [["eq", "EQ"], ["comp", "Compressor"], ["limit", "Limiter"], ["whole", "Whole strip"]];
    this.extra.copy = this.add(new TextButton("Copy…", () => PopupMenuShow(sections.map(([k, t]) => ({ text: "Copy " + t, action: () => net.send({ t: "copy", s: key, sec: k }) })), this.extra.copy)));
    this.extra.paste = this.add(new TextButton("Paste", () => { if (!(C.isLocked && C.isLocked("everything"))) net.send({ t: "paste", s: key }); }));
    C.collectWants(this);
    store.endWants();
    // curves
    for (const k of this.curveKeys || []) C.curves.drop(k);
    this.curveKeys = [];
    const strip = sel.kind === "ch" ? "ch" + (sel.index + 1) : sel.kind === "bus" ? "bus" + (sel.index + 1) : sel.kind;
    this.curveKeys.push("eq:" + strip);
    C.curves.want("eq:" + strip, d => { if (this.eqCurve) this.eqCurve.points = d; });
    if (sel.kind === "ch") { this.curveKeys.push("comp:" + strip); C.curves.want("comp:" + strip, d => { if (this.compCurve) this.compCurve.points = d; }); }
    this.resized(); app.dirty = true;
  }

  knob(id, label, section, dimWhenOff = true) {
    const k = this.add(new Knob(id, label));
    k.locked = () => C.isLocked && C.isLocked("everything");
    if (section && dimWhenOff) this.dimRules.push([k, section]);
    return k;
  }
  toggle(id, text) { const t = this.add(new ToggleSwitch(id, accent, text)); t.locked = () => C.isLocked && C.isLocked("everything"); return t; }
  segmented(id, labels, title) { const t = this.add(new SegmentedSwitch(id, accent, labels, title)); return t; }

  // RKMixEditor::ChannelPanel and SendsPanel
  buildChannel(ch) {
    const n = num(ch), id = s => `ch${n}_${s}`, stereo = C.isStereo(ch);
    this.ch = ch; this.chain = -1; this.stereo = stereo;
    const x = this.extra;
    x.link = this.toggle(`link${Math.floor(ch / 2) + 1}`, `Link ${Math.floor(ch / 2) * 2 + 1}/${Math.floor(ch / 2) * 2 + 2}`);
    // Input
    x.trim = this.knob(id("trim"), "Trim");
    x.pol = this.toggle(id("pol"), stereo ? "Ø L" : "Ø");
    x.polr = stereo ? this.toggle(id("polr"), "Ø R") : null;
    // Delay
    x.delayOn = this.toggle(id("delay_on"), "On");
    x.delay = this.knob(id("delay"), "Time", id("delay_on"));
    // HPF
    x.hpfOn = this.toggle(id("hpf_on"), "On");
    x.hpf = this.knob(id("hpf"), "Freq", id("hpf_on"));
    x.hpfSlope = this.segmented(id("hpf_slope"), ["12", "24"], "dB/oct");
    // Gate
    x.gateOn = this.toggle(id("gate_on"), "On");
    x.gateKnobs = [["gate_thr", "Threshold"], ["gate_range", "Range"], ["gate_att", "Attack"], ["gate_hold", "Hold"], ["gate_rel", "Release"]]
      .map(([s, l]) => this.knob(id(s), l, id("gate_on")));
    x.gateKey = this.add(new ChoiceBox(id("gate_key"), keyName)); x.gateKey.separatorBefore = 1;
    // Output
    x.pan = this.knob(id("pan"), stereo ? "Balance" : "Pan");
    x.toMain = this.toggle(id("main"), "Main");
    x.soloSafe = this.toggle(id("solosafe"), "Solo safe");
    // EQ
    x.eqOn = this.toggle(id("eq_on"), "On");
    x.lowType = this.segmented(id("eq_low_type"), ["Shelf", "Bell"], "Low");
    x.highType = this.segmented(id("eq_high_type"), ["Shelf", "Bell"], "High");
    const bands = ["low", "lmid", "hmid", "high"], bandNames = ["Low", "Lo Mid", "Hi Mid", "High"];
    x.eqKnobs = [];
    bands.forEach((b, i) => {
      x.eqKnobs.push(this.knob(id(`eq_${b}_freq`), bandNames[i], id("eq_on")), this.knob(id(`eq_${b}_gain`), "Gain", id("eq_on")),
                     this.knob(id(`eq_${b}_q`), "Q", id("eq_on")));
    });
    this.eqCurve = this.add(new EqEditor());
    const eqOn = () => store.on(id("eq_on"));
    this.eqCurve.setHandles([
      { label: "H", freq: id("hpf"), gain: null, minHz: 20, maxHz: 400, on: () => store.on(id("hpf_on")) },
      ...bands.map((b, i) => ({ label: String(i + 1), freq: id(`eq_${b}_freq`), gain: id(`eq_${b}_gain`), minHz: 20, maxHz: 20000, on: eqOn })),
    ]);
    // Compressor
    x.compOn = this.toggle(id("comp_on"), "On");
    x.compKnobs = [["comp_thr", "Threshold"], ["comp_ratio", "Ratio"], ["comp_knee", "Knee"], ["comp_att", "Attack"], ["comp_rel", "Release"], ["comp_makeup", "Makeup"]]
      .map(([s, l]) => this.knob(id(s), l, id("comp_on")));
    x.compKey = this.add(new ChoiceBox(id("comp_key"), keyName)); x.compKey.separatorBefore = 1;
    this.compMeter = this.add(new GainReductionMeter(accent, 24));
    this.compCurve = this.add(new TransferCurve(accent));
    // Sends (SendsPanel), and the direct out
    x.sends = [];
    const s = S();
    for (let b = 0; b < 16; b++) {
      const t = s.bus[b].t, col = { bus: b };
      if (t === 0) col.assign = this.toggle(id(`grp${b + 1}`), "Assign");
      else col.send = this.knob(id(`send${b + 1}`), "Send", null);
      if (t === 1 && s.bus[b].own) { col.pan = this.add(new LinearSlider(id(`span${b + 1}`))); col.pan.trackColour = stripColour(s.bus[b].c); col.pan.popupText = () => store.text(col.pan.id); }
      if (t === 1) { col.tap = this.add(new OptionList(id(`tap${b + 1}`), tapLabels)); store.want(`bus${b + 1}_tap`); }
      x.sends.push(col);
    }
    x.directOut = this.add(new ChoiceBox(id("direct_out"), null));
    x.directOut.offered = v => v === 0 || (v >= 33) === stereo;
    x.directTap = this.add(new ChoiceBox(id("direct_tap"), null));
  }

  // RKMixEditor::ProcessingPanel (a bus, the main, the mono sum), and a bus's sends
  buildChain(chain) {
    this.ch = -1; this.chain = chain;
    const owner = chain === 16 ? "main" : chain === 17 ? "mono" : "bus" + (chain + 1), id = s => `${owner}_${s}`;
    const x = this.extra;
    x.eqOn = this.toggle(id("eq_on"), "On");
    x.lowType = this.segmented(id("eq_low_type"), ["Shelf", "Bell"], "Band 1");
    x.highType = this.segmented(id("eq_high_type"), ["Shelf", "Bell"], "Band 6");
    x.hpfOn = this.toggle(id("hpf_on"), "HPF");
    x.hpf = this.knob(id("hpf"), "Freq", id("eq_on"));
    x.hpfSlope = this.segmented(id("hpf_slope"), ["12", "24"]);
    x.lpfOn = this.toggle(id("lpf_on"), "LPF");
    x.lpf = this.knob(id("lpf"), "Freq", id("eq_on"));
    x.lpfSlope = this.segmented(id("lpf_slope"), ["12", "24"]);
    x.bands = [];
    for (let b = 1; b <= 6; b++) x.bands.push(this.knob(id(`eq_b${b}_freq`), String(b), id("eq_on")), this.knob(id(`eq_b${b}_gain`), "Gain", id("eq_on")),
                                               this.knob(id(`eq_b${b}_q`), "Q", id("eq_on")));
    this.eqCurve = this.add(new EqEditor());
    const eqOn = () => store.on(id("eq_on"));
    this.eqCurve.setHandles([
      { label: "H", freq: id("hpf"), gain: null, minHz: 20, maxHz: 400, on: () => eqOn() && store.on(id("hpf_on")) },
      ...[1, 2, 3, 4, 5, 6].map(b => ({ label: String(b), freq: id(`eq_b${b}_freq`), gain: id(`eq_b${b}_gain`), minHz: 20, maxHz: 20000, on: eqOn })),
      { label: "L", freq: id("lpf"), gain: null, minHz: 1000, maxHz: 20000, on: () => eqOn() && store.on(id("lpf_on")) },
    ]);
    x.compOn = this.toggle(id("comp_on"), "On");
    x.compAuto = this.toggle(id("comp_auto"), "Auto rel.");
    x.comp = [["comp_thr", "Threshold"], ["comp_ratio", "Ratio"], ["comp_knee", "Knee"], ["comp_att", "Attack"], ["comp_rel", "Release"], ["comp_makeup", "Makeup"]]
      .map(([s, l]) => this.knob(id(s), l, id("comp_on")));
    this.compMeter = this.add(new GainReductionMeter(accent, 24));
    x.limOn = this.toggle(id("lim_on"), "On");
    x.limit = [this.knob(id("lim_ceiling"), "Ceiling", id("lim_on")), this.knob(id("lim_rel"), "Release", id("lim_on"))];
    this.limitMeter = this.add(new GainReductionMeter(accent, 24));
    this.compCurve = null;
    if (C.buildSetup) C.buildSetup(this, chain);
    // A bus's sends and group assigns to the other buses (BusSendsPanel)
    x.sends = [];
    if (chain < 16) for (let b = 0; b < 16; b++) {
      const col = { bus: b };
      if (canAssign(chain, b)) col.assign = this.toggle(`bus${chain + 1}_grp${b + 1}`, "Assign");
      else if (canSend(chain, b)) col.send = this.knob(`bus${chain + 1}_send${b + 1}`, "Send", null);
      x.sends.push(col);
    }
  }

  resized() {
    if (!this.extra) return;
    const W = this.w, H = this.h, x = this.extra, gap = K.gap;
    // the bottom row: strip picker
    const by = H - K.bottom + 12;
    this.prev.setBounds(12, by, 44, 32); this.next.setBounds(60, by, 44, 32);
    const cw = Math.min(90, Math.floor((W - 116 - 12) / Math.max(1, this.chips.length)) - 4);
    this.chips.forEach((b, i) => b.setBounds(116 + i * (cw + 4), by, cw, 32));
    const top = 40, bottom = H - K.bottom - gap;
    const right = this.ch >= 0 ? W - 8 - 100 - 16 - 8 * 26 - 16 : W - 8 - 8 * 26 - 16;
    x.paste.setBounds(right - 200, 6, 96, 28); x.copy.setBounds(right - 96, 6, 96, 28);
    this.sections = [];
    const section = (title, a) => { this.sections.push({ title, ...a }); return a; };
    if (this.ch >= 0) this.layoutChannel(W, top, bottom, section);
    else this.layoutChain(W, top, bottom, section);
  }

  layoutChannel(W, top, bottom, section) {
    const x = this.extra, gap = K.gap;
    x.link.setBounds(W - 8 - 100, 6, 100, 28);
    this.swatchX = W - 8 - 100 - 16 - 8 * 26;
    // Row A
    const rowH = 120;
    let cx = 8;
    const place = w => { const a = { x: cx, y: top, w, h: rowH }; cx += w + gap; return a; };
    const knobAt = (k, a, dx) => k.setBounds(a.x + dx, a.y, K.knobW, K.knobH);
    const centreY = a => a.y + (K.knobH - K.switchH) / 2;
    let a = inner(section("Input", place(150)));
    knobAt(x.trim, a, 0);
    if (x.polr) { x.pol.setBounds(a.x + K.knobW + 6, a.y + 6, K.switchW + 6, K.switchH); x.polr.setBounds(a.x + K.knobW + 6, a.y + 6 + K.switchH + 6, K.switchW + 6, K.switchH); }
    else x.pol.setBounds(a.x + K.knobW + 6, centreY(a), K.switchW + 6, K.switchH);
    a = inner(section("Delay", place(132)));
    x.delayOn.setBounds(a.x, centreY(a), K.switchW, K.switchH); knobAt(x.delay, a, K.switchW + 6);
    a = inner(section("High-pass", place(202)));
    x.hpfOn.setBounds(a.x, centreY(a), K.switchW, K.switchH); knobAt(x.hpf, a, K.switchW + 6);
    x.hpfSlope.setBounds(a.x + K.switchW + 12 + K.knobW, a.y + (K.knobH - 50) / 2, 64, 50);
    a = inner(section("Gate", place(96 + 5 * K.knobW + 12)));
    x.gateOn.setBounds(a.x, a.y + 4, K.switchW, K.switchH); x.gateKey.setBounds(a.x, a.y + 4 + K.switchH + 8, 88, K.switchH);
    x.gateKnobs.forEach((k, i) => knobAt(k, a, 96 + i * K.knobW));
    a = inner(section("Output", { x: cx, y: top, w: W - 8 - cx, h: rowH }));
    knobAt(x.pan, a, 0);
    x.toMain.setBounds(a.x + K.knobW + 8, a.y + 4, 92, K.switchH); x.soloSafe.setBounds(a.x + K.knobW + 8, a.y + 4 + K.switchH + 8, 92, K.switchH);
    // Row B: EQ, compressor
    const bTop = top + rowH + gap, bH = 300;
    const eqW = 12 * K.knobW + 28;
    a = inner(section("EQ", { x: 8, y: bTop, w: eqW, h: bH }));
    x.eqOn.setBounds(a.x, a.y, K.switchW, K.switchH);
    x.lowType.setBounds(a.x, a.y + K.switchH + 8, 80, 48);
    x.highType.setBounds(a.x, a.y + K.switchH + 64, 80, 48);
    this.eqCurve.setBounds(a.x + 90, a.y + 2, a.w - 90, a.h - K.knobH - 10);
    x.eqKnobs.forEach((k, i) => k.setBounds(a.x + 6 + i * K.knobW, a.y + a.h - K.knobH, K.knobW, K.knobH));
    a = inner(section("Compressor", { x: 8 + eqW + gap, y: bTop, w: W - 16 - eqW - gap, h: bH }));
    x.compOn.setBounds(a.x, a.y, K.switchW, K.switchH); x.compKey.setBounds(a.x, a.y + K.switchH + 8, 88, K.switchH);
    x.compKnobs.forEach((k, i) => k.setBounds(a.x + 96 + (i % 3) * K.knobW, a.y + Math.floor(i / 3) * (K.knobH + 4), K.knobW, K.knobH));
    const gx = a.x + 96 + 3 * K.knobW + 6;
    this.compMeter.setBounds(gx, a.y, 40, 2 * K.knobH);
    const side = Math.min(a.x + a.w - (gx + 48), a.h);
    this.compCurve.setBounds(gx + 48, a.y, side, side);
    // Row C: sends
    const cTop = bTop + bH + gap;
    this.sendsArea = { x: 8, y: cTop, w: W - 16, h: bottom - cTop };
    this.layoutSends(cTop + 30, bottom);
    const sa = this.sendsArea;
    x.directTap.setBounds(sa.x + sa.w - 130, sa.y + 2, 130, 26);
    x.directOut.setBounds(sa.x + sa.w - 130 - 26 - 130, sa.y + 2, 130, 26);
  }

  layoutSends(y, bottom) {
    const x = this.extra, sa = this.sendsArea, w = Math.floor((sa.w - 15 * 4) / 16);
    this.sendColumns = x.sends.map((c, i) => {
      const area = { x: sa.x + i * (w + 4), y, w, h: bottom - y };
      let cy = area.y + 40;
      if (c.send) c.send.setBounds(area.x + (w - K.knobW) / 2, cy, K.knobW, K.knobH);
      if (c.assign) c.assign.setBounds(area.x + 4, cy + (K.knobH - K.switchH) / 2, w - 8, K.switchH);
      cy += K.knobH;
      if (c.pan) c.pan.setBounds(area.x + 4, cy, w - 8, 20);
      cy += 24;
      if (c.tap) { c.tap.setBounds(area.x + 3, cy, w - 6, Math.max(70, Math.min(7 * 20, area.y + area.h - cy - 2))); c.tap.marked = 1 + Math.round(store.value(`bus${c.bus + 1}_tap`) * 5); }
      return area;
    });
  }

  layoutChain(W, top, bottom, section) {
    const x = this.extra, gap = K.gap, h = 3 * K.procKnobH + K.title + 10;
    // Row 1: the EQ, the whole width: switches, the curve, HPF, 6 bands, LPF
    let a = inner(section("EQ", { x: 8, y: top, w: W - 16, h }));
    x.eqOn.setBounds(a.x, a.y, K.switchW, K.switchH);
    x.lowType.setBounds(a.x, a.y + K.switchH + 10, 68, 48);
    x.highType.setBounds(a.x, a.y + K.switchH + 66, 68, 48);
    const cols = 8 * (K.procKnobW + 4), curveW = Math.max(120, a.w - 76 - cols - 8);
    this.eqCurve.setBounds(a.x + 76, a.y + 4, curveW, a.h - 8);
    let cx = a.x + 76 + curveW + 12;
    const column = (on, k, slope) => {
      on.setBounds(cx, a.y, K.procKnobW, K.switchH); k.setBounds(cx, a.y + K.switchH + 2, K.procKnobW, K.procKnobH);
      slope.setBounds(cx, a.y + K.switchH + K.procKnobH + 6, K.procKnobW, 28); cx += K.procKnobW + 4;
    };
    column(x.hpfOn, x.hpf, x.hpfSlope);
    for (let b = 0; b < 6; b++) { for (let k = 0; k < 3; k++) x.bands[3 * b + k].setBounds(cx, a.y + k * K.procKnobH, K.procKnobW, K.procKnobH); cx += K.procKnobW + 4; }
    column(x.lpfOn, x.lpf, x.lpfSlope);
    // Row 2: compressor, limiter (and the bus's or the main's setup)
    const top2 = top + h + gap, compW = 260, limW = 134;
    a = inner(section("Compressor", { x: 8, y: top2, w: compW, h }));
    x.compOn.setBounds(a.x, a.y, K.switchW, K.switchH); x.compAuto.setBounds(a.x, a.y + K.switchH + 8, 68, K.switchH);
    x.comp.forEach((k, i) => k.setBounds(a.x + 72 + Math.floor(i / 3) * (K.procKnobW + 4), a.y + (i % 3) * K.procKnobH, K.procKnobW, K.procKnobH));
    this.compMeter.setBounds(a.x + 72 + 2 * (K.procKnobW + 4), a.y, 40, a.h - 4);
    a = inner(section("Limiter", { x: 8 + compW + gap, y: top2, w: limW, h }));
    x.limOn.setBounds(a.x, a.y, K.procKnobW, K.switchH);
    x.limit[0].setBounds(a.x, a.y + K.switchH + 4, K.procKnobW, K.procKnobH);
    x.limit[1].setBounds(a.x, a.y + K.switchH + 4 + K.procKnobH, K.procKnobW, K.procKnobH);
    this.limitMeter.setBounds(a.x + K.procKnobW + 6, a.y, 40, a.h - 4);
    this.setupArea = { x: 8 + compW + limW + 2 * gap, y: top2, w: W - 16 - compW - limW - 2 * gap, h };
    if (C.layoutSetup) C.layoutSetup(this, this.setupArea, section);
    // Row 3: a bus's sends
    const cTop = top2 + h + gap;
    const anySend = x.sends.some(c => c.send || c.assign);
    this.sendsArea = this.chain < 16 && anySend && bottom - cTop > 120 ? { x: 8, y: cTop, w: W - 16, h: bottom - cTop } : null;
    if (this.sendsArea) this.layoutSends(cTop + 30, bottom);
    else this.sendColumns = [];
  }

  paint(g) {
    const s = S(); if (!s || !this.extra) return;
    const x = this.extra, sel = state.selected;
    // dimmed knobs: a section that is off; a shelf has no Q
    for (const [k, on] of this.dimRules) k.dimmed = !store.on(on);
    if (this.ch >= 0) {
      const eqOn = store.on(`ch${this.ch + 1}_eq_on`);
      if (eqOn) { x.eqKnobs[2].dimmed = store.value(`ch${this.ch + 1}_eq_low_type`) < 0.5; x.eqKnobs[11].dimmed = store.value(`ch${this.ch + 1}_eq_high_type`) < 0.5; }
      x.directTap.enabled = store.value(`ch${this.ch + 1}_direct_out`) > 0;
    } else {
      const o = this.chain === 16 ? "main" : this.chain === 17 ? "mono" : "bus" + (this.chain + 1), eq = store.on(o + "_eq_on");
      x.hpf.dimmed = !(eq && store.on(o + "_hpf_on")); x.lpf.dimmed = !(eq && store.on(o + "_lpf_on"));
      if (eq) { x.bands[2].dimmed = store.value(o + "_eq_low_type") < 0.5; x.bands[17].dimmed = store.value(o + "_eq_high_type") < 0.5; }
      x.comp[4].dimmed = !store.on(o + "_comp_on") || store.on(o + "_comp_auto");
    }
    const kind = this.ch >= 0 ? "channel" : "chain";
    x.paste.enabled = state.clipKind === kind;
    x.paste.text = state.clipKind === kind ? "Paste" : "Paste";
    if (state.clipboard && state.clipKind === kind) { g.setColour(Palette.textDim); g.setFont(11.5); g.drawText("Copied: " + state.clipboard, x.paste.x - 330, 6, 324, 28, J.centredRight); }
    // the header: "Ch 1" and the name, the colours
    g.setColour(Palette.textDim); g.setFont(14, true);
    const head = this.ch >= 0 ? "Ch " + C.channelLabel(this.ch) : this.chain < 16 ? "Bus " + (this.chain + 1) + " · " + C.busTypeNames[s.bus[this.chain].t] : "";
    g.drawText(head, 12, 4, 120, 32, J.centredLeft);
    g.setColour(Palette.text); g.setFont(16, true);
    const name = this.ch >= 0 ? C.channelName(this.ch) : this.chain < 16 ? C.busName(this.chain) : this.chain === 16 ? "Main" : "Mono";
    this.nameArea = { x: 130, y: 4, w: 320, h: 32 };
    g.drawText(name, 130, 4, 320, 32, J.centredLeft);
    if (this.ch >= 0 || this.chain < 16) {
      const current = this.ch >= 0 ? s.ch[this.ch].c : s.bus[this.chain].c;
      const sx = this.ch >= 0 ? this.swatchX : this.w - 8 - 8 * 26;
      this.swatches = [];
      for (let i = 0; i < 8; i++) {
        const r = { x: sx + i * 26 + 2, y: 9, w: 22, h: 22 };
        this.swatches.push(r);
        g.setColour(stripColour(i)); g.fillRoundedRectangle(r.x, r.y, r.w, r.h, 3);
        if (i === current) { g.setColour(Palette.text); g.drawRoundedRectangle(r.x - 1.5, r.y - 1.5, r.w + 3, r.h + 3, 3, 1.5); }
      }
    } else this.swatches = [];
    for (const sec of this.sections) drawPanel(g, sec.x, sec.y, sec.w, sec.h, Palette.tintedPanel, sec.title);
    for (const [t, lx, ly] of this.labels || []) { g.setColour(Palette.textDim); g.setFont(11.5); g.drawText(t, lx, ly, 200, 16, J.centredLeft); }
    // the sends
    if (this.sendsArea) {
      const sa = this.sendsArea;
      g.setColour(Palette.text); g.setFont(15, true);
      g.drawText(this.ch >= 0 ? name + " - sends" : name + " - sends and group assigns", sa.x + 4, sa.y, 400, 30, J.centredLeft);
      if (this.ch >= 0) {
        g.setColour(Palette.textDim); g.setFont(12);
        const d = x.directOut;
        g.drawText("Direct out", d.x - 74, sa.y + 2, 70, 26, J.centredRight);
        g.drawText("at", d.x + d.w, sa.y + 2, 26, 26, J.centred);
      }
      (this.sendColumns || []).forEach((area, i) => {
        const b = x.sends[i].bus;
        drawPanel(g, area.x, area.y, area.w, area.h, Palette.tintedPanel);
        g.setColour(stripColour(s.bus[b].c)); g.fillRoundedRectangle(area.x + 4, area.y + 2, area.w - 8, 4, 2);
        g.setColour(Palette.text); g.setFont(11.5, true); g.drawFittedText(C.busName(b), area.x + 2, area.y + 8, area.w - 4, 18, J.centred, 1);
        g.setColour(Palette.textDim); g.setFont(10.5);
        g.drawText(C.busTypeNames[s.bus[b].t] + (s.bus[b].st ? "" : " mono"), area.x, area.y + 24, area.w, 14, J.centred);
        const col = x.sends[i];
        if (!col.send && !col.assign) g.drawText("—", area.x, area.y + 40, area.w, K.knobH, J.centred);
        if (this.ch >= 0 && s.bus[b].t === 2) g.drawText("Post-Fader", area.x, area.y + 40 + K.knobH + 6, area.w, 20, J.centred);
        if (col.tap) col.tap.marked = 1 + Math.round(store.value(`bus${b + 1}_tap`) * 5);
      });
    }
    // the strip picker's colours
    g.setColour(Palette.tintedPanel); g.fillRect(0, this.h - K.bottom, this.w, K.bottom);
    g.setColour(Palette.outline); g.fillRect(0, this.h - K.bottom, this.w, 1);
    for (const b of this.chips) if (b.stripColour >= 0) { g.setColour(stripColour(b.stripColour)); g.fillRoundedRectangle(b.x + 4, b.y - 5, b.w - 8, 3, 1.5); }
  }
  pointerDown(e) {
    for (let i = 0; i < (this.swatches || []).length; i++) {
      const r = this.swatches[i];
      if (e.x >= r.x - 2 && e.x <= r.x + r.w + 2 && e.y >= r.y - 2 && e.y <= r.y + r.h + 2) {
        net.send({ t: "colour", s: this.ch >= 0 ? "ch" + (this.ch + 1) : "bus" + (this.chain + 1), c: i });
        return false;
      }
    }
    const n = this.nameArea;
    if (n && e.taps >= 2 && e.x >= n.x && e.x <= n.x + n.w && e.y <= n.y + n.h && C.renameStrip && (this.ch >= 0 || this.chain < 16))
      C.renameStrip(this.ch >= 0 ? "ch" : "bus", this.ch >= 0 ? this.ch : this.chain, { screenPos: () => ({ x: n.x + this.screenPos().x, y: n.y + this.screenPos().y }), w: n.w, h: n.h });
    return false;
  }
  meters(m, dt) {
    if (this.ch >= 0) {
      if (this.compMeter) this.compMeter.setReduction(m.k[this.ch], dt);
      if (this.compCurve) { const level = Math.max(-60, m.c[this.ch]); if (Math.abs(level - this.compCurve.inputDb) > 0.2) { this.compCurve.inputDb = level; app.dirty = true; } }
    } else if (this.chain >= 0) {
      if (this.compMeter) this.compMeter.setReduction(m.cc[this.chain], dt);
      if (this.limitMeter) this.limitMeter.setReduction(m.cl[this.chain], dt);
    }
  }
}
StripPage.prototype.takesTouches = true;

C.addPage("strip", "Channel", () => new StripPage());
})();
