"""Mock-up of the WAVES tab (not part of the build): docs/mockups/waves.png"""
from PIL import Image, ImageDraw, ImageFont
import math
art = Image.open('vst/images/page_WAVES.png').convert('RGBA')
S = 3
big = art.resize((1280 * S, 628 * S), Image.LANCZOS)
d = ImageDraw.Draw(big, 'RGBA')
fnt = ImageFont.truetype('/home/sam/mpc-vst-plugins-434/tools/html_art/fonts/TitilliumWeb-SemiBold.ttf', 15 * S)

def knob(cx, cy, v, red=False):
    ph = math.radians(-135 + 270 * v); r = 33 * S; X, Y = cx * S, cy * S
    d.ellipse((X - r, Y - r, X + r, Y + r), fill=(218, 46, 46, 255) if red else (84, 84, 84, 255))
    dx, dy = X + 22 * S * math.sin(ph), Y - 22 * S * math.cos(ph)
    d.ellipse((dx - 6 * S, dy - 6 * S, dx + 6 * S, dy + 6 * S), fill=(138, 28, 28, 255) if red else (42, 42, 42, 255))

def val(cx, cy, t):
    d.text((cx * S, (cy + 55) * S), t, font=fnt, fill=(187, 200, 219, 255), anchor='mm')

for x, v, t in zip([113, 534, 745, 956, 1166], (.4, .5, .2, .1, .0), ('12', '0', '+8', '0', '0')):
    knob(x, 122, v, red=(x == 113)); val(x, 122, t)
d.rounded_rectangle((300 * S, 95 * S, 350 * S, 125 * S), 6 * S, fill=(96, 126, 167, 255))
d.text((325 * S, 110 * S), 'OFF', font=fnt, fill=(255, 255, 255, 255), anchor='mm')
xs = [86 + 158 * i for i in range(8)]
for cy in (328, 532):
    for i, x in enumerate(xs):
        knob(x, cy, (i * 37 % 100) / 100); val(x, cy, str(i * 9 - 12))
base = big.copy()

def rect(x0, y0, x1, y1):
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), 10 * S, outline=(255, 140, 0, 255), width=2 * S)

def save(name, rects):
    global d
    im = base.copy(); d = ImageDraw.Draw(im, 'RGBA')
    for r in rects: rect(*r)
    im.resize((1280, 628), Image.LANCZOS).convert('RGB').save('docs/mockups/' + name)

L = lambda cy: (xs[0] - 72, cy - 82, xs[3] + 72, cy + 78)
R = lambda cy: (xs[4] - 72, cy - 82, xs[7] + 72, cy + 78)
save('waves_sub1.png', [(30, 50, 1042, 195), (1075, 50, 1255, 195), L(328), R(328)])
save('waves_sub2.png', [L(532), R(532)])
