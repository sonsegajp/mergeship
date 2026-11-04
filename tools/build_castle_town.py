"""MergeShip2 -- Blender side of tools/rip_castle_town.py: builds a Castle Town area in the open Blender scene (run it
inside Blender, e.g. through Blender MCP: `import build_castle_town as b; b.build_area(path)`).

An area gets a collection with:
  - "<area> collision": the collision mesh (the real 3D layout of floors and building fronts);
  - "<area> geometry": the room's own 3D display lists, textured (the Market square is all of this);
  - a camera per background, placed and aimed as the game's fixed camera, with its background as the camera's
    reference image;
  - "<area> projected": the collision mesh again, each face textured from the background of the camera that sees it
    best (in frame, not hidden, most face-on): camera projection, so the pre-rendered art lands on the 3D shapes.

Coordinates: Ocarina of Time's units (Y up) to Blender's (Z up), x 1/30 (adult Link about 1.8 m).

Camera conventions (Camera_Fixed3, OLib_VecGeoToVec3f): the camera looks along yaw rot.y and pitch rot.x (binary
angles; PITCH_SIGN says which way pitch is up), field of view vertical, the screen 4:3.
"""

import json
import math
import os
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(__file__))
import build_blend  # noqa: E402

SCALE = 1.0 / 30.0
PITCH_SIGN = -1.0


def to_blender(p, offset=(0.0, 0.0, 0.0)):
    return Vector((p[0] * SCALE + offset[0], -p[2] * SCALE + offset[1], p[1] * SCALE + offset[2]))


def binang(v):
    return v * math.pi / 32768.0


def camera_matrix(cam, offset):
    yaw, pitch, roll = binang(cam["rot"][1]), PITCH_SIGN * binang(cam["rot"][0]), binang(cam["rot"][2])
    d = (math.sin(yaw) * math.cos(pitch), math.sin(pitch), math.cos(yaw) * math.cos(pitch))
    look = Vector((d[0], -d[2], d[1])).normalized()
    rot = look.to_track_quat("-Z", "Y").to_matrix().to_4x4() @ Matrix.Rotation(roll, 4, "Z")
    return Matrix.Translation(to_blender(cam["pos"], offset)) @ rot


def collection(name):
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(coll)
    return coll


def clear(coll):
    for o in list(coll.objects):
        bpy.data.objects.remove(o)


def collision_mesh(name, col, offset):
    me = bpy.data.meshes.new(name)
    verts = [to_blender(v, offset) for v in col["vertices"]]
    faces = [tuple(t[:3]) for t in col["triangles"] if len({t[0], t[1], t[2]}) == 3]
    me.from_pydata(verts, [], faces)
    me.validate()
    me.update()
    return bpy.data.objects.new(name, me)


def import_geometry(area_dir, name, offset):
    path = os.path.join(area_dir, "scene.json")
    if not os.path.exists(path):
        return []
    data = json.load(open(path))
    mats = [build_blend.build_material(m, data["texture_dir"]) for m in data["materials"]]
    objs = []
    for coll_data in data["collections"]:
        for o in coll_data["objects"]:
            n = len(o["pos"]) // 3
            if n == 0:
                continue
            me = bpy.data.meshes.new(name)
            # scene.json is in N64 units already turned Z-up (rip_models.export with scale 1)
            me.from_pydata([Vector(p) * SCALE + Vector(offset) for p in o["pos"]], [],
                           [(3 * i, 3 * i + 1, 3 * i + 2) for i in range(n)])
            uv = me.uv_layers.new(name="UVMap")
            cols = me.color_attributes.new(name="Col", type="FLOAT_COLOR", domain="CORNER")
            for poly in me.polygons:
                for li in poly.loop_indices:
                    vi = me.loops[li].vertex_index
                    uv.data[li].uv = o["uv"][vi]
                    c = o["col"][vi]
                    cols.data[li].color = (c[0], c[1], c[2], 1.0)
            slot = {}
            for mi in sorted(set(o["mat"])):
                slot[mi] = len(me.materials)
                me.materials.append(mats[mi])
            for i, poly in enumerate(me.polygons):
                poly.material_index = slot[o["mat"][i]]
            obj = bpy.data.objects.new(name, me)
            objs.append(obj)
    return objs


def projection_material(name, image, uv_name):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    nt.nodes.remove(bsdf)
    out = nt.nodes.get("Material Output")
    emit = nt.nodes.new("ShaderNodeEmission")  # the background already has its lighting baked in
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = image
    tex.extension = "EXTEND"
    uvn = nt.nodes.new("ShaderNodeUVMap")
    uvn.uv_map = uv_name
    nt.links.new(uvn.outputs["UV"], tex.inputs["Vector"])
    nt.links.new(tex.outputs["Color"], emit.inputs["Color"])
    nt.links.new(emit.outputs["Emission"], out.inputs["Surface"])
    return mat


def project(obj, cams, images, aspect=4.0 / 3.0):
    """Each face of `obj` textured from the camera that sees it best."""
    scene = bpy.context.scene
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    tree = BVHTree.FromBMesh(bm)
    mw = obj.matrix_world
    uv_layers = []
    for i, (cam_obj, _) in enumerate(cams):
        uv_layers.append(me.uv_layers.new(name=f"cam{i}"))
    for i, (cam_obj, image) in enumerate(cams):
        me.materials.append(projection_material(f"{obj.name} cam{i}", image, f"cam{i}"))
    me.materials.append(bpy.data.materials.get("unseen") or bpy.data.materials.new("unseen"))
    unseen = len(cams)

    def screen(cam_obj, co):
        from bpy_extras.object_utils import world_to_camera_view
        v = world_to_camera_view(scene, cam_obj, mw @ co)
        return v

    for face, poly in zip(bm.faces, me.polygons):
        best, best_score = unseen, 0.0
        centre = mw @ face.calc_center_median()
        normal = (mw.to_3x3() @ face.normal).normalized()
        for i, (cam_obj, _) in enumerate(cams):
            eye = cam_obj.matrix_world.translation
            to_face = centre - eye
            dist = to_face.length
            facing = -normal.dot(to_face.normalized())
            if facing <= 0.05:
                continue
            s = screen(cam_obj, face.calc_center_median())
            if not (0.0 <= s.x <= 1.0 and 0.0 <= s.y <= 1.0 and s.z > 0):
                continue
            hit = tree.ray_cast(mw.inverted() @ eye, (mw.inverted().to_3x3() @ to_face).normalized(), dist * 1.01)
            if hit[2] is not None and hit[2] != face.index and hit[3] < dist * 0.98:
                continue
            score = facing / (1.0 + dist * 0.02)
            if score > best_score:
                best, best_score = i, score
        poly.material_index = best
    bm.free()
    # every camera's projection into its own UV layer (image space: x left to right, y bottom to top)
    for i, (cam_obj, _) in enumerate(cams):
        from bpy_extras.object_utils import world_to_camera_view
        layer = uv_layers[i]
        for poly in me.polygons:
            for li in poly.loop_indices:
                co = me.vertices[me.loops[li].vertex_index].co
                v = world_to_camera_view(scene, cam_obj, mw @ co)
                layer.data[li].uv = (v.x, v.y)


def build_area(area_dir, offset=(0.0, 0.0, 0.0), projected=True):
    info = json.load(open(os.path.join(area_dir, "area.json")))
    label = info["label"]
    coll = collection(label)
    clear(coll)
    col = info.get("collision")
    if col:
        o = collision_mesh(f"{label} collision", col, offset)
        o.display_type = "WIRE"
        coll.objects.link(o)
    for g in import_geometry(area_dir, f"{label} geometry", offset):
        coll.objects.link(g)
    cams = []
    images = {}
    for b in info["backgrounds"]:
        img = bpy.data.images.load(os.path.join(area_dir, b["file"]), check_existing=True)
        img.pack()
        images[b["camera"]] = img
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = 320, 240
    for c in (col or {}).get("cameras", []):
        if "pos" not in c:
            continue
        data = bpy.data.cameras.new(f"{label} cam{c['index']}")
        data.sensor_fit = "VERTICAL"
        data.angle_y = math.radians(max(5.0, min(c["fov_deg"], 150.0)))
        data.clip_start, data.clip_end = 0.05, 2000.0
        cam = bpy.data.objects.new(f"{label} cam{c['index']}", data)
        cam.matrix_world = camera_matrix(c, offset)
        coll.objects.link(cam)
        if c["index"] in images:
            bg = data.background_images.new()
            bg.image = images[c["index"]]
            bg.alpha = 0.6
            bg.display_depth = "FRONT"
            data.show_background_images = True
            cams.append((cam, images[c["index"]]))
    if projected and col and cams:
        o = collision_mesh(f"{label} projected", col, offset)
        coll.objects.link(o)
        bpy.context.view_layer.update()
        project(o, cams, images)
    return coll
