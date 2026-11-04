"""MergeShip2 -- Blender side of tools/rip_models.py: builds the ripped meshes and materials from scene.json and saves
a .blend (textures packed in, so the file stands alone).

    blender --background --factory-startup --python tools/build_blend.py -- scene.json out.blend

Materials follow the games' colour combiner as far as Blender needs: the texture times its constant colour (the
combiner's prim / env part) times the vertex colour (unlit parts) or Blender's lighting (lit parts, with the games'
own normals as custom normals). Texture wrap modes are kept (repeat, mirror, clamp); textures are point-sampled
("Closest"), as the originals are small.
"""

import json
import os
import sys

import bpy


def build_material(m, tex_dir):
    mat = bpy.data.materials.new(m["name"])
    mat.use_nodes = True
    nt = mat.node_tree
    nodes, links = nt.nodes, nt.links
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Roughness"].default_value = 0.85
    base = nodes.new("ShaderNodeRGB")
    base.outputs[0].default_value = (m["base"][0], m["base"][1], m["base"][2], 1.0)
    base.location = (-700, 200)
    vcol = nodes.new("ShaderNodeVertexColor")
    vcol.layer_name = "Col"
    vcol.location = (-700, -50)
    mul1 = nodes.new("ShaderNodeMixRGB")
    mul1.blend_type = "MULTIPLY"
    mul1.inputs["Fac"].default_value = 1.0
    mul1.location = (-450, 150)
    links.new(base.outputs[0], mul1.inputs[1])
    links.new(vcol.outputs["Color"], mul1.inputs[2])
    color_out = mul1.outputs[0]
    if m["image"]:
        img = bpy.data.images.load(os.path.join(tex_dir, m["image"]), check_existing=True)
        img.pack()
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = img
        tex.interpolation = "Closest"
        cms, cmt = m["wrap"]
        if (cms & 2) or (cmt & 2):
            tex.extension = "EXTEND"
        elif (cms & 1) or (cmt & 1):
            tex.extension = "MIRROR"
        else:
            tex.extension = "REPEAT"
        tex.location = (-700, 450)
        mul2 = nodes.new("ShaderNodeMixRGB")
        mul2.blend_type = "MULTIPLY"
        mul2.inputs["Fac"].default_value = 1.0
        mul2.location = (-250, 300)
        links.new(tex.outputs["Color"], mul2.inputs[1])
        links.new(mul1.outputs[0], mul2.inputs[2])
        color_out = mul2.outputs[0]
        if m["alpha"]:
            links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
            try:
                mat.surface_render_method = "DITHERED"
            except Exception:
                pass
    links.new(color_out, bsdf.inputs["Base Color"])
    return mat


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    scene_path, out_path = argv[0], argv[1]
    data = json.load(open(scene_path))
    tex_dir = data["texture_dir"]
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj)
    mats = [build_material(m, tex_dir) for m in data["materials"]]

    for coll_data in data["collections"]:
        coll = bpy.data.collections.new(coll_data["name"])
        bpy.context.scene.collection.children.link(coll)
        for o in coll_data["objects"]:
            n = len(o["pos"]) // 3
            if n == 0:
                continue
            verts = [tuple(p) for p in o["pos"]]
            faces = [(3 * i, 3 * i + 1, 3 * i + 2) for i in range(n)]
            mesh = bpy.data.meshes.new(o["name"])
            mesh.from_pydata(verts, [], faces)
            mesh.update()
            uv = mesh.uv_layers.new(name="UVMap")
            colors = mesh.color_attributes.new(name="Col", type="FLOAT_COLOR", domain="CORNER")
            for poly in mesh.polygons:
                for li in poly.loop_indices:
                    vi = mesh.loops[li].vertex_index
                    uv.data[li].uv = o["uv"][vi]
                    c = o["col"][vi]
                    colors.data[li].color = (c[0], c[1], c[2], 1.0)
            used = sorted(set(o["mat"]))
            slot = {}
            for mi in used:
                slot[mi] = len(mesh.materials)
                mesh.materials.append(mats[mi])
            for i, poly in enumerate(mesh.polygons):
                poly.material_index = slot[o["mat"][i]]
                poly.use_smooth = True
            # The games' own normals where the part is lit; the face's own elsewhere.
            normals = []
            for poly in mesh.polygons:
                for li in poly.loop_indices:
                    nv = o["nrm"][mesh.loops[li].vertex_index]
                    normals.append(tuple(nv) if nv is not None else tuple(poly.normal))
            try:
                mesh.normals_split_custom_set(normals)
            except Exception as e:
                print("custom normals:", e)
            obj = bpy.data.objects.new(o["name"], mesh)
            coll.objects.link(obj)
            # Weld the vertices the triangles share (UVs and colours are per corner, so nothing is lost).
            bpy.context.view_layer.objects.active = obj
            obj.select_set(True)
            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.select_all(action="SELECT")
            bpy.ops.mesh.remove_doubles(threshold=0.0001)
            bpy.ops.object.mode_set(mode="OBJECT")
            obj.select_set(False)

    # A light and a camera, so the file opens ready to look at.
    light = bpy.data.lights.new("Sun", "SUN")
    light.energy = 3.0
    lo = bpy.data.objects.new("Sun", light)
    lo.rotation_euler = (0.8, 0.2, 0.6)
    bpy.context.scene.collection.objects.link(lo)
    cam = bpy.data.cameras.new("Camera")
    co = bpy.data.objects.new("Camera", cam)
    co.location = (1.0, -4.5, 1.2)
    co.rotation_euler = (1.48, 0.0, 0.2)
    bpy.context.scene.collection.objects.link(co)
    bpy.context.scene.camera = co
    bpy.ops.wm.save_as_mainfile(filepath=out_path, compress=True)
    print("saved", out_path)


if __name__ == "__main__":  # (imported by build_castle_town.py for its materials)
    main()
