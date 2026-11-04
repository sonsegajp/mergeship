"""MergeShip2 -- Ganondorf mode: Epona's skeleton against Ganondorf's horse's (object_horse_ganon), to carry EnHorse's
limb-specific points (the rider's saddle point on limb 30, the hooves, the eyes on limb 13) over to his horse.

Skin limbs are not parsed: each limb resource ends with its joint offset (3 x s16) and child and sibling (u8, u8).
Both horses are posed by frame 0 of their idle animations; for each Epona limb of interest, the Ganon horse limb
whose joint is nearest (relative to the root, scaled by the two heights) is printed.

    python tools/horse_compare.py --oot-o2r build/latest/oot.o2r
"""

import argparse
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402
import rip_models as rm  # noqa: E402
import render_ganon_preview as gp  # noqa: E402
import ganon_rest_compare as rc  # noqa: E402


def limbs_of(arc, skel):
    names, _ = rm.read_skeleton(arc, skel)
    out = []
    for n in names:
        d = arc.read(n)
        pos = struct.unpack_from("<hhh", d, len(d) - 8)
        child, sibling = d[-2], d[-1]
        out.append({"name": n.split("/")[-1], "pos": pos, "child": child, "sibling": sibling})
    return out


def pose(limbs, joints):
    world = [None] * len(limbs)

    def visit(i, parent):
        t = np.eye(4, dtype=np.float32)
        t[:3, 3] = joints[0] if i == 0 else limbs[i]["pos"]
        world[i] = parent @ t @ gp.rot(joints[i + 1])
        if limbs[i]["child"] != 0xFF:
            visit(limbs[i]["child"], world[i])
        if limbs[i]["sibling"] != 0xFF:
            visit(limbs[i]["sibling"], parent)

    visit(0, np.eye(4, dtype=np.float32))
    return world


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    a = ap.parse_args()
    arc = rg.Archive(a.oot_o2r)
    ep = limbs_of(arc, "objects/object_horse/gEponaSkel")
    gh = limbs_of(arc, "objects/object_horse_ganon/gHorseGanonSkel")
    ej, _ = rc.normal_anim_frame(arc, "objects/object_horse/gEponaIdleAnim", 0)
    gj, _ = rc.normal_anim_frame(arc, "objects/object_horse_ganon/gHorseGanonIdleAnim", 0)
    ew = pose(ep, ej)
    gw = pose(gh, gj)
    ep_pts = np.array([m[:3, 3] for m in ew])
    gh_pts = np.array([m[:3, 3] for m in gw])
    eh = ep_pts[:, 1].max() - ep_pts[:, 1].min()
    hh = gh_pts[:, 1].max() - gh_pts[:, 1].min()
    print("joint heights: Epona %.0f, Ganon's horse %.0f (ratio %.2f)" % (eh, hh, hh / eh))
    ground_e, ground_g = ep_pts[:, 1].min(), gh_pts[:, 1].min()
    rel_g = (gh_pts - np.array([0, ground_g, 0])) / hh
    saddle = ew[30] @ np.array([600.0, -1670.0, 0.0, 1.0])
    for label, p in [("saddle (limb 30 + 600,-1670,0)", saddle[:3])] + \
            [("limb %d" % i, ep_pts[i]) for i in (13, 20, 28, 30, 37, 45)]:
        rel = (p - np.array([0, ground_e, 0])) / eh
        d = np.linalg.norm(rel_g - rel, axis=1)
        k = int(d.argmin())
        print("%-32s -> Ganon horse limb %2d (%.3f)  rel=%s" % (label, k, d[k], np.round(rel, 2)))
    # hooves: the lowest leaf limbs of each horse
    for label, pts, limbs in (("Epona", ep_pts, ep), ("Ganon", gh_pts, gh)):
        leaves = [i for i, l in enumerate(limbs) if l["child"] == 0xFF]
        low = sorted(leaves, key=lambda i: pts[i][1])[:6]
        print(label, "lowest leaf limbs:", [(i, np.round(pts[i]).astype(int).tolist()) for i in low])


if __name__ == "__main__":
    main()
