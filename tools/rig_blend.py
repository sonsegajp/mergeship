"""MergeShip2 -- Blender side of tools/rip_models.py's armatures: rigs the Links of an open .blend (one rip_models.py
built, or one edited in Blender since) and saves it as a new file.

    blender --background in.blend --python tools/rig_blend.py -- rig.json out.blend

rig.json maps each Link's collection to its armature (rip_models.py rig_to_blender): bones at the limbs' joints, in the
skeleton's hierarchy, pointing along each limb's +X with its roll from the limb's +Z; and the skinned vertices, which
the game places with a neighbouring limb's matrix. Each mesh of the collection is parented to the armature and bound to
its limb's bone -- by its name, or, renamed in an edit (gLinkTorso for gLinkAdultTorso), by the limb word its name ends
with -- every vertex weighted 1.0: to the neighbouring limb's bone where it is one of the skinned vertices (matched by
position), else to its own. What an edit added or moved (a belt on the torso) goes with its object's bone.
"""

import json
import re
import sys

import bpy
from mathutils import Vector, kdtree

SKIN_MATCH = 0.0005  # Blender units: rip_models' vertices are welded at 0.0001


def limb_word(name):
    """'MM_gLinkHumanTorso', 'gLinkAdultTorso', 'gLinkTorso' -> 'Torso'."""
    name = re.sub(r"\.\d+$", "", name)  # Blender's duplicate suffix
    name = name[3:] if name.startswith("MM_") else name
    return re.sub(r"^g?Link(Adult|Human)?", "", name)


def build_armature(coll, rig):
    name = coll.name + " Armature"
    old = bpy.data.objects.get(name)
    if old is not None:
        bpy.data.objects.remove(old)
    data = bpy.data.armatures.new(name)
    arm = bpy.data.objects.new(name, data)
    coll.objects.link(arm)
    arm.show_in_front = True
    data.display_type = "STICK"

    for o in bpy.context.view_layer.objects.selected:
        o.select_set(False)
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    for b in rig["bones"]:
        eb = data.edit_bones.new(b["name"])
        head = Vector(b["head"])
        eb.head = head
        eb.tail = head + Vector(b["y"]).normalized() * b["length"]
        eb.align_roll(Vector(b["z"]))
    for b in rig["bones"]:
        if b["parent"] is not None:
            data.edit_bones[b["name"]].parent = data.edit_bones[b["parent"]]
            data.edit_bones[b["name"]].use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    arm.select_set(False)
    return arm


def bind(obj, arm, rig):
    bones = {b["name"] for b in rig["bones"]}
    own = obj.name if obj.name in bones else None
    if own is None:
        word = limb_word(obj.name)
        matches = [b for b in bones if limb_word(b) == word]
        own = matches[0] if len(matches) == 1 else None
    if own is None:
        print("RIG: no bone for", obj.name)
        return None
    skin = rig["skin"].get(own, [])
    tree = None
    if skin:
        tree = kdtree.KDTree(len(skin))
        for i, (p, _) in enumerate(skin):
            tree.insert(Vector(p), i)
        tree.balance()

    obj.vertex_groups.clear()
    groups = {}

    def group(name):
        if name not in groups:
            groups[name] = obj.vertex_groups.new(name=name)
        return groups[name]

    counts = {}
    mw = obj.matrix_world
    for v in obj.data.vertices:
        bone = own
        if tree is not None:
            _, i, dist = tree.find(mw @ v.co)
            if dist is not None and dist <= SKIN_MATCH:
                bone = skin[i][1]
        group(bone).add([v.index], 1.0, "REPLACE")
        counts[bone] = counts.get(bone, 0) + 1

    world = obj.matrix_world.copy()
    obj.parent = arm
    obj.matrix_world = world
    for m in [m for m in obj.modifiers if m.type == "ARMATURE"]:
        obj.modifiers.remove(m)
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    print("RIG:", obj.name, "->", own, counts)
    return own


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    rig_path, out_path = argv[0], argv[1]
    rigs = json.load(open(rig_path))
    for coll_name, rig in rigs.items():
        coll = bpy.data.collections.get(coll_name)
        if coll is None:
            print("RIG: no collection", coll_name)
            continue
        arm = build_armature(coll, rig)
        for obj in list(coll.objects):
            if obj.type == "MESH":
                bind(obj, arm, rig)
    bpy.ops.wm.save_as_mainfile(filepath=out_path, compress=True)
    print("saved", out_path)


main()
