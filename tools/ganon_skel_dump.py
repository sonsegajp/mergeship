"""MergeShip2 -- Ganondorf mode: prints the limb hierarchies of adult Link and the Ganondorf models (joint offsets,
display lists), to map Ganondorf's limbs onto Link's rig.

    python tools/ganon_skel_dump.py --oot-o2r build/latest/oot.o2r
"""

import argparse
import os
import sys
import zipfile

sys.path.insert(0, os.path.dirname(__file__))
import rip_models as rm  # noqa: E402

SKELETONS = [
    "objects/object_link_boy/gLinkAdultSkel",
    "objects/object_link_child/gLinkChildSkel",
    "objects/object_gndd/gYoungGanondorfSkel",
    "objects/object_ganon/gGanondorfSkel",
    "objects/object_ganon2/gGanonSkel",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oot-o2r", required=True)
    a = ap.parse_args()
    z = zipfile.ZipFile(a.oot_o2r)

    class Arc:
        pass

    arc = Arc()
    arc.z = z
    arc.cache = {}
    arc.read = lambda n: z.read(n)
    for skel in SKELETONS:
        names, count = rm.read_skeleton(arc, skel)
        raw = z.read(skel)
        print("=== %s  type=%d limbtype=%d limbs=%d" % (skel, raw[0x40], raw[0x41], count))
        limbs = []
        for n in names:
            try:
                limbs.append(rm.read_limb(arc, n))
            except Exception as e:  # noqa: BLE001
                limbs.append({"err": str(e), "pos": (0, 0, 0), "child": 0xFF, "sibling": 0xFF, "dl": ""})

        def visit(i, depth):
            l = limbs[i]
            print("%s%2d %-34s pos=%s dl=%s %s" % ("  " * depth, i, names[i].split("/")[-1], l["pos"],
                                                   "y" if l.get("dl") else "-", l.get("err", "")))
            if l["child"] != 0xFF:
                visit(l["child"], depth + 1)
            if l["sibling"] != 0xFF:
                visit(l["sibling"], depth)

        visit(0, 0)


if __name__ == "__main__":
    main()
