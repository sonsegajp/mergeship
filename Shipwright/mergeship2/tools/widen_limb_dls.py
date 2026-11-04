#!/usr/bin/env python3
"""Widen Ocarina of Time's player limb display-list tables from ages to forms.

OoT selects a limb's model with sPlayerDListGroups[modelType][age], where the "far" LOD sits at
+2 -- the number of ages baked into the stride (sDListsLodOffset = lod * 2). Majora's Mask uses
the same idea with PLAYER_FORM_MAX. Every one of these tables is built from blocks of
2 ages x 2 LODs = 4; widening means every block becomes 2 * PLAYER_FORM_MAX.

All 21 tables have to move together. Widening some and not others, or widening them and leaving
the stride at 2, reads off the end of the short ones -- silently, with a garbage Gfx pointer.
That is why every generated table carries a static assert tying its length to PLAYER_FORM_MAX.

Where Majora's Mask has no counterpart for an item Termina does not contain -- the Megaton
Hammer, the boomerang, the slingshot -- the form falls back to its own open hand, so the item
still draws from the limb matrix on a hand belonging to that form.
"""
import io, re, sys

OOT = 'Shipwright/soh/src/code/z_player_lib.c'
MM = '2ship2harkinian/mm/src/code/z_player_lib.c'

# our form order after adult and child
FORMS = ['DEKU', 'GORON', 'ZORA', 'FIERCE_DEITY']
# MM's own order within its interleaved [form*2 + lod] tables
MMORDER = ['FIERCE_DEITY', 'GORON', 'ZORA', 'DEKU', 'HUMAN']

# OoT table -> MM table supplying the four form entries. None means Termina has no such item.
MAP = {
    'gPlayerLeftHandOpenDLs': 'gPlayerLeftHandOpenDLs',
    'gPlayerLeftHandClosedDLs': 'gPlayerLeftHandClosedDLs',
    'sPlayerLeftHandSwordDLs': 'gPlayerLeftHandOneHandSwordDLs',
    'sPlayerLeftHandSwordDLs2': 'gPlayerLeftHandOneHandSwordDLs',
    'gPlayerLeftHandBgsDLs': 'gPlayerLeftHandTwoHandSwordDLs',
    'sPlayerLeftHandHammerDLs': None,
    'gPlayerLeftHandBoomerangDLs': None,
    'sPlayerLeftHandBottleDLs': 'gPlayerLeftHandBottleDLs',
    'sPlayerRightHandOpenDLs': 'gPlayerRightHandOpenDLs',
    'sPlayerRightHandClosedDLs': 'gPlayerRightHandClosedDLs',
    'sPlayerRightHandShieldDLs': None,
    'sPlayerRightHandBowSlingshotDLs': 'gPlayerRightHandBowDLs',
    'sPlayerRightHandBowSlingshotDLs2': 'gPlayerRightHandBowDLs',
    'sPlayerRightHandOcarinaDLs': 'gPlayerRightHandInstrumentDLs',
    'sPlayerRightHandOotDLs': 'gPlayerRightHandInstrumentDLs',
    'sPlayerRightHandHookshotDLs': 'gPlayerRightHandHookshotDLs',
    'sSwordAndSheathDLs': 'gPlayerSheath12DLs',
    'sSheathDLs': 'gPlayerSheath12DLs',
    'sSheathWithSwordDLs': 'gPlayerSheath12DLs',
    'sSheathWithoutSwordDLs': 'gPlayerSheath12DLs',
    'sPlayerWaistDLs': 'gPlayerWaistDLs',
}
# which hand a table belongs to, for the fallback
HAND = {'gPlayerLeftHandOpenDLs': 'L', 'gPlayerLeftHandClosedDLs': 'L', 'sPlayerLeftHandSwordDLs': 'L',
        'sPlayerLeftHandSwordDLs2': 'L', 'gPlayerLeftHandBgsDLs': 'L', 'sPlayerLeftHandHammerDLs': 'L',
        'gPlayerLeftHandBoomerangDLs': 'L', 'sPlayerLeftHandBottleDLs': 'L',
        'sPlayerRightHandOpenDLs': 'R', 'sPlayerRightHandClosedDLs': 'R', 'sPlayerRightHandShieldDLs': 'R',
        'sPlayerRightHandBowSlingshotDLs': 'R', 'sPlayerRightHandBowSlingshotDLs2': 'R',
        'sPlayerRightHandOcarinaDLs': 'R', 'sPlayerRightHandOotDLs': 'R', 'sPlayerRightHandHookshotDLs': 'R'}


def parse(path):
    s = io.open(path, encoding='utf-8', errors='replace').read()
    stripped = re.sub(r'//[^\n]*', '', s)
    out = {}
    for m in re.finditer(r'Gfx\* (\w+)\[([^\]]*)\] = \{(.*?)\n\};', stripped, re.S):
        out[m.group(1)] = (m.group(2).strip(),
                           [e.strip() for e in m.group(3).split(',') if e.strip()])
    return s, out


def main():
    oot_src, oot = parse(OOT)
    _, mm = parse(MM)

    # MM's fallback hands, per form, prefixed for the SoH side
    openhand = {}
    for side, tbl in (('L', 'gPlayerLeftHandOpenDLs'), ('R', 'gPlayerRightHandOpenDLs')):
        e = mm[tbl][1]
        openhand[side] = {f: e[MMORDER.index(f) * 2] for f in MMORDER}

    out = oot_src
    changed = 0
    for name, mmname in MAP.items():
        size, ents = oot[name]
        assert len(ents) % 4 == 0, (name, len(ents))
        blocks = len(ents) // 4
        mments = mm[mmname][1] if mmname else None

        lines = []
        for b in range(blocks):
            blk = ents[b * 4:(b + 1) * 4]           # [adultNear, childNear, adultFar, childFar]
            near = [blk[0], blk[1]]
            far = [blk[2], blk[3]]
            for f in FORMS:
                if mments:
                    raw = mments[MMORDER.index(f) * 2]
                    # MM leaves slots empty where a form has no such part; NULL is not a symbol.
                    v = 'NULL' if raw == 'NULL' else 'mm_' + raw
                else:
                    v = 'mm_' + openhand[HAND[name]][f]
                near.append(v)
                far.append(v)
            for v in near + far:
                lines.append('    %s,' % v)

        newsize = {1: '2 * PLAYER_FORM_MAX', 2: '4 * PLAYER_FORM_MAX'}.get(blocks)
        if newsize is None:
            newsize = size.replace('* 4', '* 2 * PLAYER_FORM_MAX')
        body = '\n'.join(lines)
        new = 'Gfx* %s[%s] = {\n%s\n};\nMERGESHIP_ASSERT_LIMB_TABLE(%s, %d);' % (
            name, newsize, body, name, blocks)

        old = re.search(r'Gfx\* %s\[[^\]]*\] = \{.*?\n\};' % re.escape(name), out, re.S)
        assert old, name
        out = out[:old.start()] + new + out[old.end():]
        changed += 1

    io.open(OOT, 'w', encoding='utf-8', newline='\n').write(out)
    print('widened %d limb tables to PLAYER_FORM_MAX' % changed)


if __name__ == '__main__':
    main()
