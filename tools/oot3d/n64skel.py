"""Prints a vanilla skeleton's limbs (0-based index, parent, translation, display list) from oot.o2r.
    py -3.13 tools/oot3d/n64skel.py <oot.o2r> objects/object_x/gXSkel"""
import sys, zipfile
sys.path.insert(0, r"C:\Users\hyper\oot3d-tools")
from actor_formats import parse_skeleton, parse_limb
z = zipfile.ZipFile(sys.argv[1])
sk = parse_skeleton(z.read(sys.argv[2]))
limbs = [parse_limb(z.read(p.replace("__OTR__", ""))) for p in sk["limbTable"]]
parent = [-1] * len(limbs)
for i, lb in enumerate(limbs):
    c = lb["childIndex"]
    while c != 0xFF and c < len(limbs):
        parent[c] = i
        c = limbs[c]["siblingIndex"]
for i, lb in enumerate(limbs):
    print(f"{i:2d} parent {parent[i]:2d} t{lb['trans']}  {lb['dListPtr'].replace('__OTR__','')}")
