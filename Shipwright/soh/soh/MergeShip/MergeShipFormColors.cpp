// MergeShip2 -- tunic colours on Majora's Mask's forms (MergeShipFormColors.h).
//
// Where the colour lives. None of the forms is green through a primitive or environment colour: every one
// of their display lists sets a white primitive colour, and the green is in the texels. Decoding the
// forms' display lists in mm.o2r (every G_SETTIMG, the palette it loads, the palette entries each texture
// really uses) finds exactly these green sources, which are also the ones 2Ship2Harkinian's cosmetics
// editor recolours for its Deku/Goron/Zora/Fierce Deity tunic options (2s2h/BenGui/CosmeticEditor.cpp):
//   Deku   object_link_nuts_TLUT_003EB0 entries 243-254: the leaf hat, the leaves on the head and the
//          leaf skirt (Tex_0043B0 is drawn only from those entries; entries 109-242 are the Deku's
//          yellow "hair", which MM colours separately and is left alone).
//          object_link_nuts_Tex_009068: the green stalks of the flower propellers in the Deku's hands.
//   Goron  object_link_goron_Tex_002780 (the cap and the belt) and Tex_00CEB8 (the green of the ball).
//   Zora   object_link_zora_TLUT_005000 and TLUT_00C578 entries 151-177, 179-180, 183: the green fading
//          into the Zora's skin on the head fin, arms, fins and trunks; Tex_010228 (the fins' boomerang
//          edge and the barrier shield) and gameplay_keep_Tex_0700B0 (the thrown boomerangs), pixels 80-511.
//   Fierce Deity  object_link_boy_TLUT_008128 entries 0-12: the tunic, the hat and the cloth on the head.
//
// How it is recoloured. The recolouring is 2S2H's (CosmeticShading.cpp): ShadePaletteNewBase(MODE_MAX)
// for the Deku, Goron and Fierce Deity (2S2H bleaches those entries white and multiplies a primitive
// colour in; the product is the same, baked into the palette), ShadePaletteGradient for the Zora, whose
// green fades into its skin. 2S2H overwrites the game's own palettes, so it has one colour for everybody;
// here every Player actor can have its own (Anchor draws other players in their own colours), so the
// palettes are never modified. Instead the form lists load their green palettes and textures through
// segment MERGESHIP_FORM_COLOR_SEGMENT, and each Player points that segment at a block holding its own
// recoloured copies before it is drawn. A block is made once per colour and kept.
//
// MM's human as young Link (MergeShipChildLinkDraw.inc) is green through a primitive colour in its body
// lists; those commands become calls to a two-command list in the same block, so every young Link is drawn
// in his own tunic colour too (MergeShipChildLinkDraw.inc's rewrite of those commands can only give every
// Player drawn in a frame the same one).
//
// The default colours change nothing: while every Player drawn wears the default Kokiri Tunic green and
// the default Fierce Deity tunic, the display lists are left exactly as mm.o2r has them.

#include "soh/MergeShip/MergeShipFormColors.h"
#include "soh/frame_interpolation.h" // C linkage for OPEN_DISPS's FrameInterpolation_Record* calls

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <fast/resource/ResourceType.h>
#include <fast/resource/type/Texture.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/utils/StrHash64.h>

#include <algorithm>
#include <cstring>
#include <vector>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "soh/MergeShip/MergeShipForms.h"
void Gfx_TextureCacheDelete(const uint8_t* addr);
void MergeShipTest_Log(const char* fmt, ...);
extern PlayState* gPlayState;
}

#define CVAR_FORM_COLORS_NAME CVAR_ENHANCEMENT("MergeShip.FormTunicColors")
// MergeShip2 (soh_fd): 0 off, 1 the Deku, Goron and Zora only (the Fierce Deity keeps MM's colours), 2 every form.
// The old checkbox's 1 reads as the Deku, Goron and Zora.
#define CVAR_FORM_COLORS_DEFAULT 2
#define FORM_COLORS_DEKU_GORON_ZORA 1

namespace {

// OPEN_DISPS / CLOSE_DISPS redeclare these at block scope; declared C here so that, inside this namespace, they
// still name the C functions in frame_interpolation.cpp.
extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

// Ocarina of Time's Kokiri Tunic (z_player_lib.c sTunicColors[PLAYER_TUNIC_KOKIRI], the cosmetics
// editor's default for Link.KokiriTunic).
const Color_RGB8 kKokiriTunicDefault = { 30, 105, 27 };
// The cosmetics editor's default for Link.FierceDeityTunic (2S2H's Player.FierceDeityTunic default).
const Color_RGB8 kFierceDeityTunicDefault = { 174, 208, 207 };
// 2S2H CosmeticEditor.cpp zoraTunicBaseColor / zoraSkinColor: the ends of the Zora's green-to-skin fade.
const Color_RGB8 kZoraTunicBase = { 0, 74, 16 };
const Color_RGB8 kZoraSkin = { 197, 247, 247 };

enum class Shade { NewBaseMax, Gradient };
enum class Source { Green, FierceDeity };

struct Range {
    u16 begin;
    u16 end;
};

struct Target {
    const char* path; // MM_ASSET path of a palette or an RGBA16 texture
    Shade shade;
    Source source;
    Range ranges[3];
    s32 rangeCount;
    bool isTlut; // read by CI textures (their cached decodes must go when a block is reused)
};

const Target kTargets[] = {
    { MM_ASSET("objects/object_link_nuts/object_link_nuts_TLUT_003EB0"), Shade::NewBaseMax, Source::Green,
      { { 243, 254 } }, 1, true },
    { MM_ASSET("objects/object_link_nuts/object_link_nuts_Tex_009068"), Shade::NewBaseMax, Source::Green,
      { { 0, 63 } }, 1, false },
    { MM_ASSET("objects/object_link_goron/object_link_goron_Tex_002780"), Shade::NewBaseMax, Source::Green,
      { { 0, 127 } }, 1, false },
    { MM_ASSET("objects/object_link_goron/object_link_goron_Tex_00CEB8"), Shade::NewBaseMax, Source::Green,
      { { 0, 127 } }, 1, false },
    { MM_ASSET("objects/object_link_zora/object_link_zora_TLUT_005000"), Shade::Gradient, Source::Green,
      { { 151, 177 }, { 179, 180 }, { 183, 183 } }, 3, true },
    { MM_ASSET("objects/object_link_zora/object_link_zora_TLUT_00C578"), Shade::Gradient, Source::Green,
      { { 151, 177 }, { 179, 180 }, { 183, 183 } }, 3, true },
    { MM_ASSET("objects/object_link_zora/object_link_zora_Tex_010228"), Shade::Gradient, Source::Green,
      { { 80, 511 } }, 1, false },
    { MM_ASSET("objects/gameplay_keep/gameplay_keep_Tex_0700B0"), Shade::Gradient, Source::Green, { { 80, 511 } },
      1, false },
    { MM_ASSET("objects/object_link_boy/object_link_boy_TLUT_008128"), Shade::NewBaseMax, Source::FierceDeity,
      { { 0, 12 } }, 1, true },
};
constexpr s32 kTargetCount = (s32)ARRAY_COUNT(kTargets);

// Every display list in mm.o2r that loads one of the targets (found by decoding all of them).
const char* const kDisplayLists[] = {
    MM_ASSET("objects/object_link_nuts/gLinkDekuHatDL"),
    MM_ASSET("objects/object_link_nuts/gLinkDekuHeadDL"),
    MM_ASSET("objects/object_link_nuts/gLinkDekuTorsoDL"),
    MM_ASSET("objects/object_link_nuts/gLinkDekuWaistDL"),
    MM_ASSET("objects/object_link_nuts/object_link_nuts_DL_008660"),
    MM_ASSET("objects/object_link_nuts/object_link_nuts_DL_008760"),
    MM_ASSET("objects/object_link_nuts/object_link_nuts_DL_009AB8"),
    MM_ASSET("objects/object_link_nuts/object_link_nuts_DL_009C48"),
    MM_ASSET("objects/object_link_nuts/object_link_nuts_DL_009DB8"),
    MM_ASSET("objects/object_link_nuts/object_link_nuts_DL_00B690"),
    MM_ASSET("objects/object_link_goron/gLinkGoronCurledDL"),
    MM_ASSET("objects/object_link_goron/gLinkGoronHatDL"),
    MM_ASSET("objects/object_link_goron/gLinkGoronWaistDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraCollarDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraHatDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraHeadDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraLeftFootDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraLeftForearmDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraLeftHandClosedDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraLeftHandHoldBottleDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraLeftHandOpenDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraLeftShinDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraLeftShoulderDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraLeftThighDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraRightFootDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraRightForearmDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraRightHandClosedDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraRightHandOpenDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraRightShinDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraRightShoulderDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraRightThighDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraSheathDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraTorsoDL"),
    MM_ASSET("objects/object_link_zora/gLinkZoraWaistDL"),
    MM_ASSET("objects/object_link_zora/object_link_zora_DL_00CC38"),
    MM_ASSET("objects/object_link_zora/object_link_zora_DL_00CDA0"),
    MM_ASSET("objects/object_link_zora/object_link_zora_DL_00E088"),
    MM_ASSET("objects/object_link_zora/object_link_zora_DL_010868"),
    MM_ASSET("objects/object_link_zora/object_link_zora_DL_010978"),
    MM_ASSET("objects/object_link_zora/object_link_zora_DL_0110A8"),
    MM_ASSET("objects/gameplay_keep/gameplay_keep_DL_06FE20"),
    MM_ASSET("objects/gameplay_keep/gameplay_keep_DL_06FF68"),
    MM_ASSET("objects/object_link_boy/gLinkFierceDeityHatDL"),
    MM_ASSET("objects/object_link_boy/gLinkFierceDeityHeadDL"),
    MM_ASSET("objects/object_link_boy/gLinkFierceDeityLeftShoulderDL"),
    MM_ASSET("objects/object_link_boy/gLinkFierceDeityLeftThighDL"),
    MM_ASSET("objects/object_link_boy/gLinkFierceDeityRightShoulderDL"),
    MM_ASSET("objects/object_link_boy/gLinkFierceDeityRightThighDL"),
};
constexpr s32 kDisplayListCount = (s32)ARRAY_COUNT(kDisplayLists);

const char* StripOtr(const char* path) {
    return (strncmp(path, "__OTR__", 7) == 0) ? path + 7 : path;
}

// ---- the untouched data --------------------------------------------------------------------------------

struct Original {
    std::vector<u8> data;
    u64 hash = 0;
    u32 offset = 0; // within a block
    std::vector<u64> readers; // CI textures drawn with this palette (hashes)
};

Original sOriginals[kTargetCount];
u32 sHumanTunicOffset = 0; // a two-command list (set the MM human's tunic colour, end) within a block
u32 sBlockSize = 0;
s32 sOriginalsState = 0; // 0 not loaded, 1 loaded, -1 unusable

bool LoadOriginals() {
    if (sOriginalsState != 0) {
        return sOriginalsState > 0;
    }
    sOriginalsState = -1;
    auto resMgr = Ship::Context::GetRawInstance()->GetResourceManager();
    u32 offset = 0;

    for (s32 i = 0; i < kTargetCount; i++) {
        auto res = resMgr->LoadResource(StripOtr(kTargets[i].path));
        if ((res == nullptr) || (res->GetInitData()->Type != static_cast<uint32_t>(Fast::ResourceType::Texture))) {
            MergeShipTest_Log("FORMCOLOR missing %s", kTargets[i].path + 7);
            return false;
        }
        auto tex = std::static_pointer_cast<Fast::Texture>(res);
        u32 needed = 0;
        for (s32 r = 0; r < kTargets[i].rangeCount; r++) {
            needed = std::max<u32>(needed, (kTargets[i].ranges[r].end + 1u) * 2u);
        }
        if (res->GetInitData()->IsCustom || (tex->Type != Fast::TextureType::RGBA16bpp) ||
            (tex->ImageDataSize < needed) || (tex->HByteScale != 1.0f) || (tex->VPixelScale != 1.0f)) {
            MergeShipTest_Log("FORMCOLOR not a native RGBA16 %s", kTargets[i].path + 7);
            return false;
        }
        sOriginals[i].data.assign(tex->ImageData, tex->ImageData + tex->ImageDataSize);
        sOriginals[i].hash = CRC64(StripOtr(kTargets[i].path));
        sOriginals[i].offset = offset;
        offset += (tex->ImageDataSize + 7) & ~7u;
    }
    sHumanTunicOffset = (offset + 15) & ~15u;
    sBlockSize = sHumanTunicOffset + (u32)(2 * sizeof(Gfx));
    sOriginalsState = 1;
    MergeShipTest_Log("FORMCOLOR originals loaded: %d targets, block %u bytes", kTargetCount, sBlockSize);
    return true;
}

// ---- 2S2H's shading (CosmeticShading.cpp), on a copy ---------------------------------------------------

inline u16 Entry(const u8* data, u32 i) {
    return (u16)((data[i * 2] << 8) | data[i * 2 + 1]);
}

inline void SetEntry(u8* data, u32 i, s32 r, s32 g, s32 b, u16 original) {
    r = CLAMP(r, 0, 31);
    g = CLAMP(g, 0, 31);
    b = CLAMP(b, 0, 31);
    // 2S2H writes alpha 1; these entries are all opaque, and keeping the original bit keeps it so.
    u16 col16 = (u16)((r << 11) | (g << 6) | (b << 1) | (original & 1));
    data[i * 2] = col16 >> 8;
    data[i * 2 + 1] = col16 & 0xFF;
}

// 2S2H ShadePaletteNewBase(..., MODE_MAX): keeps each entry's darkness below the brightest of the range and
// takes its hue from newBase.
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

// 2S2H ShadePaletteGradient / mapNewBaseColorToGradient: each entry's place on the fade from oldBase to
// targetEnd is kept, on a fade from newBase to targetEnd.
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

// ---- colour blocks -------------------------------------------------------------------------------------

inline bool SameColor(Color_RGB8 a, Color_RGB8 b) {
    return (a.r == b.r) && (a.g == b.g) && (a.b == b.b);
}

// One Player's colours.
struct Tints {
    Color_RGB8 green;       // every green of the Deku, Goron and Zora: the Kokiri Tunic
    Color_RGB8 fierceDeity; // the Fierce Deity's tunic
    Color_RGB8 human;       // MM's human as young Link (MergeShipChildLinkDraw.inc): the tunic he wears

    bool IsVanilla() const {
        return SameColor(green, kKokiriTunicDefault) && SameColor(fierceDeity, kFierceDeityTunicDefault) &&
               SameColor(human, kKokiriTunicDefault);
    }
    bool operator==(const Tints& o) const {
        return SameColor(green, o.green) && SameColor(fierceDeity, o.fierceDeity) && SameColor(human, o.human);
    }
};

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
    auto resMgr = Ship::Context::GetRawInstance()->GetResourceManager();

    for (s32 i = 0; i < kTargetCount; i++) {
        if (!kTargets[i].isTlut) {
            Gfx_TextureCacheDelete(block.data.data() + sOriginals[i].offset);
            continue;
        }
        for (u64 reader : sOriginals[i].readers) {
            const char* name = resMgr->GetArchiveManager()->HashToCString(reader);
            if (name == nullptr) {
                continue;
            }
            auto res = resMgr->LoadResource(name);
            if ((res != nullptr) && (res->GetInitData()->Type == static_cast<uint32_t>(Fast::ResourceType::Texture))) {
                Gfx_TextureCacheDelete(std::static_pointer_cast<Fast::Texture>(res)->ImageData);
            }
        }
    }
}

// MM's own primitive colour on the human's tunic (gLinkHuman*DL: gsDPSetPrimColor(0, 0xFF, 30, 105, 27, 255)).
constexpr u32 kHumanTunicPrimW0 = 0xFA0000FF;
constexpr u32 kHumanTunicPrimW1 = 0x1E691BFF;

void FillBlock(Block& block) {
    block.data.assign(sBlockSize, 0);
    for (s32 i = 0; i < kTargetCount; i++) {
        const Target& t = kTargets[i];
        const std::vector<u8>& src = sOriginals[i].data;
        u8* dst = block.data.data() + sOriginals[i].offset;
        Color_RGB8 color = (t.source == Source::Green) ? block.tints.green : block.tints.fierceDeity;
        bool vanilla = (t.source == Source::Green) ? SameColor(color, kKokiriTunicDefault)
                                                   : SameColor(color, kFierceDeityTunicDefault);

        memcpy(dst, src.data(), src.size());
        if (vanilla) {
            continue;
        }
        for (s32 r = 0; r < t.rangeCount; r++) {
            if (t.shade == Shade::NewBaseMax) {
                ShadeNewBaseMax(src.data(), dst, t.ranges[r], color);
            } else {
                ShadeGradient(src.data(), dst, t.ranges[r], kZoraTunicBase, color, kZoraSkin);
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
        // Never one another Player uses this frame: its bytes are read when the frame is rendered.
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
    MergeShipTest_Log("FORMCOLOR block green=(%d,%d,%d) fd=(%d,%d,%d) human=(%d,%d,%d)", tints.green.r,
                      tints.green.g, tints.green.b, tints.fierceDeity.r, tints.fierceDeity.g, tints.fierceDeity.b,
                      tints.human.r, tints.human.g, tints.human.b);
    return victim;
}

// ---- the display lists ---------------------------------------------------------------------------------

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
    return ((uintptr_t)MERGESHIP_FORM_COLOR_SEGMENT << 24) | offset | 1;
}

struct Site {
    s32 index;
    Gfx original[2];
};

struct ListPatch {
    Gfx* base = nullptr; // the resource's instructions when scanned
    std::vector<Site> sites;
};

ListPatch sLists[kDisplayListCount];

// The MM human's tunic: each gsDPSetPrimColor(0, 0xFF, 30, 105, 27, 255) in its body lists (the commands
// 2S2H's Player.HumanTunic option replaces; MergeShipChildLinkDraw.inc rewrites the same ones). Each becomes
// a call to the block's two-command list.
struct HumanSite {
    const char* path;
    s32 index;
};
const HumanSite kHumanSites[] = {
    { MM_ASSET("objects/object_link_child/gLinkHumanWaistDL"), 5 },
    { MM_ASSET("objects/object_link_child/gLinkHumanRightThighDL"), 10 },
    { MM_ASSET("objects/object_link_child/gLinkHumanLeftThighDL"), 10 },
    { MM_ASSET("objects/object_link_child/gLinkHumanHeadDL"), 92 },
    { MM_ASSET("objects/object_link_child/gLinkHumanHatDL"), 10 },
    { MM_ASSET("objects/object_link_child/gLinkHumanCollarDL"), 5 },
    { MM_ASSET("objects/object_link_child/gLinkHumanLeftShoulderDL"), 10 },
    { MM_ASSET("objects/object_link_child/gLinkHumanLeftShoulderDL"), 65 },
    { MM_ASSET("objects/object_link_child/gLinkHumanRightShoulderDL"), 10 },
    { MM_ASSET("objects/object_link_child/gLinkHumanRightShoulderDL"), 65 },
    { MM_ASSET("objects/object_link_child/gLinkHumanTorsoDL"), 5 },
};
constexpr s32 kHumanSiteCount = (s32)ARRAY_COUNT(kHumanSites);
Gfx* sHumanSiteBase[kHumanSiteCount];

bool sPatched = false;
bool sOtherTintSeen = false; // a Player other than GET_PLAYER was drawn in a non-default colour
bool sOtherTintSeenLastFrame = false;

Gfx SegmentedSetTImg(const Gfx& original, u32 offset) {
    Gfx g;
    // Same format, size and width as the G_SETTIMG_OTR_HASH it replaces; the image is in the segment.
    g.words.w0 = (original.words.w0 & 0x00FFFFFF) | ((uintptr_t)G_SETTIMG << 24);
    g.words.w1 = SegmentAddress(offset);
    return g;
}

void ScanList(s32 l, Gfx* dl) {
    ListPatch& lp = sLists[l];
    s32 pendingReaderOf = -1;

    lp.base = dl;
    lp.sites.clear();
    for (s32 i = 0; Opcode(&dl[i]) != G_ENDDL; i++) {
        u8 op = Opcode(&dl[i]);

        if (op == G_SETTIMG_OTR_HASH) {
            u64 hash = WideHash(&dl[i + 1]);
            s32 t;

            for (t = 0; t < kTargetCount; t++) {
                if (sOriginals[t].hash == hash) {
                    break;
                }
            }
            if (t < kTargetCount) {
                Site site;
                site.index = i;
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

s32 TargetOfSite(const Site& site) {
    u64 hash = WideHash(&site.original[1]);
    for (s32 t = 0; t < kTargetCount; t++) {
        if (sOriginals[t].hash == hash) {
            return t;
        }
    }
    return -1;
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

u32 sEnsuredFrame = 0;

void EnsurePatched() {
    s32 sites = 0;

    // Once a frame is enough: a resource is only reloaded between frames.
    if ((sPatched && (sEnsuredFrame == sFrame)) || !LoadOriginals()) {
        return;
    }
    sEnsuredFrame = sFrame;
    for (s32 l = 0; l < kDisplayListCount; l++) {
        if (ResourceMgr_FileIsCustomByName(kDisplayLists[l])) {
            sLists[l].base = nullptr;
            sLists[l].sites.clear();
            continue;
        }
        Gfx* dl = ResourceMgr_LoadGfxByName(kDisplayLists[l]);
        if (dl == nullptr) {
            continue;
        }
        if (dl != sLists[l].base) {
            // First use, or the resource was loaded again (it is untouched then).
            ScanList(l, dl);
        }
        for (Site& site : sLists[l].sites) {
            s32 t = TargetOfSite(site);
            if ((t < 0) || (Opcode(&dl[site.index]) != G_SETTIMG_OTR_HASH)) {
                continue;
            }
            dl[site.index] = SegmentedSetTImg(site.original[0], sOriginals[t].offset);
            dl[site.index + 1] = gsDPNoOp();
            sites++;
        }
    }
    for (s32 h = 0; h < kHumanSiteCount; h++) {
        sHumanSiteBase[h] = nullptr;
        if (ResourceMgr_FileIsCustomByName(kHumanSites[h].path)) {
            continue;
        }
        Gfx* dl = ResourceMgr_LoadGfxByName(kHumanSites[h].path);
        Gfx* cmd = (dl != nullptr) ? CommandAt(dl, kHumanSites[h].index) : nullptr;
        if (cmd == nullptr) {
            continue;
        }
        sHumanSiteBase[h] = dl;
        if ((Opcode(cmd) == G_SETPRIMCOLOR) && ((u32)cmd->words.w0 == kHumanTunicPrimW0)) {
            cmd->words.w0 = (uintptr_t)G_DL << 24; // gsSPDisplayList (push)
            cmd->words.w1 = SegmentAddress(sHumanTunicOffset);
            sites++;
        }
    }
    if (!sPatched) {
        MergeShipTest_Log("FORMCOLOR patched (%d sites)", sites);
    }
    sPatched = true;
}

void Unpatch() {
    if (!sPatched) {
        return;
    }
    for (s32 l = 0; l < kDisplayListCount; l++) {
        if (sLists[l].base == nullptr) {
            continue;
        }
        Gfx* dl = ResourceMgr_LoadGfxByName(kDisplayLists[l]);
        if (dl != sLists[l].base) {
            continue; // reloaded since: already the archive's own
        }
        for (Site& site : sLists[l].sites) {
            if (Opcode(&dl[site.index]) == G_SETTIMG) {
                dl[site.index] = site.original[0];
                dl[site.index + 1] = site.original[1];
            }
        }
    }
    for (s32 h = 0; h < kHumanSiteCount; h++) {
        Gfx* dl = (sHumanSiteBase[h] != nullptr) ? ResourceMgr_LoadGfxByName(kHumanSites[h].path) : nullptr;
        if ((dl == nullptr) || (dl != sHumanSiteBase[h])) {
            continue;
        }
        Gfx* cmd = &dl[kHumanSites[h].index];
        if ((Opcode(cmd) == G_DL) && (cmd->words.w1 == SegmentAddress(sHumanTunicOffset))) {
            cmd->words.w0 = kHumanTunicPrimW0;
            cmd->words.w1 = kHumanTunicPrimW1;
        }
    }
    sPatched = false;
    MergeShipTest_Log("FORMCOLOR unpatched");
}

// ---- whose colours -------------------------------------------------------------------------------------

Color_RGB8 CosmeticColor(const char* changedCvar, const char* valueCvar, Color_RGB8 defaultColor) {
    return CVarGetInteger(changedCvar, 0) ? CVarGetColor24(valueCvar, defaultColor) : defaultColor;
}

Tints PlayerTints(Player* player) {
    // z_player_lib.c sTunicColors, with the cosmetics editor's colours as Player_DrawImpl applies them.
    static const Color_RGB8 kTunics[] = { { 30, 105, 27 }, { 100, 20, 0 }, { 0, 60, 100 } };
    Tints t;

    t.green = CosmeticColor(CVAR_COSMETIC("Link.KokiriTunic.Changed"), CVAR_COSMETIC("Link.KokiriTunic.Value"),
                            kKokiriTunicDefault);
    // MergeShip2 (soh_fd): "Deku, Goron, Zora" leaves the Fierce Deity in Majora's Mask's colours.
    bool tintDeity = CVarGetInteger(CVAR_FORM_COLORS_NAME, CVAR_FORM_COLORS_DEFAULT) != FORM_COLORS_DEKU_GORON_ZORA;
    t.fierceDeity = tintDeity ? CosmeticColor(CVAR_COSMETIC("Link.FierceDeityTunic.Changed"),
                                              CVAR_COSMETIC("Link.FierceDeityTunic.Value"), kFierceDeityTunicDefault)
                              : kFierceDeityTunicDefault;
    switch (player->currentTunic) {
        case PLAYER_TUNIC_GORON:
            t.human = CosmeticColor(CVAR_COSMETIC("Link.GoronTunic.Changed"), CVAR_COSMETIC("Link.GoronTunic.Value"),
                                    kTunics[PLAYER_TUNIC_GORON]);
            break;
        case PLAYER_TUNIC_ZORA:
            t.human = CosmeticColor(CVAR_COSMETIC("Link.ZoraTunic.Changed"), CVAR_COSMETIC("Link.ZoraTunic.Value"),
                                    kTunics[PLAYER_TUNIC_ZORA]);
            break;
        default:
            t.human = t.green;
            break;
    }
    // The same hook Player_DrawImpl asks for Link's tunic: Anchor answers with each player's colour.
    GameInteractor_Should(VB_APPLY_TUNIC_COLOR, true, (void*)player, &t.green);
    if (tintDeity) {
        GameInteractor_Should(VB_APPLY_TUNIC_COLOR, true, (void*)player, &t.fierceDeity);
    }
    GameInteractor_Should(VB_APPLY_TUNIC_COLOR, true, (void*)player, &t.human);
    return t;
}

bool Enabled() {
    return CVarGetInteger(CVAR_FORM_COLORS_NAME, CVAR_FORM_COLORS_DEFAULT) && MergeShip_IsMajorasMaskMounted();
}

void PointSegment(PlayState* play, Block* block) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, MERGESHIP_FORM_COLOR_SEGMENT, (uintptr_t)block->data.data());
    gSPSegment(POLY_XLU_DISP++, MERGESHIP_FORM_COLOR_SEGMENT, (uintptr_t)block->data.data());
    gSPSegment(OVERLAY_DISP++, MERGESHIP_FORM_COLOR_SEGMENT, (uintptr_t)block->data.data());
    CLOSE_DISPS(play->state.gfxCtx);
}

Tints sLogged = {};

void FormColors_OnPlayDrawBegin() {
    PlayState* play = gPlayState;

    sFrame++;
    sOtherTintSeenLastFrame = sOtherTintSeen;
    sOtherTintSeen = false;

    if (!Enabled() || (play == nullptr) || !LoadOriginals()) {
        Unpatch();
        return;
    }

    Player* player = GET_PLAYER(play);
    Tints tints = { kKokiriTunicDefault, kFierceDeityTunicDefault, kKokiriTunicDefault };
    if (player != nullptr) {
        tints = PlayerTints(player);
    }

    // The lists are changed only while someone is drawn in a colour that is not MM's own.
    if (!tints.IsVanilla() || sOtherTintSeenLastFrame) {
        EnsurePatched();
    } else {
        Unpatch();
    }

    // Always set: a patched list reached without it would call through an unset segment.
    Block* block = GetBlock(tints);
    if (block != nullptr) {
        PointSegment(play, block);
    }
    if (!(tints == sLogged)) {
        sLogged = tints;
        MergeShipTest_Log("FORMCOLOR player green=(%d,%d,%d) fd=(%d,%d,%d) human=(%d,%d,%d) patched=%d",
                          tints.green.r, tints.green.g, tints.green.b, tints.fierceDeity.r, tints.fierceDeity.g,
                          tints.fierceDeity.b, tints.human.r, tints.human.g, tints.human.b, sPatched);
    }
}

void RegisterFormColors() {
    COND_HOOK(OnPlayDrawBegin, true, FormColors_OnPlayDrawBegin);
}

RegisterShipInitFunc sFormColorsInit(RegisterFormColors, {});

} // namespace

extern "C" void MergeShip_FormColors_SetForPlayer(PlayState* play, Player* player) {
    if ((play == nullptr) || (player == nullptr) || !Enabled() || !LoadOriginals()) {
        return;
    }

    Tints tints = PlayerTints(player);
    if (!tints.IsVanilla()) {
        if (player != GET_PLAYER(play)) {
            sOtherTintSeen = true;
        }
        EnsurePatched();
    }

    Block* block = GetBlock(tints);
    if (block != nullptr) {
        PointSegment(play, block);
    }
}
