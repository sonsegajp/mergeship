"""MergeShip2 -- the Adult Mask's art: Majora's Mask's Fierce Deity mask wearing Ocarina of Time's adult Link's face.

    python tools/gen_adult_mask_assets.py --mm-o2r mm.o2r --oot-o2r oot.o2r --icon <icon image> --out <folder>

The mask keeps the Fierce Deity mask's model and texture sizes (so it is the same kind of N64 asset); its textures are
adult Link's own (object_link_boy):
  - eyes (64x32): gLinkAdultEyesOpenTex, fitted onto the mask's eye sockets (his eyes are a little closer together and
    lower in his texture than the mask's sockets);
  - mouth (32x16): the middle of gLinkAdultMouth1Tex (32x32); ear (16x16): gLinkAdultEarTex;
  - hair (32x32): the Fierce Deity's strands in adult Link's hair colours; the cap (8x8): Link's green.
Everything is tinted from adult Link's skin to the mask's (the Fierce Deity mask's untextured parts are that colour).
The icon (square, white or transparent background, 32x32 pixel art at any scale) becomes the 32x32 item icon.
Writes PNGs (the textures at their own size) and preview.png into --out.

The Child Mask (the Adult Mask while Link is the adult: the Skull Kid's Link mask, object_stk gSkullKidLinkMask1DL, as
it is, its hair in young Link's colours) has its hair and its scream's textures made: its eye with the pupil
constricted, and young Link's open mouth (gLinkHumanMouthOpenTex) in the mask's skin, as ChildMaskHairTex.png,
ChildMaskScreamEyeTex.png and ChildMaskScreamMouthTex.png.
"""

import argparse
import os
import struct
import zipfile

from PIL import Image

FD = "mm/objects/gameplay_keep/gFierceDeityMask%s"
LINK = "objects/object_link_boy/%s"
STK = "mm/objects/object_stk/gSkullKidLinkMask%s"
HUMAN = "mm/objects/object_link_child/gLinkHuman%s"

MASK_SKIN = (230, 172, 115)  # the Fierce Deity mask's skin (its textures' background)


def read_tex(z, name):
    d = z.read(name)
    kind, w, h, size = struct.unpack("<IIII", d[0x40:0x50])
    return kind, w, h, d[0x50:0x50 + size]


def rgba16(v):
    return ((v >> 11) & 31) * 255 // 31, ((v >> 6) & 31) * 255 // 31, ((v >> 1) & 31) * 255 // 31, 255 if v & 1 else 0


def decode(z, name, palette=None):
    kind, w, h, data = read_tex(z, name)
    im = Image.new("RGBA", (w, h))
    if kind == 4:  # CI8
        im.putdata([palette[b] for b in data[: w * h]])
    elif kind == 2:  # RGBA16
        im.putdata([rgba16(struct.unpack(">H", data[i * 2:i * 2 + 2])[0]) for i in range(w * h)])
    else:
        raise SystemExit("unexpected texture type %d for %s" % (kind, name))
    return im


def palette_of(z, name):
    kind, w, h, tlut = read_tex(z, name)
    return [rgba16(struct.unpack(">H", tlut[i * 2:i * 2 + 2])[0]) for i in range(256)]


def most_common(im):
    counts = {}
    for c in im.getdata():
        counts[c[:3]] = counts.get(c[:3], 0) + 1
    return max(counts, key=counts.get)


def tint(im, src_skin, dst_skin):
    """Adult Link's skin tone onto the mask's: every channel scaled by the ratio of the two skins."""
    ratio = [dst_skin[i] / max(1, src_skin[i]) for i in range(3)]
    out = im.copy()
    px = out.load()
    for y in range(out.size[1]):
        for x in range(out.size[0]):
            r, g, b, a = px[x, y]
            px[x, y] = (min(255, int(r * ratio[0])), min(255, int(g * ratio[1])), min(255, int(b * ratio[2])), a)
    return out


def tint_skin(im, src_skin, dst_skin):
    """tint on the warm colours only (AdultMask.hpp TintSkin): the eyes' whites and irises stay his."""
    ratio = [dst_skin[i] / max(1, src_skin[i]) for i in range(3)]
    out = im.copy()
    px = out.load()
    for y in range(out.size[1]):
        for x in range(out.size[0]):
            r, g, b, a = px[x, y]
            w = min(1.0, max(0.0, (r - b) / 80.0))
            px[x, y] = tuple(min(255, int(c * (1 + (ratio[i] - 1) * w))) for i, c in enumerate((r, g, b))) + (a,)
    return out


def bilinear(im, x, y):
    w, h = im.size
    x = min(max(x, 0.0), w - 1.0)
    y = min(max(y, 0.0), h - 1.0)
    x0, y0 = int(x), int(y)
    x1, y1 = min(x0 + 1, w - 1), min(y0 + 1, h - 1)
    fx, fy = x - x0, y - y0
    px = im.load()
    out = []
    for i in range(4):
        top = px[x0, y0][i] * (1 - fx) + px[x1, y0][i] * fx
        bottom = px[x0, y1][i] * (1 - fx) + px[x1, y1][i] * fx
        out.append(int(round(top * (1 - fy) + bottom * fy)))
    return tuple(out)


def fit_eyes(link_eyes, skin):
    """Adult Link's eyes moved onto the mask's sockets: his eye centres (17.5, 25) and (46.5, 25) go to (16, 22.5) and
    (48, 22.5), where its scream model has them (AdultMask.hpp FitEyes); his eyes grow by the same factor."""
    w, h = link_eyes.size
    scale_x = (48.0 - 16.0) / (46.5 - 17.5)
    scale_y = 1.2
    out = Image.new("RGBA", (w, h))
    px = out.load()
    for y in range(h):
        for x in range(w):
            sx = 32.0 + (x + 0.5 - 32.0) / scale_x - 0.5
            sy = 25.0 + (y + 0.5 - 22.5) / scale_y - 0.5
            if sy < 0 or sy > h - 1:
                px[x, y] = skin + (255,)
            else:
                px[x, y] = bilinear(link_eyes, sx, sy)
    # The mask's nose and chin reuse the bottom middle of this texture (clamped), where Link's has the shadow beside
    # his nose: fade that part to plain skin.
    for y in range(h):
        for x in range(w):
            fade = min(1.0, max(0.0, (y - 25) / 5.0)) * min(1.0, max(0.0, (9.0 - abs(x + 0.5 - 32.0)) / 4.0))
            if fade > 0:
                c = px[x, y]
                px[x, y] = tuple(int(c[i] + (skin[i] - c[i]) * fade) for i in range(3)) + (c[3],)
    return out


def fit_eyes_to(link_eyes, skin, left, right, centre_y, scale_y=1.2):
    """fit_eyes onto other sockets (AdultMask.hpp FitEyesTo): his eye centres go to (left, centre_y), (right, centre_y)."""
    w, h = link_eyes.size
    scale_x = (right - left) / (46.5 - 17.5)
    mid = (left + right) / 2.0
    out = Image.new("RGBA", (w, h))
    px = out.load()
    for y in range(h):
        for x in range(w):
            sx = 32.0 + (x + 0.5 - mid) / scale_x - 0.5
            sy = 25.0 + (y + 0.5 - centre_y) / scale_y - 0.5
            px[x, y] = (skin + (255,)) if (sy < 0 or sy > h - 1) else bilinear(link_eyes, sx, sy)
            fade = min(1.0, max(0.0, (y - (centre_y + 2.5)) / 5.0)) * min(1.0, max(0.0, (9.0 - abs(x + 0.5 - mid)) / 4.0))
            if fade > 0:
                c = px[x, y]
                px[x, y] = tuple(int(c[i] + (skin[i] - c[i]) * fade) for i in range(3)) + (c[3],)
    return out


def constrict_pupils(eyes, scale=0.55, halves=2, top=12):
    """The scream's eyes (AdultMask.hpp ConstrictPupils): each iris and pupil shrunk to `scale` about its centre.
    `halves`: eyes side by side in the texture (1: one eye, mirrored by the model), searched from row `top` down."""
    out = eyes.copy()
    src, op = eyes.load(), out.load()
    w, h = eyes.size

    def is_iris(c):
        return c[2] > c[0] + 40 or (sum(c[:3]) < 140 and c[2] >= c[0])

    for x0, x1 in [(i * (w // halves), (i + 1) * (w // halves)) for i in range(halves)]:
        iris = {(x, y) for y in range(top, h) for x in range(x0, x1) if is_iris(src[x, y])}
        if not iris:
            continue
        cx = sum(p[0] for p in iris) / len(iris) + 0.5
        cy = sum(p[1] for p in iris) / len(iris) + 0.5
        rad = max(((p[0] + 0.5 - cx) ** 2 + (p[1] + 0.5 - cy) ** 2) ** 0.5 for p in iris) + 0.5
        whites = [src[x, y][:3] for y in range(top, h) for x in range(x0, x1)
                  if (x, y) not in iris and min(src[x, y][:3]) > 170 and src[x, y][2] >= src[x, y][0] - 10]
        white = max(set(whites), key=whites.count) + (255,) if whites else (235, 235, 235, 255)
        for p in iris:
            op[p] = white
        for y in range(int(cy - rad) - 1, int(cy + rad) + 2):
            for x in range(int(cx - rad) - 1, int(cx + rad) + 2):
                if not (0 <= x < w and 0 <= y < h):
                    continue
                acc, n = [0.0, 0.0, 0.0], 0
                for sy in range(8):
                    for sx in range(8):
                        u = cx + (x + (sx + 0.5) / 8 - cx) / scale
                        v = cy + (y + (sy + 0.5) / 8 - cy) / scale
                        if (int(u), int(v)) in iris and x0 <= int(u) < x1:
                            c = src[int(u), int(v)]
                            acc = [acc[i] + c[i] for i in range(3)]
                            n += 1
                if n:
                    a = n / 64.0
                    c = op[x, y]
                    op[x, y] = tuple(int(c[i] * (1 - a) + acc[i] / n * a) for i in range(3)) + (255,)
    return out


def make_scream_eyes(oot):
    """The Adult Mask's scream eyes (AdultMask.hpp: MS_ADULT_MASK_SCREAM_DL): his normal eyes, pupils constricted,
    fitted onto MM's Fierce Deity scream mask (object_mask_boy)'s sockets."""
    link_pal = palette_of(oot, LINK % "gLinkAdultHeadTLUT")
    link_eyes = decode(oot, LINK % "gLinkAdultEyesOpenTex", link_pal)
    link_skin = most_common(link_eyes)
    return fit_eyes_to(tint(constrict_pupils(link_eyes), link_skin, MASK_SKIN), MASK_SKIN, 16.0, 48.0, 22.5)


def resample(im, y0, y1, w, h):
    """Rows [y0, y1) of `im` into w x h, each texel the average of a 4x4 grid of samples (AdultMask.hpp Resample)."""
    src = im.load()
    out = Image.new("RGBA", (w, h))
    px = out.load()
    for y in range(h):
        for x in range(w):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(4):
                for sx in range(4):
                    v = y0 + (y1 - y0) * (y + (sy + 0.5) / 4.0) / h
                    u = im.size[0] * (x + (sx + 0.5) / 4.0) / w
                    c = src[min(max(int(u), 0), im.size[0] - 1), min(max(int(v), 0), im.size[1] - 1)]
                    acc = [acc[i] + c[i] for i in range(4)]
            px[x, y] = tuple(int(a / 16.0) for a in acc)
    return out


# The Child Mask's scream eye, cleaned as the user cleaned it by hand (2026-10-01): these texels white.
CHILD_SCREAM_WHITE = [(21, 13), (22, 14), (19, 15), (11, 16), (19, 16), (20, 16), (10, 17), (11, 17), (20, 17),
                      (22, 17), (11, 18), (12, 18), (20, 18), (22, 18), (12, 19), (13, 19), (20, 19), (10, 20),
                      (12, 20), (13, 20), (20, 20)]


def make_child_hair(mm):
    """The Child Mask's hair (AdultMask.hpp MakeChild): the mask's strands in young Link's hair colours."""
    return match_colours(decode(mm, STK % "HairTex"), decode(mm, "mm/objects/object_link_child/object_link_child_Tex_005400"),
                         (0.1, 0.5, 0.92))


def make_child_scream(mm):
    """The Child Mask's scream textures (AdultMask.hpp MakeChild): the mask's eye (one eye, mirrored) with the pupil
    constricted and its white cleaned, and young Link's open mouth in the mask's skin, its rows 2..28 into the mask's
    32x16 mouth."""
    eye = constrict_pupils(decode(mm, STK % "EyeTex"), 0.55, halves=1, top=12)
    for xy in CHILD_SCREAM_WHITE:
        eye.putpixel(xy, (255, 255, 255, 255))
    open_mouth = decode(mm, HUMAN % "MouthOpenTex", palette_of(mm, HUMAN % "MouthTLUT"))
    open_mouth = tint(open_mouth, most_common(open_mouth), most_common(decode(mm, STK % "MouthTex")))
    mouth = resample(open_mouth, 2.0, 28.0, 32, 16)
    mouth.putalpha(255)
    return eye, mouth


def match_colours(im, reference, targets):
    """`im` in `reference`'s colours (AdultMask.hpp MatchColours): its 5th, 50th and 95th percentile of brightness take
    the colours `reference` has at `targets` (three fractions) of its brightness range (the mean within 8% of that
    rank)."""
    def lum(c):
        return 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]

    ref = sorted(reference.getdata(), key=lum)
    px = list(im.getdata())
    levels = sorted(lum(c) for c in px)
    at = [levels[round(q * (len(levels) - 1))] for q in (0.05, 0.5, 0.95)]
    to = []
    for q in targets:
        lo, hi = int(max(0.0, (q - 0.08) * (len(ref) - 1))), int(min(len(ref) - 1, (q + 0.08) * (len(ref) - 1)))
        band = ref[lo:hi + 1]
        to.append([sum(c[i] for c in band) / len(band) for i in range(3)])
    out = []
    for c in px:
        l = lum(c)
        seg = 0 if l <= at[1] else 1
        span = at[seg + 1] - at[seg]
        k = min(1.0, max(0.0, (l - at[seg]) / span)) if span > 0 else (1.0 if l > at[seg] else 0.0)
        out.append(tuple(int(round(to[seg][i] + (to[seg + 1][i] - to[seg][i]) * k)) for i in range(3)) + (c[3],))
    res = im.copy()
    res.putdata(out)
    return res


def recolor_luminance(im, dark, light, lo=0.5, hi=1.0):
    out = im.copy()
    px = out.load()
    for y in range(out.size[1]):
        for x in range(out.size[0]):
            r, g, b, a = px[x, y]
            lum = (0.3 * r + 0.59 * g + 0.11 * b) / 255.0
            t = max(0.0, min(1.0, (lum - lo) / (hi - lo)))
            px[x, y] = tuple(int(dark[i] + (light[i] - dark[i]) * t) for i in range(3)) + (a,)
    return out


def make_icon(path):
    src = Image.open(path).convert("RGBA")
    cell = src.size[0] / 32.0
    icon = Image.new("RGBA", (32, 32))
    px = icon.load()
    for y in range(32):
        for x in range(32):
            # the middle of each source pixel block (the picture is 32x32 pixel art, scaled up and blurred)
            samples = [src.getpixel((int((x + 0.5 + dx) * cell), int((y + 0.5 + dy) * cell)))
                       for dy in (-0.15, 0.0, 0.15) for dx in (-0.15, 0.0, 0.15)]
            r, g, b, a = (sorted(s[i] for s in samples)[4] for i in range(4))
            px[x, y] = (0, 0, 0, 0) if (a < 128 or (r > 225 and g > 225 and b > 225)) else (r, g, b, 255)
    return icon


def make_textures(mm, oot):
    fd_pal = palette_of(mm, FD % "TLUT")
    link_pal = palette_of(oot, LINK % "gLinkAdultHeadTLUT")
    link_eyes = decode(oot, LINK % "gLinkAdultEyesOpenTex", link_pal)
    link_skin = most_common(link_eyes)

    before = {part: decode(mm, FD % (part + "Tex"), fd_pal) for part in ("Eyes", "Hair", "Mouth", "Ear", "Hat")}
    after = {}
    after["Eyes"] = fit_eyes(tint_skin(link_eyes, link_skin, MASK_SKIN), MASK_SKIN)
    mouth = tint(decode(oot, LINK % "gLinkAdultMouth1Tex", link_pal), link_skin, MASK_SKIN)
    after["Mouth"] = mouth.crop((0, 4, 32, 20))  # his lip line is at y 12 of 32
    after["Ear"] = tint(decode(oot, LINK % "gLinkAdultEarTex", link_pal), link_skin, MASK_SKIN)
    # adult Link's own hair colours (gameplay_keep gLinkHairTex), the strands' shading kept (AdultMask.hpp)
    after["Hair"] = match_colours(before["Hair"], decode(oot, "objects/gameplay_keep/gLinkHairTex"), (0.3, 0.6, 0.92))
    after["Hat"] = recolor_luminance(before["Hat"], (45, 110, 30), (95, 170, 55), 0.3, 1.0)
    return before, after


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mm-o2r", required=True)
    ap.add_argument("--oot-o2r", required=True)
    ap.add_argument("--icon", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    mm = zipfile.ZipFile(args.mm_o2r)
    oot = zipfile.ZipFile(args.oot_o2r)
    before, after = make_textures(mm, oot)
    for part, im in after.items():
        im.save(os.path.join(args.out, "AdultMask%sTex.png" % part))
    make_scream_eyes(oot).save(os.path.join(args.out, "AdultMaskScreamEyesTex.png"))
    child_eye, child_mouth = make_child_scream(mm)
    child_eye.save(os.path.join(args.out, "ChildMaskScreamEyeTex.png"))
    make_child_hair(mm).save(os.path.join(args.out, "ChildMaskHairTex.png"))
    child_mouth.save(os.path.join(args.out, "ChildMaskScreamMouthTex.png"))

    icon = make_icon(args.icon)
    icon.save(os.path.join(args.out, "AdultMaskIcon.png"))
    kind, w, h, data = read_tex(mm, "mm/icon_item_static_yar/gItemIconFierceDeityMaskTex")
    fd_icon = Image.frombytes("RGBA", (w, h), data[: w * h * 4])

    # preview.png: the Fierce Deity's (top) and the Adult Mask's (bottom), 6x with bilinear filtering as the game draws
    # them, then the icons at 6x and at their own size.
    scale = 6
    order = ("Eyes", "Mouth", "Ear", "Hair", "Hat")
    row_w = sum(before[p].size[0] * scale + 12 for p in order) + 32 * scale + 12 + 40
    row_h = 32 * scale
    sheet = Image.new("RGBA", (row_w, row_h * 2 + 12), (45, 45, 50, 255))
    for row, (textures, ic) in enumerate(((before, fd_icon), (after, icon))):
        x = 0
        y = row * (row_h + 12)
        for part in order:
            tex = textures[part]
            sheet.paste(tex.resize((tex.size[0] * scale, tex.size[1] * scale), Image.BILINEAR), (x, y))
            x += tex.size[0] * scale + 12
        sheet.alpha_composite(ic.resize((32 * scale, 32 * scale), Image.BILINEAR), (x, y))
        x += 32 * scale + 12
        sheet.alpha_composite(ic, (x, y))
    sheet.save(os.path.join(args.out, "preview.png"))
    print("wrote", args.out)


if __name__ == "__main__":
    main()
