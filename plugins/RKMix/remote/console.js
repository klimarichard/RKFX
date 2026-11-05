// RKMix's console remote: the shell. The connection to RKMix (a WebSocket on
// the page's own server), signing in with the PIN, the header with the page
// tabs and undo, and what the pages share (names, labels, the meters).
"use strict";

const C = (() => {
const { Palette, J, Component, TextButton, app, store, Colour } = RK;

const kHeader = 44;          // EditorBase::kHeaderHeight
const kDesignHeight = 850;   // the logical height every iPad is scaled to

const state = {
  connected: false, signedIn: false, structure: null, meters: null, lastMeters: 0,
  authMessage: "", waitUntil: 0, page: "mix",
  selected: { kind: "ch", index: 0 },   // the strip the Channel page shows
};

// ---------------------------------------------------------------- names
const S = () => state.structure;
function isStereo(ch) { const s = S(); return !!(s && s.ch[ch] && s.ch[ch].st); }
function channelLabel(ch) { return isStereo(ch) ? `${ch + 1}/${ch + 2}` : String(ch + 1); }
function channelName(ch) { const s = S(), n = s && s.ch[ch] ? s.ch[ch].n : ""; return n || "Ch " + channelLabel(ch); }
function busName(bus) { const s = S(), n = s && s.bus[bus] ? s.bus[bus].n : ""; return n || "Bus " + (bus + 1); }
function dcaName(d) { const s = S(), n = s && s.dca[d] ? s.dca[d].n : ""; return n || "DCA " + (d + 1); }
function muteGroupName(g) { const s = S(), n = s && s.mg[g] ? s.mg[g].n : ""; return n || "Mute " + (g + 1); }
const kUserPageBus = 100;
function stripName(id) { return id >= kUserPageBus ? busName(id - kUserPageBus) : channelName(id); }
const busTypeNames = ["Group", "Aux", "FX"];
function busType(bus) { const s = S(); return s && s.bus[bus] ? s.bus[bus].t : 1; }
function num(i) { return String(i + 1); }

// ---------------------------------------------------------------- the connection
const net = {
  ws: null, retry: null,
  connect() {
    clearTimeout(this.retry);
    const ws = new WebSocket((location.protocol === "https:" ? "wss://" : "ws://") + location.host + "/ws/console");
    this.ws = ws;
    ws.onopen = () => { state.connected = true; state.authAnswered = false; this.hello(); app.dirty = true; };
    ws.onmessage = e => receive(JSON.parse(e.data));
    ws.onclose = () => {
      if (this.ws !== ws) return;
      state.connected = false; state.signedIn = false; app.dirty = true;
      this.retry = setTimeout(() => this.connect(), 2000);
    };
  },
  send(msg) { if (this.ws && this.ws.readyState === 1) this.ws.send(JSON.stringify(msg)); },
  hello(pin) {
    let token = ""; try { token = localStorage.getItem("rkmix.console.token") || ""; } catch (_) {}
    const msg = { t: "hello", name: deviceName() };
    if (pin) msg.pin = pin; else msg.token = token;
    this.send(msg);
  },
};
store.send = msg => net.send(msg);

function deviceName() {
  const ua = navigator.userAgent;
  if (/iPad/.test(ua) || (/Macintosh/.test(ua) && navigator.maxTouchPoints > 1)) return "iPad";
  if (/iPhone/.test(ua)) return "iPhone";
  if (/Android/.test(ua)) return "Android";
  return "Browser";
}

const listeners = { structure: new Set(), meters: new Set(), curve: new Set() };

function receive(msg) {
  switch (msg.type) {
    case "auth":
      state.authAnswered = true;
      if (msg.ok) {
        if (msg.token) try { localStorage.setItem("rkmix.console.token", msg.token); } catch (_) {}
        state.signedIn = true; state.authMessage = "";
        store.resend();
        net.send({ t: "curves", k: curves.keys() });
        if (C.onSignedIn) C.onSignedIn();
      } else {
        state.signedIn = false;
        state.authMessage = msg.off ? "The console remote is off on RKMix's Remote page."
                          : msg.wait ? `Too many tries: wait ${msg.wait} seconds.` : msg.wrong ? "Wrong PIN" : "";
        if (msg.wait) state.waitUntil = performance.now() + msg.wait * 1000;
        pinScreen.reset();
      }
      root.layout();
      break;
    case "s":
      state.structure = msg;
      for (const f of listeners.structure) f(msg);
      break;
    case "v": store.receive(msg.p); break;
    case "m": {
      const now = performance.now(), dt = state.lastMeters ? Math.min(0.5, (now - state.lastMeters) / 1000) : 0.05;
      state.lastMeters = now; state.meters = msg;
      for (const f of listeners.meters) f(msg, dt);
      break;
    }
    case "curve": curves.receive(msg.k, msg.d); break;
    case "types": state.types = msg.list; break;
    case "clip": state.clipboard = msg.text; state.clipKind = msg.kind; break;
    case "insParams": if (C.onInsertParams) C.onInsertParams(msg); break;
    case "meq": if (C.onMonitorEq) C.onMonitorEq(msg); break;
  }
  app.dirty = true;
}

// Curves (EQ responses, compressor curves) the pages show, by key ("eq:ch3").
const curves = {
  wanted: new Map(), data: new Map(), sent: "",
  want(key, onData) { this.wanted.set(key, onData); this.update(); if (this.data.has(key)) onData(this.data.get(key)); },
  drop(key) { this.wanted.delete(key); this.update(); },
  keys() { return [...this.wanted.keys()]; },
  update() { const k = this.keys().sort().join(","); if (k !== this.sent) { this.sent = k; net.send({ t: "curves", k: this.keys() }); } },
  receive(key, d) { this.data.set(key, d); const f = this.wanted.get(key); if (f) f(d); app.dirty = true; },
};

// ---------------------------------------------------------------- pages
const pages = [];      // { key, title, make: () => Component }
function addPage(key, title, make) { pages.push({ key, title, make }); }

// The header (EditorBase's): the name, the page tabs, Undo, Redo, the connection.
class Header extends Component {
  constructor() {
    super();
    this.tabs = [];
    this.undo = this.add(new TextButton("Undo", () => net.send({ t: "undo" })));
    this.redo = this.add(new TextButton("Redo", () => net.send({ t: "redo" })));
    this.extra = [];   // more buttons pages add (Lock)
  }
  setTabs() {
    for (const t of this.tabs) this.remove(t);
    this.tabs = pages.map(p => this.add(new TextButton(p.title, () => root.showPage(p.key))));
    this.resized();
  }
  resized() {
    let x = 120;
    for (const t of this.tabs) { t.setBounds(x, 8, 96, kHeader - 16); x += 102; }
    let r = this.w - 12 - 150;
    for (const b of this.extra) { b.setBounds(r - b.width, 8, b.width, kHeader - 16); r -= b.width + 6; }
    this.redo.setBounds(r - 64, 8, 64, kHeader - 16); r -= 68;
    this.undo.setBounds(r - 64, 8, 64, kHeader - 16);
  }
  paint(g) {
    g.setColour(Palette.tintedPanel); g.fillRect(0, 0, this.w, kHeader);
    g.setColour(Palette.accent); g.fillRect(0, kHeader - 2, this.w, 2);
    g.setColour(Palette.text); g.setFont(18, true); g.drawText("RKMix", 14, 0, 200, kHeader, J.centredLeft);
    const s = S();
    // What the show is at (Gig Performer's names, by OSC): between the tabs and Undo.
    const now = s && s.now && (s.now.t || s.now.s) ? s.now : null;
    if (now) {
      const last = this.tabs[this.tabs.length - 1], left = (last ? last.x + last.w : 120) + 16, right = this.undo.x - 16;
      if (right - left > 60) {
        g.setFont(15, true);
        const tw = now.t ? Math.min(g.textWidth(now.t), now.s ? (right - left) * 0.65 : right - left) : 0;
        if (now.t) { g.setColour(Palette.text); g.drawText(now.t, left, 0, tw, kHeader, J.centredLeft); }
        if (now.s) {
          const sx = left + (tw ? tw + 10 : 0);
          g.setFont(13); g.setColour(Palette.textDim); g.drawText(now.s, sx, 0, right - sx, kHeader, J.centredLeft);
        }
      }
    }
    this.undo.enabled = !!(s && s.undo); this.redo.enabled = !!(s && s.redo);
    this.tabs.forEach((t, i) => { t.toggle = pages[i].key === state.page; });
    // The connection
    const ok = state.connected && state.signedIn;
    g.setColour(ok ? Palette.accent.brighter(0.4) : Palette.warning);
    g.fillEllipse(this.w - 150, kHeader / 2 - 4, 8, 8);
    g.setFont(12); g.setColour(ok ? Palette.textDim : Palette.warning);
    g.drawText(ok ? "Connected" : state.connected ? "Sign in" : "Not connected: trying again", this.w - 136, 0, 130, kHeader, J.centredLeft);
  }
}

// ---------------------------------------------------------------- the PIN
class PinScreen extends Component {
  constructor() {
    super();
    this.digits = "";
    this.keys = [];
    for (const k of ["1", "2", "3", "4", "5", "6", "7", "8", "9", "⌫", "0", "OK"]) {
      const b = this.add(new TextButton(k, () => this.press(k)));
      b.fontHeight = 22;
      if (k === "OK") { b.toggle = true; b.bold = true; }
      this.keys.push(b);
    }
  }
  reset() { this.digits = ""; this.repaint(); }
  press(k) {
    if (performance.now() < state.waitUntil) return;
    if (k === "⌫") this.digits = this.digits.slice(0, -1);
    else if (k === "OK") { if (this.digits.length >= 4) { net.hello(this.digits); state.authMessage = ""; } }
    else if (this.digits.length < 8) this.digits += k;
    this.repaint();
  }
  resized() {
    const bw = 88, bh = 60, gap = 10, x0 = (this.w - (3 * bw + 2 * gap)) / 2, y0 = this.h / 2 - 70;
    this.keys.forEach((b, i) => b.setBounds(x0 + (i % 3) * (bw + gap), y0 + Math.floor(i / 3) * (bh + gap), bw, bh));
  }
  paint(g) {
    const pw = 420, px = (this.w - pw) / 2, py = this.h / 2 - 250;
    RK.drawPanel(g, px, py, pw, 500, Palette.tintedPanel);
    g.setColour(Palette.text); g.setFont(20, true); g.drawText("RKMix console", px, py + 18, pw, 28, J.centred);
    g.setColour(Palette.textDim); g.setFont(14);
    g.drawFittedText("Type the PIN shown on RKMix's Remote page. This iPad remembers it.", px + 30, py + 50, pw - 60, 40, J.centred, 2);
    // The digits typed, as dots
    const n = Math.max(4, this.digits.length), dx = 26, x0 = this.w / 2 - (n - 1) * dx / 2, y = py + 112;
    for (let i = 0; i < n; i++) {
      g.setColour(i < this.digits.length ? Palette.text : Palette.knobTrack);
      if (i < this.digits.length) g.fillEllipse(x0 + i * dx - 7, y - 7, 14, 14); else g.drawEllipse(x0 + i * dx - 7, y - 7, 14, 14, 1.5);
    }
    if (state.authMessage) { g.setColour(Palette.warning); g.setFont(14); g.drawText(state.authMessage, px, py + 132, pw, 22, J.centred); }
  }
}

class Message extends Component {
  paint(g) {
    g.setColour(Palette.textDim); g.setFont(16);
    g.drawText(state.connected ? "Signing in…" : "Looking for RKMix on the network…", 0, 0, this.w, this.h, J.centred);
  }
}

// ---------------------------------------------------------------- the root
class Root extends Component {
  constructor() {
    super();
    this.header = this.add(new Header());
    this.pin = new PinScreen();
    this.wait = new Message();
    this.pageViews = new Map();
    this.content = null;
  }
  showPage(key) {
    state.page = key;
    this.layout();
    app.dirty = true;
  }
  page(key) {
    if (!this.pageViews.has(key)) { const p = pages.find(p => p.key === key); if (p) this.pageViews.set(key, p.make()); }
    return this.pageViews.get(key);
  }
  layout() {
    const next = !state.connected ? this.wait : state.signedIn ? this.page(state.page) || this.wait
               : state.authAnswered ? this.pin : this.wait;
    if (next !== this.content) {
      if (this.content) { this.remove(this.content); if (this.content.hidden) this.content.hidden(); }
      this.content = next;
      if (next) { this.add(next); if (next.shown) next.shown(); }
    }
    this.resized();
  }
  resized() {
    this.header.setBounds(0, 0, this.w, kHeader);
    if (this.content) this.content.setBounds(0, kHeader, this.w, this.h - kHeader);
  }
}

let root, pinScreen;
function start() {
  root = new Root();
  pinScreen = root.pin;
  root.header.setTabs();
  if (C.afterStart) C.afterStart();
  app.start(root, kDesignHeight);
  root.layout();
  net.connect();
  // Keep the connection state in the layout
  let last = "";
  app.frameHooks.add(() => {
    const k = `${state.connected}|${state.signedIn}|${state.authAnswered}`;
    if (k !== last) { last = k; root.layout(); }
  });
  // Wake from the background: reconnect at once
  document.addEventListener("visibilitychange", () => {
    if (document.visibilityState === "visible" && (!net.ws || net.ws.readyState > 1)) net.connect();
  });
}

return { state, net, store, curves, listeners, pages, addPage, start, get root() { return root; },
         kHeader, isStereo, channelLabel, channelName, busName, dcaName, muteGroupName, stripName, busType, busTypeNames, num, kUserPageBus };
})();
