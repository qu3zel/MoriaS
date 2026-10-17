"""res/moria.ico dosyasını üretir (Durin kapısı kemeri + ithildin parıltısı).
Kullanım:  pip install pillow  &&  python tools/make_icon.py
"""
from PIL import Image, ImageDraw, ImageFilter
import math, os

def draw(size=512):
    S = size
    glow_col = (135, 225, 255)
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    x1, x2, top, bot = S*0.17, S*0.83, S*0.07, S*0.93
    w = x2 - x1
    mid = top + w/2
    # dolgu
    d.ellipse([x1, top, x2, top+w], fill=(12, 18, 24, 255))
    d.rectangle([x1, mid, x2, bot], fill=(12, 18, 24, 255))
    # parıltı katmanı
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    g = ImageDraw.Draw(glow)
    lw = int(S*0.035)
    def outline(dr, col, width):
        dr.line([(x1, bot), (x1, mid)], fill=col, width=width)
        dr.line([(x2, bot), (x2, mid)], fill=col, width=width)
        dr.line([(x1, bot), (x2, bot)], fill=col, width=width)
        h = width/2
        dr.arc([x1-h, top-h, x2+h, top+w+h], 180, 360, fill=col, width=width)
    outline(g, glow_col + (255,), lw*3)
    glow = glow.filter(ImageFilter.GaussianBlur(S*0.03))
    layer = Image.alpha_composite(layer, glow)
    d = ImageDraw.Draw(layer)
    outline(d, glow_col + (255,), lw)
    # sekiz uçlu küçük yıldız
    cx, cy, ro, ri = S/2, mid + (bot-mid)*0.30, S*0.15, S*0.055
    pts = []
    for i in range(16):
        a = -math.pi/2 + i*math.pi/8
        r = ro if i % 2 == 0 else ri
        pts.append((cx + math.cos(a)*r, cy + math.sin(a)*r))
    d.polygon(pts, fill=(200, 245, 255, 255))
    return layer

if __name__ == "__main__":
    img = draw(512)
    out = os.path.join(os.path.dirname(__file__), "..", "res", "moria.ico")
    img.save(out, format="ICO", sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
    print("yazıldı:", os.path.abspath(out))
