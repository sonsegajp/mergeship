// MergeShip2 -- the Adult Mask's art, both engines. Header-only C++ (libultraship's archives).
//
// The Adult Mask is Majora's Mask's Fierce Deity mask wearing Ocarina of Time's adult Link's face. Its model is the
// Fierce Deity mask's (gameplay_keep gFierceDeityMaskDL: same vertices, same texture sizes and formats), and its
// textures are adult Link's own (object_link_boy): his eyes fitted onto the mask's eye sockets, his mouth and ear, the
// Fierce Deity's hair strands in his hair's colours and a green cap. Its pause name "Adult Mask" is put together from
// letters of Majora's Mask's own item names.
//
// While Link is the adult the same item is the Child Mask (the user, 2026-10-01: "When adult, replace all adult mask
// assets with Child Mask"): its model is the Skull Kid's Link mask, the one he wears (object_stk gSkullKidLinkMask1DL,
// with its hair in young Link's colours), its name "Child Mask" is put together the same way, and its icon is the
// Skull Kid's mask rendered in the Adult Mask icon's style. As the adult screams with it on, it is the same mask with
// his pupils constricted (the Adult Mask scream's recipe on its eye) and young Link's own open mouth
// (gLinkHumanMouthOpenTex, in the mask's skin).
//
// All of that is Nintendo's art, so none of it ships: it is made at startup from the player's own mm.o2r and oot.o2r
// (tools/gen_adult_mask_assets.py is the same recipe in Python, with previews) and mounted as an archive that lives in
// memory (MsAdultMaskArchive). Only the icons, 32x32, are MergeShip2's own (AdultMaskIcon.inc, ChildMaskIcon.inc).
//
// Resources (the paths code draws with carry "__OTR__" in front):
//   mergeship/adultmask/gAdultMaskDL            the mask (as gFierceDeityMaskDL: drawn on Link's head limb)
//   mergeship/adultmask/gAdultMaskIconTex       the item icon, 32x32 RGBA32
//   mergeship/adultmask/gAdultMaskNameENGTex    the pause name, 128x16 IA4 (Majora's Mask's item name format)
//   mergeship/adultmask/gAdultMaskScreamDL      the mask as Link screams (as object_mask_boy_DL_000900)
//   mergeship/adultmask/gChildMaskDL            the Child Mask (gSkullKidLinkMask1DL, in the Skull Kid's head space:
//                                               placed on the adult's head by MaskFit.h's MS_MASK_FIT_CHILD_MASK;
//                                               its hair retextured)
//   mergeship/adultmask/gChildMaskIconTex       its item icon, 32x32 RGBA32
//   mergeship/adultmask/gChildMaskNameENGTex    its pause name
//   mergeship/adultmask/gChildMaskScreamDL      the Child Mask as Link screams
#pragma once

#include <ship/resource/File.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/utils/StrHash64.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define MS_ADULT_MASK_DL "mergeship/adultmask/gAdultMaskDL"
#define MS_ADULT_MASK_ICON "mergeship/adultmask/gAdultMaskIconTex"
#define MS_ADULT_MASK_NAME "mergeship/adultmask/gAdultMaskNameENGTex"
// Link's scream: Majora's Mask swaps each transformation mask for its own scream model at frame 51 of the mask going
// on (z_player_lib.c: D_801C0B20[mask - 1 + 4]); the Adult Mask's is the Fierce Deity's (object_mask_boy) wearing
// adult Link's normal eyes and brows, his pupils constricted, with its screaming mouth.
#define MS_ADULT_MASK_SCREAM_DL "mergeship/adultmask/gAdultMaskScreamDL"
// The Child Mask: the Adult Mask while Link is the adult.
#define MS_CHILD_MASK_DL "mergeship/adultmask/gChildMaskDL"
#define MS_CHILD_MASK_ICON "mergeship/adultmask/gChildMaskIconTex"
#define MS_CHILD_MASK_NAME "mergeship/adultmask/gChildMaskNameENGTex"
// MergeShip2: Ganondorf mode's masks' icons (the Deku Mask's and the Fierce Deity's Mask's places)
#define MS_PHANTOM_GANON_MASK_ICON "mergeship/ganondorf/gPhantomGanonMaskIconTex"
#define MS_GANON_MASK_ICON "mergeship/ganondorf/gGanonMaskIconTex"
// Ganondorf mode's equipment (soh MergeShipGanondorf.c): his hand in the Kokiri Sword's place, the Iron Knuckle's axe in
// the Biggoron Sword's (tools/gen_ganon_equip_icons.py)
#define MS_GANONDORF_HAND_ICON "mergeship/ganondorf/gGanondorfHandIconTex"
#define MS_IRON_KNUCKLE_AXE_ICON "mergeship/ganondorf/gIronKnuckleAxeIconTex"
#define MS_CHILD_MASK_SCREAM_DL "mergeship/adultmask/gChildMaskScreamDL"

namespace MsAdultMask {

// ---- an archive held in memory --------------------------------------------------------------------------------------

class MemoryArchive final : public Ship::Archive {
  public:
    // `gameVersion`: one the archive manager accepts (0: none), so it mounts beside the game's own archives.
    // `name`: the archive's path, which ArchiveManager::RemoveArchive goes by (a different one per mounted archive).
    MemoryArchive(std::map<std::string, std::vector<char>> files, uint32_t gameVersion,
                  const std::string& name = "mergeship-adult-mask (generated)")
        : Archive(name), mFiles(std::move(files)), mGameVersion(gameVersion) {
    }

    bool Open() override {
        if (mGameVersion != 0) {
            SetGameVersion(mGameVersion);
        }
        for (const auto& [path, data] : mFiles) {
            IndexFile(path);
            mByHash[CRC64(path.c_str())] = path;
        }
        return true;
    }
    bool Close() override {
        return true;
    }
    bool WriteFile(const std::string&, const std::vector<uint8_t>&) override {
        return false;
    }
    std::shared_ptr<Ship::File> LoadFile(const std::string& path) override {
        return Get(path);
    }
    std::shared_ptr<Ship::File> LoadFile(uint64_t hash) override {
        auto it = mByHash.find(hash);
        return (it == mByHash.end()) ? nullptr : Get(it->second);
    }

  private:
    std::shared_ptr<Ship::File> Get(const std::string& path) {
        auto it = mFiles.find(path);
        if (it == mFiles.end()) {
            return nullptr;
        }
        auto file = std::make_shared<Ship::File>();
        file->Buffer = std::make_shared<std::vector<char>>(it->second);
        file->IsLoaded = true;
        return file;
    }

    std::map<std::string, std::vector<char>> mFiles;
    std::map<uint64_t, std::string> mByHash;
    uint32_t mGameVersion = 0;
};

// ---- images -----------------------------------------------------------------------------------------------------------

struct Rgba {
    float r = 0, g = 0, b = 0, a = 255;
};

struct Image {
    int w = 0, h = 0;
    std::vector<Rgba> px;
    Rgba& at(int x, int y) {
        return px[y * w + x];
    }
    const Rgba& at(int x, int y) const {
        return px[y * w + x];
    }
};

// A texture resource: its header (0x50 bytes: the resource header, then type, width, height, data size) and data.
struct TexFile {
    std::vector<char> header;
    uint32_t type = 0, w = 0, h = 0;
    std::vector<uint8_t> data;
};

inline bool ReadTex(const std::shared_ptr<Ship::ArchiveManager>& archives, const std::string& path, TexFile& out) {
    auto file = archives->LoadFile(path);
    if ((file == nullptr) || !file->IsLoaded || (file->Buffer == nullptr) || (file->Buffer->size() < 0x50)) {
        SPDLOG_ERROR("MergeShip: Adult Mask: {} is missing", path);
        return false;
    }
    const auto& buf = *file->Buffer;
    uint32_t size;
    memcpy(&out.type, buf.data() + 0x40, 4);
    memcpy(&out.w, buf.data() + 0x44, 4);
    memcpy(&out.h, buf.data() + 0x48, 4);
    memcpy(&size, buf.data() + 0x4C, 4);
    if (buf.size() < 0x50 + size) {
        return false;
    }
    out.header.assign(buf.begin(), buf.begin() + 0x50);
    out.data.assign(buf.begin() + 0x50, buf.begin() + 0x50 + size);
    return true;
}

inline std::vector<char> WriteTex(const TexFile& like, uint32_t type, uint32_t w, uint32_t h,
                                  const std::vector<uint8_t>& data) {
    std::vector<char> out(like.header);
    const uint32_t size = (uint32_t)data.size();
    memcpy(out.data() + 0x40, &type, 4);
    memcpy(out.data() + 0x44, &w, 4);
    memcpy(out.data() + 0x48, &h, 4);
    memcpy(out.data() + 0x4C, &size, 4);
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

inline Rgba FromRgba16(uint16_t v) {
    return { (float)(((v >> 11) & 31) * 255 / 31), (float)(((v >> 6) & 31) * 255 / 31),
             (float)(((v >> 1) & 31) * 255 / 31), (v & 1) ? 255.0f : 0.0f };
}

inline uint16_t ToRgba16(const Rgba& c) {
    auto q = [](float v) { return (uint16_t)std::clamp((int)std::lround(v * 31.0f / 255.0f), 0, 31); };
    return (uint16_t)((q(c.r) << 11) | (q(c.g) << 6) | (q(c.b) << 1) | (c.a >= 128.0f ? 1 : 0));
}

inline std::vector<Rgba> Palette(const TexFile& tlut) {
    std::vector<Rgba> palette(256);
    for (size_t i = 0; (i < 256) && (i * 2 + 1 < tlut.data.size()); i++) {
        palette[i] = FromRgba16((uint16_t)((tlut.data[i * 2] << 8) | tlut.data[i * 2 + 1]));
    }
    return palette;
}

// TEXTURE_TYPE 4 is CI8 (with `palette`), 2 is RGBA16.
inline Image Decode(const TexFile& tex, const std::vector<Rgba>& palette) {
    Image im;
    im.w = (int)tex.w;
    im.h = (int)tex.h;
    im.px.resize(im.w * im.h);
    for (int i = 0; i < im.w * im.h; i++) {
        if (tex.type == 4) {
            im.px[i] = palette[tex.data[i]];
        } else {
            im.px[i] = FromRgba16((uint16_t)((tex.data[i * 2] << 8) | tex.data[i * 2 + 1]));
        }
    }
    return im;
}

inline std::vector<uint8_t> EncodeRgba16(const Image& im) {
    std::vector<uint8_t> out;
    for (const auto& c : im.px) {
        uint16_t v = ToRgba16(c);
        out.push_back((uint8_t)(v >> 8));
        out.push_back((uint8_t)v);
    }
    return out;
}

// CI8 with its own 256-colour palette: the exact colours when there are few enough, else a median cut.
inline void EncodeCi8(const Image& im, std::vector<uint8_t>& indices, std::vector<uint8_t>& tlut) {
    std::map<uint16_t, int> exact;
    for (const auto& c : im.px) {
        exact.emplace(ToRgba16(c), 0);
    }
    std::vector<uint16_t> colors;
    if (exact.size() <= 256) {
        for (auto& [v, index] : exact) {
            index = (int)colors.size();
            colors.push_back(v);
        }
    } else {
        struct Box {
            std::vector<Rgba> px;
        };
        std::vector<Box> boxes(1);
        for (const auto& c : im.px) {
            if (c.a >= 128.0f) {
                boxes[0].px.push_back(c);
            }
        }
        while (boxes.size() < 255) {
            // the box with the widest channel range is split at its median
            int best = -1;
            int channel = 0;
            float widest = 0;
            for (int i = 0; i < (int)boxes.size(); i++) {
                if (boxes[i].px.size() < 2) {
                    continue;
                }
                for (int ch = 0; ch < 3; ch++) {
                    float lo = 1e9f, hi = -1e9f;
                    for (const auto& c : boxes[i].px) {
                        float v = (ch == 0) ? c.r : ((ch == 1) ? c.g : c.b);
                        lo = std::min(lo, v);
                        hi = std::max(hi, v);
                    }
                    if (hi - lo > widest) {
                        widest = hi - lo;
                        best = i;
                        channel = ch;
                    }
                }
            }
            if (best < 0 || widest <= 0) {
                break;
            }
            auto& px = boxes[best].px;
            std::sort(px.begin(), px.end(), [channel](const Rgba& a, const Rgba& b) {
                return (channel == 0) ? (a.r < b.r) : ((channel == 1) ? (a.g < b.g) : (a.b < b.b));
            });
            Box upper;
            upper.px.assign(px.begin() + px.size() / 2, px.end());
            px.resize(px.size() / 2);
            boxes.push_back(std::move(upper));
        }
        for (const auto& box : boxes) {
            if (box.px.empty()) {
                continue;
            }
            Rgba mean{ 0, 0, 0, 255 };
            for (const auto& c : box.px) {
                mean.r += c.r;
                mean.g += c.g;
                mean.b += c.b;
            }
            mean.r /= box.px.size();
            mean.g /= box.px.size();
            mean.b /= box.px.size();
            colors.push_back(ToRgba16(mean));
        }
        colors.push_back(0); // transparent
    }
    colors.resize(256, 0);
    std::vector<Rgba> palette;
    for (uint16_t v : colors) {
        palette.push_back(FromRgba16(v));
    }
    indices.clear();
    for (const auto& c : im.px) {
        auto it = exact.find(ToRgba16(c));
        if (exact.size() <= 256) {
            indices.push_back((uint8_t)it->second);
            continue;
        }
        int best = 0;
        float bestDist = 1e30f;
        for (int i = 0; i < 256; i++) {
            if ((palette[i].a >= 128.0f) != (c.a >= 128.0f)) {
                continue;
            }
            float d = (palette[i].r - c.r) * (palette[i].r - c.r) + (palette[i].g - c.g) * (palette[i].g - c.g) +
                      (palette[i].b - c.b) * (palette[i].b - c.b);
            if (d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        indices.push_back((uint8_t)best);
    }
    tlut.clear();
    for (uint16_t v : colors) {
        tlut.push_back((uint8_t)(v >> 8));
        tlut.push_back((uint8_t)v);
    }
}

// ---- the recipe (tools/gen_adult_mask_assets.py) ---------------------------------------------------------------------

inline const Rgba kMaskSkin{ 230, 172, 115, 255 }; // the Fierce Deity mask's skin

inline Rgba MostCommon(const Image& im) {
    std::map<uint32_t, int> counts;
    for (const auto& c : im.px) {
        counts[((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b]++;
    }
    uint32_t best = 0;
    int bestCount = -1;
    for (const auto& [v, n] : counts) {
        if (n > bestCount) {
            bestCount = n;
            best = v;
        }
    }
    return { (float)((best >> 16) & 255), (float)((best >> 8) & 255), (float)(best & 255), 255 };
}

// Tint, on the warm colours only (the skin, the brows: weighted by how much redder than blue they are), so the eyes'
// whites and irises stay his -- tinted with the skin they went lavender and dull (the user, 2026-10-01).
inline void TintSkin(Image& im, const Rgba& from, const Rgba& to) {
    const float ratio[3] = { to.r / std::max(1.0f, from.r), to.g / std::max(1.0f, from.g),
                             to.b / std::max(1.0f, from.b) };
    for (auto& c : im.px) {
        const float w = std::clamp((c.r - c.b) / 80.0f, 0.0f, 1.0f);
        c.r = std::min(255.0f, c.r * (1.0f + (ratio[0] - 1.0f) * w));
        c.g = std::min(255.0f, c.g * (1.0f + (ratio[1] - 1.0f) * w));
        c.b = std::min(255.0f, c.b * (1.0f + (ratio[2] - 1.0f) * w));
    }
}

// Adult Link's skin tone onto the mask's: every channel scaled by the ratio of the two skins.
inline void Tint(Image& im, const Rgba& from, const Rgba& to) {
    const float rr = to.r / std::max(1.0f, from.r), rg = to.g / std::max(1.0f, from.g),
                rb = to.b / std::max(1.0f, from.b);
    for (auto& c : im.px) {
        c.r = std::min(255.0f, c.r * rr);
        c.g = std::min(255.0f, c.g * rg);
        c.b = std::min(255.0f, c.b * rb);
    }
}

inline Rgba Bilinear(const Image& im, float x, float y) {
    x = std::clamp(x, 0.0f, im.w - 1.0f);
    y = std::clamp(y, 0.0f, im.h - 1.0f);
    const int x0 = (int)x, y0 = (int)y;
    const int x1 = std::min(x0 + 1, im.w - 1), y1 = std::min(y0 + 1, im.h - 1);
    const float fx = x - x0, fy = y - y0;
    auto mix = [&](float a, float b, float c, float d) {
        return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
    };
    const Rgba &a = im.at(x0, y0), &b = im.at(x1, y0), &c = im.at(x0, y1), &d = im.at(x1, y1);
    return { mix(a.r, b.r, c.r, d.r), mix(a.g, b.g, c.g, d.g), mix(a.b, b.b, c.b, d.b), mix(a.a, b.a, c.a, d.a) };
}

// Adult Link's eyes moved onto a mask's sockets: his eye centres (17.5, 25) and (46.5, 25) go to the mask's (left, y)
// and (right, y), grown by the same factor so they fill the sockets; the bottom middle (which the mask's nose and chin
// reuse, clamped) fades to plain skin.
inline Image FitEyesTo(const Image& eyes, float left, float right, float centreY, float scaleY) {
    const float scaleX = (right - left) / (46.5f - 17.5f);
    const float mid = (left + right) / 2.0f;
    Image out = eyes;
    for (int y = 0; y < out.h; y++) {
        for (int x = 0; x < out.w; x++) {
            const float sx = 32.0f + (x + 0.5f - mid) / scaleX - 0.5f;
            const float sy = 25.0f + (y + 0.5f - centreY) / scaleY - 0.5f;
            out.at(x, y) = ((sy < 0) || (sy > out.h - 1)) ? kMaskSkin : Bilinear(eyes, sx, sy);
            const float fade = std::clamp((y - (centreY + 2.5f)) / 5.0f, 0.0f, 1.0f) *
                               std::clamp((9.0f - std::fabs(x + 0.5f - mid)) / 4.0f, 0.0f, 1.0f);
            if (fade > 0) {
                Rgba& c = out.at(x, y);
                c.r += (kMaskSkin.r - c.r) * fade;
                c.g += (kMaskSkin.g - c.g) * fade;
                c.b += (kMaskSkin.b - c.b) * fade;
            }
        }
    }
    return out;
}

// The Adult Mask's eyes: where its scream model has them (the two models share their mesh and UVs), (16, 22.5) and
// (48, 22.5) -- the Fierce Deity texture's own sockets, (14, 20.5) and (50, 20.5), set them too far apart (the user,
// 2026-10-01).
inline Image FitEyes(const Image& eyes) {
    return FitEyesTo(eyes, 16.0f, 48.0f, 22.5f, 1.2f);
}

// The scream's eyes: Adult Link's own, each iris (and its pupil) shrunk to `scale` of its size about its own centre,
// the white filling in around it -- his pupils constricted. `halves`: eyes side by side in the texture (2, as Link's
// 64x32 eyes; 1, a texture of one eye the model mirrors), each searched from row `top` down (below the brows).
inline Image ConstrictPupils(const Image& eyes, float scale, int halves = 2, int top = 12) {
    Image out = eyes;
    std::vector<char> iris(eyes.px.size(), 0);
    auto isIris = [&](const Rgba& c) {
        return (c.b > c.r + 40) || (((c.r + c.g + c.b) < 140) && (c.b >= c.r));
    };
    for (int half = 0; half < halves; half++) {
        const int x0 = half * (eyes.w / halves), x1 = x0 + eyes.w / halves;
        float sumX = 0, sumY = 0;
        int n = 0;
        for (int y = top; y < eyes.h; y++) {
            for (int x = x0; x < x1; x++) {
                if (isIris(eyes.at(x, y))) {
                    iris[y * eyes.w + x] = 1;
                    sumX += x;
                    sumY += y;
                    n++;
                }
            }
        }
        if (n == 0) {
            continue;
        }
        const float cx = sumX / n + 0.5f, cy = sumY / n + 0.5f;
        float radius = 0;
        std::map<uint32_t, int> whites;
        for (int y = top; y < eyes.h; y++) {
            for (int x = x0; x < x1; x++) {
                const Rgba& c = eyes.at(x, y);
                if (iris[y * eyes.w + x]) {
                    radius = std::max(radius, std::hypot(x + 0.5f - cx, y + 0.5f - cy));
                } else if ((std::min({ c.r, c.g, c.b }) > 170.0f) && (c.b >= c.r - 10.0f)) {
                    whites[((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b]++;
                }
            }
        }
        radius += 0.5f;
        Rgba white{ 235, 235, 235, 255 };
        if (!whites.empty()) {
            const auto best = std::max_element(whites.begin(), whites.end(),
                                               [](const auto& a, const auto& b) { return a.second < b.second; });
            white = { (float)((best->first >> 16) & 255), (float)((best->first >> 8) & 255),
                      (float)(best->first & 255), 255 };
        }
        for (int y = top; y < eyes.h; y++) {
            for (int x = x0; x < x1; x++) {
                if (iris[y * eyes.w + x]) {
                    out.at(x, y) = white;
                }
            }
        }
        // the shrunk iris, sampled from the original about its centre (8x8 samples a texel)
        for (int y = (int)(cy - radius) - 1; y < (int)(cy + radius) + 2; y++) {
            for (int x = (int)(cx - radius) - 1; x < (int)(cx + radius) + 2; x++) {
                if ((x < 0) || (x >= eyes.w) || (y < 0) || (y >= eyes.h)) {
                    continue;
                }
                float r = 0, g = 0, b = 0;
                int hits = 0;
                for (int sy = 0; sy < 8; sy++) {
                    for (int sx = 0; sx < 8; sx++) {
                        const float u = cx + (x + (sx + 0.5f) / 8.0f - cx) / scale;
                        const float v = cy + (y + (sy + 0.5f) / 8.0f - cy) / scale;
                        const int ix = (int)u, iy = (int)v;
                        if ((ix >= x0) && (ix < x1) && (iy >= 0) && (iy < eyes.h) && iris[iy * eyes.w + ix]) {
                            const Rgba& c = eyes.at(ix, iy);
                            r += c.r;
                            g += c.g;
                            b += c.b;
                            hits++;
                        }
                    }
                }
                if (hits > 0) {
                    const float a = hits / 64.0f;
                    Rgba& o = out.at(x, y);
                    o.r = o.r * (1 - a) + (r / hits) * a;
                    o.g = o.g * (1 - a) + (g / hits) * a;
                    o.b = o.b * (1 - a) + (b / hits) * a;
                }
            }
        }
    }
    return out;
}

// `im` in `reference`'s colours: its darkest, middle and brightest (5th, 50th, 95th percentile of brightness) take the
// colours `reference` has at `to0`, `to1` and `to2` of its brightness range (the mean within 8% of that rank), the rest
// in between -- `im`'s own shading in the reference's colours. A narrow, bright range keeps fine strands gentle (all of
// the reference's range turned the Fierce Deity's strands to camouflage).
inline void MatchColours(Image& im, const Image& reference, float to0, float to1, float to2) {
    auto lum = [](const Rgba& c) { return 0.3f * c.r + 0.59f * c.g + 0.11f * c.b; };
    if (im.px.empty() || (reference.px.size() < 8)) {
        return;
    }
    std::vector<Rgba> ref = reference.px;
    std::stable_sort(ref.begin(), ref.end(), [&](const Rgba& a, const Rgba& b) { return lum(a) < lum(b); });
    std::vector<float> levels;
    for (const auto& c : im.px) {
        levels.push_back(lum(c));
    }
    std::sort(levels.begin(), levels.end());
    const float kAt[3] = { 0.05f, 0.5f, 0.95f };
    const float kTo[3] = { to0, to1, to2 };
    float at[3];
    Rgba to[3];
    for (int i = 0; i < 3; i++) {
        at[i] = levels[(size_t)std::lround(kAt[i] * (levels.size() - 1))];
        // the reference's colour there: the mean of the texels within 8% of that rank
        const size_t n = ref.size();
        const size_t lo = (size_t)std::max(0.0f, (kTo[i] - 0.08f) * (n - 1));
        const size_t hi = (size_t)std::min((float)(n - 1), (kTo[i] + 0.08f) * (n - 1));
        Rgba sum{ 0, 0, 0, 0 };
        for (size_t k = lo; k <= hi; k++) {
            sum.r += ref[k].r;
            sum.g += ref[k].g;
            sum.b += ref[k].b;
        }
        const float count = (float)(hi - lo + 1);
        to[i] = { sum.r / count, sum.g / count, sum.b / count, 255 };
    }
    for (auto& c : im.px) {
        const float l = lum(c);
        int seg = (l <= at[1]) ? 0 : 1;
        const float span = at[seg + 1] - at[seg];
        const float t = (span > 0.0f) ? std::clamp((l - at[seg]) / span, 0.0f, 1.0f) : ((l > at[seg]) ? 1.0f : 0.0f);
        c = { to[seg].r + (to[seg + 1].r - to[seg].r) * t, to[seg].g + (to[seg + 1].g - to[seg].g) * t,
              to[seg].b + (to[seg + 1].b - to[seg].b) * t, c.a };
    }
}

inline void RecolorLuminance(Image& im, const Rgba& dark, const Rgba& light, float lo, float hi) {
    for (auto& c : im.px) {
        const float lum = (0.3f * c.r + 0.59f * c.g + 0.11f * c.b) / 255.0f;
        const float t = std::clamp((lum - lo) / (hi - lo), 0.0f, 1.0f);
        c.r = dark.r + (light.r - dark.r) * t;
        c.g = dark.g + (light.g - dark.g) * t;
        c.b = dark.b + (light.b - dark.b) * t;
    }
}

// A pause name put together from Majora's Mask's item names (128x16 IA4, centred): each piece is a source name and its
// columns.
struct NamePiece {
    const char* name;
    int x0, x1; // columns, inclusive
    int gapAfter;
};

// The letters abut, as in the names they come from (each carries its own shadow column).
inline const NamePiece kAdultNamePieces[] = {
    { "AllNightMask", 21, 30, 0 }, // A
    { "GibdoMask", 50, 56, 0 },    // d
    { "DekuMask", 55, 60, 0 },     // u
    { "BlastMask", 41, 43, 0 },    // l
    { "BlastMask", 56, 60, 4 },    // t, then the space
    { "BlastMask", 65, 94, 0 },    // Mask
};
inline const NamePiece kChildNamePieces[] = {
    { "ChateauRomani", 17, 33, 0 }, // Ch
    { "GibdoMask", 40, 42, 0 },     // i
    { "BlastMask", 41, 43, 0 },     // l
    { "GibdoMask", 50, 56, 4 },     // d, then the space
    { "BlastMask", 65, 94, 0 },     // Mask
};

template <size_t N>
inline bool MakeName(const std::shared_ptr<Ship::ArchiveManager>& archives, const std::string& mm,
                     const NamePiece (&kPieces)[N], TexFile& out) {
    std::map<std::string, TexFile> sources;
    int width = 0;
    for (const auto& piece : kPieces) {
        if (!sources.contains(piece.name) &&
            !ReadTex(archives, mm + "item_name_static/gItemName" + piece.name + "ENGTex", sources[piece.name])) {
            return false;
        }
        width += piece.x1 - piece.x0 + 1 + piece.gapAfter;
    }
    const TexFile& like = sources.begin()->second;
    if ((like.w != 128) || (like.h != 16)) {
        return false;
    }
    std::vector<uint8_t> nibbles(128 * 16, 0);
    int x = 64 - width / 2;
    for (const auto& piece : kPieces) {
        const TexFile& src = sources[piece.name];
        for (int sx = piece.x0; sx <= piece.x1; sx++, x++) {
            for (int y = 0; y < 16; y++) {
                const int si = y * 128 + sx;
                const uint8_t b = src.data[si / 2];
                nibbles[y * 128 + x] = (si & 1) ? (b & 15) : (b >> 4);
            }
        }
        x += piece.gapAfter;
    }
    std::vector<uint8_t> data(128 * 16 / 2);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = (uint8_t)((nibbles[i * 2] << 4) | nibbles[i * 2 + 1]);
    }
    out = like;
    out.data = data;
    return true;
}

// ---- building and mounting -------------------------------------------------------------------------------------------

// A mask's display list with its textures' (and palettes') hashes pointed at ours: `ours` maps each source texture's
// hash to our texture's path, `tlutFor` each of our CI8 textures to its own palette's; the source palette (`tlutHash`)
// loaded before one of our textures becomes that texture's palette. Its commands are 8 bytes (w0, w1, little endian)
// after 0x50 bytes; a G_SETTIMG by resource (0x20) is followed by the resource's hash (w0 the high half). Vertices stay
// the source's. Returns how many hashes it replaced.
inline size_t RetargetList(std::vector<char>& dl, const std::map<uint64_t, std::string>& ours, uint64_t tlutHash,
                           const std::map<std::string, std::string>& tlutFor) {
    auto cmd = [&](size_t i, uint32_t& w0, uint32_t& w1) {
        memcpy(&w0, dl.data() + 0x50 + i * 8, 4);
        memcpy(&w1, dl.data() + 0x50 + i * 8 + 4, 4);
    };
    auto setHash = [&](size_t i, uint64_t hash) {
        const uint32_t hi = (uint32_t)(hash >> 32), lo = (uint32_t)hash;
        memcpy(dl.data() + 0x50 + i * 8, &hi, 4);
        memcpy(dl.data() + 0x50 + i * 8 + 4, &lo, 4);
    };
    const size_t count = (dl.size() - 0x50) / 8;
    size_t replaced = 0;
    for (size_t i = 0; i + 1 < count; i++) {
        uint32_t w0, w1, h0, h1;
        cmd(i, w0, w1);
        if ((w0 >> 24) != 0x20) {
            continue;
        }
        cmd(i + 1, h0, h1);
        const uint64_t hash = ((uint64_t)h0 << 32) | h1;
        if (hash == tlutHash) {
            // the palette of the next texture this list loads
            for (size_t j = i + 2; j + 1 < count; j++) {
                uint32_t v0, v1, g0, g1;
                cmd(j, v0, v1);
                if ((v0 >> 24) != 0x20) {
                    continue;
                }
                cmd(j + 1, g0, g1);
                auto next = ours.find(((uint64_t)g0 << 32) | g1);
                if ((next != ours.end()) && tlutFor.contains(next->second)) {
                    setHash(i + 1, CRC64(tlutFor.at(next->second).c_str()));
                    replaced++;
                }
                break;
            }
        } else if (auto it = ours.find(hash); it != ours.end()) {
            setHash(i + 1, CRC64(it->second.c_str()));
            replaced++;
        }
        i++;
    }
    return replaced;
}

// Adds a CI8 texture with its own palette (a 16x16 RGBA16 TLUT resource) to `files`.
inline void AddCi8(std::map<std::string, std::vector<char>>& files, std::map<std::string, std::string>& tlutFor,
                   const std::string& path, const Image& image, const TexFile& like, const TexFile& tlutLike) {
    std::vector<uint8_t> indices, tlut;
    EncodeCi8(image, indices, tlut);
    files[path] = WriteTex(like, 4, image.w, image.h, indices);
    files[path + "TLUT"] = WriteTex(tlutLike, 2, 16, 16, tlut);
    tlutFor[path] = path + "TLUT";
}

inline const uint8_t kIcon[32 * 32 * 4] = {
#include <mergeship/AdultMaskIcon.inc>
};

inline const uint8_t kPhantomGanonIcon[32 * 32 * 4] = {
#include <mergeship/PhantomGanonMaskIcon.inc>
};

inline const uint8_t kGanonIcon[32 * 32 * 4] = {
#include <mergeship/GanonMaskIcon.inc>
};

inline const uint8_t kGanondorfHandIcon[32 * 32 * 4] = {
#include <mergeship/GanondorfHandIcon.inc>
};

inline const uint8_t kIronKnuckleAxeIcon[32 * 32 * 4] = {
#include <mergeship/IronKnuckleAxeIcon.inc>
};

inline const uint8_t kChildIcon[32 * 32 * 4] = {
#include <mergeship/ChildMaskIcon.inc>
};

// Rows [y0, y1) of `src` into w x h, each texel the average of a 4x4 grid of samples over its part of the source.
inline Image Resample(const Image& src, float y0, float y1, int w, int h) {
    Image out;
    out.w = w;
    out.h = h;
    out.px.resize(w * h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            Rgba sum{ 0, 0, 0, 0 };
            for (int sy = 0; sy < 4; sy++) {
                for (int sx = 0; sx < 4; sx++) {
                    const float v = y0 + (y1 - y0) * (y + (sy + 0.5f) / 4.0f) / h;
                    const float u = src.w * (x + (sx + 0.5f) / 4.0f) / w;
                    const Rgba& c = src.at(std::clamp((int)u, 0, src.w - 1), std::clamp((int)v, 0, src.h - 1));
                    sum.r += c.r;
                    sum.g += c.g;
                    sum.b += c.b;
                    sum.a += c.a;
                }
            }
            out.at(x, y) = { sum.r / 16.0f, sum.g / 16.0f, sum.b / 16.0f, sum.a / 16.0f };
        }
    }
    return out;
}

// The Child Mask's resources into `files` (the Adult Mask while Link is the adult): the Skull Kid's Link mask with its
// hair in young Link's colours, its icon and name, and its scream -- the mask with its pupils constricted and young
// Link's own open mouth (the Adult Mask's scream keeps its model's screaming mouth; this mask has none, so it takes
// that of the face it copies). Returns false (and adds nothing) when a source is missing.
inline bool MakeChild(const std::shared_ptr<Ship::ArchiveManager>& archives, const std::string& mm,
                      const TexFile& iconLike, std::map<std::string, std::vector<char>>& files) {
    const std::string stk = mm + "objects/object_stk/gSkullKidLinkMask";
    const std::string human = mm + "objects/object_link_child/gLinkHuman";
    TexFile eye, mouth, hair, openMouth, mouthTlut, youngHair, name;
    if (!ReadTex(archives, stk + "EyeTex", eye) || !ReadTex(archives, stk + "MouthTex", mouth) ||
        !ReadTex(archives, stk + "HairTex", hair) || !ReadTex(archives, human + "MouthOpenTex", openMouth) ||
        !ReadTex(archives, human + "MouthTLUT", mouthTlut) ||
        !ReadTex(archives, mm + "objects/object_link_child/object_link_child_Tex_005400", youngHair) ||
        !MakeName(archives, mm, kChildNamePieces, name)) {
        return false;
    }
    auto dlFile = archives->LoadFile(stk + "1DL");
    if ((dlFile == nullptr) || !dlFile->IsLoaded || (dlFile->Buffer == nullptr)) {
        return false;
    }
    if ((eye.type != 2) || (mouth.type != 2) || (mouth.w != 32) || (mouth.h != 16) || (hair.type != 2) ||
        (youngHair.type != 2) || (openMouth.type != 4) || (openMouth.w != 32) || (openMouth.h != 32)) {
        SPDLOG_ERROR("MergeShip: Child Mask: unexpected source texture sizes");
        return false;
    }

    // The hair in young Link's own hair colours (object_link_child_Tex_005400, his head's: the user, 2026-10-01), the
    // mask's strands kept.
    Image childHair = Decode(hair, {});
    MatchColours(childHair, Decode(youngHair, {}), 0.1f, 0.5f, 0.92f);

    // The scream: the pupils constricted (the mask's eye texture is one eye, mirrored), then the eye's white cleaned
    // the way the user cleaned it by hand (2026-10-01): the old iris's rim and highlights, and the edge of the shrunk
    // iris's blend, white. And young Link's open mouth in the mask's skin, its rows 2..28 (the mouth and its chin
    // shadow) into the mask's 32x16 mouth.
    Image screamEye = ConstrictPupils(Decode(eye, {}), 0.55f, 1, 12);
    static const uint8_t kWhite[][2] = { { 21, 13 }, { 22, 14 }, { 19, 15 }, { 11, 16 }, { 19, 16 }, { 20, 16 },
                                         { 10, 17 }, { 11, 17 }, { 20, 17 }, { 22, 17 }, { 11, 18 }, { 12, 18 },
                                         { 20, 18 }, { 22, 18 }, { 12, 19 }, { 13, 19 }, { 20, 19 }, { 10, 20 },
                                         { 12, 20 }, { 13, 20 }, { 20, 20 } };
    if ((screamEye.w == 32) && (screamEye.h == 32)) {
        for (const auto& xy : kWhite) {
            screamEye.at(xy[0], xy[1]) = { 255, 255, 255, 255 };
        }
    }
    Image open = Decode(openMouth, Palette(mouthTlut));
    Tint(open, MostCommon(open), MostCommon(Decode(mouth, {})));
    Image screamMouth = Resample(open, 2.0f, 28.0f, 32, 16);
    for (auto& c : screamMouth.px) {
        c.a = 255.0f;
    }

    const std::string dir = "mergeship/adultmask/";
    std::vector<char> normal = *dlFile->Buffer;
    std::vector<char> scream = *dlFile->Buffer;
    const std::map<uint64_t, std::string> hairOnly = {
        { CRC64((stk + "HairTex").c_str()), dir + "gChildMaskHairTex" },
    };
    const std::map<uint64_t, std::string> screamOurs = {
        { CRC64((stk + "HairTex").c_str()), dir + "gChildMaskHairTex" },
        { CRC64((stk + "EyeTex").c_str()), dir + "gChildMaskScreamEyeTex" },
        { CRC64((stk + "MouthTex").c_str()), dir + "gChildMaskScreamMouthTex" },
    };
    const size_t normalReplaced = RetargetList(normal, hairOnly, 0, {});
    const size_t screamReplaced = RetargetList(scream, screamOurs, 0, {});
    if ((normalReplaced < 1) || (screamReplaced < 3)) {
        SPDLOG_ERROR("MergeShip: Child Mask: gSkullKidLinkMask1DL is not the expected list ({}, {} textures)",
                     normalReplaced, screamReplaced);
        return false;
    }
    files[dir + "gChildMaskHairTex"] = WriteTex(hair, 2, hair.w, hair.h, EncodeRgba16(childHair));
    files[dir + "gChildMaskScreamEyeTex"] = WriteTex(eye, 2, eye.w, eye.h, EncodeRgba16(screamEye));
    files[dir + "gChildMaskScreamMouthTex"] = WriteTex(mouth, 2, 32, 16, EncodeRgba16(screamMouth));
    files[MS_CHILD_MASK_DL] = normal;
    files[MS_CHILD_MASK_SCREAM_DL] = scream;
    files[MS_CHILD_MASK_ICON] =
        WriteTex(iconLike, iconLike.type, 32, 32, std::vector<uint8_t>(std::begin(kChildIcon), std::end(kChildIcon)));
    files[MS_CHILD_MASK_NAME] = WriteTex(name, name.type, name.w, name.h, name.data);
    return true;
}

// Builds the Adult Mask's resources from the mounted archives and mounts them. `mm` and `oot` are the path prefixes
// under which this engine sees Majora's Mask's and Ocarina of Time's resources ("mm/" and "" in SoH, "mm/" and "oot/"
// in 2Ship). Returns false (and mounts nothing) when a source is missing. `scream`, when given, says whether the scream
// model was made too (its sources are Majora's Mask's own; without them the mask keeps its face as Link screams);
// `child`, whether the Child Mask's art was (without it the adult holds the Adult Mask as it is).
inline bool Mount(const std::shared_ptr<Ship::ArchiveManager>& archives, const std::string& mm, const std::string& oot,
                  bool* scream = nullptr, bool* child = nullptr) {
    const std::string fd = mm + "objects/gameplay_keep/gFierceDeityMask";
    const std::string link = oot + "objects/object_link_boy/gLinkAdult";
    TexFile fdTlut, fdEyes, fdHair, fdMouth, fdEar, fdHat, linkTlut, linkEyes, linkMouth, linkEar, fdIcon, name;
    if (!ReadTex(archives, fd + "TLUT", fdTlut) || !ReadTex(archives, fd + "EyesTex", fdEyes) ||
        !ReadTex(archives, fd + "HairTex", fdHair) || !ReadTex(archives, fd + "MouthTex", fdMouth) ||
        !ReadTex(archives, fd + "EarTex", fdEar) || !ReadTex(archives, fd + "HatTex", fdHat) ||
        !ReadTex(archives, link + "HeadTLUT", linkTlut) || !ReadTex(archives, link + "EyesOpenTex", linkEyes) ||
        !ReadTex(archives, link + "Mouth1Tex", linkMouth) || !ReadTex(archives, link + "EarTex", linkEar) ||
        !ReadTex(archives, mm + "icon_item_static_yar/gItemIconFierceDeityMaskTex", fdIcon) ||
        !MakeName(archives, mm, kAdultNamePieces, name)) {
        return false;
    }
    auto dlFile = archives->LoadFile(fd + "DL");
    if ((dlFile == nullptr) || !dlFile->IsLoaded || (dlFile->Buffer == nullptr)) {
        return false;
    }
    if ((fdEyes.w != 64) || (fdEyes.h != 32) || (linkEyes.w != 64) || (linkEyes.h != 32) || (fdMouth.w != 32) ||
        (fdMouth.h != 16) || (linkMouth.w != 32) || (linkMouth.h != 32) || (fdEar.w != linkEar.w) ||
        (fdEar.h != linkEar.h) || (fdHat.type != 2)) {
        SPDLOG_ERROR("MergeShip: Adult Mask: unexpected source texture sizes");
        return false;
    }

    const auto fdPalette = Palette(fdTlut);
    const auto linkPalette = Palette(linkTlut);
    Image eyes = Decode(linkEyes, linkPalette);
    const Rgba linkSkin = MostCommon(eyes);
    TintSkin(eyes, linkSkin, kMaskSkin);
    eyes = FitEyes(eyes);
    Image mouthFull = Decode(linkMouth, linkPalette);
    Tint(mouthFull, linkSkin, kMaskSkin);
    Image mouth;
    mouth.w = 32;
    mouth.h = 16;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 32; x++) {
            mouth.px.push_back(mouthFull.at(x, y + 4)); // his lip line is at y 12 of 32
        }
    }
    Image ear = Decode(linkEar, linkPalette);
    Tint(ear, linkSkin, kMaskSkin);
    // The hair in adult Link's own hair colours (gameplay_keep gLinkHairTex, as his head draws it: the user,
    // 2026-10-01, "match the color of child link and adult link's hair"), or a gold like it when that is missing.
    TexFile linkHair;
    const bool haveLinkHair = ReadTex(archives, oot + "objects/gameplay_keep/gLinkHairTex", linkHair) &&
                              (linkHair.type == 2);
    auto recolorHair = [&](Image& im) {
        if (haveLinkHair) {
            MatchColours(im, Decode(linkHair, {}), 0.3f, 0.6f, 0.92f);
        } else {
            RecolorLuminance(im, { 150, 95, 25, 255 }, { 235, 190, 90, 255 }, 0.5f, 1.0f);
        }
    };
    Image hair = Decode(fdHair, fdPalette);
    recolorHair(hair);
    Image hat = Decode(fdHat, fdPalette);
    RecolorLuminance(hat, { 45, 110, 30, 255 }, { 95, 170, 55, 255 }, 0.3f, 1.0f);

    std::map<std::string, std::vector<char>> files;
    const std::string dir = "mergeship/adultmask/";
    // Each CI8 texture has its own palette; the list loads one before each texture.
    std::map<std::string, std::string> tlutFor; // texture resource -> its palette's
    for (const auto& [part, image, like] : std::vector<std::tuple<std::string, const Image*, const TexFile*>>{
             { "Eyes", &eyes, &fdEyes }, { "Hair", &hair, &fdHair }, { "Mouth", &mouth, &fdMouth },
             { "Ear", &ear, &fdEar } }) {
        std::vector<uint8_t> indices, tlut;
        EncodeCi8(*image, indices, tlut);
        files[dir + "gAdultMask" + part + "Tex"] = WriteTex(*like, 4, image->w, image->h, indices);
        files[dir + "gAdultMask" + part + "TLUT"] = WriteTex(fdTlut, 2, 16, 16, tlut);
        tlutFor[dir + "gAdultMask" + part + "Tex"] = dir + "gAdultMask" + part + "TLUT";
    }
    files[dir + "gAdultMaskHatTex"] = WriteTex(fdHat, 2, hat.w, hat.h, EncodeRgba16(hat));
    files[MS_ADULT_MASK_ICON] =
        WriteTex(fdIcon, fdIcon.type, 32, 32, std::vector<uint8_t>(std::begin(kIcon), std::end(kIcon)));
    // MergeShip2: Ganondorf mode's masks (soh MergeShipGanondorf.c): their icons, rendered from Ocarina of Time's
    // models (tools/gen_ganon_mask_icons.py)
    files[MS_PHANTOM_GANON_MASK_ICON] = WriteTex(fdIcon, fdIcon.type, 32, 32,
                                                 std::vector<uint8_t>(std::begin(kPhantomGanonIcon),
                                                                      std::end(kPhantomGanonIcon)));
    files[MS_GANON_MASK_ICON] =
        WriteTex(fdIcon, fdIcon.type, 32, 32, std::vector<uint8_t>(std::begin(kGanonIcon), std::end(kGanonIcon)));
    files[MS_GANONDORF_HAND_ICON] = WriteTex(fdIcon, fdIcon.type, 32, 32,
                                             std::vector<uint8_t>(std::begin(kGanondorfHandIcon),
                                                                  std::end(kGanondorfHandIcon)));
    files[MS_IRON_KNUCKLE_AXE_ICON] = WriteTex(fdIcon, fdIcon.type, 32, 32,
                                               std::vector<uint8_t>(std::begin(kIronKnuckleAxeIcon),
                                                                    std::end(kIronKnuckleAxeIcon)));
    files[MS_ADULT_MASK_NAME] = WriteTex(name, name.type, name.w, name.h, name.data);

    // The list: gFierceDeityMaskDL with its textures (and palettes) pointed at ours; its vertices stay the mask's.
    std::vector<char> dl = *dlFile->Buffer;
    const std::map<uint64_t, std::string> ours = {
        { CRC64((fd + "EyesTex").c_str()), dir + "gAdultMaskEyesTex" },
        { CRC64((fd + "HairTex").c_str()), dir + "gAdultMaskHairTex" },
        { CRC64((fd + "MouthTex").c_str()), dir + "gAdultMaskMouthTex" },
        { CRC64((fd + "EarTex").c_str()), dir + "gAdultMaskEarTex" },
        { CRC64((fd + "HatTex").c_str()), dir + "gAdultMaskHatTex" },
    };
    const size_t replaced = RetargetList(dl, ours, CRC64((fd + "TLUT").c_str()), tlutFor);
    if (replaced < 9) {
        SPDLOG_ERROR("MergeShip: Adult Mask: gFierceDeityMaskDL is not the expected list ({} textures)", replaced);
        return false;
    }
    files[MS_ADULT_MASK_DL] = dl;

    // Link's scream: the Fierce Deity's scream mask (object_mask_boy) wearing adult Link's normal eyes and brows (the
    // same fit as above, onto its sockets), his pupils constricted; its screaming mouth stays; his hair, ear and cap.
    bool screamMade = false;
    {
        const std::string boy = mm + "objects/object_mask_boy/object_mask_boy_";
        TexFile boyTlut, boyHair, boyEyes, boyEar, boyCap;
        auto boyDl = archives->LoadFile(boy + "DL_000900");
        if (ReadTex(archives, boy + "TLUT_000D50", boyTlut) && ReadTex(archives, boy + "Tex_000F50", boyHair) &&
            ReadTex(archives, boy + "Tex_001350", boyEyes) && ReadTex(archives, boy + "Tex_001F50", boyEar) &&
            ReadTex(archives, boy + "Tex_002050", boyCap) && (boyDl != nullptr) && boyDl->IsLoaded &&
            (boyDl->Buffer != nullptr) && (boyEyes.w == 64) && (boyEyes.h == 32) && (boyEar.w == (uint32_t)ear.w) &&
            (boyEar.h == (uint32_t)ear.h) && (boyCap.type == 2)) {
            const auto boyPalette = Palette(boyTlut);
            Image screamEyes = ConstrictPupils(Decode(linkEyes, linkPalette), 0.55f);
            Tint(screamEyes, linkSkin, kMaskSkin);
            screamEyes = FitEyesTo(screamEyes, 16.0f, 48.0f, 22.5f, 1.2f); // object_mask_boy's sockets
            Image screamHair = Decode(boyHair, boyPalette);
            recolorHair(screamHair);
            Image screamCap = Decode(boyCap, boyPalette);
            RecolorLuminance(screamCap, { 45, 110, 30, 255 }, { 95, 170, 55, 255 }, 0.3f, 1.0f);

            std::map<std::string, std::string> screamTluts;
            AddCi8(files, screamTluts, dir + "gAdultMaskScreamEyesTex", screamEyes, boyEyes, boyTlut);
            AddCi8(files, screamTluts, dir + "gAdultMaskScreamHairTex", screamHair, boyHair, boyTlut);
            AddCi8(files, screamTluts, dir + "gAdultMaskScreamEarTex", ear, boyEar, boyTlut);
            files[dir + "gAdultMaskScreamCapTex"] =
                WriteTex(boyCap, 2, screamCap.w, screamCap.h, EncodeRgba16(screamCap));
            std::vector<char> screamDl = *boyDl->Buffer;
            const std::map<uint64_t, std::string> screamOurs = {
                { CRC64((boy + "Tex_001350").c_str()), dir + "gAdultMaskScreamEyesTex" },
                { CRC64((boy + "Tex_000F50").c_str()), dir + "gAdultMaskScreamHairTex" },
                { CRC64((boy + "Tex_001F50").c_str()), dir + "gAdultMaskScreamEarTex" },
                { CRC64((boy + "Tex_002050").c_str()), dir + "gAdultMaskScreamCapTex" },
            };
            // the mouth (Tex_001B50) keeps its own texture and the mask's own palette
            const size_t screamReplaced =
                RetargetList(screamDl, screamOurs, CRC64((boy + "TLUT_000D50").c_str()), screamTluts);
            if (screamReplaced >= 7) {
                files[MS_ADULT_MASK_SCREAM_DL] = screamDl;
                screamMade = true;
            } else {
                SPDLOG_ERROR("MergeShip: Adult Mask: object_mask_boy_DL_000900 is not the expected list ({} textures)",
                             screamReplaced);
            }
        }
    }
    if (scream != nullptr) {
        *scream = screamMade;
    }
    const bool childMade = MakeChild(archives, mm, fdIcon, files);
    if (!childMade) {
        SPDLOG_ERROR("MergeShip: Child Mask: its art could not be made; the adult holds the Adult Mask as it is");
    }
    if (child != nullptr) {
        *child = childMade;
    }

    uint32_t gameVersion = 0;
    for (const auto& mounted : *archives->GetArchives()) {
        if (mounted->HasGameVersion()) {
            gameVersion = mounted->GetGameVersion();
            break;
        }
    }
    auto archive = std::make_shared<MemoryArchive>(std::move(files), gameVersion);
    archive->Load();
    if (!archive->IsLoaded() || (archives->AddArchive(archive) == nullptr)) {
        SPDLOG_ERROR("MergeShip: Adult Mask: the generated archive could not be mounted");
        return false;
    }
    SPDLOG_INFO("MergeShip: Adult Mask art made from the player's archives and mounted");
    return true;
}

} // namespace MsAdultMask
