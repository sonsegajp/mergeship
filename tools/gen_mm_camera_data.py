#!/usr/bin/env python3
"""MergeShip2: generate MM's camera mode data for the settings Ocarina of Time shares with MM by name.

Reads 2ship2harkinian/mm/src/code/z_camera_data.inc and include/z64camera.h and writes
Shipwright/soh/soh/MergeShip/MergeShipCameraMMData.inc: every CameraModeValue array those settings use,
as the raw s16 values MM's camera functions read in order (GET_NEXT_RO_DATA), and each setting's
cameraModes table indexed by MM CameraModeType (CAM_SETTING_MODE_NULL entries kept as holes).

Usage: python tools/gen_mm_camera_data.py
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MM = os.path.join(ROOT, "2ship2harkinian", "mm")
OUT = os.path.join(ROOT, "Shipwright", "soh", "soh", "MergeShip", "MergeShipCameraMMData.inc")

# MM settings whose names OoT's CAM_SET_* share (the C name of MM's cameraModes array).
SETTINGS = [
    ("NORMAL0", "sCamSetNormal0Modes"),
    ("NORMAL1", "sCamSetNormal1Modes"),
    ("NORMAL2", "sCamSetNormal2Modes"),
    ("NORMAL3", "sCamSetNormal3Modes"),
    ("NORMAL4", "sCamSetNorm4Modes"),
    ("DUNGEON0", "sCamSetDungeon0Modes"),
    ("DUNGEON1", "sCamSetDungeon1Modes"),
    ("DUNGEON2", "sCamSetDungeon2Modes"),
]

FUNCS = {
    "CAM_FUNC_NORMAL1": "MMCAM_FUNC_NORMAL1",
    "CAM_FUNC_NORMAL3": "MMCAM_FUNC_NORMAL3",
    "CAM_FUNC_PARALLEL1": "MMCAM_FUNC_PARALLEL1",
    "CAM_FUNC_JUMP2": "MMCAM_FUNC_JUMP2",
    "CAM_FUNC_JUMP3": "MMCAM_FUNC_JUMP3",
    "CAM_FUNC_SUBJECT1": "MMCAM_FUNC_SUBJECT1",
    "CAM_FUNC_KEEPON1": "MMCAM_FUNC_KEEPON1",
    "CAM_FUNC_KEEPON3": "MMCAM_FUNC_KEEPON3",
    "CAM_FUNC_BATTLE1": "MMCAM_FUNC_BATTLE1",
    "CAM_FUNC_SPECIAL5": "MMCAM_FUNC_SPECIAL5",
}

MODES = ["NORMAL", "JUMP", "GORONDASH", "DEKUSHOOT", "BOWARROWZ", "DEKUFLY", "FIRSTPERSON", "FOLLOWBOOMERANG",
         "ZORAFIN", "FOLLOWTARGET", "TARGET", "TALK", "SLINGSHOT", "BOWARROW", "BATTLE", "DEKUHIDE", "STILL",
         "CHARGE", "CLIMB", "CLIMBZ", "HOOKSHOT", "FREEFALL", "HANG", "HANGZ", "PUSHPULL", "DEKUFLYZ",
         "GORONJUMP", "BOOMERANG", "CHARGEZ", "ZORAFINZ"]


def read(p):
    with open(p, encoding="utf-8", errors="replace") as f:
        return f.read()


def split_args(s):
    args, depth, cur = [], 0, ""
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            args.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        args.append(cur.strip())
    return args


def main():
    header = read(os.path.join(MM, "include", "z64camera.h"))
    save = read(os.path.join(MM, "include", "z64save.h"))
    data = read(os.path.join(MM, "src", "code", "z_camera_data.inc"))

    sym = {}
    for m in re.finditer(r"#define\s+(\w+_FLAG_\d+)\s+\(1 << (\d+)\)", header):
        sym[m.group(1)] = 1 << int(m.group(2))
    for m in re.finditer(r"#define\s+(CAM_LETTERBOX_\w+)\s+(0x[0-9A-Fa-f]+)", header):
        sym[m.group(1)] = int(m.group(2), 16)
    for m in re.finditer(r"/\*\s*(\d+)\s*\*/\s*(HUD_VISIBILITY_\w+)", save):
        sym[m.group(2)] = int(m.group(1))
    for m in re.finditer(r"#define\s+(CAM_HUD_VISIBILITY_\w+)\s*\\?\s*CAM_HUD_VISIBILITY\((\w+)\)", header):
        v = m.group(2)
        v = int(v, 0) if re.match(r"^(0x)?[0-9A-Fa-f]+$", v) else sym[v]
        sym[m.group(1)] = (v & 0xF) << 8

    # CAM_FUNCDATA_* macros: parameter order -> output order.
    macros = {}
    for m in re.finditer(r"#define\s+(CAM_FUNCDATA_\w+)\(([^)]*)\)((?:[^\n]*\\\n)*[^\n]*)", header):
        params = [p.strip() for p in m.group(2).split(",")]
        body = m.group(3)
        order = re.findall(r"\{\s*(\w+)\s*,\s*CAM_DATA_\w+\s*\}", body)
        if not order:
            continue
        macros[m.group(1)] = [params.index(o) for o in order]

    def ev(expr):
        expr = expr.strip()
        m = re.match(r"^CAM_INTERFACE_FLAGS\((.*)\)$", expr, re.S)
        if m:
            a = split_args(m.group(1))
            return (ev(a[0]) & 0xF000) | ev(a[1]) | (ev(a[2]) & 0xFF)
        if "|" in expr:
            v = 0
            for part in expr.split("|"):
                v |= ev(part)
            return v
        if re.match(r"^-?(0x[0-9A-Fa-f]+|\d+)$", expr):
            return int(expr, 0)
        if expr in sym:
            return sym[expr]
        raise ValueError("unknown symbol " + expr)

    arrays = {}
    for m in re.finditer(r"CameraModeValue (\w+)\[\] = \{(.*?)\n\};", data, re.S):
        vals = []
        body = m.group(2)
        for mm in re.finditer(r"(CAM_FUNCDATA_\w+)\(", body):
            start = mm.end()
            depth, i = 1, start
            while depth:
                if body[i] == "(":
                    depth += 1
                elif body[i] == ")":
                    depth -= 1
                i += 1
            args = split_args(body[start:i - 1])
            for idx in macros[mm.group(1)]:
                v = ev(args[idx]) & 0xFFFF
                vals.append(v - 0x10000 if v >= 0x8000 else v)
        arrays[m.group(1)] = vals

    tables = {}
    for m in re.finditer(r"CameraMode (\w+)\[\] = \{(.*?)\n\};", data, re.S):
        tables[m.group(1)] = re.findall(r"CAM_SETTING_MODE_(?:ENTRY\((CAM_FUNC_\w+), (\w+)\)|NULL)", m.group(2))

    used = []
    out = []
    out.append("// MergeShip2 -- GENERATED by tools/gen_mm_camera_data.py from 2ship2harkinian/mm/src/code/z_camera_data.inc.")
    out.append("// Do not edit. MM's cameraModes of the settings Ocarina of Time shares with MM by name, and the")
    out.append("// CameraModeValue arrays they use, as the raw values MM's camera functions read in order.")
    out.append("")
    body = []
    for oot, arr in SETTINGS:
        entries = tables[arr]
        body.append("// MM %s (%s)" % (arr, oot))
        body.append("static const MMCamModeData sMMCamData%s[MM_CAM_MODE_TABLE_SIZE] = {" % oot.title().replace("_", ""))
        for i, (func, name) in enumerate(entries):
            if not func:
                continue
            if func not in FUNCS:
                raise ValueError(func)
            if name not in used:
                used.append(name)
            body.append("    [%d] = { %s, sMMData_%s }, // CAM_MODE_%s" % (i, FUNCS[func], name, MODES[i]))
        body.append("};")
        body.append("")
    for name in used:
        out.append("static const s16 sMMData_%s[] = { %s };" % (name, ", ".join(str(v) for v in arrays[name])))
    out.append("")
    out.extend(body)
    with open(OUT, "w", newline="\n") as f:
        f.write("\n".join(out))
    print("wrote", OUT, len(used), "arrays")


if __name__ == "__main__":
    main()
