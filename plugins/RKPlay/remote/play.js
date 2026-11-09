// RKPlay's web remote: a phone or an iPad playing the music of a night.
// Pages: Play (the song, the transport, Level, Duck, End at), Songs (any
// playlist, and a search across all of them), Queue (reorder, remove),
// Lists (switch now or after this song) and Pads. A phone shows one page at
// a time (tabs along the bottom); an iPad in landscape shows Play on the left
// and the other pages (Pads too) on the right. Drawn with the suite's
// own look (rk.js) on one canvas; the protocol is PlayRemoteHub's
// (plugins/RKPlay/PlayRemote.h). Playback only: a tap on a song only
// selects it, and Stop and Panic need Unlock first.
"use strict";

const P = (() => {
const { Colour, Palette, J, Component, TextButton, PopupMenu, app, editText, Style } = RK;

// RKPlay's accent: the utility family's sage (rk.js already tints for utility).
Palette.accent = Colour.hex(0xff6fa58a);
const Green = Colour.hex(0xff5cb85c);

// An iPad (or a wide window) in landscape: Play on the left, the rest on the right, scaled to about 1000 wide.
const wideScreen = () => window.innerWidth >= 900 && window.innerWidth > window.innerHeight;
const kHeader = 48, kTabs = 58, kPad = 12, kRow = 54;
const kUnlockSeconds = 5, kFollowAfter = 30000, kNoteSeconds = 6;

const state = {
  connected: false, signedIn: false, authMessage: "", waitUntil: 0,
  players: [], player: null,
  now: null, nowAt: 0, lists: null, queue: [], pads: [],
  songs: null,                 // { playlist, songs: [[title, artist, len, missing, file]] }
  browse: -1,                  // the playlist the Songs page shows (-1: the playing one)
  query: "", results: null, more: false,
  selected: null,              // { playlist, index, file, title, artist }
  page: "play", wide: false,
  unlockedAt: 0, noteVersion: -1, note: "", noteAt: 0, toast: "", toastAt: 0,
};

const fmt = s => {
  s = Math.max(0, Math.round(s || 0));
  const h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, sec = s % 60;
  return (h > 0 ? h + ":" + String(m).padStart(2, "0") : String(m)) + ":" + String(sec).padStart(2, "0");
};
const songName = (title, artist) => artist ? artist + " - " + title : title;
const isPlaying = () => state.now && state.now.state === "playing";
const unlocked = () => performance.now() - state.unlockedAt < kUnlockSeconds * 1000;
function toast(text) { state.toast = text; state.toastAt = performance.now(); app.dirty = true; }

// The position, moved on between the server's quarter seconds.
function position() {
  const n = state.now; if (!n) return 0;
  const p = n.pos + (isPlaying() ? (performance.now() - state.nowAt) / 1000 : 0);
  return Math.min(p, n.len || p);
}

// ---------------------------------------------------------------- the connection
const net = {
  ws: null, retry: null,
  connect() {
    clearTimeout(this.retry);
    const ws = new WebSocket((location.protocol === "https:" ? "wss://" : "ws://") + location.host + "/ws/play");
    this.ws = ws;
    ws.onopen = () => { state.connected = true; this.opened = false; this.hello(takePinFromLink()); root.layout(); };
    ws.onmessage = e => receive(JSON.parse(e.data));
    ws.onclose = () => {
      if (this.ws !== ws) return;
      state.connected = false; state.signedIn = false; root.layout();
      this.retry = setTimeout(() => this.connect(), 2000);
    };
  },
  send(msg) { if (this.ws && this.ws.readyState === 1) this.ws.send(JSON.stringify(msg)); },
  hello(pin) {
    let token = ""; try { token = localStorage.getItem("rkplay.token") || ""; } catch (_) {}
    const msg = { t: "hello", name: deviceName() };
    if (pin) msg.pin = pin; else msg.token = token;
    this.send(msg);
  },
};

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
        if (msg.token) try { localStorage.setItem("rkplay.token", msg.token); } catch (_) {}
        state.signedIn = true; state.authMessage = "";
      } else {
        state.signedIn = false;
        state.authMessage = msg.off ? "The web remote is off in RKPlay (Settings > Web remote)."
                          : msg.wait ? `Too many tries: wait ${msg.wait} seconds.` : msg.wrong ? "Wrong PIN" : "";
        if (msg.wait) state.waitUntil = performance.now() + msg.wait * 1000;
      }
      root.layout();
      break;
    case "players":
      state.players = msg.list;
      choosePlayer();
      break;
    case "now": {
      const before = state.now;
      state.now = msg; state.nowAt = performance.now();
      if (state.noteVersion < 0) state.noteVersion = msg.noteVersion;   // an old note isn't news
      else if (msg.noteVersion !== state.noteVersion) { state.noteVersion = msg.noteVersion; state.note = msg.note; state.noteAt = performance.now(); }
      if (!before) { root.layout(); askSongs(); }
      else if (state.browse < 0 && before.active !== msg.active) askSongs();
      if (!before || !before.playing !== !msg.playing || (before.playing && msg.playing &&
          (before.playing.index !== msg.playing.index || before.playing.playlist !== msg.playing.playlist))) root.pages.songs.follow();
      break;
    }
    case "lists": state.lists = msg; if (state.browse >= (msg.lists || []).length) state.browse = -1; askSongs(); break;
    case "songs": state.songs = msg; reselect(); root.pages.songs.rebuild(); break;
    case "queue": state.queue = msg.items; root.pages.queue.rebuild(); break;
    case "pads": state.pads = msg.pads; break;
    case "search": if (msg.q === state.query) { state.results = msg.results; state.more = msg.more; root.pages.songs.rebuild(); } break;
    case "moved": toast("That song has moved or gone: try again"); break;
    case "gone": state.player = null; state.now = null; choosePlayer(); root.layout(); break;
  }
  app.dirty = true;
}

// Several RKPlays (one per rackspace): the one this device had, the only one, or the list.
function choosePlayer() {
  const list = state.players;
  // (A new connection opens it again: the server forgot it.)
  if (state.player && net.opened && list.some(p => p.id === state.player.id)) { root.layout(); return; }
  if (state.player && !net.opened) { const same = list.find(p => p.id === state.player.id && p.name === state.player.name); if (same) { openPlayer(same); return; } }
  let remembered = ""; try { remembered = localStorage.getItem("rkplay.player") || ""; } catch (_) {}
  const pick = list.length === 1 ? list[0] : list.find(p => p.name === remembered && list.filter(o => o.name === p.name).length === 1);
  if (pick) openPlayer(pick);
  else root.layout();
}
function openPlayer(p) {
  try { localStorage.setItem("rkplay.player", p.name); } catch (_) {}
  net.opened = true;
  state.player = p; state.now = null; state.lists = null; state.songs = null; state.selected = null; state.results = null; state.query = "";
  net.send({ t: "open", p: p.id });
  root.layout();
}

// The playlist the Songs page shows.
const browsed = () => state.browse >= 0 ? state.browse : state.now ? state.now.active : 0;
function askSongs() {
  const want = browsed();
  if (!state.songs || state.songs.playlist !== want || askSongs.sent !== want) { askSongs.sent = want; net.send({ t: "songs", playlist: want }); }
}
askSongs.sent = -1;

// The selection survives a list that changed (it follows its file), or goes.
function reselect() {
  const s = state.selected; if (!s || !state.songs || state.songs.playlist !== s.playlist) return;
  const rows = state.songs.songs;
  if (rows[s.index] && rows[s.index][4] === s.file) return;
  const i = rows.findIndex(r => r[4] === s.file);
  if (i >= 0) s.index = i; else if (!state.results) state.selected = null;
}

function playSelected(t) {
  const s = state.selected; if (!s) return;
  net.send({ t, playlist: s.playlist, index: s.index, file: s.file });
  if (t === "queueNext") toast("Next: " + songName(s.title, s.artist));
  state.selected = null; app.dirty = true;
}

// ---------------------------------------------------------------- small parts
// A big button with a symbol or a word; drawn lit when toggled.
class BigButton extends TextButton {
  constructor(text, onClick, fontHeight = 18) { super(text, onClick); this.fontHeight = fontHeight; this.bold = true; }
}

// A list that scrolls under a finger (with a little momentum); its rows are drawn, not components.
class ListView extends Component {
  constructor() {
    super(); this.takesTouches = true; this.offset = 0; this.rows = []; this.rowH = kRow; this.v = 0; this.scrolledAt = 0;
    app.frameHooks.add(dt => {
      if (this.dragging || Math.abs(this.v) < 5) { this.v = 0; return; }
      this.scrollBy(this.v * dt); this.v *= Math.pow(0.04, dt);
    });
  }
  heightOf(row) { return row && row.header ? 30 : this.rowH; }
  contentHeight() { let h = 0; for (const r of this.rows) h += this.heightOf(r); return h + kPad; }
  maxOffset() { return Math.max(0, this.contentHeight() - this.h); }
  scrollBy(dy) { const o = Math.max(0, Math.min(this.maxOffset(), this.offset - dy)); if (o !== this.offset) { this.offset = o; this.repaint(); } }
  topOf(i) { let y = 0; for (let k = 0; k < i && k < this.rows.length; k++) y += this.heightOf(this.rows[k]); return y; }
  scrollTo(i) {   // row i about a third down
    const y = this.topOf(i) - this.h / 3;
    this.offset = Math.max(0, Math.min(this.maxOffset(), y)); this.repaint();
  }
  rowAt(y) {
    let top = -this.offset;
    for (let i = 0; i < this.rows.length; i++) { const h = this.heightOf(this.rows[i]); if (y >= top && y < top + h) return i; top += h; }
    return -1;
  }
  paint(g) {
    const c = g.ctx; c.save(); c.beginPath(); c.rect(0, 0, this.w, this.h); c.clip();
    let y = -this.offset;
    for (let i = 0; i < this.rows.length; i++) {
      const h = this.heightOf(this.rows[i]);
      if (y + h > 0 && y < this.h) this.paintRow(g, this.rows[i], i, y, h);
      y += h;
      if (y > this.h) break;
    }
    if (!this.rows.length && this.empty) {
      g.setColour(Palette.textDim); g.setFont(15);
      g.drawFittedText(this.empty, kPad * 2, 30, this.w - kPad * 4, 60, J.centred, 3);
    }
    // A scroll bar while there's more than a screen.
    const total = this.contentHeight();
    if (total > this.h + 1) {
      const bh = Math.max(30, this.h * this.h / total), by = (this.h - bh) * this.offset / Math.max(1, total - this.h);
      g.setColour(Palette.textDim.withAlpha(0.35)); g.fillRoundedRectangle(this.w - 5, by, 3, bh, 1.5);
    }
    c.restore();
  }
  paintRow() {}
  tapRow() {}
  pointerDown(e) { this.dragging = true; this.v = 0; this.startY = this.lastY = e.y; this.lastT = e.time; this.moved = false; this.downRow = this.rowAt(e.y); this.downX = e.x; return true; }
  pointerMove(e) {
    if (Math.abs(e.y - this.startY) > 10) this.moved = true;
    if (!this.moved) return;
    const dt = Math.max(1, e.time - this.lastT) / 1000;
    this.v = 0.6 * this.v + 0.4 * (e.y - this.lastY) / dt;
    this.scrollBy(e.y - this.lastY); this.lastY = e.y; this.lastT = e.time; this.scrolledAt = performance.now();
  }
  pointerUp(e) {
    this.dragging = false;
    if (!this.moved && !e.cancelled && this.downRow >= 0) this.tapRow(this.rows[this.downRow], this.downRow, this.downX);
    if (!this.moved) this.v = 0;
  }
}

// One song's row: name, artist, length; the playing one marked, a missing one dimmed, the selected one lit.
function paintSong(g, w, y, h, row, opts) {
  const sel = state.selected, now = state.now;
  const selected = sel && sel.playlist === row.playlist && sel.file === row.file;
  const playing = now && now.playing && now.playing.playlist === row.playlist && now.playing.index === row.index;
  if (selected) { g.setColour(Palette.accent.withAlpha(0.35)); g.fillRoundedRectangle(4, y + 2, w - 8, h - 4, Style.cornerRadius); }
  else if (playing) { g.setColour(Palette.accent.withAlpha(0.14)); g.fillRoundedRectangle(4, y + 2, w - 8, h - 4, Style.cornerRadius); }
  const left = opts && opts.number ? 46 : 14;
  if (opts && opts.number) {
    g.setColour(Palette.textDim); g.setFont(12.5);
    g.drawText(String(row.index + 1), 4, y, 36, h, J.centredRight);
  }
  const textW = w - left - 70;
  g.setColour(row.missing ? Palette.textDim.withAlpha(0.6) : playing ? Palette.accent.brighter(0.4) : Palette.text);
  g.setFont(16, true);
  g.drawText((playing ? "▶ " : "") + (row.missing ? "(missing) " : "") + row.title, left, y + 6, textW, 22, J.centredLeft);
  g.setColour(Palette.textDim); g.setFont(13.5);
  g.drawText(row.artist || "", left, y + 28, textW, 18, J.centredLeft);
  g.setFont(13.5); g.drawText(fmt(row.len), w - 70, y, 56, h, J.centredRight);
  g.setColour(Palette.outline.withAlpha(0.5)); g.drawHorizontalLine(y + h - 1, left, w - 8);
}

// ---------------------------------------------------------------- the header and tabs
class Header extends Component {
  constructor() {
    super();
    this.name = this.add(new TextButton("", () => { if (state.players.length > 1) showPlayerMenu(this.name); }));
    this.name.colour = Colour.hex(0x00000000);
  }
  resized() { this.name.setBounds(kPad - 6, 6, this.w - 2 * kPad - 120, this.h - 12); }
  paint(g) {
    g.setColour(Palette.panel); g.fillRect(0, 0, this.w, this.h);
    g.setColour(Palette.accent); g.fillRect(0, this.h - 2, this.w, 2);
    const p = state.player, multi = state.players.length > 1;
    this.name.text = "";
    g.setColour(Palette.text); g.setFont(17, true);
    const title = p ? p.name + (multi ? " ▾" : "") : "RKPlay";
    g.drawText(title, kPad, 0, this.w - 2 * kPad - 130, this.h, J.centredLeft);
    // The connection.
    g.setColour(state.connected ? Green : Palette.warning);
    g.fillEllipse(this.w - kPad - 8, this.h / 2 - 4, 8, 8);
    g.setColour(Palette.textDim); g.setFont(13);
    g.drawText(state.connected ? (state.now ? state.now.list : "") : "Reconnecting...", this.w - kPad - 140, 0, 124, this.h, J.centredRight);
  }
}

function showPlayerMenu(anchor) {
  PopupMenu.show(state.players.map(p => ({ text: p.name, ticked: state.player && p.id === state.player.id, action: () => openPlayer(p) })), anchor);
}

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
    for (const b of this.buttons) {
      b.toggle = b.key === state.page;
      if (b.key === "queue") b.text = state.queue.length ? `Queue (${state.queue.length})` : "Queue";
    }
  }
}

// ---------------------------------------------------------------- Play
// The Level: a wide fader, held by the finger while it moves (the server's value waits).
class LevelFader extends Component {
  constructor() { super(); this.takesTouches = true; this.held = false; this.value = 0; this.sentAt = 0; }
  norm() { return this.held || performance.now() - this.sentAt < 600 ? this.value : state.now && state.now.level !== undefined ? state.now.level : 0; }
  paint(g) {
    const cy = this.h / 2, ind = 20, pos = ind + this.norm() * (this.w - 2 * ind);
    g.setColour(Palette.knobTrack); g.fillRoundedRectangle(ind - 4, cy - 5, this.w - 2 * ind + 8, 10, 5);
    g.setColour(Palette.accent.withAlpha(0.6)); g.fillRoundedRectangle(ind - 4, cy - 5, pos - ind + 8, 10, 5);
    g.setColour(Palette.text); g.fillRoundedRectangle(pos - 14, cy - 20, 28, 40, 6);
    g.setColour(Palette.knobBody); g.fillRect(pos - 8, cy - 1, 16, 2);
  }
  pointerDown(e) { this.held = true; this.value = this.norm(); this.lastX = e.x; return true; }
  pointerMove(e) {
    this.value = Math.max(0, Math.min(1, this.value + (e.x - this.lastX) / Math.max(60, this.w - 40))); this.lastX = e.x;
    this.send();
  }
  pointerUp() { this.held = false; this.send(); }
  send() { this.sentAt = performance.now(); net.send({ t: "level", v: +this.value.toFixed(4) }); this.repaint(); }
}

// The song's progress: elapsed, the bar, the time left.
class Progress extends Component {
  paint(g) {
    const n = state.now; if (!n) return;
    const stopped = n.state === "stopped", pos = position(), len = n.len || 0;
    const bar = 8, y = 4;
    g.setColour(Palette.knobTrack); g.fillRoundedRectangle(0, y, this.w, bar, bar / 2);
    if (!stopped && len > 0) {
      g.setColour(n.state === "paused" ? Palette.accent.withAlpha(0.5) : Palette.accent);
      g.fillRoundedRectangle(0, y, Math.max(bar, this.w * Math.min(1, pos / len)), bar, bar / 2);
    }
    g.setColour(Palette.textDim); g.setFont(14);
    g.drawText(fmt(pos), 0, y + bar + 4, 80, 20, J.centredLeft);
    g.drawText(stopped ? fmt(len) : "-" + fmt(Math.max(0, len - pos)), this.w - 90, y + bar + 4, 90, 20, J.centredRight);
  }
}

class PlayPage extends Component {
  constructor() {
    super();
    this.progress = this.add(new Progress());
    this.prev = this.add(new BigButton("⏮", () => net.send({ t: "prev" }), 24));
    this.play = this.add(new BigButton("Play", () => net.send({ t: "playpause" }), 22));
    this.next = this.add(new BigButton("⏭", () => net.send({ t: "next" }), 24));
    this.fade = this.add(new BigButton("Fade out", () => net.send({ t: "fade" }), 16));
    this.unlock = this.add(new BigButton("Unlock", () => { state.unlockedAt = unlocked() ? 0 : performance.now(); app.dirty = true; }, 15));
    this.stop = this.add(new BigButton("Stop", () => { if (unlocked()) { net.send({ t: "stop" }); state.unlockedAt = 0; } }, 15));
    this.panic = this.add(new BigButton("Panic", () => { if (unlocked()) { net.send({ t: "panic" }); state.unlockedAt = 0; } }, 15));
    this.level = this.add(new LevelFader());
    this.duck = this.add(new BigButton("Duck", () => net.send({ t: "duck", on: !(state.now && state.now.duck) }), 16));
    this.endAt = this.add(new TextButton("End at...", () => this.askEndAt()));
    this.endOff = this.add(new TextButton("Off", () => net.send({ t: "endAt", off: true })));
    this.cancel = this.add(new TextButton("Cancel", () => net.send({ t: "cancelSwitch" })));
    for (const b of [this.endAt, this.endOff, this.cancel]) b.fontHeight = 14;
  }
  structureChanged() { return false; }
  askEndAt() {
    const n = state.now, value = n && n.endAt ? n.endAt.time : "22:00";
    editText(this.endAt, value, text => {
      const m = /^\s*(\d{1,2})[:.](\d{2})\s*$/.exec(text);
      if (m && +m[1] < 24 && +m[2] < 60) net.send({ t: "endAt", time: m[1].padStart(2, "0") + ":" + m[2] });
      else if (text.trim()) toast("End at: a time like 23:30");
    }, { maxLength: 5, fontHeight: 18 });
  }
  resized() {
    const w = this.w - 2 * kPad;
    let y = kPad + 96;                       // the song's names above
    this.progress.setBounds(kPad, y, w, 40); y += 48;
    this.infoY = y; y += 26;                // Next
    const gap = 8, big = 64;
    const side = Math.min(90, (w - 3 * gap) * 0.22), fadeW = Math.min(120, (w - 3 * gap) * 0.24), playW = w - 2 * side - fadeW - 3 * gap;
    this.prev.setBounds(kPad, y, side, big);
    this.play.setBounds(kPad + side + gap, y, playW, big);
    this.next.setBounds(kPad + side + gap + playW + gap, y, side, big);
    this.fade.setBounds(this.w - kPad - fadeW, y, fadeW, big);
    y += big + gap;
    const third = (w - 2 * gap) / 3;
    this.unlock.setBounds(kPad, y, third, 44);
    this.stop.setBounds(kPad + third + gap, y, third, 44);
    this.panic.setBounds(kPad + 2 * (third + gap), y, third, 44);
    y += 44 + 18;
    this.levelY = y; y += 20;
    const duckW = 96;
    this.level.setBounds(kPad - 8, y, w - duckW - gap + 8, 56);
    this.duck.setBounds(this.w - kPad - duckW, y + 4, duckW, 48);
    y += 56 + 12;
    this.endY = y;
    this.endAt.setBounds(this.w - kPad - 96 - gap - 60, y, 96, 36);
    this.endOff.setBounds(this.w - kPad - 60, y, 60, 36);
    y += 44;
    this.switchY = y;
    this.cancel.setBounds(this.w - kPad - 80, y + 26, 80, 34);
  }
  paint(g) {
    const n = state.now;
    if (!n) { g.setColour(Palette.textDim); g.setFont(16); g.drawText("...", 0, 40, this.w, 30, J.centred); return; }
    const stopped = n.state === "stopped";
    // The song.
    g.setColour(Palette.textDim); g.setFont(13, true);
    g.drawText((stopped ? "READY" : n.state === "paused" ? "PAUSED" : "PLAYING") + "  ·  " + n.list.toUpperCase(), kPad, kPad, this.w - 2 * kPad, 18, J.centredLeft);
    g.setColour(Palette.text); g.setFont(26, true);
    g.drawFittedText(n.title || "No songs", kPad, kPad + 22, this.w - 2 * kPad, 40, J.centredLeft, 1, 0.6);
    g.setColour(Palette.textDim); g.setFont(18);
    g.drawFittedText(n.artist || "", kPad, kPad + 62, this.w - 2 * kPad, 26, J.centredLeft, 1, 0.7);
    g.setFont(14);
    const next = state.queue.length ? songName(state.queue[0].title, state.queue[0].artist) : n.next;
    if (next) { g.setColour(Palette.textDim); g.drawText("Next:  " + next, kPad, this.infoY, this.w - 2 * kPad, 22, J.centredLeft); }

    this.play.text = isPlaying() ? "Pause" : "Play";
    this.play.toggle = isPlaying();
    const open = unlocked();
    this.unlock.toggle = open; this.unlock.onColour = Palette.warning; this.unlock.text = open ? "Unlocked" : "Unlock";
    this.stop.enabled = this.panic.enabled = open;

    g.setColour(Palette.textDim); g.setFont(13, true);
    g.drawText("LEVEL", kPad, this.levelY, 80, 18, J.centredLeft);
    g.setFont(13); g.drawText(n.levelText || "", kPad + 60, this.levelY, this.level.w - 60, 18, J.centredRight);
    this.duck.toggle = !!n.duck;
    if (n.ducking && n.duckDb < 0) { g.setColour(Palette.textDim); g.setFont(12.5); g.drawText(n.duckDb + " dB", this.duck.x, this.levelY, this.duck.w, 18, J.centred); }

    // End at, Switch at, a switch that waits.
    g.setFont(15);
    if (n.endAt) {
      g.setColour(Palette.text);
      g.drawFittedText(`Ends ${n.endAt.atSongEnd ? "after the song at" : "at"} ${n.endAt.time}  ·  in ${fmt(n.endAt.left)}`, kPad, this.endY, this.endAt.x - kPad - 8, 36, J.centredLeft, 1, 0.7);
    } else { g.setColour(Palette.textDim); g.drawText("End at: not set", kPad, this.endY, this.endAt.x - kPad - 8, 36, J.centredLeft); }
    this.endOff.setVisible(!!n.endAt);
    let line = this.switchY;
    if (n.switchAt) {
      g.setColour(Palette.textDim);
      g.drawFittedText(`Switches to ${n.switchAt.to} at ${n.switchAt.time}  ·  in ${fmt(n.switchAt.left)}`, kPad, line, this.w - 2 * kPad, 24, J.centredLeft, 1, 0.7);
    }
    line += 26;
    this.cancel.setVisible(n.pending !== undefined);
    if (n.pending !== undefined) {
      g.setColour(Palette.warning);
      g.drawFittedText(`Switching to ${n.pendingName} after this song`, kPad, line, this.cancel.x - kPad - 8, 34, J.centredLeft, 1, 0.7);
    }
  }
}

// ---------------------------------------------------------------- Pads
class PadTile extends Component {
  constructor(n) { super(); this.n = n; this.takesTouches = true; }
  pad() { return state.pads[this.n] || { name: "", sound: false, playing: false, progress: 0 }; }
  paint(g) {
    const p = this.pad(), on = p.playing;
    let fill = on ? Palette.accent : Palette.knobBody;
    if (this.down) fill = fill.brighter(0.15);
    g.setColour(p.sound ? fill : fill.withMultipliedAlpha(0.5)); g.fillRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, Style.panelRadius);
    const tc = on ? Palette.onAccent(Palette.accent) : Palette.text;
    g.setColour(tc.withMultipliedAlpha(p.sound ? 1 : 0.5)); g.setFont(Math.min(16, this.h * 0.3), true);
    g.drawFittedText(p.name || `Pad ${this.n + 1}`, 6, 4, this.w - 12, this.h - 18, J.centred, 2, 0.7);
    g.setFont(11.5); g.setColour(tc.withMultipliedAlpha(0.6));
    g.drawText(p.sound ? String(this.n + 1) : "no sound", 6, this.h - 18, this.w - 12, 14, J.centredLeft);
    if (on) { g.setColour(Palette.onAccent(Palette.accent).withAlpha(0.5)); g.fillRect(4, this.h - 4, (this.w - 8) * p.progress, 3); }
  }
  pointerDown() { if (!this.pad().sound) return false; this.down = true; this.repaint(); return true; }
  pointerUp(e) { this.down = false; this.repaint(); if (!e.cancelled && this.hitTest(e.x, e.y)) net.send({ t: "pad", n: this.n }); }
}

class PadsPage extends Component {
  constructor() { super(); this.tiles = []; for (let i = 0; i < 8; i++) this.tiles.push(this.add(new PadTile(i))); }
  structureChanged() { return false; }
  resized() {
    const cols = this.w >= 560 ? 4 : 2, rows = 8 / cols, gap = 8;
    const top = kPad, tw = (this.w - 2 * kPad - (cols - 1) * gap) / cols;
    const th = Math.max(48, Math.min(110, (this.h - top - kPad - (rows - 1) * gap) / rows));
    this.tiles.forEach((t, i) => t.setBounds(kPad + (i % cols) * (tw + gap), top + Math.floor(i / cols) * (th + gap), tw, th));
  }
}

// ---------------------------------------------------------------- Songs
class SongList extends ListView {
  paintRow(g, row, i, y, h) {
    if (row.header) {
      g.setColour(Palette.textDim); g.setFont(12.5, true);
      g.drawText(row.header.toUpperCase(), kPad, y + 8, this.w - 2 * kPad, 18, J.centredLeft);
      return;
    }
    paintSong(g, this.w, y, h, row, { number: !state.results });
  }
  tapRow(row) {
    if (!row || row.header) return;
    const s = state.selected;
    state.selected = s && s.playlist === row.playlist && s.file === row.file ? null
                   : { playlist: row.playlist, index: row.index, file: row.file, title: row.title, artist: row.artist };
    this.repaint();
  }
}

class SongsPage extends Component {
  constructor() {
    super();
    this.listButton = this.add(new TextButton("", () => this.chooseList()));
    this.listButton.fontHeight = 15;
    this.searchButton = this.add(new TextButton("Search", () => this.askSearch()));
    this.searchButton.fontHeight = 15;
    this.clear = this.add(new TextButton("×", () => { state.query = ""; state.results = null; this.rebuild(); }));
    this.clear.fontHeight = 20;
    this.list = this.add(new SongList());
    this.playNext = this.add(new BigButton("Play next", () => playSelected("queueNext"), 15));
    this.playNow = this.add(new BigButton("Play now", () => playSelected("playNow"), 15));
    this.jump = this.add(new BigButton("Go on from here", () => playSelected("jump"), 14));
  }
  structureChanged() { return false; }
  chooseList() {
    const l = state.lists; if (!l) return;
    PopupMenu.show(l.lists.map((p, i) => ({
      text: p.name + "  (" + p.count + ")" + (i === l.active ? "  ▶" : ""), ticked: i === browsed(),
      action: () => { state.browse = i === l.active ? -1 : i; state.query = ""; state.results = null; askSongs(); this.rebuild(); this.list.offset = 0; },
    })), this.listButton);
  }
  askSearch() {
    editText(this.searchButton, state.query, text => {
      state.query = text.trim();
      state.results = null;
      if (state.query) net.send({ t: "search", q: state.query });
      this.rebuild();
    }, { maxLength: 60, fontHeight: 16, left: true });
  }
  rebuild() {
    const rows = [];
    if (state.query) {
      if (state.results) {
        let last = -1;
        for (const r of state.results) {
          if (r[0] !== last) { last = r[0]; rows.push({ header: state.lists && state.lists.lists[r[0]] ? state.lists.lists[r[0]].name : "Playlist " + (r[0] + 1) }); }
          rows.push({ playlist: r[0], index: r[1], title: r[2], artist: r[3], len: r[4], missing: !!r[5], file: r[6] });
        }
        if (state.more) rows.push({ header: "More songs match: type more of the name" });
      }
      this.list.empty = state.results ? `Nothing found for "${state.query}"` : "Searching...";
    } else {
      const s = state.songs;
      if (s && s.playlist === browsed())
        s.songs.forEach((r, i) => rows.push({ playlist: s.playlist, index: i, title: r[0], artist: r[1], len: r[2], missing: !!r[3], file: r[4] }));
      this.list.empty = s ? "No songs in this playlist" : "...";
    }
    const was = this.list.rows.length;
    this.list.rows = rows;
    this.list.offset = Math.min(this.list.offset, this.list.maxOffset());
    if (!was && rows.length) this.follow(true);
    this.repaint();
  }
  // Scroll to the playing song, unless the list was scrolled by hand in the last 30 s.
  follow(force) {
    const n = state.now; if (!n || state.query) return;
    if (!force && performance.now() - this.list.scrolledAt < kFollowAfter) return;
    const at = n.playing && n.playing.playlist === browsed() ? n.playing.index : browsed() === n.active ? n.cursor : -1;
    if (at >= 0 && at < this.list.rows.length) this.list.scrollTo(at);
  }
  resized() {
    const bar = 44, w = this.w - 2 * kPad;
    this.listButton.setBounds(kPad, kPad, w * 0.45, bar - 4);
    this.searchButton.setBounds(kPad + w * 0.45 + 8, kPad, w * 0.55 - 8 - 46, bar - 4);
    this.clear.setBounds(this.w - kPad - 40, kPad, 40, bar - 4);
    const actions = state.selected ? 60 : 0;
    this.list.setBounds(0, kPad + bar + 4, this.w, this.h - kPad - bar - 4 - actions);
    const ay = this.h - actions + 8, aw = (w - 16) / 3;
    this.playNext.setBounds(kPad, ay, aw, 46);
    this.playNow.setBounds(kPad + aw + 8, ay, aw, 46);
    this.jump.setBounds(kPad + 2 * (aw + 8), ay, aw, 46);
  }
  paint(g) {
    const l = state.lists, b = browsed();
    this.listButton.text = (l && l.lists[b] ? l.lists[b].name : "Playlist") + " ▾";
    this.listButton.toggle = state.browse >= 0 && !!l && b !== l.active;
    this.searchButton.text = state.query ? "Search: " + state.query : "Search all playlists";
    this.clear.setVisible(!!state.query);
    const shown = !!state.selected;
    for (const c of [this.playNext, this.playNow, this.jump]) c.setVisible(shown);
    this.jump.enabled = shown;
    if ((this.list.h + (shown ? 60 : 0)) !== this.h - kPad - 48) this.resized();
    if (shown) { g.setColour(Palette.panel); g.fillRect(0, this.h - 60, this.w, 60); }
  }
}

// ---------------------------------------------------------------- Queue
class QueueList extends ListView {
  constructor() { super(); this.drag = null; }
  paintRow(g, row, i, y, h) {
    const dragging = this.drag && this.drag.from === i;
    if (dragging) return;
    this.paintItem(g, row, y, h, i);
  }
  paintItem(g, row, y, h, i) {
    g.setColour(Palette.textDim); g.setFont(18); g.drawText("≡", 8, y, 30, h, J.centred);
    g.setColour(Palette.text); g.setFont(16, true);
    g.drawText(row.title, 46, y + 6, this.w - 46 - 110, 22, J.centredLeft);
    g.setColour(Palette.textDim); g.setFont(13.5);
    g.drawText(row.artist || "", 46, y + 28, this.w - 46 - 110, 18, J.centredLeft);
    g.drawText(fmt(row.len), this.w - 110, y, 50, h, J.centredRight);
    g.setColour(Palette.knobBody); g.fillRoundedRectangle(this.w - 50, y + 9, 38, h - 18, Style.cornerRadius);
    g.setColour(Palette.text); g.setFont(18); g.drawText("×", this.w - 50, y + 9, 38, h - 18, J.centred);
    g.setColour(Palette.outline.withAlpha(0.5)); g.drawHorizontalLine(y + h - 1, 46, this.w - 8);
    if (i === 0) { g.setColour(Palette.accent); g.fillRect(4, y + 8, 3, h - 16); }
  }
  paintOver(g) {
    if (!this.drag) return;
    const row = this.rows[this.drag.from], y = this.drag.y - this.rowH / 2;
    g.setColour(Palette.panel.brighter(0.15)); g.fillRoundedRectangle(2, y, this.w - 4, this.rowH, Style.cornerRadius);
    this.paintItem(g, row, y, this.rowH, -1);
    const to = this.dropIndex();
    g.setColour(Palette.accent); g.fillRect(8, Math.min(to, this.rows.length) * this.rowH - this.offset - 1 + (to > this.drag.from ? this.rowH : 0), this.w - 16, 2);
  }
  dropIndex() { return Math.max(0, Math.min(this.rows.length - 1, Math.floor((this.drag.y + this.offset) / this.rowH))); }
  pointerDown(e) {
    const i = this.rowAt(e.y);
    if (i >= 0 && e.x < 46) { this.drag = { from: i, y: e.y }; this.repaint(); return true; }   // the handle: reorder
    return super.pointerDown(e);
  }
  pointerMove(e) { if (this.drag) { this.drag.y = e.y; this.repaint(); return; } super.pointerMove(e); }
  pointerUp(e) {
    if (this.drag) {
      const d = this.drag, to = this.dropIndex(); this.drag = null; this.repaint();
      if (!e.cancelled && to !== d.from) net.send({ t: "queueMove", id: this.rows[d.from].id, to });
      return;
    }
    super.pointerUp(e);
  }
  tapRow(row, i, x) { if (row && x > this.w - 56) net.send({ t: "queueRemove", id: row.id }); }
}

class QueuePage extends Component {
  constructor() {
    super();
    this.list = this.add(new QueueList());
    this.list.empty = "No requests. In Songs, tap a song and choose Play next.";
    this.clear = this.add(new TextButton("Clear", () => PopupMenu.show([{ text: `Remove all ${state.queue.length} requests`, colour: Palette.warning, action: () => net.send({ t: "queueClear" }) }], this.clear)));
    this.clear.fontHeight = 14;
  }
  structureChanged() { return false; }
  rebuild() { this.list.rows = state.queue; this.list.offset = Math.min(this.list.offset, this.list.maxOffset()); this.repaint(); }
  resized() { this.clear.setBounds(this.w - kPad - 80, kPad, 80, 36); this.list.setBounds(0, kPad + 44, this.w, this.h - kPad - 44); }
  paint(g) {
    const n = state.queue.length;
    g.setColour(Palette.textDim); g.setFont(15);
    g.drawText(n ? `${n} ${n === 1 ? "request" : "requests"}: they play after this song, in this order` : "The queue", kPad, kPad, this.w - 2 * kPad - 90, 36, J.centredLeft);
    this.clear.setVisible(n > 0);
  }
}

// ---------------------------------------------------------------- Lists
class ListsPage extends ListView {
  constructor() { super(); this.rowH = 64; this.empty = "..."; }
  structureChanged() { return false; }
  paint(g) {
    const l = state.lists;
    this.rows = l ? l.lists.map((p, i) => Object.assign({ i }, p)) : [];
    super.paint(g);
  }
  paintRow(g, row, i, y, h) {
    const n = state.now, active = state.lists && state.lists.active === row.i, pending = n && n.pending === row.i;
    let fill = active ? Palette.accent : Palette.knobBody;
    g.setColour(fill); g.fillRoundedRectangle(kPad, y + 4, this.w - 2 * kPad, h - 8, Style.panelRadius);
    const tc = active ? Palette.onAccent(Palette.accent) : Palette.text;
    g.setColour(tc); g.setFont(17, true);
    g.drawText((active ? "▶ " : "") + row.name, kPad + 14, y + 4, this.w - 2 * kPad - 150, h - 8, J.centredLeft);
    g.setFont(13.5); g.setColour(pending ? Palette.warning : tc.withMultipliedAlpha(0.7));
    g.drawText(pending ? "next, after this song" : `${row.count} songs${row.linked ? ", linked" : ""}`, this.w - kPad - 150, y + 4, 136, h - 8, J.centredRight);
  }
  tapRow(row) {
    if (!row || (state.lists && state.lists.active === row.i)) return;
    const anchor = { screenPos: () => { const p = this.screenPos(); return { x: p.x + kPad, y: p.y + this.topOf(row.i) - this.offset }; }, w: this.w - 2 * kPad, h: this.rowH };
    PopupMenu.show([
      { header: true, text: row.name },
      { text: "Switch now (crossfade)", action: () => net.send({ t: "switch", playlist: row.i, now: true }) },
      { text: "After this song", action: () => net.send({ t: "switch", playlist: row.i, now: false }) },
      { separator: true },
      { text: "Show its songs", action: () => { state.browse = row.i; state.query = ""; state.results = null; askSongs(); root.pages.songs.rebuild(); root.showPage("songs"); } },
    ], anchor);
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
    g.setColour(Palette.text); g.setFont(24, true); g.drawText("RKPlay Remote", 0, this.h / 2 - 150, this.w, 36, J.centred);
    g.setColour(Palette.textDim); g.setFont(15);
    const text = !state.connected ? "Connecting to RKPlay..." : state.authMessage ||
                 "Type the PIN shown in RKPlay's Settings > Web remote (or scan its QR code).";
    g.drawFittedText(text, kPad * 2, this.h / 2 - 100, this.w - kPad * 4, 80, J.centred, 3);
    this.enter.setVisible(state.connected && !state.authMessage.startsWith("The web remote is off"));
  }
}

class PlayerList extends Component {
  constructor() { super(); this.buttons = []; }
  structureChanged() { return false; }
  resized() {
    for (const b of this.buttons) this.remove(b);
    this.buttons = state.players.map(p => { const t = this.add(new TextButton(p.name, () => openPlayer(p))); t.fontHeight = 17; return t; });
    this.buttons.forEach((b, i) => b.setBounds(kPad, 70 + i * 64, this.w - 2 * kPad, 54));
  }
  paint(g) {
    g.setColour(Palette.textDim); g.setFont(16);
    g.drawText(state.players.length ? "Which RKPlay?" : "No RKPlay is running", 0, 16, this.w, 40, J.centred);
  }
}

// ---------------------------------------------------------------- the root
const phonePages = [
  { key: "play", title: "Play" }, { key: "songs", title: "Songs" }, { key: "queue", title: "Queue" },
  { key: "lists", title: "Lists" }, { key: "pads", title: "Pads" },
];
const widePages = phonePages.filter(d => d.key !== "play");

class Root extends Component {
  constructor() {
    super();
    this.header = this.add(new Header());
    this.tabs = this.add(new TabBar());
    this.pages = { play: new PlayPage(), songs: new SongsPage(), queue: new QueuePage(), lists: new ListsPage(), pads: new PadsPage() };
    this.left = new PlayPage();           // an iPad in landscape: Play, always there
    this.pin = new PinScreen(); this.list = new PlayerList();
    this.page = this.pin;
    this.add(this.page);
  }
  showPage(key) { state.page = key; this.layout(); }
  layout() {
    state.wide = wideScreen();
    const ready = state.signedIn && state.player && state.now;
    let next;
    if (!state.signedIn) next = this.pin;
    else if (!state.player) next = this.list;
    else {
      const defs = state.wide ? widePages : phonePages;
      if (!defs.some(d => d.key === state.page)) state.page = state.wide ? "songs" : "play";
      if (this.tabs.buttons.length !== defs.length) this.tabs.setPages(defs);
      next = this.pages[state.page];
    }
    if (next !== this.page) { this.remove(this.page); this.page = next; this.add(next); }
    const showLeft = ready && state.wide;
    if (showLeft && !this.left.parent) this.add(this.left);
    if (!showLeft && this.left.parent) this.remove(this.left);
    this.tabs.setVisible(!!ready);
    this.resized();
    app.dirty = true;
  }
  resized() {
    if (wideScreen() !== state.wide) { this.layout(); return; }
    this.header.setBounds(0, 0, this.w, kHeader);
    const leftW = this.left.parent ? Math.round(Math.min(520, this.w * 0.46)) : 0;
    this.left.setBounds(0, kHeader, leftW, this.h - kHeader);
    const tabs = this.tabs.visible ? kTabs : 0;
    if (state.wide) this.tabs.setBounds(leftW, kHeader, this.w - leftW, tabs);
    else this.tabs.setBounds(0, this.h - tabs, this.w, tabs);
    if (state.wide) this.page.setBounds(leftW, kHeader + tabs, this.w - leftW, this.h - kHeader - tabs);
    else this.page.setBounds(0, kHeader, this.w, this.h - kHeader - tabs);
  }
  paintOver(g) {
    if (this.left.parent) { g.setColour(Palette.outline); g.fillRect(this.left.w, kHeader, 1, this.h - kHeader); }
    // A note from RKPlay ("Skipped: file missing"), or this page's own: a few seconds at the bottom.
    const now = performance.now();
    const text = now - state.toastAt < 3000 ? state.toast : now - state.noteAt < kNoteSeconds * 1000 ? state.note : "";
    if (!text) return;
    g.setFont(14); const tw = Math.min(this.w - 32, g.textWidth(text) + 32), y = this.h - (this.tabs.visible && !state.wide ? kTabs : 0) - 56;
    g.setColour(Palette.panel.brighter(0.25)); g.fillRoundedRectangle((this.w - tw) / 2, y, tw, 40, Style.panelRadius);
    g.setColour(Palette.text); g.drawFittedText(text, (this.w - tw) / 2 + 12, y, tw - 24, 40, J.centred, 2, 0.75);
  }
}

let root = null;
function start() {
  root = new Root();
  // (Before app's own listener: the scale follows the shape.)
  const fit = () => { app.fitWidth = wideScreen() ? 1000 : 400; };
  window.addEventListener("resize", fit);
  window.addEventListener("orientationchange", () => setTimeout(fit, 150));
  app.start(root, 0, { fitWidth: wideScreen() ? 1000 : 400 });
  // The clock and the bars move on between messages (ten times a second is plenty).
  let lastTick = 0;
  app.frameHooks.add(() => {
    const now = performance.now();
    if (now - lastTick > 100) {
      lastTick = now;
      if (isPlaying() || unlocked() || state.unlockedAt || now - state.noteAt < kNoteSeconds * 1000 + 200 || now - state.toastAt < 3200) app.dirty = true;
      if (state.unlockedAt && !unlocked()) state.unlockedAt = 0;
    }
  });
  net.connect();
}

return { start, state };
})();
