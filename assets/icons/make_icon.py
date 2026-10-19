# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) FissionCAD contributors
# Draws the Fission 360 app / file icon (original artwork): an orange rounded tile with a white "F"
# stroke and an isometric block. Writes fission360.ico (16-256 px). Run: python make_icon.py
from PIL import Image, ImageDraw

S = 1024
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
d.rounded_rectangle([40, 40, S - 40, S - 40], radius=180, fill=(242, 140, 40, 255))
d.rounded_rectangle([40, 40, S - 40, S // 2], radius=180, fill=(246, 160, 72, 255))
d.rectangle([40, S // 2 - 180, S - 40, S // 2], fill=(242, 140, 40, 255))
d.rounded_rectangle([40, 40, S - 40, S - 40], radius=180, outline=(196, 102, 18, 255), width=24)

white = (255, 255, 255, 255)
# "F"
d.rectangle([230, 210, 330, 820], fill=white)
d.rectangle([230, 210, 640, 300], fill=white)
d.rectangle([230, 460, 560, 545], fill=white)

# Isometric block, lower right
cx, cy, a = 700, 690, 150
top = [(cx, cy - a), (cx + a, cy - a // 2), (cx, cy), (cx - a, cy - a // 2)]
left = [(cx - a, cy - a // 2), (cx, cy), (cx, cy + a), (cx - a, cy + a // 2)]
right = [(cx, cy), (cx + a, cy - a // 2), (cx + a, cy + a // 2), (cx, cy + a)]
d.polygon(top, fill=(255, 255, 255, 255))
d.polygon(left, fill=(255, 226, 196, 255))
d.polygon(right, fill=(255, 205, 150, 255))
for poly in (top, left, right):
    d.line(poly + [poly[0]], fill=(196, 102, 18, 255), width=10)

img.save("fission360.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
img.resize((256, 256), Image.LANCZOS).save("fission360.png")
