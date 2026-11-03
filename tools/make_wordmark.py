#!/usr/bin/env python3
"""The WELLE wordmark, drawn in the manner of the Wave's own: a monoline hand-drawn script on the navy panel, the W and E in
the panel's pale blue-grey, the double L as one tan swoosh with a pale stroke run through it (as the Wave's A/V and crossbar).
Strokes are Catmull-Rom curves through hand-placed points, drawn at 4x and scaled down.
    make_wordmark.py      writes skin/parts/wordmark.png (transparent, 4x) and vst/images/wordmark.png (for the pages)"""
import os
from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PALE, TAN = (187, 200, 219, 255), (209, 174, 136, 255)
S = 4                       # supersampling
W, H = 260, 70              # design units (the strokes below)


def spline(pts, n=24):
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


im = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
# W: a loose script W, its first stroke rising from a curl at the left (as the Wave's)
stroke(d, [(8, 52), (12, 30), (22, 12), (24, 18), (20, 40), (24, 58), (34, 44), (42, 26), (44, 40), (50, 58), (58, 44), (66, 22), (72, 10)], PALE)
# E: open, three strokes in one, like the Wave's E
stroke(d, [(106, 14), (92, 12), (84, 22), (86, 34), (98, 35), (86, 38), (80, 50), (86, 59), (104, 58), (112, 54)], PALE)
# LL: one tan swoosh, two tall loops sweeping up and right (the Wave's A/V)
stroke(d, [(116, 58), (126, 44), (134, 22), (138, 8), (134, 12), (130, 34), (132, 56), (142, 56), (154, 40), (162, 18), (166, 6),
           (162, 10), (158, 34), (160, 56), (172, 57), (182, 52)], TAN)
# the pale stroke run through the swoosh (the Wave's crossbar)
stroke(d, [(118, 38), (140, 37), (164, 35), (186, 34)], PALE, 2.6)
# E
stroke(d, [(232, 16), (216, 12), (206, 22), (209, 34), (222, 35), (209, 38), (202, 50), (210, 59), (230, 58), (240, 53)], PALE)

im = im.crop(im.getbbox())
os.makedirs(os.path.join(ROOT, "skin", "parts"), exist_ok=True)
im.save(os.path.join(ROOT, "skin", "parts", "wordmark.png"))
small = im.resize((im.width // S * 1, im.height // S * 1), Image.LANCZOS)
small.save(os.path.join(ROOT, "vst", "images", "wordmark.png"))
print("wordmark", small.size)
