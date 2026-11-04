"""MergeShip2 -- rips Ocarina of Time's Castle Town for a 3D rebuild in Blender: every pre-rendered background (JPEG,
as the game shows it) with the fixed camera it was rendered from, each area's collision mesh (the real 3D layout of its
floors and building fronts), and the real 3D geometry the scenes have (the Market square is not pre-rendered: it is
low-poly 3D with fixed cameras; the pre-rendered rooms also draw a few 3D pieces over their backgrounds).

    python tools/rip_castle_town.py --oot-o2r build/host10/oot.o2r --out build/castletown

Writes, per area, <out>/<area>/: bg_<n>.jpg (the background for camera n), collision.json (vertices, triangles, the
cameras: setting, position, rotation, field of view), and the room's 3D display lists through rip_models.py's exporter
(scene.json + textures) for tools/build_castle_town.py.

Camera data (the collision header's camera list, CollisionHeaderFactory.cpp): each entry is a camera setting and an
index into the camera position list, whose entries for a fixed camera are three Vec3s: position, rotation (binary
angles: pitch, yaw, roll) and (field of view, flags, -). A field of view above 360 is hundredths of a degree.
"""

import argparse
import json
import os
import re
import struct
import sys
import zipfile

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402
import rip_models  # noqa: E402

AREAS = [
    # (folder, scene, label)
    ("market_day", "market_day", "Market (day)"),
    ("market_night", "market_night", "Market (night)"),
    ("market_ruins", "market_ruins", "Market (ruins)"),
    ("entrance_day", "entra", "Market Entrance (day)"),
    ("entrance_night", "entra_n", "Market Entrance (night)"),
    ("entrance_ruins", "enrui", "Market Entrance (ruins)"),
    ("alley_day", "market_alley", "Back Alley (day)"),
    ("alley_night", "market_alley_n", "Back Alley (night)"),
    ("temple_day", "shrine", "Temple of Time exterior (day)"),
    ("temple_night", "shrine_n", "Temple of Time exterior (night)"),
    ("temple_ruins", "shrine_r", "Temple of Time exterior (ruins)"),
    ("bazaar", "shop1", "Bazaar"),
    ("bombchu_shop", "alley_shop", "Bombchu Shop"),
    ("potion_shop", "night_shop", "Potion Shop (Market)"),
    ("mask_shop", "face_shop", "Happy Mask Shop"),
]


class Reader:
    def __init__(self, data, offset=0x40):
        self.d, self.o = data, offset

    def take(self, fmt):
        v = struct.unpack_from("<" + fmt, self.d, self.o)
        self.o += struct.calcsize("<" + fmt)
        return v[0] if len(v) == 1 else v


def read_collision(data):
    r = Reader(data)
    r.take("3h")
    r.take("3h")
    nv = r.take("i")
    verts = [list(r.take("3h")) for _ in range(nv)]
    npoly = r.take("I")
    tris = []
    for _ in range(npoly):
        ptype, a, b, c, nx, ny, nz, dist = r.take("8H")
        tris.append([a & 0x1FFF, b & 0x1FFF, c & 0x1FFF, ptype])
    nsurf = r.take("I")
    surfaces = [list(r.take("2I")) for _ in range(nsurf)]
    ncam = r.take("I")
    cams = []
    for _ in range(ncam):
        setting, count = r.take("Hh")
        idx = r.take("i")
        cams.append((setting, count, idx))
    npos = r.take("i")
    pos = [list(r.take("3h")) for _ in range(npos)]
    cameras = []
    for i, (setting, count, idx) in enumerate(cams):
        entry = {"index": i, "setting": setting, "count": count}
        if npos > 0 and 0 <= idx and idx + 2 < npos and count >= 3:
            p, rot, extra = pos[idx], pos[idx + 1], pos[idx + 2]
            fov = extra[0]
            entry.update({"pos": p, "rot": rot, "fov_raw": fov,
                          "fov_deg": (fov / 100.0) if fov > 360 else (60.0 if fov == -1 else float(fov)),
                          "extra": extra})
        cameras.append(entry)
    return {"vertices": verts, "triangles": tris, "surfaces": surfaces, "cameras": cameras}


def room_images(data):
    """The room's background list: (camera index, resource path) for each, from its mesh command."""
    out = []
    for m in re.finditer(rb"scenes/[\x20-\x7e]+?Background_[0-9A-F]+", data):
        start = m.start()
        n = struct.unpack_from("<I", data, start - 4)[0]
        if n != len(m.group(0)):
            continue
        cam = data[start - 5]
        path = m.group(0).decode()
        if not any(p == path for _, p in out):
            out.append((cam, path))
    return out


def room_dls(data):
    return sorted({m.group(0).decode() for m in re.finditer(rb"scenes/[\x20-\x7e]+?_room_\d+DL_[0-9A-F]+", data)})


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    z = zipfile.ZipFile(args.oot_o2r)
    arc = rg.Archive(args.oot_o2r)
    names = z.namelist()
    summary = []
    for folder, scene, label in AREAS:
        base = f"scenes/shared/{scene}_scene/"
        files = [n for n in names if n.startswith(base)]
        if not files:
            print("missing", scene)
            continue
        out = os.path.join(args.out, folder)
        os.makedirs(out, exist_ok=True)
        col_name = [n for n in files if "CollisionHeader" in n]
        collision = read_collision(z.read(col_name[0])) if col_name else None
        rooms = sorted(n for n in files if re.search(r"_room_\d+$", n))
        backgrounds, dls = [], []
        for room in rooms:
            data = z.read(room)
            for cam, path in room_images(data):
                d = z.read(path)
                size = struct.unpack_from("<I", d, 0x40)[0]
                jpg = d[0x44:0x44 + size]
                fname = f"bg_cam{cam}.jpg"
                if os.path.exists(os.path.join(out, fname)):
                    fname = f"bg_cam{cam}_{path.split('_')[-1]}.jpg"
                open(os.path.join(out, fname), "wb").write(jpg)
                backgrounds.append({"camera": cam, "file": fname, "source": path})
            dls += room_dls(data)
        info = {"label": label, "scene": scene, "backgrounds": backgrounds, "display_lists": dls}
        if collision is not None:
            info["collision"] = collision
        json.dump(info, open(os.path.join(out, "area.json"), "w"))
        # the rooms' 3D geometry, textured, in the scene's own space (no scale: build_castle_town.py scales)
        if dls:
            tris = []
            for dl in dls:
                try:
                    tris += rg.run_dl(arc, dl, {}, [])
                except IndexError as e:  # a list the reader does not follow to the end: keep what it read
                    print(f"  {dl}: stopped ({e})")
            rip_models.export({label: [("geometry", tris, (0.0, 0.0, 0.0))]}, out, 1.0)
        cams = [c for c in (collision or {}).get("cameras", []) if "pos" in c]
        summary.append((label, len(backgrounds), len(dls), len(collision["triangles"]) if collision else 0, len(cams)))
        print(f"{label}: {len(backgrounds)} backgrounds, {len(dls)} display lists, "
              f"{len(collision['triangles']) if collision else 0} collision triangles, {len(cams)} fixed cameras")
    json.dump(summary, open(os.path.join(args.out, "summary.json"), "w"))


if __name__ == "__main__":
    main()
