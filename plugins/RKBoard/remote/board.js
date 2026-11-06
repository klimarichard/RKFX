// RKBoard's remote: a phone on a mic stand or an iPad on the pedalboard.
// Pages: Scenes (with the Morph fader), Board (the pedals: tap a footswitch
// for bypass, a tile for its knobs), Assign (the Assign parameters in use),
// Tuner (the app) and Tempo. Drawn with the suite's own controls (rk.js) on
// one canvas; the protocol is BoardRemoteHub's (plugins/RKBoard/BoardRemote.h).
"use strict";

const B = (() => {
const { Colour, Palette, J, Component, TextButton, ToggleSwitch, SegmentedSwitch, Knob, LinearSlider, ChoiceBox, PopupMenu, app, store, fine, FineButton, editText, Style } = RK;

// rkfx::Palette::boardAccent: the board's own accent, on every lit control.
Palette.accent = Colour.hex(0xffe6e2d8);

const kHeader = 48, kTabs = 58, kPad = 12;

const state = {
  connected: false, signedIn: false, authMessage: "", waitUntil: 0,
  boards: [], board: null, page: "scenes", locked: false, controls: null, shown: 0,
};
const isLocked = () => state.locked;
const sceneLabel = n => { const b = state.board, name = b && b.names[n] ? b.names[n] : ""; return name ? `${n + 1} ${name}` : `Scene ${n + 1}`; };

// ---------------------------------------------------------------- the connection
const net = {
  ws: null, retry: null,
  connect() {
    clearTimeout(this.retry);
    const ws = new WebSocket((location.protocol === "https:" ? "wss://" : "ws://") + location.host + "/ws/board");
    this.ws = ws;
    ws.onopen = () => { state.connected = true; this.hello(takePinFromLink()); root.layout(); };
    ws.onmessage = e => receive(JSON.parse(e.data));
    ws.onclose = () => {
      if (this.ws !== ws) return;
      state.connected = false; state.signedIn = false; root.layout();
      this.retry = setTimeout(() => this.connect(), 2000);
    };
  },
  send(msg) { if (this.ws && this.ws.readyState === 1) this.ws.send(JSON.stringify(msg)); },
  hello(pin) {
    let token = ""; try { token = localStorage.getItem("rkboard.token") || ""; } catch (_) {}
    const msg = { t: "hello", name: deviceName() };
    if (pin) msg.pin = pin; else msg.token = token;
    this.send(msg);
  },
};
store.send = msg => net.send(msg);

// The QR code's link carries the PIN (#pin=1234): signed in without typing.
function takePinFromLink() {
  const m = /pin=(\d{4,8})/.exec(location.hash || "");
  if (!m) return "";
  try { history.replaceState(null, "", location.pathname); } catch (_) {}
  return m[1];
}

function deviceName() {
  const ua = navigator.userAgent;
  if (/iPad/.test(ua) || (/Macintosh/.test(ua) && navigator.maxTouchPoints > 1)) return "iPad";
  if (/iPhone/.test(ua)) return "iPhone";
  if (/Android/.test(ua)) return "Android";
  return "Browser";
}

function receive(msg) {
  switch (msg.type) {
    case "auth":
      if (msg.ok) {
        if (msg.token) try { localStorage.setItem("rkboard.token", msg.token); } catch (_) {}
        state.signedIn = true; state.authMessage = "";
        store.resend();
      } else {
        state.signedIn = false;
        state.authMessage = msg.off ? "The remote is off in RKBoard (Remote, in the window's header)."
                          : msg.wait ? `Too many tries: wait ${msg.wait} seconds.` : msg.wrong ? "Wrong PIN" : "";
        if (msg.wait) state.waitUntil = performance.now() + msg.wait * 1000;
      }
      root.layout();
      break;
    case "boards":
      state.boards = msg.list;
      chooseBoard();
      break;
    case "board": {
      const first = !state.board || state.board.id !== msg.id;
      state.board = msg;
      if (first || root.page.structureChanged(msg)) root.layout();
      updateWants();
      break;
    }
    case "controls":
      state.controls = msg;
      if (sheet) sheet.build();
      updateWants();
      break;
    case "gone":
      state.board = null; closeSheet(); chooseBoard(); root.layout();
      break;
    case "v": store.receive(msg.p); break;
  }
  app.dirty = true;
}

// Several RKBoards (one per rackspace): the one this device had, the only
// one, or the list.
function chooseBoard() {
  const list = state.boards;
  if (state.board && list.some(b => b.id === state.board.id)) { root.layout(); return; }
  let remembered = ""; try { remembered = localStorage.getItem("rkboard.board") || ""; } catch (_) {}
  const pick = list.length === 1 ? list[0] : list.find(b => b.name === remembered && list.filter(o => o.name === b.name).length === 1);
  if (pick) openBoard(pick);
  else root.layout();
}
function openBoard(b) {
  try { localStorage.setItem("rkboard.board", b.name); } catch (_) {}
  state.board = null; state.controls = null; state.shown = 0;
  net.send({ t: "open", b: b.id });
  root.layout();
}

// The values on screen (Morph, the Assign slots, the knobs shown).
function updateWants() {
  store.beginWants();
  const b = state.board;
  if (b) {
    store.want("morph");
    for (const a of b.assign) store.want("a" + a.slot);
  }
  if (sheet && state.controls) for (const p of state.controls.params) store.want(p.id);
  store.endWants();
}

// ---------------------------------------------------------------- small parts
// Drags on a page's background scroll it (tiles and rows pass theirs on).
class Scroller extends Component {
  constructor() { super(); this.offset = 0; this.content = 0; this.takesTouches = true; }
  maxOffset() { return Math.max(0, this.content - this.h); }
  scrollBy(dy) { const o = Math.max(0, Math.min(this.maxOffset(), this.offset - dy)); if (o !== this.offset) { this.offset = o; this.resized(); this.repaint(); } }
  pointerDown(e) { this.lastY = e.y; return true; }
  pointerMove(e) { this.scrollBy(e.y - this.lastY); this.lastY = e.y; }
  draw(g) {
    if (!this.visible) return;
    const c = g.ctx; c.save(); c.beginPath(); c.rect(this.x, this.y, this.w, this.h); c.clip();
    super.draw(g); c.restore();
  }
}

// A press that becomes a scroll once the finger moves.
function pressOrScroll(target, scroller) {
  return {
    down(e) { this.start = e.y; this.last = e.y; this.moved = false; },
    move(e) { if (Math.abs(e.y - this.start) > 10) this.moved = true; if (this.moved && scroller) scroller.scrollBy(e.y - this.last); this.last = e.y; },
  };
}

// A big fader (Morph, an Assign slot): slide sideways; double-tap for the default.
class BigFader extends LinearSlider {
  constructor(id, colour) { super(id); this.colour = colour || Palette.accent; this.locked = null; }
  indent() { return 20; }
  paint(g) {
    const a = this.enabled && !(this.locked && this.locked()) ? 1 : 0.45, cy = this.h / 2, ind = this.indent();
    const pos = ind + this.norm * (this.w - 2 * ind);
    g.setColour(Palette.knobTrack.withMultipliedAlpha(a)); g.fillRoundedRectangle(ind - 4, cy - 5, this.w - 2 * ind + 8, 10, 5);
    g.setColour(this.colour.withAlpha(0.6 * a)); g.fillRoundedRectangle(ind - 4, cy - 5, pos - ind + 8, 10, 5);
    g.setColour(Palette.text.withMultipliedAlpha(a)); g.fillRoundedRectangle(pos - 14, cy - 20, 28, 40, 6);
    g.setColour(Palette.knobBody); g.fillRect(pos - 8, cy - 1, 16, 2);
  }
}

// rkfx::Knob, ToggleSwitch or ChoiceBox, as the parameter is (known once its value comes).
function controlFor(id, name) {
  const info = store.get(id), n = info && info.choices ? info.choices.length : 0;
  let c;
  if (n > 4) { c = new ChoiceBox(id); c.label = name; }
  else if (n >= 2) c = new SegmentedSwitch(id, Palette.accent, null, name);
  else if (info && info.steps === 2) c = new ToggleSwitch(id, Palette.accent, name);
  else c = new Knob(id, name);
  c.locked = isLocked;
  return c;
}

// ---------------------------------------------------------------- the header and tabs
class Header extends Component {
  constructor() {
    super();
    this.lock = this.add(new TextButton("Lock", () => { state.locked = !state.locked; this.update(); app.dirty = true; }));
    this.fine = this.add(new FineButton());
    // The board's name (tap: the other boards); drawn by the header, left-aligned.
    this.name = this.add(new TextButton("", () => { if (state.boards.length > 1) showBoardMenu(this.name); }));
    this.name.colour = Colour.hex(0x00000000);
    this.update();
  }
  update() { this.lock.toggle = state.locked; this.lock.onColour = Palette.warning; this.lock.text = state.locked ? "Locked" : "Lock"; }
  resized() {
    this.fine.setBounds(this.w - kPad - 58, 8, 58, this.h - 16);
    this.lock.setBounds(this.fine.x - 6 - 72, 8, 72, this.h - 16);
    this.name.setBounds(kPad - 6, 6, this.lock.x - kPad - 30, this.h - 12);
  }
  paint(g) {
    g.setColour(Palette.panel); g.fillRect(0, 0, this.w, this.h);
    g.setColour(Palette.accent); g.fillRect(0, this.h - 2, this.w, 2);
    const b = state.board;
    this.name.text = (b ? (state.boards.length > 1 ? b.name + " ▾" : b.name) : "RKBoard");
    // The connection.
    g.setColour(state.connected ? Colour.hex(0xff5cb85c) : Palette.warning);
    g.fillEllipse(this.lock.x - 18, this.h / 2 - 4, 8, 8);
  }
}

function showBoardMenu(anchor) {
  PopupMenu.show(state.boards.map(b => ({ text: b.name + (b.preset ? "  ·  " + b.preset : ""), ticked: state.board && b.id === state.board.id, action: () => openBoard(b) })), anchor);
}

const pageDefs = [
  { key: "scenes", title: "Scenes" }, { key: "board", title: "Board" }, { key: "assign", title: "Assign" },
  { key: "tuner", title: "Tuner", appOnly: true }, { key: "tempo", title: "Tempo" },
];

class TabBar extends Component {
  constructor() { super(); this.buttons = []; }
  setPages(defs) {
    for (const b of this.buttons) this.remove(b);
    this.buttons = defs.map(d => { const t = this.add(new TextButton(d.title, () => root.showPage(d.key))); t.key = d.key; t.fontHeight = 15; return t; });
    this.resized();
  }
  resized() {
    const n = Math.max(1, this.buttons.length), w = (this.w - kPad * 2 - (n - 1) * 6) / n;
    this.buttons.forEach((b, i) => b.setBounds(kPad + i * (w + 6), 8, w, this.h - 16));
  }
  paint(g) {
    g.setColour(Palette.panel); g.fillRect(0, 0, this.w, this.h);
    for (const b of this.buttons) b.toggle = b.key === state.page;
  }
}

// ---------------------------------------------------------------- Scenes
class SceneTile extends Component {
  constructor(n) { super(); this.n = n; this.takesTouches = true; }
  paint(g) {
    const b = state.board, current = b && b.scene === this.n, name = b ? b.names[this.n] : "";
    let fill = current ? Palette.accent : Palette.knobBody;
    if (this.down) fill = fill.brighter(0.15);
    g.setColour(fill); g.fillRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, Style.panelRadius);
    const tc = current ? Palette.onAccent(Palette.accent) : Palette.text;
    g.setColour(tc);
    if (!name) { g.setFont(Math.min(26, this.h * 0.45), true); g.drawText(String(this.n + 1), 0, 0, this.w, this.h, J.centred); return; }
    g.setFont(Math.min(22, this.h * 0.36), true); g.drawText(String(this.n + 1), 0, 4, this.w, this.h * 0.5, J.centred);
    g.setColour(tc.withMultipliedAlpha(current ? 0.85 : 0.75)); g.setFont(Math.min(14, this.h * 0.24));
    g.drawFittedText(name, 4, this.h * 0.5, this.w - 8, this.h * 0.45, J.centred, 2, 0.8);
  }
  pointerDown() { this.down = true; this.repaint(); return true; }
  pointerUp(e) { this.down = false; this.repaint(); if (!e.cancelled && this.hitTest(e.x, e.y)) net.send({ t: "scene", n: this.n }); }
}

class ScenesPage extends Component {
  constructor() {
    super();
    this.tiles = [];
    for (let i = 0; i < 16; i++) this.tiles.push(this.add(new SceneTile(i)));
    this.fromButton = this.add(new TextButton("", () => this.chooseScene(false)));
    this.toButton = this.add(new TextButton("", () => this.chooseScene(true)));
    this.fader = this.add(new BigFader("morph"));
  }
  structureChanged() { return false; }
  chooseScene(to) {
    const b = state.board; if (!b || isLocked()) return;
    PopupMenu.show(b.names.map((_, i) => ({ text: sceneLabel(i), ticked: (to ? b.morph.to : b.morph.from) === i,
      action: () => net.send({ t: "morphScenes", from: to ? b.morph.from : i, to: to ? i : b.morph.to }) })), to ? this.toButton : this.fromButton);
  }
  resized() {
    const cols = this.w >= 640 ? 8 : 4, gap = 8, area = this.w - 2 * kPad;
    const tw = (area - (cols - 1) * gap) / cols, rows = 16 / cols;
    const morphH = 150, th = Math.max(54, Math.min(110, (this.h - 2 * kPad - morphH - (rows - 1) * gap) / rows));
    this.tiles.forEach((t, i) => t.setBounds(kPad + (i % cols) * (tw + gap), kPad + Math.floor(i / cols) * (th + gap), tw, th));
    const top = kPad + rows * (th + gap) + 14;
    this.morphTop = top;
    this.fromButton.setBounds(this.w - kPad - 2 * 64 - 26, top, 64, 36);
    this.toButton.setBounds(this.w - kPad - 64, top, 64, 36);
    this.fader.setBounds(kPad, top + 48, this.w - 2 * kPad, 64);
  }
  paint(g) {
    const b = state.board; if (!b) return;
    this.fromButton.text = String(b.morph.from + 1) + " ▾"; this.toButton.text = String(b.morph.to + 1) + " ▾";
    this.fromButton.enabled = this.toButton.enabled = !isLocked();
    g.setFont(15, true); g.setColour(b.morph.on ? Palette.accent : Palette.textDim);
    g.drawText(b.morph.on ? `Morphing ${b.morph.from + 1} → ${b.morph.to + 1}` : "Morph", kPad, this.morphTop, 200, 36, J.centredLeft);
    g.setColour(Palette.textDim); g.setFont(18);
    g.drawText("→", this.fromButton.x + this.fromButton.w, this.morphTop, this.toButton.x - this.fromButton.x - this.fromButton.w, 36, J.centred);
    g.setFont(13); g.drawText(store.text("morph") || "", kPad, this.fader.y + this.fader.h, this.w - 2 * kPad, 20, J.centred);
  }
}

// ---------------------------------------------------------------- Board
class Tile extends Component {
  constructor(p, scroller) { super(); this.p = p; this.takesTouches = true; this.press = pressOrScroll(this, scroller); }
  footswitch() { const s = Math.min(46, this.w * 0.5); return { x: (this.w - s) / 2, y: this.h - s - 10, s }; }
  paint(g) {
    const p = this.p, family = Colour.hex(p.c), on = p.on;
    let body = Palette.knobBody.interpolatedWith(family, on ? 0.32 : 0.14);
    if (this.down) body = body.brighter(0.08);
    g.setColour(body); g.fillRoundedRectangle(0, 0, this.w, this.h, Style.panelRadius);
    g.setColour(family.withAlpha(on ? 1 : 0.45)); g.fillRoundedRectangle(0, 0, this.w, 6, 3);
    if (state.shown === p.id && sheet) { g.setColour(Palette.accent); g.drawRoundedRectangle(1, 1, this.w - 2, this.h - 2, Style.panelRadius, 2); }
    g.setColour(on ? Palette.text : Palette.textDim); g.setFont(15, true);
    g.drawFittedText(p.n, 4, 12, this.w - 8, 20, J.centred, 1, 0.75);
    if (p.s % 3 !== 0) {   // on a split's path: A1, B1...
      g.setFont(11, true); g.setColour(Palette.textDim);
      g.drawText((p.s % 3 === 1 ? "A" : "B") + (Math.floor(p.s / 3) + 1), 6, 32, 30, 14, J.centredLeft);
    }
    const ledY = 44;
    if (on) { g.setColour(family.brighter(0.4).withAlpha(0.35)); g.fillEllipse(this.w / 2 - 9, ledY - 9, 18, 18); g.setColour(family.brighter(0.5)); }
    else g.setColour(Palette.background);
    g.fillEllipse(this.w / 2 - 5, ledY - 5, 10, 10);
    const f = this.footswitch();
    g.setColour(Palette.knobTrack); g.fillEllipse(f.x, f.y, f.s, f.s);
    g.setColour(Palette.knobBody.brighter(this.downOnSwitch ? 0.35 : 0.15)); g.fillEllipse(f.x + 5, f.y + 5, f.s - 10, f.s - 10);
    g.setColour(Palette.outline); g.drawEllipse(f.x, f.y, f.s, f.s, 1);
  }
  onSwitch(e) { const f = this.footswitch(); return e.y >= f.y - 12; }
  pointerDown(e) { this.press.down(e); this.down = true; this.downOnSwitch = this.onSwitch(e); this.repaint(); return true; }
  pointerMove(e) { this.press.move(e); if (this.press.moved && this.down) { this.down = this.downOnSwitch = false; this.repaint(); } }
  pointerUp(e) {
    const tapped = !e.cancelled && !this.press.moved && this.hitTest(e.x, e.y), sw = this.downOnSwitch;
    this.down = this.downOnSwitch = false; this.repaint();
    if (!tapped) return;
    if (sw) { this.p.on = !this.p.on; net.send({ t: "bypass", id: this.p.id, on: this.p.on }); }   // footswitches work while locked
    else openSheet(this.p);
  }
}

class BoardPage extends Scroller {
  constructor() { super(); this.tiles = []; this.signature = ""; }
  structureChanged(b) { return this.signatureOf(b) !== this.signature; }
  signatureOf(b) { return b ? b.pedals.map(p => p.id + ":" + p.s).join(",") : ""; }
  rebuild() {
    for (const t of this.tiles) this.remove(t);
    const b = state.board;
    this.signature = this.signatureOf(b);
    this.tiles = b ? b.pedals.map(p => this.add(new Tile(p, this))) : [];
  }
  resized() {
    const b = state.board;
    if (this.signatureOf(b) !== this.signature || this.tiles.length !== (b ? b.pedals.length : 0)) this.rebuild();
    // Keep each tile's pedal fresh (on/off, name).
    if (b) this.tiles.forEach((t, i) => { t.p = b.pedals[i]; });
    const gap = 10, minW = 92, area = this.w - 2 * kPad;
    const cols = Math.max(2, Math.floor((area + gap) / (minW + gap))), tw = (area - (cols - 1) * gap) / cols, th = 128;
    this.tiles.forEach((t, i) => t.setBounds(kPad + (i % cols) * (tw + gap), kPad + Math.floor(i / cols) * (th + gap) - this.offset, tw, th));
    const rows = Math.ceil(this.tiles.length / cols);
    this.content = kPad * 2 + rows * (th + gap);
  }
  paint(g) {
    const b = state.board;
    if (b) this.tiles.forEach((t, i) => { if (b.pedals[i]) t.p = b.pedals[i]; });
    if (b && !b.pedals.length) {
      g.setColour(Palette.textDim); g.setFont(16);
      g.drawFittedText("No pedals on this board yet: add them in RKBoard's window.", kPad, 40, this.w - 2 * kPad, 60, J.centred, 2);
    }
  }
}

// A pedal's knobs, over the page (as the console remote shows an insert's).
let sheet = null;
class ControlsSheet extends Scroller {
  constructor(p) { super(); this.p = p; this.cells = []; this.close = this.add(new TextButton("Done", closeSheet)); this.close.fontHeight = 15; this.missing = true; }
  build() {
    for (const c of this.cells) this.remove(c);
    const params = state.controls && state.controls.id === this.p.id ? state.controls.params : [];
    this.cells = params.map(p => this.add(controlFor(p.id, p.n)));
    this.missing = params.some(p => !store.get(p.id));
    this.resized(); this.repaint();
  }
  resized() {
    this.close.setBounds(this.w - kPad - 80, 10, 80, 36);
    const gap = 10, area = this.w - 2 * kPad, cw = 96, cols = Math.max(2, Math.floor((area + gap) / (cw + gap))), w = (area - (cols - 1) * gap) / cols;
    let x = 0, y = 0, row = 0;
    this.cells.forEach((c, i) => {
      const col = i % cols;
      if (col === 0 && i > 0) { y += row + gap; row = 0; }
      const h = c instanceof Knob ? 110 : 64;
      const top = 60 + y - this.offset;
      if (c instanceof Knob || c instanceof SegmentedSwitch) c.setBounds(kPad + col * (w + gap), top, w, c instanceof Knob ? h : 58);
      else c.setBounds(kPad + col * (w + gap), top + 22, w, 38);
      row = Math.max(row, h); x++;
    });
    this.content = 60 + y + row + kPad * 2;
  }
  paint(g) {
    g.setColour(Palette.tintedPanel); g.fillRect(0, 0, this.w, this.h);
    const family = Colour.hex(this.p.c);
    g.setColour(family); g.fillRect(0, 0, this.w, 5);
    g.setColour(Palette.text); g.setFont(19, true); g.drawText(this.p.n, kPad, 10, this.w - 120, 36, J.centredLeft);
    g.setFont(12.5); g.setColour(Palette.textDim);
    for (const c of this.cells) if (c instanceof ChoiceBox) g.drawFittedText(c.label || "", c.x, c.y - 20, c.w, 18, J.centred, 1);
    if (isLocked()) { g.setColour(Palette.warning); g.drawText("Locked", this.close.x - 90, 10, 80, 36, J.centredRight); }
    if (!this.cells.length) { g.setColour(Palette.textDim); g.setFont(15); g.drawText("...", 0, 80, this.w, 30, J.centred); }
  }
}
function openSheet(p) {
  closeSheet();
  state.shown = p.id; state.controls = null;
  // Over the page, under the header (Lock and Fine stay at hand).
  sheet = root.add(new ControlsSheet(p));
  const page = root.page;
  sheet.setBounds(page.x, page.y, page.w, page.h);
  app.dirty = true;
  net.send({ t: "show", id: p.id });
}
function closeSheet() {
  if (!sheet) return;
  root.remove(sheet);
  sheet = null; state.shown = 0; state.controls = null; net.send({ t: "show", id: 0 }); updateWants(); app.dirty = true;
}
// Rebuild once the controls' kinds are known (their first values have come).
app.frameHooks.add(() => { if (sheet && sheet.missing && state.controls && state.controls.params.every(p => store.get(p.id))) sheet.build(); });

// ---------------------------------------------------------------- Assign
class AssignRow extends Component {
  constructor(a, scroller) {
    super(); this.a = a; this.takesTouches = true; this.press = pressOrScroll(this, scroller);
    const id = "a" + a.slot;
    if (a.sw) { this.control = this.add(new ToggleSwitch(id, Palette.accent, "On")); this.control.fontHeight = 15; }
    else this.control = this.add(new BigFader(id));
    this.control.locked = isLocked;
  }
  resized() { if (this.a.sw) this.control.setBounds(this.w - 96, 22, 96, 44); else this.control.setBounds(0, 26, this.w, 56); }
  paint(g) {
    g.setColour(Palette.text); g.setFont(15, true); g.drawText(this.a.n, 0, 0, this.w - (this.a.sw ? 110 : 90), 24, J.centredLeft);
    g.setColour(Palette.textDim); g.setFont(12.5); g.drawText("Assign " + this.a.slot, 0, 0, this.w, 24, J.centredRight);
    if (!this.a.sw) { g.setFont(13); g.setColour(Palette.textDim); g.drawText(store.text("a" + this.a.slot), 0, 78, this.w, 18, J.centred); }
  }
  pointerDown(e) { this.press.down(e); return true; }
  pointerMove(e) { this.press.move(e); }
}

class AssignPage extends Scroller {
  constructor() { super(); this.rows = []; this.signature = ""; }
  structureChanged(b) { return this.signatureOf(b) !== this.signature; }
  signatureOf(b) { return b ? b.assign.map(a => a.slot + a.n + a.sw).join("|") : ""; }
  resized() {
    const b = state.board;
    if (this.signatureOf(b) !== this.signature) {
      for (const r of this.rows) this.remove(r);
      this.signature = this.signatureOf(b);
      this.rows = b ? b.assign.map(a => this.add(new AssignRow(a, this))) : [];
    }
    let y = kPad - this.offset;
    for (const r of this.rows) { const h = r.a.sw ? 72 : 100; r.setBounds(kPad, y, this.w - 2 * kPad, h); y += h + 10; }
    this.content = y + this.offset + kPad;
  }
  paint(g) {
    if (state.board && !this.rows.length) {
      g.setColour(Palette.textDim); g.setFont(16);
      g.drawFittedText("No Assign slots in use: right-click a knob in RKBoard's window and choose Assign to.", kPad, 40, this.w - 2 * kPad, 60, J.centred, 3);
    }
  }
}

// ---------------------------------------------------------------- Tuner
class TunerPage extends Component {
  constructor() {
    super();
    this.toggle = this.add(new TextButton("Tuner", () => { const t = state.board && state.board.tuner; if (t) net.send({ t: "tuner", on: !t.on }); }));
    this.toggle.fontHeight = 17; this.toggle.bold = true;
  }
  structureChanged() { return false; }
  resized() { this.toggle.setBounds(kPad, this.h - kPad - 64, this.w - 2 * kPad, 64); }
  paint(g) {
    const b = state.board, t = b && b.tuner;
    if (!t) {
      this.toggle.setVisible(false);
      g.setColour(Palette.textDim); g.setFont(16);
      g.drawFittedText("The tuner is in RKBoard's app. In a host, use the host's (Gig Performer has one).", kPad, 60, this.w - 2 * kPad, 60, J.centred, 3);
      return;
    }
    this.toggle.setVisible(true);
    this.toggle.toggle = t.on; this.toggle.text = t.on ? "Tuner on (tap to close)" : "Tuner off";
    const top = kPad + 20, mid = this.w / 2;
    if (!t.on || !t.hz) {
      g.setColour(Palette.textDim); g.setFont(20); g.drawText(t.on ? "Play a note" : "Turn the tuner on", 0, top + 60, this.w, 40, J.centred);
      return;
    }
    const inTune = Math.abs(t.cents) < 3;
    g.setColour(inTune ? Colour.hex(0xff5cb85c) : Palette.text);
    g.setFont(Math.min(150, this.w * 0.35), true); g.drawText(t.note, 0, top, this.w, Math.min(160, this.w * 0.38), J.centred);
    const y = top + Math.min(180, this.w * 0.42);
    g.setFont(18); g.setColour(Palette.textDim);
    g.drawText(`${t.note}${t.octave}   ${t.cents >= 0 ? "+" : ""}${t.cents.toFixed(1)} cents   ${t.hz.toFixed(1)} Hz`, 0, y, this.w, 28, J.centred);
    // The cents scale: -50 to +50.
    const w = this.w - 2 * kPad - 20, x0 = kPad + 10, sy = y + 60;
    g.setColour(Palette.knobTrack); g.fillRoundedRectangle(x0, sy, w, 10, 5);
    g.setColour(Palette.text); g.fillRect(mid - 1, sy - 12, 2, 34);
    const nx = x0 + (t.cents + 50) / 100 * w;
    g.setColour(inTune ? Colour.hex(0xff5cb85c) : Palette.warning); g.fillRoundedRectangle(nx - 5, sy - 16, 10, 42, 4);
    g.setColour(Palette.textDim); g.setFont(12); g.drawText(`A = ${t.ref} Hz`, 0, sy + 40, this.w, 18, J.centred);
  }
}

// ---------------------------------------------------------------- Tempo
class TempoPage extends Component {
  constructor() {
    super();
    this.tap = this.add(new TextButton("Tap", () => net.send({ t: "tap" })));
    this.tap.fontHeight = 30; this.tap.bold = true;
  }
  structureChanged() { return false; }
  resized() { const s = Math.min(this.w - 2 * kPad, this.h * 0.5); this.tap.setBounds((this.w - s) / 2, this.h - kPad - s, s, s); }
  paint(g) {
    const b = state.board; if (!b) return;
    const t = b.tempo;
    g.setColour(Palette.text); g.setFont(Math.min(110, this.w * 0.26), true);
    g.drawText(t.bpm > 0 ? t.bpm.toFixed(1) : "-", 0, kPad + 10, this.w, Math.min(120, this.w * 0.3), J.centred);
    g.setColour(Palette.textDim); g.setFont(16);
    const note = t.tap ? "BPM" : t.clock ? "BPM, from MIDI clock" : b.app ? "BPM" : "BPM, the host's (tap it there)";
    g.drawText(note, 0, kPad + 10 + Math.min(120, this.w * 0.3), this.w, 26, J.centred);
    this.tap.enabled = t.tap;
  }
}

// ---------------------------------------------------------------- signing in
class PinScreen extends Component {
  constructor() {
    super();
    this.enter = this.add(new TextButton("Enter the PIN", () => {
      if (performance.now() < state.waitUntil) return;
      editText(this.enter, "", pin => { if (pin) net.hello(pin.trim()); }, { numeric: true, maxLength: 8, fontHeight: 22 });
    }));
    this.enter.fontHeight = 17;
  }
  structureChanged() { return false; }
  resized() { this.enter.setBounds((this.w - 240) / 2, this.h / 2, 240, 52); }
  paint(g) {
    g.setColour(Palette.text); g.setFont(24, true); g.drawText("RKBoard Remote", 0, this.h / 2 - 150, this.w, 36, J.centred);
    g.setColour(Palette.textDim); g.setFont(15);
    const text = !state.connected ? "Connecting to RKBoard..." : state.authMessage ||
                 "Type the PIN shown on RKBoard's Remote page (or scan its QR code).";
    g.drawFittedText(text, kPad * 2, this.h / 2 - 100, this.w - kPad * 4, 80, J.centred, 3);
    this.enter.setVisible(state.connected && !state.authMessage.startsWith("The remote is off"));
  }
}

class BoardList extends Component {
  constructor() { super(); this.buttons = []; }
  structureChanged() { return false; }
  resized() {
    for (const b of this.buttons) this.remove(b);
    this.buttons = state.boards.map(b => { const t = this.add(new TextButton(b.name + (b.preset ? "  ·  " + b.preset : ""), () => openBoard(b))); t.fontHeight = 17; return t; });
    this.buttons.forEach((b, i) => b.setBounds(kPad, 70 + i * 64, this.w - 2 * kPad, 54));
  }
  paint(g) {
    g.setColour(Palette.textDim); g.setFont(16);
    g.drawText(state.boards.length ? "Which board?" : "No RKBoard is running", 0, 16, this.w, 40, J.centred);
  }
}

// ---------------------------------------------------------------- the root
class Root extends Component {
  constructor() {
    super();
    this.header = this.add(new Header());
    this.tabs = this.add(new TabBar());
    this.pages = { scenes: new ScenesPage(), board: new BoardPage(), assign: new AssignPage(), tuner: new TunerPage(), tempo: new TempoPage() };
    this.pin = new PinScreen(); this.list = new BoardList();
    this.page = this.pin;
    this.add(this.page);
  }
  showPage(key) { state.page = key; closeSheet(); this.layout(); }
  layout() {
    const b = state.board;
    let next;
    if (!state.signedIn) next = this.pin;
    else if (!b) next = this.list;
    else {
      const defs = pageDefs.filter(d => !d.appOnly || b.app);
      if (!defs.some(d => d.key === state.page)) state.page = "scenes";
      if (this.tabs.buttons.length !== defs.length) this.tabs.setPages(defs);
      next = this.pages[state.page];
    }
    if (next !== this.page) {
      this.remove(this.page); this.page = next; this.add(next);
      if (sheet) { this.remove(sheet); this.add(sheet); }   // the knobs stay on top
    }
    this.tabs.setVisible(!!b && state.signedIn);
    this.header.update();
    this.resized();
    app.dirty = true;
  }
  resized() {
    this.header.setBounds(0, 0, this.w, kHeader);
    const tabs = this.tabs.visible ? kTabs : 0;
    this.tabs.setBounds(0, this.h - tabs, this.w, tabs);
    this.page.setBounds(0, kHeader, this.w, this.h - kHeader - tabs);
    if (sheet) sheet.setBounds(this.page.x, this.page.y, this.page.w, this.page.h);
  }
  paint(g) {
    // Pages re-lay out when the board changes shape (pedals, Assign slots).
    if (this.page.structureChanged && state.board && this.page.structureChanged(state.board)) this.page.resized();
  }
}

let root = null;
function start() {
  fine.load("rkboard.fine");
  root = new Root();
  app.start(root, 0, { fitWidth: 400 });
  net.connect();
}

return { start, state };
})();
