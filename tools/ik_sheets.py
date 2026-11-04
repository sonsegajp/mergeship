"""MergeShip2 -- Ganondorf mode's axe: frame-by-frame sheets of the Iron Knuckle and Ganondorf side by side, Ganondorf
posed exactly as the game poses him (soh MergeShipGanondorf.c MergeShip_Ganondorf_ApplyPose's Iron Knuckle branch and
MergeShip_Ganondorf_IkReach, mirrored here), so the arms can be judged and fixed against the Iron Knuckle's own.

    python tools/ik_sheets.py --oot-o2r build/latest/oot.o2r --out-dir build/ik_sheets [--pole upper] [--widen 1.5]

One sheet per animation: the Iron Knuckle on top, Ganondorf under it, every `--step` frames, both from the same camera.
"""

import argparse
import copy
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
import ganon_retarget as rt  # noqa: E402
import ik_retarget as ik  # noqa: E402

L = gp
ANIMS = ["Walk", "Run", "VerticalAttack", "AxeStuck", "RecoverFromVerticalAttack", "HorizontalAttack",
         "RecoverFromHorizontalAttack"]


IK_HAND_DL = {15: "objects/object_ik/object_ik_DL_016118", 20: "objects/object_ik/object_ik_DL_016340"}


def mesh_mid(arc, dl):
    pts = np.concatenate([t.p for t in rg.run_dl(arc, dl, {}, [], initial=np.eye(4, dtype=np.float32))])
    return (pts.min(axis=0) + pts.max(axis=0)) / 2


def hand_maps(arc, rig, k):
    """Per hand (Iron Knuckle hand limb 15 / 20): R_f, his fist model's turn in his hand limb (the pairing's C, so his fist
    turns as the Iron Knuckle's hand does), and H, the Iron Knuckle's hand space in his hand limb's (rig units): its fist's
    middle on his fist's middle, turned as his fist, at his size (the axe is drawn by it; its hand's origin goes by it)."""
    out = {}
    for kh, g, link in ((15, 9, L.L_RHAND), (20, 5, L.L_LHAND)):
        cmat = np.linalg.inv(rig["bind"][link]) @ rig["gworld"][g]
        sig = np.cbrt(np.linalg.det(cmat[:3, :3]))
        r_f = cmat[:3, :3] / sig
        g_mid = (cmat @ np.append(mesh_mid(arc, gr.skeleton(arc, gp.ADULT)[1][g]["dl"]), 1.0))[:3]
        ik_mid = mesh_mid(arc, IK_HAND_DL[kh])
        h = np.eye(4)
        h[:3, :3] = r_f * k
        h[:3, 3] = g_mid - r_f @ (ik_mid * k)
        out[kh] = (r_f, h)
    return out


def solve_arm(rot, pos, offsets, ls, la, lh, wrist, pole):
    a = float(np.linalg.norm(offsets[la]))
    b = float(np.linalg.norm(offsets[lh]))
    d = wrist - pos[ls]
    dist = float(np.linalg.norm(d))
    if dist < 1e-3:
        return pos[lh]
    dist_c = min(max(dist, abs(a - b) + 1e-3), (a + b) * 0.999)
    n = d / dist
    p = pole - n * np.dot(pole, n)
    if np.linalg.norm(p) < 1e-6:
        p = np.array([n[2], 0.0, -n[0]])
    p = p / np.linalg.norm(p)
    cos_a = (a * a + dist_c * dist_c - b * b) / (2 * a * dist_c)
    sin_a = np.sqrt(max(0.0, 1 - cos_a * cos_a))
    e = pos[ls] + n * a * cos_a + p * a * sin_a
    out = pos[ls] + n * dist_c
    rot[ls] = ik.align(rot[ls], e - pos[ls])
    rot[la] = ik.align(rot[la], out - e)
    return out


def ganon_pose(arc, rig, idle, own, opts):
    """Link joints for Ganondorf in Iron Knuckle frame `own`, as the game makes them; and the rig they are drawn on."""
    c = ik.pairing(arc, rig)
    maps = opts["maps"]
    for kh in (15, 20):
        c[kh] = maps[kh][0]  # (his fist turns as its hand: LinkWorld[hand] * R_f = IkWorld[hand])
    link, ikw = ik.retarget(arc, rig, c, own, opts["rs"])
    has = {limb for limb in ik.CARRIERS}
    for i in range(21):
        if (i != 0) and (i not in has):
            link[i + 1] = list(idle[i + 1])  # (his own: the head's carriage)
    if opts.get("head") == "ik":
        c2 = dict(c)
        c2[10] = np.eye(3)
        saved = dict(ik.CARRIERS)
        ik.CARRIERS[L.L_HEAD] = 10
        link2, _ = ik.retarget(arc, rig, c2, own, opts["rs"])
        ik.CARRIERS.clear()
        ik.CARRIERS.update(saved)
        link[L.L_HEAD + 1] = link2[L.L_HEAD + 1]
    elif opts.get("head") == "level":
        # the head's world turn: the chest's facing only (its yaw), upright as in his idle
        w0 = ik.link_world(rig, link)
        up = w0[L.L_UPPER][:3, :3] / np.cbrt(np.linalg.det(w0[L.L_UPPER][:3, :3]))
        idle_w = ik.link_world(rig, idle)
        ih = idle_w[L.L_HEAD][:3, :3] / np.cbrt(np.linalg.det(idle_w[L.L_HEAD][:3, :3]))
        root = w0[0][:3, :3] / np.cbrt(np.linalg.det(w0[0][:3, :3]))
        # the chest's forward (its +Y in his rig is ~ facing; use the chest's local x projected) -> yaw about world Y
        fwd = up[:, 1]
        yaw = np.arctan2(fwd[0], fwd[2])
        iup = idle_w[L.L_UPPER][:3, :3] / np.cbrt(np.linalg.det(idle_w[L.L_UPPER][:3, :3]))
        ifwd = iup[:, 1]
        yaw -= np.arctan2(ifwd[0], ifwd[2])
        cy, sy = np.cos(yaw), np.sin(yaw)
        ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
        target = ry @ ih
        link[L.L_HEAD + 1] = rt.euler_zyx(np.linalg.inv(up) @ target)
    rig_w = dict(rig)
    rig_w["offsets"] = {i: np.array(v, dtype=np.float64) for i, v in rig["offsets"].items()}
    for s_ in (L.L_LSHOULDER, L.L_RSHOULDER):
        rig_w["offsets"][s_][2] *= opts["widen"]
    if not opts["reach"]:
        return link, rig_w
    w = ik.link_world(rig_w, link)
    llimbs = rig["limbs"]
    parent = ik.link_parent(llimbs)
    rot = {i: w[i][:3, :3] / np.cbrt(np.linalg.det(w[i][:3, :3])) for i in range(len(llimbs))}
    pos = {i: w[i][:3, 3] for i in range(len(llimbs))}
    k = opts["k"]
    P = {i: ikw[i][:3, 3] for i in range(len(ikw))}

    def pole_of(ks, ka, kh):
        if opts["pole"] == "upper":
            return P[ka] - P[ks]  # the Iron Knuckle's upper arm's direction
        return P[ka] - (P[ks] + P[kh]) / 2  # its elbow off the shoulder-hand line

    # the Iron Knuckle's right hand origin (its hand space's 0), on him: by H, from his wrist
    q_r = maps[15][1][:3, 3]
    q_l = maps[20][1][:3, 3]
    wrist = pos[L.L_UPPER] + (P[15] - P[9]) * k - rot[L.L_RHAND] @ q_r
    reached = solve_arm(rot, pos, rig_w["offsets"], L.L_RSHOULDER, L.L_RARM, L.L_RHAND, wrist, pole_of(13, 14, 15))
    # its left hand origin where it is from its right's, on his axe (its right hand's frame is his fist's: rot @ R_f)
    origin_r = reached + rot[L.L_RHAND] @ q_r
    grip = origin_r + (P[20] - P[15]) * k
    wrist_l = grip - rot[L.L_LHAND] @ q_l
    solve_arm(rot, pos, rig_w["offsets"], L.L_LSHOULDER, L.L_LARM, L.L_LHAND, wrist_l, pole_of(18, 19, 20))
    out = [list(v) for v in link]
    for limb in (L.L_RSHOULDER, L.L_RARM, L.L_RHAND, L.L_LSHOULDER, L.L_LARM, L.L_LHAND):
        out[limb + 1] = rt.euler_zyx(np.linalg.inv(rot[parent[limb]]) @ rot[limb])
    return out, rig_w


def axe_on(arc, rig_w, link, h):
    hand = ik.link_world(rig_w, link)[L.L_RHAND]
    rot = hand[:3, :3] / np.cbrt(np.linalg.det(hand[:3, :3]))
    m = np.eye(4)
    m[:3, :3] = rot
    m[:3, 3] = hand[:3, 3]
    return rg.run_dl(arc, ik.AXE, {}, [], initial=(m @ h).astype(np.float32))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--anims", default=",".join(ANIMS))
    ap.add_argument("--step", type=int, default=3)
    ap.add_argument("--pole", default="upper")
    ap.add_argument("--widen", type=float, default=1.5)
    ap.add_argument("--reach", type=int, default=1)
    ap.add_argument("--snap", type=int, default=1)
    ap.add_argument("--yaw", type=float, default=35.0)
    ap.add_argument("--size", type=int, default=220)
    ap.add_argument("--cols", type=int, default=8)
    ap.add_argument("--upper", type=int, default=0)
    ap.add_argument("--head", default="idle")
    a = ap.parse_args()
    arc = rg.Archive(a.oot_o2r)
    rig = rt.rig_with_c(arc, "tower")
    idle = rm.read_link_animation_frame(arc, gr.IDLE, 0, 22)
    opts = {"rs": ik.ik_root_scale(arc, rig), "k": ik.axe_scale(arc, rig), "widen": a.widen, "pole": a.pole,
            "reach": a.reach, "snap": a.snap, "head": a.head}
    opts["maps"] = hand_maps(arc, rig, opts["k"])
    names, _ = rm.read_skeleton(arc, ik.IK)
    os.makedirs(a.out_dir, exist_ok=True)
    for anim in a.anims.split(","):
        path = "objects/object_ik/gIronKnuckle%sAnim" % anim
        _, count = rc.normal_anim_frame(arc, path, 0)
        frames = [(f, rc.normal_anim_frame(arc, path, f)[0]) for f in range(0, count, a.step)]
        size = a.size
        per = a.cols
        rows = (len(frames) + per - 1) // per
        sheet = Image.new("RGB", (size * min(per, len(frames)), (size + 14) * 2 * rows), (30, 30, 36))
        d = ImageDraw.Draw(sheet)
        for n, (f, own) in enumerate(frames):
            col = n % per
            band = (n // per) * 2
            ikt = gp.to_view(gp.pose(arc, ik.IK, [gp.rot(own[i + 1]) for i in range(len(names))],
                                     np.array(own[0], dtype=np.float32), {}))
            link, rig_w = ganon_pose(arc, rig, idle, own, opts)
            gt = gp.to_view(gr.draw(rig_w, link) + axe_on(arc, rig_w, link, opts["maps"][15][1]))
            for r, tris in enumerate((ikt, gt)):
                pts = np.concatenate([t.p for t in tris])
                lo, hi = pts.min(axis=0), pts.max(axis=0)
                center = (lo + hi) / 2
                zoom = 0.88 / max(hi - lo)
                if a.upper:
                    # (the upper body: the box's top half, twice as near)
                    center[1] = lo[1] + (hi[1] - lo[1]) * 0.3
                    zoom *= 1.7
                sheet.paste(rg.render(tris, a.yaw, size, center, size * zoom),
                            (col * size, (band + r) * (size + 14) + 14))
                d.text((col * size + 3, (band + r) * (size + 14) + 1), "%s f%d" % ("IK" if r == 0 else "Ganon", f),
                       fill=(230, 230, 230))
        out = os.path.join(a.out_dir, "%s.png" % anim)
        sheet.save(out)
        print(out)


if __name__ == "__main__":
    main()
