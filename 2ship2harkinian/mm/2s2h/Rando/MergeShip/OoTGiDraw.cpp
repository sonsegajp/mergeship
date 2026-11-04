// MergeShip2 -- the get-item models of the unified randomizer's Hyrule items in Termina (OoTGiDraw.h).
//
// A port of Ocarina of Time's z_draw.c (Shipwright soh/src/code/z_draw.c: GetItem_Draw and its draw functions) onto
// Majora's Mask's graphics API (Gfx_SetupDL25_Opa/Xlu, MATRIX_FINALIZE_AND_LOAD), with the table generated from SoH's
// source into OoTGiData.inc, and of the oot.o2r parts of SoH's randomizer draws (draw.cpp Randomizer_DrawMap,
// Randomizer_DrawCompass, Randomizer_DrawSilverRupee, Randomizer_DrawBossSoul, Randomizer_DrawBeanSprout,
// Randomizer_DrawScarecrowsSong). SoH's cosmetic colour options are SoH's; the defaults are drawn.

#include "OoTGiDraw.h"

#include <unordered_map>

#include "Rando/Rando.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "align_asset_macro.h"
#include "2s2h/MergeShip/MergeShipAssets.h"
}

namespace {

// OPEN_DISPS / CLOSE_DISPS declare these at block scope; inside this namespace they need their C linkage declared.
extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

// OoT z_draw.c's draw functions (GetItem_Draw<Name>).
enum MsOoTGiFunc : u8 {
    MSOOTGI_None, // a row whose model is not in oot.o2r
    MSOOTGI_MaskOrBombchu,
    MSOOTGI_SoldOut,
    MSOOTGI_BlueFire,
    MSOOTGI_Poes,
    MSOOTGI_Fairy,
    MSOOTGI_MirrorShield,
    MSOOTGI_SkullToken,
    MSOOTGI_EggOrMedallion,
    MSOOTGI_Compass,
    MSOOTGI_Potion,
    MSOOTGI_GoronSword,
    MSOOTGI_DekuNuts,
    MSOOTGI_RecoveryHeart,
    MSOOTGI_Fish,
    MSOOTGI_Opa0,
    MSOOTGI_Opa0Xlu1,
    MSOOTGI_Xlu01,
    MSOOTGI_Opa10Xlu2,
    MSOOTGI_MagicArrow,
    MSOOTGI_MagicSpell,
    MSOOTGI_Opa1023,
    MSOOTGI_Opa10Xlu32,
    MSOOTGI_SmallRupee,
    MSOOTGI_Scale,
    MSOOTGI_BulletBag,
    MSOOTGI_Wallet,
    MSOOTGI_JewelKokiri,
    MSOOTGI_JewelGoron,
    MSOOTGI_JewelZora,
    MSOOTGI_GenericMusicNote,
    MSOOTGI_TriforcePiece,
    MSOOTGI_FishingPole,
};

struct MsOoTGiRow {
    MsOoTGiFunc func;
    const char* dl[8];
};

// How a proxy is drawn (tools/gen_cross_gi.py OOT_KEY_KINDS).
enum MsOoTGiKeyKind : u8 {
    MSOGK_OoTGid,     // OoT's GetItem_Draw of `gid`, tinted by `tint`
    MSOGK_BossSoul,   // SoH's soul flame in boss `arg`'s colour, and the boss
    MSOGK_BeanSprout, // SoH's bean soul: OoT's magic bean sprout
    MSOGK_Scarecrow,  // SoH's Scarecrow's Song: OoT's note in its colour
    MSOGK_MMGid,      // a Majora's Mask item: 2Ship's own GetItem_Draw of MM GID `gid`
    MSOGK_Triforce,   // 2Ship's own Triforce piece
};

enum MsOoTGiTint : u8 {
    MSOGT_None,
    MSOGT_Map,     // SoH MapOrCompassColor[arg]
    MSOGT_Compass, // SoH MapOrCompassColor[arg]
    MSOGT_Silver,  // SoH Randomizer_DrawSilverRupee
};

struct MsOoTGiKeyRow {
    MsItemKey key;
    MsOoTGiKeyKind kind;
    s16 gid;
    MsOoTGiTint tint;
    u8 arg;
};

#include "OoTGiData.inc"

// Not get-item models, but the oot.o2r models SoH's randomizer draws for these items.
static const ALIGN_ASSET(2) char sMagicBeanSeedlingDL[] = OOT_ASSET("objects/object_mamenoki/gMagicBeanSeedlingDL");
static const ALIGN_ASSET(2) char sFishingLureFloatDL[] = OOT_ASSET("objects/object_fish/gFishingLureFloatDL");
static const ALIGN_ASSET(2) char sFishingLureHookDL[] = OOT_ASSET("objects/object_fish/gFishingLureHookDL");

const MsOoTGiKeyRow* FindKey(MsItemKey key) {
    static std::unordered_map<int, const MsOoTGiKeyRow*> sRows;
    if (sRows.empty()) {
        for (const MsOoTGiKeyRow& row : kMsOoTGiKeys) {
            sRows[row.key] = &row;
        }
    }
    auto it = sRows.find((int)key);
    return (it != sRows.end()) ? it->second : nullptr;
}

#define MS_DL(res) ((Gfx*)(res))
#define MS_LOAD(dl) MATRIX_FINALIZE_AND_LOAD(dl, gfxCtx)

// SoH draw.cpp MapOrCompassColor.
const Color_RGB8 kMapOrCompassColor[10] = {
    { 4, 100, 46 },   { 140, 30, 30 },  { 30, 60, 255 },  { 4, 195, 46 },    { 237, 95, 95 },
    { 85, 180, 223 }, { 222, 158, 47 }, { 126, 16, 177 }, { 227, 110, 255 }, { 0, 255, 255 },
};

// OoT GetItem_Draw (z_draw.c) for draw id `gid`. False when the row has no model in oot.o2r.
bool DrawOoTGid(PlayState* play, s16 gid) {
    if ((gid < 0) || (gid >= MSOOTGID_COUNT) || (kMsOoTGiTable[gid].func == MSOOTGI_None)) {
        return false;
    }
    const MsOoTGiRow& e = kMsOoTGiTable[gid];
    const char* const* d = e.dl;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frames = play->state.frames;
    // GetItem_DrawJewel* colours: primXlu, envXlu (primOpa, envOpa are the same for the three).
    u8 jewel[6] = { 0 };

    OPEN_DISPS(gfxCtx);

    switch (e.func) {
        case MSOOTGI_None:
            break;
        case MSOOTGI_MaskOrBombchu:
            Gfx_SetupDL26_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            break;
        case MSOOTGI_SoldOut:
            POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_5);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[0]));
            break;
        case MSOOTGI_BlueFire:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 16, 32, 1, frames, -(frames * 8), 16, 32, 0, 0, 1, -8));
            Matrix_Push();
            Matrix_Translate(-8.0f, -2.0f, 0.0f, MTXMODE_APPLY);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            Matrix_Pop();
            break;
        case MSOOTGI_Poes:
        case MSOOTGI_Fairy:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            if (e.func == MSOOTGI_Poes) {
                gSPSegment(POLY_XLU_DISP++, 0x08,
                           (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 16, 32, 1, frames, -(frames * 6), 16, 32, 0, 0, 1, -6));
            } else {
                gSPSegment(POLY_XLU_DISP++, 0x08,
                           (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 32, 32, 1, frames, -(frames * 6), 32, 32, 0, 0, 1, -6));
            }
            Matrix_Push();
            Matrix_ReplaceRotation(&play->billboardMtxF);
            MS_LOAD(POLY_XLU_DISP++);
            if (e.func == MSOOTGI_Poes) {
                gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[3]));
            }
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[2]));
            Matrix_Pop();
            break;
        case MSOOTGI_MirrorShield:
            Gfx_SetupDL25_Opa(gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, (frames * 2) % 256, 64, 64, 1, 0, frames % 128, 32, 32, 0, 2,
                                          0, 1));
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            break;
        case MSOOTGI_SkullToken:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, -(frames * 5), 32, 32, 1, 0, 0, 32, 64, 0, -5, 0, 0));
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            break;
        case MSOOTGI_EggOrMedallion:
            Gfx_SetupDL26_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[1]));
            break;
        case MSOOTGI_Compass:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_5);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            break;
        case MSOOTGI_Potion:
            Gfx_SetupDL25_Opa(gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, -frames, frames, 32, 32, 1, -frames, frames, 32, 32, -1, 1, -1,
                                          1));
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[1]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[2]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[3]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[4]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[5]));
            break;
        case MSOOTGI_GoronSword:
            Gfx_SetupDL25_Opa(gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, frames, 0, 32, 32, 1, 0, 0, 32, 32, 1, 0, 0, 0));
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            break;
        case MSOOTGI_DekuNuts:
            Gfx_SetupDL25_Opa(gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, frames * 6, frames * 6, 32, 32, 1, frames * 6, frames * 6, 32, 32,
                                          6, 6, 6, 6));
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            break;
        case MSOOTGI_RecoveryHeart:
            Gfx_SetupDL25_Xlu(gfxCtx);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, -(frames * 3), 32, 32, 1, 0, -(frames * 2), 32, 32, 0, -3, 0,
                                          -2));
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[0]));
            break;
        case MSOOTGI_Fish:
            Gfx_SetupDL25_Xlu(gfxCtx);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, frames, 32, 32, 1, 0, frames, 32, 32, 0, 1, 0, 1));
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[0]));
            break;
        case MSOOTGI_Opa0:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            break;
        case MSOOTGI_Opa0Xlu1:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            break;
        case MSOOTGI_Xlu01:
            Gfx_SetupDL25_Xlu(gfxCtx);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[0]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            break;
        case MSOOTGI_Opa10Xlu2:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[1]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[2]));
            break;
        case MSOOTGI_MagicArrow:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[2]));
            break;
        case MSOOTGI_MagicSpell:
            Gfx_SetupDL25_Xlu(gfxCtx);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, frames * 2, -(frames * 6), 32, 32, 1, frames, -(frames * 2), 32,
                                          32, 2, -6, 1, -2));
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[0]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[2]));
            break;
        case MSOOTGI_Opa1023:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[1]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[2]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[3]));
            break;
        case MSOOTGI_SmallRupee:
            Matrix_Scale(0.7f, 0.7f, 0.7f, MTXMODE_APPLY);
            [[fallthrough]];
        case MSOOTGI_Opa10Xlu32:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[1]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[3]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[2]));
            break;
        case MSOOTGI_Scale:
            Gfx_SetupDL25_Xlu(gfxCtx);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, frames * 2, -(frames * 2), 64, 64, 1, frames * 4, -(frames * 4),
                                          32, 32, 2, -2, 4, -4));
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[2]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[3]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[0]));
            break;
        case MSOOTGI_BulletBag:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[1]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[2]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[3]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[4]));
            break;
        case MSOOTGI_Wallet:
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[1]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            for (s32 i = 2; i < 8; i++) {
                gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[i]));
            }
            break;
        case MSOOTGI_JewelKokiri:
        case MSOOTGI_JewelGoron:
        case MSOOTGI_JewelZora: {
            // OoT GetItem_DrawJewelKokiri / Goron / Zora -> GetItem_DrawJewel.
            static const u8 kJewelXlu[3][6] = {
                { 255, 255, 160, 0, 255, 0 }, { 255, 170, 255, 255, 0, 100 }, { 50, 255, 255, 50, 0, 150 }
            };
            const u8* c = kJewelXlu[e.func - MSOOTGI_JewelKokiri];
            for (s32 i = 0; i < 6; i++) {
                jewel[i] = c[i];
            }
            gSPSegment(POLY_XLU_DISP++, 0x09,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 255, 64, 64, 1, 0, 255, 16, 16, 0, 0, 0, 0));
            gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)Gfx_TexScrollEx(gfxCtx, 0, 0, 16, 16, 0, 0));
            Matrix_Push();
            Matrix_RotateZYX(0, -0x4000, 0x4000, MTXMODE_APPLY);
            MS_LOAD(POLY_XLU_DISP++);
            MS_LOAD(POLY_OPA_DISP++);
            Gfx_SetupDL25_Xlu(gfxCtx);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 128, jewel[0], jewel[1], jewel[2], 255);
            gDPSetEnvColor(POLY_XLU_DISP++, jewel[3], jewel[4], jewel[5], 255);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Opa(gfxCtx);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 128, 255, 255, 170, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 150, 120, 0, 255);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[1]));
            Matrix_Pop();
            break;
        }
        case MSOOTGI_GenericMusicNote: {
            // SoH GetItem_DrawGenericMusicNote: the note, greyscale-tinted in the song's colour.
            static const u8 kSongColors[7][3] = {
                { 255, 255, 255 }, { 109, 73, 143 }, { 217, 110, 48 }, { 62, 109, 23 },
                { 237, 231, 62 },  { 98, 177, 211 }, { 146, 146, 146 },
            };
            s32 slot = gid - MSOOTGID_SONG_GENERIC;
            if ((slot < 0) || (slot >= 7)) {
                slot = 0;
            }
            MS_LOAD(POLY_XLU_DISP++);
            gDPSetGrayscaleColor(POLY_XLU_DISP++, kSongColors[slot][0], kSongColors[slot][1], kSongColors[slot][2],
                                 255);
            gSPGrayscale(POLY_XLU_DISP++, true);
            Gfx_SetupDL25_Xlu(gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[0]));
            gSPGrayscale(POLY_XLU_DISP++, false);
            break;
        }
        case MSOOTGI_TriforcePiece:
            // SoH's Triforce pieces are soh.o2r's: never a row with a model here (gen_cross_gi.py).
            break;
        case MSOOTGI_FishingPole:
            // SoH GetItem_DrawFishingPole: its rod is soh.o2r's; the lure and hooks are Ocarina of Time's.
            Gfx_SetupDL25_Opa(gfxCtx);
            Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);
            if (d[0] != nullptr) {
                MS_LOAD(POLY_OPA_DISP++);
                gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            }
            Matrix_Push();
            Matrix_Scale(5.0f, 5.0f, 5.0f, MTXMODE_APPLY);
            Matrix_Translate(0.0f, -25.5f, -4.0f, MTXMODE_APPLY);
            Matrix_RotateZF(M_PI / -2, MTXMODE_APPLY);
            Matrix_RotateYF((M_PI / -2) - 0.2f, MTXMODE_APPLY);
            Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
            Gfx_SetupDL25_Opa(gfxCtx);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(sFishingLureFloatDL));
            Matrix_RotateYF(0.2f, MTXMODE_APPLY);
            Matrix_Translate(0.0f, 0.0f, -300.0f, MTXMODE_APPLY);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(sFishingLureHookDL));
            Matrix_RotateZF(M_PI / 2, MTXMODE_APPLY);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(sFishingLureHookDL));
            Matrix_Translate(0.0f, -2200.0f, 700.0f, MTXMODE_APPLY);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(sFishingLureHookDL));
            Matrix_RotateZF(M_PI / 2, MTXMODE_APPLY);
            MS_LOAD(POLY_OPA_DISP++);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(sFishingLureHookDL));
            Matrix_Pop();
            break;
    }

    CLOSE_DISPS(gfxCtx);
    return true;
}

// SoH Randomizer_DrawMap / Randomizer_DrawCompass / Randomizer_DrawSilverRupee (its default colours).
bool DrawTinted(PlayState* play, const MsOoTGiKeyRow& row) {
    if ((row.gid < 0) || (row.gid >= MSOOTGID_COUNT)) {
        return false;
    }
    const char* const* d = kMsOoTGiTable[row.gid].dl;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    const Color_RGB8 color = kMapOrCompassColor[(row.arg < 10) ? row.arg : 0];

    if ((d[0] == nullptr) || ((row.tint == MSOGT_Compass) && (d[1] == nullptr)) ||
        ((row.tint == MSOGT_Silver) && (d[2] == nullptr))) {
        return false;
    }
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL25_Opa(gfxCtx);
    MS_LOAD(POLY_OPA_DISP++);
    switch (row.tint) {
        case MSOGT_Map:
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, color.r, color.g, color.b, 255);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            break;
        case MSOGT_Compass:
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, color.r, color.g, color.b, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, color.r / 2, color.g / 2, color.b / 2, 255);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_5);
            MS_LOAD(POLY_XLU_DISP++);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[1]));
            break;
        case MSOGT_Silver:
            // GID_RUPEE_GOLD's rows: [0] inner, [2] outer.
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 255 / 5, 255 / 5, 255 / 5, 255);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(d[0]));
            Gfx_SetupDL25_Xlu(gfxCtx);
            MS_LOAD(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_XLU_DISP++, 191, 191, 191, 255);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(d[2]));
            break;
        case MSOGT_None:
            break;
    }
    CLOSE_DISPS(gfxCtx);
    return true;
}

// ---- SoH draw.cpp's boss souls: the boss itself (DrawGohma ... DrawGanon), its skeleton from oot.o2r -------------


static const ALIGN_ASSET(2) char sGohmaSkel[] = OOT_ASSET("objects/object_goma/gGohmaSkel");
static const ALIGN_ASSET(2) char sGohmaIdleCrouchedAnim[] = OOT_ASSET("objects/object_goma/gGohmaIdleCrouchedAnim");
static const ALIGN_ASSET(2) char sKingDodongoSkel[] =
    OOT_ASSET("objects/object_kingdodongo/object_kingdodongo_Skel_01B310");
static const ALIGN_ASSET(2) char sKingDodongoAnim[] =
    OOT_ASSET("objects/object_kingdodongo/object_kingdodongo_Anim_00F0D8");
static const ALIGN_ASSET(2) char sBarinadeBodySkel[] = OOT_ASSET("objects/object_bv/gBarinadeBodySkel");
static const ALIGN_ASSET(2) char sBarinadeBodyAnim[] = OOT_ASSET("objects/object_bv/gBarinadeBodyAnim");
static const ALIGN_ASSET(2) char sBarinadeDL_008D70[] = OOT_ASSET("objects/object_bv/gBarinadeDL_008D70");
static const ALIGN_ASSET(2) char sBarinadeDL_008BB8[] = OOT_ASSET("objects/object_bv/gBarinadeDL_008BB8");
static const ALIGN_ASSET(2) char sPhantomGanonSkel[] = OOT_ASSET("objects/object_gnd/gPhantomGanonSkel");
static const ALIGN_ASSET(2) char sPhantomGanonNeutralAnim[] = OOT_ASSET("objects/object_gnd/gPhantomGanonNeutralAnim");
static const ALIGN_ASSET(2) char sVolvagiaHeadSkel[] = OOT_ASSET("objects/object_fd/gVolvagiaHeadSkel");
static const ALIGN_ASSET(2) char sVolvagiaHeadEmergeAnim[] = OOT_ASSET("objects/object_fd/gVolvagiaHeadEmergeAnim");
static const ALIGN_ASSET(2) char sVolvagiaEyeOpenTex[] = OOT_ASSET("objects/object_fd/gVolvagiaEyeOpenTex");
static const ALIGN_ASSET(2) char sMorphaCoreMembraneDL[] = OOT_ASSET("objects/object_mo/gMorphaCoreMembraneDL");
static const ALIGN_ASSET(2) char sMorphaCoreNucleusDL[] = OOT_ASSET("objects/object_mo/gMorphaCoreNucleusDL");
static const ALIGN_ASSET(2) char sBongoLeftHandSkel[] = OOT_ASSET("objects/object_sst/gBongoLeftHandSkel");
static const ALIGN_ASSET(2) char sBongoLeftHandIdleAnim[] = OOT_ASSET("objects/object_sst/gBongoLeftHandIdleAnim");
static const ALIGN_ASSET(2) char sKotakeHeadDL[] = OOT_ASSET("objects/object_tw/gTwinrovaKotakeHeadDL");
static const ALIGN_ASSET(2) char sKotakeIceHairDL[] = OOT_ASSET("objects/object_tw/gTwinrovaKotakeIceHairDL");
static const ALIGN_ASSET(2) char sKotakeSkel[] = OOT_ASSET("objects/object_tw/gTwinrovaKotakeSkel");
static const ALIGN_ASSET(2) char sKotakeKoumeFlyAnim[] = OOT_ASSET("objects/object_tw/gTwinrovaKotakeKoumeFlyAnim");
static const ALIGN_ASSET(2) char sKotakeKoumeEyeOpenTex[] = OOT_ASSET("objects/object_tw/gTwinrovaKotakeKoumeEyeOpenTex");
static const ALIGN_ASSET(2) char sGanonSkel[] = OOT_ASSET("objects/object_ganon2/gGanonSkel");
static const ALIGN_ASSET(2) char sGanonGuardIdleAnim[] = OOT_ASSET("objects/object_ganon2/gGanonGuardIdleAnim");
static const ALIGN_ASSET(2) char sGanonEyeOpenTex[] = OOT_ASSET("objects/object_ganon2/gGanonEyeOpenTex");


// One skeleton per boss, initialised in the play that draws it and advanced once per frame (SoH's static SkelAnime).
struct BossSkel {
    SkelAnime skelAnime;
    Vec3s jointTable[96];
    Vec3s morphTable[96];
    PlayState* initPlay = nullptr;
    u32 lastUpdate = 0;
    bool flex = false;
};

void SetupBossSkel(PlayState* play, BossSkel& s, const char* skeleton, const char* animation, bool flex,
                   bool freezeOnLastFrame = false) {
    if (s.initPlay != play) {
        s.initPlay = play;
        s.flex = flex;
        if (flex) {
            SkelAnime_InitFlex(play, &s.skelAnime, (FlexSkeletonHeader*)skeleton, (AnimationHeader*)animation,
                               s.jointTable, s.morphTable, 96);
        } else {
            SkelAnime_Init(play, &s.skelAnime, (SkeletonHeader*)skeleton, (AnimationHeader*)animation, s.jointTable,
                           s.morphTable, 96);
        }
        if (freezeOnLastFrame) {
            f32 lastFrame = Animation_GetLastFrame((void*)animation);
            Animation_Change(&s.skelAnime, (AnimationHeader*)animation, 1.0f, lastFrame, lastFrame, ANIMMODE_ONCE,
                             0.0f);
        }
    }
    if (s.lastUpdate != play->state.frames) {
        s.lastUpdate = play->state.frames;
        SkelAnime_Update(&s.skelAnime);
    }
}

void DrawBossSkel(PlayState* play, BossSkel& s, OverrideLimbDrawOpa override = nullptr,
                  PostLimbDrawOpa post = nullptr) {
    if (s.flex) {
        SkelAnime_DrawFlexOpa(play, s.skelAnime.skeleton, s.skelAnime.jointTable, s.skelAnime.dListCount, override,
                              post, nullptr);
    } else {
        SkelAnime_DrawOpa(play, s.skelAnime.skeleton, s.skelAnime.jointTable, override, post, nullptr);
    }
}

Gfx* BossEmptyDL(GraphicsContext* gfxCtx) {
    Gfx* gfx = (Gfx*)GRAPH_ALLOC(gfxCtx, sizeof(Gfx));
    gSPEndDisplayList(gfx);
    return gfx;
}

s32 GohmaOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 255, 170, 255);
    if (limbIndex == 5) { // BOSSGOMA_LIMB_EYE
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 63);
    } else if (limbIndex == 39) { // BOSSGOMA_LIMB_IRIS
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    return false;
}

s32 BarinadeOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    s16 phase = (s16)(play->gameplayFrames * 0xC31);
    if (limbIndex == 20) {
        gDPPipeSync(POLY_OPA_DISP++);
        gSPSegment(POLY_OPA_DISP++, 0x08,
                   (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 8, 16, 1, 0, (play->gameplayFrames * -2) % 64, 16, 16,
                                                 0, 0, 0, -2));
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 200);
        Matrix_RotateXF(-M_PIf / 2.0f, MTXMODE_APPLY);
    } else if ((limbIndex >= 10) && (limbIndex < 20)) {
        rot->x -= 0x4000;
        *dList = NULL;
    } else if (limbIndex == 6) {
        f32 scale = (Math_SinS(phase) * 0.05f) + 1.0f;
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    } else if (limbIndex == 61) {
        f32 scale = (Math_CosS(phase) * 0.1f) + 1.0f;
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    } else if (limbIndex == 7) {
        rot->x -= 0xCCC;
    }
    CLOSE_DISPS(gfxCtx);
    return false;
}

void BarinadePostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    if (limbIndex == 25) {
        gSPSegment(POLY_XLU_DISP++, 0x09,
                   (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, (play->gameplayFrames * 10) % 128, 16, 32, 1, 0,
                                                 (play->gameplayFrames * 5) % 128, 16, 32, 0, 10, 0, 5));
        MS_LOAD(POLY_XLU_DISP++);
        gSPDisplayList(POLY_XLU_DISP++, MS_DL(sBarinadeDL_008D70));
    } else if ((limbIndex >= 10) && (limbIndex < 20)) {
        MS_LOAD(POLY_XLU_DISP++);
        gSPDisplayList(POLY_XLU_DISP++, MS_DL(sBarinadeDL_008BB8));
    } else if ((*dList != NULL) && (limbIndex >= 29) && (limbIndex < 56)) {
        MS_LOAD(POLY_XLU_DISP++);
        gSPDisplayList(POLY_XLU_DISP++, *dList);
    }
    CLOSE_DISPS(gfxCtx);
}

s32 KotakeOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    if (limbIndex == 21) { // head
        *dList = MS_DL(sKotakeHeadDL);
    }
    return false;
}

void KotakePostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    if (limbIndex == 21) { // head
        GraphicsContext* gfxCtx = play->state.gfxCtx;
        OPEN_DISPS(gfxCtx);
        MS_LOAD(POLY_XLU_DISP++);
        gSPDisplayList(POLY_XLU_DISP++, MS_DL(sKotakeIceHairDL));
        CLOSE_DISPS(gfxCtx);
    }
}

s32 GanonOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    if (limbIndex >= 42) { // the tail, brightened
        OPEN_DISPS(play->state.gfxCtx);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    return false;
}

// SoH Randomizer_DrawBossSoul's boss (SimplerBossSoulModels off, SoH's default: its skull is soh.o2r's).
void DrawBossBody(PlayState* play, u8 boss) {
    static BossSkel sSkels[9];
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frames = play->state.frames;
    if (boss >= 9) {
        return;
    }
    BossSkel& s = sSkels[boss];

    OPEN_DISPS(gfxCtx);
    switch (boss) {
        case 0: // Gohma
            SetupBossSkel(play, s, sGohmaSkel, sGohmaIdleCrouchedAnim, false);
            Gfx_SetupDL25_Opa(gfxCtx);
            Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);
            gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)BossEmptyDL(gfxCtx));
            DrawBossSkel(play, s, GohmaOverrideLimbDraw);
            break;
        case 1: // King Dodongo
            SetupBossSkel(play, s, sKingDodongoSkel, sKingDodongoAnim, false);
            Gfx_SetupDL25_Opa(gfxCtx);
            Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(0.003f, 0.003f, 0.003f, MTXMODE_APPLY);
            DrawBossSkel(play, s);
            break;
        case 2: // Barinade, frozen on its animation's last frame
            SetupBossSkel(play, s, sBarinadeBodySkel, sBarinadeBodyAnim, false, true);
            Gfx_SetupDL25_Opa(gfxCtx);
            Gfx_SetupDL25_Xlu(gfxCtx);
            Matrix_Translate(0.0f, -25.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 8, 16, 1, 0, (play->gameplayFrames * -10) % 16, 16,
                                                     16, 0, 0, 0, -10));
            gSPSegment(POLY_OPA_DISP++, 0x09,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, (play->gameplayFrames * -10) % 32, 16, 0x20, 1, 0,
                                                     (play->gameplayFrames * -5) % 32, 16, 32, 0, -10, 0, -5));
            DrawBossSkel(play, s, BarinadeOverrideLimbDraw, BarinadePostLimbDraw);
            break;
        case 3: // Phantom Ganon
            SetupBossSkel(play, s, sPhantomGanonSkel, sPhantomGanonNeutralAnim, false);
            Gfx_SetupDL25_Opa(gfxCtx);
            Matrix_Translate(0.0f, 10.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(0.007f, 0.007f, 0.007f, MTXMODE_APPLY);
            gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
            gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)BossEmptyDL(gfxCtx));
            DrawBossSkel(play, s);
            break;
        case 4: // Volvagia
            SetupBossSkel(play, s, sVolvagiaHeadSkel, sVolvagiaHeadEmergeAnim, false);
            Gfx_SetupDL25_Opa(gfxCtx);
            Matrix_Scale(0.007f, 0.007f, 0.007f, MTXMODE_APPLY);
            gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sVolvagiaEyeOpenTex);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, frames * 4, 120, 0x20, 0x20, 1, frames * 3,
                                                     frames * -2, 0x20, 0x20, 4, 0, 3, -2));
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
            DrawBossSkel(play, s);
            break;
        case 5: // Morpha: its core
            Gfx_SetupDL25_Xlu(gfxCtx);
            Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, frames * 3, frames * 3, 32, 32, 1, frames * -3,
                                                     frames * -3, 32, 32, 3, 3, -3, -3));
            gSPSegment(POLY_XLU_DISP++, 0x09,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, frames * 3, 0, 32, 32, 1, 0, frames * -5, 32, 32, 3, 0,
                                                     0, -5));
            Matrix_RotateXF(frames * 0.1f, MTXMODE_APPLY);
            Matrix_RotateZF(frames * 0.16f, MTXMODE_APPLY);
            MS_LOAD(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 255, 255);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(sMorphaCoreMembraneDL));
            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetEnvColor(POLY_XLU_DISP++, 0, 220, 255, 128);
            gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 255, 255);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(sMorphaCoreNucleusDL));
            break;
        case 6: // Bongo Bongo: its left hand
            SetupBossSkel(play, s, sBongoLeftHandSkel, sBongoLeftHandIdleAnim, true);
            Gfx_SetupDL25_Opa(gfxCtx);
            Matrix_Translate(0.0f, -25.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
            gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)BossEmptyDL(gfxCtx));
            gDPSetPrimColor(POLY_OPA_DISP++, 0x80, 0x80, 255, 255, 255, 255);
            DrawBossSkel(play, s);
            break;
        case 7: // Twinrova: Kotake
            SetupBossSkel(play, s, sKotakeSkel, sKotakeKoumeFlyAnim, true);
            Gfx_SetupDL25_Opa(gfxCtx);
            Gfx_SetupDL25_Xlu(gfxCtx);
            Matrix_Translate(0.0f, -10.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
            gSPSegment(POLY_OPA_DISP++, 10, (uintptr_t)sKotakeKoumeEyeOpenTex);
            gSPSegment(POLY_XLU_DISP++, 10, (uintptr_t)sKotakeKoumeEyeOpenTex);
            gSPSegment(POLY_XLU_DISP++, 8,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x20, 0x20, 1, frames & 0x7F,
                                                     (frames * -7) & 0xFF, 0x20, 0x40, 0, 0, 1, -7));
            gSPSegment(POLY_XLU_DISP++, 9, (uintptr_t)Gfx_TexScrollEx(gfxCtx, 0, frames & 0xFF, 0x20, 0x40, 0, 1));
            DrawBossSkel(play, s, KotakeOverrideLimbDraw, KotakePostLimbDraw);
            break;
        case 8: // Ganon
            SetupBossSkel(play, s, sGanonSkel, sGanonGuardIdleAnim, true);
            Gfx_SetupDL25_Opa(gfxCtx);
            Matrix_Translate(0.0f, -33.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);
            gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sGanonEyeOpenTex);
            DrawBossSkel(play, s, GanonOverrideLimbDraw);
            break;
    }
    CLOSE_DISPS(gfxCtx);
}

// SoH Randomizer_DrawBossSoul's flame: OoT's blue fire, greyscale-tinted in the boss's colour.
bool DrawBossSoul(PlayState* play, u8 boss) {
    static const u8 kFlameColors[9][3] = {
        { 0, 255, 0 },    { 255, 0, 100 },  { 50, 255, 255 }, { 4, 195, 46 },    { 237, 95, 95 },
        { 85, 180, 223 }, { 126, 16, 177 }, { 222, 158, 47 }, { 150, 150, 150 },
    };
    const char* flame = kMsOoTGiTable[MSOOTGID_BLUE_FIRE].dl[1];
    if (flame == nullptr) {
        return false;
    }
    const u8* c = kFlameColors[(boss < 9) ? boss : 0];
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frames = play->state.frames;

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL25_Xlu(gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 16, 32, 1, frames, -(frames * 8), 16, 32, 0, 0, 1, -8));
    Matrix_Push();
    Matrix_Translate(0.0f, -70.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(5.0f, 5.0f, 5.0f, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    MS_LOAD(POLY_XLU_DISP++);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, c[0], c[1], c[2], 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(flame));
    gSPGrayscale(POLY_XLU_DISP++, false);
    Matrix_Pop();
    CLOSE_DISPS(gfxCtx);
    DrawBossBody(play, boss);
    return true;
}

// SoH Randomizer_DrawBeanSprout.
bool DrawBeanSprout(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL25_Opa(gfxCtx);
    Matrix_Scale(0.3f, 0.3f, 0.3f, MTXMODE_APPLY);
    MS_LOAD(POLY_OPA_DISP++);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(sMagicBeanSeedlingDL));
    CLOSE_DISPS(gfxCtx);
    return true;
}

// SoH Randomizer_DrawScarecrowsSong: the note in dark green.
bool DrawScarecrowNote(PlayState* play) {
    const char* note = kMsOoTGiTable[MSOOTGID_SONG_GENERIC].dl[0];
    if (note == nullptr) {
        return false;
    }
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL25_Xlu(gfxCtx);
    MS_LOAD(POLY_XLU_DISP++);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 40, 160, 40, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(note));
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(gfxCtx);
    return true;
}

} // namespace

bool MergeShip_OoTGi_HasKey(MsItemKey key) {
    return FindKey(key) != nullptr;
}

bool MergeShip_OoTGi_DrawKey(PlayState* play, MsItemKey key) {
    const MsOoTGiKeyRow* row = FindKey(key);
    if ((play == nullptr) || (row == nullptr)) {
        return false;
    }
    switch (row->kind) {
        case MSOGK_MMGid:
            GetItem_Draw(play, row->gid);
            return true;
        case MSOGK_Triforce:
            Rando::DrawItem(RI_TRIFORCE_PIECE, RC_UNKNOWN, nullptr);
            return true;
        default:
            break;
    }
    if (!MergeShip_IsOcarinaMounted()) {
        return false;
    }
    switch (row->kind) {
        case MSOGK_OoTGid:
            return (row->tint == MSOGT_None) ? DrawOoTGid(play, row->gid) : DrawTinted(play, *row);
        case MSOGK_BossSoul:
            return DrawBossSoul(play, row->arg);
        case MSOGK_BeanSprout:
            return DrawBeanSprout(play);
        case MSOGK_Scarecrow:
            return DrawScarecrowNote(play);
        default:
            return false;
    }
}
