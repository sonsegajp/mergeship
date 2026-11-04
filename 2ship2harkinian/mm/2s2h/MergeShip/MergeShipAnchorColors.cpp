// MergeShip2 -- Anchor's player colours on Link and Majora's Mask's forms in Termina (MergeShipAnchorColors.h).
//
// The port of Ship of Harkinian's Anchor colouring: its HookHandlers.cpp answers VB_APPLY_TUNIC_COLOR with the local
// player's own Anchor colour (gRemote.Anchor.Color.Value) and each dummy's client colour, and its
// soh/MergeShip/MergeShipFormColors.cpp carries that colour onto every green Majora's Mask's forms wear, per Player
// actor. The same happens here, gated as SoH gates its hook (Anchor connected, or its loopback test player: the
// dummy-player hooks' condition in Network/Anchor/HookHandlers.cpp).
//
// Where the colour lives (found by decoding the forms' lists; the same sources 2S2H's cosmetic editor recolours):
//   Deku   object_link_nuts_TLUT_003EB0 entries 243-254 (leaf hat, head leaves, leaf skirt) and Tex_009068 (flower stalks).
//   Goron  object_link_goron_Tex_002780 (cap and belt) and Tex_00CEB8 (the ball).
//   Zora   object_link_zora_TLUT_005000 / TLUT_00C578 entries 151-177, 179-180, 183 (the green fading into the skin),
//          Tex_010228 (fin edges, barrier) and gameplay_keep_Tex_0700B0 (the boomerangs), pixels 80-511.
//   Fierce Deity  object_link_boy_TLUT_008128 entries 0-12 (tunic, hat, head cloth).
//   The MM human  a primitive colour in its body lists (the eleven gsDPSetPrimColor(0, 0xFF, 30, 105, 27, 255)).
//   MergeShip's adult Link  the environment colour MergeShip_AdultLink_DrawSetup sets (MergeShipAdultLink.c).
//
// How. The palettes and textures are never modified: the form lists load them through segment
// MERGESHIP_ANCHOR_COLOR_SEGMENT (see the header for why 0x07), and before each Player draws, the segment is pointed at
// a block holding recoloured copies for that player's colour (2S2H's CosmeticShading: ShadePaletteNewBase MODE_MAX for
// the Deku, Goron and Fierce Deity, ShadePaletteGradient for the Zora). The human's tunic commands become calls to a
// two-command list in the same block. A block is made once per colour and kept. The adult's environment colour is set
// in its own draw, so it needs no segment.
//
// Majora's Mask's archive holds each list twice (mm/... and the root alias; BenPort.cpp MsGfxPatchPaths): the
// skeletons draw the mm/ copies and code draws the root ones, so both are patched.
//
// 2S2H's cosmetic editor (BenGui/CosmeticEditor.cpp) recolours the same things for everybody: it bleaches the palettes
// in place and patches the lists' primitive colours. While Anchor colours the players, its tunic colours step aside:
// the blocks are made from the archive's own colours (ShadeCopyOriginalEntries undoes its edits in the copy), its
// tunic primitive colour calls are replaced by white ones, and whatever it (or MergeShipOoTEquip.cpp's OoT tunics) put
// on the human's tunic commands is set aside. When Anchor stops, every command is put back as it was, and nothing of
// the cosmetic editor was changed. The Deku's hair and the rest of those palettes keep the cosmetic editor's colours.

#include "2s2h/MergeShip/MergeShipAnchorColors.h"
#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/BenGui/CosmeticShading.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <fast/resource/ResourceType.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Texture.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/utils/StrHash64.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
void Gfx_TextureCacheDelete(const uint8_t* texAddr);
extern uintptr_t gSegments[]; // main.c
void MergeShipTest_Log(const char* fmt, ...);
extern PlayState* gPlayState;
}

namespace {

// OPEN_DISPS / CLOSE_DISPS declare these at block scope; declared C here so that, inside this namespace, they still
// name the C functions of the frame interpolation.
extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

// Majora's Mask's own colours: the human's and the forms' green (the Kokiri Tunic), the Fierce Deity's tunic
// (2S2H's Player.FierceDeityTunic default), and the ends of the Zora's green-to-skin fade (CosmeticEditor.cpp).
const Color_RGB8 kGreenDefault = { 30, 105, 27 };
const Color_RGB8 kFierceDeityDefault = { 174, 208, 207 };
const Color_RGB8 kZoraTunicBase = { 0, 74, 16 };
const Color_RGB8 kZoraSkin = { 197, 247, 247 };
// Anchor's colour when none was picked (Network/Anchor/Menu.cpp).
const Color_RGBA8 kAnchorColorDefault = { 100, 255, 100, 255 };

enum class Shade { NewBaseMax, Gradient };
enum class Source { Green, FierceDeity };

struct Range {
    u16 begin;
    u16 end;
};

struct Target {
    const char* path; // root alias of a palette or an RGBA16 texture
    Shade shade;
    Source source;
    Range ranges[3];
    s32 rangeCount;
    bool isTlut; // read by CI textures (their cached decodes must go when a block is reused)
};

const Target kTargets[] = {
    { "objects/object_link_nuts/object_link_nuts_TLUT_003EB0", Shade::NewBaseMax, Source::Green, { { 243, 254 } }, 1,
      true },
    { "objects/object_link_nuts/object_link_nuts_Tex_009068", Shade::NewBaseMax, Source::Green, { { 0, 63 } }, 1, false },
    { "objects/object_link_goron/object_link_goron_Tex_002780", Shade::NewBaseMax, Source::Green, { { 0, 127 } }, 1,
      false },
    { "objects/object_link_goron/object_link_goron_Tex_00CEB8", Shade::NewBaseMax, Source::Green, { { 0, 127 } }, 1,
      false },
    { "objects/object_link_zora/object_link_zora_TLUT_005000", Shade::Gradient, Source::Green,
      { { 151, 177 }, { 179, 180 }, { 183, 183 } }, 3, true },
    { "objects/object_link_zora/object_link_zora_TLUT_00C578", Shade::Gradient, Source::Green,
      { { 151, 177 }, { 179, 180 }, { 183, 183 } }, 3, true },
    { "objects/object_link_zora/object_link_zora_Tex_010228", Shade::Gradient, Source::Green, { { 80, 511 } }, 1, false },
    { "objects/gameplay_keep/gameplay_keep_Tex_0700B0", Shade::Gradient, Source::Green, { { 80, 511 } }, 1, false },
    { "objects/object_link_boy/object_link_boy_TLUT_008128", Shade::NewBaseMax, Source::FierceDeity, { { 0, 12 } }, 1,
      true },
};
constexpr s32 kTargetCount = (s32)ARRAY_COUNT(kTargets);

// Every display list that loads one of the targets (SoH's MergeShipFormColors.cpp list).
const char* const kDisplayLists[] = {
    "objects/object_link_nuts/gLinkDekuHatDL",
    "objects/object_link_nuts/gLinkDekuHeadDL",
    "objects/object_link_nuts/gLinkDekuTorsoDL",
    "objects/object_link_nuts/gLinkDekuWaistDL",
    "objects/object_link_nuts/object_link_nuts_DL_008660",
    "objects/object_link_nuts/object_link_nuts_DL_008760",
    "objects/object_link_nuts/object_link_nuts_DL_009AB8",
    "objects/object_link_nuts/object_link_nuts_DL_009C48",
    "objects/object_link_nuts/object_link_nuts_DL_009DB8",
    "objects/object_link_nuts/object_link_nuts_DL_00B690",
    "objects/object_link_goron/gLinkGoronCurledDL",
    "objects/object_link_goron/gLinkGoronHatDL",
    "objects/object_link_goron/gLinkGoronWaistDL",
    "objects/object_link_zora/gLinkZoraCollarDL",
    "objects/object_link_zora/gLinkZoraHatDL",
    "objects/object_link_zora/gLinkZoraHeadDL",
    "objects/object_link_zora/gLinkZoraLeftFootDL",
    "objects/object_link_zora/gLinkZoraLeftForearmDL",
    "objects/object_link_zora/gLinkZoraLeftHandClosedDL",
    "objects/object_link_zora/gLinkZoraLeftHandHoldBottleDL",
    "objects/object_link_zora/gLinkZoraLeftHandOpenDL",
    "objects/object_link_zora/gLinkZoraLeftShinDL",
    "objects/object_link_zora/gLinkZoraLeftShoulderDL",
    "objects/object_link_zora/gLinkZoraLeftThighDL",
    "objects/object_link_zora/gLinkZoraRightFootDL",
    "objects/object_link_zora/gLinkZoraRightForearmDL",
    "objects/object_link_zora/gLinkZoraRightHandClosedDL",
    "objects/object_link_zora/gLinkZoraRightHandOpenDL",
    "objects/object_link_zora/gLinkZoraRightShinDL",
    "objects/object_link_zora/gLinkZoraRightShoulderDL",
    "objects/object_link_zora/gLinkZoraRightThighDL",
    "objects/object_link_zora/gLinkZoraSheathDL",
    "objects/object_link_zora/gLinkZoraTorsoDL",
    "objects/object_link_zora/gLinkZoraWaistDL",
    "objects/object_link_zora/object_link_zora_DL_00CC38",
    "objects/object_link_zora/object_link_zora_DL_00CDA0",
    "objects/object_link_zora/object_link_zora_DL_00E088",
    "objects/object_link_zora/object_link_zora_DL_010868",
    "objects/object_link_zora/object_link_zora_DL_010978",
    "objects/object_link_zora/object_link_zora_DL_0110A8",
    "objects/gameplay_keep/gameplay_keep_DL_06FE20",
    "objects/gameplay_keep/gameplay_keep_DL_06FF68",
    "objects/object_link_boy/gLinkFierceDeityHatDL",
    "objects/object_link_boy/gLinkFierceDeityHeadDL",
    "objects/object_link_boy/gLinkFierceDeityLeftShoulderDL",
    "objects/object_link_boy/gLinkFierceDeityLeftThighDL",
    "objects/object_link_boy/gLinkFierceDeityRightShoulderDL",
    "objects/object_link_boy/gLinkFierceDeityRightThighDL",
};
constexpr s32 kDisplayListCount = (s32)ARRAY_COUNT(kDisplayLists);

// Primitive-colour commands in the lists (the indices CosmeticEditor.cpp patches). Human: the MM human's tunic
// colour, which becomes a call to the block's list. White: a form's white primitive colour, which the cosmetic editor
// replaces by a call to its tunic colour; while Anchor colours the players the call goes to a white one instead.
enum class PrimKind { Human, White };
struct PrimSite {
    const char* path;
    s32 index;
    PrimKind kind;
};
const PrimSite kPrimSites[] = {
    { "objects/object_link_child/gLinkHumanWaistDL", 5, PrimKind::Human },
    { "objects/object_link_child/gLinkHumanRightThighDL", 10, PrimKind::Human },
    { "objects/object_link_child/gLinkHumanLeftThighDL", 10, PrimKind::Human },
    { "objects/object_link_child/gLinkHumanHeadDL", 92, PrimKind::Human },
    { "objects/object_link_child/gLinkHumanHatDL", 10, PrimKind::Human },
    { "objects/object_link_child/gLinkHumanCollarDL", 5, PrimKind::Human },
    { "objects/object_link_child/gLinkHumanLeftShoulderDL", 10, PrimKind::Human },
    { "objects/object_link_child/gLinkHumanLeftShoulderDL", 65, PrimKind::Human },
    { "objects/object_link_child/gLinkHumanRightShoulderDL", 10, PrimKind::Human },
    { "objects/object_link_child/gLinkHumanRightShoulderDL", 65, PrimKind::Human },
    { "objects/object_link_child/gLinkHumanTorsoDL", 5, PrimKind::Human },
    { "objects/object_link_nuts/gLinkDekuWaistDL", 22, PrimKind::White },
    { "objects/object_link_nuts/gLinkDekuHeadDL", 55, PrimKind::White },
    { "objects/object_link_nuts/gLinkDekuHatDL", 29, PrimKind::White },
    { "objects/object_link_goron/gLinkGoronWaistDL", 16, PrimKind::White },
    { "objects/object_link_goron/gLinkGoronHatDL", 17, PrimKind::White },
    { "objects/object_link_boy/gLinkFierceDeityHatDL", 32, PrimKind::White },
    { "objects/object_link_boy/gLinkFierceDeityHeadDL", 87, PrimKind::White },
    { "objects/object_link_boy/gLinkFierceDeityRightShoulderDL", 30, PrimKind::White },
    { "objects/object_link_boy/gLinkFierceDeityLeftShoulderDL", 30, PrimKind::White },
    { "objects/object_link_boy/gLinkFierceDeityRightThighDL", 30, PrimKind::White },
    { "objects/object_link_boy/gLinkFierceDeityLeftThighDL", 36, PrimKind::White },
    { "objects/object_link_boy/gLinkFierceDeityLeftThighDL", 118, PrimKind::White },
};
constexpr s32 kPrimSiteCount = (s32)ARRAY_COUNT(kPrimSites);

// MM's own primitive colour on the human's tunic (gsDPSetPrimColor(0, 0xFF, 30, 105, 27, 255)).
constexpr u32 kHumanTunicPrimW0 = 0xFA0000FF;

// ---- the archive's two copies ------------------------------------------------------------------------------------

constexpr s32 kCopies = 2; // 0: mm/<path>, 1: the root alias

std::shared_ptr<Ship::ArchiveManager> Archives() {
    return Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
}

std::string CopyPath(const char* root, s32 copy) {
    return (copy == 0) ? (std::string("mm/") + root) : std::string(root);
}

bool HasCopy(const char* root, s32 copy) {
    return Archives()->HasFile(CopyPath(root, copy));
}

std::shared_ptr<Ship::IResource> Load(const std::string& path) {
    return Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path);
}

// The instructions of a copy of a list, null when it is missing or a custom (replaced) one.
Gfx* LoadList(const char* root, s32 copy) {
    if (!HasCopy(root, copy)) {
        return nullptr;
    }
    auto res = Load(CopyPath(root, copy));
    if ((res == nullptr) || res->GetInitData()->IsCustom ||
        (res->GetInitData()->Type != static_cast<uint32_t>(Fast::ResourceType::DisplayList))) {
        return nullptr;
    }
    auto dl = std::static_pointer_cast<Fast::DisplayList>(res);
    return dl->Instructions.empty() ? nullptr : dl->Instructions.data();
}

std::shared_ptr<Fast::Texture> LoadTexture(const std::string& path) {
    auto res = Load(path);
    if ((res == nullptr) || (res->GetInitData()->Type != static_cast<uint32_t>(Fast::ResourceType::Texture))) {
        return nullptr;
    }
    return std::static_pointer_cast<Fast::Texture>(res);
}

// ---- the archive's colours ---------------------------------------------------------------------------------------

struct Original {
    std::vector<u8> pristine; // as the archive has it
    std::vector<u8> current;  // as it is now (the cosmetic editor's colours outside our ranges)
    u64 hashes[kCopies] = {};
    u32 offset = 0; // within a block
    std::vector<u64> readers; // CI textures drawn with this palette (hashes)
};

Original sOriginals[kTargetCount];
u32 sHumanTunicOffset = 0; // a two-command list (set the human's tunic colour, end) within a block
u32 sBlockSize = 0;
s32 sOriginalsState = 0; // 0 not loaded, 1 loaded, -1 unusable

bool LoadOriginals() {
    if (sOriginalsState != 0) {
        return sOriginalsState > 0;
    }
    sOriginalsState = -1;
    u32 offset = 0;

    for (s32 i = 0; i < kTargetCount; i++) {
        const char* root = kTargets[i].path;
        // The root alias is the one the cosmetic editor edits (and ShadeCopyOriginalEntries undoes).
        s32 copy = HasCopy(root, 1) ? 1 : 0;
        auto tex = LoadTexture(CopyPath(root, copy));
        u32 needed = 0;

        for (s32 r = 0; r < kTargets[i].rangeCount; r++) {
            needed = std::max<u32>(needed, (kTargets[i].ranges[r].end + 1u) * 2u);
        }
        if ((tex == nullptr) || tex->GetInitData()->IsCustom || (tex->Type != Fast::TextureType::RGBA16bpp) ||
            ((tex->Flags & TEX_FLAG_LOAD_AS_RAW) != 0) || (tex->ImageDataSize < needed) || (tex->HByteScale != 1.0f) ||
            (tex->VPixelScale != 1.0f)) {
            MergeShipTest_Log("ANCHORCOLOR not a native RGBA16 %s", root);
            return false;
        }
        Original& o = sOriginals[i];
        o.current.assign(tex->ImageData, tex->ImageData + tex->ImageDataSize);
        o.pristine = o.current;
        if (copy == 1) {
            ShadeCopyOriginalEntries(root, o.pristine.data(), (uint32_t)o.pristine.size());
        }
        for (s32 c = 0; c < kCopies; c++) {
            o.hashes[c] = CRC64(CopyPath(root, c).c_str());
        }
        o.offset = offset;
        offset += (tex->ImageDataSize + 7) & ~7u;
    }
    sHumanTunicOffset = (offset + 15) & ~15u;
    sBlockSize = sHumanTunicOffset + (u32)(2 * sizeof(Gfx));
    sOriginalsState = 1;
    MergeShipTest_Log("ANCHORCOLOR originals loaded: %d targets, block %u bytes", kTargetCount, sBlockSize);
    return true;
}

s32 TargetOfHash(u64 hash) {
    for (s32 t = 0; t < kTargetCount; t++) {
        for (s32 c = 0; c < kCopies; c++) {
            if (sOriginals[t].hashes[c] == hash) {
                return t;
            }
        }
    }
    return -1;
}

// ---- 2S2H's shading (CosmeticShading.cpp), on a copy -------------------------------------------------------------

inline u16 Entry(const u8* data, u32 i) {
    return (u16)((data[i * 2] << 8) | data[i * 2 + 1]);
}

inline void SetEntry(u8* data, u32 i, s32 r, s32 g, s32 b, u16 original) {
    r = CLAMP(r, 0, 31);
    g = CLAMP(g, 0, 31);
    b = CLAMP(b, 0, 31);
    u16 col16 = (u16)((r << 11) | (g << 6) | (b << 1) | (original & 1));
    data[i * 2] = col16 >> 8;
    data[i * 2 + 1] = col16 & 0xFF;
}

// ShadePaletteNewBase(..., MODE_MAX): each entry's darkness below the brightest of the range, in newBase's hue.
void ShadeNewBaseMax(const u8* src, u8* dst, Range range, Color_RGB8 newBase) {
    s32 maxR = 0, maxG = 0, maxB = 0;

    for (u32 i = range.begin; i <= range.end; i++) {
        u16 c = Entry(src, i);
        maxR = std::max(maxR, (s32)(c >> 11));
        maxG = std::max(maxG, (s32)((c >> 6) & 0x1F));
        maxB = std::max(maxB, (s32)((c >> 1) & 0x1F));
    }
    for (u32 i = range.begin; i <= range.end; i++) {
        u16 c = Entry(src, i);
        s32 diff = std::max({ maxR - (s32)(c >> 11), maxG - (s32)((c >> 6) & 0x1F), maxB - (s32)((c >> 1) & 0x1F) });

        diff = CLAMP(31 - diff, 0, 31);
        SetEntry(dst, i, (diff * newBase.r) / 255, (diff * newBase.g) / 255, (diff * newBase.b) / 255, c);
    }
}

// ShadePaletteGradient: each entry's place on the fade from oldBase to targetEnd, on a fade from newBase.
void ShadeGradient(const u8* src, u8* dst, Range range, Color_RGB8 oldBase, Color_RGB8 newBase, Color_RGB8 targetEnd) {
    double oR = oldBase.r >> 3, oG = oldBase.g >> 3, oB = oldBase.b >> 3;
    double nR = newBase.r >> 3, nG = newBase.g >> 3, nB = newBase.b >> 3;
    double tR = targetEnd.r >> 3, tG = targetEnd.g >> 3, tB = targetEnd.b >> 3;
    double gR = tR - oR, gG = tG - oG, gB = tB - oB;
    double gradient = gR * gR + gG * gG + gB * gB;

    for (u32 i = range.begin; i <= range.end; i++) {
        u16 c = Entry(src, i);
        double dR = (double)(c >> 11) - oR;
        double dG = (double)((c >> 6) & 0x1F) - oG;
        double dB = (double)((c >> 1) & 0x1F) - oB;
        double position = (gradient != 0.0) ? ((gR * dR + gG * dG + gB * dB) / gradient) : 0.0;

        SetEntry(dst, i, (s32)(nR + position * (tR - nR)), (s32)(nG + position * (tG - nG)),
                 (s32)(nB + position * (tB - nB)), c);
    }
}

// ---- colour blocks -----------------------------------------------------------------------------------------------

inline bool SameColor(Color_RGB8 a, Color_RGB8 b) {
    return (a.r == b.r) && (a.g == b.g) && (a.b == b.b);
}

// One Player's colours.
struct Tints {
    Color_RGB8 green;       // every green of the Deku, Goron and Zora
    Color_RGB8 fierceDeity; // the Fierce Deity's tunic
    Color_RGB8 human;       // the MM human's tunic

    bool operator==(const Tints& o) const {
        return SameColor(green, o.green) && SameColor(fierceDeity, o.fierceDeity) && SameColor(human, o.human);
    }
};

const Tints kVanillaTints = { kGreenDefault, kFierceDeityDefault, kGreenDefault };

struct Block {
    bool used = false;
    Tints tints = {};
    u32 lastFrame = 0;
    std::vector<u8> data;
};

constexpr s32 kMaxBlocks = 16;
Block sBlocks[kMaxBlocks];
u32 sFrame = 1;

// Drops the renderer's decodes of everything `block` held, before its bytes change.
void ForgetBlockTextures(Block& block) {
    for (s32 i = 0; i < kTargetCount; i++) {
        if (!kTargets[i].isTlut) {
            Gfx_TextureCacheDelete(block.data.data() + sOriginals[i].offset);
            continue;
        }
        for (u64 reader : sOriginals[i].readers) {
            const char* name = Archives()->HashToCString(reader);
            if (name == nullptr) {
                continue;
            }
            auto tex = LoadTexture(name);
            if (tex != nullptr) {
                Gfx_TextureCacheDelete(tex->ImageData);
            }
        }
    }
}

void FillBlock(Block& block) {
    block.data.assign(sBlockSize, 0);
    for (s32 i = 0; i < kTargetCount; i++) {
        const Target& t = kTargets[i];
        const Original& o = sOriginals[i];
        u8* dst = block.data.data() + o.offset;
        Color_RGB8 color = (t.source == Source::Green) ? block.tints.green : block.tints.fierceDeity;
        bool vanilla = SameColor(color, (t.source == Source::Green) ? kGreenDefault : kFierceDeityDefault);

        memcpy(dst, o.current.data(), o.current.size());
        for (s32 r = 0; r < t.rangeCount; r++) {
            const Range range = t.ranges[r];
            if (vanilla) {
                memcpy(dst + range.begin * 2, o.pristine.data() + range.begin * 2, (range.end - range.begin + 1) * 2);
            } else if (t.shade == Shade::NewBaseMax) {
                ShadeNewBaseMax(o.pristine.data(), dst, range, color);
            } else {
                ShadeGradient(o.pristine.data(), dst, range, kZoraTunicBase, color, kZoraSkin);
            }
        }
    }

    Gfx* human = (Gfx*)(block.data.data() + sHumanTunicOffset);
    human[0].words.w0 = kHumanTunicPrimW0;
    human[0].words.w1 = ((u32)block.tints.human.r << 24) | ((u32)block.tints.human.g << 16) |
                        ((u32)block.tints.human.b << 8) | 0xFF;
    human[1].words.w0 = (uintptr_t)G_ENDDL << 24;
    human[1].words.w1 = 0;
}

Block* GetBlock(const Tints& tints) {
    Block* victim = nullptr;

    for (Block& block : sBlocks) {
        if (block.used && (block.tints == tints)) {
            block.lastFrame = sFrame;
            return &block;
        }
    }
    for (Block& block : sBlocks) {
        if (!block.used) {
            victim = &block;
            break;
        }
        // Never one used this frame: its bytes are read when the frame is rendered.
        if ((block.lastFrame != sFrame) && ((victim == nullptr) || (block.lastFrame < victim->lastFrame))) {
            victim = &block;
        }
    }
    if (victim == nullptr) {
        return nullptr;
    }
    if (victim->used) {
        ForgetBlockTextures(*victim);
    }
    victim->used = true;
    victim->tints = tints;
    victim->lastFrame = sFrame;
    FillBlock(*victim);
    MergeShipTest_Log("ANCHORCOLOR block green=(%d,%d,%d) fd=(%d,%d,%d) human=(%d,%d,%d)", tints.green.r, tints.green.g,
                      tints.green.b, tints.fierceDeity.r, tints.fierceDeity.g, tints.fierceDeity.b, tints.human.r,
                      tints.human.g, tints.human.b);
    return victim;
}

void FreeBlocks() {
    for (Block& block : sBlocks) {
        if (block.used) {
            ForgetBlockTextures(block);
        }
        block = Block();
    }
}

// ---- the display lists -------------------------------------------------------------------------------------------

inline u8 Opcode(const Gfx* g) {
    return (u8)(g->words.w0 >> 24);
}

// Commands the resource loader stores as two Gfx (the second holds a CRC64 of a resource path).
bool IsWide(u8 op) {
    return (op == G_SETTIMG_OTR_HASH) || (op == G_DL_OTR_HASH) || (op == G_VTX_OTR_HASH) || (op == G_BRANCH_Z_OTR) ||
           (op == G_MARKER) || (op == G_MTX_OTR) || (op == G_MOVEMEM_OTR);
}

inline u64 WideHash(const Gfx* second) {
    return ((u64)(u32)second->words.w0 << 32) | (u32)second->words.w1;
}

inline uintptr_t SegmentAddress(u32 offset) {
    return ((uintptr_t)MERGESHIP_ANCHOR_COLOR_SEGMENT << 24) | offset | 1;
}

inline bool SameGfx(const Gfx& a, const Gfx& b) {
    return (a.words.w0 == b.words.w0) && (a.words.w1 == b.words.w1);
}

struct Site {
    s32 index;
    s32 target;
    Gfx original[2];
};

struct ListPatch {
    Gfx* base = nullptr; // the resource's instructions when scanned
    std::vector<Site> sites;
};

ListPatch sLists[kDisplayListCount][kCopies];

struct PrimState {
    Gfx* base = nullptr;
    bool applied = false;
    Gfx displaced = {};
    Gfx mine = {};
    Gfx white[3] = {}; // PrimKind::White: the cosmetic editor's call, in white
};

PrimState sPrims[kPrimSiteCount][kCopies];

bool sPatched = false;
// gSegments[MERGESHIP_ANCHOR_COLOR_SEGMENT] before we set it (see OnPlayDrawWorldStart).
uintptr_t sSavedCpuSegment = 0;
bool sCpuSegmentSet = false;

Gfx SegmentedSetTImg(const Gfx& original, u32 offset) {
    Gfx g;
    // Same format, size and width as the G_SETTIMG_OTR_HASH it replaces; the image is in the segment.
    g.words.w0 = (original.words.w0 & 0x00FFFFFF) | ((uintptr_t)G_SETTIMG << 24);
    g.words.w1 = SegmentAddress(offset);
    return g;
}

void ScanList(ListPatch& lp, Gfx* dl) {
    s32 pendingReaderOf = -1;

    lp.base = dl;
    lp.sites.clear();
    for (s32 i = 0; Opcode(&dl[i]) != G_ENDDL; i++) {
        u8 op = Opcode(&dl[i]);

        if (op == G_SETTIMG_OTR_HASH) {
            u64 hash = WideHash(&dl[i + 1]);
            s32 t = TargetOfHash(hash);

            if (t >= 0) {
                Site site;
                site.index = i;
                site.target = t;
                site.original[0] = dl[i];
                site.original[1] = dl[i + 1];
                lp.sites.push_back(site);
                pendingReaderOf = kTargets[t].isTlut ? t : -1;
            } else if (pendingReaderOf >= 0) {
                std::vector<u64>& readers = sOriginals[pendingReaderOf].readers;
                if (std::find(readers.begin(), readers.end(), hash) == readers.end()) {
                    readers.push_back(hash);
                }
                pendingReaderOf = -1;
            }
        }
        if (IsWide(op)) {
            i++;
        }
    }
}

// The command at `index` of a list, if the list is at least that long.
Gfx* CommandAt(Gfx* dl, s32 index) {
    for (s32 i = 0; i < index; i++) {
        if (Opcode(&dl[i]) == G_ENDDL) {
            return nullptr;
        }
    }
    return &dl[index];
}

void ApplyPrim(s32 s, s32 c) {
    PrimState& st = sPrims[s][c];
    Gfx* dl = LoadList(kPrimSites[s].path, c);
    Gfx* cmd = (dl != nullptr) ? CommandAt(dl, kPrimSites[s].index) : nullptr;

    if (dl != st.base) {
        st = PrimState(); // first use, or loaded again (untouched then)
        st.base = dl;
    }
    if (cmd == nullptr) {
        return;
    }
    if (st.applied && SameGfx(*cmd, st.mine)) {
        return;
    }
    // Not ours (any more): whatever is there now is what goes back when Anchor stops.
    st.applied = false;
    if (kPrimSites[s].kind == PrimKind::Human) {
        bool vanilla = (Opcode(cmd) == G_SETPRIMCOLOR) && ((u32)cmd->words.w0 == kHumanTunicPrimW0);
        // G_DL: the cosmetic editor's Human Tunic or MergeShipOoTEquip.cpp's Goron / Zora Tunic.
        if (!vanilla && (Opcode(cmd) != G_DL)) {
            return;
        }
        st.mine.words.w0 = (uintptr_t)G_DL << 24; // gsSPDisplayList (push)
        st.mine.words.w1 = SegmentAddress(sHumanTunicOffset);
    } else {
        // Only the cosmetic editor's call to its tunic colour (gsDPSetPrimColor, gsDPPipeSync, end) is replaced.
        if (Opcode(cmd) != G_DL) {
            return;
        }
        const Gfx* called = (const Gfx*)cmd->words.w1;
        if ((called == nullptr) || (Opcode(&called[0]) != G_SETPRIMCOLOR)) {
            return;
        }
        st.white[0].words.w0 = called[0].words.w0; // its min LOD and LOD fraction
        st.white[0].words.w1 = 0xFFFFFFFF;
        st.white[1] = gsDPPipeSync();
        st.white[2] = gsSPEndDisplayList();
        st.mine.words.w0 = (uintptr_t)G_DL << 24;
        st.mine.words.w1 = (uintptr_t)st.white;
    }
    st.displaced = *cmd;
    *cmd = st.mine;
    st.applied = true;
}

void RestorePrim(s32 s, s32 c) {
    PrimState& st = sPrims[s][c];
    if (st.applied && (st.base != nullptr) && (LoadList(kPrimSites[s].path, c) == st.base)) {
        Gfx* cmd = CommandAt(st.base, kPrimSites[s].index);
        if ((cmd != nullptr) && SameGfx(*cmd, st.mine)) {
            *cmd = st.displaced;
        }
    }
    st.applied = false;
}

u32 sEnsuredFrame = 0;

void EnsurePatched() {
    s32 sites = 0;

    // Once a frame is enough: a resource is only reloaded between frames.
    if ((sPatched && (sEnsuredFrame == sFrame)) || !LoadOriginals()) {
        return;
    }
    sEnsuredFrame = sFrame;
    for (s32 l = 0; l < kDisplayListCount; l++) {
        for (s32 c = 0; c < kCopies; c++) {
            ListPatch& lp = sLists[l][c];
            Gfx* dl = LoadList(kDisplayLists[l], c);
            if (dl == nullptr) {
                lp.base = nullptr;
                lp.sites.clear();
                continue;
            }
            if (dl != lp.base) {
                ScanList(lp, dl); // first use, or the resource was loaded again (untouched then)
            }
            for (Site& site : lp.sites) {
                if (Opcode(&dl[site.index]) != G_SETTIMG_OTR_HASH) {
                    continue;
                }
                dl[site.index] = SegmentedSetTImg(site.original[0], sOriginals[site.target].offset);
                dl[site.index + 1] = gsDPNoOp();
                sites++;
            }
        }
    }
    for (s32 s = 0; s < kPrimSiteCount; s++) {
        for (s32 c = 0; c < kCopies; c++) {
            ApplyPrim(s, c);
        }
    }
    if (!sPatched) {
        MergeShipTest_Log("ANCHORCOLOR patched (%d sites)", sites);
    }
    sPatched = true;
}

void Unpatch() {
    if (!sPatched) {
        return;
    }
    for (s32 l = 0; l < kDisplayListCount; l++) {
        for (s32 c = 0; c < kCopies; c++) {
            ListPatch& lp = sLists[l][c];
            if ((lp.base == nullptr) || (LoadList(kDisplayLists[l], c) != lp.base)) {
                continue; // reloaded since: already the archive's own
            }
            for (Site& site : lp.sites) {
                Gfx* cmd = &lp.base[site.index];
                if ((Opcode(cmd) == G_SETTIMG) && (cmd->words.w1 == SegmentAddress(sOriginals[site.target].offset))) {
                    lp.base[site.index] = site.original[0];
                    lp.base[site.index + 1] = site.original[1];
                }
            }
        }
    }
    for (s32 s = 0; s < kPrimSiteCount; s++) {
        for (s32 c = 0; c < kCopies; c++) {
            RestorePrim(s, c);
        }
    }
    if (sCpuSegmentSet) {
        gSegments[MERGESHIP_ANCHOR_COLOR_SEGMENT] = sSavedCpuSegment;
        sCpuSegmentSet = false;
    }
    sPatched = false;
    // Made again from the archive (and the cosmetic editor's colours then) next time.
    FreeBlocks();
    sOriginalsState = 0;
    MergeShipTest_Log("ANCHORCOLOR unpatched");
}

// Another patcher (the cosmetic editor's Human Tunic, MergeShipOoTEquip.cpp's OoT tunics) that patched a human tunic
// command while it was ours keeps our call as that command's "original" and may write it back after Anchor stopped. A
// call through the segment then would read an unset segment: it goes back to MM's own command.
bool sEverPatched = false;

void SweepStaleHumanCalls() {
    for (s32 s = 0; s < kPrimSiteCount; s++) {
        if (kPrimSites[s].kind != PrimKind::Human) {
            continue;
        }
        for (s32 c = 0; c < kCopies; c++) {
            Gfx* dl = LoadList(kPrimSites[s].path, c);
            Gfx* cmd = (dl != nullptr) ? CommandAt(dl, kPrimSites[s].index) : nullptr;
            if ((cmd != nullptr) && (Opcode(cmd) == G_DL) &&
                ((cmd->words.w1 >> 24) == MERGESHIP_ANCHOR_COLOR_SEGMENT) && ((cmd->words.w1 & 1) != 0)) {
                cmd->words.w0 = kHumanTunicPrimW0;
                cmd->words.w1 = ((u32)kGreenDefault.r << 24) | ((u32)kGreenDefault.g << 16) |
                                ((u32)kGreenDefault.b << 8) | 0xFF;
                MergeShipTest_Log("ANCHORCOLOR stale human call %s[%d]", kPrimSites[s].path, kPrimSites[s].index);
            }
        }
    }
}

// ---- whose colours -----------------------------------------------------------------------------------------------

// SoH's condition for its VB_APPLY_TUNIC_COLOR hook: the dummy players are shown.
bool Active() {
    Anchor* anchor = Anchor::Instance;
    return (anchor != nullptr) && (anchor->isConnected || anchor->mergeShipLoopback);
}

bool IsDummy(const Actor* actor) {
    return (actor != nullptr) && (actor->id == ANCHOR_DUMMY_ACTOR_ID);
}

// Anchor's answer for `actor`: the local player's own colour, a dummy's client colour.
bool AnchorColor(Actor* actor, Color_RGB8* color) {
    if (!Active() || (actor == nullptr) || (gPlayState == nullptr)) {
        return false;
    }
    if (actor == (Actor*)GET_PLAYER(gPlayState)) {
        Color_RGBA8 own = CVarGetColor(CVAR_REMOTE_ANCHOR("Color.Value"), kAnchorColorDefault);
        *color = { own.r, own.g, own.b };
        return true;
    }
    if (!IsDummy(actor)) {
        return false;
    }
    uint32_t clientId = Anchor::Instance->GetDummyPlayerClientId(actor);
    auto it = Anchor::Instance->clients.find(clientId);
    if (it == Anchor::Instance->clients.end()) {
        return false;
    }
    *color = it->second.color;
    return true;
}

Tints ActorTints(Actor* actor) {
    Tints t = kVanillaTints;
    Color_RGB8 color;

    // SoH asks the hook for each of the three; Anchor answers all of them with the player's colour.
    if (AnchorColor(actor, &color)) {
        t.green = color;
        t.fierceDeity = color;
        t.human = color;
    }
    return t;
}

void PointSegment(PlayState* play, Block* block) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, MERGESHIP_ANCHOR_COLOR_SEGMENT, (uintptr_t)block->data.data());
    gSPSegment(POLY_XLU_DISP++, MERGESHIP_ANCHOR_COLOR_SEGMENT, (uintptr_t)block->data.data());
    CLOSE_DISPS(play->state.gfxCtx);
}

// Points the segment at `actor`'s colours for everything drawn after it.
void SetForActor(PlayState* play, Actor* actor) {
    if ((play == nullptr) || !sPatched || (sOriginalsState <= 0)) {
        return;
    }
    Block* block = GetBlock(ActorTints(actor));
    if (block != nullptr) {
        PointSegment(play, block);
    }
}

Tints sLogged = {};

void OnPlayDrawWorldStart() {
    PlayState* play = gPlayState;

    sFrame++;
    if (!Active() || (play == nullptr) || !LoadOriginals()) {
        Unpatch();
        if (sEverPatched) {
            SweepStaleHumanCalls();
        }
        return;
    }
    EnsurePatched();
    sEverPatched = sEverPatched || sPatched;
    if (!sPatched) {
        return;
    }
    // Always set: a patched list reached without it would read through an unset segment.
    Player* player = GET_PLAYER(play);
    Tints tints = (player != nullptr) ? ActorTints(&player->actor) : kVanillaTints;
    Block* block = GetBlock(tints);
    if (block != nullptr) {
        PointSegment(play, block);
        // Also the CPU's table, which each frame's first lists copy to the RSP (z_rcp.c syncSegments): a patched list
        // drawn before this point of a frame (outside the world draw) still finds the local player's colours.
        if (!sCpuSegmentSet) {
            sSavedCpuSegment = gSegments[MERGESHIP_ANCHOR_COLOR_SEGMENT];
            sCpuSegmentSet = true;
        }
        gSegments[MERGESHIP_ANCHOR_COLOR_SEGMENT] = (uintptr_t)block->data.data();
    }
    if (!(tints == sLogged)) {
        sLogged = tints;
        MergeShipTest_Log("ANCHORCOLOR player green=(%d,%d,%d)", tints.green.r, tints.green.g, tints.green.b);
    }
}

void RegisterAnchorColors() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawWorldStart>(OnPlayDrawWorldStart);

    // Before each Player draws: its own colours (the local player's, or a dummy's).
    auto beforeDraw = [](Actor* actor, bool* should) {
        if (sPatched && Active()) {
            SetForActor(gPlayState, actor);
        }
    };
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorDraw>(ACTOR_PLAYER, beforeDraw);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorDraw>(ANCHOR_DUMMY_ACTOR_ID,
                                                                                    beforeDraw);
    // The Zora's boomerangs are their own actors, drawn with the local player's colours (as in SoH).
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorDraw>(
        ACTOR_EN_BOOM, [](Actor* actor, bool* should) {
            if (sPatched && Active() && (gPlayState != nullptr)) {
                SetForActor(gPlayState, (Actor*)GET_PLAYER(gPlayState));
            }
        });
    // After a dummy: everything drawn after it is the local player's again.
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorDraw>(
        ANCHOR_DUMMY_ACTOR_ID, [](Actor* actor) {
            if (sPatched && Active() && (gPlayState != nullptr)) {
                SetForActor(gPlayState, (Actor*)GET_PLAYER(gPlayState));
            }
        });
}

RegisterShipInitFunc sAnchorColorsInit(RegisterAnchorColors, {});

} // namespace

extern "C" s32 MergeShip_AnchorColors_TunicColor(Actor* player, Color_RGB8* color) {
    return AnchorColor(player, color) ? 1 : 0;
}

// A Player drawn outside its actor draw (the pause screen's Link, MergeShipPauseLink.c): the segment at its colours.
extern "C" void MergeShip_AnchorColors_SetForPlayer(PlayState* play, Actor* player) {
    if (sPatched && Active() && (player != nullptr)) {
        SetForActor(play, player);
    }
}
