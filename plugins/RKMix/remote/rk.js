// RKFX suite graphics for the web: the palette, the fonts and the drawing
// of the suite's widgets (core/gui: LookAndFeel, Knob, Fader, Switches,
// GainReductionMeter, ResponseCurve; RKMix's MixComponents), ported from
// their paint() code so the iPad console looks like the plugin. Everything
// draws on one canvas through a small component tree with JUCE's
// coordinates (logical pixels, origin top left); touches go to the
// component under each finger, so several faders move at once.
"use strict";

const RK = (() => {

// ---------------------------------------------------------------- colours
class Colour {
  constructor(r, g, b, a = 1) { this.r = r; this.g = g; this.b = b; this.a = a; }
  static hex(argb) {
    if (typeof argb === "string") { const v = parseInt(argb.replace("#", ""), 16); return new Colour((v >> 16) & 255, (v >> 8) & 255, v & 255, 1); }
    return new Colour((argb >> 16) & 255, (argb >> 8) & 255, argb & 255, ((argb >>> 24) & 255) / 255);
  }
  withAlpha(a) { return new Colour(this.r, this.g, this.b, a); }
  withMultipliedAlpha(m) { return new Colour(this.r, this.g, this.b, this.a * m); }
  // juce::Colour::brighter / darker
  brighter(amount = 0.4) { const k = 1 / (1 + amount); return new Colour(255 - k * (255 - this.r), 255 - k * (255 - this.g), 255 - k * (255 - this.b), this.a); }
  darker(amount = 0.4) { const k = 1 / (1 + amount); return new Colour(this.r * k, this.g * k, this.b * k, this.a); }
  interpolatedWith(o, p) {
    return new Colour(this.r + (o.r - this.r) * p, this.g + (o.g - this.g) * p, this.b + (o.b - this.b) * p, this.a + (o.a - this.a) * p);
  }
  perceivedBrightness() {
    const r = this.r / 255, g = this.g / 255, b = this.b / 255;
    return Math.sqrt(r * r * 0.241 + g * g * 0.691 + b * b * 0.068);
  }
  css() { return `rgba(${Math.round(this.r)},${Math.round(this.g)},${Math.round(this.b)},${this.a.toFixed(3)})`; }
}

// rkfx::Palette (core/gui/LookAndFeel.h)
const Palette = {
  background: Colour.hex(0xff1c1d20),
  panel:      Colour.hex(0xff26282c),
  outline:    Colour.hex(0xff3a3d43),
  knobBody:   Colour.hex(0xff34373c),
  knobTrack:  Colour.hex(0xff4a4e55),
  text:       Colour.hex(0xffe8e8ea),
  textDim:    Colour.hex(0xff8d9198),
  warning:    Colour.hex(0xffe0a040),
  utility:    Colour.hex(0xffb4b4b4),
  onAccent(accent) { return accent.perceivedBrightness() > 0.55 ? Colour.hex(0xff15161a) : Colour.hex(0xffffffff); },
};
// RKMix: accent steel, utility family tint (EditorBase::backgroundColour / panelColour)
Palette.accent = Colour.hex(0xff6b8ca6);
Palette.tintedBackground = Palette.background.interpolatedWith(Palette.utility, 0.06);
Palette.tintedPanel = Palette.panel.interpolatedWith(Palette.utility, 0.09);

// MixGui::stripColour
const StripColours = [0xff6e737b, 0xffd4524a, 0xffe8913a, 0xffe3c341, 0xff5cb85c, 0xff45b5c4, 0xff5b8ee0, 0xffb06ad9].map(Colour.hex);
const StripColourNames = ["None", "Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple"];
function stripColour(i) { return StripColours[Math.max(0, Math.min(7, i | 0))]; }

const Style = { cornerRadius: 4, panelRadius: 6, lineWidth: 1 };

// ---------------------------------------------------------------- fonts
// A JUCE font height is ascent + descent; the system font's is 1.193 em.
const kEm = 1.193, kAscent = 0.952 / kEm, kDescent = 0.241 / kEm;
const Family = '-apple-system, "SF Pro Text", "Helvetica Neue", Helvetica, Arial, sans-serif';
function cssFont(h, bold) { return `${bold ? 700 : 400} ${(h / kEm).toFixed(2)}px ${Family}`; }

// juce::Justification flags
const J = { left: 1, right: 2, hcentre: 4, top: 8, bottom: 16, vcentre: 32 };
J.centred = J.hcentre | J.vcentre; J.centredLeft = J.left | J.vcentre; J.centredRight = J.right | J.vcentre;
J.centredTop = J.hcentre | J.top; J.topLeft = J.left | J.top;

// ---------------------------------------------------------------- graphics
// juce::Graphics on a canvas context (logical pixels).
class Graphics {
  constructor(ctx) { this.ctx = ctx; this.fontH = 14; this.bold = false; this.colour = Palette.text; }
  setColour(c) { this.colour = c; const s = c.css(); this.ctx.fillStyle = s; this.ctx.strokeStyle = s; }
  setFont(h, bold = false) { this.fontH = h; this.bold = bold; this.ctx.font = cssFont(h, bold); }
  save() { this.ctx.save(); }
  restore() { this.ctx.restore(); this.ctx.font = cssFont(this.fontH, this.bold); this.setColour(this.colour); }
  fillAll(c) { this.setColour(c); this.ctx.fillRect(-1e4, -1e4, 2e4, 2e4); }
  fillRect(x, y, w, h) { if (w > 0 && h > 0) this.ctx.fillRect(x, y, w, h); }
  roundedPath(x, y, w, h, r) {
    const c = this.ctx; r = Math.max(0, Math.min(r, w / 2, h / 2));
    c.beginPath(); c.moveTo(x + r, y); c.arcTo(x + w, y, x + w, y + h, r); c.arcTo(x + w, y + h, x, y + h, r);
    c.arcTo(x, y + h, x, y, r); c.arcTo(x, y, x + w, y, r); c.closePath();
  }
  fillRoundedRectangle(x, y, w, h, r) { if (w > 0 && h > 0) { this.roundedPath(x, y, w, h, r); this.ctx.fill(); } }
  drawRoundedRectangle(x, y, w, h, r, lw) { if (w > 0 && h > 0) { this.roundedPath(x, y, w, h, r); this.ctx.lineWidth = lw; this.ctx.stroke(); } }
  drawRect(x, y, w, h, lw = 1) { this.ctx.lineWidth = lw; this.ctx.strokeRect(x + lw / 2, y + lw / 2, w - lw, h - lw); }
  fillEllipse(x, y, w, h) { const c = this.ctx; c.beginPath(); c.ellipse(x + w / 2, y + h / 2, w / 2, h / 2, 0, 0, Math.PI * 2); c.fill(); }
  drawEllipse(x, y, w, h, lw) { const c = this.ctx; c.beginPath(); c.ellipse(x + w / 2, y + h / 2, w / 2, h / 2, 0, 0, Math.PI * 2); c.lineWidth = lw; c.stroke(); }
  drawLine(x1, y1, x2, y2, lw = 1, cap = "butt") { const c = this.ctx; c.beginPath(); c.moveTo(x1, y1); c.lineTo(x2, y2); c.lineWidth = lw; c.lineCap = cap; c.stroke(); c.lineCap = "butt"; }
  drawHorizontalLine(y, x1, x2) { this.fillRect(x1, Math.floor(y), x2 - x1, 1); }
  drawVerticalLine(x, y1, y2) { this.fillRect(Math.floor(x), y1, 1, y2 - y1); }
  // A path of points (x, y pairs), stroked rounded as PathStrokeType (w, curved, rounded)
  strokePolyline(pts, lw) {
    if (pts.length < 4) return;
    const c = this.ctx; c.beginPath(); c.moveTo(pts[0], pts[1]);
    for (let i = 2; i < pts.length; i += 2) c.lineTo(pts[i], pts[i + 1]);
    c.lineWidth = lw; c.lineJoin = "round"; c.lineCap = "round"; c.stroke(); c.lineJoin = "miter"; c.lineCap = "butt";
  }
  fillPolygon(pts) {
    const c = this.ctx; c.beginPath(); c.moveTo(pts[0], pts[1]);
    for (let i = 2; i < pts.length; i += 2) c.lineTo(pts[i], pts[i + 1]);
    c.closePath(); c.fill();
  }
  strokeArc(cx, cy, r, from, to, lw) {   // juce angles: clockwise from 12 o'clock
    const c = this.ctx; c.beginPath(); c.arc(cx, cy, r, from - Math.PI / 2, to - Math.PI / 2);
    c.lineWidth = lw; c.lineCap = "round"; c.stroke(); c.lineCap = "butt";
  }
  textWidth(s) { return this.ctx.measureText(s).width; }

  baselineFor(y, h, just) {
    const fh = this.fontH;
    if (just & J.top) return y + fh * kAscent;
    if (just & J.bottom) return y + h - fh * kDescent;
    return y + (h - fh) / 2 + fh * kAscent;
  }
  xFor(x, w, tw, just) {
    if (just & J.right) return x + w - tw;
    if (just & J.hcentre) return x + (w - tw) / 2;
    return x;
  }
  // Graphics::drawText: one line, "..." when it doesn't fit.
  drawText(text, x, y, w, h, just = J.centredLeft, ellipsis = true) {
    text = String(text ?? "");
    let tw = this.textWidth(text);
    if (tw > w && ellipsis) { text = this.truncate(text, w); tw = this.textWidth(text); }
    this.ctx.fillText(text, this.xFor(x, w, tw, just), this.baselineFor(y, h, just));
  }
  truncate(text, w) {
    const dots = "…";
    if (this.textWidth(dots) > w) return "";
    let lo = 0, hi = text.length;
    while (lo < hi) { const mid = (lo + hi + 1) >> 1; if (this.textWidth(text.slice(0, mid) + dots) <= w) lo = mid; else hi = mid - 1; }
    return text.slice(0, lo).trimEnd() + dots;
  }
  // Graphics::drawFittedText: squeezed sideways (to minScale) then wrapped onto
  // up to maxLines lines, then cut short.
  drawFittedText(text, x, y, w, h, just = J.centred, maxLines = 1, minScale = 0.7) {
    text = String(text ?? "");
    if (w <= 0 || h <= 0 || !text) return;
    const tw = this.textWidth(text);
    if (tw <= w && !text.includes("\n")) return this.drawText(text, x, y, w, h, just, false);
    if (maxLines <= 1 || (!text.includes(" ") && !text.includes("\n"))) {
      const scale = Math.max(minScale, w / tw);
      if (tw * scale <= w + 0.5) return this.drawScaled(text, x, y, w, h, just, scale);
      return this.drawText(text, x, y, w, h, just, true);
    }
    // Wrap into lines
    const fh = this.fontH, fitLines = Math.max(1, Math.min(maxLines, Math.floor(h / fh)));
    let lines = this.wrap(text, w);
    let scale = 1;
    if (lines.length > fitLines) { lines = this.wrap(text, w / minScale); scale = minScale; }
    if (lines.length > fitLines) { lines = lines.slice(0, fitLines); lines[fitLines - 1] = this.truncate(lines[fitLines - 1] + " " + "…", w / scale); }
    const blockH = lines.length * fh;
    let top = y;
    if (just & J.bottom) top = y + h - blockH; else if (!(just & J.top)) top = y + (h - blockH) / 2;
    lines.forEach((line, i) => {
      const lw = this.textWidth(line);
      const s = lw > w ? Math.max(minScale, w / lw) : 1;
      this.drawScaled(line, x, top + i * fh, w, fh, (just & (J.left | J.right | J.hcentre)) | J.vcentre, Math.min(s, scale === 1 ? 1 : s));
    });
  }
  drawScaled(text, x, y, w, h, just, scale) {
    if (scale >= 0.999) return this.drawText(text, x, y, w, h, just, true);
    const tw = this.textWidth(text) * scale, c = this.ctx;
    const tx = this.xFor(x, w, tw, just), by = this.baselineFor(y, h, just);
    c.save(); c.translate(tx, by); c.scale(scale, 1); c.fillText(text, 0, 0); c.restore();
  }
  wrap(text, w) {
    const out = [];
    for (const para of text.split("\n")) {
      let line = "";
      for (const word of para.split(" ")) {
        const t = line ? line + " " + word : word;
        if (this.textWidth(t) <= w || !line) line = t; else { out.push(line); line = word; }
      }
      out.push(line);
    }
    return out;
  }
}

// rkfx::drawPanel: a rounded section, optional dim uppercase title.
function drawPanel(g, x, y, w, h, fill, title) {
  g.setColour(fill); g.fillRoundedRectangle(x, y, w, h, Style.panelRadius);
  if (title) { g.setColour(Palette.textDim); g.setFont(12.5, true); g.drawText(title.toUpperCase(), x + 10, y + 6, w - 20, 16, J.centredLeft); }
}

// ---------------------------------------------------------------- components
class Component {
  constructor() { this.x = 0; this.y = 0; this.w = 0; this.h = 0; this.children = []; this.parent = null; this.visible = true; this.enabled = true; }
  setBounds(x, y, w, h) { this.x = x; this.y = y; this.w = Math.max(0, w); this.h = Math.max(0, h); this.resized(); return this; }
  add(c) { c.parent = this; this.children.push(c); return c; }
  remove(c) { const i = this.children.indexOf(c); if (i >= 0) this.children.splice(i, 1); c.parent = null; }
  removeAll() { for (const c of this.children) c.parent = null; this.children = []; }
  setVisible(v) { if (this.visible !== v) { this.visible = v; this.repaint(); } }
  resized() {}
  paint(g) {}
  paintOver(g) {}
  repaint() { app.dirty = true; }
  hitTest(x, y) { return x >= 0 && y >= 0 && x < this.w && y < this.h; }
  /** Absolute position (logical). */
  screenPos() { let x = this.x, y = this.y, p = this.parent; while (p) { x += p.x; y += p.y; p = p.parent; } return { x, y }; }
  // Pointer handlers: pointerDown returns true to take the touch.
  pointerDown(e) { return false; }
  pointerMove(e) {}
  pointerUp(e) {}
  draw(g) {
    if (!this.visible || this.w <= 0 || this.h <= 0) return;
    const c = g.ctx; c.save(); c.translate(this.x, this.y);
    this.paint(g);
    for (const child of this.children) child.draw(g);
    this.paintOver(g);
    c.restore(); g.ctx.font = cssFont(g.fontH, g.bold);
  }
  find(x, y) {   // the deepest visible, enabled component under (x, y) that takes touches
    if (!this.visible || !this.hitTest(x, y)) return null;
    for (let i = this.children.length - 1; i >= 0; --i) {
      const c = this.children[i], hit = c.find(x - c.x, y - c.y);
      if (hit) return hit;
    }
    return this.takesTouches && this.enabled ? this : null;
  }
}

// ---------------------------------------------------------------- the app
const app = {
  dirty: true, scale: 1, width: 1180, height: 820, root: null, overlays: [], frameHooks: new Set(),
  canvas: null, ctx: null, touches: new Map(), lastDown: null,

  start(root, designHeight) {
    this.root = root; this.designHeight = designHeight;
    this.canvas = document.getElementById("screen");
    this.ctx = this.canvas.getContext("2d");
    const resize = () => this.resize();
    window.addEventListener("resize", resize);
    window.addEventListener("orientationchange", () => setTimeout(resize, 200));
    this.resize();
    const c = this.canvas;
    c.addEventListener("pointerdown", e => this.down(e));
    c.addEventListener("pointermove", e => this.move(e));
    c.addEventListener("pointerup", e => this.up(e));
    c.addEventListener("pointercancel", e => this.up(e));
    c.addEventListener("contextmenu", e => e.preventDefault());
    document.addEventListener("gesturestart", e => e.preventDefault());
    let last = performance.now();
    const frame = now => {
      const dt = Math.min(0.2, (now - last) / 1000); last = now;
      for (const hook of this.frameHooks) hook(dt);
      if (this.dirty) { this.dirty = false; this.render(); }
      requestAnimationFrame(frame);
    };
    requestAnimationFrame(frame);
  },
  resize() {
    const w = window.innerWidth, h = window.innerHeight, dpr = window.devicePixelRatio || 1;
    this.scale = h / this.designHeight;
    this.width = w / this.scale; this.height = this.designHeight;
    this.canvas.width = Math.round(w * dpr); this.canvas.height = Math.round(h * dpr);
    this.canvas.style.width = w + "px"; this.canvas.style.height = h + "px";
    this.dpr = dpr;
    this.root.setBounds(0, 0, this.width, this.height);
    for (const o of this.overlays) o.setBounds(0, 0, this.width, this.height);
    this.dirty = true;
  },
  render() {
    const c = this.ctx, s = this.scale * this.dpr;
    c.setTransform(s, 0, 0, s, 0, 0);
    c.textBaseline = "alphabetic";
    const g = new Graphics(c);
    g.fillAll(Palette.tintedBackground);
    this.root.draw(g);
    for (const o of this.overlays) o.draw(g);
  },
  // Overlays (menus, dialogs) lie over everything and take every touch.
  push(o) { this.overlays.push(o); o.setBounds(0, 0, this.width, this.height); this.dirty = true; return o; },
  pop(o) { const i = this.overlays.indexOf(o); if (i >= 0) this.overlays.splice(i, 1); this.dirty = true; },

  local(target, e) {
    const p = target.screenPos();
    return { x: e.clientX / this.scale - p.x, y: e.clientY / this.scale - p.y };
  },
  down(e) {
    e.preventDefault();
    const x = e.clientX / this.scale, y = e.clientY / this.scale;
    const top = this.overlays[this.overlays.length - 1];
    const target = top ? (top.find(x, y) || top) : this.root.find(x, y);
    if (!target) return;
    const now = performance.now();
    const ld = this.lastDown;
    const taps = ld && ld.target === target && now - ld.time < 350 && Math.hypot(ld.x - x, ld.y - y) < 30 ? ld.taps + 1 : 1;
    this.lastDown = { target, time: now, x, y, taps };
    const ev = Object.assign(this.local(target, e), { id: e.pointerId, taps, time: now, startX: x, startY: y });
    if (target.pointerDown(ev)) {
      this.touches.set(e.pointerId, target);
      try { this.canvas.setPointerCapture(e.pointerId); } catch (_) {}
    }
  },
  move(e) {
    const t = this.touches.get(e.pointerId);
    if (!t) return;
    e.preventDefault();
    t.pointerMove(Object.assign(this.local(t, e), { id: e.pointerId, time: performance.now() }));
  },
  up(e) {
    const t = this.touches.get(e.pointerId);
    if (!t) return;
    this.touches.delete(e.pointerId);
    t.pointerUp(Object.assign(this.local(t, e), { id: e.pointerId, cancelled: e.type === "pointercancel" }));
  },
};

// ---------------------------------------------------------------- the store
// Parameter values by ID, as the server sends them: [normalised, text,
// default, steps, choices]. A control moved here is set at once (and sent);
// what the server says about it is ignored while a finger holds it.
const store = {
  values: new Map(), held: new Map(), pending: new Map(), wanted: new Set(), extra: new Set(), sentWanted: "", listeners: new Set(), send: null,
  get(id) { return this.values.get(id); },
  value(id, fallback = 0) { const v = this.values.get(id); return v ? v.v : fallback; },
  text(id) { const v = this.values.get(id); return v ? v.text : ""; },
  on(id) { return this.value(id) >= 0.5; },
  receive(p) {
    const now = performance.now();
    for (const id in p) {
      const d = p[id];
      let v = this.values.get(id);
      if (!v) { v = { v: 0, text: "", def: 0, steps: 0, choices: null }; this.values.set(id, v); }
      const held = this.held.get(id);
      if (!(held && (held.count > 0 || now - held.released < 400))) v.v = d[0];
      v.text = d[1];
      if (d.length > 2) { v.def = d[2]; v.steps = d[3]; if (d[4]) v.choices = d[4]; }
    }
    app.dirty = true;
  },
  snap(id, n) {
    const v = this.values.get(id);
    n = Math.max(0, Math.min(1, n));
    if (v && v.steps > 1) n = Math.round(n * (v.steps - 1)) / (v.steps - 1);
    return n;
  },
  set(id, n) {
    n = this.snap(id, n);
    let v = this.values.get(id);
    if (!v) { v = { v: n, text: "", def: 0, steps: 0, choices: null }; this.values.set(id, v); }
    v.v = n;
    this.pending.set(id, n);
    const h = this.held.get(id); if (h) h.released = performance.now();
    else this.held.set(id, { count: 0, released: performance.now() });
    app.dirty = true;
  },
  hold(id, on) {
    let h = this.held.get(id);
    if (!h) { h = { count: 0, released: 0 }; this.held.set(id, h); }
    h.count = Math.max(0, h.count + (on ? 1 : -1));
    if (!on) h.released = performance.now();
  },
  flush() {
    if (!this.send) return;
    for (const [id, v] of this.pending) this.send({ t: "set", id, v: +v.toFixed(6) });
    this.pending.clear();
  },
  // The IDs the screen shows: sent when the set changes.
  want(id) { if (id) this.wanted.add(id); },
  beginWants() { this.wanted = new Set(); },
  endWants() {
    const list = [...new Set([...this.wanted, ...this.extra])].sort(), key = list.join(",");
    if (key !== this.sentWanted && this.send) { this.sentWanted = key; this.send({ t: "sub", ids: list }); }
  },
  resend() { this.sentWanted = ""; this.endWants(); },
};
app.frameHooks.add(() => store.flush());

// ---------------------------------------------------------------- widgets
// A component bound to a parameter ID.
class ParamComponent extends Component {
  constructor(id) { super(); this.takesTouches = true; this.bind(id); }
  bind(id) { this.id = id; store.want(id); this.repaint(); }
  get norm() { return store.value(this.id); }
  get info() { return store.get(this.id); }
  setNorm(n) { store.set(this.id, n); if (this.onUserChange) this.onUserChange(store.value(this.id)); }
}

// juce::TextButton with rkfx::LookAndFeel
class TextButton extends Component {
  constructor(text, onClick) { super(); this.text = text; this.onClick = onClick; this.toggle = false; this.takesTouches = true; this.down = false; this.colour = Palette.knobBody; this.onColour = Palette.accent; }
  paint(g) {
    let fill = this.toggle ? this.onColour : this.colour;
    if (this.down) fill = fill.brighter(0.15);
    g.setColour(this.enabled ? fill : fill.withMultipliedAlpha(0.6));
    g.fillRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, Style.cornerRadius);
    const fh = Math.min(this.fontHeight || 14, this.h * 0.55);
    g.setFont(fh, !!this.bold);
    const tc = this.toggle ? Palette.onAccent(this.onColour) : Palette.text;
    g.setColour(this.enabled ? tc : tc.withMultipliedAlpha(0.5));
    const yIndent = Math.min(4, this.h * 0.3), left = Math.min(fh, 2 + Style.cornerRadius / 2);
    g.drawFittedText(this.text, left, yIndent, this.w - left * 2, this.h - yIndent * 2, J.centred, 2);
  }
  pointerDown(e) { if (!this.enabled) return false; this.down = true; this.repaint(); if (this.onPress) this.onPress(e); return true; }
  pointerUp(e) {
    this.down = false; this.repaint();
    if (this.onRelease) this.onRelease(e);
    if (!e.cancelled && this.hitTest(e.x, e.y) && this.onClick) this.onClick(e);
  }
}

// rkfx::ToggleSwitch
class ToggleSwitch extends ParamComponent {
  constructor(id, accent, text) { super(id); this.accent = accent; this.text = text; this.fontHeight = 13; }
  paint(g) {
    const on = this.norm >= 0.5, a = this.enabled ? 1 : 0.4;
    g.setColour((on ? this.accent : Palette.knobBody).withMultipliedAlpha(a));
    g.fillRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, 4);
    g.setColour((on ? Palette.onAccent(this.accent) : Palette.text).withMultipliedAlpha(a));
    g.setFont(this.fontHeight, !!this.bold);
    g.drawFittedText(this.text, 4, 0, this.w - 8, this.h, J.centred, 1);
  }
  pointerDown(e) { if (this.locked && this.locked()) return false; this.setNorm(this.norm >= 0.5 ? 0 : 1); if (this.afterClick) this.afterClick(); return true; }
}

// rkfx::SegmentedSwitch
class SegmentedSwitch extends ParamComponent {
  constructor(id, accent, labels, title) { super(id); this.accent = accent; this.labels = labels; this.title = title || ""; }
  segArea() { return this.title ? { x: 0, y: 18, w: this.w, h: this.h - 18 } : { x: 0, y: 0, w: this.w, h: this.h }; }
  paint(g) {
    const labels = this.labels || (this.info && this.info.choices) || [];
    const sel = Math.round(this.norm * Math.max(1, labels.length - 1));
    const a = this.enabled ? 1 : 0.4;
    if (this.title) { g.setColour(Palette.textDim.withMultipliedAlpha(a)); g.setFont(12.5); g.drawFittedText(this.title, 0, 0, this.w, 18, J.centred, 1); }
    const r = this.segArea(), x = r.x + 0.5, y = r.y + 0.5, w = r.w - 1, h = r.h - 1;
    g.setColour(Palette.knobBody.withMultipliedAlpha(a)); g.fillRoundedRectangle(x, y, w, h, 4);
    const sw = w / Math.max(1, labels.length);
    g.setFont(13);
    labels.forEach((label, i) => {
      const sx = x + sw * i;
      if (i === sel) { g.setColour(this.accent.withMultipliedAlpha(a)); g.fillRoundedRectangle(sx + 1, y + 1, sw - 2, h - 2, 3.5); }
      else if (i > 0 && i - 1 !== sel) { g.setColour(Palette.outline.withMultipliedAlpha(a)); g.drawVerticalLine(sx, y + 5, y + h - 5); }
      g.setColour((i === sel ? Palette.onAccent(this.accent) : Palette.text).withMultipliedAlpha(a));
      g.drawFittedText(label, Math.round(sx) + 3, Math.round(y), Math.round(sw) - 6, Math.round(h), J.centred, 1);
    });
  }
  pointerDown(e) {
    const labels = this.labels || (this.info && this.info.choices) || [];
    const r = this.segArea();
    if (!labels.length || e.y < r.y) return false;
    const i = Math.max(0, Math.min(labels.length - 1, Math.floor((e.x - r.x) * labels.length / Math.max(1, r.w))));
    this.setNorm(labels.length > 1 ? i / (labels.length - 1) : 0);
    return true;
  }
}

// rkfx::Knob: drag up and down (200 px for the whole range); double-tap for the default.
const kStart = Math.PI * 1.25, kEnd = Math.PI * 2.75;
class Knob extends ParamComponent {
  constructor(id, label) { super(id); this.label = label; this.dimmed = false; }
  paint(g) {
    const a = this.dimmed || !this.enabled ? 0.4 : 1;
    g.setColour(Palette.textDim.withMultipliedAlpha(a)); g.setFont(12.5);
    g.drawFittedText(this.label, 0, 0, this.w, 18, J.centred, 1);
    const dx = 4, dy = 18 + 2, dw = this.w - 8, dh = this.h - 36 - 4;
    const size = Math.min(dw, dh), cx = dx + dw / 2, cy = dy + dh / 2, r = size / 2;
    g.setColour(Palette.knobBody.withMultipliedAlpha(a)); g.fillEllipse(cx - r + 3, cy - r + 3, size - 6, size - 6);
    const angle = kStart + this.norm * (kEnd - kStart);
    g.setColour(Palette.knobTrack.withMultipliedAlpha(a)); g.strokeArc(cx, cy, r - 1.5, kStart, kEnd, 2);
    g.setColour(Palette.text.withMultipliedAlpha(a));
    g.drawLine(cx + r * 0.25 * Math.sin(angle), cy - r * 0.25 * Math.cos(angle), cx + (r - 6) * Math.sin(angle), cy - (r - 6) * Math.cos(angle), 2.5);
    g.setFont(13); g.drawFittedText(store.text(this.id), 0, this.h - 18, this.w, 18, J.centred, 1);
  }
  pointerDown(e) {
    if (this.locked && this.locked()) return false;
    if (e.taps >= 2 && this.info) { this.setNorm(this.info.def); return true; }
    this.lastY = e.y; this.dragN = this.norm; store.hold(this.id, true); return true;
  }
  pointerMove(e) {
    const delta = (this.lastY - e.y) / 200; this.lastY = e.y;
    this.dragN = Math.max(0, Math.min(1, this.dragN + delta));
    this.setNorm(this.dragN);
  }
  pointerUp() { store.hold(this.id, false); }
}

// The fader law (mix::faderDbAt / faderPositionOf)
const Law = [[0, -100], [0.1, -60], [0.25, -40], [0.45, -20], [0.6, -10], [0.75, 0], [1, 10]];
function faderDbAt(p) { p = Math.min(1, Math.max(0, p)); for (let i = 1; i < Law.length; i++) { const a = Law[i - 1], b = Law[i]; if (p <= b[0]) return a[1] + (b[1] - a[1]) * (p - a[0]) / (b[0] - a[0]); } return 10; }
function faderPositionOf(db) { db = Math.min(10, Math.max(-100, db)); for (let i = 1; i < Law.length; i++) { const a = Law[i - 1], b = Law[i]; if (db <= b[1]) return a[0] + (b[0] - a[0]) * (db - a[1]) / (b[1] - a[1]); } return 1; }

// MixGui::Fader: the console fader, a long track with a dB scale, a cap, the
// value below. Touch anywhere on it and slide: it moves from where it was
// (never jumps to the finger).
class MixFader extends ParamComponent {
  constructor(id) { super(id); this.capColour = Palette.knobTrack; this.showScale = true; }
  resized() { this.valueTop = this.h - 18; this.trackTop = 13 + 2; this.trackBottom = this.valueTop - 13 - 2; }
  yFor(n) { return this.trackBottom - n * (this.trackBottom - this.trackTop); }
  trackRange() { return [this.trackTop, this.trackBottom]; }
  paint(g) {
    const trackX = this.showScale ? this.w * 0.62 : this.w * 0.5;
    if (this.showScale) {
      g.setFont(9.5);
      const bottomY = this.yFor(faderPositionOf(-100));
      let lastY = -100;
      for (const db of [10, 0, -10, -20, -40, -60, -100]) {
        const y = this.yFor(faderPositionOf(db));
        if (db > -100 && (y - lastY < 14 || bottomY - y < 11)) continue;
        lastY = y;
        g.setColour(db === 0 ? Palette.text : Palette.textDim);
        g.drawText(db <= -100 ? "-inf" : String(db), 0, Math.floor(y) - 6, Math.floor(trackX) - 9, 12, J.centredRight, false);
        g.setColour(Palette.outline); g.drawHorizontalLine(y, trackX - 7, trackX - 3);
      }
    }
    const th = this.trackBottom - this.trackTop;
    g.setColour(Palette.knobBody); g.fillRoundedRectangle(trackX - 2, this.trackTop, 4, th, 2);
    const capW = Math.min(28, this.w * (this.showScale ? 0.62 : 0.8)), capH = 26, cy = this.yFor(this.norm);
    const cx = trackX - capW / 2, ctop = cy - capH / 2;
    const cap = this.active ? this.capColour.brighter(0.25) : this.capColour;
    g.setColour(cap.darker(0.2)); g.fillRoundedRectangle(cx, ctop, capW, capH, 3);
    g.setColour(cap.brighter(0.3)); g.drawRoundedRectangle(cx + 0.5, ctop + 0.5, capW - 1, capH - 1, 3, 1);
    g.setColour(Palette.text); g.fillRect(cx + 3, cy - 1, capW - 6, 2);
    const db = faderDbAt(this.norm);
    const text = db <= -99.999 ? "-inf" : (db > 0.05 ? "+" : "") + (Math.abs(db) < 0.05 ? 0 : db).toFixed(1);
    g.setColour(Palette.text); g.setFont(11.5); g.drawText(text, 0, this.valueTop, this.w, 18, J.centred);
  }
  pointerDown(e) {
    if (this.locked && this.locked()) return false;
    this.lastY = e.y; this.dragN = this.norm; this.active = true; store.hold(this.id, true);
    if (this.onGestureStart) this.onGestureStart();
    this.gestureStart = this.norm;
    return true;
  }
  pointerMove(e) {
    const delta = (this.lastY - e.y) / Math.max(150, this.trackBottom - this.trackTop); this.lastY = e.y;
    this.dragN = Math.max(0, Math.min(1, this.dragN + delta));
    this.setNorm(this.dragN);
    if (this.onUserMove) this.onUserMove(this.norm - this.gestureStart);
  }
  pointerUp() { this.active = false; store.hold(this.id, false); this.repaint(); }
}

// MixGui::Meter: one or two bars, -60 to +6 on the fader law's scale, with a hold line.
class Meter extends Component {
  constructor(accent) { super(); this.accent = accent; this.bars = 1; this.shown = [-100, -100]; this.hold = [-100, -100]; this.holdT = [0, 0]; this.range = null; }
  // dB peaks since the last call, @p dt seconds ago (falls back at 24 dB/s, holds 1.5 s)
  setPeaks(l, r, dt) {
    const peaks = [l, r];
    for (let b = 0; b < this.bars; b++) {
      const db = peaks[b];
      const shown = Math.max(db, this.shown[b] - 24 * dt);
      if (db >= this.hold[b]) { this.hold[b] = db; this.holdT[b] = 0; }
      else if ((this.holdT[b] += dt) > 1.5) this.hold[b] = shown;
      if (Math.abs(shown - this.shown[b]) > 0.2) app.dirty = true;
      this.shown[b] = shown;
    }
  }
  paint(g) {
    const [top, bottom] = this.range || [0, this.h];
    const height = bottom - top, bw = this.bars === 2 ? (this.w - 1) / 2 : this.w;
    const yFor = db => bottom - faderPositionOf(Math.max(db, -60)) * height;
    for (let b = 0; b < this.bars; b++) {
      const x = b * (bw + 1);
      g.setColour(Palette.knobBody); g.fillRect(x, top, bw, height);
      const db = this.shown[b];
      if (db > -60) {
        const t = yFor(db), zero = yFor(0);
        g.setColour(this.accent); g.fillRect(x, Math.max(t, zero), bw, bottom - Math.max(t, zero));
        if (t < zero) { g.setColour(Palette.warning); g.fillRect(x, t, bw, zero - t); }
      }
      const hold = this.hold[b];
      if (hold > -60) { g.setColour(hold > 0 ? Palette.warning : Palette.text); g.fillRect(x, yFor(hold) - 1, bw, 2); }
    }
  }
}

// juce::Slider LinearHorizontal with rkfx::LookAndFeel::drawLinearSlider
// (the strips' pans): slide sideways, double-tap for the default.
class LinearSlider extends ParamComponent {
  constructor(id) { super(id); this.trackColour = Palette.accent.withAlpha(0.5); }
  indent() { return Math.min(12, this.h / 2); }
  paint(g) {
    const a = this.enabled ? 1 : 0.4, cy = this.h / 2, ind = this.indent();
    const pos = ind + this.norm * (this.w - 2 * ind);
    g.setColour(Palette.knobTrack.withMultipliedAlpha(a)); g.drawLine(0, cy, this.w, cy, 4);
    g.setColour(this.trackColour.withMultipliedAlpha(a)); g.drawLine(0, cy, pos, cy, 4);
    g.setColour(Palette.text.withMultipliedAlpha(a)); g.fillEllipse(pos - 6, cy - 6, 12, 12);
    if (this.dragging && this.popupText) {   // the value while it moves (setPopupDisplayEnabled)
      const t = this.popupText(); g.setFont(12); const tw = g.textWidth(t) + 12;
      g.setColour(Palette.panel.brighter(0.2)); g.fillRoundedRectangle(pos - tw / 2, -22, tw, 18, 3);
      g.setColour(Palette.text); g.drawText(t, pos - tw / 2, -22, tw, 18, J.centred);
    }
  }
  pointerDown(e) {
    if (this.locked && this.locked()) return false;
    if (e.taps >= 2 && this.info) { this.setNorm(this.info.def); return true; }
    this.lastX = e.x; this.dragN = this.norm; this.dragging = true; store.hold(this.id, true); return true;
  }
  pointerMove(e) {
    const delta = (e.x - this.lastX) / Math.max(60, this.w - 2 * this.indent()); this.lastX = e.x;
    this.dragN = Math.max(0, Math.min(1, this.dragN + delta)); this.setNorm(this.dragN);
  }
  pointerUp() { this.dragging = false; store.hold(this.id, false); this.repaint(); }
}

// rkfx::GainReductionMeter
class GainReductionMeter extends Component {
  constructor(accent, range = 24) { super(); this.accent = accent; this.rangeDb = range; this.shownDb = 0; }
  setReduction(db, dt) {
    const released = Math.min(0, this.shownDb + this.rangeDb * 0.8 * dt);
    const next = Math.max(-this.rangeDb, Math.min(db, released));
    if (Math.abs(next - this.shownDb) > 0.05) { this.shownDb = next; app.dirty = true; }
  }
  paint(g) {
    const scaleW = 20, barX = 2, barY = 16, barW = this.w - scaleW - 4, barH = this.h - 16 - 6;
    g.setColour(Palette.textDim); g.setFont(11);
    const lw = Math.max(this.w - scaleW, 16);
    g.drawText("GR", (this.w - scaleW - lw) / 2, 0, lw, 16, J.centred);
    g.setColour(Palette.background); g.fillRoundedRectangle(barX, barY, barW, barH, 3);
    const depth = Math.max(0, Math.min(1, -this.shownDb / this.rangeDb));
    g.setColour(this.accent); g.fillRoundedRectangle(barX, barY, barW, barH * depth, 3);
    const step = this.rangeDb > 30 ? 12 : 6;
    for (let db = 0; db <= this.rangeDb; db += step) {
      const y = barY + barH * db / this.rangeDb;
      g.setColour(Palette.outline); g.drawHorizontalLine(y, barX, barX + barW);
      g.setColour(Palette.textDim); g.drawText(String(-db), this.w - scaleW, y - 6, scaleW, 12, J.centredRight);
    }
  }
}

// rkfx::ResponseCurve: 20 Hz to 20 kHz, +/- range dB; the curve is a list of dB
// values at log-spaced frequencies.
class ResponseCurve extends Component {
  constructor(range, accent) { super(); this.range = range; this.accent = accent; this.points = null; }
  xFor(hz) { return Math.log(hz / 20) / Math.log(1000) * this.w; }
  yFor(db) { const c = Math.max(-this.range * 1.1, Math.min(this.range * 1.1, db)); return this.h * 0.5 * (1 - c / this.range); }
  paint(g) {
    g.setColour(Palette.background); g.fillRoundedRectangle(0, 0, this.w, this.h, 6);
    g.setFont(11);
    let lastRight = -1e9;
    for (const [hz, major] of [[50, 0], [100, 1], [200, 0], [500, 0], [1000, 1], [2000, 0], [5000, 0], [10000, 1]]) {
      const x = this.xFor(hz);
      g.setColour(Palette.outline.withAlpha(major ? 0.9 : 0.45)); g.drawVerticalLine(x, 0, this.h);
      if (major) {
        const text = hz >= 1000 ? (hz / 1000) + "k" : String(hz), tw = Math.round(g.textWidth(text)) + 1;
        const left = Math.floor(x) + 3 + tw <= this.w - 2 ? Math.floor(x) + 3 : Math.floor(x) - 3 - tw;
        if (left < lastRight + 4) continue;
        lastRight = left + tw;
        g.setColour(Palette.textDim); g.drawText(text, left, this.h - 15, tw, 13, J.left);
      }
    }
    const step = this.range > 12 ? 6 : 3;
    for (let db = -this.range + step; db < this.range; db += step) {
      const y = this.yFor(db);
      g.setColour(Palette.outline.withAlpha(Math.abs(db) < 0.01 ? 1 : 0.45)); g.drawHorizontalLine(y, 0, this.w);
      if (Math.abs(db) > 0.01) { g.setColour(Palette.textDim); g.drawText((db > 0 ? "+" : "") + db, 3, Math.floor(y) - 13, 30, 12, J.left); }
    }
    if (!this.points) return;
    const pts = [], n = this.points.length;
    for (let i = 0; i < n; i++) pts.push(i / (n - 1) * this.w, this.yFor(this.points[i]));
    g.setColour(this.accent.withAlpha(0.15));
    g.fillPolygon(pts.concat([this.w, this.yFor(0), 0, this.yFor(0)]));
    g.setColour(this.accent); g.strokePolyline(pts, 2);
  }
}

// TransferCurve (RKCompress): input -60..0 dB against output, with the live dot.
class TransferCurve extends Component {
  constructor(accent) { super(); this.accent = accent; this.points = null; this.inputDb = -60; }
  pointFor(i, o) { return [(i + 60) / 60 * this.w, this.h - (Math.max(-60, Math.min(0, o)) + 60) / 60 * this.h]; }
  outFor(inDb) { if (!this.points) return inDb; const p = Math.max(0, Math.min(120, (inDb + 60) * 2)), i = Math.floor(p), f = p - i; return i >= 120 ? this.points[120] : this.points[i] * (1 - f) + this.points[i + 1] * f; }
  paint(g) {
    g.setColour(Palette.background); g.fillRoundedRectangle(0, 0, this.w, this.h, 6);
    g.setFont(10.5);
    const every = this.pointFor(-36, -36)[0] - this.pointFor(-48, -48)[0] < 28 ? 24 : 12;
    for (let db = -48; db < 0; db += 12) {
      const [x, y] = this.pointFor(db, db);
      g.setColour(Palette.outline.withAlpha(0.5)); g.drawVerticalLine(x, 0, this.h); g.drawHorizontalLine(y, 0, this.w);
      if ((db + 48) % every !== 0) continue;
      g.setColour(Palette.textDim); g.drawText(String(db), Math.floor(x) + 2, this.h - 14, 30, 12, J.left);
    }
    g.setColour(Palette.outline); const a = this.pointFor(-60, -60), b = this.pointFor(0, 0); g.drawLine(a[0], a[1], b[0], b[1], 1);
    if (this.points) {
      const pts = [];
      for (let i = 0; i <= 120; i++) { const p = this.pointFor(-60 + i * 0.5, this.points[i]); pts.push(p[0], p[1]); }
      g.setColour(this.accent); g.strokePolyline(pts, 2);
    }
    if (this.inputDb > -60) { const [x, y] = this.pointFor(this.inputDb, this.outFor(this.inputDb)); g.setColour(Palette.text); g.fillEllipse(x - 4, y - 4, 8, 8); }
  }
}

// ---------------------------------------------------------------- menus
// juce::PopupMenu with rkfx::LookAndFeel, sized for fingers. Items:
// { text, action, ticked, enabled, colour, separator, header }.
class PopupMenu extends Component {
  constructor(items, anchor, opts = {}) {
    super(); this.items = items; this.anchor = anchor; this.takesTouches = true; this.hot = -1; this.opts = opts;
    this.rowH = 36; this.sepH = 10;
  }
  static show(items, anchor, opts) { const m = new PopupMenu(items, anchor, opts); app.push(m); return m; }
  layout(g) {
    g.setFont(15);
    let w = this.opts.minWidth || 160, h = 8;
    for (const it of this.items) { if (it.separator) h += this.sepH; else { h += this.rowH; w = Math.max(w, g.textWidth(it.text || "") + 56); } }
    w = Math.min(w, this.w - 20);
    const a = this.anchor ? this.anchor.screenPos() : { x: this.w / 2, y: this.h / 2 };
    const aw = this.anchor ? this.anchor.w : 0, ah = this.anchor ? this.anchor.h : 0;
    let x = a.x, y = a.y + ah + 2;
    if (y + h > this.h - 8) y = a.y - h - 2;
    if (y < 8) { y = 8; h = Math.min(h, this.h - 16); }
    x = Math.max(8, Math.min(x, this.w - w - 8));
    if (aw > w) x = a.x;
    this.box = { x, y, w, h };
  }
  rowAt(px, py) {
    const b = this.box; if (!b || px < b.x || px > b.x + b.w) return -1;
    let y = b.y + 4;
    for (let i = 0; i < this.items.length; i++) {
      const it = this.items[i], h = it.separator ? this.sepH : this.rowH;
      if (py >= y && py < y + h) return it.separator || it.header || it.enabled === false ? -1 : i;
      y += h;
    }
    return -1;
  }
  paint(g) {
    g.setColour(Colour.hex(0x55000000)); g.fillRect(0, 0, this.w, this.h);
    this.layout(g);
    const b = this.box;
    g.setColour(Palette.panel); g.fillRoundedRectangle(b.x, b.y, b.w, b.h, Style.panelRadius);
    g.setColour(Palette.outline); g.drawRoundedRectangle(b.x + 0.5, b.y + 0.5, b.w - 1, b.h - 1, Style.panelRadius, 1);
    const c = g.ctx; c.save(); c.beginPath(); c.rect(b.x, b.y, b.w, b.h); c.clip();
    let y = b.y + 4;
    this.items.forEach((it, i) => {
      if (it.separator) { g.setColour(Palette.outline); g.drawHorizontalLine(y + this.sepH / 2, b.x + 10, b.x + b.w - 10); y += this.sepH; return; }
      const hot = i === this.hot;
      if (hot) { g.setColour(Palette.accent); g.fillRoundedRectangle(b.x + 4, y, b.w - 8, this.rowH, 4); }
      const off = it.enabled === false;
      let tc = it.header ? Palette.textDim : hot ? Palette.onAccent(Palette.accent) : it.colour || Palette.text;
      if (off) tc = tc.withMultipliedAlpha(0.4);
      g.setColour(tc); g.setFont(it.header ? 12.5 : 15, !!it.header);
      g.drawText(it.header ? it.text.toUpperCase() : it.text, b.x + 30, y, b.w - 40, this.rowH, J.centredLeft);
      if (it.ticked) {
        g.setColour(hot ? Palette.onAccent(Palette.accent) : Palette.text);
        const tx = b.x + 10, ty = y + this.rowH / 2;
        g.strokePolyline([tx, ty, tx + 4, ty + 5, tx + 12, ty - 5], 2);
      }
      y += this.rowH;
    });
    c.restore();
  }
  pointerDown(e) { this.hot = this.rowAt(e.x, e.y); this.repaint(); if (this.hot < 0 && !this.boxHit(e)) { this.close(); return false; } return true; }
  boxHit(e) { const b = this.box; return b && e.x >= b.x && e.x <= b.x + b.w && e.y >= b.y && e.y <= b.y + b.h; }
  pointerMove(e) { const r = this.rowAt(e.x, e.y); if (r !== this.hot) { this.hot = r; this.repaint(); } }
  pointerUp(e) {
    const r = this.rowAt(e.x, e.y);
    if (r >= 0) { this.close(); const it = this.items[r]; if (it.action) it.action(); }
    else { this.hot = -1; this.repaint(); }
  }
  close() { app.pop(this); if (this.opts.onClose) this.opts.onClose(); }
}

// A combo box on a choice parameter (RKMixEditor::ChoiceBox): its menu lists
// the choices (or what itemsFor gives).
class ChoiceBox extends ParamComponent {
  constructor(id, nameOf) { super(id); this.nameOf = nameOf; }
  choices() { const i = this.info; return i && i.choices ? i.choices : []; }
  index() { const n = this.choices().length; return Math.round(this.norm * Math.max(1, n - 1)); }
  paint(g) {
    const a = this.enabled ? 1 : 0.4;
    g.setColour(Palette.knobBody.withMultipliedAlpha(a)); g.fillRoundedRectangle(0.5, 0.5, this.w - 1, this.h - 1, Style.cornerRadius);
    const cx = this.w - 11, cy = this.h / 2;
    g.setColour(Palette.textDim.withMultipliedAlpha(a)); g.fillPolygon([cx - 4, cy - 2.5, cx + 4, cy - 2.5, cx, cy + 2.5]);
    g.setColour(Palette.text.withMultipliedAlpha(a)); g.setFont(Math.min(14, this.h * 0.6));
    const i = this.index(), text = this.nameOf ? this.nameOf(i) : (this.choices()[i] || store.text(this.id));
    g.drawFittedText(text, 2 + 3, 1, this.w - 22 - 3, this.h - 2, J.centredLeft, 1);
  }
  pointerDown() {
    if (this.locked && this.locked()) return false;
    const n = this.choices().length; if (!n) return false;
    const items = [];
    for (let i = 0; i < n; i++) {
      if (this.offered && !this.offered(i)) continue;
      if (this.separatorBefore === i) items.push({ separator: true });
      items.push({ text: this.nameOf ? this.nameOf(i) : this.choices()[i], ticked: i === this.index(), action: () => this.setNorm(n > 1 ? i / (n - 1) : 0) });
    }
    PopupMenu.show(items, this, { minWidth: this.w });
    return false;
  }
}

// ---------------------------------------------------------------- text entry
// A text field over the canvas (juce::Label's editor), for names.
function editText(target, value, onDone, opts = {}) {
  const input = document.createElement("input");
  const p = target.screenPos(), s = app.scale;
  Object.assign(input.style, {
    position: "fixed", left: (p.x * s) + "px", top: (p.y * s) + "px", width: (Math.max(target.w, 120) * s) + "px",
    height: (Math.max(target.h, 30) * s) + "px", font: cssFont((opts.fontHeight || 14) * s, !!opts.bold), color: Palette.text.css(),
    background: Palette.background.css(), border: `1px solid ${Palette.accent.css()}`, borderRadius: "4px",
    textAlign: opts.left ? "left" : "center", outline: "none", padding: "0 6px", boxSizing: "border-box", zIndex: 10,
  });
  input.value = value; input.maxLength = opts.maxLength || 32;
  if (opts.numeric) input.inputMode = "numeric";
  let done = false;
  const finish = ok => { if (done) return; done = true; input.remove(); if (ok) onDone(input.value); };
  input.addEventListener("keydown", e => { if (e.key === "Enter") finish(true); if (e.key === "Escape") finish(false); });
  input.addEventListener("blur", () => finish(true));
  document.body.appendChild(input);
  input.focus(); input.select();
}

return { Colour, Palette, StripColours, StripColourNames, stripColour, Style, J, cssFont, Graphics, drawPanel, Component, app, store,
         ParamComponent, TextButton, ToggleSwitch, SegmentedSwitch, Knob, MixFader, Meter, LinearSlider, GainReductionMeter,
         ResponseCurve, TransferCurve, PopupMenu, ChoiceBox, editText, faderDbAt, faderPositionOf };
})();
