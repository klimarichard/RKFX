#!/usr/bin/env python3
"""Writes RKPlay's TouchOSC layouts (plugins/RKPlay/remote/*.tosc).

A .tosc is zlib-compressed XML (TouchOSC's "lexml" version 3). Run:
    python3 tools/touchosc/rkplay_layouts.py

The OSC addresses are RKPlay's (docs/manuals/RKPlay.md, Remote control):
buttons send on press and release (RKPlay acts on the press). A button that
lights up (play, duck, a playlist, a pad, the selected song) is three layers:
a "lamp" that only shows RKPlay's feedback, its label, and a clear button on
top that only sends, so a finger's release never puts the light out.
"""

import os
import uuid
import zlib
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "plugins", "RKPlay", "remote")

# RKPlay's colours
BG = (0.067, 0.067, 0.067, 1.0)
PANEL = (0.133, 0.133, 0.133, 1.0)
KEY = (0.22, 0.22, 0.22, 1.0)          # a plain button
ACCENT = (0.47, 0.75, 0.60, 1.0)       # playing, selected
RED = (0.886, 0.341, 0.298, 1.0)       # Duck, Stop, Panic
AMBER = (0.95, 0.70, 0.30, 1.0)        # End at, the lock
WHITE = (1.0, 1.0, 1.0, 1.0)
DIM = (0.62, 0.62, 0.62, 1.0)
FLASH = (1.0, 1.0, 1.0, 0.18)          # a clear button, pressed
CLEAR = (0.0, 0.0, 0.0, 0.0)

LEFT, CENTRE, RIGHT = 1, 2, 3


# ---------------------------------------------------------------------------
# XML

def prop(kind, key, value):
    if kind == "c":
        r, g, b, a = value
        v = f"<r>{r}</r><g>{g}</g><b>{b}</b><a>{a}</a>"
    elif kind == "r":
        x, y, w, h = value
        v = f"<x>{x}</x><y>{y}</y><w>{w}</w><h>{h}</h>"
    elif kind == "b":
        v = "1" if value else "0"
    elif kind == "s":
        v = escape(str(value))
    else:
        v = str(value)
    return f'<property type="{kind}"><key>{key}</key><value>{v}</value></property>'


def val(key, default):
    return (f"<value><key>{key}</key><locked>0</locked><lockedDefaultCurrent>{1 if key == 'text' else 0}</lockedDefaultCurrent>"
            f"<default>{escape(str(default))}</default><defaultPull>0</defaultPull></value>")


def partial(kind, conversion, value):
    return (f"<partial><type>{kind}</type><conversion>{conversion}</conversion><value>{escape(str(value))}</value>"
            f"<scaleMin>0</scaleMin><scaleMax>1</scaleMax></partial>")


def osc(path, send, receive, var="x", conversion="FLOAT", condition="ANY", args=None):
    if args is None:
        args = [partial("VALUE", conversion, var)]
    return (f"<osc><enabled>1</enabled><send>{int(send)}</send><receive>{int(receive)}</receive><feedback>0</feedback>"
            f"<connections>00001</connections><triggers><trigger><var>{var}</var><condition>{condition}</condition></trigger></triggers>"
            f"<path>{partial('CONSTANT', 'STRING', path)}</path><arguments>{''.join(args)}</arguments></osc>")


def common(name, frame, color, background, interactive, outline=False, outlineStyle=0, grabFocus=False):
    return [prop("b", "background", background), prop("c", "color", color), prop("f", "cornerRadius", 1),
            prop("r", "frame", frame), prop("b", "grabFocus", grabFocus), prop("b", "interactive", interactive),
            prop("b", "locked", False), prop("s", "name", name), prop("i", "orientation", 0),
            prop("b", "outline", outline), prop("i", "outlineStyle", outlineStyle), prop("i", "pointerPriority", 0),
            prop("i", "shape", 1), prop("b", "visible", True)]


def node(kind, props, values, messages=(), children=(), script=None):
    props = list(props)
    if script:
        props.append(prop("s", "script", script))
    props.sort(key=lambda p: p.split("<key>")[1].split("</key>")[0])
    out = f'<node ID="{uuid.uuid4()}" type="{kind}"><properties>{"".join(props)}</properties><values>{"".join(values)}</values>'
    if messages:
        out += f"<messages>{''.join(messages)}</messages>"
    if children:
        out += f"<children>{''.join(children)}</children>"
    return out + "</node>"


# ---------------------------------------------------------------------------
# Controls

def group(name, frame, children, color=CLEAR, script=None, extra=()):
    return node("GROUP", common(name, frame, color, True, False) + list(extra),
                [val("touch", "false")], children=children, script=script)


def label(name, frame, text="", address=None, size=16, color=WHITE, align=LEFT):
    props = common(name, frame, (0, 0, 0, 1), False, False, outlineStyle=1) + [
        prop("i", "font", 0), prop("i", "textAlignH", align), prop("i", "textAlignV", 2), prop("b", "textClip", True),
        prop("c", "textColor", color), prop("i", "textLength", 0), prop("i", "textSize", size)]
    messages = [osc(address, False, True, var="text", conversion="STRING")] if address else []
    return node("LABEL", props, [val("text", text), val("touch", "false")], messages)


def button(name, frame, color, interactive=True, background=True, outline=True, toggle=False, messages=(), script=None):
    props = common(name, frame, color, background, interactive, outline=outline, outlineStyle=1, grabFocus=interactive) + [
        prop("i", "buttonType", 2 if toggle else 0), prop("b", "press", True), prop("b", "release", True),
        prop("b", "valuePosition", False)]
    return node("BUTTON", props, [val("touch", "false"), val("x", 0)], messages, script=script)


def key(name, frame, text, address, color=KEY, size=20, text_color=WHITE, script=None):
    """A plain button: sends 1 on press, 0 on release."""
    return [button(name, frame, color, messages=[osc(address, True, False)], script=script),
            label(name + "_label", frame, text, size=size, color=text_color, align=CENTRE)]


def lit_key(name, frame, text, send, light, color=ACCENT, size=18, text_address=None, align=CENTRE,
            send_args=None, condition="ANY", extra_labels=(), quiet=False):
    """A button that lights from RKPlay's feedback (the lamp), its label, and a clear button on top that sends.
    quiet: unlit, only a plain dark bar (a song row), not the lamp's dim colour."""
    lamp = button(name + "_lamp", frame, color, interactive=False, background=not quiet, outline=not quiet,
                  messages=[osc(light, False, True)])
    if quiet:
        lamp = button(name + "_bar", frame, (0.16, 0.16, 0.16, 1.0), interactive=False, outline=False) + lamp
    pad = 10 if align == LEFT else 0
    x, y, w, h = frame
    text_frame = (x + pad, y, w - 2 * pad, h)
    hit_messages = [osc(send, True, False, condition=condition, args=send_args)]
    hit = button(name, frame, FLASH, background=False, outline=False, messages=hit_messages)
    return [lamp, label(name + "_label", text_frame, text, text_address, size=size, align=align), *extra_labels, hit]


def fader(name, frame, address, color=ACCENT, interactive=True, send=True):
    props = common(name, frame, color, True, interactive, outline=True, outlineStyle=1, grabFocus=interactive) + [
        prop("b", "bar", True), prop("i", "barDisplay", 0), prop("b", "cursor", interactive), prop("i", "cursorDisplay", 0),
        prop("b", "grid", False), prop("i", "gridSteps", 10), prop("i", "response", 0), prop("i", "responseFactor", 100)]
    props = [p for p in props if "<key>orientation</key>" not in p] + [prop("i", "orientation", 1)]   # left to right
    return node("FADER", props, [val("touch", "false"), val("x", 0)], [osc(address, send, True)])


# Scripts

def hello_script(rows):
    # On opening (and with the Sync button): tell RKPlay how many song rows there are, and ask for everything.
    return f"""-- RKPlay remote: the song list's rows, and all the feedback again
function sync()
  sendOSC('/rkplay/songs/rows', {rows})
  sendOSC('/rkplay/hello')
end

function init()
  sync()
end

function onReceiveNotify(what)
  if what == 'sync' then sync() end
end
"""


SYNC_SCRIPT = """function onValueChanged(key)
  if key == 'x' and self.values.x == 1 then root:notify('sync') end
end
"""

LOCK_SCRIPT = """-- Stop and Panic only work while this is on (one press each, then it locks again)
function onValueChanged(key)
  if key == 'x' then
    local on = self.values.x == 1
    for _, name in ipairs({ 'stop', 'panic' }) do
      local c = self.parent:findByName(name, true)
      if c then c.interactive = on end
      local l = self.parent:findByName(name .. '_label', true)
      if l then l.textColor = on and Color(1, 1, 1, 1) or Color(1, 1, 1, 0.3) end
    end
  end
end
"""

RELOCK_SCRIPT = """function onValueChanged(key)
  if key == 'x' and self.values.x == 0 then
    local lock = self.parent:findByName('lock', true)
    if lock then lock.values.x = 0 end
  end
end
"""


def guarded(name, frame, text, address):
    """Stop or Panic: off until the lock is on."""
    b = button(name, frame, RED, interactive=False, messages=[osc(address, True, False)], script=RELOCK_SCRIPT)
    return [b, label(name + "_label", frame, text, size=18, color=(1, 1, 1, 0.3), align=CENTRE)]


def lock_row(x, y, w, h):
    lock_w = int(w * 0.28)
    rest = (w - lock_w - 16) // 2
    return [button("lock", (x, y, lock_w, h), AMBER, toggle=True, script=LOCK_SCRIPT),
            label("lock_label", (x, y, lock_w, h), "UNLOCK", size=14, align=CENTRE),
            *guarded("stop", (x + lock_w + 8, y, rest, h), "STOP", "/rkplay/stop"),
            *guarded("panic", (x + lock_w + 16 + rest, y, rest, h), "PANIC", "/rkplay/panic")]


# ---------------------------------------------------------------------------
# Building blocks shared by the phone and the tablet

def now_playing(x, y, w):
    """State, playlist, artist, title, times, progress, next: 210 high."""
    return [
        label("state", (x, y, w // 2, 24), "Stopped", "/rkplay/state", size=15, color=DIM),
        label("playlist_name", (x + w // 2, y, w - w // 2, 24), "", "/rkplay/playlist/name", size=15, color=ACCENT, align=RIGHT),
        label("artist", (x, y + 30, w, 30), "", "/rkplay/artist", size=20, color=DIM),
        label("title", (x, y + 62, w, 44), "RKPlay", "/rkplay/title", size=28),
        label("elapsed", (x, y + 114, 120, 30), "0:00", "/rkplay/elapsed", size=22),
        label("remaining", (x + w - 120, y + 114, 120, 30), "", "/rkplay/remaining", size=22, color=ACCENT, align=RIGHT),
        fader("progress", (x, y + 150, w, 12), "/rkplay/progress", interactive=False, send=False),
        label("next_title", (x, y + 172, 50, 28), "NEXT", size=13, color=DIM),
        label("next", (x + 52, y + 172, w - 52, 28), "", "/rkplay/next", size=16, color=DIM),
    ]


def transport(x, y, w, h_main=110, h_second=80):
    third = (w - 16) // 4
    mid = w - 2 * third - 16
    half = (w - 8) // 2
    y2 = y + h_main + 10
    return [
        *key("prev", (x, y, third, h_main), "|<", "/rkplay/prev", size=26),
        *lit_key("playpause", (x + third + 8, y, mid, h_main), "PLAY / PAUSE", "/rkplay/playpause", "/rkplay/play", size=20),
        *key("next_song", (x + third + mid + 16, y, third, h_main), ">|", "/rkplay/next", size=26),
        *key("fade", (x, y2, half, h_second), "FADE OUT", "/rkplay/fade", size=18),
        # Duck: no argument toggles it (a value would mean "on while held").
        *lit_key("duck", (x + half + 8, y2, w - half - 8, h_second), "DUCK", "/rkplay/duck", "/rkplay/duck", color=RED, size=18,
                 send_args=[], condition="RISE"),
    ]


def level(x, y, w):
    return [
        label("level_title", (x, y, 120, 24), "LEVEL", size=13, color=DIM),
        label("level_db", (x + w - 140, y, 140, 24), "", "/rkplay/level/db", size=15, align=RIGHT),
        fader("level", (x, y + 28, w, 56), "/rkplay/level"),
    ]


def requests(x, y, w, rows):
    out = [label("queue_title", (x, y, 120, 24), "REQUESTS", size=13, color=DIM),
           label("queue_count", (x + w - 160, y, 160, 24), "", "/rkplay/queue/count", size=13, color=DIM, align=RIGHT)]
    for i in range(rows):
        out.append(label(f"queue_{i + 1}", (x, y + 28 + i * 26, w, 26), "", f"/rkplay/queue/{i + 1}", size=15))
    return out


def song_list(x, y, w, rows, row_h, gap=4):
    """The playing playlist, a page at a time; a row selects (it doesn't play)."""
    out = [label("songs_list_name", (x, y, w - 130, 30), "", "/rkplay/playlist/name", size=17, color=ACCENT),
           label("songs_page", (x + w - 130, y, 130, 30), "", "/rkplay/songs/page", size=14, color=DIM, align=RIGHT)]
    top = y + 36
    for i in range(rows):
        n = i + 1
        ry = top + i * (row_h + gap)
        time_w = 60
        out += lit_key(f"song_{n}", (x, ry, w, row_h), "", f"/rkplay/song/{n}", f"/rkplay/song/{n}",
                       size=15 if row_h < 40 else 16, text_address=f"/rkplay/song/{n}/name", align=LEFT, quiet=True,
                       extra_labels=[label(f"song_{n}_time", (x + w - time_w - 10, ry, time_w, row_h), "",
                                           f"/rkplay/song/{n}/time", size=13, color=DIM, align=RIGHT)])
    return out, top + rows * (row_h + gap)


def song_controls(x, y, w, nav_h=56, action_h=84):
    third = (w - 16) // 3
    half = (w - 8) // 2
    ys = y + nav_h + 8
    ya = ys + 34
    return [
        *key("songs_up", (x, y, third, nav_h), "PAGE UP", "/rkplay/songs/up", size=15),
        *key("songs_playing", (x + third + 8, y, w - 2 * third - 16, nav_h), "PLAYING", "/rkplay/songs/playing", size=15,
             text_color=ACCENT),
        *key("songs_down", (x + w - third, y, third, nav_h), "PAGE DOWN", "/rkplay/songs/down", size=15),
        label("selected_title", (x, ys, 90, 30), "SELECTED", size=13, color=DIM),
        label("selected", (x + 92, ys, w - 92, 30), "Tap a song", "/rkplay/selected", size=16),
        *key("selected_next", (x, ya, half, action_h), "PLAY NEXT", "/rkplay/selected/next", size=19),
        *key("selected_now", (x + half + 8, ya, w - half - 8, action_h), "PLAY NOW", "/rkplay/selected/now", size=19,
             text_color=ACCENT),
    ], ya + action_h


def playlists(x, y, w, cols, h, after_h, title_now="NOW", after_cols=4):
    out = [label("playlists_title", (x, y, w, 24), title_now, size=13, color=DIM)]
    cw = (w - (cols - 1) * 8) // cols
    top = y + 28
    for i in range(8):
        n = i + 1
        cx = x + (i % cols) * (cw + 8)
        cy = top + (i // cols) * (h + 8)
        out += lit_key(f"playlist_{n}", (cx, cy, cw, h), "", f"/rkplay/playlist/{n}", f"/rkplay/playlist/{n}", size=16,
                       text_address=f"/rkplay/playlist/{n}/name")
    ay = top + (8 // cols) * (h + 8) + 8
    out.append(label("after_title", (x, ay, w, 24), "AFTER THIS SONG", size=13, color=DIM))
    aw = (w - (after_cols - 1) * 8) // after_cols
    for i in range(8):
        n = i + 1
        cx = x + (i % after_cols) * (aw + 8)
        cy = ay + 28 + (i // after_cols) * (after_h + 8)
        out += [button(f"after_{n}", (cx, cy, aw, after_h), KEY,
                       messages=[osc("/rkplay/playlist/after", True, False, condition="RISE",
                                     args=[partial("CONSTANT", "INTEGER", n)])]),
                label(f"after_{n}_label", (cx + 4, cy, aw - 8, after_h), "", f"/rkplay/playlist/{n}/name", size=12, align=CENTRE)]
    return out, ay + 28 + (8 // after_cols) * (after_h + 8)


def pads(x, y, w, cols, h):
    out = [label("pads_title", (x, y, w, 24), "PADS", size=13, color=DIM)]
    cw = (w - (cols - 1) * 8) // cols
    for i in range(8):
        n = i + 1
        cx = x + (i % cols) * (cw + 8)
        cy = y + 28 + (i // cols) * (h + 8)
        out += lit_key(f"pad_{n}", (cx, cy, cw, h), f"Pad {n}", f"/rkplay/pad/{n}", f"/rkplay/pad/{n}", color=AMBER,
                       size=16, text_address=f"/rkplay/pad/{n}/name")
    return out, y + 28 + (8 // cols) * (h + 8)


def end_at(x, y, w):
    return [label("endat_title", (x, y, 80, 28), "END AT", size=13, color=DIM),
            label("endat", (x + 80, y, w - 80, 28), "", "/rkplay/endat", size=16, color=AMBER)]


def sync_key(frame):
    return key("sync", frame, "SYNC", "/rkplay/hello", size=13, text_color=DIM, script=SYNC_SCRIPT)


def document(root):
    return "<?xml version='1.0' encoding='UTF-8'?>\n" + f'<lexml version="3">{root}</lexml>'


def write(name, xml):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name)
    with open(path, "wb") as f:
        f.write(zlib.compress(xml.encode("utf-8")))
    print("wrote", os.path.normpath(path))


# ---------------------------------------------------------------------------
# The phone: 390 x 844, four pages (Play, Songs, Lists, Pads)

def phone():
    W, H, TAB = 390, 844, 56
    PH = H - TAB
    M = 12
    w = W - 2 * M

    def page(name, tab, children):
        extra = [prop("c", "tabColorOff", (0.2, 0.2, 0.2, 1.0)), prop("c", "tabColorOn", ACCENT), prop("s", "tabLabel", tab),
                 prop("c", "textColorOff", WHITE), prop("c", "textColorOn", (0, 0, 0, 1))]
        return group(name, (0, TAB, W, PH), children, extra=extra)

    play = [*now_playing(M, 10, w), *transport(M, 226, w), *level(M, 436, w), *requests(M, 528, w, 3),
            *end_at(M, 644, w - 80), *sync_key((W - M - 70, 644, 70, 28)), *lock_row(M, 690, w, 80)]
    songs, bottom = song_list(M, 8, w, 10, 48)
    controls, _ = song_controls(M, bottom + 4, w)
    lists, _ = playlists(M, 8, w, 2, 74, 56)
    pad_page, pads_bottom = pads(M, 8, w, 2, 120)

    pager_props = common("pager", (0, 0, W, H), PANEL, True, True, outline=True) + [
        prop("b", "tabLabels", True), prop("b", "tabbar", True), prop("b", "tabbarDoubleTap", False),
        prop("i", "tabbarSize", TAB), prop("i", "textSizeOff", 16), prop("i", "textSizeOn", 16)]
    pager = node("PAGER", pager_props, [val("page", 0), val("touch", "false")], children=[
        page("play", "PLAY", play),
        page("songs", "SONGS", songs + controls),
        page("lists", "LISTS", lists),
        page("pads", "PADS", pad_page + end_at(M, pads_bottom + 12, w)),
    ])
    root = group("RKPlay iPhone", (0, 0, W, H), [pager], color=BG, script=hello_script(10))
    return document(root)


# ---------------------------------------------------------------------------
# The tablet: 1180 x 820 (iPad, landscape), everything on one screen

def tablet():
    W, H = 1180, 820
    M = 14
    left_w, mid_w = 380, 420
    mid_x = M + left_w + 18
    right_x = mid_x + mid_w + 18
    right_w = W - right_x - M

    left = [*now_playing(M, 12, left_w), *transport(M, 228, left_w, 104, 76), *level(M, 432, left_w),
            *requests(M, 528, left_w, 4), *end_at(M, 670, left_w - 80), *sync_key((M + left_w - 70, 670, 70, 28)),
            *lock_row(M, 722, left_w, 80)]
    songs, bottom = song_list(mid_x, 12, mid_w, 20, 27, gap=3)
    controls, _ = song_controls(mid_x, bottom + 4, mid_w, nav_h=44, action_h=64)
    lists, lists_bottom = playlists(right_x, 12, right_w, 2, 50, 40)
    pad_page, _ = pads(right_x, lists_bottom + 10, right_w, 2, 70)

    panels = [group("left_panel", (M - 6, 4, left_w + 12, H - 8), [], color=PANEL),
              group("mid_panel", (mid_x - 6, 4, mid_w + 12, H - 8), [], color=PANEL),
              group("right_panel", (right_x - 6, 4, right_w + 12, H - 8), [], color=PANEL)]
    root = group("RKPlay iPad", (0, 0, W, H), panels + left + songs + controls + lists + pad_page,
                 color=BG, script=hello_script(20))
    return document(root)


if __name__ == "__main__":
    write("RKPlay iPhone.tosc", phone())
    write("RKPlay iPad.tosc", tablet())
