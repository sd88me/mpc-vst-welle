#!/usr/bin/env python3
"""The WELLE wordmark, drawn in the manner of the Wave's own: a monoline hand-drawn script on the navy panel, the W and E in
the panel's pale blue-grey, the double L as one tan swoosh with a pale stroke run through it (as the Wave's A/V and crossbar).
Strokes are Catmull-Rom curves through hand-placed points, drawn at 4x and scaled down.
    make_wordmark.py      writes skin/parts/wordmark.png (transparent, 4x) and vst/images/wordmark.png (for the pages)"""
import os
from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PALE, TAN = (187, 200, 219, 255), (209, 174, 136, 255)
S = 8                       # supersampling
W, H = 200, 70              # design units (the strokes below)


def spline(pts, n=48):
    """Catmull-Rom through the points (ends repeated)"""
    p = [pts[0]] + pts + [pts[-1]]
    out = []
    for i in range(1, len(p) - 2):
        p0, p1, p2, p3 = p[i - 1], p[i], p[i + 1], p[i + 2]
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * (2 * p1[j] + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t2
                                    + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t3) for j in (0, 1)))
    out.append(pts[-1])
    return out


def stroke(d, pts, col, w=3.2):
    q = [(x * S, y * S) for x, y in spline(pts)]
    d.line(q, fill=col, width=int(w * S), joint="curve")
    r = w * S / 2
    for x, y in (q[0], q[-1]):
        d.ellipse((x - r, y - r, x + r, y + r), fill=col)


SLANT = 0.22                # forward lean: x += SLANT * (baseline - y)
BASE = 60


def lean(pts):
    return [(x + SLANT * (BASE - y), y) for x, y in pts]


im = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
LW = 2.4
# W: two smooth Vs, rounded at the foot
stroke(d, lean([(6, 16), (9, 40), (15, 59), (21, 46), (27, 30), (33, 46), (39, 59), (46, 40), (52, 14)]), PALE, LW)
# E: a small top bowl into a fuller lower one
stroke(d, lean([(86, 18), (78, 14), (70, 18), (70, 29), (79, 36), (68, 40), (65, 52), (73, 59), (84, 58), (91, 53)]), PALE, LW)
# LL: one tan swoosh, two tall looped l's
stroke(d, lean([(100, 58), (108, 46), (116, 26), (120, 12), (116, 9), (112, 20), (110, 42), (112, 56), (118, 59), (126, 48),
                (134, 26), (138, 12), (134, 9), (130, 20), (128, 42), (130, 56), (136, 59), (144, 54)]), TAN, LW)
# the pale stroke through the swoosh, as the Wave's crossbar
stroke(d, lean([(102, 38), (122, 36.5), (144, 35)]), PALE, LW * 0.85)
# E
stroke(d, lean([(176, 18), (168, 14), (160, 18), (160, 29), (169, 36), (158, 40), (155, 52), (163, 59), (174, 58), (181, 53)]), PALE, LW)

im = im.crop(im.getbbox())
os.makedirs(os.path.join(ROOT, "skin", "parts"), exist_ok=True)
im.save(os.path.join(ROOT, "skin", "parts", "wordmark.png"))
small = im.resize((im.width // S * 1, im.height // S * 1), Image.LANCZOS)
small.save(os.path.join(ROOT, "vst", "images", "wordmark.png"))
print("wordmark", small.size)
