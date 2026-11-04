"""MergeShip2 -- Ganondorf mode: how each of the tower Ganondorf's limb meshes sits in its limb's space against the
Hyrule-era Ganondorf's (whose limbs sit as Link's do), found from the meshes themselves.

The two models are the same character: hands, forearms, upper arms, thighs, shins, feet, pelvis, torso and head have
nearly the same shape. For each pair, the limb's own vertices (those its matrix places) are normalized (centroid, RMS
radius) and the rotation A with young ~ A @ tower is searched: the principal axes of both, every proper sign choice
(4), refined by iterative closest points; the one with the least mean distance wins. ganon_rig.py then poses the tower
Ganondorf so that every limb faces exactly as the Hyrule-era one does in the bind pose.

    python tools/ganon_align.py --oot-o2r build/latest/oot.o2r
"""

import argparse
import itertools
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402
import rip_models as rm  # noqa: E402
import render_ganon_preview as gp  # noqa: E402

# tower limb -> Hyrule-era limb
PAIRS = {1: 1, 3: 2, 4: 3, 5: 4, 7: 5, 8: 6, 9: 7, 17: 14, 18: 15, 19: 16, 20: 17, 21: 18, 22: 20, 23: 21, 24: 22}


def limb_points(arc, skel):
    """{limb: vertices in its own space} -- every limb's matrix the identity, each vertex kept by the limb that placed it."""
    names, _ = rm.read_skeleton(arc, skel)
    limbs = [rm.read_limb(arc, n) for n in names]
    order = []

    def visit(i):
        order.append(i)
        if limbs[i]["child"] != 0xFF:
            visit(limbs[i]["child"])
        if limbs[i]["sibling"] != 0xFF:
            visit(limbs[i]["sibling"])

    visit(0)
    eye = np.eye(4, dtype=np.float32)
    matrices, by_addr = {}, {}
    k = 0
    for i in order:
        if limbs[i]["dl"]:
            matrices[0x0D000000 + k * 0x40] = eye
            by_addr[0x0D000000 + k * 0x40] = i
            k += 1
    pts = {}
    for i in order:
        if not limbs[i]["dl"]:
            continue
        for t in rg.run_dl(arc, limbs[i]["dl"], {}, [], matrices=matrices, initial=eye):
            for v in range(3):
                m = t.mids[v] if t.mids is not None else None
                owner = by_addr.get(m, i) if m is not None else i
                pts.setdefault(owner, []).append(t.p[v])
    return {k_: np.unique(np.round(np.array(v), 1), axis=0) for k_, v in pts.items()}


def normalize(p):
    c = p.mean(axis=0)
    q = p - c
    return q / np.sqrt((q ** 2).sum(axis=1).mean())


def nearest(a, b):
    d = ((a[:, None, :] - b[None, :, :]) ** 2).sum(axis=2)
    j = d.argmin(axis=1)
    return b[j], np.sqrt(d[np.arange(len(a)), j])


def kabsch(a, b):
    """Rotation r minimizing |r a - b| (rows are points)."""
    h = a.T @ b
    u, _, vt = np.linalg.svd(h)
    d = np.sign(np.linalg.det(vt.T @ u.T))
    return vt.T @ np.diag([1, 1, d]) @ u.T


def align(tower, young):
    """Rotation A (3x3) with young ~ A @ tower, and the mean distance after it."""
    t, y = normalize(tower), normalize(young)
    et = np.linalg.eigh(t.T @ t)[1]
    ey = np.linalg.eigh(y.T @ y)[1]
    best = (None, 1e9)
    for signs in itertools.product((1, -1), repeat=3):
        s = np.diag(signs)
        a = ey @ s @ et.T
        if np.linalg.det(a) < 0:
            continue
        for _ in range(30):  # ICP
            moved = t @ a.T
            target, _ = nearest(moved, y)
            a = kabsch(t, target)
        _, dist = nearest(t @ a.T, y)
        err = float(dist.mean())
        if err < best[1]:
            best = (a, err)
    return best


def alignments(arc):
    tp = limb_points(arc, gp.ADULT)
    yp = limb_points(arc, gp.YOUNG)
    out = {}
    for t, y in PAIRS.items():
        if t in tp and y in yp:
            out[t] = align(tp[t], yp[y])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    a = ap.parse_args()
    arc = rg.Archive(a.oot_o2r)
    names, _ = rm.read_skeleton(arc, gp.ADULT)
    for t, (m, err) in sorted(alignments(arc).items()):
        angle = np.degrees(np.arccos(np.clip((np.trace(m) - 1) / 2, -1, 1)))
        print("%-30s fit %.3f  turn %5.1f deg" % (names[t].split("/")[-1], err, angle))


if __name__ == "__main__":
    main()
