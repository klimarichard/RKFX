// RKMix's console remote: the inserts. Tapping an empty slot picks a plugin
// (by family, as the window's menu); tapping a filled one opens it over the
// console (RKMixEditor::InsertView): On, Copy, Paste, Replace, Remove, and
// its controls where the plugin's window on the computer has them.
"use strict";

(() => {
const { Palette, J, Component, TextButton, Knob, Fader, ToggleSwitch, SegmentedSwitch, ChoiceBox, app, store, Colour, drawPanel } = RK;
const { state, net } = C;

function S() { return state.structure; }
const stripKey = (strip, index) => strip === "ch" ? "ch" + (index + 1) : strip === "bus" ? "bus" + (index + 1) : strip;
function stripName(strip, index) { return strip === "ch" ? C.channelName(index) : strip === "bus" ? C.busName(index) : strip === "main" ? "Main" : "Mono"; }
function entryOf(strip, index, slot) {
  const s = S(); if (!s) return null;
  const list = strip === "ch" ? s.ch[index].ins : strip === "bus" ? s.bus[index].ins : s[strip].ins;
  return list ? list[slot] : null;
}

// A dimmed backdrop and a panel over the content area.
class Sheet extends Component {
  constructor() { super(); this.takesTouches = true; }
  area() { return { x: 8, y: C.kHeader + 8, w: this.w - 16, h: this.h - C.kHeader - 16 }; }
  paint(g) {
    g.setColour(Colour.hex(0x99000000)); g.fillRect(0, 0, this.w, this.h);
    const a = this.area();
    g.setColour(Palette.tintedPanel.darker(0.15)); g.fillRoundedRectangle(a.x, a.y, a.w, a.h, 6);
    g.setColour(Palette.outline); g.drawRoundedRectangle(a.x + 0.5, a.y + 0.5, a.w - 1, a.h - 1, 6, 1);
  }
  close() { app.pop(this); store.extra.clear(); store.endWants(); if (this.onClose) this.onClose(); }
}

// The plugin picker: every type, a column per family.
class Picker extends Sheet {
  constructor(strip, index, slot, replacing) {
    super();
    this.strip = strip; this.index = index; this.slot = slot;
    this.title = (replacing ? "Replace " : "Add to ") + stripName(strip, index) + ", insert " + (slot + 1);
    this.families = [];
    for (const t of state.types || []) {
      let f = this.families.find(f => f.name === t.fam);
      if (!f) this.families.push(f = { name: t.fam, colour: Colour.hex(t.f), types: [] });
      f.types.push(t);
    }
    this.buttons = [];
    for (const f of this.families)
      for (const t of f.types) {
        const b = this.add(new TextButton(t.n.replace(/^RK/, ""), () => { this.close(); net.send({ t: "insAdd", s: stripKey(strip, index), n: slot, p: t.n }); C.openInsert(strip, index, slot, null, true); }));
        b.fontHeight = 13; b.family = f; this.buttons.push(b);
      }
    this.closeButton = this.add(new TextButton("Cancel", () => this.close()));
    if (state.clipboard && state.clipKind === "insert") this.paste = this.add(new TextButton("Paste " + state.clipboard, () => { this.close(); net.send({ t: "insPaste", s: stripKey(strip, index), n: slot }); }));
  }
  resized() {
    const a = this.area(), cols = Math.max(1, this.families.length), cw = Math.floor((a.w - 24 - (cols - 1) * 8) / cols);
    this.closeButton.setBounds(a.x + a.w - 12 - 100, a.y + 10, 100, 32);
    if (this.paste) this.paste.setBounds(a.x + a.w - 12 - 100 - 8 - 320, a.y + 10, 320, 32);
    this.colX = [];
    this.families.forEach((f, i) => {
      const x = a.x + 12 + i * (cw + 8); this.colX.push(x);
      f.types.forEach((t, j) => {
        const b = this.buttons.find(b => b.family === f && b.text === t.n.replace(/^RK/, ""));
        b.setBounds(x, a.y + 86 + j * 40, cw, 34);
      });
    });
    this.cw = cw;
  }
  paint(g) {
    super.paint(g);
    const a = this.area();
    g.setColour(Palette.text); g.setFont(17, true); g.drawText(this.title, a.x + 14, a.y + 10, a.w - 480, 32, J.centredLeft);
    this.families.forEach((f, i) => {
      const x = this.colX[i];
      g.setColour(f.colour); g.fillRoundedRectangle(x, a.y + 54, this.cw, 4, 2);
      g.setColour(Palette.textDim); g.setFont(12.5, true); g.drawFittedText(f.name.toUpperCase(), x, a.y + 60, this.cw, 20, J.centredLeft, 1);
    });
  }
}

// A footswitch that is on only while held (RKLoop's Record, Stop...).
class MomentaryButton extends TextButton {
  constructor(id, text) {
    super(text, null); this.id = id;
    this.onPress = () => store.set(id, 1);
    this.onRelease = () => store.set(id, 0);
  }
}

// A tap area over the picture that sets a parameter to one value (RKLoop's loop tiles).
class PickArea extends Component {
  constructor(id, value) { super(); this.id = id; this.value = value; this.takesTouches = true; }
  pointerDown() { store.set(this.id, this.value); return true; }
}

// An open insert: its header, and its controls laid out as the plugin's window
// on the computer (MixInsertLayout): a picture of the window (its panels, titles,
// curves and meters, redrawn as they change) with the iPad's own knobs, faders
// and switches where the window has them. A plugin without a layout lists its
// controls in rows.
class InsertView extends Sheet {
  constructor(strip, index, slot) {
    super();
    this.strip = strip; this.index = index; this.slot = slot; this.controls = []; this.params = null;
    this.layout = null; this.picture = null; this.showMore = false;
    const key = stripKey(strip, index);
    this.on = this.add(new TextButton("On", () => { const e = entryOf(strip, index, slot); if (e) net.send({ t: "insOn", s: key, n: slot, on: !e.on }); }));
    this.copy = this.add(new TextButton("Copy", () => net.send({ t: "insCopy", s: key, n: slot })));
    this.paste = this.add(new TextButton("Paste", () => net.send({ t: "insPaste", s: key, n: slot })));
    this.replace = this.add(new TextButton("Replace…", () => { this.close(); app.push(new Picker(strip, index, slot, true)); }));
    this.removeButton = this.add(new TextButton("Remove", () => this.confirmRemove()));
    this.closeButton = this.add(new TextButton("Close", () => this.close()));
    this.more = this.add(new TextButton("More…", () => { this.showMore = !this.showMore; this.build(); }));
    this.more.visible = false;
    this.request();
  }
  request() { net.send({ t: "insParams", s: stripKey(this.strip, this.index), n: this.slot }); }
  close() { net.send({ t: "insClose" }); super.close(); }
  confirmRemove() {
    if (this.removeArmed) { net.send({ t: "insRemove", s: stripKey(this.strip, this.index), n: this.slot }); this.close(); return; }
    this.removeArmed = true; this.removeButton.text = "Tap to remove"; this.removeButton.colour = Palette.warning.withAlpha(0.6); this.repaint();
    setTimeout(() => { this.removeArmed = false; this.removeButton.text = "Remove"; this.removeButton.colour = Palette.knobBody; this.repaint(); }, 3000);
  }
  setParams(msg) {
    this.params = msg.params || [];
    this.plugin = msg.p;
    this.layout = msg.layout || null;
    if (this.layout) this.setImage(this.layout.img);
    this.build();
  }
  setLayout(layout) { this.layout = layout; this.setImage(layout.img); this.build(); }
  setImage(url) {
    if (!url) return;
    const img = new Image();
    img.onload = () => { this.picture = img; this.repaint(); app.dirty = true; };
    img.src = url;
  }
  accent() { return this.layout && this.layout.accent !== undefined ? Colour.hex(this.layout.accent >>> 0) : Palette.accent; }
  // The controls: the window's (or, with More, those it doesn't show; or, with no layout, all of them as a list).
  build() {
    for (const c of this.controls) this.remove(c);
    this.controls = [];
    store.extra.clear();
    const more = this.layout ? (this.layout.more || []) : [];
    this.more.visible = more.length > 0;
    this.more.toggle = this.showMore;
    const listed = !this.layout ? this.params : this.showMore ? this.params.filter(p => more.includes(p.id)) : null;
    if (listed) {
      for (const p of listed) {
        let c;
        if (p.k === "bool") c = new ToggleSwitch(p.id, this.accent(), p.name);
        else if (p.k === "choice") c = new ChoiceBox(p.id, null);
        else c = new Knob(p.id, p.name);
        c.kind = p.k; c.title = p.name;
        store.extra.add(p.id);
        this.controls.push(this.add(c));
      }
    } else {
      const accent = this.accent();
      for (const d of this.layout.ctl) {
        let c;
        if (d.k === "fader") c = new Fader(d.id, d.label || "", d.zero || 0, !!d.compact);
        else if (d.k === "toggle") c = new ToggleSwitch(d.id, accent, d.label || "");
        else if (d.k === "segments") c = new SegmentedSwitch(d.id, accent, d.labels || null, d.title || "");
        else if (d.k === "choice") c = new ChoiceBox(d.id, null);
        else if (d.k === "momentary") c = new MomentaryButton(d.id, d.label || "");
        else if (d.k === "pick") c = new PickArea(d.id, d.value || 0);
        else { c = new Knob(d.id, d.label || ""); c.dimmed = !!d.dim; }
        c.kind = "placed"; c.spec = d;
        store.extra.add(d.id);
        this.controls.push(this.add(c));
      }
    }
    store.endWants();
    this.resized(); this.repaint();
  }
  // The window's place in the sheet: as large as fits (at most twice its size), centred.
  frame() {
    const a = this.area(), top = a.y + 56, w = a.w - 24, h = a.y + a.h - 12 - top;
    const s = Math.min(w / this.layout.w, h / this.layout.h, 2);
    return { x: a.x + 12 + (w - this.layout.w * s) / 2, y: top + (h - this.layout.h * s) / 2, s };
  }
  resized() {
    const a = this.area();
    let r = a.x + a.w - 12;
    for (const b of [this.closeButton, this.removeButton, this.replace, this.paste, this.copy, this.on, this.more]) {
      if (b === this.more && !b.visible) continue;
      const w = b === this.removeButton ? 120 : b === this.replace ? 100 : 76;
      b.setBounds(r - w, a.y + 10, w, 34); r -= w + 6;
    }
    if (this.layout && !this.showMore) {
      const f = this.frame();
      for (const c of this.controls) {
        const d = c.spec;
        c.setBounds(Math.round(f.x + d.x * f.s), Math.round(f.y + d.y * f.s), Math.round(d.w * f.s), Math.round(d.h * f.s));
      }
      return;
    }
    // the controls in rows: knobs 72 x 92; switches and choices in a cell of their own
    const cellW = 84, cellH = 100, x0 = a.x + 16, y0 = a.y + 64;
    const perRow = Math.max(1, Math.floor((a.w - 32) / cellW));
    this.controls.forEach((c, i) => {
      const cx = x0 + (i % perRow) * cellW, cy = y0 + Math.floor(i / perRow) * cellH;
      if (c.kind === "float") c.setBounds(cx + 4, cy, cellW - 8, 88);
      else c.setBounds(cx + 2, cy + 30, cellW - 4, 34);
    });
  }
  paint(g) {
    super.paint(g);
    const e = entryOf(this.strip, this.index, this.slot), a = this.area();
    if (!e) { setTimeout(() => this.close(), 0); return; }
    if (this.plugin && this.plugin !== e.p) { this.plugin = e.p; this.request(); }   // replaced elsewhere
    this.on.toggle = e.on; this.paste.enabled = !!state.clipboard && state.clipKind === "insert";
    g.setColour(Colour.hex(e.f)); g.fillRoundedRectangle(a.x + 12, a.y + 12, 5, 30, 2);
    g.setColour(Palette.text); g.setFont(17, true); g.drawText(e.p, a.x + 26, a.y + 10, 220, 34, J.centredLeft);
    g.setColour(Palette.textDim); g.setFont(14);
    g.drawText(stripName(this.strip, this.index) + ", insert " + (this.slot + 1), a.x + 26 + 130, a.y + 10, 300, 34, J.centredLeft);
    if (this.layout && !this.showMore) {
      const f = this.frame();
      g.drawImage(this.picture, f.x, f.y, this.layout.w * f.s, this.layout.h * f.s);
      return;
    }
    // choices: their name above
    g.setFont(12.5);
    for (const c of this.controls) if (c.kind === "choice") { g.setColour(Palette.textDim); g.drawFittedText(c.title, c.x, c.y - 24, c.w, 18, J.centred, 1); }
    if (this.params && !this.params.length) { g.setColour(Palette.textDim); g.setFont(14); g.drawText("No controls", a.x, a.y, a.w, a.h, J.centred); }
    g.setColour(Palette.textDim); g.setFont(12);
    g.drawText((this.layout ? "Controls the window doesn't show. " : "The plugin's own editor (its curves and pads) is in RKMix's window. ") + (state.clipboard ? "Copied: " + state.clipboard : ""),
               a.x + 16, a.y + a.h - 30, a.w - 32, 22, J.centredLeft);
  }
}

let view = null;
const isOpen = msg => view && msg.s === stripKey(view.strip, view.index) && msg.n === view.slot;
C.onInsertParams = msg => { if (isOpen(msg)) view.setParams(msg); };
C.onInsertLayout = msg => { if (isOpen(msg)) view.setLayout(msg.layout); };
C.onInsertImage = msg => { if (isOpen(msg)) view.setImage(msg.img); };
// From a strip's insert cell: an empty slot picks a plugin, a filled one opens.
C.openInsert = (strip, index, slot, cell, justAdded) => {
  if (C.isLocked && C.isLocked("everything")) return;
  if (strip === "ch" && C.isStereo(index - 1) && index % 2 === 1) index -= 1;
  const e = entryOf(strip, index, slot);
  if (!e && !justAdded) { app.push(new Picker(strip, index, slot, false)); return; }
  view = new InsertView(strip, index, slot);
  view.onClose = () => { view = null; };
  if (justAdded) {   // wait for the structure to show it
    const v = view;
    const wait = () => { if (view !== v) return; if (entryOf(strip, index, slot)) { app.push(v); v.request(); } else setTimeout(wait, 100); };
    setTimeout(wait, 100);
  } else app.push(view);
};
})();
