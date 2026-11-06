#!/usr/bin/env python3
"""Welle's native MPC skin: skins/TUI.json + skins/Q-Links.json + the PNGs they use (the SDK method, docs/SDK-PORTING.md).
MAIN is drawn here (the Wave's LCD, the 8 faders, DISK/PERFORMANCE selectors); the other tabs are generic slider pages
(every parameter reachable) until each gets its own page.
    python3 tools/make_tui.py        # -> skins/   (build-sdk.sh copies it to Plugin Skins/)"""
import hashlib, json, os, re, shutil, sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "skins")
VST_IMG = os.path.join(ROOT, "vst", "images")
FONTS = os.path.join(ROOT, "vst", "fonts")
LOCK = [l.strip() for l in open(os.path.join(ROOT, "vst", "params.lock")) if l.strip()]
PARAMS = json.load(open(os.path.join(ROOT, "vst", "params.json")))
if "ui_open" not in LOCK:
    LOCK.append("ui_open")   # UI-only param appended by the plugin (src/sdk/params_gen.h kUiParam)
UI_STEPS = 97             # ui_open is 0..96: 0 closed, k+1 = pick-list k open


def P(key):
    return "Parameter %d" % LOCK.index(key)


# ---- a JSON writer that keeps key order and allows the overlay widgets' duplicate "version" key ------------------
class Obj:
    """An ordered JSON object that may repeat a key: Obj(("version", 2), ("version", 1), ...)."""
    def __init__(self, *pairs):
        self.pairs = list(pairs)


def dump(v, ind=0):
    sp = "  " * ind
    if hasattr(v, "pairs"):   # Obj from either copy of this module (it is also imported by skin_pages)
        pairs = v.pairs
    elif isinstance(v, dict):
        pairs = list(v.items())
    else:
        pairs = None
    if pairs is not None:
        if not pairs:
            return "{}"
        inner = (",\n").join("%s  %s: %s" % (sp, dump(k), dump(x, ind + 1)) for k, x in pairs)
        return "{\n%s\n%s}" % (inner, sp)
    if isinstance(v, list):
        if not v:
            return "[]"
        return "[\n%s\n%s]" % (",\n".join(sp + "  " + dump(x, ind + 1) for x in v), sp)
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, float):
        return repr(v)
    if isinstance(v, int):
        return str(v)
    s = str(v).replace("\\", "\\\\").replace('"', '\\"')
    return '"%s"' % s


# ---- building blocks -----------------------------------------------------------------------------------------------
def bounds(b, focus="No", visible="Always", invalid="Show", ind=None):
    d = {"version": 2 if ind else 1, "acceptsHWFocus": focus, "showWhenDataModelInvalid": invalid, "whenVisible": visible,
         "boundsType": "Absolute", "bounds": b}
    if ind:
        d["additionalInvalidatingHandles"] = ind
    return d


def entry(name, typ, data, b, remap=None, focus="No", visible="Always", ind=None):
    return {"version": 2,
            "componentData": {"version": 1, "name": name, "type": typ, "data": data},
            "handle remapping": {"version": 1, "map": [{"key": "Data", "value": remap}] if remap else []},
            "bounds": bounds(b, focus, visible, ind=ind)}


def bg(colour="0", image=""):
    return {"version": 1, "focussed": {"version": 1, "colour": colour, "image": image},
            "unfocussed": {"version": 1, "colour": colour, "image": image}}


def component(children, actions=None, ignore=False, background=None):
    return {"version": 2, "actions": actions or [], "backgroundData": background or bg(),
            "ignoreMousePresses": ignore, "disableCoarseDataWheel": False, "componentsData": children}


def act(on, handler, extra=""):
    return {"version": 1, "onAction": on, "handler": handler, "handleName": "Data", "additionalData": extra}


FOCUS = {"version": 1, "backgroundColour": "19ffffff", "outlineColour": "ffe81f40", "backgroundInset": 4.0, "outlineThickness": 1.0}


def text_style(h=22.0, colour="ffbea880", style="SemiBold", just="left verticallyCentred", case="Upper Case", font="Titillium Web"):
    return {"version": 1, "font": {"version": 1, "name": font, "style": style, "height": h}, "colour": colour,
            "justification": just, "case": case}


def label(kind, b, remap=None, text=None, **ts):
    d = {"version": 1, "textStyle": text_style(**ts), "type": kind, "handleName": "Data" if remap else "Invalid"}
    if text is not None:
        d["text"] = text
    return entry("label", "Label", d, b, remap)


def _inherit(e):
    """A label inside a component reads the component's own Data handle."""
    e["componentData"]["data"]["handleName"] = "Data"
    return e


def definitions():
    """Reusable components: generic slider slot, the two overlays."""
    slider_slot = component([
        entry("focus", "Focus", FOCUS, "0 0 278 107", visible="WhenFocussed"),
        entry("slider", "Slider", {"version": 1, "handleName": "Data"}, "15 15 248 77"),
    ], actions=[act("Mouse Down", "Q-Link"), act("Double Click", "Show Overlay", "knob overlay"),
                act("Enter Pressed", "Show Overlay", "knob overlay")])
    knob_overlay = component([entry("overlay", "KnobOverlay", Obj(
        ("version", 2), ("version", 1),
        ("actions", [{"version": 1, "onAction": "Focus Lost", "handler": "Focus Lost", "handleName": "Focus Lost", "additionalData": ""}]),
        ("overlayType", "Knob With Header"), ("scaleType", "Linear"), ("polarityType", "Unipolar"),
        ("showMinusSign", True), ("handleName", "Data")), "0 0 1280 800", focus="Yes")], ignore=True)
    return [{"key": "sliderSlot", "value": slider_slot}, {"key": "knob overlay", "value": knob_overlay}]


def slider_slot_def():
    slot = component([
        entry("focus", "Focus", FOCUS, "0 0 278 107", visible="WhenFocussed"),
        entry("slider", "Slider", {"version": 1, "handleName": "Data"}, "15 15 248 77"),
    ], actions=[act("Mouse Down", "Q-Link"), act("Double Click", "Show Overlay", "knob overlay"),
                act("Enter Pressed", "Show Overlay", "knob overlay")])
    knob_overlay = component([entry("overlay", "KnobOverlay", Obj(
        ("version", 2), ("version", 1),
        ("actions", [{"version": 1, "onAction": "Focus Lost", "handler": "Focus Lost", "handleName": "Focus Lost", "additionalData": ""}]),
        ("overlayType", "Knob With Header"), ("scaleType", "Linear"), ("polarityType", "Unipolar"),
        ("showMinusSign", True), ("handleName", "Data")), "0 0 1280 800", focus="Yes")], ignore=True)
    menu_overlay = component([entry("overlay", "MenuOverlay", Obj(
        ("version", 1), ("version", 1),
        ("actions", [{"version": 1, "onAction": "Focus Lost", "handler": "Focus Lost", "handleName": "Focus Lost", "additionalData": ""}]),
        ("withNumbers", False), ("handleName", "Data")), "0 0 1280 800", focus="Yes")], ignore=True)
    return [{"key": "sliderSlot", "value": slot}, {"key": "knob overlay", "value": knob_overlay}, {"key": "menu overlay", "value": menu_overlay}]


def focus_ring(b):
    return entry("focus", "Focus", FOCUS, b, visible="WhenFocussed")


def fader_def():
    """The MAIN fader: a film-strip Knob dragged vertically, a focus ring, Q-Link on touch, the big overlay on double tap."""
    return {"key": "vfader", "value": component([
        focus_ring("-4 -4 48 208"),
        entry("strip", "Knob", {"version": 3, "knobType": "FilmStrip", "filmStrip": "fader_strip.png", "numFrames": 128, "invert": False,
                                "dragOrientation": "Vertical", "handleName": "Data"}, "0 0 40 200"),
    ], actions=[act("Mouse Down", "Q-Link"), act("Double Click", "Show Overlay", "knob overlay"),
                act("Enter Pressed", "Show Overlay", "knob overlay")])}


def volume_def(key="volume", strip="knob_red.png"):
    return {"key": key, "value": component([
        focus_ring("-6 -6 84 84"),
        entry("knob", "Knob", {"version": 3, "knobType": "FilmStrip", "filmStrip": strip, "numFrames": 101, "invert": False,
                               "dragOrientation": "Vertical", "handleName": "Data"}, "0 0 72 72"),
    ], actions=[act("Mouse Down", "Q-Link"), act("Double Click", "Show Overlay", "knob overlay"),
                act("Enter Pressed", "Show Overlay", "knob overlay")])}


# ---- art ------------------------------------------------------------------------------------------------------------
GESTURE = os.environ.get("WELLE_GESTURE", "Instant")
NAVY, TAN, WHITE, PANEL = (46, 45, 69), (209, 174, 136), (235, 235, 240), (60, 57, 83)
TEAL, TEAL_DK, BEZEL, INK = (38, 137, 160), (8, 30, 36), (30, 40, 46), (36, 35, 45)


def font(size, bold=True):
    return ImageFont.truetype(os.path.join(FONTS, "LiberationSans-Bold.ttf" if bold else "LiberationSans-Regular.ttf"), size)


def save(im, name):
    im.save(os.path.join(OUT, name))
    return name


def text_button(w, h, caption, lit, fill_on=TAN, size=17):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, h - 1), 4, fill=fill_on if lit else PANEL)
    d.text((w // 2, h // 2), caption, font=font(size), fill=INK if lit else WHITE, anchor="mm")
    return im


def arrow_button(w, h, direction, lit):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy, s = w // 2, h // 2, 10
    tip = cx + direction * s
    base = cx - direction * s * 0.6
    d.polygon([(tip, cy), (base, cy - s), (base, cy + s)], fill=TAN if lit else (235, 235, 240))
    return im


def red_button(w, h, caption, lit):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, h - 1), 6, fill=(255, 90, 90) if lit else (218, 46, 46))
    d.text((w // 2, h // 2), caption, font=font(19), fill=WHITE, anchor="mm")
    return im


def blue_button(w, h, caption, lit):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, h - 1), 6, fill=(130, 160, 205) if lit else (96, 126, 167))
    d.text((w // 2, h // 2), caption, font=font(21), fill=WHITE, anchor="mm")
    return im


def fader_strip():
    """128 frames of the Wave's fader cap on its slot (the slot itself is in the page art): 40 x 200 each."""
    cap = Image.open(os.path.join(VST_IMG, "fader_cap.png")).convert("RGBA")
    strip = Image.new("RGBA", (40, 200 * 128), (0, 0, 0, 0))
    for v in range(128):
        cy = 168 - v / 127 * (168 - 30)
        strip.paste(cap, (20 - cap.width // 2, int(cy - cap.height / 2) + 200 * v), cap)
    return save(strip, "fader_strip.png")


def lcd_strip():
    shutil.copy(os.path.join(VST_IMG, "lcd_cell.png"), os.path.join(OUT, "lcd_cell.png"))
    return "lcd_cell.png"


def knob_strip(name="knob_red.png", body=(218, 46, 46), dot=(138, 28, 28)):
    """Cap redrawn centred: 72 px frames, a 66 px disc, the dot sweeping 270 deg (red = Volume/Wavetable/Cutoff as on the Wave, grey = the rest)."""
    import math
    S, N, W, R, DR, DOT = 4, 101, 72, 33, 6.0, 22.0
    out = Image.new("RGBA", (W, W * N), (0, 0, 0, 0))
    for i in range(N):
        f = Image.new("RGBA", (W * S, W * S), (0, 0, 0, 0))
        d = ImageDraw.Draw(f)
        c = W * S / 2
        d.ellipse((c - R * S, c - R * S, c + R * S, c + R * S), fill=body + (255,))
        phi = math.radians(-135 + 2.7 * i)
        dx, dy = c + DOT * S * math.sin(phi), c - DOT * S * math.cos(phi)
        d.ellipse((dx - DR * S, dy - DR * S, dx + DR * S, dy + DR * S), fill=dot + (255,))
        out.paste(f.resize((W, W), Image.LANCZOS), (0, i * W))
    out.save(os.path.join(OUT, name))
    return name


# ---- pages ----------------------------------------------------------------------------------------------------------
class Page:
    def __init__(self, name, fn, sub=0):
        self.name, self.fn, self.sub = name, fn, sub
        self.kids, self.groups, self.qrows, self.choices = [], [], [], {}

    def tab(self, bgfile):
        kids = [entry("bg", "Image", {"version": 2, "imageType": "Regular", "colour": "0", "image": bgfile}, "0 0 1280 628")] + self.kids
        return {"version": 1, "tabName": self.name, "fnKeyIndex": self.fn, "fnKeySubIndex": self.sub,
                "componentDefinition": component(kids, background=bg("ff2e2d45")), "qlinkBoundsData": self.groups}

    def button(self, key, b, on, off, bid=0, n=1, name=None):
        self.kids.append(entry(name or key, "Button", {"version": 2, "onImage": on, "offImage": off, "buttonId": (bid if n > 1 else 1), "numButtonsInGroup": n,
                                                       "handleName": "Data", "gestureBehaviour": GESTURE}, b, remap=P(key), focus="No"))

    def radio(self, key, x, y, w, h, labels, horizontal=False, gap=2):
        for i, lb in enumerate(labels):
            bx, by = (x + i * (w + gap), y) if horizontal else (x, y + i * (h + gap))
            self.button(key, "%d %d %d %d" % (bx, by, w, h), save(text_button(w, h, lb, True), "rb_%s_%d_on.png" % (key, i)),
                        save(text_button(w, h, lb, False), "rb_%s_%d_off.png" % (key, i)), bid=i, n=len(labels), name="%s %d" % (key, i))
        self.choices[key] = labels

    def qgroup(self, rect, keys):
        self.groups.append("%d %d %d %d" % (rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1]))
        self.qrows.append(keys)


def main_page():
    p = Page("MAIN", 0)
    art = Image.open(os.path.join(VST_IMG, "page_MAIN.png")).convert("RGB")
    d = ImageDraw.Draw(art)
    # DISK / PERFORMANCE windows: a dark bezel, a teal window, arrow keys at each end (live keys drawn as buttons)
    for x0, x1 in ((295, 585), (624, 1264)):
        d.rounded_rectangle((x0, 28, x1, 76), 6, fill=BEZEL)
        d.rectangle((x0 + 56, 34, x1 - 56, 70), fill=TEAL)
    # captions for the right-hand panel (the groups of buttons are placed over them)
    # LCD cell backgrounds and text
    strip = lcd_strip()
    for r in range(8):
        for c in range(8):
            x, y = 96 + 116 * c, 120 + 27 * r
            p.kids.append(entry("cell %d.%d" % (r + 1, c + 1), "Image", {"version": 2, "imageType": "FilmStrip", "colour": "0", "image": strip,
                                                                         "numFrames": 32, "handleName": "Data"},
                                "%d %d 116 27" % (x, y), remap=P("lcdst_%d_%d" % (r + 1, c + 1))))
    for r in range(8):
        for c in range(8):
            x, y, w = 102 + 116 * c, 120 + 27 * r, 926 - 116 * c
            for pre, col in (("lcd", "ff081e24"), ("lcdi", "ffc8f2f8")):
                p.kids.append(label("Value", "%d %d %d 27" % (x, y, w), remap=P("%s_%d_%d" % (pre, r + 1, c + 1)),
                                    colour=col, style="SemiBold", case="Original", h=22.0, font="Titillium Web"))
    # DISK / PERFORMANCE: names + prev/next keys
    for key, x0, x1, pk, nk in (("bank_name", 295, 585, "bank_prev", "bank_next"), ("preset_name", 624, 1264, "preset_prev", "preset_next")):
        p.kids.append(label("Value", "%d 34 %d 36" % (x0 + 60, x1 - x0 - 120), remap=P(key), just="horizontallyCentred verticallyCentred",
                            colour="ff081e24", style="Bold", case="Original", h=24.0))
        for k, bx, dr in ((pk, x0, -1), (nk, x1 - 56, 1)):
            p.button(k, "%d 26 56 52" % bx, save(arrow_button(56, 52, dr, True), "%s_on.png" % k), save(arrow_button(56, 52, dr, False), "%s_off.png" % k))
    # faders
    p.kids_defs = [fader_def(), volume_def(), volume_def("gknob", "knob_grey.png"), volume_def("rknob", "knob_red.png")]
    fader_strip()
    for i in range(8):
        x = 154 + 116 * i - 20
        p.kids.append(entry("fader %d" % (i + 1), "vfader", {"version": 1, "handleName": "Data"}, "%d 381 40 200" % x, remap=P("fader_%d" % (i + 1)), focus="Yes"))
    # volume
    knob_strip()
    knob_strip("knob_grey.png", (84, 84, 84), (42, 42, 42))
    p.kids.append(entry("volume", "volume", {"version": 1, "handleName": "Data"}, "1129 134 72 72", remap=P("gain"), focus="Yes"))
    p.kids.append(label("Value", "1066 211 200 22", remap=P("gain"), just="horizontallyCentred verticallyCentred", colour="ffbbc8db", style="Regular",
                        case="Original", h=15.0))
    # VOICES / MIDI / FADERS
    p.radio("voices", 1111, 266, 110, 30, ["6", "8", "12"], gap=2)
    p.radio("midi_mode", 1060, 408, 100, 30, ["ALL", "AS SENT"], gap=2)
    p.radio("fader_mode", 1172, 408, 100, 30, ["QUICK", "PERF"], gap=2)
    # PANIC / RELOAD
    p.button("all_notes_off", "1058 520 104 40", save(red_button(104, 40, "PANIC", True), "panic_on.png"), save(red_button(104, 40, "PANIC", False), "panic_off.png"))
    p.button("init_patch", "1170 520 104 40", save(blue_button(104, 40, "RELOAD", True), "reload_on.png"), save(blue_button(104, 40, "RELOAD", False), "reload_off.png"))
    p.kids.append(label("Value", "1066 584 200 24", remap=P("engine_stats"), just="horizontallyCentred verticallyCentred", colour="ffbbc8db",
                        style="Bold", case="Upper Case", h=12.0))
    # Q-Links: faders 1-4 and 5-8 in the printed slots, then the volume
    p.qgroup((126, 376, 530, 626), ["fader_1", "fader_2", "fader_3", "fader_4"])
    p.qgroup((590, 376, 994, 626), ["fader_5", "fader_6", "fader_7", "fader_8"])
    p.qgroup((1100, 106, 1232, 236), ["gain"])
    return p, art


# ---- pages built from the old vst/layout.conf (same positions as the art) ---------------------------------------
def parse_layout():
    tabs, cur = {}, None
    for line in open(os.path.join(ROOT, "vst", "layout.conf")):
        line = line.strip()
        m = re.match(r"\[tab (.+)\]", line)
        if m:
            cur = tabs.setdefault(m.group(1), {"items": [], "q": [], "art": None})
            continue
        if not cur or not line or line.startswith("#"):
            continue
        if line.startswith("qlinks"):
            cur["q"].append(line.split("=", 1)[1].strip().split(","))
            continue
        kind, rest = line.split(None, 1)
        attrs = {k: v.strip('"') for k, v in re.findall(r'(\w+)=("[^"]*"|\S+)', rest)}
        if kind == "art":
            cur["art"] = attrs["file"]
        else:
            cur["items"].append((kind, attrs))
    return tabs


PICKERS = []   # per-size picker components, filled by layout_tab
OPTIONS = {p["key"]: p.get("options") for p in PARAMS["params"]}
LOCKSET = set(LOCK)


def toggle_images():
    a = Image.new("RGBA", (84, 44), (0, 0, 0, 0)); b = a.copy()
    for im, fill, cap, col in ((a, TAN, "ON", INK), (b, (96, 126, 167), "OFF", WHITE)):
        dd = ImageDraw.Draw(im)
        dd.rounded_rectangle((0, 0, 83, 43), 8, fill=fill)
        dd.text((42, 22), cap, font=font(22), fill=col, anchor="mm")
    return save(a, "tg_on.png"), save(b, "tg_off.png")


PICK_N = 0
PICK_DEFS = {}


def pick_panel(key, n, px, py, fw, fh, opts):
    """A drawn pick-list for the enum `key`: a panel with one radio Button per option (text baked into the images) and a close key,
    visible only while ui_open == n + 1. The plugin clears ui_open when the enum changes (the proven pop-up recipe, NOTES.md)."""
    cols = 1 if len(opts) <= 8 else 5
    rows = (len(opts) + cols - 1) // cols
    cw, ch, gap, hdr = (240 if cols == 1 else 232), 48, 4, 48
    pw, ph = cols * (cw + gap) + gap + 8, hdr + rows * (ch + gap) + gap + 8
    ck = "list_%d_%s" % (len(opts), hashlib.md5("|".join(opts).encode()).hexdigest()[:8])
    if ck not in PICK_DEFS:
        bgim = Image.new("RGBA", (pw, ph), (0, 0, 0, 0))
        dd = ImageDraw.Draw(bgim)
        dd.rounded_rectangle((0, 0, pw - 1, ph - 1), 10, fill=BEZEL, outline=TAN, width=3)
        dd.text((16, hdr // 2 + 2), "SELECT", font=font(18), fill=TAN, anchor="lm")
        bgf = save(bgim, ck + "_bg.png")
        kidsl = [entry("panel", "Image", {"version": 2, "imageType": "Regular", "colour": "0", "image": bgf}, "0 0 %d %d" % (pw, ph))]
        for i, o in enumerate(opts):
            r, c = (i // cols, i % cols) if cols == 1 or True else (0, 0)
            r, c = i % rows, i // rows          # fill down the columns
            on = save(text_button(cw, ch, o, True, size=19), "%s_%d_on.png" % (ck, i))
            off = save(text_button(cw, ch, o, False, size=19), "%s_%d_off.png" % (ck, i))
            kidsl.append(entry("opt %d" % i, "Button", {"version": 2, "onImage": on, "offImage": off, "buttonId": i, "numButtonsInGroup": len(opts),
                                                         "handleName": "Data", "gestureBehaviour": GESTURE},
                               "%d %d %d %d" % (8 + c * (cw + gap), hdr + gap + r * (ch + gap), cw, ch)))
        xon = save(text_button(56, 36, "X", True, size=20), "x_on.png")
        xoff = save(text_button(56, 36, "X", False, size=20), "x_off.png")
        kidsl.append(entry("close", "Button", {"version": 2, "onImage": xon, "offImage": xoff, "buttonId": 0, "numButtonsInGroup": UI_STEPS,
                                               "handleName": "Data", "gestureBehaviour": GESTURE}, "%d 6 56 36" % (pw - 66), remap=P("ui_open")))
        PICKERS.append({"key": ck, "value": component(kidsl, ignore=False)})
        PICK_DEFS[ck] = (pw, ph)
    pw, ph = PICK_DEFS[ck]
    if cols == 1:
        x = max(8, min(px - pw // 2, 1272 - pw))
        y = py + fh // 2 + 4
        if y + ph > 620:
            y = py - fh // 2 - 4 - ph
        y = max(4, y)
    else:
        x, y = (1280 - pw) // 2, max(4, (628 - ph) // 2)
    return entry("list " + key, ck, {"version": 1, "handleName": "Data"}, "%d %d %d %d" % (x, y, pw, ph), remap=P(key), focus="No",
                 ind=["IndexedEnabling/%d/%d/Parameter %d" % (n + 1, UI_STEPS, LOCK.index("ui_open"))])


def layout_tab(name, fn, spec, blocks_by_sub, art_name):
    """One printed page of knobs/toggles/popups at the layout.conf positions; one Page per sub-tab (same art, different Q-Link blocks)."""
    art = Image.open(os.path.join(VST_IMG, os.path.basename(spec["art"]))).convert("RGB")
    d = ImageDraw.Draw(art)
    pos, kids, panels = {}, [], []
    ton, toff = toggle_images()
    save(Image.new("RGBA", (8, 8), (0, 0, 0, 0)), "tile_off.png")
    for kind, a in spec["items"]:
        key = a.get("key")
        if key not in LOCKSET:
            continue
        cx, cy = int(a["cx"]), int(a["cy"]) - 86
        opts = OPTIONS.get(key)
        if kind == "popup" or (kind == "knob" and opts and len(opts) > 2):
            if kind == "popup":
                w, h = int(a["w"]), max(int(a["h"]), 40)
                px, py = cx, cy
            else:   # a Source / Control knob: wipe the printed ring, put a list box there
                w, h = 150, 44
                px, py = cx, cy + 2
                bgc = art.getpixel((min(cx + 80, 1270), py))
                d.rectangle((px - 66, py - 52, px + 66, py + 54), fill=bgc)
            pos[key] = (px, py)
            d.rounded_rectangle((px - w // 2, py - h // 2, px + w // 2, py + h // 2), 6, fill=BEZEL)
            d.rounded_rectangle((px - w // 2 + 3, py - h // 2 + 3, px + w // 2 - 3, py + h // 2 - 3), 4, fill=TEAL)
            ax = px + w // 2 - 18
            d.polygon([(ax - 7, py - 3), (ax + 7, py - 3), (ax, py + 6)], fill=TEAL_DK)
            global PICK_N
            n = PICK_N; PICK_N += 1
            assert n < UI_STEPS - 1, "too many pick-lists"
            # the field: the current choice as text, a transparent button on top that opens this list (ui_open = n + 1)
            kids.append(label("Value", "%d %d %d %d" % (px - w // 2 + 10, py - h // 2, w - 40, h), remap=P(key), just="left verticallyCentred",
                              colour="ff081e24", style="SemiBold", case="Original", h=20.0, font="Titillium Web"))
            kids.append(entry(key + " field", "Button", {"version": 2, "onImage": "tile_off.png", "offImage": "tile_off.png", "buttonId": n + 1,
                                                         "numButtonsInGroup": UI_STEPS, "handleName": "Data", "gestureBehaviour": GESTURE},
                              "%d %d %d %d" % (px - w // 2, py - h // 2, w, h), remap=P("ui_open"), focus="No"))
            panels.append(pick_panel(key, n, px, py, w, h, opts))
            continue
        if kind == "knob":
            pos[key] = (cx, cy + 2)
            comp = "rknob" if "knob_red" in a.get("strip", "") else "gknob"
            kids.append(entry(key, comp, {"version": 1, "handleName": "Data"}, "%d %d 72 72" % (cx - 36, cy + 2 - 36), remap=P(key), focus="Yes"))
            kids.append(label("Value", "%d %d 124 28" % (cx - 62, cy + 2 + 41), remap=P(key), just="horizontallyCentred verticallyCentred",
                              colour="ffd5deea", style="SemiBold", case="Original", h=20.0, font="Titillium Web"))
        elif kind == "toggle":
            pos[key] = (cx, cy + 10)
            kids.append(entry(key, "Button", {"version": 2, "onImage": ton, "offImage": toff, "buttonId": 1, "numButtonsInGroup": 1, "handleName": "Data",
                                              "gestureBehaviour": GESTURE}, "%d %d 84 44" % (cx - 42, cy + 10 - 22), remap=P(key), focus="No"))
    pages = []
    for sub, blocks in enumerate(blocks_by_sub):
        p = Page(name, fn, sub)
        p.kids = list(kids) + list(panels)
        for keys, rect in blocks:
            if rect is None:
                pts = [pos[k] for k in keys]
                rect = (min(x for x, _ in pts) - 72, min(y for _, y in pts) - 82, max(x for x, _ in pts) + 72, max(y for _, y in pts) + 78)
            p.qgroup(rect, keys)
        pages.append(p)
    for k, o in OPTIONS.items():
        pass
    return pages, art


def mod_blocks(spec):
    """MOD pages: amounts, then sources, then controls, as blocks of four columns; four blocks per sub-tab."""
    ks = [(int(a["cx"]), a["key"]) for kind, a in spec["items"] if kind == "knob" and a.get("key") in LOCKSET]
    groups = {"amt": [], "src": [], "ctl": []}
    for cx, k in sorted(ks):
        groups["src" if k.endswith("Source") else "ctl" if k.endswith("Control") else "amt"].append(k)
    blocks = []
    for g in ("amt", "src", "ctl"):
        for i in range(0, len(groups[g]), 4):
            blocks.append((groups[g][i:i + 4], None))
    return [blocks[i:i + 4] for i in range(0, len(blocks), 4)]


def auto_blocks(spec):
    """The old 8-key Q-Link banks, as blocks of four (dashes dropped), four blocks per sub-tab."""
    blocks = []
    for line in spec["q"]:
        for i in range(0, len(line), 4):
            ks = [k for k in line[i:i + 4] if k != "-" and k in LOCKSET]
            if ks:
                blocks.append((ks, None))
    return [blocks[i:i + 4] for i in range(0, len(blocks), 4)] or [[]]


WAVES_BLOCKS = [
    [(["wavetable", "balance", "detune", "noise"], (30, 50, 1042, 195)), (["drive"], (1075, 50, 1255, 195)),
     (["position", "scan", "oscillator1Semitone", "oscillator1Detune"], None), (["wave1Phase", "wave1EnvelopeVelocity", "wave1Keytrack", "wave1Level"], None)],
    [(["position2", "scan2", "oscillator2Semitone", "oscillator2Detune"], None), (["wave2Phase", "wave2EnvelopeVelocity", "wave2Keytrack", "wave2Level"], None)],
]
TAB_ORDER = ["WAVES", "WAVE ENV", "FILTER", "AMP", "LFO", "MOD 1", "MOD 2", "MOD 3", "MOD 4", "FREE ENV", "QUICK"]


def layout_pages(first_fn):
    spec_all = parse_layout()
    pages, arts, fn = [], {}, first_fn
    for name in TAB_ORDER:
        spec = spec_all[name]
        blocks = WAVES_BLOCKS if name == "WAVES" else mod_blocks(spec) if name.startswith("MOD") else auto_blocks(spec)
        ps, art = layout_tab(name, fn, spec, blocks, None)
        fn += 1
        for p in ps:
            for key, o in OPTIONS.items():
                pass
        pages += ps
        arts[name] = art
        for p in ps:
            p.art_key = name
        for kind, a in spec["items"]:
            if kind == "popup" and a.get("key") in LOCKSET and OPTIONS.get(a["key"]):
                for p in ps:
                    p.choices[a["key"]] = OPTIONS[a["key"]]
    return pages, arts


# generic pages (unused now): every other parameter, 16 sliders per page (until each tab gets its own art)
GENERIC = [("WAVES", ["Waves"]), ("WAVE ENV", ["Wave Env"]), ("FILTER", ["Filter"]), ("AMP", ["Amp"]), ("LFO", ["LFO"]),
           ("MOD", ["Mod"]), ("FREE ENV", ["Free Env"]), ("QUICK", ["Quick"])]
SKIP = {"Browse", "LCD", "Banks", "System", "Faders"}


def generic_pages(first_fn):
    secs = {s["label"]: s["keys"] for s in PARAMS["sections"]}
    pages, fn = [], first_fn
    for title, labels in GENERIC:
        keys = [k for lb in labels for k in secs.get(lb, [])]
        n = (len(keys) + 15) // 16
        for pg in range(n):
            p = Page(title if n == 1 else "%s %d" % (title, pg + 1), fn)
            fn += 1
            for i, k in enumerate(keys[pg * 16:(pg + 1) * 16]):
                r, c = divmod(i, 4)
                x, y = 23 + c * 320, 12 + r * 150
                p.kids.append(entry(k, "sliderSlot", {"version": 1, "handleName": "Data"}, "%d %d 278 107" % (x, y), remap=P(k), focus="Yes"))
            for r in range(4):
                p.qgroup((12 + 0, 4 + r * 150, 1268, 4 + r * 150 + 124), keys[pg * 16 + r * 4:pg * 16 + r * 4 + 4])
            pages.append(p)
    return pages


def disks_page(fn):
    """DISKS: ten disk tiles, 32 performance tiles (4 x 8, filled down the columns), pagers, the current performance, +FAV / -FAV.
    Tile names are Value labels UNDER transparent Buttons (a label on top would swallow the tap)."""
    p = Page("DISKS", fn)
    art = Image.open(os.path.join(VST_IMG, "page_DISKS.png")).convert("RGB")
    d = ImageDraw.Draw(art)
    clear = save(Image.new("RGBA", (8, 8), (0, 0, 0, 0)), "tile_off.png")
    lit = Image.new("RGBA", (240, 56), (0, 0, 0, 0))
    ImageDraw.Draw(lit).rounded_rectangle((0, 0, 239, 55), 5, outline=TAN, width=4, fill=(209, 174, 136, 70))
    save(lit, "tile_on_p.png")
    litb = lit.crop((0, 0, 226, 40)).resize((226, 40))
    ib = Image.new("RGBA", (226, 40), (0, 0, 0, 0))
    ImageDraw.Draw(ib).rounded_rectangle((0, 0, 225, 39), 5, outline=TAN, width=4, fill=(209, 174, 136, 70))
    save(ib, "tile_on_b.png")

    def tile(key, x, y, w, h, on):
        d.rounded_rectangle((x, y, x + w - 1, y + h - 1), 5, fill=PANEL, outline=(92, 88, 120), width=1)
        p.kids.append(label("Value", "%d %d %d %d" % (x + 10, y, w - 20, h), remap=P(key), colour="ffebebf0", style="SemiBold", case="Original", h=20.0))
        p.kids.append(entry(key + " tile", "Button", {"version": 2, "onImage": on, "offImage": "tile_off.png", "buttonId": 1, "numButtonsInGroup": 1,
                                                      "handleName": "Data", "gestureBehaviour": GESTURE}, "%d %d %d %d" % (x, y, w, h), remap=P(key), focus="No"))
    for r in range(10):
        tile("bank_slot_%d" % (r + 1), 20, 46 + 46 * r, 226, 40, "tile_on_b.png")
    for c in range(4):
        for r in range(8):
            tile("patch_slot_%d" % (c * 8 + r + 1), 284 + c * 245, 100 + r * 62, 239, 56, "tile_on_p.png")

    def stepper(prev, nxt, text, x, y, w):
        p.kids.append(label("Value", "%d %d %d 40" % (x + 50, y, w - 100), remap=P(text), just="horizontallyCentred verticallyCentred",
                            colour="ffebebf0", style="SemiBold", case="Original", h=20.0))
        for k, bx, dr in ((prev, x, -1), (nxt, x + w - 50, 1)):
            p.button(k, "%d %d 50 40" % (bx, y), save(arrow_button(50, 40, dr, True), "%s_on.png" % k), save(arrow_button(50, 40, dr, False), "%s_off.png" % k))
    d.rounded_rectangle((20, 536, 245, 576), 5, fill=BEZEL)
    stepper("bank_page_prev", "bank_page_next", "bank_page_text", 20, 536, 226)
    d.rounded_rectangle((290, 42, 729, 81), 5, fill=BEZEL)
    d.rectangle((340, 46, 679, 78), fill=TEAL)
    p.kids.append(label("Value", "340 42 340 40", remap=P("preset_name"), just="horizontallyCentred verticallyCentred", colour="ff081e24", style="SemiBold",
                        case="Original", h=20.0))
    for k, bx, dr in (("preset_prev", 290, -1), ("preset_next", 679, 1)):
        p.button(k, "%d 42 50 40" % bx, save(arrow_button(50, 40, dr, True), "%s_on.png" % k), save(arrow_button(50, 40, dr, False), "%s_off.png" % k))
    p.button("fav_save", "742 42 96 40", save(text_button(96, 40, "+FAV", True), "fav_save_on.png"), save(text_button(96, 40, "+FAV", False), "fav_save_off.png"))
    p.button("fav_delete", "848 42 96 40", save(text_button(96, 40, "-FAV", True), "fav_delete_on.png"), save(text_button(96, 40, "-FAV", False), "fav_delete_off.png"))
    p.kids.append(label("Value", "20 586 226 30", remap=P("fav_text"), just="horizontallyCentred verticallyCentred", colour="ffbbc8db", style="Regular",
                        case="Original", h=16.0))
    d.rounded_rectangle((1010, 42, 1259, 81), 5, fill=BEZEL)
    stepper("patch_page_prev", "patch_page_next", "patch_page_text", 1010, 42, 249)
    p.qgroup((284, 36, 1000, 90), ["bank_index", "preset"])
    return p, art


def font_test_page(fn):
    """Temporary: every font and style the MPC can draw, live (bound to the performance name), captions baked."""
    p = Page("FONTS", fn)
    art = Image.new("RGB", (1280, 628), NAVY)
    d = ImageDraw.Draw(art)
    fams = [("Titillium Web", ["Light", "Regular", "SemiBold", "Bold", "Black"]), ("Roboto", ["Thin", "Light", "Regular", "Bold", "Black"]),
            ("Nexa", ["Light", "Regular", "Bold", "Black"])]
    rows = [(f, st) for f, sts in fams for st in sts]
    for i, (f, st) in enumerate(rows):
        col, r = divmod(i, 7)
        x, y = 20 + col * 640, 20 + r * 84
        d.rectangle((x + 170, y, x + 620, y + 70), fill=TEAL)
        d.text((x, y + 35), "%s %s" % (f, st), font=font(17), fill=TAN, anchor="lm")
        p.kids.append(label("Value", "%d %d 440 70" % (x + 176, y), remap=P("preset_name"), colour="ff081e24", style=st, case="Original", h=24.0, font=f))
    return p, art


def qlinks_for(pages):
    maps = []
    for p in pages:
        q = {"Q-Link %d" % n: -1 for n in range(1, 17)}
        for gi, keys in enumerate(p.qrows[:4]):
            first = 13 - 4 * gi
            for i, key in enumerate(keys):
                q["Q-Link %d" % (first + i)] = LOCK.index(key)
        maps.append({"Tab": p.fn + 1, "SubTab": p.sub + 1, "Bank Direction": "RowTopDown", "Q-Links": q})
    prog = {"Q-Link %d" % n: -1 for n in range(1, 17)}
    return {"version": 4, "info": {"version": 1, "type": "CompleteDescription"},
            "Screen Mode Q-Links": {"version": 4, "map": maps}, "Program Mode Q-Links": prog}


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)
    mp, art = main_page()
    dp, dart = disks_page(1)
    lp, larts = layout_pages(2)
    pages = [mp, dp] + lp
    tabs = [mp.tab(save(art, "bg_main.png"))]
    tabs.append(dp.tab(save(dart, "bg_disks.png")))
    bgs = {n: save(a, "bg_%s.png" % n.replace(" ", "_").lower()) for n, a in larts.items()}
    tabs += [p.tab(bgs[p.art_key]) for p in lp]
    if os.environ.get("WELLE_FONTTEST"):
        fp, fart = font_test_page(len(pages))
        pages.append(fp)
        tabs.append(fp.tab(save(fart, "bg_fonts.png")))
    repl, seen = [], set()
    for p in pages:
        for key, labels in p.choices.items():
            if key not in seen:
                seen.add(key)
                repl.append({"key": P(key), "value": labels})
    tui = {"pageData": {"version": 1,
                        "componentDefinitions": {"version": 2, "importFiles": [], "localComponentDefinitions": slider_slot_def() + mp.kids_defs + PICKERS},
                        "replacementStrings": repl, "info": {"version": 1, "type": "CompleteDescription"}, "tabs": tabs}}
    open(os.path.join(OUT, "TUI.json"), "w").write(dump(tui) + "\n")
    open(os.path.join(OUT, "Q-Links.json"), "w").write(dump(qlinks_for(pages)) + "\n")
    print("wrote", OUT, "-", len(tabs), "tabs,", len(os.listdir(OUT)), "files")


if __name__ == "__main__":
    main()
