"""Review sheets from an `oot3d-extract audit` folder: per character the N64 render, the 3DS render and a close-up of
the top of the 3DS figure (the face of a standing character), 12 characters per sheet.
    py -3.13 tools/oot3d/review_sheets.py <audit dir> <out prefix>"""
import glob, os, sys
from PIL import Image, ImageDraw, ImageChops
audit, prefix = sys.argv[1], sys.argv[2]
BG = (90, 90, 110)
T = 200
def bbox(im):
    diff = ImageChops.difference(im, Image.new("RGB", im.size, BG))
    return diff.convert("L").point(lambda v: 255 if v > 12 else 0).getbbox()
cells = []
for f in sorted(glob.glob(os.path.join(audit, "*~3ds.png"))):
    name = os.path.basename(f)[:-8]
    if name.startswith("obj~"):
        continue
    a = Image.open(f.replace("~3ds", "~n64")).convert("RGB")
    b = Image.open(f).convert("RGB")
    face = b
    bb = bbox(b)
    if bb:
        x0, y0, x1, y1 = bb
        side = max(int((y1 - y0) * 0.30), int((x1 - x0) * 0.25), 20)
        cx = (x0 + x1) // 2
        face = b.crop((cx - side // 2 - side // 4, y0, cx + side // 2 + side // 4, y0 + side))
    cells.append((name, a.resize((T, T)), b.resize((T, T)), face.resize((T, T))))
per = 12
for p in range(0, len(cells), per):
    chunk = cells[p:p + per]
    rows = (len(chunk) + 1) // 2
    im = Image.new("RGB", (6 * T + 8, rows * (T + 14)), (20, 20, 20))
    d = ImageDraw.Draw(im)
    for i, (n, a, b, c) in enumerate(chunk):
        x = (i % 2) * (3 * T + 8)
        y = (i // 2) * (T + 14)
        for k, img in enumerate((a, b, c)):
            im.paste(img, (x + k * T, y + 14))
        d.text((x + 3, y + 1), n.replace("object_", "")[:70], fill=(255, 255, 0))
    im.save(f"{prefix}{p // per:02d}.png")
print(len(cells), "characters")
