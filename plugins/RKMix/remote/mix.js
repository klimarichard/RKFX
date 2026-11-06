// RKMix's console remote: the Mix page. The strips of a layer (Inputs 1-16,
// Inputs 17-32, Buses, DCA, User 1, User 2) as in the window, the main and
// mono, the scenes, Clear solo and Talk. Strips as RKMixEditor::Strip draws
// them (PluginEditor.cpp), with buttons sized for fingers.
"use strict";

(() => {
const { Palette, J, Component, TextButton, ToggleSwitch, MixFader, Meter, LinearSlider, app, store, stripColour, drawPanel } = RK;
const { state, net, num } = C;

const L = {   // MixGui::Layout, with the touch rows taller
  gap: 8, colourBar: 5, name: 22, number: 14, section: 14, insertRow: 20, pan: 24, button: 34, meter: 9,
  master: 196, bottom: 56,
};
const kLayers = ["Inputs 1–16", "Inputs 17–32", "Buses", "DCA", "User 1", "User 2"];
const kBusLayer = 2, kDcaLayer = 3, kUserLayer = 4;
const soloColour = stripColour(3);

const mix = {
  layer: 0, spill: -1, sendsOnFader: -1,
};
C.mix = mix;

function S() { return state.structure; }
function isSelected(kind, index) { return state.selected.kind === kind && state.selected.index === index; }
function select(kind, index) { state.selected = { kind, index }; app.dirty = true; if (C.onSelect) C.onSelect(); }

// ---------------------------------------------------------------- small parts
// RKMixEditor::InsertButton: the plugin's short name with its family colour, or "+".
class InsertCell extends Component {
  constructor(strip, index, slot) { super(); this.strip = strip; this.index = index; this.slot = slot; this.takesTouches = true; }
  entry() {
    const s = S(); if (!s) return null;
    const list = this.strip === "ch" ? s.ch[this.index].ins : this.strip === "bus" ? s.bus[this.index].ins : s[this.strip].ins;
    return list ? list[this.slot] : null;
  }
  paint(g) {
    const t = this.entry();
    g.setColour(Palette.knobBody.withAlpha(t ? 1 : 0.5)); g.fillRoundedRectangle(0, 0, this.w, this.h, 3);
    if (!t) { g.setColour(Palette.knobTrack); g.setFont(11); g.drawText("+", 0, 0, this.w, this.h, J.centred); return; }
    const family = RK.Colour.hex(t.f);
    g.setColour((t.x ? Palette.warning : family).withAlpha(t.on ? 1 : 0.35)); g.fillRoundedRectangle(0, 0, 4, this.h, 2);
    let right = this.w - 2;
    if (t.midi) { g.setColour(t.x ? Palette.warning : family); g.setFont(8.5, true); g.drawText("MIDI", right - 22, 0, 22, this.h, J.centredRight); right -= 22; }
    g.setColour(t.on ? Palette.text : Palette.textDim); g.setFont(10.5);
    g.drawFittedText(t.n, 6, 0, right - 6, this.h, J.centredLeft, 1);
  }
  pointerDown() { if (C.openInsert) C.openInsert(this.strip, this.index, this.slot, this); return false; }
}

// RKMixEditor::SoloButton (solo is the window's, not a parameter)
class SoloButton extends Component {
  constructor(key) { super(); this.key = key; this.takesTouches = true; }
  isOn() {
    const s = S(); if (!s) return false;
    const [kind, i] = this.key;
    return kind === "ch" ? s.ch[i].so : s.bus[i].so;
  }
  paint(g) {
    const on = this.isOn();
    g.setColour(on ? soloColour : Palette.knobBody); g.fillRoundedRectangle(0, 0, this.w, this.h, 4);
    g.setColour(on ? Palette.onAccent(soloColour) : Palette.text); g.setFont(11, true);
    g.drawText("S", 0, 0, this.w, this.h, J.centred);
  }
  pointerDown() {
    if (C.isLocked && C.isLocked()) return false;
    const [kind, i] = this.key, on = !this.isOn();
    net.send({ t: "solo", s: kind + (i + 1), on });
    const s = S(); if (s) (kind === "ch" ? s.ch[i] : s.bus[i]).so = on;   // shown at once
    this.repaint(); return false;
  }
}

// RKMixEditor::TalkButton: held (momentary) or tapped on and off (latch).
class TalkButton extends Component {
  constructor() { super(); this.takesTouches = true; store.want("talk"); store.want("talk_latch"); }
  paint(g) {
    const on = store.on("talk");
    g.setColour(on ? Palette.warning : Palette.knobBody); g.fillRoundedRectangle(0, 0, this.w, this.h, 4);
    g.setColour(on ? Palette.onAccent(Palette.warning) : Palette.text); g.setFont(15, true);
    g.drawText("TALK", 0, 0, this.w, this.h, J.centred);
    const s = S();
    if (s && !s.talkSrc) { g.setColour(on ? Palette.onAccent(Palette.warning) : Palette.textDim); g.setFont(10); g.drawText("no source set", 0, this.h - 14, this.w, 14, J.centred); }
  }
  pointerDown() {
    const latch = store.on("talk_latch");
    store.hold("talk", true);
    store.set("talk", latch ? (store.on("talk") ? 0 : 1) : 1);
    return true;
  }
  pointerUp() { if (!store.on("talk_latch")) store.set("talk", 0); store.hold("talk", false); }
}

// ---------------------------------------------------------------- the strips
// RKMixEditor::Strip: a channel (a linked pair's left channel shows the pair) or a bus.
class Strip extends Component {
  constructor(kind, index) {
    super();
    this.kind = kind; this.index = index; this.takesTouches = true;
    const n = num(index);
    this.isBus = kind === "bus";
    this.meter = this.add(new Meter(Palette.accent));
    this.pan = this.add(new LinearSlider(this.isBus ? `bus${n}_pan` : `ch${n}_pan`));
    this.pan.popupText = () => store.text(this.pan.id);
    this.mute = this.add(new ToggleSwitch(this.isBus ? `bus_mute_${n}` : `ch_mute_${n}`, Palette.warning, "Mute"));
    this.solo = this.add(new SoloButton([kind, index]));
    this.inserts = [];
    for (let slot = 0; slot < (this.isBus ? 4 : 2); ++slot) this.inserts.push(this.add(new InsertCell(kind, index, slot)));
    this.sends = this.isBus ? this.add(new TextButton("Sends", () => C.setSendsOnFader(mix.sendsOnFader === index ? -1 : index))) : null;
    if (this.sends) { this.sends.fontHeight = 12; this.sends.visible = false; }
    this.fader = this.add(new MixFader(""));
    this.showSendTo(mix.sendsOnFader);
    for (const c of [this.pan, this.mute, this.fader]) c.locked = () => C.isLocked && C.isLocked("faders");
  }
  entry() { const s = S(); return s ? (this.isBus ? s.bus[this.index] : s.ch[this.index]) : null; }
  stereo() { const e = this.entry(); return !!(e && e.st); }
  // Sends on fader: the fader shows the send to @p bus (-1: the fader itself).
  showSendTo(bus) {
    this.sendBus = this.isBus ? -1 : bus;
    const n = num(this.index);
    this.fader.bind(this.sendBus >= 0 ? `ch${n}_send${this.sendBus + 1}` : this.isBus ? `bus_fader_${n}` : `ch_fader_${n}`);
    this.fader.capColour = this.sendBus >= 0 ? stripColour(S() ? S().bus[this.sendBus].c : 0) : Palette.knobTrack;
    this.bindPan();
    // The send's own EQ (an Aux or FX bus): a button above the fader.
    const s = S(), eqBus = !this.isBus && this.sendBus >= 0 && !!s && s.bus[this.sendBus].t !== 0;
    if (eqBus && C.SendEqButton) {
      if (!this.eq) this.eq = this.add(new C.SendEqButton(this.index, this.sendBus));
      else this.eq.set(this.index, this.sendBus);
    }
    if (this.eq) this.eq.setVisible(eqBus);
    if (this.w) this.resized();
  }
  bindPan() {
    const s = S(), n = num(this.index);
    const sendPan = !this.isBus && this.sendBus >= 0 && s && s.bus[this.sendBus].own;
    this.pan.bind(this.isBus ? `bus${n}_pan` : sendPan ? `ch${n}_span${this.sendBus + 1}` : `ch${n}_pan`);
    this.pan.trackColour = sendPan ? stripColour(s.bus[this.sendBus].c) : Palette.accent.withAlpha(0.5);
  }
  label() {
    if (!this.isBus) return C.channelLabel(this.index);
    const e = this.entry();
    return C.busTypeNames[e ? e.t : 1] + (e && !e.st ? " mono" : "");
  }
  resized() {
    let x = 3, w = this.w - 6, y = L.colourBar + 2;
    this.nameArea = { x, y, w, h: L.name }; y += L.name;
    this.numberArea = { y, h: L.number }; y += L.number;
    this.sectionRow = { x, y, w, h: L.section };
    if (this.sends) this.sends.setBounds(x, y - 3, w, L.section + 8);
    y += L.section + 4 + (this.sends ? 4 : 0);
    for (const b of this.inserts) { b.setBounds(x, y, w, L.insertRow); y += L.insertRow + 2; }
    y += 2;
    this.pan.setBounds(x + 2, y, w - 4, L.pan); y += L.pan + 6;
    const bw = w - 4, soloW = Math.floor(bw / 3);
    this.solo.setBounds(x + 2 + bw - soloW, y, soloW, L.button);
    this.mute.setBounds(x + 2, y, bw - soloW - 2, L.button); y += L.button + 8;
    if (this.eq && this.eq.visible) { this.eq.setBounds(x + 2, y, w - 4, 20); y += 24; }
    const fw = w - L.meter - 2;
    this.fader.setBounds(x, y, fw, this.h - y);
    this.meter.setBounds(x + fw + 2, y, L.meter, this.h - y - 18);
    this.meter.range = this.fader.trackRange();
  }
  paint(g) {
    const e = this.entry(); if (!e) return;
    const sel = isSelected(this.kind, this.index), showingSend = this.sendBus >= 0;
    const s = S();
    this.meter.bars = this.stereo() ? 2 : 1;
    if (this.isBus) {
      this.pan.visible = e.t !== 1;
      if (this.sends) { this.sends.visible = e.t !== 0; this.sends.toggle = mix.sendsOnFader === this.index; }
    }
    if (showingSend) this.fader.capColour = stripColour(s.bus[this.sendBus].c);
    g.setColour(sel ? Palette.accent.withAlpha(0.16) : showingSend ? stripColour(s.bus[this.sendBus].c).withAlpha(0.12) : Palette.tintedPanel);
    g.fillRoundedRectangle(0, 0, this.w, this.h, 6);
    if (sel) { g.setColour(Palette.accent); g.drawRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, 6, 1); }

    g.setColour(stripColour(e.c)); g.fillRoundedRectangle(4, 2, this.w - 8, L.colourBar, 2);
    // the name (bold 12, squeezed to 60%)
    g.setColour(Palette.text); g.setFont(12, true);
    const a = this.nameArea; g.drawFittedText(this.isBus ? C.busName(this.index) : C.channelName(this.index), a.x + 2, a.y, a.w - 4, a.h, J.centred, 1, 0.6);
    // the number; on orange while a DCA or mute group silences it
    const nr = this.numberArea;
    if (e.gm) { g.setColour(Palette.warning.withAlpha(0.8)); g.fillRoundedRectangle(6, nr.y, this.w - 12, nr.h, 3); }
    g.setColour(e.gm ? Palette.text : Palette.textDim); g.setFont(11);
    g.drawText(this.label(), 0, nr.y, this.w, nr.h, J.centred);
    if (this.isBus) return;
    // Sections in use: H G E C, the gate's and the compressor's lit while they reduce
    const letters = ["H", "G", "E", "C"], r = this.sectionRow, cw = Math.floor(r.w / 4);
    g.setFont(10.5, true);
    letters.forEach((l, i) => {
      const active = (i === 1 && this.reducing && this.reducing[0]) || (i === 3 && this.reducing && this.reducing[1]);
      g.setColour(active ? Palette.warning : e.sec[i] ? Palette.accent.brighter(0.3) : Palette.knobTrack);
      g.drawText(l, r.x + i * cw, r.y, cw, r.h, J.centred);
    });
  }
  pointerDown(e) {
    // The name and number select the strip (the Channel page shows it); the
    // section letters open it there.
    if (e.y < this.numberArea.y + this.numberArea.h) {
      select(this.kind, this.index);
      if (e.taps >= 2 && C.renameStrip) C.renameStrip(this.kind, this.index, this);
      return false;
    }
    if (!this.isBus && e.y < this.sectionRow.y + this.sectionRow.h + 2) { select(this.kind, this.index); if (C.root.page("strip")) C.root.showPage("strip"); return false; }
    return false;
  }
  meters(m, dt) {
    if (this.isBus) this.meter.setPeaks(m.b[2 * this.index], m.b[2 * this.index + 1], dt);
    else {
      this.meter.setPeaks(m.c[this.index], this.stereo() ? m.c[this.index + 1] : -100, dt);
      const now = [m.g[this.index] < -1, m.k[this.index] < -1];
      if (!this.reducing || now[0] !== this.reducing[0] || now[1] !== this.reducing[1]) { this.reducing = now; app.dirty = true; }
    }
  }
}

// RKMixEditor::MasterStrip: the main or the mono sum.
class MasterStrip extends Component {
  constructor(chain) {
    super(); this.chain = chain; this.takesTouches = true;
    this.inserts = [];
    for (let slot = 0; slot < (chain === "main" ? 4 : 2); ++slot) this.inserts.push(this.add(new InsertCell(chain, 0, slot)));
    this.mute = this.add(new ToggleSwitch(chain + "_mute", Palette.warning, "Mute"));
    this.fader = this.add(new MixFader(chain + "_fader"));
    this.meter = this.add(new Meter(Palette.accent));
    this.meter.bars = chain === "main" ? 2 : 1;
    for (const c of [this.mute, this.fader]) c.locked = () => C.isLocked && C.isLocked("faders");
  }
  resized() {
    let x = 6, w = this.w - 12, y = L.colourBar + 2 + L.name + L.number + 4;
    for (const b of this.inserts) { b.setBounds(x, y, w, L.insertRow); y += L.insertRow + 2; }
    y = L.colourBar + 2 + L.name + L.number + 4 + 4 * (L.insertRow + 2) + 4;
    this.mute.setBounds(x + 4, y, w - 8, L.button); y += L.button + 8;
    const mw = 2 * L.meter + 1;
    this.fader.setBounds(x, y, w - mw - 4, this.h - y);
    this.meter.setBounds(x + w - mw, y, mw, this.h - y - 18);
    this.meter.range = this.fader.trackRange();
  }
  paint(g) {
    const sel = isSelected(this.chain, 0), s = S();
    g.setColour(sel ? Palette.accent.withAlpha(0.16) : Palette.panel.brighter(0.06)); g.fillRoundedRectangle(0, 0, this.w, this.h, 6);
    if (sel) { g.setColour(Palette.accent); g.drawRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, 6, 1); }
    g.setColour(Palette.text); g.setFont(13, true);
    g.drawText(this.chain === "main" ? "Main" : "Mono", 0, L.colourBar + 2, this.w, L.name, J.centred);
    g.setColour(Palette.textDim); g.setFont(11);
    g.drawText("→ " + (s ? s[this.chain].out : ""), 0, L.colourBar + 2 + L.name, this.w, L.number, J.centred);
  }
  pointerDown(e) { if (e.y < L.colourBar + 2 + L.name + L.number) select(this.chain, 0); return false; }
  meters(m, dt) { if (this.chain === "main") this.meter.setPeaks(m.mn[0], m.mn[1], dt); else this.meter.setPeaks(m.mo, -100, dt); }
}

// RKMixEditor::GroupStrip: a DCA (name, Spill, mute, fader) or a mute group (name, its big Mute, its members).
class GroupStrip extends Component {
  constructor(index, muteGroup) {
    super(); this.index = index; this.muteGroup = muteGroup; this.takesTouches = true;
    const n = num(index);
    if (muteGroup) this.mute = this.add(new ToggleSwitch(`mgrp_${n}`, Palette.warning, "MUTE"));
    else {
      this.mute = this.add(new ToggleSwitch(`dca_mute_${n}`, Palette.warning, "Mute"));
      this.fader = this.add(new MixFader(`dca_fader_${n}`));
      this.fader.locked = () => C.isLocked && C.isLocked("faders");
      this.spill = this.add(new TextButton("Spill", () => { mix.spill = mix.spill === index ? -1 : index; page.rebuild(); }));
    }
    this.mute.locked = () => C.isLocked && C.isLocked("faders");
  }
  members() { const s = S(); return s ? (this.muteGroup ? s.mg[this.index].m : s.dca[this.index].m) : []; }
  resized() {
    let x = 3, w = this.w - 6, y = L.colourBar + 2 + L.name + L.number + 4;
    if (this.muteGroup) {
      this.mute.setBounds(x + 2, y, w - 4, 64); y += 64 + 8;
      this.membersArea = { x: x + 2, y, w: w - 4, h: this.h - y };
      return;
    }
    this.spill.setBounds(x + 2, y, w - 4, L.button); y += L.button + 2;
    this.membersArea = { x, y, w, h: L.section * 3 }; y += L.section * 3 + 4;
    this.mute.setBounds(x + 2, y, w - 4, L.button); y += L.button + 8;
    this.fader.setBounds(x, y, w - L.meter - 2, this.h - y);
  }
  paint(g) {
    const s = S(); if (!s) return;
    const sel = isSelected(this.muteGroup ? "mg" : "dca", this.index);
    g.setColour(sel ? Palette.accent.withAlpha(0.16) : Palette.tintedPanel); g.fillRoundedRectangle(0, 0, this.w, this.h, 6);
    if (sel) { g.setColour(Palette.accent); g.drawRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, 6, 1); }
    g.setColour(this.muteGroup ? Palette.warning.withAlpha(0.7) : stripColour(s.dca[this.index].c));
    g.fillRoundedRectangle(4, 2, this.w - 8, L.colourBar, 2);
    g.setColour(Palette.text); g.setFont(12, true);
    g.drawFittedText(this.muteGroup ? C.muteGroupName(this.index) : C.dcaName(this.index), 5, L.colourBar + 2, this.w - 10, L.name, J.centred, 1, 0.6);
    g.setColour(Palette.textDim); g.setFont(11);
    g.drawText((this.muteGroup ? "Mute " : "DCA ") + num(this.index), 0, L.colourBar + 2 + L.name, this.w, L.number, J.centred);
    const m = this.members(), a = this.membersArea;
    if (this.spill) { this.spill.toggle = mix.spill === this.index; this.spill.enabled = m.length > 0; }
    g.setFont(10.5);
    if (!this.muteGroup) {
      g.drawFittedText(m.length ? `${m.length} ${m.length === 1 ? "member" : "members"}` : "no members", a.x, a.y, a.w, a.h, J.centred, 2);
      return;
    }
    if (!m.length) { g.drawFittedText("No members", a.x, a.y, a.w, a.h, J.centredTop, 4); return; }
    const rows = Math.max(1, Math.floor(a.h / 14));
    for (let i = 0; i < m.length && i < rows; i++) {
      const more = i === rows - 1 && m.length > rows;
      g.drawText(more ? `+ ${m.length - i} more` : C.stripName(m[i]), a.x, a.y + i * 14, a.w, 14, J.centred);
    }
  }
  pointerDown(e) {
    if (e.y < L.colourBar + 2 + L.name + L.number) {
      select(this.muteGroup ? "mg" : "dca", this.index);
      if (e.taps >= 2 && C.renameStrip) C.renameStrip(this.muteGroup ? "mg" : "dca", this.index, this);
    }
    return false;
  }
}

// ---------------------------------------------------------------- the page
class MixPage extends Component {
  constructor() {
    super();
    this.strips = [];
    this.layerButtons = kLayers.map((t, i) => this.add(new TextButton(t, () => { mix.layer = i; mix.spill = -1; this.rebuild(); })));
    this.sceneButtons = [];
    for (let i = 0; i < 16; i++) this.sceneButtons.push(this.add(new TextButton(num(i), () => { if (!C.isLocked("everything")) net.send({ t: "scene", n: i }); })));
    this.clearSolo = this.add(new TextButton("Clear solo", () => net.send({ t: "clearSolo" })));
    this.talk = this.add(new TalkButton());
    this.main = this.add(new MasterStrip("main"));
    this.mono = this.add(new MasterStrip("mono"));
    this.sendsBack = this.add(new TextButton("Back to faders", () => C.setSendsOnFader(-1)));
    this.key = "";
    C.listeners.structure.add(() => this.structureChanged());
    C.listeners.meters.add((m, dt) => this.meters(m, dt));
  }
  shown() { this.rebuild(); }
  // The strips the layer shows (as RKMixEditor::layoutStrips).
  layerStrips() {
    const s = S(); if (!s) return [];
    const L = mix.layer, out = [];
    if (L === kDcaLayer && mix.spill < 0) {
      for (let d = 0; d < 8; d++) out.push(["dca", d]);
      for (let g = 0; g < 8; g++) out.push(["mg", g]);
      return out;
    }
    if (L >= kUserLayer || L === kDcaLayer) {
      let list = L === kDcaLayer ? s.dca[mix.spill].m.slice(0, 15) : s.up[L - kUserLayer] || [];
      for (const id of list) out.push(id >= C.kUserPageBus ? ["bus", id - C.kUserPageBus] : ["ch", id]);
      if (L === kDcaLayer) out.push(["dca", mix.spill, "right"]);
      return out;
    }
    if (L === kBusLayer) { for (let b = 0; b < 16; b++) out.push(["bus", b]); return out; }
    for (let ch = L * 16; ch < L * 16 + 16; ch++) if (!s.ch[ch].hid) out.push(["ch", ch]);
    return out;
  }
  structureChanged() {
    const key = JSON.stringify(this.layerStrips()) + "|" + mix.layer + "|" + mix.spill;
    if (key !== this.key) this.rebuild();
    for (const st of this.strips) if (st.bindPan) st.bindPan();
  }
  rebuild() {
    this.key = JSON.stringify(this.layerStrips()) + "|" + mix.layer + "|" + mix.spill;
    for (const st of this.strips) this.remove(st);
    store.beginWants();
    this.strips = this.layerStrips().map(([kind, i, where]) => {
      const st = kind === "dca" || kind === "mg" ? new GroupStrip(i, kind === "mg") : new Strip(kind, i);
      st.right = where === "right";
      return this.add(st);
    });
    collectWants(this);
    for (const id of ["talk", "talk_latch"]) store.want(id);
    if (C.extraWants) C.extraWants();
    store.endWants();
    this.resized(); app.dirty = true;
  }
  resized() {
    const W = this.w, H = this.h, gap = L.gap;
    const bottomY = H - L.bottom;
    // the bottom row: layers, then the scenes
    let x = 12;
    const widths = [104, 104, 76, 60, 68, 68];
    this.layerButtons.forEach((b, i) => { b.setBounds(x, bottomY + 12, widths[i], 32); x += widths[i] + 6; });
    x += 56;   // "SCENE"
    this.sceneX = x - 54;
    const sw = Math.max(26, Math.min(40, Math.floor((W - 12 - x) / 16) - 3));
    for (const b of this.sceneButtons) { b.setBounds(x, bottomY + 12, sw, 32); x += sw + 3; }
    // the master column
    const mx = W - L.master - gap, top = gap;
    this.clearSolo.setBounds(mx, top, L.master, 36);
    this.sendsBack.setBounds(mx, top, L.master, 36);
    if (mix.sendsOnFader >= 0) { this.caption = { x: mx, y: top + 38, w: L.master, h: 22 }; this.talk.setBounds(mx, top + 62, L.master, 34); }
    else this.talk.setBounds(mx, top + 42, L.master, 52);
    const stripTop = top + 102, half = Math.floor(L.master / 2) + 10;
    this.main.setBounds(mx, stripTop, half - 4, bottomY - gap - stripTop);
    this.mono.setBounds(mx + half + 4, stripTop, L.master - half - 4, bottomY - gap - stripTop);
    // the strips
    const area = { x: gap, y: top, w: mx - 2 * gap, h: bottomY - gap - top };
    const sw16 = Math.floor((area.w - 15 * 4) / 16);
    let sx = area.x;
    for (const st of this.strips) {
      if (st.right) st.setBounds(area.x + area.w - sw16, area.y, sw16, area.h);
      else { st.setBounds(sx, area.y, sw16, area.h); sx += sw16 + 4; }
    }
  }
  paint(g) {
    const s = S(), H = this.h, bottomY = H - L.bottom;
    // EditorBase's bottom row
    g.setColour(Palette.tintedPanel); g.fillRect(0, bottomY, this.w, L.bottom);
    g.setColour(Palette.outline); g.fillRect(0, bottomY, this.w, 1);
    this.layerButtons.forEach((b, i) => { b.toggle = mix.layer === i; });
    g.setColour(Palette.textDim); g.setFont(12, true);
    g.drawText("SCENE", this.sceneX - 4, bottomY + 12, 50, 32, J.centredRight);
    if (s) this.sceneButtons.forEach((b, i) => { b.toggle = s.scene === i; });
    const sending = mix.sendsOnFader >= 0;
    this.clearSolo.visible = !sending; this.sendsBack.visible = sending;
    this.clearSolo.enabled = !!(s && s.anySolo);
    this.clearSolo.colour = s && s.anySolo ? soloColour.withAlpha(0.35) : Palette.knobBody;
    if (sending && this.caption) {
      const c = this.caption, colour = stripColour(s.bus[mix.sendsOnFader].c);
      this.sendsBack.colour = colour.withAlpha(0.35);
      g.setColour(colour.brighter(0.2)); g.setFont(13, true);
      g.drawFittedText("Faders: sends to " + C.busName(mix.sendsOnFader), c.x, c.y, c.w, c.h, J.centred, 1, 0.8);
    }
    // an empty user page
    if (mix.layer >= kUserLayer && !this.strips.length) {
      g.setColour(Palette.textDim); g.setFont(14);
      g.drawText(`User ${mix.layer - kUserLayer + 1} is empty: choose its channels and buses on the Setup page (or in RKMix's window).`,
                 0, 0, this.w - L.master - 16, bottomY, J.centred);
    }
  }
  meters(m, dt) {
    for (const st of this.strips) if (st.meters) st.meters(m, dt);
    this.main.meters(m, dt); this.mono.meters(m, dt);
  }
}

// Sends on fader (an Aux or FX bus's Sends button): every channel's fader
// becomes its send to that bus, and the pans its send pans in an Own bus.
C.setSendsOnFader = bus => {
  mix.sendsOnFader = bus;
  if (bus >= 0 && (mix.layer === kBusLayer || mix.layer === kDcaLayer)) { mix.layer = 0; mix.spill = -1; }
  page.rebuild();
};
function collectWants(c) { if (c.id) store.want(c.id); for (const ch of c.children) collectWants(ch); }
C.collectWants = collectWants;

let page;
C.addPage("mix", "Mix", () => (page = new MixPage()));
})();
