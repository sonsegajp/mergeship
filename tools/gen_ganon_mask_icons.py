"""MergeShip2 -- Ganondorf mode's mask icons: the Phantom Ganon mask (his horned mask, object_gnd limb 14) and Ganon's
mask (Beast Ganon's head, object_ganon2), rendered from the player's oot.o2r into 32x32 RGBA32 item icons in the
style of the other mask icons (the face from the front, a slight turn), as C includes for AdultMask.hpp's in-memory
archive (mergeship/ganondorf/...).

    python tools/gen_ganon_mask_icons.py --oot-o2r build/latest/oot.o2r --out-dir include/mergeship --preview build/icons.png
"""

import argparse
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402
import rip_models as rm  # noqa: E402
import render_ganon_preview as gp  # noqa: E402
import ganon_rest_compare as rc  # noqa: E402

KEY = (255, 0, 255)


def head_tris(arc, skel, anim, keep):
    names, _ = rm.read_skeleton(arc, skel)
    own, _ = rc.normal_anim_frame(arc, anim, 0)
    rots = [gp.rot(own[i + 1]) for i in range(len(names))]
    limbs = [rm.read_limb(arc, n) for n in names]
    world = [None] * len(limbs)
    order = []

    def visit(i, parent):
        t = np.eye(4, dtype=np.float32)
        t[:3, 3] = own[0] if i == 0 else limbs[i]["pos"]
        world[i] = parent @ t @ rots[i]
        order.append(i)
        if limbs[i]["child"] != 0xFF:
            visit(limbs[i]["child"], world[i])
        if limbs[i]["sibling"] != 0xFF:
            visit(limbs[i]["sibling"], parent)

    visit(0, np.eye(4, dtype=np.float32))
    matrices = {}
    k = 0
    for i in order:
        if limbs[i]["dl"]:
            matrices[0x0D000000 + k * 0x40] = world[i]
            k += 1
    tris = []
    for i in order:
        if limbs[i]["dl"] and (i in keep):
            tris += rg.run_dl(arc, limbs[i]["dl"], {}, [], matrices=matrices, initial=world[i])
    return gp.to_view(tris)


def pitched(tris, deg):
    """The triangles turned about the view's sideways axis (render space Z), so the face is seen from below/above."""
    import copy
    c, sn = np.cos(np.radians(deg)), np.sin(np.radians(deg))
    m = np.array([[c, -sn, 0], [sn, c, 0], [0, 0, 1]], dtype=np.float32)
    out = []
    for t in tris:
        q = copy.copy(t)
        q.p = t.p @ m.T
        (kind, cv), base = t.col
        if kind == "n":
            q.col = ((kind, [m @ np.asarray(v) for v in cv]), base)
        out.append(q)
    return out


def icon(tris, yaw, zoom, lift, pitch=0.0):
    big = 256
    tris = pitched(tris, pitch)
    pts = np.concatenate([t.p for t in tris])
    lo, hi = pts.min(axis=0), pts.max(axis=0)
    center = (lo + hi) / 2
    center[1] += lift * (hi[1] - lo[1])
    img = rg.render(tris, yaw, big, center, big * zoom / max(hi - lo), bg=KEY).convert("RGBA")
    a = np.array(img)
    key = (a[:, :, 0] == KEY[0]) & (a[:, :, 1] == KEY[1]) & (a[:, :, 2] == KEY[2])
    a[key] = (0, 0, 0, 0)
    small = Image.fromarray(a).resize((32, 32), Image.LANCZOS)
    s = np.array(small)
    s[s[:, :, 3] < 100] = (0, 0, 0, 0)
    s[s[:, :, 3] >= 100, 3] = 255
    return Image.fromarray(s)


def shadowed(img, dx=1, dy=1, alpha=170):
    """The icon over its own silhouette, dark, a pixel down and right: the drop shadow Ocarina of Time's item icons
    carry (the user, 2026-10-17: "put drop shadows on all custom ganon icons")."""
    a = np.array(img)
    out = np.zeros_like(a)
    mask = a[:, :, 3] > 0
    sh = np.zeros_like(mask)
    sh[dy:, dx:] = mask[:-dy, :-dx]
    out[sh] = (0, 0, 0, alpha)
    out[mask] = a[mask]
    return Image.fromarray(out)


def write_inc(img, path, title):
    data = np.array(shadowed(img)).reshape(-1)
    lines = ["// MergeShip2 -- %s: 32x32 RGBA32, rows top to bottom (tools/gen_ganon_mask_icons.py). Included by" % title,
             "// AdultMask.hpp."]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02X" % v for v in data[i:i + 16]) + ",")
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--preview")
    ap.add_argument("--pp", type=float, default=0.0)
    ap.add_argument("--gp", type=float, default=0.0)
    ap.add_argument("--py", type=float, default=15.0)
    ap.add_argument("--gy", type=float, default=15.0)
    a = ap.parse_args()
    arc = rg.Archive(a.oot_o2r)
    phantom = head_tris(arc, "objects/object_gnd/gPhantomGanonSkel", "objects/object_gnd/gPhantomGanonNeutralAnim",
                        {14})
    ganon = head_tris(arc, "objects/object_ganon2/gGanonSkel", "objects/object_ganon2/gGanonGuardIdleAnim",
                      set(range(14, 34)))
    icons = [("Phantom Ganon mask", "PhantomGanonMaskIcon.inc", icon(phantom, a.py, 0.95, 0.0, a.pp)),
             ("Ganon's mask", "GanonMaskIcon.inc", icon(ganon, a.gy, 0.95, 0.0, a.gp))]
    for title, name, img in icons:
        write_inc(img, os.path.join(a.out_dir, name), title + "'s item icon")
    if a.preview:
        sheet = Image.new("RGBA", (32 * 8 * len(icons), 32 * 8), (40, 40, 48, 255))
        for k, (_, _, img) in enumerate(icons):
            img = shadowed(img)
            sheet.paste(img.resize((256, 256), Image.NEAREST), (k * 256, 0), img.resize((256, 256), Image.NEAREST))
        sheet.save(a.preview)
    print("ok")


if __name__ == "__main__":
    main()
