"""MergeShip2 -- out-of-engine preview grid: every Majora's Mask mask on Ocarina of Time's adult Link's head, with the
placement include/mergeship/MaskFit.h gives it, next to the young head the mask was modelled for.

    python tools/render_mask_grid.py --mm-o2r build/host10/mm.o2r --oot-o2r build/host10/oot.o2r --out grid.png
        [--fit fits.json]   per-mask overrides {"<n>": {"x":..,"y":..,"z":..,"sx":..,"sy":..,"sz":..,"pitch":..}}

A small software renderer for the games' display lists as the archives hold them (libultraship's F3DEX2 with resource
hashes): vertices (G_VTX by hash), triangles, textures (G_SETTIMG by hash or a segment, LOADBLOCK / LOADTILE,
LOADTLUT; RGBA16/32, CI4/8, I4/8, IA4/8/16), tile sizes and wrap modes, sub-display lists (G_DL by hash), the
geometry mode's lighting (vertex normals) or vertex colours, prim and env colours. Only the head limb's own vertices
are drawn (G_MTX of the skeleton's entry 7): the neck's, which hang on the collar's matrix, are left out. Segments the
game sets at run time are filled in: the eyes and mouth (open eyes, closed mouth).

Head limb space: +X out of the face, -Y up, Z sideways. A mask's point p is drawn at
    (x, y, z) + RotZ(pitch) * (sx, sy, sz) p
exactly as MergeShipMMMasks.c (Hyrule) and MergeShipAdultLink.c (Termina) apply MaskFit.h.
"""

import argparse
import json
import math
import os
import re
import struct
import sys
import zipfile

import numpy as np
from PIL import Image, ImageDraw, ImageFont

# ---- resources ------------------------------------------------------------------------------------------------------

_CRC_TABLE = []
for _i in range(256):
    _c = _i << 56
    for _ in range(8):
        _c = ((_c << 1) ^ 0x42F0E1EBA9EA3693) & 0xFFFFFFFFFFFFFFFF if _c & (1 << 63) else (_c << 1) & 0xFFFFFFFFFFFFFFFF
    _CRC_TABLE.append(_c)


def crc64(text):
    crc = 0xFFFFFFFFFFFFFFFF
    for ch in text.encode():
        crc = _CRC_TABLE[((crc >> 56) ^ ch) & 0xFF] ^ ((crc << 8) & 0xFFFFFFFFFFFFFFFF)
    return crc


class Archive:
    def __init__(self, path):
        self.z = zipfile.ZipFile(path)
        self.names = {crc64(n): n for n in self.z.namelist()}
        self.cache = {}

    def read(self, name):
        if name not in self.cache:
            self.cache[name] = self.z.read(name)
        return self.cache[name]


def rgba16(v):
    return (((v >> 11) & 31) * 255 // 31, ((v >> 6) & 31) * 255 // 31, ((v >> 1) & 31) * 255 // 31, 255 if v & 1 else 0)


def decode_texture(arc, name, palette):
    d = arc.read(name)
    kind, w, h, size = struct.unpack("<IIII", d[0x40:0x50])
    data = d[0x50:0x50 + size]
    px = np.zeros((h, w, 4), dtype=np.float32)
    n = w * h
    if kind == 1:  # RGBA32
        a = np.frombuffer(data[:n * 4], dtype=np.uint8).reshape(h, w, 4)
        px = a.astype(np.float32) / 255.0
    elif kind == 2:  # RGBA16
        v = np.frombuffer(data[:n * 2], dtype=">u2").reshape(h, w).astype(np.uint32)
        px[..., 0] = ((v >> 11) & 31) / 31.0
        px[..., 1] = ((v >> 6) & 31) / 31.0
        px[..., 2] = ((v >> 1) & 31) / 31.0
        px[..., 3] = (v & 1).astype(np.float32)
    elif kind in (3, 4):  # CI4 / CI8
        if kind == 3:
            b = np.frombuffer(data[:(n + 1) // 2], dtype=np.uint8)
            idx = np.empty(b.size * 2, dtype=np.uint8)
            idx[0::2] = b >> 4
            idx[1::2] = b & 15
            idx = idx[:n]
        else:
            idx = np.frombuffer(data[:n], dtype=np.uint8)
        pal = np.array(palette if palette else [(255, 0, 255, 255)] * 256, dtype=np.float32) / 255.0
        if len(pal) < 256:
            pal = np.vstack([pal, np.tile(pal[-1:], (256 - len(pal), 1))])
        px = pal[idx].reshape(h, w, 4)
    elif kind in (5, 6):  # I4 / I8
        if kind == 5:
            b = np.frombuffer(data[:(n + 1) // 2], dtype=np.uint8)
            v = np.empty(b.size * 2, dtype=np.uint8)
            v[0::2] = (b >> 4) * 17
            v[1::2] = (b & 15) * 17
            v = v[:n]
        else:
            v = np.frombuffer(data[:n], dtype=np.uint8)
        v = v.reshape(h, w).astype(np.float32) / 255.0
        px = np.stack([v, v, v, v], axis=-1)
    elif kind in (7, 8, 9):  # IA4 / IA8 / IA16
        if kind == 7:
            b = np.frombuffer(data[:(n + 1) // 2], dtype=np.uint8)
            q = np.empty(b.size * 2, dtype=np.uint8)
            q[0::2] = b >> 4
            q[1::2] = b & 15
            q = q[:n]
            i = ((q >> 1) * 255 // 7).astype(np.float32) / 255.0
            a = (q & 1).astype(np.float32)
        elif kind == 8:
            b = np.frombuffer(data[:n], dtype=np.uint8)
            i = ((b >> 4) * 17).astype(np.float32) / 255.0
            a = ((b & 15) * 17).astype(np.float32) / 255.0
        else:
            b = np.frombuffer(data[:n * 2], dtype=np.uint8)
            i = b[0::2].astype(np.float32) / 255.0
            a = b[1::2].astype(np.float32) / 255.0
        i = i.reshape(h, w)
        a = a.reshape(h, w)
        px = np.stack([i, i, i, a], axis=-1)
    return px


def palette_of(arc, name):
    d = arc.read(name)
    kind, w, h, size = struct.unpack("<IIII", d[0x40:0x50])
    data = d[0x50:0x50 + size]
    return [rgba16(struct.unpack(">H", data[i * 2:i * 2 + 2])[0]) for i in range(min(256, size // 2))]


# ---- display lists --------------------------------------------------------------------------------------------------

HEAD_MTX = 0x0D0001C0  # the skeleton's matrix list, entry 7: the head


def _rot(axis, binang):
    a = binang * 2 * math.pi / 65536.0
    c, s = math.cos(a), math.sin(a)
    m = np.eye(4, dtype=np.float32)
    if axis == "x":
        m[1, 1], m[1, 2], m[2, 1], m[2, 2] = c, -s, s, c
    elif axis == "y":
        m[0, 0], m[0, 2], m[2, 0], m[2, 2] = c, s, -s, c
    else:
        m[0, 0], m[0, 1], m[1, 0], m[1, 1] = c, -s, s, c
    return m


def translate_rotate_yxz(x, y, z, rx, ry, rz):
    """MM Matrix_SetTranslateRotateYXZ."""
    t = np.eye(4, dtype=np.float32)
    t[:3, 3] = (x, y, z)
    return t @ _rot("y", ry) @ _rot("x", rx) @ _rot("z", rz)


# Matrices a mask's draw puts in a segment at run time, in head space (MM z_player_lib.c): the Bunny Hood's ears at
# rest (Player_DrawBunnyHood, segment 0x0B).
RUNTIME_MATRICES = {
    0x0B000000: translate_rotate_yxz(97.0, -1203.0, -240.0, 0x3E2, 0xDBE, -0x348A),
    0x0B000040: translate_rotate_yxz(97.0, -1203.0, 240.0, -0x3E2, -0xDBE, -0x348A),
}


class Tri:
    __slots__ = ("p", "uv", "col", "tex", "wrap", "alpha", "comb", "prim", "env", "key", "mids")

    def __init__(self, p, uv, col, tex, wrap, alpha, comb=None, prim=(1, 1, 1, 1), env=(1, 1, 1, 1), key=None):
        self.p, self.uv, self.col, self.tex, self.wrap, self.alpha = p, uv, col, tex, wrap, alpha
        self.comb, self.prim, self.env, self.key = comb, prim, env, key
        self.mids = None  # per vertex: the G_MTX address that placed it (None: the list's own matrix)


def decode_combine(w0, w1):
    """G_SETCOMBINE's colour selectors, both cycles: ((a, b, c, d), (a, b, c, d))."""
    return ((((w0 >> 20) & 0xF), ((w1 >> 28) & 0xF), ((w0 >> 15) & 0x1F), ((w1 >> 15) & 7)),
            (((w0 >> 5) & 0xF), ((w1 >> 24) & 0xF), (w0 & 0x1F), ((w1 >> 6) & 7)))


def combine(comb, tex, shade, prim, env):
    """The colour combiner (RGB), both cycles; TEXEL1 is taken as TEXEL0."""
    one = np.ones_like(shade)
    zero = np.zeros_like(shade)
    P = one * np.array(prim[:3])
    E = one * np.array(env[:3])

    def pick(sel, which, combined):
        table = {0: combined, 1: tex, 2: tex, 3: P, 4: shade, 5: E}
        if which == "a":
            table.update({6: one})
        elif which == "c":
            table.update({7: one * 1.0, 8: one, 9: one, 10: one * prim[3], 11: one, 12: one * env[3], 13: one,
                          14: one, 15: one})
        elif which == "d":
            table.update({6: one, 7: zero})
        return table.get(sel, zero)

    result = zero
    for cyc in comb:
        a, b, c, d = cyc
        result = (pick(a, "a", result) - pick(b, "b", result)) * pick(c, "c", result) + pick(d, "d", result)
    return np.clip(result, 0, 1)


def run_dl(arc, dl_name, segments, out, default_env=(0.12, 0.41, 0.11), stats=None, matrices=None, initial=None):
    """Appends the head-space triangles of `dl_name` to `out`. `matrices` (segment address -> 4x4) and `initial` (the
    list's own matrix) place a skeleton's limb: the vertices of every G_MTX go through that matrix."""
    st = {
        "vb": [None] * 64, "img": None, "tmem": {}, "tlut": None, "tiles": {i: {"tmem": 0, "fmt": 0, "pal": 0,
                                                                              "cms": 0, "cmt": 0, "w": 32, "h": 32}
                                                                       for i in range(8)},
        "lighting": True, "prim": (1.0, 1.0, 1.0, 1.0), "env": default_env + (1.0,), "texon": True,
        "mtx_ok": True, "mtx": initial, "mid": None, "tex_cache": {}, "comb": None, "key": None,
    }

    def texture_for(tile):
        t = st["tiles"][0]
        name = st["tmem"].get(t["tmem"])
        if name is None:
            return None
        key = (name, st["tlut"], t["pal"] if t["fmt"] == 2 else 0)
        st["key"] = key
        if key not in st["tex_cache"]:
            pal = None
            if st["tlut"] is not None:
                pal = palette_of(arc, st["tlut"])
                if t["fmt"] == 2:  # CI
                    kind = struct.unpack("<I", arc.read(name)[0x40:0x44])[0]
                    if kind == 3:
                        base = t["pal"] * 16
                        pal = pal[base:base + 16]
            try:
                st["tex_cache"][key] = decode_texture(arc, name, pal)
            except Exception:
                st["tex_cache"][key] = None
        return st["tex_cache"][key]

    def walk(name, depth):
        if depth > 8 or name is None:
            return
        raw = arc.read(name)
        # SoH's marker (0x33) at 0x48 is followed by the list's name hash: skip both, or a hash whose top byte happens
        # to be an opcode (gGanondorfPelvisDL's 0xDA, a G_MTX) is run as a command.
        data = raw[0x58:] if raw[0x4B] == 0x33 else raw[0x50:]
        cmds = [struct.unpack("<II", data[i:i + 8]) for i in range(0, len(data) - 7, 8)]
        i = 0
        while i < len(cmds):
            w0, w1 = cmds[i]
            op = w0 >> 24
            if op in (0x20, 0x31, 0x32, 0x36, 0x35):  # by resource hash: the hash is the next command
                h0, h1 = cmds[i + 1]
                res = arc.names.get((h0 << 32) | h1)
                if op == 0x20:
                    st["img"] = res
                elif op == 0x31:
                    walk(res, depth + 1)
                    if (w0 >> 16) & 0xFF:  # branch: this list ends here
                        return
                elif op == 0x32 and res is not None:
                    n = (w0 >> 12) & 0xFF
                    v0 = ((w0 & 0xFF) >> 1) - n
                    vd = arc.read(res)[0x48:]
                    start = w1 // 16
                    for k in range(n):
                        x, y, z, _, s, t, r, g, b, a = struct.unpack(
                            "<hhhHhhBBBB", vd[(start + k) * 16:(start + k) * 16 + 16])
                        if st["mtx"] is not None:
                            x, y, z = (st["mtx"] @ np.array([x, y, z, 1.0], dtype=np.float32))[:3]
                            if st["lighting"]:  # a normal turns with the matrix (a colour does not)
                                nv = st["mtx"][:3, :3] @ np.array([(r if r < 128 else r - 256),
                                                                   (g if g < 128 else g - 256),
                                                                   (b if b < 128 else b - 256)], dtype=np.float32)
                                r, g, b = (int(v) & 0xFF for v in np.clip(nv, -127, 127))
                        st["vb"][v0 + k] = (x, y, z, s / 32.0, t / 32.0, r, g, b, a, st["mtx_ok"], st["mid"])
                i += 2
                continue
            if op == 0xDA:  # G_MTX by segment: the head's own, a runtime one (the ears), or another limb's (left out)
                addr = w1 & ~0xF
                if matrices is not None and addr in matrices:
                    st["mtx_ok"], st["mtx"], st["mid"] = True, matrices[addr], addr
                elif addr == HEAD_MTX:
                    st["mtx_ok"], st["mtx"] = True, None
                elif addr in RUNTIME_MATRICES:
                    st["mtx_ok"], st["mtx"] = True, RUNTIME_MATRICES[addr]
                elif (addr >> 24) not in (0x0B, 0x0D):
                    # not a segment (gameplay_keep's Zora Mask loads its head matrix this way): the head's
                    st["mtx_ok"], st["mtx"] = True, None
                else:
                    st["mtx_ok"] = False
            elif op == 0xD8:  # G_POPMTX: back to the head (the limb's own)
                st["mtx_ok"], st["mtx"], st["mid"] = True, initial, None
            elif op == 0xDE:  # G_DL by segment: a runtime list (not drawn)
                pass
            elif op == 0xFD:  # G_SETTIMG by segment
                st["img"] = segments.get(w1 >> 24)
            elif op == 0xF5:  # G_SETTILE
                tile = (w1 >> 24) & 7
                st["tiles"][tile].update(tmem=w0 & 0x1FF, fmt=(w0 >> 21) & 7, pal=(w1 >> 20) & 0xF,
                                         cmt=(w1 >> 18) & 3, cms=(w1 >> 8) & 3)
            elif op == 0xF2:  # G_SETTILESIZE
                tile = (w1 >> 24) & 7
                st["tiles"][tile]["w"] = (((w1 >> 12) & 0xFFF) - ((w0 >> 12) & 0xFFF)) / 4.0 + 1
                st["tiles"][tile]["h"] = ((w1 & 0xFFF) - (w0 & 0xFFF)) / 4.0 + 1
            elif op in (0xF3, 0xF4):  # LOADBLOCK / LOADTILE: the image goes to the load tile's TMEM
                tile = (w1 >> 24) & 7
                st["tmem"][st["tiles"][tile]["tmem"]] = st["img"]
            elif op == 0xF0:  # LOADTLUT
                st["tlut"] = st["img"]
            elif op == 0xD9:  # G_GEOMETRYMODE
                clear, setb = (~w0) & 0xFFFFFF, w1
                if clear & 0x00020000:
                    st["lighting"] = False
                if setb & 0x00020000:
                    st["lighting"] = True
            elif op == 0xD7:  # G_TEXTURE
                st["texon"] = (w0 & 0xFF) != 0
            elif op == 0xFC:  # G_SETCOMBINE
                st["comb"] = decode_combine(w0, w1)
            elif op == 0xFA:
                st["prim"] = tuple(((w1 >> s) & 0xFF) / 255.0 for s in (24, 16, 8, 0))
            elif op == 0xFB:
                st["env"] = tuple(((w1 >> s) & 0xFF) / 255.0 for s in (24, 16, 8, 0))
            elif op in (0x05, 0x06, 0x07):
                tris = []
                if op == 0x05:
                    tris.append(w0)
                else:
                    tris += [w0, w1]
                for w in tris:
                    idx = [((w >> s) & 0xFF) // 2 for s in (16, 8, 0)]
                    vs = [st["vb"][k] for k in idx]
                    if any(v is None for v in vs) or not all(v[9] for v in vs):
                        continue
                    st["key"] = None
                    tex = texture_for(0) if st["texon"] else None
                    t0 = st["tiles"][0]
                    p = np.array([v[:3] for v in vs], dtype=np.float32)
                    uv = np.array([(v[3] / t0["w"], v[4] / t0["h"]) for v in vs], dtype=np.float32)
                    if st["lighting"]:
                        nrm = np.array([[(v[5] if v[5] < 128 else v[5] - 256), (v[6] if v[6] < 128 else v[6] - 256),
                                         (v[7] if v[7] < 128 else v[7] - 256)] for v in vs], dtype=np.float32)
                        col = ("n", nrm)
                    else:
                        col = ("c", np.array([v[5:8] for v in vs], dtype=np.float32) / 255.0)
                    base = np.ones(3)
                    tri = Tri(p, uv, (col, base), tex, (t0["cms"], t0["cmt"]), 1.0, st["comb"], st["prim"],
                              st["env"], st["key"] if tex is not None else None)
                    tri.mids = [v[10] for v in vs]
                    out.append(tri)
            elif op == 0xDF:
                return
            elif stats is not None:
                stats[op] = stats.get(op, 0) + 1
            i += 1

    walk(dl_name, 0)
    return out


# ---- rasterizer -----------------------------------------------------------------------------------------------------


def wrap_coord(c, mode):
    if mode & 2:
        return np.clip(c, 0.0, 0.999)
    if mode & 1:
        m = np.mod(c, 2.0)
        return np.where(m > 1.0, 2.0 - m, m)
    return np.mod(c, 1.0)


def sample(tex, u, v, wrap):
    h, w = tex.shape[:2]
    x = (wrap_coord(u, wrap[0]) * w).astype(int).clip(0, w - 1)
    y = (wrap_coord(v, wrap[1]) * h).astype(int).clip(0, h - 1)
    return tex[y, x]


def view_matrix(yaw_deg):
    """Head space -> screen: front (yaw 0) looks at the face from +X; screen up is -Y."""
    yaw = math.radians(yaw_deg)
    # screen x (right), screen y (up), depth (toward the viewer)
    right = np.array([-math.sin(yaw), 0.0, -math.cos(yaw)])
    up = np.array([0.0, -1.0, 0.0])
    toward = np.array([math.cos(yaw), 0.0, -math.sin(yaw)])
    return np.stack([right, up, toward])


def render(tris, yaw, size, center, scale, bg=(52, 52, 60)):
    img = np.zeros((size, size, 3), dtype=np.float32) + np.array(bg, dtype=np.float32) / 255.0
    depth = np.full((size, size), -1e9, dtype=np.float32)
    m = view_matrix(yaw)
    light = m.T @ np.array([0.35, 0.55, 0.75])
    light /= np.linalg.norm(light)
    for t in tris:
        q = (t.p - center) @ m.T
        sx = size / 2 + q[:, 0] * scale
        sy = size / 2 - q[:, 1] * scale
        sz = q[:, 2]
        area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0])
        if abs(area) < 1e-6:
            continue
        x0, x1 = max(int(sx.min()), 0), min(int(sx.max()) + 1, size - 1)
        y0, y1 = max(int(sy.min()), 0), min(int(sy.max()) + 1, size - 1)
        if x0 > x1 or y0 > y1:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        w0 = ((sx[1] - gx) * (sy[2] - gy) - (sx[2] - gx) * (sy[1] - gy)) / area
        w1 = ((sx[2] - gx) * (sy[0] - gy) - (sx[0] - gx) * (sy[2] - gy)) / area
        w2 = 1 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not inside.any():
            continue
        zz = w0 * sz[0] + w1 * sz[1] + w2 * sz[2]
        region = depth[y0:y1 + 1, x0:x1 + 1]
        draw = inside & (zz > region)
        if not draw.any():
            continue
        (kind, cv), base = t.col
        if kind == "n":
            n = w0[..., None] * cv[0] + w1[..., None] * cv[1] + w2[..., None] * cv[2]
            n = n / (np.linalg.norm(n, axis=-1, keepdims=True) + 1e-6)
            shade = (0.5 + 0.55 * np.clip(n @ light, 0, 1))[..., None] * np.ones(3)
        else:
            shade = w0[..., None] * cv[0] + w1[..., None] * cv[1] + w2[..., None] * cv[2]
        alpha = np.ones(w0.shape)
        texrgb = np.ones(w0.shape + (3,))
        if t.tex is not None:
            u = w0 * t.uv[0, 0] + w1 * t.uv[1, 0] + w2 * t.uv[2, 0]
            v = w0 * t.uv[0, 1] + w1 * t.uv[1, 1] + w2 * t.uv[2, 1]
            texel = sample(t.tex, u, v, t.wrap)
            texrgb = texel[..., :3]
            alpha = texel[..., 3]
        if t.comb is not None:
            color = combine(t.comb, texrgb, shade, t.prim, t.env)
        else:
            color = shade * texrgb
        draw &= alpha > 0.3
        region[draw] = zz[draw]
        img[y0:y1 + 1, x0:x1 + 1][draw] = np.clip(color, 0, 1)[draw]
    return Image.fromarray((img * 255).astype(np.uint8))


# ---- the fit (include/mergeship/MaskFit.h) --------------------------------------------------------------------------


def read_fits(header):
    """MaskFit.h's table as {mask: (x, y, z, sx, sy, sz, pitch)}."""
    text = open(header, encoding="utf-8").read()
    head = re.search(r"#define MS_MASK_FIT_HEAD \{([^}]*)\}", text).group(1)
    head_vals = [float(v.strip().rstrip("f")) for v in head.split(",")]
    table = re.search(r"kMsAdultMaskFit\[MS_MASK_FIT_COUNT\] = \{(.*?)\n\};", text, re.S).group(1)
    fits = {}
    for line in table.splitlines():
        m = re.match(r"\s*/\*\s*(\d+)[^*]*\*/\s*(.*?),?\s*$", line)
        if not m:
            continue
        n, val = int(m.group(1)), m.group(2).strip().rstrip(",")
        if val.startswith("MS_MASK_FIT_HEAD"):
            fits[n] = tuple(head_vals)
        else:
            fits[n] = tuple(float(v.strip().rstrip("f")) for v in val.strip("{} ").split(","))
    return fits


def fit_tris(tris, fit):
    x, y, z, sx, sy, sz, pitch = fit
    a = pitch * 2 * math.pi / 65536.0
    c, s = math.cos(a), math.sin(a)
    rot = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]], dtype=np.float32)
    out = []
    for t in tris:
        p = (t.p * np.array([sx, sy, sz], dtype=np.float32)) @ rot.T + np.array([x, y, z], dtype=np.float32)
        out.append(Tri(p, t.uv, t.col, t.tex, t.wrap, t.alpha, t.comb, t.prim, t.env))
    return out


MASKS = [
    (1, "Mask of Truth", "object_mask_truth/object_mask_truth_DL_0001A0"),
    (2, "Kafei's Mask", "object_mask_kerfay/gKafeisMaskDL"),
    (3, "All-Night Mask", "object_mask_yofukasi/object_mask_yofukasi_DL_000490"),
    (4, "Bunny Hood", "object_mask_rabit/object_mask_rabit_DL_000610"),
    (5, "Keaton Mask", "object_mask_ki_tan/object_mask_ki_tan_DL_0004A0"),
    (6, "Garo's Mask", "object_mask_json/object_mask_json_DL_0004C0"),
    (7, "Romani's Mask", "object_mask_romerny/object_mask_romerny_DL_0007A0"),
    (8, "Circus Leader's Mask", "object_mask_zacho/object_mask_zacho_DL_000700"),
    (9, "Postman's Hat", "object_mask_posthat/object_mask_posthat_DL_000290"),
    (10, "Couple's Mask", "object_mask_meoto/object_mask_meoto_DL_0005A0"),
    (11, "Great Fairy's Mask", "object_mask_bigelf/object_mask_bigelf_DL_0016F0"),
    (12, "Gibdo Mask", "object_mask_gibudo/object_mask_gibudo_DL_000250"),
    (13, "Don Gero's Mask", "object_mask_gero/gDonGeroMaskDL"),
    (14, "Kamaro's Mask", "object_mask_dancer/object_mask_dancer_DL_000EF0"),
    (15, "Captain's Hat", "object_mask_skj/object_mask_skj_DL_0009F0"),
    (16, "Stone Mask", "object_mask_stone/object_mask_stone_DL_000820"),
    (17, "Bremen Mask", "object_mask_bree/object_mask_bree_DL_0003C0"),
    (18, "Blast Mask", "object_mask_bakuretu/object_mask_bakuretu_DL_0005C0"),
    (19, "Mask of Scents", "object_mask_bu_san/object_mask_bu_san_DL_000710"),
    (20, "Giant's Mask", "object_mask_kyojin/object_mask_kyojin_DL_000380"),
    (21, "Fierce Deity's Mask", "gameplay_keep/gFierceDeityMaskDL"),
    (22, "Goron Mask", "gameplay_keep/gGoronMaskDL"),
    (23, "Zora Mask", "gameplay_keep/gZoraMaskDL"),
    (24, "Deku Mask", "gameplay_keep/gDekuMaskDL"),
]


def find(arc, suffix):
    for n in arc.z.namelist():
        if n.endswith(suffix):
            return n
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mm-o2r", required=True)
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--header", default=os.path.join(os.path.dirname(__file__), "..", "include", "mergeship",
                                                     "MaskFit.h"))
    ap.add_argument("--fit", help="JSON overrides {mask: {x,y,z,sx,sy,sz,pitch}}")
    ap.add_argument("--masks", help="comma-separated mask numbers (default all)")
    ap.add_argument("--cell", type=int, default=230)
    args = ap.parse_args()

    mm = Archive(args.mm_o2r)
    oot = Archive(args.oot_o2r)
    fits = read_fits(args.header)
    if args.fit:
        for k, v in json.load(open(args.fit)).items():
            base = list(fits[int(k)])
            for i, f in enumerate(("x", "y", "z", "sx", "sy", "sz", "pitch")):
                if f in v:
                    base[i] = float(v[f])
            fits[int(k)] = tuple(base)

    adult_seg = {8: "objects/object_link_boy/gLinkAdultEyesOpenTex", 9: "objects/object_link_boy/gLinkAdultMouth1Tex"}
    young_seg = {8: "mm/objects/object_link_child/gLinkHumanEyesOpenTex",
                 9: "mm/objects/object_link_child/gLinkHumanMouthClosedTex"}
    adult_head = run_dl(oot, "objects/object_link_boy/gLinkAdultHeadNearDL", adult_seg, [])
    young_head = run_dl(mm, "mm/objects/object_link_child/gLinkHumanHeadDL", young_seg, [])

    def frame(tris):
        pts = np.concatenate([t.p for t in tris])
        return (pts.min(0) + pts.max(0)) / 2

    center_adult = frame(adult_head) + np.array([150.0, -420.0, 0.0])
    center_young = frame(young_head) + np.array([150.0, -420.0, 0.0])
    size = args.cell
    scale = size / 2750.0  # one scale for every cell: sizes compare

    wanted = None if not args.masks else {int(m) for m in args.masks.split(",")}
    rows = []
    font = ImageFont.load_default()
    stats = {}
    for n, title, suffix in MASKS:
        if wanted and n not in wanted:
            continue
        name = find(mm, "mm/objects/" + suffix)
        mask = run_dl(mm, name, {}, [], stats=stats) if name else []
        fitted = fit_tris(mask, fits.get(n, fits[0]))
        cells = [render(young_head + mask, 25, size, center_young, scale),
                 render(adult_head + fitted, 0, size, center_adult, scale),
                 render(adult_head + fitted, 35, size, center_adult, scale),
                 render(adult_head + fitted, 90, size, center_adult, scale)]
        row = Image.new("RGB", (size * 4, size + 18), (25, 25, 30))
        d = ImageDraw.Draw(row)
        f = fits.get(n, fits[0])
        d.text((4, 2), f"{n:2d} {title}   fit x{f[0]:.0f} y{f[1]:.0f} z{f[2]:.0f} s{f[3]:.2f}/{f[4]:.2f}/{f[5]:.2f}"
                       f" pitch {f[6]:.0f}", fill=(255, 230, 120), font=font)
        for i, c in enumerate(cells):
            row.paste(c, (i * size, 18))
        rows.append(row)
    cols = 2
    rw, rh = rows[0].size
    hdr = 22
    sheet = Image.new("RGB", (rw * cols + 8 * (cols - 1), hdr + rh * ((len(rows) + cols - 1) // cols)), (15, 15, 18))
    d = ImageDraw.Draw(sheet)
    for c in range(cols):
        d.text((c * (rw + 8) + 4, 5), "young (MM's own placement)  |  adult front  |  adult 3/4  |  adult side",
               fill=(200, 200, 200), font=font)
    for i, row in enumerate(rows):
        sheet.paste(row, ((i % cols) * (rw + 8), hdr + (i // cols) * rh))
    sheet.save(args.out)
    print("wrote", args.out, len(rows), "masks; unhandled ops:", {hex(k): v for k, v in stats.items()})


if __name__ == "__main__":
    main()
