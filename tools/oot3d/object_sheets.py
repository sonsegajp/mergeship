"""Review sheets of an `oot3d-extract audit` folder's objects: N64 and 3DS side by side, 24 per sheet.
    py -3.13 tools/oot3d/object_sheets.py <audit dir> <out prefix> [filter]"""
import glob, os, sys
from PIL import Image, ImageDraw
audit, prefix = sys.argv[1], sys.argv[2]
flt = sys.argv[3] if len(sys.argv) > 3 else ""
T = 150
cells = []
for f in sorted(glob.glob(os.path.join(audit, "obj~*~3ds.png"))):
    name = os.path.basename(f)[4:-8]
    if flt not in name:
        continue
    a = Image.open(f.replace("~3ds", "~n64")).convert("RGB").resize((T, T))
    b = Image.open(f).convert("RGB").resize((T, T))
    cells.append((name, a, b))
cols, rows = 4, 6
per = cols * rows
for p in range(0, len(cells), per):
    chunk = cells[p:p + per]
    im = Image.new("RGB", (cols * (2 * T + 6), ((len(chunk) + cols - 1) // cols) * (T + 13)), (20, 20, 20))
    d = ImageDraw.Draw(im)
    for i, (n, a, b) in enumerate(chunk):
        x = (i % cols) * (2 * T + 6)
        y = (i // cols) * (T + 13)
        im.paste(a, (x, y + 13))
        im.paste(b, (x + T, y + 13))
        d.text((x + 2, y), n.replace("object_", "")[:46], fill=(255, 255, 0))
    im.save(f"{prefix}{p // per:02d}.png")
print(len(cells), "objects,", (len(cells) + per - 1) // per, "sheets")
