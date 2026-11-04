"""MergeShip2 -- measures and tunes each Majora's Mask mask's placement on Ocarina of Time's adult Link
(include/mergeship/MaskFit.h) against how Majora's Mask places it on its own young Link, with depth maps of the
head and of the mask from five views (front, three-quarter, side, back, top):

  clipping   the share of the mask's silhouette where the head shows in front of the mask (the head poking
             through), beyond what young Link shows with the same mask (MM's own hair and hat poke out of some)
  gap        how far the mask stands off the face (front view, the nearest tenth of the face under the mask),
             against young Link's gap scaled to the adult's head

The search moves each mask forward / back, up / down and scales it (coordinate descent, a few rounds) until
neither is worse than young Link's, preferring the smallest change from the head-to-head mapping.

    python tools/tune_mask_fit.py --mm-o2r build/host10/mm.o2r --oot-o2r build/host10/oot.o2r --out fits.json
        [--report] (measure the header's fits only)
"""

import argparse
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402

RES = 160
HEAD_SCALE = 1.10
VIEWS = [("front", 0), ("3/4", 35), ("side", 90), ("back", 180), ("top", None)]


def view_m(v):
    if v is None:  # from above: screen up = forward (+X), toward the viewer = up (-Y)
        return np.stack([np.array([0.0, 0.0, -1.0]), np.array([1.0, 0.0, 0.0]), np.array([0.0, -1.0, 0.0])])
    return rg.view_matrix(v)


def depth_map(tris, m, center, scale):
    depth = np.full((RES, RES), -np.inf, dtype=np.float32)
    for t in tris:
        q = (t.p - center) @ m.T
        sx = RES / 2 + q[:, 0] * scale
        sy = RES / 2 - q[:, 1] * scale
        sz = q[:, 2]
        area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0])
        if abs(area) < 1e-6:
            continue
        x0, x1 = max(int(sx.min()), 0), min(int(sx.max()) + 1, RES - 1)
        y0, y1 = max(int(sy.min()), 0), min(int(sy.max()) + 1, RES - 1)
        if x0 > x1 or y0 > y1:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        w0 = ((sx[1] - gx) * (sy[2] - gy) - (sx[2] - gx) * (sy[1] - gy)) / area
        w1 = ((sx[2] - gx) * (sy[0] - gy) - (sx[0] - gx) * (sy[2] - gy)) / area
        w2 = 1 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not inside.any():
            continue
        zz = w0 * sz[0] + w1 * sz[1] + w2 * sz[2]
        region = depth[y0:y1 + 1, x0:x1 + 1]
        upd = inside & (zz > region)
        region[upd] = zz[upd]
    return depth


class Scene:
    def __init__(self, head, center, scale):
        self.center = center
        self.scale = scale
        self.ms = [view_m(v) for _, v in VIEWS]
        self.head = [depth_map(head, m, center, scale) for m in self.ms]

    def measure(self, mask):
        clips = []
        gap = None
        for i, m in enumerate(self.ms):
            md = depth_map(mask, m, self.center, self.scale)
            hd = self.head[i]
            sil = np.isfinite(md)
            n = sil.sum()
            if n == 0:
                clips.append(0.0)
                continue
            poke = sil & np.isfinite(hd) & (hd > md + 8.0)
            clips.append(poke.sum() / n)
            if VIEWS[i][0] == "front":
                both = sil & np.isfinite(hd) & (md >= hd)
                if both.sum() > 10:
                    d = (md - hd)[both]
                    gap = float(np.percentile(d, 10))
        return clips, gap


def fitted(mask, fit):
    return rg.fit_tris(mask, fit)


def score(adult_scene, mask, fit, target_clips, target_gap):
    clips, gap = adult_scene.measure(fitted(mask, fit))
    over = sum(max(0.0, c - t - 0.01) for c, t in zip(clips, target_clips))
    gap_err = 0.0
    if gap is not None and target_gap is not None:
        gap_err = abs(gap - target_gap) / 100.0
    return over * 10.0 + gap_err, clips, gap


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mm-o2r", required=True)
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--header", default=os.path.join(os.path.dirname(__file__), "..", "include", "mergeship",
                                                     "MaskFit.h"))
    ap.add_argument("--out", help="tuned fits as JSON (render_mask_grid.py --fit)")
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--masks")
    args = ap.parse_args()

    mm = rg.Archive(args.mm_o2r)
    oot = rg.Archive(args.oot_o2r)
    fits = rg.read_fits(args.header)
    adult_head = rg.run_dl(oot, "objects/object_link_boy/gLinkAdultHeadNearDL",
                           {8: "objects/object_link_boy/gLinkAdultEyesOpenTex",
                            9: "objects/object_link_boy/gLinkAdultMouth1Tex"}, [])
    young_head = rg.run_dl(mm, "mm/objects/object_link_child/gLinkHumanHeadDL",
                           {8: "mm/objects/object_link_child/gLinkHumanEyesOpenTex",
                            9: "mm/objects/object_link_child/gLinkHumanMouthClosedTex"}, [])

    def center_of(tris):
        pts = np.concatenate([t.p for t in tris])
        return (pts.min(0) + pts.max(0)) / 2 + np.array([150.0, -420.0, 0.0])

    scale = RES / 2750.0
    adult = Scene(adult_head, center_of(adult_head), scale)
    young = Scene(young_head, center_of(young_head), scale)
    wanted = None if not args.masks else {int(m) for m in args.masks.split(",")}
    results = {}
    for n, title, suffix in rg.MASKS:
        if wanted and n not in wanted:
            continue
        name = rg.find(mm, "mm/objects/" + suffix)
        mask = rg.run_dl(mm, name, {}, []) if name else []
        if not mask:
            print(f"{n:2d} {title}: no geometry")
            continue
        yclips, ygap = young.measure(mask)
        base = list(fits.get(n, fits[0]))
        # The stand-off young Link has, scaled to the adult's head (about 1.1 times the young one): a mask stretched
        # further front to back stands out in front of the face (the user, 2026-10-01: "make sure no mask is too far
        # forward").
        tgap = None if ygap is None else ygap * HEAD_SCALE
        s0, c0, g0 = score(adult, mask, tuple(base), yclips, tgap)
        line = (f"{n:2d} {title:22s} young clip {['%.2f' % c for c in yclips]} gap {ygap if ygap is None else round(ygap)}"
                f" | adult clip {['%.2f' % c for c in c0]} gap {g0 if g0 is None else round(g0)} score {s0:.2f}")
        if args.report:
            print(line)
            continue
        best = (s0, base)
        cur = list(base)
        steps = {0: [-160, -120, -80, -40, -20, 0, 20, 40, 80], 1: [-120, -60, -30, 0, 30, 60, 120],
                 "k": [0.92, 0.96, 1.0, 1.04, 1.08, 1.12, 1.16], "ax": [0.80, 0.85, 0.90, 0.95, 1.0]}
        for _round in range(3):
            for key in ("ax", 0, "k", 1, 0):
                cands = []
                for d in steps[key]:
                    f = list(cur)
                    if key == "k":
                        f[3], f[4], f[5] = cur[3] * d, cur[4] * d, cur[5] * d
                    elif key == "ax":  # the front-to-back stretch only
                        f[3] = cur[3] * d
                    else:
                        f[key] = cur[key] + d
                    # prefer the smallest change from the head-to-head mapping
                    change = (abs(f[0] - base[0]) + abs(f[1] - base[1])) / 4000.0 + abs(f[3] / base[3] - 1.0) / 5.0
                    sc, _, _ = score(adult, mask, tuple(f), yclips, tgap)
                    cands.append((sc + change, f))
                cands.sort(key=lambda c: c[0])
                if cands[0][0] < best[0]:
                    best = cands[0]
                cur = list(best[1])
        s1, c1, g1 = score(adult, mask, tuple(best[1]), yclips, tgap)
        f = best[1]
        print(line)
        print(f"   tuned x{f[0]:.0f} y{f[1]:.0f} s{f[3]:.2f}/{f[4]:.2f}/{f[5]:.2f}: clip {['%.2f' % c for c in c1]}"
              f" gap {g1 if g1 is None else round(g1)} (target {tgap if tgap is None else round(tgap)}) score {s1:.2f}")
        results[str(n)] = {"x": round(f[0], 1), "y": round(f[1], 1), "z": round(f[2], 1), "sx": round(f[3], 3),
                           "sy": round(f[4], 3), "sz": round(f[5], 3), "pitch": f[6]}
        sys.stdout.flush()
    if args.out and results:
        json.dump(results, open(args.out, "w"), indent=1)
        print("wrote", args.out)


if __name__ == "__main__":
    main()
