"""MergeShip2 -- Ganondorf mode: Ocarina of Time's Ganondorfs rigged onto Link's skeleton, previewed through Link's own
animations.

    python tools/ganon_rig.py --oot-o2r build/latest/oot.o2r --out build/ganon_rig.png [--json build/ganon_rig.json]

The rig, per model:
  1. Bind pose. Ganondorf is posed once in Link's idle (frame 0 of gPlayerAnim_link_normal_wait): his limbs take
     Link's idle joint rotations (both Ganondorfs' limbs rest as Link's do), so every bone points where Link's does.
     The tower Ganondorf's face and hair limbs (Link has none) keep his own rest (gGanondorfStandIdleAnim); his shoulder
     pads too, and his upper arms -- the joint Link's shoulder stands for -- take Link's shoulder rotation undone by the
     pad's, so the arm points where Link's does.
  2. Skeleton. Link's adult skeleton -- his 21 limbs, his hierarchy, so every Link animation drives it unchanged --
     with Ganondorf's proportions: each joint offset is solved so that, in Link's idle (frame 0 of
     gPlayerAnim_link_normal_wait), the joint sits exactly on Ganondorf's own joint in the bind pose (hips, knees,
     ankles, shoulders, elbows, wrists, neck). The root's translation from Link's animations is scaled so the pelvis is
     at Ganondorf's height.
  3. Skin. Every vertex of his limb lists is bound to a Link limb (by the Ganondorf limb whose matrix placed it, mapped
     onto Link's limbs: face and hair onto the head, the shoulder pads and upper arms onto the shoulders, and so on), in
     that limb's space at the bind pose. Drawing the skeleton in any Link animation then moves every part with its Link
     bone, pivoting at Ganondorf's own joints -- and at Link's idle reproduces the bind pose exactly.
Rows: adult Link (reference), the Hyrule-era Ganondorf, the tower Ganondorf; each drawn at his own size.
"""

import argparse
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402
import rip_models as rm  # noqa: E402
import render_ganon_preview as gp  # noqa: E402
import ganon_rest_compare as rc  # noqa: E402

IDLE = "objects/gameplay_keep/gPlayerAnim_link_normal_wait"
L = gp  # Link limb index constants (L_ROOT ...)

PHANTOM = "objects/object_gnd/gPhantomGanonSkel"
# Phantom Ganon (the Phantom Ganon mask, Ganondorf mode): the Hyrule-era Ganondorf's skeleton with pivot limbs (no
# list) at the joints and the list limbs under them at 0; his spear on limb 12, his head (the mask) on limb 14.
MAP_PHANTOM = {
    0: [L.L_ROOT], 1: [L.L_UPPER], 3: [L.L_LSHOULDER], 5: [L.L_LARM], 6: [L.L_LHAND], 7: [L.L_RSHOULDER],
    9: [L.L_RARM], 10: [L.L_RHAND], 14: [L.L_HEAD], 15: [L.L_WAIST, L.L_LOWER], 17: [L.L_LTHIGH], 19: [L.L_LLEG],
    20: [L.L_LFOOT], 21: [L.L_RTHIGH], 23: [L.L_RLEG], 24: [L.L_RFOOT],
}

# Link limb -> the Ganondorf limb whose joint it sits on
JOINTS = {
    "phantom": {L.L_WAIST: 15, L.L_RTHIGH: 21, L.L_RLEG: 23, L.L_RFOOT: 24, L.L_LTHIGH: 17, L.L_LLEG: 19,
                L.L_LFOOT: 20, L.L_UPPER: 1, L.L_HEAD: 14, L.L_LSHOULDER: 3, L.L_LARM: 5, L.L_LHAND: 6,
                L.L_RSHOULDER: 7, L.L_RARM: 9, L.L_RHAND: 10},
    "young": {L.L_WAIST: 15, L.L_RTHIGH: 20, L.L_RLEG: 21, L.L_RFOOT: 22, L.L_LTHIGH: 16, L.L_LLEG: 17, L.L_LFOOT: 18,
              L.L_UPPER: 1, L.L_HEAD: 8, L.L_LSHOULDER: 2, L.L_LARM: 3, L.L_LHAND: 4, L.L_RSHOULDER: 5, L.L_RARM: 6,
              L.L_RHAND: 7},
    "tower": {L.L_WAIST: 18, L.L_RTHIGH: 22, L.L_RLEG: 23, L.L_RFOOT: 24, L.L_LTHIGH: 19, L.L_LLEG: 20, L.L_LFOOT: 21,
              L.L_UPPER: 1, L.L_HEAD: 10, L.L_LSHOULDER: 2, L.L_LARM: 4, L.L_LHAND: 5, L.L_RSHOULDER: 6, L.L_RARM: 8,
              L.L_RHAND: 9},
}
# Ganondorf limb -> the Link limb its vertices are bound to
SKIN = {
    "phantom": {4: L.L_LSHOULDER, 5: L.L_LARM, 6: L.L_LHAND, 8: L.L_RSHOULDER, 9: L.L_RARM, 10: L.L_RHAND,
                12: L.L_RHAND, 13: L.L_TORSO, 14: L.L_HEAD, 16: L.L_WAIST, 18: L.L_LTHIGH, 19: L.L_LLEG,
                20: L.L_LFOOT, 22: L.L_RTHIGH, 23: L.L_RLEG, 24: L.L_RFOOT},
    "young": {1: L.L_TORSO, 2: L.L_LSHOULDER, 3: L.L_LARM, 4: L.L_LHAND, 5: L.L_RSHOULDER, 6: L.L_RARM, 7: L.L_RHAND,
              8: L.L_HEAD, 9: L.L_HEAD, 10: L.L_HEAD, 11: L.L_HEAD, 12: L.L_HEAD, 13: L.L_HEAD, 14: L.L_HEAD,
              15: L.L_WAIST, 16: L.L_LTHIGH, 17: L.L_LLEG, 18: L.L_LFOOT, 19: L.L_WAIST, 20: L.L_RTHIGH,
              21: L.L_RLEG, 22: L.L_RFOOT},
    "tower": {1: L.L_TORSO, 2: L.L_LSHOULDER, 3: L.L_LSHOULDER, 4: L.L_LARM, 5: L.L_LHAND, 6: L.L_RSHOULDER,
              7: L.L_RSHOULDER, 8: L.L_RARM, 9: L.L_RHAND, 10: L.L_HEAD, 11: L.L_HEAD, 12: L.L_HEAD, 13: L.L_HEAD,
              14: L.L_HEAD, 15: L.L_HEAD, 16: L.L_HEAD, 17: L.L_HEAD, 18: L.L_WAIST, 19: L.L_LTHIGH, 20: L.L_LLEG,
              21: L.L_LFOOT, 22: L.L_RTHIGH, 23: L.L_RLEG, 24: L.L_RFOOT},
}


# tower Ganondorf limbs bound at his own rest relative to their parent: his hands and feet sit on the wrist and ankle
# at other angles than Link's (a quarter turn; the feet tilt), so Link's angles twist the fists and tilt the boots
TOWER_OWN_REST = [21, 24]
# tower Ganondorf limbs posed to face exactly as the Hyrule-era Ganondorf's do in the bind pose, by matching the two
# meshes (tools/ganon_align.py): his fists (a tight fit; they sit about 121 degrees from the Hyrule-era fists).
# tower limb -> Hyrule-era limb
TOWER_ALIGNED = {5: 4, 9: 7}


def skeleton(arc, skel):
    names, _ = rm.read_skeleton(arc, skel)
    return names, [rm.read_limb(arc, n) for n in names]


def walk(limbs, local):
    """World matrices (depth first order) of a skeleton whose limb i has local matrix local(i, parent_world)."""
    world = [None] * len(limbs)
    order = []

    def visit(i, parent):
        world[i] = local(i, parent)
        order.append(i)
        if limbs[i]["child"] != 0xFF:
            visit(limbs[i]["child"], world[i])
        if limbs[i]["sibling"] != 0xFF:
            visit(limbs[i]["sibling"], parent)

    visit(0, np.eye(4, dtype=np.float32))
    return world, order


def translate(p):
    t = np.eye(4, dtype=np.float32)
    t[:3, 3] = p
    return t


def bind_pose(arc, kind):
    """Ganondorf's world limb matrices and triangles (with each vertex's Ganondorf limb) in the bind pose."""
    skel = {"young": gp.YOUNG, "tower": gp.ADULT, "phantom": PHANTOM}[kind]
    names, limbs = skeleton(arc, skel)
    link = rm.read_link_animation_frame(arc, IDLE, 0, 22)
    mapping = {"young": gp.MAP_YOUNG, "tower": gp.MAP_ADULT, "phantom": MAP_PHANTOM}[kind]
    rots = [np.eye(4, dtype=np.float32) for _ in limbs]
    for target, srcs in mapping.items():
        m = np.eye(4, dtype=np.float32)
        for s in srcs:
            m = m @ gp.rot(link[s + 1])
        rots[target] = m
    if kind == "young":
        # his face (lips, jaw, chin, teeth, head) at its own rest, from his walk: at 0 the jaw tucks into his neck
        own, _ = rc.normal_anim_frame(arc, "objects/object_gndd/gYoungGanondorfWalkAnim", 0)
        for limb in (9, 10, 11, 12, 13, 14):
            rots[limb] = gp.rot(own[limb + 1])
    if kind == "tower":
        own, _ = rc.normal_anim_frame(arc, "objects/object_ganon_anime2/gGanondorfStandIdleAnim", 0)
        for limb in (11, 12, 13, 14, 15, 16, 17):  # lips, mouth, jaw, hair, head: his own rest
            rots[limb] = gp.rot(own[limb + 1])
        # his hands sit on the wrist about a quarter turn from Link's (tools/ganon_rest_compare.py): bound at his own
        # wrist angle, or the fists come out twisted on the forearm
        for limb in TOWER_OWN_REST:
            rots[limb] = gp.rot(own[limb + 1])
        for pad, arm, src in ((2, 3, L.L_LSHOULDER), (6, 7, L.L_RSHOULDER)):
            rots[pad] = gp.rot(own[pad + 1])
            rots[arm] = np.linalg.inv(rots[pad]) @ gp.rot(link[src + 1])
    root = np.array(link[0], dtype=np.float32) * (2.0 if kind == "tower" else 1.61)
    world, order = walk(limbs, lambda i, parent: parent @ translate(root if i == 0 else limbs[i]["pos"]) @ rots[i])
    if kind == "tower" and TOWER_ALIGNED:
        import ganon_align as ga

        young_world, _ = bind_pose(arc, "young")
        fits = ga.alignments(arc)
        parent_of = {}
        for i, limb in enumerate(limbs):
            c = limb["child"]
            while c != 0xFF:
                parent_of[c] = i
                c = limbs[c]["sibling"]
        for t, y in TOWER_ALIGNED.items():
            a = np.eye(4, dtype=np.float32)
            a[:3, :3] = fits[t][0]
            want = young_world[y] @ a  # the Hyrule-era limb's facing, carried over to the tower mesh
            parent = world[parent_of[t]]
            rots[t] = np.eye(4, dtype=np.float32)
            rots[t][:3, :3] = np.linalg.inv(parent[:3, :3]) @ want[:3, :3]
        world, order = walk(limbs, lambda i, parent: parent @ translate(root if i == 0 else limbs[i]["pos"]) @ rots[i])
    matrices, by_addr = {}, {}
    k = 0
    for i in order:
        if limbs[i]["dl"]:
            matrices[0x0D000000 + k * 0x40] = world[i]
            by_addr[0x0D000000 + k * 0x40] = i
            k += 1
    tris = []
    for i in order:
        if not limbs[i]["dl"]:
            continue
        for t in rg.run_dl(arc, limbs[i]["dl"], {}, [], matrices=matrices, initial=world[i]):
            owners = [by_addr.get(m, i) if m is not None else i for m in (t.mids or [None, None, None])]
            tris.append((t, owners))
    return world, tris


def link_rig(arc, kind):
    """The Ganondorf-proportioned Link skeleton (limb offsets, root scale) and the skin (triangles in Link limb space)."""
    gworld, gtris = bind_pose(arc, kind)
    lnames, llimbs = skeleton(arc, gp.LINK)
    idle = rm.read_link_animation_frame(arc, IDLE, 0, 22)
    joints = JOINTS[kind]
    # root: Link's idle translation scaled so the pelvis lands at Ganondorf's
    pelvis = gworld[joints[L.L_WAIST]][:3, 3]
    scale = float(pelvis[1] / idle[0][1])
    offsets = {}

    def local(i, parent):
        if i == 0:
            return translate(np.array(idle[0], dtype=np.float32) * scale) @ gp.rot(idle[1])
        if i in joints:
            target = gworld[joints[i]][:3, 3]
            off = np.linalg.inv(parent[:3, :3]) @ (target - parent[:3, 3])
        else:
            off = np.zeros(3, dtype=np.float32)  # a control or empty limb: on its parent's joint
        offsets[i] = off
        return parent @ translate(off) @ gp.rot(idle[i + 1])

    bind, _ = walk(llimbs, local)
    skin = SKIN[kind]
    bound = []
    for t, owners in gtris:
        limbs = [skin.get(o, L.L_TORSO) for o in owners]
        inv = [np.linalg.inv(bind[l]) for l in limbs]
        p = np.stack([(inv[v] @ np.append(t.p[v], 1.0))[:3] for v in range(3)])
        (kind_, cv), base = t.col
        if kind_ == "n":
            cv = [inv[v][:3, :3] @ np.asarray(cv[v], dtype=np.float32) for v in range(3)]
        bound.append((t, limbs, p, ((kind_, cv), base)))
    return {"offsets": offsets, "scale": scale, "limbs": llimbs, "skin": bound, "gworld": gworld, "bind": bind,
            "kind": kind}


def idle_height(tris):
    pts = np.concatenate([t.p for t in tris])
    return float(pts[:, 1].max() - pts[:, 1].min())


TARGET = 1.3  # in-game height of both Ganondorfs, in adult Link's heights (bigger than adult Link, same in both eras)


def export_c(arc, rigs, path):
    """The rig as C data for soh/MergeShip/MergeShipGanondorf.c: per model, Link's 21 joint offsets (Ganondorf-sized),
    the root translation scale, and per Ganondorf limb with a display list (in his flexible skeleton's matrix order,
    segment 0x0D) the Link limb it rides and its constant matrix C (Ganondorf limb space -> that Link limb's space at
    the bind pose), all scaled to TARGET adult Link heights."""
    idle = rm.read_link_animation_frame(arc, IDLE, 0, 22)
    link_tris = gp.pose(arc, gp.LINK, [gp.rot(idle[i + 1]) for i in range(21)], np.array(idle[0], dtype=np.float32), {})
    link_h = idle_height(link_tris)
    out = ["// MergeShip2 -- generated by tools/ganon_rig.py (Ganondorf mode); do not edit.",
           "// Adult Link's idle height %.0f model units; both Ganondorfs drawn %.2f times it." % (link_h, TARGET), ""]
    for kind in ("young", "tower", "phantom"):
        rig = rigs[kind]
        s_ = TARGET * link_h / idle_height(draw(rig, idle))
        skel = {"young": gp.YOUNG, "tower": gp.ADULT, "phantom": PHANTOM}[kind]
        names, glimbs = skeleton(arc, skel)
        _, order = walk(glimbs, lambda i, parent: parent)
        name = {"young": "Young", "tower": "Tower", "phantom": "Phantom"}[kind]
        out.append("static const f32 sGanondorf%sRootScale = %.6ff;" % (name, rig["scale"] * s_))
        out.append("static const Vec3f sGanondorf%sJoints[PLAYER_LIMB_MAX - 1] = {" % name)
        for i in range(21):
            o = rig["offsets"].get(i, np.zeros(3)) * s_
            out.append("    { %.3ff, %.3ff, %.3ff }, // %s" % (o[0], o[1], o[2], "root (unused)" if i == 0 else
                                                             rm.read_skeleton(arc, gp.LINK)[0][i].split("/")[-1]))
        out.append("};")
        out.append("static const MsGanondorfPart sGanondorf%sParts[] = {" % name)
        for i in order:
            if not glimbs[i]["dl"]:
                continue
            link = SKIN[kind].get(i, L.L_TORSO)
            c = np.linalg.inv(rig["bind"][link]) @ rig["gworld"][i]
            sc = np.diag([s_, s_, s_, 1.0]).astype(np.float32)
            c = sc @ c
            mf = ", ".join("%.6ff" % c[r, col] for col in range(4) for r in range(4))  # MtxF: column by column
            out.append("    { \"__OTR__%s\", %d, %d, { %s } }, // %s" % (glimbs[i]["dl"], i, link + 1, mf,
                                                                       names[i].split("/")[-1]))
        out.append("};")
        out.append("")
        if kind in ("tower", "phantom"):
            export_retarget(arc, rig, kind, name, glimbs, out)
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(out))


def export_retarget(arc, rig, kind, name, glimbs, out):
    """The boss's own animations on the rig (MergeShipGanondorf.c MsGanondorf_Retarget, tools/ganon_retarget.py):
    his limbs' parents, the Link limbs carrying his limbs with the inverse of each pairing's rotation (C_g^-1), and the
    root translation's scale."""
    import ganon_retarget as rt

    parent = [255] * len(glimbs)
    for i, limb in enumerate(glimbs):
        c = limb["child"]
        while c != 0xFF:
            parent[c] = i
            c = glimbs[c]["sibling"]
    out.append("static const u8 sGanondorf%sBossParents[%d] = { %s };" % (name, len(glimbs),
                                                                         ", ".join(str(v) for v in parent)))
    out.append("static const MsGanondorfCarrier sGanondorf%sCarriers[] = {" % name)
    for link, g in sorted(rt.CARRIERS[kind].items()):
        c = np.linalg.inv(rig["bind"][SKIN[kind].get(g, L.L_TORSO)]) @ rig["gworld"][g]
        r = c[:3, :3] / np.cbrt(np.linalg.det(c[:3, :3]))
        inv = np.linalg.inv(r)
        out.append("    { %d, %d, { %s } }," % (link + 1, g, ", ".join("%.6ff" % inv[a, b] for a in range(3) for b in range(3))))
    out.append("};")
    out.append("static const f32 sGanondorf%sRetargetRoot = %.6ff; // his root translation over this: Link's" % (
        name, rig["scale"]))
    # positions too (tools/ganon_retarget.py retarget_full): his limbs' offsets, and the boss joint each Link joint is
    # put on each frame (the rig's own pairing), so a pose that bends his spine (Phantom Ganon's charge) holds
    out.append("static const Vec3f sGanondorf%sBossLimbPos[%d] = {" % (name, len(glimbs)))
    for limb in glimbs:
        out.append("    { %.1ff, %.1ff, %.1ff }," % tuple(float(v) for v in limb["pos"]))
    out.append("};")
    jmap = [255] * 22
    for link, g in JOINTS[kind].items():
        jmap[link + 1] = g
    out.append("static const u8 sGanondorf%sJointOn[PLAYER_LIMB_MAX] = { %s }; // Link limb -> boss limb (255: none)" % (
        name, ", ".join(str(v) for v in jmap)))
    if kind == "phantom":
        out.append("static const Vec3f sGanondorfPhantomSpearPivot = { %.1ff, %.1ff, %.1ff }; // limb 11 on limb 10" %
                   tuple(float(v) for v in glimbs[11]["pos"]))
    out.append("")


def draw(rig, joints_frame):
    """Triangles of the rigged Ganondorf in a Link animation frame."""
    llimbs, offsets, scale = rig["limbs"], rig["offsets"], rig["scale"]

    def local(i, parent):
        if i == 0:
            return translate(np.array(joints_frame[0], dtype=np.float32) * scale) @ gp.rot(joints_frame[1])
        return parent @ translate(offsets[i]) @ gp.rot(joints_frame[i + 1])

    world, _ = walk(llimbs, local)
    out = []
    for t, limbs, p, col in rig["skin"]:
        q = rg.Tri(np.stack([(world[limbs[v]] @ np.append(p[v], 1.0))[:3] for v in range(3)]), t.uv, col, t.tex,
                   t.wrap, t.alpha, t.comb, t.prim, t.env, t.key)
        (kind_, cv), base = col
        if kind_ == "n":
            q.col = ((kind_, [world[limbs[v]][:3, :3] @ cv[v] for v in range(3)]), base)
        out.append(q)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--json")
    ap.add_argument("--c-out", help="write the rig as C data (soh/MergeShip/MergeShipGanondorfRig.inc)")
    ap.add_argument("--size", type=int, default=300)
    a = ap.parse_args()
    arc = rg.Archive(a.oot_o2r)

    def mid(name):
        return gp.anim_frames(arc, name) // 2

    run = "objects/gameplay_keep/gPlayerAnim_link_normal_run"
    swing = "objects/gameplay_keep/gPlayerAnim_link_fighter_Lnormal_kiru"
    shield = "objects/gameplay_keep/gPlayerAnim_link_anchor_waitR"
    poses = [("idle", IDLE, 0, 20), ("idle side", IDLE, 0, 90), ("run", run, mid(run), 40),
             ("swing", swing, mid(swing), 40)]
    if shield in arc.z.NameToInfo:
        poses.append(("target", shield, 0, 40))

    rigs = {"young": link_rig(arc, "young"), "tower": link_rig(arc, "tower"), "phantom": link_rig(arc, "phantom")}
    size = a.size
    rows = [("adult Link", None), ("Hyrule-era Ganondorf", "young"), ("tower Ganondorf", "tower"),
            ("Phantom Ganon", "phantom")]
    sheet = Image.new("RGB", (size * len(poses), (size + 18) * len(rows)), (30, 30, 36))
    d = ImageDraw.Draw(sheet)
    for r, (label, kind) in enumerate(rows):
        rendered = []
        for pname, anim, frame, yaw in poses:
            jf = rm.read_link_animation_frame(arc, anim, frame, 22)
            if kind is None:
                n = 21
                tris = gp.pose(arc, gp.LINK, [gp.rot(jf[i + 1]) for i in range(n)], np.array(jf[0], dtype=np.float32),
                               {8: "objects/object_link_boy/gLinkAdultEyesOpenTex",
                                9: "objects/object_link_boy/gLinkAdultMouth1Tex"})
            else:
                tris = draw(rigs[kind], jf)
            rendered.append((pname, yaw, gp.to_view(tris)))
        pts = np.concatenate([t.p for _, _, tr in rendered for t in tr])
        lo, hi = pts.min(axis=0), pts.max(axis=0)
        for c, (pname, yaw, tris) in enumerate(rendered):
            sheet.paste(rg.render(tris, yaw, size, (lo + hi) / 2, size * 0.9 / max(hi - lo)),
                        (c * size, r * (size + 18) + 18))
            d.text((c * size + 4, r * (size + 18) + 3), "%s - %s" % (label, pname), fill=(230, 230, 230))
    sheet.save(a.out)
    if a.json:
        json.dump({k: {"scale": v["scale"], "offsets": {str(i): [float(x) for x in o] for i, o in v["offsets"].items()}}
                   for k, v in rigs.items()}, open(a.json, "w"), indent=1)
    if a.c_out:
        export_c(arc, rigs, a.c_out)
        print("wrote", a.c_out)
    for k, v in rigs.items():
        print(k, "root scale %.2f" % v["scale"])
    print(a.out)


if __name__ == "__main__":
    main()
