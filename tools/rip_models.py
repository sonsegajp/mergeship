"""MergeShip2 -- rips models from the games' archives into a Blender scene: meshes with their UVs, vertex colours and
normals, and their textures (PNG, packed into the .blend), via tools/build_blend.py run by Blender headless.

    python tools/rip_models.py --mm-o2r build/host10/mm.o2r --oot-o2r build/host10/oot.o2r --out build/rips \
        [--blender "C:/Program Files (x86)/Steam/steamapps/common/Blender/blender.exe"]

What it rips:
  - Ocarina of Time's adult Link (objects/object_link_boy/gLinkAdultSkel): every limb's near display list, posed by
    the first frame of his idle animation (gPlayerAnim_link_normal_wait), each limb its own object; the skinned
    vertices that hang on a neighbouring limb's matrix (the neck, shoulders, hips) placed by that limb, as the game's
    flexible skeleton does (SkelAnime_DrawFlexLod: one matrix per limb with a list, depth first, segment 0x0D).
    Eyes open, mouth closed, the Kokiri Tunic's green.
  - Majora's Mask's Skull Kid's Link mask (mm/objects/object_stk/gSkullKidLinkMask1DL, worn; 2 and 3, the ones he
    holds), each in its own space.

Each Link gets an armature (tools/rig_blend.py): a bone per limb at its joint, in the skeleton's hierarchy, pointing
along the limb's own +X (the way the games' limbs chain) with its roll from the limb's +Z, so the games' joint
rotations are the bones'. Each limb's object is bound to its bone; the skinned vertices that the game places with a
neighbouring limb's matrix (the neck on the torso, the shoulders, the hips) are bound to that limb's bone instead.

    python tools/rip_models.py ... --rig-blend build/rips/edited.blend --name edited_rigged.blend
rigs an existing .blend instead (one edited in Blender: its meshes are kept as they are; a vertex the edit added or
moved is bound to its object's bone).

Display lists are read by tools/render_mask_grid.py's reader (F3DEX2 with resource hashes: vertices, triangles,
textures with their palettes, wrap modes, the colour combiner, lighting or vertex colours).
"""

import argparse
import json
import math
import os
import struct
import subprocess
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402


class Reader:
    def __init__(self, data, offset=0x40):
        self.d = data
        self.o = offset

    def take(self, fmt):
        v = struct.unpack_from("<" + fmt, self.d, self.o)
        self.o += struct.calcsize("<" + fmt)
        return v[0] if len(v) == 1 else v

    def string(self):
        n = self.take("I")
        s = self.d[self.o:self.o + n].decode("utf-8", "replace")
        self.o += n
        return s


def read_skeleton(arc, name):
    r = Reader(arc.read(name))
    r.take("b")  # type
    r.take("b")  # limb type
    limb_count = r.take("I")
    r.take("I")  # display list count
    r.take("b")  # limb table type
    n = r.take("I")
    return [r.string().replace("__OTR__", "") for _ in range(n)], limb_count


def read_limb(arc, name):
    r = Reader(arc.read(name))
    limb = {"type": r.take("b")}
    r.take("b")  # skin segment type
    r.string()  # skin list
    r.take("H")  # skin vertex count
    if r.take("I") != 0:
        raise ValueError(f"{name}: skin limbs are not read")
    r.string()  # skin list 2
    r.take("fff")  # leg translation
    r.take("HHH")  # rotation
    r.string()  # child
    r.string()  # sibling
    limb["dl"] = r.string().replace("__OTR__", "")
    limb["dl2"] = r.string().replace("__OTR__", "")
    limb["pos"] = r.take("hhh")
    limb["child"] = r.take("B")
    limb["sibling"] = r.take("B")
    return limb


def read_link_animation_frame(arc, name, frame, limbs):
    r = Reader(arc.read(name))
    r.take("I")  # animation type (Link)
    r.take("h")  # frame count
    data = resolve(arc, r.string())
    d = Reader(arc.read(data))
    count = d.take("I")
    values = struct.unpack_from("<%dh" % count, d.d, d.o)
    stride = limbs * 3 + 1
    f = values[frame * stride:(frame + 1) * stride]
    return [tuple(f[i * 3:i * 3 + 3]) for i in range(limbs)]


def mat_translate_rotate_zyx(pos, rot):
    t = np.eye(4, dtype=np.float32)
    t[:3, 3] = pos
    return t @ rg._rot("z", rot[2]) @ rg._rot("y", rot[1]) @ rg._rot("x", rot[0])


def resolve(arc, path):
    """A resource path as a skeleton names it ("objects/..."), as the archive holds it (Majora's Mask's under "mm/")."""
    path = path.replace("__OTR__", "")
    if not path or path in arc.cache or path in arc.z.NameToInfo:
        return path
    return "mm/" + path if ("mm/" + path) in arc.z.NameToInfo else path


def rip_skeleton(arc, skel, anim, segments, rig=None):
    """Every limb's near display list of flexible skeleton `skel`, posed by the first frame of player animation `anim`,
    as (limb name, triangles). `rig`, a dict, receives the armature: "bones" (name, parent, joint, the limb's +X and
    +Z, the bone's length; model space) and "skin" (object name -> [(vertex position, bone)] for the vertices another
    limb's matrix places)."""
    limb_names, limb_count = read_skeleton(arc, skel)
    limb_names = [resolve(arc, n) for n in limb_names]
    limbs = [read_limb(arc, n) for n in limb_names]
    for limb in limbs:
        limb["dl"] = resolve(arc, limb["dl"])
    joints = read_link_animation_frame(arc, anim, 0, 22)

    world = [None] * len(limbs)
    order = []  # depth first, as SkelAnime_DrawFlexLod numbers the matrices

    def visit(i, parent):
        limb = limbs[i]
        if i == 0:
            m = mat_translate_rotate_zyx(joints[0], joints[1])
        else:
            m = parent @ mat_translate_rotate_zyx(limb["pos"], joints[i + 1])
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

    def bone_name(i):
        return limb_names[i].split("/")[-1].replace("Limb", "")

    parent = [None] * len(limbs)
    for i, limb in enumerate(limbs):
        c = limb["child"]
        while c != 0xFF:
            parent[c] = i
            c = limbs[c]["sibling"]
    by_addr = {}
    k = 0
    for i in order:
        if limbs[i]["dl"]:
            by_addr[0x0D000000 + k * 0x40] = i
            k += 1

    objects = []
    for i in order:
        dl = limbs[i]["dl"]
        if not dl:
            continue
        tris = rg.run_dl(arc, dl, segments, [], matrices=matrices, initial=world[i])
        objects.append((bone_name(i), tris))
        if rig is not None:
            skin = rig.setdefault("skin", {}).setdefault(bone_name(i), [])
            for t in tris:
                for v in range(3):
                    j = by_addr.get(t.mids[v]) if t.mids is not None else None
                    if (j is not None) and (j != i):
                        skin.append(([float(c) for c in t.p[v]], bone_name(j)))

    if rig is not None:
        bones = []
        for i in order:
            m = world[i]
            joint = m[:3, 3]
            x_axis, z_axis = m[:3, 0] / np.linalg.norm(m[:3, 0]), m[:3, 2] / np.linalg.norm(m[:3, 2])
            # as long as the way to its first child, when that is along the limb (else a short bone)
            length = 0.0
            if limbs[i]["child"] != 0xFF:
                along = float(np.dot(world[limbs[i]["child"]][:3, 3] - joint, x_axis))
                length = along if along > 1.0 else 0.0
            bones.append({"name": bone_name(i), "parent": None if parent[i] is None else bone_name(parent[i]),
                          "joint": [float(c) for c in joint], "x": [float(c) for c in x_axis],
                          "z": [float(c) for c in z_axis], "length": length})
        rig["bones"] = bones
    return objects


def rip_adult_link(oot, rig=None):
    return rip_skeleton(oot, "objects/object_link_boy/gLinkAdultSkel", "objects/gameplay_keep/gPlayerAnim_link_normal_wait",
                        {8: "objects/object_link_boy/gLinkAdultEyesOpenTex",
                         9: "objects/object_link_boy/gLinkAdultMouth1Tex"}, rig)


def rip_young_link(mm, rig=None):
    return rip_skeleton(mm, "mm/objects/object_link_child/gLinkHumanSkel",
                        "mm/objects/gameplay_keep/gPlayerAnim_link_normal_wait",
                        {8: "mm/objects/object_link_child/gLinkHumanEyesOpenTex",
                         9: "mm/objects/object_link_child/gLinkHumanMouthClosedTex"}, rig)


def rig_to_blender(rig, scale, offset, prefix=""):
    """An armature from rip_skeleton's `rig`, in the .blend's space (export's: N64 Y-up -> Blender Z-up, scaled and
    moved as the meshes are), bone names (and the objects they bind) with `prefix`."""
    def pos(p):
        return [p[0] * scale + offset[0], -p[2] * scale + offset[1], p[1] * scale + offset[2]]

    def vec(v):
        return [v[0], -v[2], v[1]]

    return {
        "bones": [{"name": prefix + b["name"], "parent": None if b["parent"] is None else prefix + b["parent"],
                   "head": pos(b["joint"]), "y": vec(b["x"]), "z": vec(b["z"]),
                   "length": max(b["length"] * scale, 0.04)} for b in rig["bones"]],
        "skin": {prefix + obj: [[pos(p), prefix + bone] for p, bone in verts] for obj, verts in rig["skin"].items()},
    }


def rip_link_masks(mm):
    out = []
    for n in (1, 2, 3):
        name = f"mm/objects/object_stk/gSkullKidLinkMask{n}DL"
        tris = rg.run_dl(mm, name, {}, [])
        out.append((f"SkullKidLinkMask{n}", tris))
    return out


def export(groups, out_dir, scale):
    """groups: {collection: [(object name, tris, offset (blender units))]} -> scene.json + textures."""
    tex_dir = os.path.join(out_dir, "textures")
    os.makedirs(tex_dir, exist_ok=True)
    materials = []
    mat_index = {}
    textures = {}
    scene = {"collections": []}
    for coll, objs in groups.items():
        cobjs = []
        for oname, tris, offset in objs:
            pos, uv, col, nrm, mats = [], [], [], [], []
            for t in tris:
                image = None
                if t.tex is not None and t.key is not None:
                    if t.key not in textures:
                        fname = "%03d_%s.png" % (len(textures), t.key[0].split("/")[-1])
                        arr = (np.clip(t.tex, 0, 1) * 255).astype(np.uint8)
                        Image.fromarray(arr, "RGBA").save(os.path.join(tex_dir, fname))
                        textures[t.key] = fname
                    image = textures[t.key]
                one = np.ones((1, 3))
                base = rg.combine(t.comb, one, one, t.prim, t.env)[0] if t.comb is not None else np.ones(3)
                lit = t.col[0][0] == "n"
                alpha = bool(t.tex is not None and (t.tex[..., 3] < 0.99).any())
                mkey = (image, tuple(np.round(base, 3)), tuple(t.wrap), lit, alpha)
                if mkey not in mat_index:
                    mat_index[mkey] = len(materials)
                    materials.append({"name": "mat%03d_%s" % (len(materials), (image or "flat").split(".")[0]),
                                      "image": image, "base": [float(v) for v in base], "wrap": list(t.wrap),
                                      "lit": lit, "alpha": alpha})
                mats.append(mat_index[mkey])
                for v in range(3):
                    x, y, z = (float(c) for c in t.p[v])
                    # N64 Y-up -> Blender Z-up
                    pos.append([x * scale + offset[0], -z * scale + offset[1], y * scale + offset[2]])
                    uv.append([float(t.uv[v, 0]), 1.0 - float(t.uv[v, 1])])
                    if lit:
                        n = t.col[0][1][v]
                        n = n / (np.linalg.norm(n) + 1e-6)
                        nrm.append([float(n[0]), float(-n[2]), float(n[1])])
                        col.append([1.0, 1.0, 1.0])
                    else:
                        nrm.append(None)
                        col.append([float(c) for c in t.col[0][1][v]])
            cobjs.append({"name": oname, "pos": pos, "uv": uv, "col": col, "nrm": nrm, "mat": mats})
        scene["collections"].append({"name": coll, "objects": cobjs})
    scene["materials"] = materials
    scene["texture_dir"] = os.path.abspath(tex_dir).replace("\\", "/")
    path = os.path.join(out_dir, "scene.json")
    json.dump(scene, open(path, "w"))
    return path, len(textures)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mm-o2r", required=True)
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--blender", default="C:/Program Files (x86)/Steam/steamapps/common/Blender/blender.exe")
    ap.add_argument("--name", default="mergeship_rips.blend")
    ap.add_argument("--rig-blend", help="rig this existing .blend (edited in Blender) instead of ripping a new one")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    mm = rg.Archive(args.mm_o2r)
    oot = rg.Archive(args.oot_o2r)
    adult_rig, young_rig = {}, {}
    link = rip_adult_link(oot, adult_rig)
    young = rip_young_link(mm, young_rig)
    masks = rip_link_masks(mm)

    pts = np.concatenate([t.p for _, tris in link for t in tris])
    height = pts[:, 1].max() - pts[:, 1].min()
    scale = float(1.75 / height)  # adult Link about 1.75 m
    print(f"adult Link: {sum(len(t) for _, t in link)} triangles in {len(link)} limbs, height {height:.0f} units")
    print(f"young Link (MM): {sum(len(t) for _, t in young)} triangles in {len(young)} limbs")
    def floor(objs):  # how far up to move a Link so his soles are on the floor (z = 0)
        return float(-np.concatenate([t.p for _, tris in objs for t in tris])[:, 1].min() * scale)

    groups = {
        "Adult Link (OoT)": [(n, t, (0.0, 0.0, floor(link))) for n, t in link],
        # On adult Link's left as the camera sees them, at the same scale (their heights compare).
        "Young Link (MM)": [("MM_" + n, t, (-1.1, 0.0, floor(young))) for n, t in young],
        "Skull Kid Link Mask (MM)": [(n, t, (1.2 + i * 0.6, 0.0, 1.4)) for i, (n, t) in enumerate(masks)],
    }
    for n, t in masks:
        print(f"{n}: {len(t)} triangles")
    rigs = {"Adult Link (OoT)": rig_to_blender(adult_rig, scale, (0.0, 0.0, floor(link))),
            "Young Link (MM)": rig_to_blender(young_rig, scale, (-1.1, 0.0, floor(young)), "MM_")}
    rig_path = os.path.join(args.out, "rig.json")
    json.dump(rigs, open(rig_path, "w"))
    blend = os.path.abspath(os.path.join(args.out, args.name))
    here = os.path.dirname(os.path.abspath(__file__))
    if args.rig_blend:
        source = os.path.abspath(args.rig_blend)
    else:
        scene, ntex = export(groups, args.out, scale)
        print(f"wrote {scene} ({ntex} textures)")
        res = subprocess.run([args.blender, "--background", "--factory-startup", "--python",
                              os.path.join(here, "build_blend.py"), "--", scene, blend], capture_output=True, text=True)
        print(res.stdout[-1500:])
        if res.returncode != 0 or not os.path.exists(blend):
            print(res.stderr[-3000:])
            sys.exit(1)
        source = blend
    res = subprocess.run([args.blender, "--background", "--factory-startup", source, "--python",
                          os.path.join(here, "rig_blend.py"), "--", rig_path, blend], capture_output=True, text=True)
    print(res.stdout[-2500:])
    if res.returncode != 0 or not os.path.exists(blend):
        print(res.stderr[-3000:])
        sys.exit(1)
    print("wrote", blend)


if __name__ == "__main__":
    main()
