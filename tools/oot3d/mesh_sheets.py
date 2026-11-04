"""Per-mesh contact sheets of every converted 3DS character (tools/oot3d curation aid).

Reads build.log's "skin <skeleton> <- <zar>:<model>" lines, renders the whole model and each CMB mesh alone
(oot3d-extract render, OOT3D_RENDER_MESH) and writes one labelled sheet per character to <out>/<object>~<skel>.png.
    py -3.13 tools/oot3d/mesh_sheets.py <romfs> <build.log> <out> [filter]
"""
import os, re, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
from PIL import Image, ImageDraw

EXE = os.path.join(os.path.dirname(__file__), "..", "..", "build", "oot3d", "Release", "oot3d-extract.exe")
rom, log, out = sys.argv[1], sys.argv[2], sys.argv[3]
flt = sys.argv[4] if len(sys.argv) > 4 else ""
os.makedirs(out, exist_ok=True)
skins = []
for line in open(log, encoding="utf-8", errors="replace"):
    m = re.match(r"skin (\S+) <- (\S+?):(\S+) \(", line)
    if m and flt in m.group(1):
        skins.append(m.groups())
tmp = tempfile.mkdtemp()
S = 192

def mesh_count(zar, model):
    r = subprocess.run([EXE, "cmbinfo", rom, zar], capture_output=True, text=True)
    for l in r.stdout.splitlines():
        if l.startswith(model + ":"):
            return int(re.search(r"(\d+) meshes", l).group(1))
    return 0

def render(zar, model, mesh, png):
    env = dict(os.environ, OOT3D_RENDER_MESH="" if mesh < 0 else str(mesh))
    subprocess.run([EXE, "render", rom, zar, model, "-", "0", "20", "10", png, str(S)], env=env,
                   capture_output=True)

def sheet(item):
    skel, zar, model = item
    n = mesh_count(zar, model)
    pngs = []
    for k in range(-1, n):
        p = os.path.join(tmp, f"{abs(hash(skel))}_{k}.png")
        render(zar, model, k, p)
        pngs.append((k, p))
    cols = 8
    rows = (len(pngs) + cols - 1) // cols
    im = Image.new("RGB", (cols * S, rows * (S + 14) + 16), (30, 30, 30))
    d = ImageDraw.Draw(im)
    d.text((4, 2), f"{skel}  <-  {zar}:{model}", fill=(255, 255, 0))
    for i, (k, p) in enumerate(pngs):
        x, y = (i % cols) * S, 16 + (i // cols) * (S + 14)
        if os.path.exists(p):
            im.paste(Image.open(p).convert("RGB"), (x, y + 14))
        d.text((x + 3, y + 1), "ALL" if k < 0 else f"mesh {k}", fill=(120, 255, 120))
    name = skel.replace("objects/", "").replace("/", "~")
    im.save(os.path.join(out, name + ".png"))
    return name, n

with ThreadPoolExecutor(8) as ex:
    for name, n in ex.map(sheet, skins):
        print(f"{n:3d} meshes  {name}")
