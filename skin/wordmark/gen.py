#!/usr/bin/env python3
"""Writes the WELLE wordmark variants: welle.svg (A: round single-stroke letters, one middle stroke through both E's and the L's)
and welle_wave.svg (B: the same, the middle stroke a wavetable scan: sine, then triangle, then saw)."""
import math, os
HERE = os.path.dirname(os.path.abspath(__file__))
PALE, TAN = "#BBC8DB", "#D1AE88"
W_ = "M11,18 C9,46 15,62 26,62 C35,62 38,50 38.5,36 C39,50 42,62 51,62 C62,62 68,46 66,18"
def E(x):   # a round C, its left edge at x
    d = x - 73
    pts = [(101, 24), (97, 18), (92, 16), (87, 16), (77, 16), (73, 28), (73, 39), (73, 51), (78, 62), (88, 62), (93, 62), (98, 60), (102, 55)]
    p = [(a + d, b) for a, b in pts]
    return "M%g,%g C%g,%g %g,%g %g,%g C%g,%g %g,%g %g,%g C%g,%g %g,%g %g,%g C%g,%g %g,%g %g,%g" % tuple(v for q in p for v in q)
def L(x):   # an upright stem, a round foot
    return "M%g,14 L%g,50 C%g,58 %g,62 %g,62 L%g,62" % (x, x, x, x + 4, x + 11, x + 15)
E1, L1, L2, E2 = 73, 114, 137, 160
BAR = (75, 39, E2 + 18)
def straight():
    return "M%g,%g L%g,%g" % (BAR[0], BAR[1], BAR[2], BAR[1])
def scan():
    """three single cycles across the bar: sine, triangle, saw (each starting and ending on the centre line)"""
    x0, y, x1 = BAR
    pts, n, amp = [], 600, 6.5
    for i in range(n + 1):
        t = i / n
        x = x0 + (x1 - x0) * t
        c, ph = min(2, int(t * 3)), (t * 3) % 1.0 if t < 1 else 1.0
        if c == 0:
            v = math.sin(2 * math.pi * ph)
        elif c == 1:
            v = 4 * ph if ph < 0.25 else 2 - 4 * ph if ph < 0.75 else 4 * ph - 4
        else:
            v = 2 * ph if ph < 0.5 else 2 * ph - 2       # rise to the top, drop to the bottom, rise to the line
        pts.append((x, y - amp * v))
    return "M" + " L".join("%.2f,%.2f" % p for p in pts)
def svg(bar, w):
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="220" height="80" viewBox="0 0 220 80">
  <!-- WELLE wordmark (tools: skin/wordmark/gen.py). Monoline, one stroke weight, a common slant: round W and E's, tan L's,
       one pale stroke through both E's and the L's{" as a wavetable scan (sine, triangle, saw)" if w else ""}. Drawn for Welle for MPC. -->
  <g transform="translate(14,0) skewX(-12)" fill="none" stroke-width="4.6" stroke-linecap="round" stroke-linejoin="round">
    <path stroke="{PALE}" d="{W_}"/>
    <path stroke="{PALE}" d="{E(E1)}"/>
    <path stroke="{TAN}" d="{L(L1)}"/>
    <path stroke="{TAN}" d="{L(L2)}"/>
    <path stroke="{PALE}" d="{E(E2)}"/>
    <path stroke="{PALE}" stroke-width="{4.0 if w else 4.6}" d="{bar}"/>
  </g>
</svg>
'''
open(os.path.join(HERE, "welle.svg"), "w").write(svg(straight(), False))
open(os.path.join(HERE, "welle_wave.svg"), "w").write(svg(scan(), True))
