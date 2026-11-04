"""MergeShip2 -- Ganondorf mode's equipment icons: his own hand (object_ganon gGanondorfRightHandOpenDL; in the Kokiri
Sword's slot: his magic) and the Iron Knuckle's axe (object_ik gIronKnuckleAxeDL; in the Biggoron Sword's slot), rendered
from the player's oot.o2r into 32x32 RGBA32 item icons, as C includes for AdultMask.hpp's in-memory archive.

    python tools/gen_ganon_equip_icons.py --oot-o2r build/latest/oot.o2r --out-dir include/mergeship --preview build/equip_icons.png
"""

import argparse
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402
import render_ganon_preview as gp  # noqa: E402
import gen_ganon_mask_icons as mi  # noqa: E402


def dl_tris(arc, dl):
    tris = rg.run_dl(arc, dl, {}, [], initial=np.eye(4, dtype=np.float32))
    return gp.to_view(tris)


def rolled(tris, deg):
    """Turned about the view's depth axis (render space X): the picture's own rotation."""
    import copy
    c, sn = np.cos(np.radians(deg)), np.sin(np.radians(deg))
    m = np.array([[1, 0, 0], [0, c, -sn], [0, sn, c]], dtype=np.float32)
    out = []
    for t in tris:
        q = copy.copy(t)
        q.p = t.p @ m.T
        (kind, cv), base = t.col
        if kind == "n":
            q.col = ((kind, [m @ np.asarray(v) for v in cv]), base)
        out.append(q)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--preview")
    ap.add_argument("--hy", type=float, default=90.0)   # the hand's yaw (the palm to the viewer)
    ap.add_argument("--hp", type=float, default=0.0)
    ap.add_argument("--hr", type=float, default=90.0)
    ap.add_argument("--ay", type=float, default=90.0)   # the axe's yaw
    ap.add_argument("--ap", type=float, default=45.0)   # the axe diagonal, its head up and right, as the swords' icons
    ap.add_argument("--ar", type=float, default=90.0)   # the blade's face to the viewer
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    arc = rg.Archive(a.oot_o2r)
    hand = rolled(dl_tris(arc, "objects/object_ganon/gGanondorfRightHandOpenDL"), a.hr)
    axe = rolled(dl_tris(arc, "objects/object_ik/gIronKnuckleAxeDL"), a.ar)
    icons = [("Ganondorf's hand", "GanondorfHandIcon.inc", mi.icon(hand, a.hy, 0.95, 0.0, a.hp)),
             ("The Iron Knuckle's axe", "IronKnuckleAxeIcon.inc", mi.icon(axe, a.ay, 1.0, 0.0, a.ap))]
    if a.write:
        for title, name, img in icons:
            mi.write_inc(img, os.path.join(a.out_dir, name), title + "'s item icon")
    if a.preview:
        sheet = Image.new("RGBA", (32 * 8 * len(icons), 32 * 8), (40, 40, 48, 255))
        for k, (_, _, img) in enumerate(icons):
            img = mi.shadowed(img)
            sheet.paste(img.resize((256, 256), Image.NEAREST), (k * 256, 0), img.resize((256, 256), Image.NEAREST))
        sheet.save(a.preview)
    print("ok")


if __name__ == "__main__":
    main()
