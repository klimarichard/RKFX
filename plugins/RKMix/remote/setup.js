// RKMix's console remote: the rest of the console. The Setup page (solo and
// the cue output, talkback, the oscillator as in the window's Master section
// and Talk / Osc page; the DCAs' and mute groups' members as GroupPanel; the
// user pages), a bus's setup and the main's routing on the Channel page
// (BusPanel, MainPanel), names, and the iPad's own lock.
"use strict";

(() => {
const { Palette, J, Component, TextButton, ToggleSwitch, SegmentedSwitch, Knob, LinearSlider, ChoiceBox, PopupMenu, app, store,
        stripColour, drawPanel, editText } = RK;
const { state, net, num } = C;
const accent = Palette.accent;
const SW = 30;   // switch height

function S() { return state.structure; }
function targetName(t) {
  if (t <= 0) return "Off";
  if (t >= 33) { const f = (t - 33) * 2 + 1; return `Out ${f}/${f + 1}`; }
  return "Out " + t;
}
const tapNames = ["Input", "Pre-EQ", "Pre-Comp", "Pre-Inserts", "Pre-Fader", "Post-Fader"];
function inner(a) { return { x: a.x + 8, y: a.y + 22, w: a.w - 16, h: a.h - 24 }; }

// ---------------------------------------------------------------- the lock
// Off, faders and mutes, or everything. While locked, hold the button a second to unlock.
let lock = 0;
C.isLocked = (kind = "faders") => kind === "everything" ? lock >= 2 : lock >= 1;
class LockButton extends TextButton {
  constructor() { super("Lock"); this.width = 110; this.holdStart = 0; }
  paint(g) {
    this.text = lock ? (lock === 2 ? "Locked" : "Faders locked") : "Lock";
    this.toggle = lock > 0; this.onColour = Palette.warning;
    super.paint(g);
    if (this.holdStart) {
      const p = Math.min(1, (performance.now() - this.holdStart) / 1000);
      g.setColour(Palette.text.withAlpha(0.5)); g.fillRect(4, this.h - 5, (this.w - 8) * p, 3);
      app.dirty = true;
      if (p >= 1) { this.holdStart = 0; lock = 0; app.dirty = true; }
    }
  }
  pointerDown(e) {
    if (lock) { this.holdStart = performance.now(); this.down = true; return true; }
    PopupMenu.show([{ text: "Lock faders and mutes", action: () => { lock = 1; app.dirty = true; } },
                    { text: "Lock everything", action: () => { lock = 2; app.dirty = true; } },
                    { separator: true }, { header: true, text: "Hold Lock a second to unlock" }], this, { minWidth: 240 });
    return false;
  }
  pointerUp() { this.holdStart = 0; this.down = false; this.repaint(); }
}

// ---------------------------------------------------------------- names
C.renameStrip = (kind, index, target) => {
  if (C.isLocked("everything")) return;
  const name = kind === "ch" ? (S().ch[index].n || "") : kind === "bus" ? (S().bus[index].n || "")
             : kind === "dca" ? (S().dca[index].n || "") : (S().mg[index].n || "");
  editText(target, name, text => net.send({ t: "name", s: kind + (index + 1), text }), { fontHeight: 14, bold: true });
};

// ---------------------------------------------------------------- a bus's setup, the main's routing
// (on the Channel page, beside the compressor and limiter)
C.buildSetup = (page, chain) => {
  const x = page.extra, add = c => { c.locked = () => C.isLocked("everything"); return page.add(c); };
  x.setup = [];
  if (chain < 16) {
    const id = s => `bus${chain + 1}_${s}`, b = S().bus[chain];
    x.type = add(new SegmentedSwitch(id("type"), accent, ["Group", "Aux", "FX"], "Type"));
    x.format = add(new SegmentedSwitch(id("format"), accent, ["Mono", "Stereo"], "Format"));
    x.output = add(new ChoiceBox(id("out"), targetName)); x.output.offered = t => t === 0 || (t >= 33) === b.st;
    x.toMain = b.t !== 1 ? add(new ToggleSwitch(id("main"), accent, "Main")) : null;
    x.defTap = b.t === 1 ? add(new ChoiceBox(id("tap"), i => tapNames[i])) : null;
    x.panMode = b.t === 1 && b.st ? add(new SegmentedSwitch(id("panmode"), accent, ["Own", "Follow", "Mirror"], "Pan")) : null;
    x.busPan = b.t !== 1 ? add(new LinearSlider(id("pan"))) : null;
    if (x.busPan) x.busPan.popupText = () => store.text(id("pan"));
    x.soloSafe = add(new SegmentedSwitch(id("solosafe"), accent, ["Auto", "On", "Off"], "Solo safe"));
    x.recallSafe = add(new ToggleSwitch(id("recallsafe"), accent, "Recall safe"));
    x.sendsOnFader = b.t !== 0 ? page.add(new TextButton("Sends on fader", () => { C.setSendsOnFader(C.mix.sendsOnFader === chain ? -1 : chain); C.root.showPage("mix"); })) : null;
  } else {
    x.mainOut = add(new ChoiceBox("main_out", targetName)); x.mainOut.offered = t => t === 0 || t >= 33;
    x.monoOut = add(new ChoiceBox("mono_out", targetName)); x.monoOut.offered = t => t < 33;
    x.balance = add(new Knob("main_bal", "Balance"));
    x.midiThru = add(new ToggleSwitch("midi_thru", accent, "MIDI thru"));
  }
};
C.layoutSetup = (page, a, section) => {
  const x = page.extra, r = inner(section(page.chain < 16 ? "Bus setup" : "Routing", a));
  if (page.chain < 16) {
    x.type.setBounds(r.x, r.y, 210, 50);
    x.format.setBounds(r.x + 222, r.y, 140, 50);
    page.labels = [["Output", r.x + 374, r.y], ["Default tap", r.x + 374 + 150, r.y]];
    x.output.setBounds(r.x + 374, r.y + 20, 140, SW);
    if (x.defTap) x.defTap.setBounds(r.x + 374 + 150, r.y + 20, 140, SW);
    else page.labels.pop();
    if (x.toMain) x.toMain.setBounds(r.x + 374 + 150, r.y + 20, 80, SW);
    const y2 = r.y + 70;
    if (x.panMode) x.panMode.setBounds(r.x, y2, 210, 50);
    if (x.busPan) { x.busPan.setBounds(r.x, y2 + 24, 210, 24); page.labels.push(["Pan", r.x, y2]); }
    x.soloSafe.setBounds(r.x + 222, y2, 190, 50);
    x.recallSafe.setBounds(r.x + 424, y2 + 20, 110, SW);
    if (x.sendsOnFader) x.sendsOnFader.setBounds(r.x, y2 + 70, 210, 36);
  } else {
    page.labels = [["Main output", r.x, r.y], ["Mono output", r.x + 160, r.y]];
    x.mainOut.setBounds(r.x, r.y + 20, 140, SW);
    x.monoOut.setBounds(r.x + 160, r.y + 20, 140, SW);
    x.balance.setBounds(r.x + 320, r.y, 58, 80);
    x.midiThru.setBounds(r.x + 400, r.y + 20, 100, SW);
  }
};

// ---------------------------------------------------------------- the Setup page
class SetupPage extends Component {
  constructor() {
    super();
    this.group = 0;      // 0..7 DCAs, 8..15 mute groups
    this.userPage = 0;
    C.listeners.structure.add(() => { if (this.parent) this.structureChanged(); });
  }
  shown() { this.rebuild(); }
  structureChanged() { const k = JSON.stringify(S().bus.map(b => b.t)); if (k !== this.key) this.rebuild(); }
  rebuild() {
    const s = S(); if (!s) return;
    this.key = JSON.stringify(s.bus.map(b => b.t));
    this.removeAll();
    const add = c => { c.locked = () => C.isLocked("everything"); return this.add(c); };
    store.beginWants();
    const c = this.c = {};
    // Solo
    c.soloMode = add(new SegmentedSwitch("solo_mode", accent, ["Cue", "In place"], "Mode"));
    c.soloTap = add(new SegmentedSwitch("solo_tap", accent, ["PFL", "AFL"], "Cue listens"));
    c.cueOut = add(new ChoiceBox("cue_out", targetName));
    c.cueSrc = add(new ChoiceBox("cue_src", i => i === 0 ? "Silence" : i === 1 ? "Main" : C.busName(i - 2)));
    // Talkback
    c.talkSrc = add(new ChoiceBox("talk_src", v => v === 0 ? "Off" : "In " + v)); c.talkSrc.separatorBefore = 1;
    c.talkLevel = add(new Knob("talk_level", "Level"));
    c.talkLatch = add(new ToggleSwitch("talk_latch", accent, "Latch"));
    c.talkMain = add(new ToggleSwitch("talk_main", accent, "Main"));
    c.talkTo = [];
    for (let b = 0; b < 16; b++) { const t = add(new ToggleSwitch("talk_to" + num(b), accent, num(b))); t.enabled = s.bus[b].t === 1; c.talkTo.push(t); }
    // Oscillator
    c.oscOn = add(new ToggleSwitch("osc_on", Palette.warning, "On"));
    c.oscType = add(new SegmentedSwitch("osc_type", accent, ["Sine", "Pink"]));
    c.oscFreq = add(new Knob("osc_freq", "Freq"));
    c.oscLevel = add(new Knob("osc_level", "Level"));
    c.oscOut = add(new ChoiceBox("osc_out", targetName)); c.oscOut.separatorBefore = 33;
    c.oscMain = add(new ToggleSwitch("osc_main", accent, "Main"));
    c.oscTo = [];
    for (let b = 0; b < 16; b++) c.oscTo.push(add(new ToggleSwitch("osc_to" + num(b), accent, num(b))));
    // Groups: pick a DCA or a mute group, then its members
    c.groupButtons = [];
    for (let i = 0; i < 16; i++) {
      const b = this.add(new TextButton(i < 8 ? C.dcaName(i) : C.muteGroupName(i - 8), () => { this.group = i; this.rebuild(); }));
      b.fontHeight = 12; b.toggle = i === this.group; c.groupButtons.push(b);
    }
    const mg = this.group >= 8, gi = this.group % 8, colour = mg ? Palette.warning : accent;
    c.members = [];
    for (let ch = 0; ch < 32; ch++) c.members.push(add(new ToggleSwitch(`ch${ch + 1}_${mg ? "mgrp" : "dca"}${gi + 1}`, colour, num(ch))));
    for (let b = 0; b < 16; b++) c.members.push(add(new ToggleSwitch(`bus${b + 1}_${mg ? "mgrp" : "dca"}${gi + 1}`, colour, "B" + num(b))));
    c.rename = this.add(new TextButton("Rename", () => C.renameStrip(mg ? "mg" : "dca", gi, c.rename)));
    c.clear = this.add(new TextButton("Clear", () => { if (!C.isLocked("everything")) for (const m of c.members) store.set(m.id, 0); }));
    // User pages: pick one, then its strips in the order tapped
    c.userButtons = [0, 1].map(p => { const b = this.add(new TextButton("User " + (p + 1), () => { this.userPage = p; this.rebuild(); })); b.toggle = p === this.userPage; return b; });
    c.userStrips = [];
    for (let id = 0; id < 32; id++) c.userStrips.push(this.add(new UserToggle(this, id)));
    for (let b = 0; b < 16; b++) c.userStrips.push(this.add(new UserToggle(this, C.kUserPageBus + b)));
    c.userClear = this.add(new TextButton("Clear", () => { if (!C.isLocked("everything")) net.send({ t: "userPage", p: this.userPage, list: [] }); }));
    C.collectWants(this);
    store.endWants();
    this.resized(); app.dirty = true;
  }
  resized() {
    const c = this.c; if (!c) return;
    const W = this.w, gap = 8;
    this.sections = [];
    const section = (title, x, y, w, h) => { const a = { title, x, y, w, h }; this.sections.push(a); return inner(a); };
    this.labels = [];
    // Row 1: solo | talkback
    let a = section("Solo and cue", 8, 8, 470, 120);
    c.soloMode.setBounds(a.x, a.y, 150, 50); c.soloTap.setBounds(a.x + 160, a.y, 110, 50);
    c.cueOut.setBounds(a.x + 280, a.y + 20, 166, SW); this.labels.push(["Cue output", a.x + 280, a.y]);
    c.cueSrc.setBounds(a.x + 280, a.y + 20 + SW + 22, 166, SW); this.labels.push(["Cue with no solo", a.x + 280, a.y + SW + 22]);
    const grid = (list, gx, gy, gw) => { const w = (gw - 7 * 4) / 8; list.forEach((t, i) => t.setBounds(gx + (i % 8) * (w + 4), gy + Math.floor(i / 8) * (SW + 6), w, SW)); };
    a = section("Talkback", 8 + 470 + gap, 8, W - 16 - 470 - gap, 120);
    c.talkSrc.setBounds(a.x, a.y + 20, 110, SW); this.labels.push(["Source (input)", a.x, a.y]);
    c.talkLevel.setBounds(a.x + 120, a.y, 58, 80); c.talkLatch.setBounds(a.x + 186, a.y + 20, 70, SW);
    c.talkMain.setBounds(a.x + a.w - 60, a.y + 20, 60, SW);
    grid(c.talkTo, a.x + 270, a.y + 20, a.w - 270 - 72);
    this.labels.push(["Into the Aux buses (and the main)", a.x + 270, a.y]);
    // Row 2: oscillator
    a = section("Oscillator", 8, 136, W - 16, 120);
    c.oscOn.setBounds(a.x, a.y + 20, 50, SW); c.oscType.setBounds(a.x + 58, a.y + 20, 110, SW);
    c.oscFreq.setBounds(a.x + 176, a.y, 58, 80); c.oscLevel.setBounds(a.x + 238, a.y, 58, 80);
    c.oscOut.setBounds(a.x + 306, a.y + 20, 120, SW); this.labels.push(["Straight to", a.x + 306, a.y]);
    c.oscMain.setBounds(a.x + a.w - 60, a.y + 20, 60, SW);
    grid(c.oscTo, a.x + 440, a.y + 20, a.w - 440 - 72);
    this.labels.push(["Into buses, the main, or straight to an output", a.x + 440, a.y]);
    // Row 3: groups
    const memberGrid = (list, gx, gy, gw) => {
      const w = (gw - 15 * 4) / 16;
      list.forEach((t, i) => t.setBounds(gx + (i % 16) * (w + 4), gy + Math.floor(i / 16) * (SW + 6), w, SW));
    };
    a = section("DCAs and mute groups: members", 8, 264, W - 16, 26 + 34 + 3 * (SW + 6) + 34);
    const bw = (a.w - 15 * 4 - 180) / 16;
    c.groupButtons.forEach((b, i) => b.setBounds(a.x + i * (bw + 4), a.y + 8, bw, 30));
    c.rename.setBounds(a.x + a.w - 172, a.y + 8, 84, 30); c.clear.setBounds(a.x + a.w - 84, a.y + 8, 84, 30);
    memberGrid(c.members, a.x, a.y + 46, a.w);
    this.groupArea = a;
    // Row 4: user pages
    const top4 = 264 + 26 + 34 + 3 * (SW + 6) + 34 + gap;
    a = section("User pages: the strips they show, in the order tapped", 8, top4, W - 16, this.h - top4 - 8);
    c.userButtons.forEach((b, i) => b.setBounds(a.x + i * 90, a.y, 84, 30));
    c.userClear.setBounds(a.x + a.w - 84, a.y, 84, 30);
    memberGrid(c.userStrips, a.x, a.y + 40, a.w);
  }
  paint(g) {
    const c = this.c; if (!c) return;
    c.oscFreq.dimmed = store.value("osc_type") >= 0.5;
    for (const sec of this.sections || []) drawPanel(g, sec.x, sec.y, sec.w, sec.h, Palette.tintedPanel, sec.title);
    g.setColour(Palette.textDim); g.setFont(11.5);
    for (const [t, x, y] of this.labels || []) g.drawText(t, x, y, 300, 16, J.centredLeft);
    // the DCA's colour
    const s = S();
    c.groupButtons.forEach((b, i) => { g.setColour(i < 8 ? stripColour(s.dca[i].c) : Palette.warning.withAlpha(0.7)); g.fillRoundedRectangle(b.x + 4, b.y - 4, b.w - 8, 3, 1.5); });
  }
}

// A strip on a user page: on when the page shows it; its place in the order.
class UserToggle extends Component {
  constructor(page, id) { super(); this.page = page; this.stripId = id; this.takesTouches = true; }
  list() { const s = S(); return s ? (s.up[this.page.userPage] || []) : []; }
  hidden() { return this.stripId < 32 && S().ch[this.stripId].hid; }
  paint(g) {
    const i = this.list().indexOf(this.stripId), on = i >= 0, a = this.hidden() ? 0.3 : 1;
    g.setColour((on ? accent : Palette.knobBody).withMultipliedAlpha(a)); g.fillRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, 4);
    g.setColour((on ? Palette.onAccent(accent) : Palette.text).withMultipliedAlpha(a)); g.setFont(12);
    const label = this.stripId >= C.kUserPageBus ? "B" + (this.stripId - C.kUserPageBus + 1) : String(this.stripId + 1);
    g.drawFittedText(label, 2, 0, this.w - 4, on ? this.h - 8 : this.h, J.centred, 1);
    if (on) { g.setFont(9); g.drawText(String(i + 1), 0, this.h - 11, this.w, 10, J.centred); }
  }
  pointerDown() {
    if (this.hidden() || C.isLocked("everything")) return false;
    const list = this.list().slice(), i = list.indexOf(this.stripId);
    if (i >= 0) list.splice(i, 1); else if (list.length < 16) list.push(this.stripId);
    net.send({ t: "userPage", p: this.page.userPage, list });
    const s = S(); s.up[this.page.userPage] = list; app.dirty = true;
    return false;
  }
}

C.addPage("setup", "Setup", () => new SetupPage());

// The lock in the header
const install = () => {
  const h = C.root.header, l = new LockButton(), f = new RK.FineButton();
  RK.fine.load("rkmix.console.fine");
  h.extra.push(l, f); h.add(l); h.add(f); h.resized();
};
C.afterStart = install;
})();
