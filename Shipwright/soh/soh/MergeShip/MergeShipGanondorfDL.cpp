// MergeShip2: Ganondorf mode (MergeShipGanondorf.h) -- the items Link holds, without Link's hand.
//
// Ocarina of Time's lists for a hand holding an item draw the item and Link's hand together. A list is copied once
// and the triangles drawn while one of Link's hand textures is loaded become no-ops, so Ganondorf's own fist holds
// the item (2Ship's MergeShip_OoTEquip_RewriteDL, MS_OOTEQ_DL_STRIP_HAND, does the same for MM's young Link).

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <fast/resource/ResourceType.h>
#include <fast/resource/type/Texture.h>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "z64.h"
#include "functions.h"
Gfx* ResourceMgr_LoadGfxByName(const char* path); // ResourceManagerHelpers.h
void Gfx_TextureCacheDelete(const uint8_t* addr);
}

namespace {

std::unordered_map<std::string, std::vector<Gfx>> sStripped;

// Link's hands at both ages: the skin, the gloves and gauntlets' cuffs, the closed fist.
bool IsHandTexture(const char* name) {
    return (strstr(name, "gLinkAdultHandTex") != nullptr) || (strstr(name, "gLinkAdultUnusedHandTex") != nullptr) ||
           (strstr(name, "ClosedHand") != nullptr) || (strstr(name, "Gauntlet") != nullptr) ||
           (strstr(name, "gLinkChildHandTex") != nullptr) || (strstr(name, "gLinkChildUnusedHandTex") != nullptr);
}

// LUS's 128-bit commands: the second word is a hash.
bool IsExpanded(u8 opcode) {
    return (opcode == 0x20) || (opcode == 0x31) || (opcode == 0x32) || (opcode == 0x33) || (opcode == 0x35) ||
           (opcode == 0x36) || (opcode == 0x42);
}

} // namespace

extern "C" Gfx* MergeShip_Ganondorf_StripHandDL(const char* path) {
    if (path == nullptr) {
        return nullptr;
    }
    // The player's hand lists come as the resource's name or (Player_SetModels' lists, loaded) as the list itself: a
    // loaded list is kept by its address (a resource's lists stay where they are loaded).
    bool named = (strncmp(path, "__OTR__", 7) == 0);
    std::string key = named ? std::string(path) : ("@" + std::to_string((uintptr_t)path));
    auto found = sStripped.find(key);
    if (found != sStripped.end()) {
        return found->second.data();
    }

    Gfx* src = named ? ResourceMgr_LoadGfxByName(path) : (Gfx*)path;
    if (src == nullptr) {
        return nullptr;
    }
    std::vector<Gfx> out;
    for (size_t i = 0;; i++) {
        out.push_back(src[i]);
        u8 opcode = (u8)(src[i].words.w0 >> 24);
        if (opcode == 0xDF) { // G_ENDDL
            break;
        }
        if (IsExpanded(opcode)) {
            out.push_back(src[++i]);
        }
        if (out.size() > 0x4000) { // not a list
            return nullptr;
        }
    }

    bool dropping = false;
    size_t dropped = 0;
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    for (size_t i = 0; i < out.size(); i++) {
        u8 opcode = (u8)(out[i].words.w0 >> 24);

        if ((opcode == 0x20) && (i + 1 < out.size())) { // G_SETTIMG_OTR_HASH
            uint64_t hash = ((uint64_t)out[i + 1].words.w0 << 32) | (uint64_t)out[i + 1].words.w1;
            const std::string* name = archives->HashToString(hash);
            // a palette load keeps the texture's state; any other texture decides whether what follows is dropped
            if ((name != nullptr) && (name->find("TLUT") == std::string::npos)) {
                dropping = IsHandTexture(name->c_str());
            }
        } else if (dropping && ((opcode == 0x05) || (opcode == 0x06) || (opcode == 0x07) || (opcode == 0x26))) {
            out[i].words.w0 = 0; // G_NOOP
            out[i].words.w1 = 0;
            dropped++;
        }
        if (IsExpanded(opcode)) {
            i++;
        }
    }
    SPDLOG_INFO("MergeShip: Ganondorf mode holds {} without Link's hand ({} triangle commands dropped)", key, dropped);
    auto& stored = sStripped[key];
    stored = std::move(out);
    return stored.data();
}

// ---- his colours (MergeShipGanondorf.h: the Anchor menu's Armor and Cape) ---------------------------------------------
//
// His armour's brown and his cape's red are texels (the lists set a white primitive colour), so a coloured Ganondorf
// draws copies of his lists whose leather and cape textures are recoloured copies (G_SETTIMG to the copy in place of the
// resource's G_SETTIMG_OTR_HASH). Only the brown leather (gGanondorfLeatherTex / gYoungGanondorfLeatherTex) and the
// cape's red (gMantTex) change: the gold trim (yellow to olive, saturated) and everything else keep their colours. Each
// texel keeps its shading -- the new colour times its brightness over the recoloured texels' mean.

namespace {

enum RecolorKind { kRecolorNone, kRecolorBrown, kRecolorRed };

RecolorKind KindOf(const std::string& name) {
    // the cape, and its front on his chest and shoulders (the red cloth, the fastening)
    if ((name.find("/gMantTex") != std::string::npos) || (name.find("/gGanondorfRedFabricTex") != std::string::npos) ||
        (name.find("/gGanondorfCapeFasteningTex") != std::string::npos)) {
        return kRecolorRed;
    }
    if ((name.find("/gGanondorfLeatherTex") != std::string::npos) ||
        (name.find("/gYoungGanondorfLeatherTex") != std::string::npos)) {
        return kRecolorBrown;
    }
    return kRecolorNone;
}

bool Recolored(RecolorKind kind, float r, float g, float b) {
    float mx = std::max(r, std::max(g, b));
    float mn = std::min(r, std::min(g, b));
    float v = mx;
    float s = (mx > 0.0f) ? ((mx - mn) / mx) : 0.0f;
    float h = 0.0f;

    if (mx > mn) {
        if (mx == r) {
            h = 60.0f * fmodf((g - b) / (mx - mn), 6.0f);
        } else if (mx == g) {
            h = 60.0f * ((b - r) / (mx - mn) + 2.0f);
        } else {
            h = 60.0f * ((r - g) / (mx - mn) + 4.0f);
        }
        if (h < 0.0f) {
            h += 360.0f;
        }
    }
    if ((h >= 36.0f) && (h <= 75.0f) && (s > 0.3f)) {
        return false; // gold
    }
    if (kind == kRecolorBrown) {
        return ((h <= 35.0f) || (h >= 345.0f)) && (v < 0.75f) && (s > 0.15f);
    }
    return ((h <= 25.0f) || (h >= 330.0f)) && (s > 0.35f);
}

// RGBA16 or RGBA32 texels (big-endian, as the resource holds them), recoloured to `rgb` in place.
bool RecolorTexels(std::vector<u8>& data, Fast::TextureType type, RecolorKind kind, u32 rgb) {
    bool is16 = (type == Fast::TextureType::RGBA16bpp);
    size_t stride = is16 ? 2 : 4;
    size_t count = data.size() / stride;
    std::vector<float> lum(count, 0.0f);
    std::vector<u8> mask(count, 0);
    float sum = 0.0f;
    size_t n = 0;

    if (!is16 && (type != Fast::TextureType::RGBA32bpp)) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        float r, g, b;

        if (is16) {
            u16 c = (u16)((data[i * 2] << 8) | data[i * 2 + 1]);
            r = ((c >> 11) & 31) / 31.0f;
            g = ((c >> 6) & 31) / 31.0f;
            b = ((c >> 1) & 31) / 31.0f;
        } else {
            r = data[i * 4] / 255.0f;
            g = data[i * 4 + 1] / 255.0f;
            b = data[i * 4 + 2] / 255.0f;
        }
        lum[i] = 0.299f * r + 0.587f * g + 0.114f * b;
        if (Recolored(kind, r, g, b)) {
            mask[i] = 1;
            sum += lum[i];
            n++;
        }
    }
    if (n == 0) {
        return false;
    }
    float ref = std::max(sum / n, 0.02f);
    float tr = ((rgb >> 16) & 0xFF) / 255.0f;
    float tg = ((rgb >> 8) & 0xFF) / 255.0f;
    float tb = (rgb & 0xFF) / 255.0f;
    for (size_t i = 0; i < count; i++) {
        if (!mask[i]) {
            continue;
        }
        float k = lum[i] / ref;
        float r = std::min(tr * k, 1.0f);
        float g = std::min(tg * k, 1.0f);
        float b = std::min(tb * k, 1.0f);
        if (is16) {
            u16 c = (u16)((data[i * 2] << 8) | data[i * 2 + 1]);
            c = (u16)(((u16)(r * 31.0f + 0.5f) << 11) | ((u16)(g * 31.0f + 0.5f) << 6) |
                      ((u16)(b * 31.0f + 0.5f) << 1) | (c & 1));
            data[i * 2] = c >> 8;
            data[i * 2 + 1] = c & 0xFF;
        } else {
            data[i * 4] = (u8)(r * 255.0f + 0.5f);
            data[i * 4 + 1] = (u8)(g * 255.0f + 0.5f);
            data[i * 4 + 2] = (u8)(b * 255.0f + 0.5f);
        }
    }
    return true;
}

struct ColorVariant {
    std::vector<Gfx> dl;
    std::vector<std::vector<u8>> textures;
};

std::unordered_map<std::string, ColorVariant> sColorVariants;
std::unordered_map<std::string, bool> sHasRecolorable; // per list: whether it loads any of the textures at all

} // namespace

// Called once a frame before anything is drawn: past a few dozen colours (a colour picker being dragged), the copies
// are dropped (last frame's lists have run by now).
extern "C" void MergeShip_Ganondorf_ColorFrameBegin(void) {
    if (sColorVariants.size() <= 32) {
        return;
    }
    for (auto& entry : sColorVariants) {
        for (auto& tex : entry.second.textures) {
            Gfx_TextureCacheDelete(tex.data());
        }
    }
    sColorVariants.clear();
}

// `path`'s list with his armour's brown as `armor` and his cape's red as `cape` (0: as it is; else 0x01RRGGBB).
// Null: the list as it is will do.
extern "C" Gfx* MergeShip_Ganondorf_ColorDL(const char* path, u32 armor, u32 cape) {
    if ((path == nullptr) || (strncmp(path, "__OTR__", 7) != 0) || ((armor == 0) && (cape == 0))) {
        return nullptr;
    }
    auto known = sHasRecolorable.find(path);
    if ((known != sHasRecolorable.end()) && !known->second) {
        return nullptr;
    }
    std::string key = std::string(path) + "#" + std::to_string(armor) + "#" + std::to_string(cape);
    auto found = sColorVariants.find(key);
    if (found != sColorVariants.end()) {
        return found->second.dl.data();
    }

    Gfx* src = ResourceMgr_LoadGfxByName(path);
    if (src == nullptr) {
        return nullptr;
    }
    ColorVariant variant;
    for (size_t i = 0;; i++) {
        variant.dl.push_back(src[i]);
        u8 opcode = (u8)(src[i].words.w0 >> 24);
        if (opcode == 0xDF) {
            break;
        }
        if (IsExpanded(opcode)) {
            variant.dl.push_back(src[++i]);
        }
        if (variant.dl.size() > 0x4000) {
            return nullptr;
        }
    }

    auto resMgr = Ship::Context::GetRawInstance()->GetResourceManager();
    auto archives = resMgr->GetArchiveManager();
    bool any = false;
    bool recolorable = false;
    variant.textures.reserve(8); // (the list points into these: they never move)
    for (size_t i = 0; i + 1 < variant.dl.size(); i++) {
        u8 opcode = (u8)(variant.dl[i].words.w0 >> 24);

        if (opcode == 0x20) { // G_SETTIMG_OTR_HASH
            uint64_t hash = ((uint64_t)variant.dl[i + 1].words.w0 << 32) | (uint64_t)variant.dl[i + 1].words.w1;
            const std::string* name = archives->HashToString(hash);
            RecolorKind kind = (name != nullptr) ? KindOf(*name) : kRecolorNone;
            u32 rgb = (kind == kRecolorRed) ? cape : (kind == kRecolorBrown) ? armor : 0;

            recolorable = recolorable || (kind != kRecolorNone);
            if ((kind != kRecolorNone) && (rgb != 0) && (variant.textures.size() < 8)) {
                auto res = resMgr->LoadResource(*name);
                if ((res != nullptr) &&
                    (res->GetInitData()->Type == static_cast<uint32_t>(Fast::ResourceType::Texture))) {
                    auto tex = std::static_pointer_cast<Fast::Texture>(res);
                    std::vector<u8> data(tex->ImageData, tex->ImageData + tex->ImageDataSize);

                    if ((tex->HByteScale == 1.0f) && (tex->VPixelScale == 1.0f) &&
                        RecolorTexels(data, tex->Type, kind, rgb)) {
                        variant.textures.push_back(std::move(data));
                        Gfx_TextureCacheDelete(variant.textures.back().data());
                        // the same format, size and width; the image is the copy
                        variant.dl[i].words.w0 = (variant.dl[i].words.w0 & 0x00FFFFFF) | ((uintptr_t)0xFD << 24);
                        variant.dl[i].words.w1 = (uintptr_t)variant.textures.back().data();
                        variant.dl[i + 1].words.w0 = 0; // G_NOOP
                        variant.dl[i + 1].words.w1 = 0;
                        any = true;
                    }
                }
            }
        }
        if (IsExpanded(opcode)) {
            i++;
        }
    }
    sHasRecolorable[path] = recolorable;
    if (!any) {
        return nullptr;
    }
    auto& stored = sColorVariants[key];
    stored = std::move(variant);
    return stored.dl.data();
}

// ---- his organ (MergeShipGanondorf.h: the ocarina, played on the organ of his tower) ---------------------------------
//
// EnGanonOrgan draws the organ with the room around it (sRoomOrganAndFloorDL). A copy keeps the whole organ -- its
// keyboard, pipes, grill, corbels, canopy, friezes, pillars and edging -- and drops the room's carpet, steps and statues
// (the triangles drawn while one of those textures is loaded become no-ops).

namespace {

bool IsOrganTexture(const std::string& name) {
    static const char* kRoom[] = { "sRoomCarpetTex", "sRoomStepTex", "sRoomStatueWingTex" };
    for (const char* room : kRoom) {
        if (name.find(room) != std::string::npos) {
            return false;
        }
    }
    return true;
}

std::vector<Gfx> sOrganDL;

// Phantom Ganon's mask: his head limb's face plate, eyes, jewel and horns (not his hood or his collar).
bool IsPhantomMaskTexture(const std::string& name) {
    static const char* kMask[] = { "00BE80", "00BD80", "00BC80", "00B980", "00B380", "00AF80", "00BA80" };
    for (const char* mask : kMask) {
        if (name.find(mask) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<Gfx> sPhantomMaskDL;

// A copy of display list `path` with the triangles drawn under the textures `keep` refuses turned to G_NOOPs.
bool FilterDL(const char* path, bool (*keep)(const std::string&), std::vector<Gfx>& result) {
    Gfx* src = ResourceMgr_LoadGfxByName(path);
    if (src == nullptr) {
        return false;
    }
    std::vector<Gfx> out;
    for (size_t i = 0;; i++) {
        out.push_back(src[i]);
        u8 opcode = (u8)(src[i].words.w0 >> 24);
        if (opcode == 0xDF) {
            break;
        }
        if (IsExpanded(opcode)) {
            out.push_back(src[++i]);
        }
        if (out.size() > 0x8000) {
            return false;
        }
    }
    bool dropping = true; // until a texture says otherwise
    size_t dropped = 0;
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    for (size_t i = 0; i < out.size(); i++) {
        u8 opcode = (u8)(out[i].words.w0 >> 24);

        if ((opcode == 0x20) && (i + 1 < out.size())) { // G_SETTIMG_OTR_HASH
            uint64_t hash = ((uint64_t)out[i + 1].words.w0 << 32) | (uint64_t)out[i + 1].words.w1;
            const std::string* name = archives->HashToString(hash);
            if ((name != nullptr) && (name->find("TLUT") == std::string::npos)) {
                dropping = !keep(*name);
            }
        } else if (dropping && ((opcode == 0x05) || (opcode == 0x06) || (opcode == 0x07) || (opcode == 0x26))) {
            out[i].words.w0 = 0; // G_NOOP
            out[i].words.w1 = 0;
            dropped++;
        } else if ((opcode == 0xDE) && (((uintptr_t)out[i].words.w1 >> 24) < 0x10)) {
            // a list by segment, its owner's at run time (Phantom Ganon's segment 8: his fading body) -- not ours
            out[i].words.w0 = 0;
            out[i].words.w1 = 0;
        }
        if (IsExpanded(opcode)) {
            i++;
        }
    }
    SPDLOG_INFO("MergeShip: Ganondorf mode: {} ({} triangle commands dropped)", path, dropped);
    result = std::move(out);
    return true;
}

} // namespace

extern "C" Gfx* MergeShip_Ganondorf_OrganDL(void) {
    if (sOrganDL.empty() &&
        !FilterDL("__OTR__overlays/ovl_En_Ganon_Organ/sRoomOrganAndFloorDL", IsOrganTexture, sOrganDL)) {
        return nullptr;
    }
    return sOrganDL.data();
}

// The Iron Knuckle's axe (his axe, the Biggoron Sword's place): its list colours its parts through segments 8, 9 and
// 10 (EnIk_Draw's func_80A761B0 lists: prim and env colours); in his draw those segments are his (his eyes), so the copy
// calls the Iron Knuckle's own colour lists in their place -- the silver Iron Knuckle's.
namespace {
Gfx sAxeColor8[] = { gsDPPipeSync(), gsDPSetPrimColor(0, 0, 255, 255, 255, 255), gsDPSetEnvColor(180, 180, 180, 255),
                     gsSPEndDisplayList() };
Gfx sAxeColor9[] = { gsDPPipeSync(), gsDPSetPrimColor(0, 0, 225, 205, 115, 255), gsDPSetEnvColor(25, 20, 0, 255),
                     gsSPEndDisplayList() };
std::vector<Gfx> sAxeDL;
} // namespace

extern "C" Gfx* MergeShip_Ganondorf_IronKnuckleAxeDL(void) {
    if (!sAxeDL.empty()) {
        return sAxeDL.data();
    }
    Gfx* src = ResourceMgr_LoadGfxByName("__OTR__objects/object_ik/gIronKnuckleAxeDL");
    if (src == nullptr) {
        return nullptr;
    }
    std::vector<Gfx> out;
    for (size_t i = 0;; i++) {
        out.push_back(src[i]);
        u8 opcode = (u8)(src[i].words.w0 >> 24);
        if (opcode == 0xDF) {
            break;
        }
        if (IsExpanded(opcode)) {
            out.push_back(src[++i]);
        }
        if (out.size() > 0x8000) {
            return nullptr;
        }
    }
    for (size_t i = 0; i < out.size(); i++) {
        u8 opcode = (u8)(out[i].words.w0 >> 24);

        if ((opcode == 0xDE) && (((uintptr_t)out[i].words.w1 >> 24) < 0x10)) {
            u32 segment = (u32)((uintptr_t)out[i].words.w1 >> 24);
            Gfx* g = &out[i];

            if (segment == 8) {
                gSPDisplayList(g, sAxeColor8);
            } else if ((segment == 9) || (segment == 10)) {
                gSPDisplayList(g, sAxeColor9);
            } else {
                out[i].words.w0 = 0; // (none other: G_NOOP)
                out[i].words.w1 = 0;
            }
        }
        if (IsExpanded(opcode)) {
            i++;
        }
    }
    sAxeDL = std::move(out);
    return sAxeDL.data();
}

extern "C" Gfx* MergeShip_Ganondorf_PhantomMaskDL(void) {
    if (sPhantomMaskDL.empty() &&
        !FilterDL("__OTR__objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C628DL_0080F8", IsPhantomMaskTexture,
                  sPhantomMaskDL)) {
        return nullptr;
    }
    return sPhantomMaskDL.data();
}
