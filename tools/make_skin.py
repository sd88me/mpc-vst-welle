#!/usr/bin/env python3
"""The MPC layout in the look of Wave Emulation's panel (media/WaldorfWaveUI_NOLOGO.svg, mo0kid, GPL-3.0): dark navy body, tan
section bars with dark italic titles, the light blue-grey centre section, a teal display. Every page is drawn here (PIL) at 1280 x 628.
    make_skin.py      writes vst/layout.conf, vst/images/page_*.png and vst/images/tile.png"""
import json, os, re
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "vst")
FONT = os.path.join(OUT, "fonts")
Y86, W, H = 86, 1280, 628
NAVY, TAN, CENTRE, TEAL, INK, WHITE, RED = (46, 45, 69), (209, 174, 136), (187, 200, 219), (46, 160, 140), (36, 35, 45), (235, 235, 240), (218, 46, 46)
PANEL = (40, 39, 60)
LINE = (187, 200, 219)
RING = Image.open(os.path.join(OUT, "images", "ring.png")).convert("RGBA")
PARTS = os.path.join(ROOT, "skin", "parts")
RED_KNOBS = {"cutoff", "wavetable"}
# section titles cut from the Wave panel (tools/make_assets.py): title -> bar image
BARS = {"FILTER": "FILTER", "FILTER ENVELOPE": "FILTERENV", "AMPLIFIER ENVELOPE": "AMPENV", 
        "GLIDE": "GLIDE", "WAVE 1": "WAVE1", "WAVE 2": "WAVE2", "FREE ENVELOPE": "WAVEFREEENV", "OUTPUT / PANNING": "PANNING"}
KNOB = 84

PARAMS = {p["key"]: p for p in json.load(open(os.path.join(OUT, "params.json")))["params"]}
_f = {}
def F(size, face="LiberationSans-Bold.ttf"):
    if (size, face) not in _f:
        _f[(size, face)] = ImageFont.truetype(os.path.join(FONT, face), size)
    return _f[(size, face)]
TITLE = "LiberationSans-BoldItalic.ttf"
lines = []
emit = lines.append


def label_width(bar):
    """where the title lettering of a cut bar ends: the last dark pixel, plus a margin"""
    px = bar.load()
    last = 0
    for x in range(bar.width):
        for y in range(bar.height):
            if sum(px[x, y]) < 300:
                last = x
                break
    return min(bar.width, last + 12)


def stamp_ring(page, cx, cy, r):
    """the knob's printed scale ring from the Wave panel, sized to a knob of radius r (its body is 67 px across in the cut)"""
    sc = 2 * r / 67.0
    ring = RING.resize((int(RING.width * sc), int(RING.height * sc)), Image.LANCZOS)
    page.im.paste(ring, (int(cx - 59 * sc), int(cy - 61.5 * sc)), ring)


class Page:
    def __init__(self, name, qkeys, *more):
        self.name, self.qkeys, self.more = name, qkeys, more
        self.im = Image.new("RGB", (W, H), NAVY)
        self.d = ImageDraw.Draw(self.im)
        self.widgets = []

    def panel(self, x, y, w, h, title=None):
        """a section: outlined like the original's edit boxes, with its tan title bar"""
        self.d.rounded_rectangle((x, y, x + w, y + h), 6, fill=PANEL)
        if title:
            self.d.rectangle((x, y, x + w, y + 26), fill=TAN)
            if title in BARS:      # the Wave panel's own lettering, at twice its size
                b = Image.open(os.path.join(PARTS, "bar_%s.png" % BARS[title])).convert("RGB")
                lab = b.crop((0, 0, label_width(b), b.height))
                lab = lab.resize((lab.width * 26 // b.height, 26), Image.LANCZOS)
                self.im.paste(lab, (x, y))
            else:
                self.d.text((x + 12, y + 3), title, font=F(19, TITLE), fill=INK)

    def label(self, cx, y, s, size=16):
        self.d.text((cx, y), s, font=F(size), fill=WHITE, anchor="ma")

    def ctl(self, cx, cy, key, label, w=None):
        p = PARAMS[key]
        opts = p.get("options")
        if opts and len(opts) > 12:      # a long list (modulation sources): a knob that names the value
            opts = None
        if opts and len(opts) == 2:
            self.label(cx, cy - 44, label)
            self.widgets.append('toggle cx=%d cy=%d label="" key=%s ns=0' % (cx, cy + Y86, key))
        elif opts:
            self.label(cx, cy - 46, label)
            self.widgets.append('popup cx=%d cy=%d w=%d h=34 label="" key=%s' % (cx, cy + Y86 - 2, w or 150, key))
        else:
            r = KNOB // 2 - 10
            stamp_ring(self, cx, cy, r)
            self.label(cx, cy - r - 36, label)
            strip = " strip=images/knob_red.png" if key in RED_KNOBS else ""
            self.widgets.append('knob cx=%d cy=%d r=%d label="" key=%s ns=0 vs=20 vy=22 bw=%d%s' % (cx, cy + Y86, r, key, KNOB + 30, strip))

    def row(self, x0, x1, y, items, w=None):
        cw = (x1 - x0) / len(items)
        for i, it in enumerate(items):
            if it:
                self.ctl(int(x0 + cw * (i + 0.5)), y, it[0], it[1], w or min(int(cw) - 12, 190))

    def button(self, cx, cy, key, label, w=None):
        self.widgets.append('button cx=%d cy=%d %slabel="%s" key=%s' % (cx, cy + Y86, "w=%d h=40 " % w if w else "", label, key))

    def done(self):
        fn = "images/page_%s.png" % re.sub(r"[^A-Za-z0-9]+", "_", self.name)
        self.im.save(os.path.join(OUT, fn))
        emit("")
        emit("[tab %s]" % self.name)
        emit('art x=0 y=%d w=%d h=%d file="%s"' % (Y86, W, H, fn))
        lines.extend(self.widgets)
        emit('qlinks "%s" = %s' % (self.name, ",".join(self.qkeys)))
        for title, keys in self.more:
            emit('qlinks "%s" = %s' % (title, ",".join(keys)))


emit("""# Wavicle skin layout: generated by tools/make_skin.py (edit that, not this file), in the look of Wave Emulation's panel.
theme_bg=24232d
theme_panel=2e2d45
theme_line=d1ae88
theme_ink=ebebf0
theme_ink_dim=bbc8db
theme_accent=d1ae88
theme_accent_hi=ffffff
theme_knob_face=303030
theme_knob_ring=676767
theme_knob_dot=ffffff
theme_lcd=2ea08c
theme_box=3c3953
theme_seg_active=d1ae88
theme_seg_inactive=3c3953
theme_seg_active_tx=24232d
theme_btn_bg=607ea7
theme_btn_text=ffffff
theme_btn_text_plain=ffffff
theme_display_bg=0e3a33
theme_display_cell=124640
theme_display_off=0c312b
theme_display_bezel=bbc8db
theme_display_ink=7cf2dc
theme_tile_on=607ea7
label_scale=1.25
style=td3
art_css=skin.css
qlink_bounds=column
knob_strip=images/knob_wave.png""")


def wordmark(d, x, y, size):
    d.text((x, y), "WAVICLE", font=F(size, TITLE), fill=TAN)


def header(p):
    wordmark(p.d, 18, 22, 40)
    for x0, t in ((292, "BANK"), (612, "PROGRAM")):
        p.d.text((x0, 8), t, font=F(17, TITLE), fill=TAN)
    p.widgets.append('stepper w=290 h=44 cx=440 cy=%d label="" key=bank_index get=bank_name prev=bank_prev next=bank_next style=dotmatrix persistent=1' % (52 + Y86))
    p.widgets.append('stepper w=640 h=44 cx=944 cy=%d label="" key=preset get=preset_name prev=preset_prev next=preset_next style=dotmatrix persistent=1' % (52 + Y86))


# ---------------------------------------------------------------- MAIN: the Wave's centre section. Its LCD read back as text on the
# instrument's 8-pixel rows and eight columns (src/dsp/wave_lcd.h), each cell's background (bar, box sides) a frame of a filmstrip,
# the eight performance faders in the panel's own fader slots under it
FADERS = ["performanceFader%d" % i for i in range(1, 9)]
p = Page("MAIN", FADERS + ["gain"])
header(p)
LX0, LX1, LY0, LH = 32, 1028, 116, 27
CW = (LX1 - LX0) // 8
p.d.rounded_rectangle((8, 96, 1052, 620), 10, fill=CENTRE)
p.d.rounded_rectangle((LX0 - 14, LY0 - 12, LX1 + 14, LY0 + 8 * LH + 12), 6, fill=(30, 40, 46))
GLASS, DOT, INK_D, BAR = (38, 137, 160), (33, 121, 142), (8, 30, 36), (10, 40, 48)


def cell_frame(st):
    """one LCD cell as the firmware drew its background: glass with its pixel dots, an inverted bar, box sides"""
    im = Image.new("RGB", (CW, LH), BAR if st & 1 else GLASS)
    d = ImageDraw.Draw(im)
    if not st & 1:
        for y in range(1, LH, 3):
            for x in range(1, CW, 3):
                d.rectangle((x, y, x, y), fill=DOT)
    if st & 2: d.rectangle((0, 0, 2, LH), fill=INK_D)
    if st & 4: d.rectangle((CW - 3, 0, CW, LH), fill=INK_D)
    if st & 8: d.rectangle((0, 0, CW, 2), fill=INK_D)
    if st & 16: d.rectangle((0, LH - 3, CW, LH), fill=INK_D)
    return im


strip = Image.new("RGB", (CW, LH * 32))
for st in range(32):
    strip.paste(cell_frame(st), (0, st * LH))
strip.save(os.path.join(OUT, "images", "lcd_cell.png"))
for ln in range(8):
    for c in range(8):
        x = LX0 + c * CW
        p.im.paste(cell_frame(0), (x, LY0 + ln * LH))
        p.widgets.append('meter cx=%d cy=%d w=%d h=%d key=lcdst_%d_%d strip=images/lcd_cell.png frames=32'
                         % (x + CW // 2, LY0 + ln * LH + LH // 2 + Y86, CW, LH, ln + 1, c + 1))
for ln in range(8):
    for c in range(8):
        x = LX0 + c * CW
        w = LX1 - x - 6
        for key, col in (("lcd", "081e24"), ("lcdi", "c8f2f8")):
            p.widgets.append('readout cx=%d cy=%d w=%d h=%d label="" key=%s_%d_%d box=0 tsize=21 tcolor=%s tweight=900 tfont=Roboto talign=left'
                             % (x + 6 + w // 2, LY0 + ln * LH + LH // 2 + Y86, w, LH, key, ln + 1, c + 1, col))
p.widgets.append('readout cx=530 cy=%d w=1000 h=24 label="" key=loading_status box=0 tsize=16 tcolor=24232d tweight=700' % (358 + Y86))
# the performance faders in the panel's fader slots (cut from the Wave Emulation panel), its Slider.svg as the cap
SLOT = Image.open(os.path.join(PARTS, "fader_slot.png")).convert("RGB")
K = 0.58
slot = SLOT.resize((int(SLOT.width * K), int(SLOT.height * K)), Image.LANCZOS)
thumb = Image.open(os.path.join(PARTS, "fader_thumb.png")).convert("RGBA")
thumb = thumb.resize((int(55 * K * 0.92), int(55 * K * 0.92 * thumb.height / thumb.width)), Image.LANCZOS)
thumb.save(os.path.join(OUT, "images", "fader_cap.png"))
Image.new("RGBA", (4, 4), (0, 0, 0, 0)).save(os.path.join(OUT, "images", "clear.png"))
SY = 378
for i, k in enumerate(FADERS):
    cx = LX0 + (2 * i + 1) * CW // 2
    sx = int(cx - 61.5 * K)
    p.im.paste(slot, (sx, SY))
    nx, ny = sx + int(14 * K), SY + int(330 * K)
    p.d.rectangle((nx - 2, ny - 2, nx + 24, ny + 18), fill=(30, 30, 38))
    p.d.text((nx + 11, ny + 1), str(i + 1), font=F(15), fill=WHITE, anchor="ma")
    top, bot = SY + int(54 * K), SY + int(300 * K)
    p.widgets.append('slider_v cx=%d cy=%d w=%d h=%d label="" key=%s img=images/fader_cap.png base=images/clear.png ns=0 vs=0'
                     % (cx, (top + bot) // 2 + Y86, thumb.width, bot - top + thumb.height, k))
# system column
p.d.text((1166, 104), "VOLUME", font=F(17, TITLE), fill=TAN, anchor="ma")
stamp_ring(p, 1166, 180, 32)
p.widgets.append('knob cx=1166 cy=%d r=32 label="" key=gain ns=0 vs=18 vy=20 bw=120' % (180 + Y86))
p.d.text((1166, 262), "VOICES", font=F(17, TITLE), fill=TAN, anchor="ma")
p.widgets.append('enum_v cx=1166 cy=%d label="" key=voices sw=110 sh=36' % (346 + Y86))
p.d.text((1166, 404), "MIDI", font=F(17, TITLE), fill=TAN, anchor="ma")
p.widgets.append('enum_v cx=1166 cy=%d label="" key=midi_mode sw=130 sh=34' % (464 + Y86))
p.widgets.append('button cx=1110 cy=%d w=104 h=40 label="PANIC" key=all_notes_off' % (550 + Y86))
p.widgets.append('button cx=1222 cy=%d w=104 h=40 label="RELOAD" key=init_patch' % (550 + Y86))
p.widgets.append('readout cx=1166 cy=%d w=200 h=26 label="" key=engine_stats box=0 tsize=13 tcolor=bbc8db tweight=700' % (600 + Y86))
p.done()

# ---------------------------------------------------------------- BANKS
p = Page("BANKS", ["preset", "bank_index"])
p.panel(8, 8, 250, 612, "DISKS")
p.panel(270, 8, 1002, 612, "PERFORMANCES")
p.widgets.append('list x=20 y=%d w=226 h=454 cols=1 rows=10 gap=6 th=40 key=bank_slot color=3c3953 tcolor=ebebf0' % (46 + Y86))
p.widgets.append('stepper w=226 h=40 cx=133 cy=%d label="" key=bank_page_text get=bank_page_text prev=bank_page_prev next=bank_page_next style=dotmatrix' % (556 + Y86))
p.widgets.append('stepper w=440 h=40 cx=510 cy=%d label="" key=preset get=preset_name prev=preset_prev next=preset_next style=dotmatrix persistent=1' % (62 + Y86))
p.widgets.append('button cx=790 cy=%d w=96 h=40 label="+FAV" key=fav_save' % (62 + Y86))
p.widgets.append('button cx=896 cy=%d w=96 h=40 label="-FAV" key=fav_delete' % (62 + Y86))
p.widgets.append('readout cx=133 cy=%d w=226 h=30 label="" key=fav_text style=plain' % (606 + Y86))
p.widgets.append('stepper w=240 h=40 cx=1136 cy=%d label="" key=patch_page_text get=patch_page_text prev=patch_page_prev next=patch_page_next style=dotmatrix' % (62 + Y86))
p.widgets.append('list x=284 y=%d w=974 h=500 cols=4 rows=8 gap=6 th=56 key=patch_slot order=cols color=3c3953 tcolor=ebebf0' % (100 + Y86))
p.done()

# ---------------------------------------------------------------- WAVES
p = Page("WAVES", ["wavetable", "position", "scan", "oscillator1Detune", "position2", "scan2", "oscillator2Detune", "balance"])
p.panel(8, 8, 1264, 196, "WAVETABLE / MIX")
p.row(8, 1272, 120, [("wavetable", "Wavetable"), ("oscillatorLink", "Osc 2 Link"), ("balance", "Balance"), ("detune", "Detune"),
                    ("noise", "Noise"), ("drive", "Drive")])
for i, y in ((1, 214), (2, 418)):
    p.panel(8, y, 1264, 196, "WAVE %d" % i)
    sfx = "" if i == 1 else "2"
    p.row(8, 1272, y + 112, [("position" + sfx, "Start Wave"), ("scan" + sfx, "Env Amount"), ("oscillator%dSemitone" % i, "Semitone"),
                             ("oscillator%dDetune" % i, "Detune"), ("wave%dPhase" % i, "Start Phase"), ("wave%dEnvelopeVelocity" % i, "Env Velocity"),
                             ("wave%dKeytrack" % i, "Keytrack"), ("wave%dLevel" % i, "Level")])
p.done()

# ---------------------------------------------------------------- WAVE ENV
TL = ["waveEnvTime%d" % i for i in range(1, 9)]
LV = ["waveEnvLevel%d" % i for i in range(1, 9)]
p = Page("WAVE ENV", TL, ("WAVE ENV LEVELS", LV))
p.panel(8, 8, 1264, 300, "WAVE ENVELOPE: TIMES")
p.row(8, 1272, 170, [(k, "Time %d" % (i + 1)) for i, k in enumerate(TL)] + [("waveEnvKeyOff", "Key Off")])
p.panel(8, 320, 1264, 300, "LEVELS")
p.row(8, 1272, 470, [(k, "Level %d" % (i + 1)) for i, k in enumerate(LV)] + [("waveEnvLoopStart", "Loop Start")])
p.ctl(1202, 580, "waveEnvLoopEnabled", "")
p.d.text((1100, 570), "LOOP", font=F(18, TITLE), fill=TAN)
p.done()

# ---------------------------------------------------------------- FILTER
p = Page("FILTER", ["cutoff", "resonance", "filterEnv", "filterVelocity", "filterAttack", "filterDecay", "filterSustain", "filterRelease"],
         ("HIGH-PASS", ["highpassCutoff", "highpassEnvelopeAmount", "highpassVelocity", "highpassKeytrack", "bandpassBandwidth", "filterKeytrack", "filterDelay", "-"]))
p.panel(8, 8, 1264, 196, "FILTER")
p.row(8, 1272, 120, [("filterMode", "Mode"), ("cutoff", "Cutoff"), ("resonance", "Resonance"), ("filterEnv", "Env Amount"),
                    ("filterVelocity", "Env Velocity"), ("filterKeytrack", "Keytrack"), ("filterKeyCenter", "Key Center")])
p.panel(8, 214, 1264, 196, "FILTER ENVELOPE")
p.row(8, 1272, 326, [("filterDelay", "Delay"), ("filterAttack", "Attack"), ("filterDecay", "Decay"), ("filterSustain", "Sustain"), ("filterRelease", "Release")])
p.panel(8, 418, 1264, 202, "HIGH-PASS / BAND-PASS")
p.row(8, 1272, 532, [("highpassCutoff", "HP Cutoff"), ("highpassEnvelopeSelect", "Envelope"), ("highpassEnvelopeAmount", "Env Amount"),
                    ("highpassVelocity", "Velocity"), ("highpassKeytrack", "Keytrack"), ("highpassKeyCenter", "Key Center"), ("bandpassBandwidth", "BP Width")])
p.done()

# ---------------------------------------------------------------- AMP
p = Page("AMP", ["attack", "decay", "sustain", "release", "output", "pan", "drive", "circuitAge"])
p.panel(8, 8, 1264, 300, "AMPLIFIER ENVELOPE")
p.row(8, 1272, 170, [("attack", "Attack"), ("decay", "Decay"), ("sustain", "Sustain"), ("release", "Release")])
p.panel(8, 320, 1264, 300, "OUTPUT / PANNING")
p.row(8, 1272, 470, [("output", "Output"), ("pan", "Pan"), ("panMode", "Pan Mod Mode"), ("drive", "Drive"), ("circuitAge", "Circuit Age")])
p.done()

# ---------------------------------------------------------------- LFO / GLIDE
p = Page("LFO", ["lfo1Rate", "lfo1Symmetry", "lfo1Humanize", "lfo1Phase", "lfo2Rate", "lfo2Symmetry", "lfo2Humanize", "lfo2Phase"],
         ("GLIDE", ["glideRate", "glideRateModAmount", "-", "-", "-", "-", "-", "-"]))
for i, y in ((1, 8), (2, 214)):
    p.panel(8, y, 1264, 196, "LFO %d" % i)
    p.row(8, 1272, y + 112, [("lfo%dShape" % i, "Shape"), ("lfo%dRate" % i, "Rate"), ("lfo%dSymmetry" % i, "Symmetry"), ("lfo%dHumanize" % i, "Humanize"),
                             ("lfo%dSync" % i, "Sync"), ("lfo%dPhase" % i, "Phase")])
p.panel(8, 418, 1264, 202, "GLIDE")
p.row(8, 1272, 532, [("glideActive", "On / Off"), ("glideType", "Type"), ("glideRate", "Rate"), ("glideTimeMode", "Mode"),
                    ("glideRateModSource", "Rate Mod Src"), ("glideRateModAmount", "Rate Mod Amt")])
p.done()

# ---------------------------------------------------------------- MOD 1-3: the 26 modulation routes, source / control / amount each
ROUTES = [w["key"][:-6] for w in json.load(open(os.path.join(OUT, "params.json")))["params"] if w["key"].endswith("Source") and w["key"] + "" and
          (w["key"][:-6] + "Control") in PARAMS]
NPG = 7
DEST = {"osc1Pitch": "Osc 1 Pitch", "osc2Pitch": "Osc 2 Pitch", "wave1Start": "Wave 1 Start", "wave2Start": "Wave 2 Start",
        "wave1": "Wave 1 Pos", "wave2": "Wave 2 Pos", "wave1Volume": "Wave 1 Level", "wave2Volume": "Wave 2 Level", "noiseVolume": "Noise Level",
        "amplifier": "Amplifier", "filter": "Cutoff", "resonance": "Resonance", "highpass": "HP Cutoff", "pan": "Pan",
        "lfo1Rate": "LFO 1 Rate", "lfo1Level": "LFO 1 Level", "lfo2Rate": "LFO 2 Rate", "lfo2Level": "LFO 2 Level"}


def dest_name(r):
    m = re.match(r"(.*?)Mod(\d?)$", r)
    base, n = (m.group(1), m.group(2)) if m else (r, "")
    return DEST.get(base, base) + (" (%s)" % n if n else "")


for pg in range((len(ROUTES) + NPG - 1) // NPG):
    rs = ROUTES[pg * NPG:(pg + 1) * NPG]
    p = Page("MOD %d" % (pg + 1), [r + "Amount" for r in rs] + ["-"] * (8 - len(rs)))
    p.panel(8, 8, 1264, 612, "MODULATION %d-%d: SOURCE, CONTROL, AMOUNT" % (pg * NPG + 1, pg * NPG + len(rs)))
    cw = 1264 / NPG
    for i, r in enumerate(rs):
        cx = int(8 + cw * (i + 0.5))
        nm = PARAMS[r + "Amount"]["name"].replace(" Amt", "")
        dest = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", r).replace(" Mod", "").replace("Mod", "").strip()
        p.d.text((cx, 40), nm, font=F(17), fill=WHITE, anchor="ma")
        p.d.text((cx, 62), dest.title()[:20], font=F(15), fill=TAN, anchor="ma")
        p.ctl(cx, 180, r + "Source", "Source")
        p.ctl(cx, 350, r + "Control", "Control")
        p.ctl(cx, 520, r + "Amount", "Amount")
    p.done()

# ---------------------------------------------------------------- FREE ENV + filter envelope modulation
FE = ["freeEnvTime1", "freeEnvLevel1", "freeEnvTime2", "freeEnvLevel2", "freeEnvTime3", "freeEnvLevel3", "freeEnvTime4", "freeEnvLevel4"]
p = Page("FREE ENV", FE, ("F ENV MOD", ["filterEnv%sModAmount" % s for s in ("Attack", "Decay", "Sustain", "Release")] + ["-"] * 4))
p.panel(8, 8, 1264, 300, "FREE ENVELOPE")
p.row(8, 1272, 170, [(k, PARAMS[k]["name"]) for k in FE] + [("freeEnvZeroAxis", "Zero Axis")])
p.panel(8, 320, 1264, 300, "FILTER ENVELOPE MODULATION")
st = ["Attack", "Decay", "Sustain", "Release"]
p.row(8, 1272, 470, sum([[("filterEnv%sModSource" % s, s + " Src"), ("filterEnv%sModAmount" % s, s + " Amt")] for s in st], []))
p.done()

# ---------------------------------------------------------------- QUICK / FADERS
QK = [k for k in PARAMS if k.startswith("quick")]
FD = ["performanceFader%d" % i for i in range(1, 9)]
p = Page("QUICK", QK, ("FADERS", FD))
p.panel(8, 8, 1264, 300, "QUICK EDIT (THE WAVE'S OFFSETS)")
p.row(8, 1272, 170, [(k, PARAMS[k]["name"].replace("Quick ", "")) for k in QK])
p.panel(8, 320, 1264, 300, "PERFORMANCE FADERS")
p.row(8, 1272, 470, [(k, "Fader %d" % (i + 1)) for i, k in enumerate(FD)])
p.done()

open(os.path.join(OUT, "layout.conf"), "w").write("\n".join(lines) + "\n")
tile = Image.new("RGB", (270, 110), NAVY)
d = ImageDraw.Draw(tile)
d.rectangle((0, 0, 270, 14), fill=TAN)
d.text((135, 58), "WAVICLE", font=F(44, TITLE), fill=TAN, anchor="mm")
tile.save(os.path.join(OUT, "images", "tile.png"))
print("layout.conf: %d lines" % len(lines))
