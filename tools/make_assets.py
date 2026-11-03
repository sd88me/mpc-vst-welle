#!/usr/bin/env python3
"""Skin parts from Wave Emulation's own artwork (mo0kid, GPL-3.0, ~/src/wave/media):
  skin/parts/knob_grey.png, knob_red.png   Knob1.svg / Knob_red1.svg rendered (rsvg-convert -w 400)
  skin/parts/ring_src.png                  a knob's printed scale ring, cut from WaldorfWaveUI_NOLOGO.svg rendered 4676 px wide
  skin/parts/bar_*.png                     section title bars cut from the same render
This writes vst/images/knob_wave.png and knob_red.png (101-frame filmstrips, the knob turned through 270 degrees as the editor
turns it) and vst/images/ring.png (the ring alone, on transparency). Committed outputs: a build needs neither the SVGs nor rsvg."""
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PARTS = os.path.join(ROOT, "skin", "parts")
IMG = os.path.join(ROOT, "vst", "images")
os.makedirs(IMG, exist_ok=True)
N, S = 101, 104

for src, out in (("knob_grey.png", "knob_wave.png"), ("knob_red.png", "knob_red.png")):
    k = Image.open(os.path.join(PARTS, src)).convert("RGBA")
    film = Image.new("RGBA", (S, S * N))
    for i in range(N):
        a = 135 - 270 * i / (N - 1)                       # minimum at 7 o'clock, clockwise
        f = k.rotate(a, resample=Image.BICUBIC).resize((S, S), Image.LANCZOS)
        film.paste(f, (0, i * S))
    film.save(os.path.join(IMG, out))

ring = Image.open(os.path.join(PARTS, "ring_src.png")).convert("RGBA").crop((0, 0, 130, 106))
px = ring.load()
for y in range(ring.height):
    for x in range(ring.width):
        r, g, b, _ = px[x, y]
        light = max(0, min(255, (r + g + b) // 3 - 70) * 255 // 130)   # the ring's light grey-blue; the panel and knob body go
        px[x, y] = (187, 200, 219, min(255, light))
ring.save(os.path.join(IMG, "ring.png"))
print("assets written")
