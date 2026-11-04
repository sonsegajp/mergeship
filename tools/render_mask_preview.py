"""MergeShip2 -- renders the Fierce Deity mask's model (gameplay_keep gFierceDeityMaskDL) from mm.o2r, with its own
textures and with the Adult Mask's (tools/gen_adult_mask_assets.py), for a preview before the mask is in the game.

    python tools/render_mask_preview.py --mm-o2r mm.o2r --oot-o2r oot.o2r --out preview_3d.png

A small software rasterizer: the display list's vertices (G_VTX by resource hash), triangles (G_TRI1/G_TRI2), texture
loads (G_SETTIMG by hash, then LOADBLOCK / LOADTLUT) and tile sizes; bilinear texturing (as the game filters), one
light plus ambient from the vertex normals. Views from the front, three-quarter and side.
"""

import argparse
import math
import struct
import sys
import zipfile

import numpy as np
from PIL import Image

sys.path.insert(0, __import__("os").path.dirname(__file__))
import gen_adult_mask_assets as art  # noqa: E402

DL = "mm/objects/gameplay_keep/gFierceDeityMaskDL"


def crc64(text):
    poly = 0x42F0E1EBA9EA3693
    crc = 0xFFFFFFFFFFFFFFFF
    for ch in text.encode():
        crc ^= ch << 56
        for _ in range(8):
            crc = ((crc << 1) ^ poly) & 0xFFFFFFFFFFFFFFFF if crc & (1 << 63) else (crc << 1) & 0xFFFFFFFFFFFFFFFF
    return crc


def parse(z):
    names = {crc64(n): n for n in z.namelist() if "gameplay_keep" in n}
    data = z.read(DL)[0x50:]
    cmds = [struct.unpack("<II", data[i:i + 8]) for i in range(0, len(data) - 7, 8)]
    verts = [None] * 64
    tris = []  # (3 vertices, texture name, tile w, tile h)
    img = None
    tex = None
    tile = (32, 32)
    wrap = (0, 0)  # the render tile's cms, cmt (G_TX_MIRROR 1, G_TX_CLAMP 2)
    i = 0
    while i < len(cmds):
        w0, w1 = cmds[i]
        op = w0 >> 24
        if op in (0x20, 0x32):  # G_SETTIMG / G_VTX by resource hash: the hash is the next command
            h0, h1 = cmds[i + 1]
            name = names.get((h0 << 32) | h1)
            if op == 0x20:
                img = name
            else:
                n = (w0 >> 12) & 0xFF
                v0 = ((w0 & 0xFF) >> 1) - n
                vd = z.read(name)[0x48:]
                start = w1 // 16
                for k in range(n):
                    x, y, zz, _, s, t, r, g, b, a = struct.unpack("<hhhHhhbbbB", vd[(start + k) * 16:(start + k) * 16 + 16])
                    verts[v0 + k] = (x, y, zz, s / 32.0, t / 32.0, r, g, b)
            i += 2
            continue
        if op == 0xF3:  # G_LOADBLOCK: the texture is the last image set
            tex = img
        elif (op == 0xF5) and (((w1 >> 24) & 7) == 0):  # G_SETTILE of the render tile
            wrap = ((w1 >> 8) & 3, (w1 >> 18) & 3)
        elif op == 0xF2:  # G_SETTILESIZE
            tile = ((((w1 >> 12) & 0xFFF) >> 2) + 1, ((w1 & 0xFFF) >> 2) + 1)
        elif op == 0x05:
            tris.append(([verts[((w0 >> 16) & 0xFF) // 2], verts[((w0 >> 8) & 0xFF) // 2], verts[(w0 & 0xFF) // 2]],
                         tex, tile, wrap))
        elif op == 0x06:
            tris.append(([verts[((w0 >> 16) & 0xFF) // 2], verts[((w0 >> 8) & 0xFF) // 2], verts[(w0 & 0xFF) // 2]],
                         tex, tile, wrap))
            tris.append(([verts[((w1 >> 16) & 0xFF) // 2], verts[((w1 >> 8) & 0xFF) // 2], verts[(w1 & 0xFF) // 2]],
                         tex, tile, wrap))
        elif op == 0xDF:
            break
        i += 1
    return tris


def wrap_coord(c, mode):
    if mode & 2:  # clamp
        return np.clip(c, 0.0, 1.0)
    if mode & 1:  # mirror
        m = np.mod(c, 2.0)
        return np.where(m > 1.0, 2.0 - m, m)
    return c


def sample(texarr, u, v, wrap=(0, 0)):
    h, w = texarr.shape[:2]
    u = wrap_coord(u, wrap[0]) * w - 0.5
    v = wrap_coord(v, wrap[1]) * h - 0.5
    x0 = np.floor(u).astype(int)
    y0 = np.floor(v).astype(int)
    fx = (u - x0)[..., None]
    fy = (v - y0)[..., None]
    x0w, x1w = x0 % w, (x0 + 1) % w
    y0w, y1w = y0 % h, (y0 + 1) % h
    c = (texarr[y0w, x0w] * (1 - fx) * (1 - fy) + texarr[y0w, x1w] * fx * (1 - fy) +
         texarr[y1w, x0w] * (1 - fx) * fy + texarr[y1w, x1w] * fx * fy)
    return c


def orient(tris):
    """Rotates the model upright and facing +z: forward is the eye triangles' facing, up is mouth -> eyes (the mask is
    modelled in the head limb's axes)."""
    def centroid(name):
        pts = [np.array(v[:3], dtype=float) for t in tris if t[1] and t[1].endswith(name) for v in t[0]]
        return np.mean(pts, axis=0)

    def facing(name):
        ns = [np.array(v[5:8], dtype=float) for t in tris if t[1] and t[1].endswith(name) for v in t[0]]
        n = np.mean(ns, axis=0)
        return n / np.linalg.norm(n)

    fwd = facing("EyesTex")
    up = centroid("EyesTex") - centroid("MouthTex")
    up -= fwd * (up @ fwd)
    up /= np.linalg.norm(up)
    right = np.cross(up, fwd)
    rot = np.array([right, up, fwd])
    out = []
    for verts, tex, tile, wrap in tris:
        nv = []
        for (x, y, z, s, t, nx, ny, nz) in verts:
            p = rot @ np.array([x, y, z], dtype=float)
            n = rot @ np.array([nx, ny, nz], dtype=float)
            nv.append((p[0], p[1], p[2], s, t, n[0], n[1], n[2]))
        out.append((nv, tex, tile, wrap))
    return out


def render(tris, textures, yaw, size=360):
    img = np.zeros((size, size, 3)) + np.array([45, 45, 50]) / 255.0
    depth = np.full((size, size), -1e9)
    cy, sy = math.cos(yaw), math.sin(yaw)
    pts = [v[:3] for t in tris for v in t[0]]
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    zs = [p[2] for p in pts]
    center = ((max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2, (max(zs) + min(zs)) / 2)
    extent = max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    scale = size * 0.8 / extent
    light = np.array([0.4, 0.5, 0.75])
    light /= np.linalg.norm(light)
    for verts, texname, tile, wrap in tris:
        screen = []
        normals = []
        for (x, y, z, s, t, nx, ny, nz) in verts:
            x, y, z = x - center[0], y - center[1], z - center[2]
            rx = x * cy + z * sy
            rz = -x * sy + z * cy
            screen.append((size / 2 + rx * scale, size / 2 - y * scale, rz, s / tile[0], t / tile[1]))
            n = np.array([nx * cy + nz * sy, ny, -nx * sy + nz * cy], dtype=float)
            normals.append(n / (np.linalg.norm(n) + 1e-6))
        (x0, y0, z0, u0, v0), (x1, y1, z1, u1, v1), (x2, y2, z2, u2, v2) = screen
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 1e-6:
            continue
        minx, maxx = max(int(min(x0, x1, x2)), 0), min(int(max(x0, x1, x2)) + 1, size - 1)
        miny, maxy = max(int(min(y0, y1, y2)), 0), min(int(max(y0, y1, y2)) + 1, size - 1)
        if minx > maxx or miny > maxy:
            continue
        gx, gy = np.meshgrid(np.arange(minx, maxx + 1) + 0.5, np.arange(miny, maxy + 1) + 0.5)
        w0 = ((x1 - gx) * (y2 - gy) - (x2 - gx) * (y1 - gy)) / area
        w1 = ((x2 - gx) * (y0 - gy) - (x0 - gx) * (y2 - gy)) / area
        w2 = 1 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not inside.any():
            continue
        zz = w0 * z0 + w1 * z1 + w2 * z2
        region = depth[miny:maxy + 1, minx:maxx + 1]
        draw = inside & (zz > region)
        if not draw.any():
            continue
        u = w0 * u0 + w1 * u1 + w2 * u2
        v = w0 * v0 + w1 * v1 + w2 * v2
        tex = textures.get(texname)
        color = sample(tex, u, v, wrap) if tex is not None else np.ones(u.shape + (3,)) * 0.8
        n = (w0[..., None] * normals[0] + w1[..., None] * normals[1] + w2[..., None] * normals[2])
        n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-6
        shade = 0.45 + 0.6 * np.clip(n @ light, 0, 1)
        color = np.clip(color * shade[..., None], 0, 1)
        region[draw] = zz[draw]
        img[miny:maxy + 1, minx:maxx + 1][draw] = color[draw]
    return Image.fromarray((img * 255).astype(np.uint8))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mm-o2r", required=True)
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    mm = zipfile.ZipFile(args.mm_o2r)
    oot = zipfile.ZipFile(args.oot_o2r)
    tris = orient(parse(mm))
    before, after = art.make_textures(mm, oot)
    rows = []
    for textures in (before, after):
        arrays = {art.FD % (part + "Tex"): np.asarray(im.convert("RGB"), dtype=float) / 255.0
                  for part, im in textures.items()}
        views = [render(tris, arrays, yaw) for yaw in (0.0, 0.6, 1.3)]
        row = Image.new("RGB", (sum(v.width for v in views), views[0].height))
        x = 0
        for v in views:
            row.paste(v, (x, 0))
            x += v.width
        rows.append(row)
    sheet = Image.new("RGB", (rows[0].width, rows[0].height * 2))
    sheet.paste(rows[0], (0, 0))
    sheet.paste(rows[1], (0, rows[0].height))
    sheet.save(args.out)
    print("wrote", args.out, len(tris), "triangles")


if __name__ == "__main__":
    main()
