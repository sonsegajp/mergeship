"""MergeShip2 -- Ganondorf mode's axe: the Iron Knuckle's own attack animations (object_ik) on the tower Ganondorf's
rig (tools/ganon_rig.py), as tools/ganon_retarget.py does for the bosses' animations. Previews the Iron Knuckle in its
own animation and the rig playing it, frame for frame, with the axe in his hand.

The pairing: an Iron Knuckle limb k on Link limb L as  LinkWorld[L] * C_k = IkWorld[k]; C_k is taken where both stand
the same way -- the Iron Knuckle's limbs given the rotations Link's idle gives the Link limbs they pair with (both
skeletons hold their bones along +X, the same way up), so C_k is the difference of the two hierarchies.

    python tools/ik_retarget.py --oot-o2r build/latest/oot.o2r --out build/ik_retarget.png [--inc out.inc]
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
import ganon_retarget as rt  # noqa: E402

L = gp
IK = "objects/object_ik/gIronKnuckleSkel"
AXE = "objects/object_ik/gIronKnuckleAxeDL"
AXE_LIMB = 16
# Link limb -> the Iron Knuckle limb whose rotation it takes
# (no head: the user, 2026-10-17, "wtf is that neck" -- the Iron Knuckle's head pivot stretched his collar; his head keeps
# its own carriage on the Iron Knuckle's chest)
CARRIERS = {L.L_WAIST: 1, L.L_UPPER: 9,
            L.L_RTHIGH: 2, L.L_RLEG: 3, L.L_RFOOT: 4, L.L_LTHIGH: 5, L.L_LLEG: 6, L.L_LFOOT: 7,
            L.L_RSHOULDER: 13, L.L_RARM: 14, L.L_RHAND: 15, L.L_LSHOULDER: 18, L.L_LARM: 19, L.L_LHAND: 20}
# extra turns (degrees about the IK limb's own X, Y, Z) to apply to a pairing, found by eye
TWEAK = {}


def ik_world(arc, joints):
    names, limbs = gr.skeleton(arc, IK)
    world, _ = gr.walk(limbs, lambda i, parent: parent @ gr.translate(
        np.array(joints[0], dtype=np.float32) if i == 0 else limbs[i]["pos"]) @ gp.rot(joints[i + 1]))
    return world


def link_parent(llimbs):
    parent = {}
    for i, limb in enumerate(llimbs):
        ch = limb["child"]
        while ch != 0xFF:
            parent[ch] = i
            ch = llimbs[ch]["sibling"]
    return parent


def pairing(arc, rig, mode="direct"):
    """C_k (rotation) for each carried Iron Knuckle limb."""
    if mode == "direct":
        c = {}
        for link, k in CARRIERS.items():
            m = np.eye(3)
            if k in TWEAK:
                m = gp.rot([int(v * 32768 / 180) for v in TWEAK[k]])[:3, :3]
            c[k] = m
        return c
    names, limbs = gr.skeleton(arc, IK)
    idle = rm.read_link_animation_frame(arc, gr.IDLE, 0, 22)
    by_ik = {k: link for link, k in CARRIERS.items()}
    joints = [[0, 0, 0]] + [list(idle[by_ik[i] + 1]) if i in by_ik else [0, 0, 0] for i in range(len(limbs))]
    joints[1] = list(idle[1])  # the root turns as Link's
    world = ik_world(arc, joints)
    c = {}
    for link, k in CARRIERS.items():
        lw = rig["bind"][link][:3, :3]
        lw = lw / np.cbrt(np.linalg.det(lw))
        m = np.linalg.inv(lw) @ world[k][:3, :3]
        if k in TWEAK:
            m = m @ gp.rot([int(v * 32768 / 180) for v in TWEAK[k]])[:3, :3]
        c[k] = m
    return c


def retarget(arc, rig, c, joints, root_scale):
    world = ik_world(arc, joints)
    target = {link: world[k][:3, :3] @ np.linalg.inv(c[k]) for link, k in CARRIERS.items()}
    llimbs = rig["limbs"]
    parent = link_parent(llimbs)
    out = [[0, 0, 0] for _ in range(22)]
    out[0] = [int(round(v)) for v in np.array(joints[0], dtype=np.float32) / root_scale]
    worldrot = {0: np.eye(3)}

    def visit(i):
        p = parent.get(i)
        pw = worldrot[p] if p is not None else np.eye(3)
        local = np.linalg.inv(pw) @ target[i] if i in target else np.eye(3)
        worldrot[i] = pw @ local
        out[i + 1] = rt.euler_zyx(local)
        ch = llimbs[i]["child"]
        while ch != 0xFF:
            visit(ch)
            ch = llimbs[ch]["sibling"]

    ch = llimbs[0]["child"]
    while ch != 0xFF:
        visit(ch)
        ch = llimbs[ch]["sibling"]
    return out, world


# The arms (the user, 2026-10-17: "his arms are way too narrow and close together", "the shoulders look awkward"): the
# Iron Knuckle's wide body angles its arms in to meet on the handle; those angles on his narrower shoulders cross his
# forearms. So each arm reaches instead: the hand where the Iron Knuckle's is (from the chest, at his size), the elbow
# solved (two bones, his own lengths) bending the way the Iron Knuckle's bends; the hand keeps its turn (the axe's).
ARMS = ((L.L_RSHOULDER, L.L_RARM, L.L_RHAND, 13, 14, 15), (L.L_LSHOULDER, L.L_LARM, L.L_LHAND, 18, 19, 20))
CHEST = (L.L_UPPER, 9)


def align(r, v):
    """r turned the least so that its +X points along v."""
    x = r[:, 0] / np.linalg.norm(r[:, 0])
    v = v / np.linalg.norm(v)
    axis = np.cross(x, v)
    sn = np.linalg.norm(axis)
    cs = float(np.dot(x, v))
    if sn < 1e-6:
        return r
    axis /= sn
    k = np.array([[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]])
    q = np.eye(3) + k * sn + (k @ k) * (1 - cs)
    return q @ r


def reach_arms(rig, link, ikw, k):
    """`link` (Link joints) with the arms re-solved to reach the Iron Knuckle's hands; ikw: its world matrices."""
    llimbs = rig["limbs"]
    parent = link_parent(llimbs)
    w = link_world(rig, link)
    rot = {i: w[i][:3, :3] / np.cbrt(np.linalg.det(w[i][:3, :3])) for i in range(len(llimbs))}
    chest_g = w[CHEST[0]][:3, 3]
    chest_k = ikw[CHEST[1]][:3, 3]
    out = [list(v) for v in link]
    for ls, la, lh, ks, ka, kh in ARMS:
        s = w[ls][:3, 3]
        a = float(np.linalg.norm(rig["offsets"][la]))
        b = float(np.linalg.norm(rig["offsets"][lh]))
        hand = chest_g + (ikw[kh][:3, 3] - chest_k) * k
        elbow_k = ikw[ka][:3, 3]
        mid_k = (ikw[ks][:3, 3] + ikw[kh][:3, 3]) / 2
        pole = elbow_k - mid_k
        d = hand - s
        dist = float(np.linalg.norm(d))
        dist_c = min(max(dist, abs(a - b) + 1e-3), (a + b) * 0.999)
        n = d / dist
        p = pole - n * np.dot(pole, n)
        if np.linalg.norm(p) < 1e-6:
            p = np.cross(n, np.array([0.0, 1.0, 0.0]))
        p /= np.linalg.norm(p)
        cos_a = (a * a + dist_c * dist_c - b * b) / (2 * a * dist_c)
        sin_a = np.sqrt(max(0.0, 1 - cos_a * cos_a))
        e = s + n * a * cos_a + p * a * sin_a
        hand_c = s + n * dist_c
        rs_ = align(rot[ls], e - s)
        ra_ = align(rot[la], hand_c - e)
        rh_ = rot[lh]
        for limb, r in ((ls, rs_), (la, ra_), (lh, rh_)):
            rot[limb] = r
        for limb in (ls, la, lh):
            local = np.linalg.inv(rot[parent[limb]]) @ rot[limb]
            out[limb + 1] = rt.euler_zyx(local)
    return out


def ik_root_scale(arc, rig):
    """Iron Knuckle root translation over the rig's (pelvis heights: its walk's first frame over Link's idle)."""
    own, _ = rc.normal_anim_frame(arc, "objects/object_ik/gIronKnuckleWalkAnim", 0)
    idle = rm.read_link_animation_frame(arc, gr.IDLE, 0, 22)
    return float(own[0][1]) / float(idle[0][1])


def link_world(rig, link):
    llimbs, offsets, scale = rig["limbs"], rig["offsets"], rig["scale"]

    def local(i, parent):
        if i == 0:
            return gr.translate(np.array(link[0], dtype=np.float32) * scale) @ gp.rot(link[1])
        return parent @ gr.translate(offsets[i]) @ gp.rot(link[i + 1])

    w, _ = gr.walk(llimbs, local)
    return w


def axe_scale(arc, rig):
    """The axe on him as on the Iron Knuckle: his height over the Iron Knuckle's (both in their walk / idle)."""
    names, _ = rm.read_skeleton(arc, IK)
    own, _ = rc.normal_anim_frame(arc, "objects/object_ik/gIronKnuckleWalkAnim", 0)
    ik = gp.pose(arc, IK, [gp.rot(own[i + 1]) for i in range(len(names))], np.array(own[0], dtype=np.float32), {})
    idle = rm.read_link_animation_frame(arc, gr.IDLE, 0, 22)
    return gr.idle_height(gr.draw(rig, idle)) / gr.idle_height(ik)


def axe_tris(arc, rig, link, s):
    hand = link_world(rig, link)[L.L_RHAND]
    rot = hand[:3, :3] / np.cbrt(np.linalg.det(hand[:3, :3]))
    m = np.eye(4, dtype=np.float32)
    m[:3, :3] = rot * s
    m[:3, 3] = hand[:3, 3]
    return rg.run_dl(arc, AXE, {}, [], initial=m)


def export_inc(arc, rig, c, rs, axs, path):
    """The pairing as C data for soh/MergeShip/MergeShipGanondorf.c (its MergeShip_Ganondorf_ApplyPose: the Iron
    Knuckle's parents, the Link limbs carrying its limbs with C_k^-1, its root translation's scale) and the axe's size
    in his right hand limb's space (game units: the rig's TARGET sizing, tools/ganon_rig.py export_c)."""
    names, limbs = gr.skeleton(arc, IK)
    parent = [255] * len(limbs)
    for i, limb in enumerate(limbs):
        ch = limb["child"]
        while ch != 0xFF:
            parent[ch] = i
            ch = limbs[ch]["sibling"]
    idle = rm.read_link_animation_frame(arc, gr.IDLE, 0, 22)
    link_tris = gp.pose(arc, gp.LINK, [gp.rot(idle[i + 1]) for i in range(21)], np.array(idle[0], dtype=np.float32), {})
    s_ = gr.TARGET * gr.idle_height(link_tris) / gr.idle_height(gr.draw(rig, idle))
    out = ["// MergeShip2 -- generated by tools/ik_retarget.py (Ganondorf mode's axe: the Iron Knuckle's animations on the",
           "// tower Ganondorf's rig); do not edit.", "",
           "#define MS_IK_LIMBS %d" % len(limbs),
           "static const u8 sGanondorfIkBossParents[MS_IK_LIMBS] = { %s };" % ", ".join(str(v) for v in parent),
           "static const MsGanondorfCarrier sGanondorfIkCarriers[] = {"]
    for link, k in sorted(CARRIERS.items()):
        inv = np.linalg.inv(c[k])
        out.append("    { %d, %d, { %s } }," % (link + 1, k, ", ".join("%.6ff" % inv[a, b] for a in range(3) for b in range(3))))
    out.append("};")
    out.append("static const f32 sGanondorfIkRetargetRoot = %.6ff; // its root translation over this: Link's" % rs)
    # his fists on its hands (tools/ik_sheets.py hand_maps): the Iron Knuckle's hand space in his hand limbs' (game units)
    # -- its fist's middle on his, turned as his fist -- by which the axe is drawn in his right hand and its hands' origins
    # are reached for
    import ik_sheets as sh
    maps = sh.hand_maps(arc, rig, axs)
    h = maps[15][1].copy()
    h[:3, :] *= s_
    out.append("static const MtxF sGanondorfIkAxeMtx = { { // the axe (its right hand's space) in his right hand limb's")
    for col in range(4):
        out.append("    { %s }," % ", ".join("%.6ff" % h[r, col] for r in range(4)))
    out.append("} };")
    out.append("static const Vec3f sGanondorfIkHandOrigin[2] = { { %.2ff, %.2ff, %.2ff }, { %.2ff, %.2ff, %.2ff } }; // right, left" %
               tuple(float(v) for kh in (15, 20) for v in maps[kh][1][:3, 3] * s_))
    # his own limbs (the head, the torso, those it carries none of) held as in Link's idle while its animations play:
    # Link's own animation there (his walk's bob) on its swaying chest shook his head (the user, 2026-10-17)
    idle_j = rm.read_link_animation_frame(arc, gr.IDLE, 0, 22)
    out.append("static const Vec3s sGanondorfIkOwnJoints[PLAYER_LIMB_MAX] = { %s };" %
               ", ".join("{ %d, %d, %d }" % tuple(int(v) for v in idle_j[i]) for i in range(22)))
    # its limbs' offsets (its units), for the arms' reach (the hands where its hands are: reach_arms)
    out.append("static const Vec3f sGanondorfIkBossLimbPos[MS_IK_LIMBS] = {")
    for limb in limbs:
        out.append("    { %.1ff, %.1ff, %.1ff }," % tuple(float(v) for v in limb["pos"]))
    out.append("};")
    out.append("static const f32 sGanondorfIkAxeScale = %.6ff; // gIronKnuckleAxeDL in his right hand limb's space" %
               (axs * s_))
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--inc")
    ap.add_argument("--reach", type=int, default=1)
    ap.add_argument("--yaw", type=float, default=30.0)
    ap.add_argument("--anims", default="VerticalAttack:0/8/14/20/28,HorizontalAttack:0/8/14/22,Walk:0/8,Run:0/6")
    a = ap.parse_args()
    arc = rg.Archive(a.oot_o2r)
    rig = rt.rig_with_c(arc, "tower")
    c = pairing(arc, rig)
    rs = ik_root_scale(arc, rig)
    axs = axe_scale(arc, rig)
    import ik_sheets as sh
    for kh, (r_f, _) in sh.hand_maps(arc, rig, axs).items():
        c[kh] = r_f  # (his fist turns as its hand: LinkWorld[hand] * R_f = IkWorld[hand])
    print("axe scale", axs)
    if a.inc:
        export_inc(arc, rig, c, rs, axs, a.inc)
    names, _ = rm.read_skeleton(arc, IK)
    tests = []
    for spec in a.anims.split(","):
        anim, frames = spec.split(":")
        tests.append(("objects/object_ik/gIronKnuckle%sAnim" % anim, [int(f) for f in frames.split("/")]))
    size = 240
    cols = sum(len(f) for _, f in tests)
    sheet = Image.new("RGB", (size * cols, (size + 16) * (3 if a.reach else 2)), (30, 30, 36))
    d = ImageDraw.Draw(sheet)
    col = 0
    for anim, frames in tests:
        for f in frames:
            own, _ = rc.normal_anim_frame(arc, anim, f)
            ik = gp.to_view(gp.pose(arc, IK, [gp.rot(own[i + 1]) for i in range(len(names))],
                                    np.array(own[0], dtype=np.float32), {}))
            link, ikw = retarget(arc, rig, c, own, rs)
            rows = [("ik", ik), ("copied", gp.to_view(gr.draw(rig, link) + axe_tris(arc, rig, link, axs)))]
            if a.reach:
                reached = reach_arms(rig, link, ikw, axs)
                rows.append(("reach", gp.to_view(gr.draw(rig, reached) + axe_tris(arc, rig, reached, axs))))
            for r, (label, tris) in enumerate(rows):
                pts = np.concatenate([t.p for t in tris])
                lo, hi = pts.min(axis=0), pts.max(axis=0)
                sheet.paste(rg.render(tris, a.yaw, size, (lo + hi) / 2, size * 0.85 / max(hi - lo)),
                            (col * size, r * (size + 16) + 16))
                d.text((col * size + 3, r * (size + 16) + 2), "%s %s f%d" % (label, anim.split("Knuckle")[-1][:16], f),
                       fill=(230, 230, 230))
            col += 1
    sheet.save(a.out)
    print(a.out, "root scale", rs)


if __name__ == "__main__":
    main()
