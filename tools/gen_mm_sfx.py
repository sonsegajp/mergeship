# MergeShip2: regenerate Shipwright/soh/soh/MergeShip/MergeShipMMSfx.h from MM's sfx.h.
import re, sys, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = os.path.join(root, '2ship2harkinian/mm/include/sfx.h')
out = os.path.join(root, 'Shipwright/soh/soh/MergeShip/MergeShipMMSfx.h')
defs = []
for l in open(src, encoding='utf-8'):
    m = re.match(r'#define (NA_SE_\w+)\s+(0x[0-9A-Fa-f]+|\d+)\s*$', l.strip())
    if m:
        defs.append((m.group(1), m.group(2)))
with open(out, 'w', encoding='utf-8', newline='\n') as f:
    f.write("#pragma once\n\n// MergeShip2 -- Majora's Mask sound effect ids, generated from\n// 2ship2harkinian/mm/include/sfx.h by tools/gen_mm_sfx.py. Do not edit.\n//\n// These index MM's sequence-0 sound tables, not Ocarina of Time's: the same number names a\n// different sound in each game. Play them only through MergeShip_PlayMMSfx* (MergeShipAudio.h),\n// never through OoT's Audio_PlaySoundGeneral.\n\n")
    for n, v in defs:
        f.write(f"#define MM_{n} {v}\n")
print(len(defs), 'sfx ids ->', out)
