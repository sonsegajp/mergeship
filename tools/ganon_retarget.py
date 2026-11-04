"""MergeShip2 -- Ganondorf mode: the bosses' own animations as Link animations, for the rigs on Link's skeleton
(tools/ganon_rig.py). The game does the same at run time (soh MergeShipGanondorfAnims.c); this is its proof, with
previews: the boss in his own animation, and the rig playing the converted animation, frame for frame.

The rig puts a Ganondorf limb g on Link limb L as  LinkWorld[L] * C_g = BossWorld[g]  (C_g: the rig's constant). So a
boss pose is a Link pose with  LinkWorld[L] = BossWorld[g] * C_g^-1  (rotations; the joints stay the rig's), for
each Link limb carrying one of his limbs; Link's control limbs, the hat, collar and sheath keep no rotation of their
own, and so does the root (his pelvis and torso turn instead). The root's translation is his, over the rig's root
scale.

    python tools/ganon_retarget.py --oot-o2r build/latest/oot.o2r --out build/ganon_retarget.png
"""

import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402
import rip_models as rm  # noqa: E402
import render_ganon_preview as gp  # noqa: E402
import ganon_rest_compare as rc  # noqa: E402
import ganon_rig as gr  # noqa: E402

L = gp
# Link limb -> the boss limb whose rotation it takes
CARRIERS = {
    "tower": {L.L_WAIST: 18, L.L_UPPER: 1, L.L_HEAD: 10, L.L_LSHOULDER: 3, L.L_LARM: 4, L.L_LHAND: 5,
              L.L_RSHOULDER: 7, L.L_RARM: 8, L.L_RHAND: 9, L.L_LTHIGH: 19, L.L_LLEG: 20, L.L_LFOOT: 21,
              L.L_RTHIGH: 22, L.L_RLEG: 23, L.L_RFOOT: 24},
    "phantom": {L.L_WAIST: 16, L.L_UPPER: 13, L.L_HEAD: 14, L.L_LSHOULDER: 4, L.L_LARM: 5, L.L_LHAND: 6,
                L.L_RSHOULDER: 8, L.L_RARM: 9, L.L_RHAND: 10, L.L_LTHIGH: 18, L.L_LLEG: 19, L.L_LFOOT: 20,
                L.L_RTHIGH: 22, L.L_RLEG: 23, L.L_RFOOT: 24},
}
# the Link limb whose bind frame the carrier's C was made against (the upper control's: the torso limb under it)
C_LIMB = {L.L_UPPER: L.L_TORSO}
SKELS = {"tower": gp.ADULT, "phantom": gr.PHANTOM}


def boss_world(arc, skel, joints):
    names, limbs = gr.skeleton(arc, skel)
    world, _ = gr.walk(limbs, lambda i, parent: parent @ gr.translate(np.array(joints[0], dtype=np.float32) if i == 0
                                                                       else limbs[i]["pos"]) @ gp.rot(joints[i + 1]))
    return world


def euler_zyx(m):
    sy = -m[2, 0]
    y = np.arcsin(np.clip(sy, -1, 1))
    x = np.arctan2(m[2, 1], m[2, 2])
    z = np.arctan2(m[1, 0], m[0, 0])
    return [int(round(a * 32768 / np.pi)) for a in (x, y, z)]


def retarget_frame(arc, kind, rig, joints):
    """Link joint values (22 x 3) reproducing boss pose `joints` (his own animation's) on the rig."""
    world = boss_world(arc, SKELS[kind], joints)
    llimbs = rig["limbs"]
    # each carrier's world rotation: BossWorld[g] * C_g^-1 (rotation parts; C carries the rig's scale)
    target = {}
    for link, g in CARRIERS[kind].items():
        c = rig["c"][g][:3, :3]
        c = c / np.cbrt(np.linalg.det(c))
        target[link] = world[g][:3, :3] @ np.linalg.inv(c)
    parent = {}
    for i, limb in enumerate(llimbs):
        ch = limb["child"]
        while ch != 0xFF:
            parent[ch] = i
            ch = llimbs[ch]["sibling"]
    out = [[0, 0, 0] for _ in range(22)]
    root = np.array(joints[0], dtype=np.float32) / rig["scale_root"]
    out[0] = [int(round(v)) for v in root]
    worldrot = {0: np.eye(3)}

    def visit(i):
        p = parent.get(i)
        pw = worldrot[p] if p is not None else np.eye(3)
        if i in target:
            local = np.linalg.inv(pw) @ target[i]
        else:
            local = np.eye(3)
        worldrot[i] = pw @ local
        out[i + 1] = euler_zyx(local)
        ch = llimbs[i]["child"]
        while ch != 0xFF:
            visit(ch)
            ch = llimbs[ch]["sibling"]

    out[1] = [0, 0, 0]
    ch = llimbs[0]["child"]
    while ch != 0xFF:
        visit(ch)
        ch = llimbs[ch]["sibling"]
    return out


def rig_with_c(arc, kind):
    rig = gr.link_rig(arc, kind)
    names, glimbs = gr.skeleton(arc, SKELS[kind])
    rig["c"] = {}
    for g in range(len(glimbs)):
        link = gr.SKIN[kind].get(g, L.L_TORSO)
        rig["c"][g] = np.linalg.inv(rig["bind"][link]) @ rig["gworld"][g]
    rig["scale_root"] = rig["scale"]
    return rig


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--anims", default="")
    a = ap.parse_args()
    arc = rg.Archive(a.oot_o2r)
    ap2 = a.anims
    tests = [("tower", "objects/object_ganon_anime1/gGanondorfChargeLightBallAnim", [0, 12, 24]),
             ("tower", "objects/object_ganon_anime1/gGanondorfThrowAnim", [5, 11, 18]),
             ("tower", "objects/object_ganon_anime1/gGanondorfPoundAnim", [0, 6]),
             ("phantom", "objects/object_gnd/gPhantomGanonThrowAnim", [5, 25])]
    if ap2:
        tests = []
        for spec in ap2.split(","):
            kind, anim, frames = spec.split(":")
            tests.append((kind, anim, [int(f) for f in frames.split("/")]))
    size = 260
    cols = sum(len(f) for _, _, f in tests)
    sheet = Image.new("RGB", (size * cols, (size + 16) * 2), (30, 30, 36))
    d = ImageDraw.Draw(sheet)
    col = 0
    rigs = {}
    for kind, anim, frames in tests:
        if kind not in rigs:
            rigs[kind] = rig_with_c(arc, kind)
        rig = rigs[kind]
        names, _ = rm.read_skeleton(arc, SKELS[kind])
        for f in frames:
            own, _ = rc.normal_anim_frame(arc, anim, f)
            boss = gp.to_view(gp.pose(arc, SKELS[kind], [gp.rot(own[i + 1]) for i in range(len(names))],
                                      np.array(own[0], dtype=np.float32), {}))
            link = retarget_frame(arc, kind, rig, own)
            mine = gp.to_view(gr.draw(rig, link))
            for r, (label, tris) in enumerate((("boss", boss), ("rig", mine))):
                pts = np.concatenate([t.p for t in tris])
                lo, hi = pts.min(axis=0), pts.max(axis=0)
                sheet.paste(rg.render(tris, 30, size, (lo + hi) / 2, size * 0.9 / max(hi - lo)),
                            (col * size, r * (size + 16) + 16))
                d.text((col * size + 3, r * (size + 16) + 2), "%s %s f%d" % (label, anim.split("/")[-1][:22], f),
                       fill=(230, 230, 230))
            col += 1
    sheet.save(a.out)
    print(a.out)


if __name__ == "__main__":
    main()
