"""MergeShip2 -- Ganondorf mode preview: Ocarina of Time's Ganondorfs drawn on Link's rig, out of the engine.

    python tools/render_ganon_preview.py --oot-o2r build/latest/oot.o2r --out build/ganon_preview.png

Each Ganondorf keeps his own skeleton's proportions (its joint offsets) and takes Link's joint rotations from Link's
own animations, limb for limb (Link's 21 limbs -> his):
  root            <- Link's root (translation scaled by the leg length ratio, so the feet stay on the floor)
  pelvis          <- Link's waist then lower control (Link has a control limb between them)
  thighs/shins/feet <- Link's thighs/legs/feet
  torso           <- Link's upper control
  shoulder pads (adult) / upper arms (young) <- Link's shoulders (upper arms); the adult's upper arm keeps 0
  forearms/hands  <- Link's arms/hands
  jewel (neck: the head's base) <- Link's head
  face and hair limbs keep 0.
Rows: adult Link (reference), the Hyrule-era Ganondorf (object_gndd), the tower Ganondorf (object_ganon).
Columns: idle (front, three-quarter), run, sword swing.
"""

import argparse
import math
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402
import rip_models as rm  # noqa: E402

LINK = "objects/object_link_boy/gLinkAdultSkel"
YOUNG = "objects/object_gndd/gYoungGanondorfSkel"
ADULT = "objects/object_ganon/gGanondorfSkel"

# Link's limb indices (gLinkAdultSkel)
L_ROOT, L_WAIST, L_LOWER, L_RTHIGH, L_RLEG, L_RFOOT, L_LTHIGH, L_LLEG, L_LFOOT, L_UPPER, L_HEAD, L_HAT, L_COLLAR, \
    L_LSHOULDER, L_LARM, L_LHAND, L_RSHOULDER, L_RARM, L_RHAND, L_SWORD, L_TORSO = range(21)

# target limb -> Link limbs whose rotations it takes (composed in order); missing limbs keep 0
MAP_YOUNG = {
    0: [L_ROOT], 1: [L_UPPER], 2: [L_LSHOULDER], 3: [L_LARM], 4: [L_LHAND], 5: [L_RSHOULDER], 6: [L_RARM],
    7: [L_RHAND], 8: [L_HEAD], 15: [L_WAIST, L_LOWER], 16: [L_LTHIGH], 17: [L_LLEG], 18: [L_LFOOT],
    20: [L_RTHIGH], 21: [L_RLEG], 22: [L_RFOOT],
}
MAP_ADULT = {
    0: [L_ROOT], 1: [L_UPPER], 2: [L_LSHOULDER], 4: [L_LARM], 5: [L_LHAND], 6: [L_RSHOULDER], 8: [L_RARM],
    9: [L_RHAND], 10: [L_HEAD], 18: [L_WAIST, L_LOWER], 19: [L_LTHIGH], 20: [L_LLEG], 21: [L_LFOOT],
    22: [L_RTHIGH], 23: [L_RLEG], 24: [L_RFOOT],
}
LINK_LEG = 1306 + 1256

# Both Ganondorfs' limbs rest as Link's do (tools/ganon_rest_compare.py: a few degrees, pose only), so no limb needs a
# correction; corrections() stays for a model that would.
ADULT_REFERENCE = "objects/object_ganon_anime2/gGanondorfStandIdleAnim"
ADULT_CORRECTED = ()
ADULT_OWN = ()


def rot(r):
    return rg._rot("z", r[2]) @ rg._rot("y", r[1]) @ rg._rot("x", r[0])


def anim_frames(arc, name):
    r = rm.Reader(arc.read(name))
    r.take("I")
    return r.take("h")


def pose(arc, skel, rotations, translation, segments):
    """Triangles of skeleton `skel` in model space, each limb i rotated by rotations[i] (4x4)."""
    names, _ = rm.read_skeleton(arc, skel)
    limbs = [rm.read_limb(arc, n) for n in names]
    world = [None] * len(limbs)
    order = []

    def visit(i, parent):
        limb = limbs[i]
        t = np.eye(4, dtype=np.float32)
        t[:3, 3] = translation if i == 0 else limb["pos"]
        m = parent @ t @ rotations[i]
        world[i] = m
        order.append(i)
        if limb["child"] != 0xFF:
            visit(limb["child"], m)
        if limb["sibling"] != 0xFF:
            visit(limb["sibling"], parent)

    visit(0, np.eye(4, dtype=np.float32))
    matrices = {}
    k = 0
    for i in order:
        if limbs[i]["dl"]:
            matrices[0x0D000000 + k * 0x40] = world[i]
            k += 1
    tris = []
    for i in order:
        if limbs[i]["dl"]:
            tris += rg.run_dl(arc, limbs[i]["dl"], segments, [], matrices=matrices, initial=world[i])
    return tris


def leg_length(arc, skel, thigh_child):
    names, _ = rm.read_skeleton(arc, skel)
    limbs = [rm.read_limb(arc, n) for n in names]
    shin = limbs[thigh_child]
    foot = limbs[shin["child"]]
    return shin["pos"][0] + foot["pos"][0]


def to_view(tris):
    """Model space (Y up, Link facing +Z) -> the renderer's (front from +X, up -Y)."""
    for t in tris:
        t.p = np.stack([t.p[:, 2], -t.p[:, 1], t.p[:, 0]], axis=1)
        (kind, cv), base = t.col
        if kind == "n":
            cv = np.stack([cv[:, 2], -cv[:, 1], cv[:, 0]], axis=1) if isinstance(cv, np.ndarray) else \
                [np.array([c[2], -c[1], c[0]]) for c in cv]
            t.col = ((kind, cv), base)
    return tris


def corrections(arc, mapping, reference, corrected, own_limbs):
    """{limb: (correction 4x4 multiplied after Link's rotation, or None)}, {limb: own constant rotation}."""
    import ganon_rest_compare as rc

    link = rm.read_link_animation_frame(arc, "objects/gameplay_keep/gPlayerAnim_link_normal_wait", 0, 22)
    own, _ = rc.normal_anim_frame(arc, reference, 0)
    corr = {}
    for limb in corrected:
        m = np.eye(4, dtype=np.float32)
        for s in mapping[limb]:
            m = m @ rot(link[s + 1])
        corr[limb] = np.linalg.inv(m) @ rot(own[limb + 1])
    return corr, {limb: rot(own[limb + 1]) for limb in own_limbs}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", type=int, default=300)
    a = ap.parse_args()
    arc = rg.Archive(a.oot_o2r)

    run = "objects/gameplay_keep/gPlayerAnim_link_normal_run"
    swing = "objects/gameplay_keep/gPlayerAnim_link_fighter_normal_kiru"
    if swing not in arc.z.NameToInfo:
        swing = "objects/gameplay_keep/gPlayerAnim_link_fighter_Lnormal_kiru"
    poses = [("idle", "objects/gameplay_keep/gPlayerAnim_link_normal_wait", 0, 0),
             ("idle 3/4", "objects/gameplay_keep/gPlayerAnim_link_normal_wait", 0, 40),
             ("run", run, anim_frames(arc, run) // 4, 40),
             ("swing", swing, anim_frames(arc, swing) // 2, 40)]

    young_leg = leg_length(arc, YOUNG, 17)
    adult_leg = leg_length(arc, ADULT, 20)
    adult_corr = corrections(arc, MAP_ADULT, ADULT_REFERENCE, ADULT_CORRECTED, ADULT_OWN)
    models = [("adult Link", LINK, None, 1.0, {8: "objects/object_link_boy/gLinkAdultEyesOpenTex",
                                                9: "objects/object_link_boy/gLinkAdultMouth1Tex"}, ({}, {})),
              ("Hyrule-era Ganondorf", YOUNG, MAP_YOUNG, young_leg / LINK_LEG, {}, ({}, {})),
              ("tower Ganondorf", ADULT, MAP_ADULT, adult_leg / LINK_LEG, {}, adult_corr)]

    size = a.size
    sheet = Image.new("RGB", (size * len(poses), (size + 18) * len(models)), (30, 30, 36))
    draw = ImageDraw.Draw(sheet)
    for row, (label, skel, mapping, ratio, segs, (corr, own)) in enumerate(models):
        names, _ = rm.read_skeleton(arc, skel)
        n = len(names)
        rendered = []
        for (pname, anim, frame, yaw) in poses:
            joints = rm.read_link_animation_frame(arc, anim, frame, 22)
            if mapping is None:
                rots = [rot(joints[i + 1]) for i in range(n)]
            else:
                rots = [np.eye(4, dtype=np.float32) for _ in range(n)]
                for target, srcs in mapping.items():
                    m = np.eye(4, dtype=np.float32)
                    for s in srcs:
                        m = m @ rot(joints[s + 1])
                    rots[target] = m @ corr[target] if target in corr else m
                for limb, m in own.items():
                    rots[limb] = m
            translation = np.array(joints[0], dtype=np.float32) * ratio
            tris = to_view(pose(arc, skel, rots, translation, segs))
            rendered.append((pname, yaw, tris))
        pts = np.concatenate([t.p for _, _, tr in rendered for t in tr])
        lo, hi = pts.min(axis=0), pts.max(axis=0)
        center = (lo + hi) / 2
        scale = size * 0.9 / max(hi - lo)
        for col, (pname, yaw, tris) in enumerate(rendered):
            img = rg.render(tris, yaw, size, center, scale)
            sheet.paste(img, (col * size, row * (size + 18) + 18))
            draw.text((col * size + 4, row * (size + 18) + 3), "%s - %s" % (label, pname), fill=(230, 230, 230))
    sheet.save(a.out)
    print(a.out, "leg ratios: young %.2f adult %.2f" % (young_leg / LINK_LEG, adult_leg / LINK_LEG))


if __name__ == "__main__":
    main()
