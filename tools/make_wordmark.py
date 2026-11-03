#!/usr/bin/env python3
"""The WELLE wordmark: skin/wordmark/welle.svg (drawn for this project: monoline Bezier letters on one slant, the first E's middle stroke
running on through the tan double L) rendered with rsvg-convert (in Docker, as tools/make_assets.py) to
skin/parts/wordmark.png (4x) and vst/images/wordmark.png (57 px tall, for the pages and the browser tile). Outputs are committed."""
import os, subprocess
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
WM = os.path.join(ROOT, "skin", "wordmark")
subprocess.run(["docker", "run", "--rm", "-v", WM + ":/w", "alpine", "sh", "-c",
                "apk add -q rsvg-convert >/dev/null && rsvg-convert -w 1040 /w/welle.svg -o /w/welle_4x.png"], check=True)
w = Image.open(os.path.join(WM, "welle_4x.png")).convert("RGBA")
os.remove(os.path.join(WM, "welle_4x.png"))
w = w.crop(w.getbbox())
w.save(os.path.join(ROOT, "skin", "parts", "wordmark.png"))
w.resize((w.width * 57 // w.height, 57), Image.LANCZOS).save(os.path.join(ROOT, "vst", "images", "wordmark.png"))
print("wordmark written")
