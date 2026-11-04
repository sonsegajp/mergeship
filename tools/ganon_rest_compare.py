"""MergeShip2 -- Ganondorf mode: compares a Ganondorf model's own standing pose with Link's idle, limb for limb, to find
the joints whose rest orientation differs from Link's (the correction render_ganon_preview.py / the game apply).

    python tools/ganon_rest_compare.py --oot-o2r build/latest/oot.o2r
"""

import argparse
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import render_mask_grid as rg  # noqa: E402
import rip_models as rm  # noqa: E402
import render_ganon_preview as gp  # noqa: E402


def normal_anim_frame(arc, name, frame):
    """Joint values of a standard (non-Link) animation: [translation, limb 0 rotation, limb 1 rotation, ...]."""
    d = arc.read(name)
    o = 0x40
    _type, frames = struct.unpack_from("<Ih", d, o)
    o += 6
    nvals = struct.unpack_from("<I", d, o)[0]
    o += 4
    vals = struct.unpack_from("<%dh" % nvals, d, o)
    o += 2 * nvals
    nidx = struct.unpack_from("<I", d, o)[0]
    o += 4
    idx = [struct.unpack_from("<HHH", d, o + 6 * k) for k in range(nidx)]
    o += 6 * nidx
    limit = struct.unpack_from("<h", d, o)[0]
    frame = frame % max(frames, 1)

    def v(i):
        return vals[i + frame] if i >= limit else vals[i]

    return [tuple(v(c) for c in j) for j in idx], frames


def euler_of(m):
    """ZYX euler (binangs) of a rotation matrix, as rot() builds them (Rz Ry Rx)."""
    sy = -m[2, 0]
    y = np.arcsin(np.clip(sy, -1, 1))
    x = np.arctan2(m[2, 1], m[2, 2])
    z = np.arctan2(m[1, 0], m[0, 0])
    return tuple(int(round(a * 32768 / np.pi)) for a in (x, y, z))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    a = ap.parse_args()
    arc = rg.Archive(a.oot_o2r)
    link = rm.read_link_animation_frame(arc, "objects/gameplay_keep/gPlayerAnim_link_normal_wait", 0, 22)
    for label, skel, mapping, anim in (
            ("tower", gp.ADULT, gp.MAP_ADULT, "objects/object_ganon_anime2/gGanondorfStandIdleAnim"),
            ("young", gp.YOUNG, gp.MAP_YOUNG, "objects/object_gndd/gYoungGanondorfWalkAnim")):
        own, frames = normal_anim_frame(arc, anim, 0)
        names, _ = rm.read_skeleton(arc, skel)
        print("=== %s (%s, %d frames, %d joints)" % (label, anim.split("/")[-1], frames, len(own)))
        for target, srcs in sorted(mapping.items()):
            m = np.eye(4, dtype=np.float32)
            for s in srcs:
                m = m @ gp.rot(link[s + 1])
            g = gp.rot(own[target + 1])
            delta = np.linalg.inv(m[:3, :3]) @ g[:3, :3]
            print("%-34s link=%-22s own=%-22s delta=%s" % (names[target].split("/")[-1], euler_of(m[:3, :3]),
                                                             own[target + 1], euler_of(delta)))


if __name__ == "__main__":
    main()
