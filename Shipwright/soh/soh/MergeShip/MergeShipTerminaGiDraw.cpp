// MergeShip2 -- the get-item models of the unified randomizer's Termina items in Hyrule (MergeShipTerminaGiDraw.h).
//
// Ports, onto SoH's graphics API and mm.o2r's "mm/" paths:
//   * MM z_draw.c (2ship2harkinian/mm/src/code/z_draw.c): GetItem_Draw and its draw functions. The table itself
//     (display lists per GID) is generated from 2Ship's source into MergeShipTerminaGiData.inc.
//   * 2Ship's Rando::DrawItem special draws (2s2h/Rando/DrawItem.cpp, DrawFuncs.cpp): songs, keys, Gold Skulltula
//     tokens, owl statues, clocks, frogs, the enemy and boss souls, milk, double defense.
// API mapping: MATRIX_FINALIZE_AND_LOAD -> gSPMatrix(MATRIX_NEWMTX, NOPUSH|LOAD|MODELVIEW); Gfx_SetupDL25_Opa/Xlu ->
// Gfx_SetupDL_25Opa/Xlu; AnimatedMat_* -> MergeShip_AnimatedMat_* (MergeShipAnimatedMat.h); Scene_SetRenderModeXlu ->
// MergeShip_Scene_SetRenderModeXlu; D_801AEFA0 (MM's empty DL) -> gEmptyDL; Matrix_Rotate*S -> Matrix_Rotate* in
// radians; SkelAnime_DrawTransformFlexOpa -> SkelAnime_DrawFlexOpa (the pirate's transform callback does nothing).
// Not ported: MM's Mtx resources (GID_FAIRY's and GID_SEAHORSE_CAUGHT's billboard rotation: SoH has no Mtx resource
// loader; the billboard is Matrix_ReplaceRotation alone), 2ship.o2r's own models (2Ship's per-dungeon emblem keys:
// MM's own small key / boss key are drawn), the 2Ship draws' actor-dependent parts (sparkles, the clock tower actor's
// live rotation, the Gibdo-mask check of the ReDead's dance).

#include "soh/MergeShip/MergeShipTerminaGiDraw.h"

#include <cmath>
#include <unordered_map>

#include "soh/Enhancements/randomizer/randomizerTypes.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "align_asset_macro.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "soh/MergeShip/MergeShipAnimatedMat.h"
#include "soh/MergeShip/MergeShipMMScene.h"
#include "soh/MergeShip/MergeShipStrayFairy.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "mm/objects/object_gi_melody/object_gi_melody.h"
#include "mm/objects/object_gi_hearts/object_gi_hearts.h"
#include "mm/objects/object_gi_liquid/object_gi_liquid.h"
#include "mm/objects/object_gi_bottle_04/object_gi_bottle_04.h"
#include "mm/objects/object_gi_reserve00/object_gi_reserve00.h"
#include "mm/objects/object_sek/object_sek.h"
#include "mm/objects/object_st/object_st.h"
#include "mm/objects/object_obj_tokeidai/object_obj_tokeidai.h"
#include "mm/objects/object_fr/object_fr.h"
// Boss souls
#include "mm/objects/object_boss_hakugin/object_boss_hakugin.h"
#include "mm/objects/object_boss03/object_boss03.h"
#include "mm/objects/object_boss01/object_boss01.h"
#include "mm/objects/object_boss02/object_boss02.h"
#include "mm/objects/object_boss07/object_boss07.h"
// Enemy souls
#include "mm/objects/object_uch/object_uch.h"
#include "mm/objects/object_am/object_am.h"
#include "mm/objects/object_bat/object_bat.h"
#include "mm/objects/object_vm/object_vm.h"
#include "mm/objects/object_mkk/object_mkk.h"
#include "mm/objects/object_bsb/object_bsb.h"
#include "mm/objects/object_slime/object_slime.h"
#include "mm/objects/object_bb/object_bb.h"
#include "mm/objects/object_famos/object_famos.h"
#include "mm/objects/object_utubo/object_utubo.h"
#include "mm/objects/object_dekubaba/object_dekubaba.h"
#include "mm/objects/object_wdhand/object_wdhand.h"
#include "mm/objects/object_dinofos/object_dinofos.h"
#include "mm/objects/object_dodongo/object_dodongo.h"
#include "mm/objects/object_grasshopper/object_grasshopper.h"
#include "mm/objects/object_snowman/object_snowman.h"
#include "mm/objects/object_eg/object_eg.h"
#include "mm/objects/object_fz/object_fz.h"
#include "mm/objects/object_jso/object_jso.h"
#include "mm/objects/object_bigslime/object_bigslime.h"
#include "mm/objects/object_bee/object_bee.h"
#include "mm/objects/object_death/object_death.h"
#include "mm/objects/object_crow/object_crow.h"
#include "mm/objects/object_pp/object_pp.h"
#include "mm/objects/object_knight/object_knight.h"
#include "mm/objects/object_ik/object_ik.h"
#include "mm/objects/object_firefly/object_firefly.h"
#include "mm/objects/object_rb/object_rb.h"
#include "mm/objects/object_rr/object_rr.h"
#include "mm/objects/object_dekunuts/object_dekunuts.h"
#include "mm/objects/object_gmo/object_gmo.h"
#include "mm/objects/object_okuta/object_okuta.h"
#include "mm/objects/object_ph/object_ph.h"
#include "mm/objects/object_kz/object_kz.h"
#include "mm/objects/object_po/object_po.h"
#include "mm/objects/object_rd/object_rd.h"
#include "mm/objects/object_sb/object_sb.h"
#include "mm/objects/object_pr/object_pr.h"
#include "mm/objects/object_tl/object_tl.h"
#include "mm/objects/object_skb/object_skb.h"
#include "mm/objects/object_thiefbird/object_thiefbird.h"
#include "mm/objects/object_tite/object_tite.h"
#include "mm/objects/object_wallmaster/object_wallmaster.h"
#include "mm/objects/object_boss04/object_boss04.h"
#include "mm/objects/object_wiz/object_wiz.h"
#include "mm/objects/object_wf/object_wf.h"
extern PlayState* gPlayState;
}

namespace {

// OPEN_DISPS / CLOSE_DISPS redeclare these at block scope; declared C here so that, inside this namespace, they
// still name the C functions in frame_interpolation.cpp.
extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

#define MS_MTX_LOAD(dl, gfxCtx) gSPMatrix(dl, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW)
#define MS_DL(res) ((Gfx*)(res))

// ---- the generated tables (tools/gen_cross_gi.py) -----------------------------------------------------------------

// MM z_draw.c's draw functions (GetItem_Draw<Name>).
enum MsMMGiFunc : u8 {
    MSMMGI_Bombchu,
    MSMMGI_Poes,
    MSMMGI_FairyBottle,
    MSMMGI_SkullToken,
    MSMMGI_Compass,
    MSMMGI_Potion,
    MSMMGI_GoronSword,
    MSMMGI_DekuNuts,
    MSMMGI_RecoveryHeart,
    MSMMGI_Fish,
    MSMMGI_Opa0,
    MSMMGI_Opa0Xlu1,
    MSMMGI_Opa01,
    MSMMGI_Xlu01,
    MSMMGI_Seahorse,
    MSMMGI_FairyContainer,
    MSMMGI_MoonsTear,
    MSMMGI_MagicArrow,
    MSMMGI_Upgrades,
    MSMMGI_Rupee,
    MSMMGI_SmallRupee,
    MSMMGI_Wallet,
    MSMMGI_Remains,
};

struct MsMMGiRow {
    MsMMGiFunc func;
    const char* res[8];
};

// 2Ship's Rando::DrawItem draws (DrawItem.cpp / DrawFuncs.cpp names); MMGid is GetItem_Draw of the drawId.
enum MsTerminaGiKind : u8 {
    MSTGI_MMGid,
    MSTGI_DrawSmallKey,
    MSTGI_DrawBossKey,
    MSTGI_DrawSong,
    MSTGI_DrawStrayFairy,
    MSTGI_DrawSkulltulaToken,
    MSTGI_DrawOwlStatue,
    MSTGI_DrawClock,
    MSTGI_DrawMinifrog,
    MSTGI_DrawMilkRefill,
    MSTGI_DrawDoubleDefense,
    MSTGI_DrawAlien,
    MSTGI_DrawArmos,
    MSTGI_DrawBat,
    MSTGI_DrawBeamos,
    MSTGI_DrawBoe,
    MSTGI_DrawBubble,
    MSTGI_DrawCaptainKeeta,
    MSTGI_DrawChuchu,
    MSTGI_DrawDeathArmos,
    MSTGI_DrawDeepPython,
    MSTGI_DrawDekuBaba,
    MSTGI_DrawDexihand,
    MSTGI_DrawDinolfos,
    MSTGI_DrawDodongo,
    MSTGI_DrawDragonfly,
    MSTGI_DrawEeno,
    MSTGI_DrawEyegore,
    MSTGI_DrawFreezard,
    MSTGI_DrawGaro,
    MSTGI_DrawGekko,
    MSTGI_DrawGiantBee,
    MSTGI_DrawGomess,
    MSTGI_DrawGuay,
    MSTGI_DrawHiploop,
    MSTGI_DrawIgosDuIkana,
    MSTGI_DrawIronKnuckle,
    MSTGI_DrawKeese,
    MSTGI_DrawLeever,
    MSTGI_DrawLikeLike,
    MSTGI_DrawMadScrub,
    MSTGI_DrawNejiron,
    MSTGI_DrawOctorok,
    MSTGI_DrawPeahat,
    MSTGI_DrawPirate,
    MSTGI_DrawPoe,
    MSTGI_DrawRedead,
    MSTGI_DrawShellBlade,
    MSTGI_DrawSkullfish,
    MSTGI_DrawSkulltula,
    MSTGI_DrawSnapper,
    MSTGI_DrawStalchild,
    MSTGI_DrawTakkuri,
    MSTGI_DrawTektite,
    MSTGI_DrawWallmaster,
    MSTGI_DrawWart,
    MSTGI_DrawWizrobe,
    MSTGI_DrawWolfos,
    MSTGI_DrawGoht,
    MSTGI_DrawGyorg,
    MSTGI_DrawMajora,
    MSTGI_DrawOdolwa,
    MSTGI_DrawTwinmold,
};

struct MsTerminaGiRow {
    RandomizerGet rg;
    MsTerminaGiKind kind;
    s16 gid; // MM GetItemDrawId (MsMMGid), MSMMGID_NONE for a special draw without one
};

#include "soh/MergeShip/MergeShipTerminaGiData.inc"

const MsTerminaGiRow* FindRow(int rg) {
    static std::unordered_map<int, const MsTerminaGiRow*> sRows;
    if (sRows.empty()) {
        for (const MsTerminaGiRow& row : kMsTerminaGiRows) {
            sRows[row.rg] = &row;
        }
    }
    auto it = sRows.find(rg);
    return (it != sRows.end()) ? it->second : nullptr;
}

// ---- MM z_draw.c ----------------------------------------------------------------------------------------------------

void MMGi_Draw(PlayState* play, s16 gid) {
    if ((gid < 0) || (gid >= MSMMGID_COUNT)) {
        return;
    }
    const MsMMGiRow& e = kMsMMGiTable[gid];
    const char* const* r = e.res;
    if (r[0] == nullptr) {
        return; // MM's unused ids (GID_37, GID_46, GID_4C) have no model
    }
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 frames = (s32)play->state.frames;

    OPEN_DISPS(gfxCtx);

    switch (e.func) {
        case MSMMGI_Bombchu:
            POLY_OPA_DISP = Gfx_SetupDL(POLY_OPA_DISP, SETUPDL_23);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            break;
        case MSMMGI_Poes:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            Gfx_SetupDL_25Xlu(gfxCtx);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[1]));
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, G_TX_RENDERTILE, 0, 0, 16, 32, 1, frames, -((s32)frames * 6), 16, 32, 0,
                                          0, 1, -6));
            Matrix_Push();
            Matrix_ReplaceRotation(&play->billboardMtxF);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[3]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[2]));
            Matrix_Pop();
            break;
        case MSMMGI_FairyBottle:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            Gfx_SetupDL_25Xlu(gfxCtx);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[1]));
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, G_TX_RENDERTILE, 0, 0, 32, 32, 1, frames, -(frames * 6), 32, 320, 0,
                                          0, 1, -6));
            Matrix_Push();
            Matrix_ReplaceRotation(&play->billboardMtxF);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[2]));
            Matrix_Pop();
            break;
        case MSMMGI_SkullToken:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            Gfx_SetupDL_25Xlu(gfxCtx);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, G_TX_RENDERTILE, 0, -(frames * 5), 32, 32, 1, 0, 0, 32, 64, 0, -5,
                                          0, 0));
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[1]));
            break;
        case MSMMGI_Compass:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_5);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[1]));
            break;
        case MSMMGI_Potion:
            Gfx_SetupDL_25Opa(gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, G_TX_RENDERTILE, -frames, frames, 32, 32, 1, -frames, frames, 32, 32,
                                          -1, 1, -1, 1));
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[1]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[2]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[3]));
            Gfx_SetupDL_25Xlu(gfxCtx);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[4]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[5]));
            break;
        case MSMMGI_GoronSword:
            Gfx_SetupDL_25Opa(gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, G_TX_RENDERTILE, frames, 0, 32, 32, 1, 0, 0, 32, 32, 1, 0, 0, 0));
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            break;
        case MSMMGI_DekuNuts:
            Gfx_SetupDL_25Opa(gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, G_TX_RENDERTILE, frames * 6, frames * 6, 32, 32, 1, frames * 6,
                                          frames * 6, 32, 32, 6, 6, 6, 6));
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            break;
        case MSMMGI_RecoveryHeart:
            Gfx_SetupDL_25Xlu(gfxCtx);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, G_TX_RENDERTILE, 0, -(frames * 3), 32, 32, 1, 0, -(frames * 2), 32,
                                          32, 0, -3, 0, -2));
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[0]));
            break;
        case MSMMGI_Fish:
            Gfx_SetupDL_25Xlu(gfxCtx);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, G_TX_RENDERTILE, 0, frames, 32, 32, 1, 0, frames, 32, 32, 0, 1, 0,
                                          1));
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[0]));
            break;
        case MSMMGI_Opa0:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            break;
        case MSMMGI_Opa0Xlu1:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            Gfx_SetupDL_25Xlu(gfxCtx);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[1]));
            break;
        case MSMMGI_Opa01:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[1]));
            break;
        case MSMMGI_Xlu01:
            Gfx_SetupDL_25Xlu(gfxCtx);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[0]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[1]));
            break;
        case MSMMGI_Seahorse:
        case MSMMGI_FairyContainer:
            // MM multiplies in a billboard-rotation Mtx resource first (r[3]); SoH loads no Mtx resources, and the
            // rotation it would set is replaced by the billboard right after.
            Gfx_SetupDL_25Opa(gfxCtx);
            Gfx_SetupDL_25Xlu(gfxCtx);
            if (e.func == MSMMGI_FairyContainer) {
                MergeShip_AnimatedMat_Draw(play, (void*)mm_gGiFairyBottleTexAnim);
            }
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[1]));
            Matrix_ReplaceRotation(&play->billboardMtxF);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[2]));
            break;
        case MSMMGI_MoonsTear:
            Gfx_SetupDL_25Opa(gfxCtx);
            Gfx_SetupDL_25Xlu(gfxCtx);
            MergeShip_AnimatedMat_Draw(play, (void*)mm_gGiMoonsTearTexAnim);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            Matrix_ReplaceRotation(&play->billboardMtxF);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[1]));
            break;
        case MSMMGI_MagicArrow:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            Gfx_SetupDL_25Xlu(gfxCtx);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[1]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[2]));
            break;
        case MSMMGI_Upgrades:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[1]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[2]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[3]));
            break;
        case MSMMGI_SmallRupee:
            Matrix_Scale(0.7f, 0.7f, 0.7f, MTXMODE_APPLY);
            [[fallthrough]];
        case MSMMGI_Rupee:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[1]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            Gfx_SetupDL_25Xlu(gfxCtx);
            MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[3]));
            gSPDisplayList(POLY_XLU_DISP++, MS_DL(r[2]));
            break;
        case MSMMGI_Wallet:
            Gfx_SetupDL_25Opa(gfxCtx);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[1]));
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            for (s32 i = 2; i < 8; i++) {
                gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[i]));
            }
            break;
        case MSMMGI_Remains:
            // The remains' display lists are resources: no object segment is needed.
            Gfx_SetupDL_25Opa(gfxCtx);
            Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
            MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, MS_DL(r[0]));
            POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
            break;
    }

    CLOSE_DISPS(gfxCtx);
}

// ---- 2Ship's special draws (DrawItem.cpp) ---------------------------------------------------------------------------

// DrawSong: MM's song note in the song's colour.
void DrawSong(PlayState* play, int rg) {
    u8 r = 255, g = 255, b = 255;
    switch (rg) {
        case RG_MM_SONG_OF_DOUBLE_TIME:
        case RG_MM_INVERTED_SONG_OF_TIME:
            r = 98, g = 177, b = 211;
            break;
        case RG_MM_GORON_LULLABY_INTRO:
            r = 255, g = 100, b = 100;
            break;
        case RG_MM_GORONS_LULLABY:
            r = 255, g = 20, b = 20;
            break;
        default:
            break;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    MS_MTX_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, r, g, b, 255);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gGiSongNoteDL));
    CLOSE_DISPS(play->state.gfxCtx);
}

// DrawSkulltulaToken: MM's token (GID_SKULL_TOKEN_2, object_st), tilted 16 units so its eyes always draw (2Ship's
// note: they vanish when perfectly parallel to the camera, most of all on shop shelves). 2Ship's per-house flame
// colours need a copy of the flame display list with its colour commands removed; SoH draws MM's own flame.
void DrawSkulltulaToken(PlayState* play, s16 gid) {
    Matrix_RotateX(BINANG_TO_RAD(16), MTXMODE_APPLY);
    MMGi_Draw(play, gid);
}

// DrawOwlStatue.
void DrawOwlStatue(PlayState* play) {
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3000, 0, MTXMODE_APPLY);
    Gfx_DrawDListOpa(play, MS_DL(mm_gOwlStatueOpenedDL));
}

// DrawClock: the Clock Tower's face showing day or night (no clock tower actor to follow in Hyrule).
void DrawClock(PlayState* play, int rg) {
    s16 clockFaceRotation = (s16)0xC000;
    s16 sunMoonPanelRotation = 0;
    switch (rg) {
        case RG_MM_TIME_NIGHT_1:
        case RG_MM_TIME_NIGHT_2:
        case RG_MM_TIME_NIGHT_3:
            clockFaceRotation = 0;
            sunMoonPanelRotation = (s16)0x8000;
            break;
        default:
            break;
    }
    GraphicsContext* gfxCtx = play->state.gfxCtx;

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Opa(gfxCtx);
    Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);

    MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gClockTowerMinuteRingDL));
    MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gClockTowerClockCenterAndHandDL));

    Matrix_RotateZ(BINANG_TO_RAD(-clockFaceRotation * 2), MTXMODE_APPLY);
    MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gClockTowerClockFaceDL));

    Matrix_Translate(0.0f, -1112.0f, -19.6f, MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(sunMoonPanelRotation), MTXMODE_APPLY);
    MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gClockTowerSunAndMoonPanelDL));
    CLOSE_DISPS(gfxCtx);
}

// DrawMilkRefill: MM's potion container with milk's pattern.
void DrawMilkRefill(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 frames = (s32)play->state.frames;

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Opa(gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, G_TX_RENDERTILE, -frames, frames, 32, 32, 1, -frames, frames, 32, 32, -1, 1,
                                  -1, 1));
    MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 200, 200, 200, 255);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gGiPotionContainerPotDL));
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 200, 200, 200, 255);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gGiPotionContainerLiquidDL));
    Gfx_SetupDL_25Xlu(gfxCtx);
    MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 13, 33, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 100, 100, 255, 255);
    gDPLoadTextureBlock(POLY_XLU_DISP++, mm_gGiPotionContainerBluePatternTex, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 32, 0,
                        G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_CLAMP, 4, 5, G_TX_NOLOD, G_TX_NOLOD);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gGiPotionContainerPatternDL));
    CLOSE_DISPS(gfxCtx);
}

// DrawDoubleDefense: MM's heart container, white-bordered and red-glassed.
void DrawDoubleDefense(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    MS_MTX_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gGiHeartBorderDL));
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 255, 0, 0, 100);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gGiHeartContainerDL));
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- skeletons (DrawFuncs.cpp SETUP_DRAW): one per model, advanced once per frame ---------------------------------

constexpr s32 kMaxLimbs = 128;

struct GiSkel {
    SkelAnime skelAnime;
    Vec3s jointTable[kMaxLimbs];
    Vec3s morphTable[kMaxLimbs];
    PlayState* initPlay = nullptr; // SoH's skeletons belong to the play they were initialised in
    u32 lastUpdate = 0;
    bool fresh = false; // initialised this call
};

void SetupSkel(PlayState* play, GiSkel& s, const char* skeleton, const char* animation, s32 limbCount, bool flex) {
    s.fresh = false;
    if (s.initPlay != play) {
        s.initPlay = play;
        s.fresh = true;
        if (flex) {
            SkelAnime_InitFlex(play, &s.skelAnime, (FlexSkeletonHeader*)skeleton, (AnimationHeader*)animation,
                               s.jointTable, s.morphTable, limbCount);
        } else {
            SkelAnime_Init(play, &s.skelAnime, (SkeletonHeader*)skeleton, (AnimationHeader*)animation, s.jointTable,
                           s.morphTable, limbCount);
        }
    }
    if (s.lastUpdate != play->state.frames) {
        s.lastUpdate = play->state.frames;
        SkelAnime_Update(&s.skelAnime);
    }
}

void DrawFlexOpa(PlayState* play, GiSkel& s, OverrideLimbDrawOpa override = nullptr, PostLimbDrawOpa post = nullptr) {
    SkelAnime_DrawFlexOpa(play, s.skelAnime.skeleton, s.skelAnime.jointTable, s.skelAnime.dListCount, override, post,
                          nullptr);
}

void DrawOpa(PlayState* play, GiSkel& s, OverrideLimbDrawOpa override = nullptr, PostLimbDrawOpa post = nullptr) {
    SkelAnime_DrawOpa(play, s.skelAnime.skeleton, s.skelAnime.jointTable, override, post, nullptr);
}

// DrawEnLight: the soul's flame (gameplay_keep's En_Light flame, billboarded).
void DrawEnLight(PlayState* play, Color_RGB8 color, Vec3f size) {
    static s8 sScroll = (s8)(Rand_ZeroOne() * 255.0f);
    static u32 sLastUpdate = 0;
    GraphicsContext* gfxCtx = play->state.gfxCtx;

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x10, 0x20, 1, (sScroll * 2) & 0x3F, (sScroll * -6) & 0x7F, 0x10,
                                  0x20, 0, 0, 2, -6));
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, color.r, color.g, color.b, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, color.r, color.g, color.b, 0);
    Matrix_Scale(size.x, size.y, size.z, MTXMODE_APPLY);
    MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gameplay_keep_DL_01ACF0));
    CLOSE_DISPS(gfxCtx);

    if (sLastUpdate != play->state.frames) {
        sLastUpdate = play->state.frames;
        sScroll++;
    }
}

void Light(PlayState* play, u8 r, u8 g, u8 b, f32 x, f32 y, f32 z) {
    DrawEnLight(play, Color_RGB8{ r, g, b }, Vec3f{ x, y, z });
}

Gfx* EmptyDL(GraphicsContext* gfxCtx) {
    Gfx* gfx = (Gfx*)Graph_Alloc(gfxCtx, sizeof(Gfx));
    gSPEndDisplayList(gfx);
    return gfx;
}

// ---- frogs (DrawMinifrog) -------------------------------------------------------------------------------------------

// MM EnMinifrog_OverrideLimbDraw / 2Ship EnMinifrogPostLimbDraw: the eyes are billboarded.
s32 MinifrogOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    if (limbIndex == FROG_LIMB_LOWER_BODY) {
        pos->z -= 500.0f;
    }
    if ((limbIndex == FROG_LIMB_RIGHT_EYE) || (limbIndex == FROG_LIMB_LEFT_EYE)) {
        *dList = NULL;
    }
    return false;
}

void MinifrogPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* arg) {
    if (((limbIndex == FROG_LIMB_RIGHT_EYE) || (limbIndex == FROG_LIMB_LEFT_EYE)) && (*dList != NULL)) {
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        MS_MTX_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, *dList);
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void DrawMinifrog(PlayState* play, int rg) {
    static GiSkel s;
    Color_RGBA8 env = { 200, 170, 0, 255 }; // FROG_YELLOW
    switch (rg) {
        case RG_MM_FROG_BLUE:
            env = { 120, 130, 230, 255 };
            break;
        case RG_MM_FROG_CYAN:
            env = { 0, 170, 200, 255 };
            break;
        case RG_MM_FROG_PINK:
            env = { 210, 120, 100, 255 };
            break;
        case RG_MM_FROG_WHITE:
            env = { 190, 190, 190, 255 };
            break;
        default:
            break;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gFrogSkel, mm_gFrogIdleAnim, FROG_LIMB_MAX, true);
    gDPSetEnvColor(POLY_OPA_DISP++, env.r, env.g, env.b, env.a);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gFrogIrisOpenTex);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)mm_gFrogIrisOpenTex);
    DrawFlexOpa(play, s, MinifrogOverrideLimbDraw, MinifrogPostLimbDraw);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- enemy souls (DrawFuncs.cpp) ------------------------------------------------------------------------------------

void DrawAlien(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.007f, 0.007f, 0.007f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gAlienSkel, mm_gAlienFloatAnim, ALIEN_LIMB_MAX, true);
    MergeShip_AnimatedMat_Draw(play, (void*)mm_gAlienEmptyTexAnim);
    MergeShip_Scene_SetRenderModeXlu(play, 0, 1);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gAlienEyeTex);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 10, 138, 46, 30.0f, 30.0f, 30.0f);
}

void DrawArmos(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3100, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_object_am_Skel_005948, mm_gArmosHopAnim, OBJECT_AM_LIMB_MAX, false);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawBat(PlayState* play) {
    static u32 sLastUpdate = 0;
    static u32 sWingAnim = 0;
    static const char* const sWingsDLs[] = {
        mm_gBadBatWingsFrame0DL, mm_gBadBatWingsFrame1DL, mm_gBadBatWingsFrame2DL,
        mm_gBadBatWingsFrame3DL, mm_gBadBatWingsFrame4DL, mm_gBadBatWingsFrame5DL,
        mm_gBadBatWingsFrame6DL, mm_gBadBatWingsFrame7DL, mm_gBadBatWingsFrame8DL,
    };
    if (sLastUpdate != play->state.frames) {
        sLastUpdate = play->state.frames;
        sWingAnim = (sWingAnim + 1) % 9;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    POLY_OPA_DISP = Gfx_SetupDL(POLY_OPA_DISP, SETUPDL_25);
    MS_MTX_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gBadBatSetupDL));
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gBadBatBodyDL));
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(sWingsDLs[sWingAnim]));
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 6.0f, 6.0f, 6.0f);
}

void DrawBeamos(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3200, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gBeamosSkel, mm_gBeamosAnim, BEAMOS_LIMB_MAX, false);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawBoe(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -1200, 0, MTXMODE_APPLY);
    POLY_OPA_DISP = Gfx_SetupDL(POLY_OPA_DISP, SETUPDL_25);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0xFF, 0, 0, 0, 255);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gEmptyDL);
    MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gBlackBoeEndDL));

    POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_25);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gBlackBoeBodyMaterialDL));
    Matrix_ReplaceRotation(&play->billboardMtxF);
    MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gBlackBoeBodyModelDL));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0xFF, 245, 97, 0, 255);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gBlackBoeEyesDL));
    Matrix_Scale(0.009f, 0.009f, 0.009f, MTXMODE_APPLY);
    MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0xFF, 245, 214, 0, 255);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gBlackBoeEyesDL));
    CLOSE_DISPS(gfxCtx);
    Light(play, 155, 155, 155, 1000.0f, 1000.0f, 1000.0f);
}

void DrawBubble(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gBubbleSkel, mm_gBubbleFlyingAnim, BUBBLE_LIMB_MAX, false);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawCaptainKeeta(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3500.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_object_bsb_Skel_00C3E0, mm_object_bsb_Anim_004894, OBJECT_BSB_LIMB_MAX, false);
    MergeShip_Scene_SetRenderModeXlu(play, 0, 1);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 255, 192, 0, 5.0f, 10.0f, 5.0f);
}

void DrawChuchu(PlayState* play) {
    static s16 sTimer = 25;
    static u32 sLastUpdate = 0;
    f32 timerFactor = sqrtf((f32)sTimer) * 0.2f;
    f32 yScale = ((Math_CosS(RADF_TO_BINANG(sTimer * (2.0f * (f32)M_PI / 5.0f))) * (0.07f * timerFactor)) + 1.0f) *
                 0.01f;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Scale(0.01f, yScale, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -2700.0f, 0, MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    MergeShip_AnimatedMat_Draw(play, (void*)mm_gChuchuSlimeFlowTexAnim);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 100, 255, 255, 200, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 180, 0, 255);
    MS_MTX_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    MergeShip_Scene_SetRenderModeXlu(play, 1, 2);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gChuchuBodyDL));
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)mm_gChuchuEyeOpenTex);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gChuchuEyesDL));
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);

    if (sLastUpdate != play->state.frames) {
        sLastUpdate = play->state.frames;
        sTimer = (sTimer <= 0) ? 25 : (s16)(sTimer - 1);
    }
}

void DrawDeathArmos(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    MergeShip_AnimatedMat_Draw(play, (void*)mm_gFamosNormalGlowingEmblemTexAnim);
    Matrix_Scale(0.008f, 0.008f, 0.008f, MTXMODE_APPLY);
    Matrix_Translate(0, -4100, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gFamosSkel, mm_gFamosIdleAnim, FAMOS_LIMB_MAX, false);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawDeepPython(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gDeepPythonSkel, mm_gDeepPythonUnusedSideSwayAnim, DEEP_PYTHON_LIMB_MAX, true);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawDekuBaba(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gDekuBabaSkel, mm_gDekuBabaFastChompAnim, DEKUBABA_LIMB_MAX, false);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 6.0f, 6.0f, 6.0f);
}

void DrawDexihand(PlayState* play) {
    static GiSkel s;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Opa(gfxCtx);
    Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
    Matrix_Translate(0, -1000, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gDexihandSkel, mm_gDexihandIdleAnim, DEXIHAND_LIMB_MAX, true);
    POLY_OPA_DISP = Gfx_SetupDL(POLY_OPA_DISP, SETUPDL_25);
    MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gDexihandBaseDL));
    // A single arm segment.
    Matrix_Push();
    MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gDexihandArmSegmentDL));
    Matrix_Pop();
    Matrix_Translate(0, 2500, 0, MTXMODE_APPLY);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(gfxCtx);
    Matrix_Translate(0, -1000, 0, MTXMODE_APPLY);
    Light(play, 155, 155, 70, 6.0f, 6.0f, 6.0f);
}

void DrawDinolfos(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.014f, 0.014f, 0.014f, MTXMODE_APPLY);
    Matrix_Translate(0, -2200.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gDinolfosSkel, mm_gDinolfosIdleAnim, DINOLFOS_LIMB_MAX, true);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gDinolfosEyeOpenTex);
    gDPSetEnvColor(POLY_OPA_DISP++, 20, 40, 40, 255);
    MergeShip_Scene_SetRenderModeXlu(play, 0, 1);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawDodongo(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
    Matrix_Translate(0, -1500.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_object_dodongo_Skel_008318, mm_object_dodongo_Anim_004C20, OBJECT_DODONGO_LIMB_MAX, false);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawDragonfly(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -700.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gDragonflySkel, mm_gDragonflyFlyAnim, DRAGONFLY_LIMB_MAX, false);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawEeno(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3000.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gEenoSkel, mm_gEenoIdleAnim, EENO_LIMB_MAX, true);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 35, 10.0f, 10.0f, 10.0f);
}

void DrawEyegore(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    Matrix_Translate(0, -4000.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gEyegoreSkel, mm_gEyegoreUnusedWalkAnim, EYEGORE_LIMB_MAX, true);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0xFF, 175, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 115, 155, 255);
    MergeShip_AnimatedMat_Draw(play, (void*)mm_gEyegoreEyeLaserTexAnim);
    DrawFlexOpa(play, s);
    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 192, 192, 64, 20.0f, 20.0f, 20.0f);
}

void DrawFreezard(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 frames = (s32)play->state.frames;
    OPEN_DISPS(gfxCtx);
    Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    Matrix_Translate(0, -4100.0f, 0, MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, frames % 128, 0x20, 0x20, 1, 0, (frames * 2) % 128, 0x20, 0x20, 0, 1,
                                  0, 2));
    MS_MTX_LOAD(POLY_XLU_DISP++, gfxCtx);
    gDPSetCombineLERP(POLY_XLU_DISP++, TEXEL1, PRIMITIVE, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIMITIVE, TEXEL0,
                      PRIMITIVE, ENVIRONMENT, COMBINED, ENVIRONMENT, COMBINED, 0, ENVIRONMENT, 0);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 155, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 200, 200, 255);
    gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_object_fz_DL_001130));
    CLOSE_DISPS(gfxCtx);
    Light(play, 155, 155, 155, 20.0f, 20.0f, 20.0f);
}

void DrawGaro(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SetupSkel(play, s, mm_gGaroSkel, mm_gGaroIdleAnim, GARO_LIMB_MAX, true);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gEmptyDL);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 150, 255, 150, 8.0f, 8.0f, 8.0f);
}

void DrawGekko(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    Matrix_Translate(0, -4100.0f, 0, MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SetupSkel(play, s, mm_gGekkoSkel, mm_gGekkoBoxingStanceAnim, GEKKO_LIMB_MAX, true);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 150, 100, 255, 20.0f, 20.0f, 20.0f);
}

void DrawGiantBee(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    SetupSkel(play, s, mm_gBeeSkel, mm_gBeeFlyingAnim, OBJECT_BEE_LIMB_MAX, false);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawGomess(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gGomessSkel, mm_gGomessFloatAnim, GOMESS_LIMB_MAX, true);
    MergeShip_AnimatedMat_DrawStepOpa(play, (void*)mm_gGomessBodyMatAnim, 23);
    MergeShip_AnimatedMat_DrawOpa(play, (void*)mm_gGomessCoreMatAnim);
    MergeShip_Scene_SetRenderModeXlu(play, 0, 1);
    gDPSetEnvColor(POLY_OPA_DISP++, 30, 30, 0, 255);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 0, 0, 15.0f, 15.0f, 15.0f);
}

void DrawGuay(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gGuaySkel, mm_gGuayFlyAnim, OBJECT_CROW_LIMB_MAX, true);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 6.0f, 6.0f, 6.0f);
}

void DrawHiploop(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    Matrix_Translate(0, -1400.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gHiploopSkel, mm_gHiploopChargeAnim, HIPLOOP_LIMB_MAX, true);
    MergeShip_Scene_SetRenderModeXlu(play, 0, 1);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawIgosDuIkana(PlayState* play) {
    static GiSkel s;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)EmptyDL(gfxCtx));
    gSPSegment(POLY_XLU_DISP++, 0x0A, (uintptr_t)EmptyDL(gfxCtx));
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)EmptyDL(gfxCtx));
    Gfx_SetupDL_25Opa(gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -2000.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gIgosSkel, mm_gKnightIdleAnim, IGOS_LIMB_MAX, true);
    DrawFlexOpa(play, s);
    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
    CLOSE_DISPS(gfxCtx);
    Light(play, 0, 0, 0, 10.0f, 10.0f, 10.0f);
}

// 2Ship DrawEnIk_PostLimbDraw: the helmet's marking.
void IronKnucklePostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* arg) {
    if (limbIndex == IRON_KNUCKLE_LIMB_HELMET_ARMOR) {
        OPEN_DISPS(play->state.gfxCtx);
        MS_MTX_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gIronKnuckleHelmetMarkingDL));
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void DrawIronKnuckle(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -2900.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gIronKnuckleSkel, mm_gIronKnuckleWalkAnim, IRON_KNUCKLE_LIMB_MAX, true);
    POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_25);
    POLY_OPA_DISP = Gfx_SetupDL(POLY_OPA_DISP, SETUPDL_25);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gIronKnuckleBlackArmorMaterialDL);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)mm_gIronKnuckleBrownArmorMaterialDL);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)mm_gIronKnuckleBrownArmorMaterialDL);
    DrawFlexOpa(play, s, nullptr, IronKnucklePostLimbDraw);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 12.0f, 12.0f, 12.0f);
}

// 2Ship DrawEnFirefly_PostLimbDraw: red eyes and the flying fire's dust.
void KeesePostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* arg) {
    static Color_RGBA8 sAuraPrim[2] = { { 255, 255, 100, 255 }, { 100, 200, 255, 255 } };
    static Color_RGBA8 sAuraEnv[2] = { { 255, 50, 0, 0 }, { 0, 0, 255, 0 } };
    static u32 sDustUpdate = 0;
    static bool sAuraColor = false;
    static Vec3f sAuraVelocity = { 0, 0.5f, 0 };
    static Vec3f sAuraAccel = { 0, 0.5f, 0 };
    Vec3f auraPos = { 0.0f, 0.0f, 0.0f };

    Matrix_MultVec3f(&auraPos, &auraPos);
    auraPos.x += Rand_ZeroOne() * 0.5f;
    auraPos.y += Rand_ZeroOne() * 0.5f;
    auraPos.z += Rand_ZeroOne() * 0.5f;
    if ((play->state.frames - sDustUpdate) >= 20) {
        sDustUpdate = play->state.frames;
        sAuraColor = !sAuraColor;
    }
    if (limbIndex == FIRE_KEESE_LIMB_HEAD) {
        OPEN_DISPS(play->state.gfxCtx);
        MergeShip_Scene_SetRenderModeXlu(play, 1, 2);
        MS_MTX_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, MS_DL(mm_gKeeseRedEyesDL));
        CLOSE_DISPS(play->state.gfxCtx);
    }
    if ((limbIndex == FIRE_KEESE_LIMB_LEFT_WING_END) || (limbIndex == FIRE_KEESE_LIMB_RIGHT_WING_END_ROOT)) {
        EffectSsDust_Spawn(play, 2, &auraPos, &sAuraVelocity, &sAuraAccel, &sAuraPrim[sAuraColor],
                           &sAuraEnv[sAuraColor], 100, -40, 3, 0);
    }
}

void DrawKeese(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -700.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gFireKeeseSkel, mm_gFireKeeseFlyAnim, FIRE_KEESE_LIMB_MAX, false);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 0);
    DrawOpa(play, s, nullptr, KeesePostLimbDraw);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawLeever(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
    Matrix_Translate(0, -700.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gLeeverSkel, mm_gLeeverSpinAnim, LEEVER_LIMB_MAX, false);
    // 2Ship: the Leever's spin runs backwards, so it and the get-item's own spin do not add up.
    s.skelAnime.playSpeed = -1.0f;
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x01, 255, 255, 255, 255);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 3.0f, 3.0f, 3.0f);
}

void DrawLikeLike(PlayState* play) {
    struct Segment {
        f32 scale;
        f32 pulse;
        f32 offset;
        Vec3s rot;
    };
    static Segment sSegments[5] = {
        { 0.8f, 0.0f, 0.0f, { 0, 0, 0 } }, { 0.8f, 0.0f, 0.0f, { 0, 0, 0 } }, { 0.8f, 0.0f, 0.0f, { 0, 0, 0 } },
        { 0.8f, 0.0f, 0.0f, { 0, 0, 0 } }, { 0.8f, 0.0f, 0.0f, { 0, 0, 0 } },
    };
    static u32 sLastUpdate = 0;
    static s16 sTextureScroll = 0;
    GraphicsContext* gfxCtx = play->state.gfxCtx;

    if (sLastUpdate != play->state.frames) {
        sLastUpdate = play->state.frames;
        sTextureScroll++;
        f32 phase = play->state.frames * (2500.0f * (2.0f * (f32)M_PI / 65536.0f));
        for (s32 j = 0; j < 5; j++) {
            sSegments[j].pulse = cosf(phase + (j * 0x4000) * (2.0f * (f32)M_PI / 65536.0f)) * 0.15f;
            sSegments[j].offset = 0.0f;
        }
        for (s32 j = 1; j < 5; j++) {
            sSegments[j].rot.x = (s16)(cosf(phase + (j * 0x3000) * (2.0f * (f32)M_PI / 65536.0f)) * 2048.0f);
            sSegments[j].rot.z = (s16)(sinf(phase + (j * 0x1000) * (2.0f * (f32)M_PI / 65536.0f)) * 2048.0f);
        }
    }

    Mtx* mtx = (Mtx*)Graph_Alloc(gfxCtx, 4 * sizeof(Mtx));
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Opa(gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3000.0f, 0, MTXMODE_APPLY);
    Matrix_Push();
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)mtx);
    gSPSegment(POLY_OPA_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x20, 0x10, 1, 0, (sTextureScroll * -6) & 0x7F, 0x20, 0x10, 0, 0,
                                  0, -6));
    Matrix_Push();
    Matrix_Scale((1.0f + sSegments[0].pulse) * sSegments[0].scale, 1.0f,
                 (1.0f + sSegments[0].pulse) * sSegments[0].scale, MTXMODE_APPLY);
    MS_MTX_LOAD(POLY_OPA_DISP++, gfxCtx);
    Matrix_Pop();
    for (s32 i = 1; i < 5; i++) {
        f32 s = sSegments[i].scale * (sSegments[i].pulse + 1.0f);
        Matrix_Translate(0.0f, sSegments[i].offset + 1000.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateZYX(sSegments[i].rot.x, sSegments[i].rot.y, sSegments[i].rot.z, MTXMODE_APPLY);
        Matrix_Push();
        Matrix_Scale(s, 1.0f, s, MTXMODE_APPLY);
        Matrix_ToMtx(mtx, (char*)__FILE__, __LINE__);
        Matrix_Pop();
        mtx++;
    }
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gLikeLikeDL));
    CLOSE_DISPS(gfxCtx);
    Matrix_Pop();
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawMadScrub(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -2300, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gDekuScrubSkel, mm_gDekuScrubLookAroundAnim, DEKU_SCRUB_LIMB_MAX, false);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawNejiron(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gNejironSkel, mm_gNejironIdleAnim, NEJIRON_LIMB_MAX, false);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gNejironEyeOpenTex);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 13.0f, 13.0f, 13.0f);
}

void DrawOctorok(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.007f, 0.007f, 0.007f, MTXMODE_APPLY);
    Matrix_Translate(0, -700.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gOctorokSkel, mm_gOctorokFloatAnim, OCTOROK_LIMB_MAX, false);
    POLY_OPA_DISP = Gfx_SetupDL(POLY_OPA_DISP, SETUPDL_25);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gEmptyDL);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawPeahat(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_object_ph_Skel_001C80, mm_object_ph_Anim_0009C4, OBJECT_PH_LIMB_MAX, false);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawPirate(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -2000.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gFighterPirateSkel, mm_gFighterPirateFightingIdleAnim, KAIZOKU_LIMB_MAX, true);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gFighterPirateEyeOpenTex);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawPoe(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.0075f, 0.0075f, 0.0075f, MTXMODE_APPLY);
    Matrix_Translate(0, -5000.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gPoeSkel, mm_gPoeFloatAnim, POE_LIMB_MAX, false);
    POLY_OPA_DISP = Gfx_SetupDL(POLY_OPA_DISP, SETUPDL_25);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gEmptyDL);
    POLY_OPA_DISP = SkelAnime_Draw(play, s.skelAnime.skeleton, s.skelAnime.jointTable, NULL, NULL, NULL, POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawRedead(PlayState* play) {
    static GiSkel s;
    static u32 sAnimUpdate = 0;
    static u32 sAnimIndex = 0;
    static const char* const sDances[] = {
        mm_gGibdoRedeadSquattingDanceAnim,
        mm_gGibdoRedeadClappingDanceAnim,
        mm_gGibdoRedeadPirouetteAnim,
    };
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_60NoCDXlu(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -2900.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gRedeadSkel, mm_gGibdoRedeadPirouetteAnim, REDEAD_LIMB_MAX, true);
    // 2Ship cycles the ReDead's three dances every 35 frames.
    if ((play->state.frames - sAnimUpdate) >= 35) {
        sAnimUpdate = play->state.frames;
        Animation_MorphToLoop(&s.skelAnime, (AnimationHeader*)sDances[sAnimIndex], -6.0f);
        sAnimIndex = (sAnimIndex + 1) % 3;
    }
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gEmptyDL);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawShellBlade(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.007f, 0.007f, 0.007f, MTXMODE_APPLY);
    Matrix_Translate(0, -3500.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_object_sb_Skel_002BF0, mm_object_sb_Anim_000194, OBJECT_SB_LIMB_MAX, true);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawSkullfish(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_object_pr_Skel_004188, mm_object_pr_Anim_004340, OBJECT_PR_2_LIMB_MAX, true);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    MergeShip_Scene_SetRenderModeXlu(play, 0, 1);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 5.0f, 5.0f, 5.0f);
}

void DrawSkulltula(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_object_st_Skel_005298, mm_object_st_Anim_000304, OBJECT_ST_LIMB_MAX, false);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 5.0f, 5.0f, 5.0f);
}

void DrawSnapper(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3100.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gSnapperSkel, mm_gSnapperIdleAnim, SNAPPER_LIMB_MAX, true);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gSnapperEyeOpenTex);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

// 2Ship DrawEnSkb_OverrideLimbDraw: the Stalchild's head glows.
s32 StalchildOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    if (limbIndex == STALCHILD_LIMB_HEAD) {
        s16 glow = (s16)(fabsf(Math_SinS(play->gameplayFrames * 6000) * 95.0f) + 160.0f);
        OPEN_DISPS(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, glow, glow, glow, 255);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    return false;
}

void DrawStalchild(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3200, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gStalchildSkel, mm_gStalchildIdleAnim, STALCHILD_LIMB_MAX, false);
    DrawOpa(play, s, StalchildOverrideLimbDraw);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawTakkuri(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gTakkuriSkel, mm_gTakkuriFlyAnim, TAKKURI_LIMB_MAX, true);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawTektite(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -2900.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_object_tite_Skel_003A20, mm_object_tite_Anim_0012E4, OBJECT_TITE_LIMB_MAX, false);
    POLY_OPA_DISP = Gfx_SetupDL(POLY_OPA_DISP, SETUPDL_25);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_object_tite_Tex_001300);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)mm_object_tite_Tex_001700);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)mm_object_tite_Tex_001900);
    DrawOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawWallmaster(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3500.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gWallmasterSkel, mm_gWallmasterIdleAnim, WALLMASTER_LIMB_MAX, true);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawWart(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gWartSkel, mm_gWartIdleAnim, WART_LIMB_MAX, true);
    DrawFlexOpa(play, s);
    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

void DrawWizrobe(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gWizrobeSkel, mm_gWizrobeIdleAnim, WIZROBE_LIMB_MAX, true);
    MergeShip_Scene_SetRenderModeXlu(play, 0, 1);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gWizrobeEyeTex);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 15.0f, 15.0f, 15.0f);
}

void DrawWolfos(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3000.0f, 0, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gWolfosNormalSkel, mm_gWolfosWaitAnim, WOLFOS_NORMAL_LIMB_MAX, true);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gWolfosNormalEyeOpenTex);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 155, 155, 155, 10.0f, 10.0f, 10.0f);
}

// ---- boss souls -----------------------------------------------------------------------------------------------------

void DrawGoht(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gGohtSkel, mm_gGohtRunAnim, GOHT_LIMB_MAX, true);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gGohtMetalPlateWithCirclePatternTex);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 10, 138, 46, 30.0f, 30.0f, 30.0f);
}

void DrawGyorg(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gGyorgSkel, mm_gGyorgGentleSwimmingAnim, GYORG_LIMB_MAX, true);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 19, 99, 165, 3.0f, 3.0f, 3.0f);
}

void DrawOdolwa(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gOdolwaSkel, mm_gOdolwaReadyAnim, ODOLWA_LIMB_MAX, true);
    DrawFlexOpa(play, s);
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 145, 20, 133, 25.0f, 25.0f, 25.0f);
}

void DrawTwinmold(PlayState* play) {
    static GiSkel s;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Opa(gfxCtx);
    Matrix_Scale(0.06f, 0.06f, 0.06f, MTXMODE_APPLY);
    SetupSkel(play, s, mm_gTwinmoldHeadSkel, mm_gTwinmoldHeadFlyAnim, TWINMOLD_HEAD_LIMB_MAX, true);
    Mtx* mtxHead = (Mtx*)Graph_Alloc(gfxCtx, 23 * sizeof(Mtx));
    gSPSegment(POLY_OPA_DISP++, 0x0D, (uintptr_t)mtxHead);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gTwinmoldBlueSkinTex);
    DrawOpa(play, s);
    CLOSE_DISPS(gfxCtx);
    Light(play, 168, 180, 20, 3.0f, 3.0f, 3.0f);
}

void DrawMajora(PlayState* play) {
    static GiSkel s;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    SetupSkel(play, s, mm_gMajorasMaskSkel, mm_gMajorasMaskFloatingAnim, MAJORAS_MASK_LIMB_MAX, false);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)mm_gMajorasMaskWithNormalEyesTex);
    DrawOpa(play, s);
    gSPDisplayList(POLY_OPA_DISP++, MS_DL(mm_gMajorasMaskTentacleMaterialDL));
    CLOSE_DISPS(play->state.gfxCtx);
    Light(play, 232, 128, 21, 3.0f, 3.0f, 3.0f);
}

// ---- dispatch -------------------------------------------------------------------------------------------------------

void DrawRow(PlayState* play, const MsTerminaGiRow& row) {
    switch (row.kind) {
        case MSTGI_MMGid:
        case MSTGI_DrawSmallKey: // MM's own key: 2Ship's emblem keys are 2ship.o2r's
        case MSTGI_DrawBossKey:
            MMGi_Draw(play, row.gid);
            return;
        case MSTGI_DrawSong: DrawSong(play, row.rg); return;
        case MSTGI_DrawStrayFairy: MergeShip_StrayFairy_DrawModel(play, MM_STRAY_FAIRY_AREA_CLOCK_TOWN); return;
        case MSTGI_DrawSkulltulaToken: DrawSkulltulaToken(play, row.gid); return;
        case MSTGI_DrawOwlStatue: DrawOwlStatue(play); return;
        case MSTGI_DrawClock: DrawClock(play, row.rg); return;
        case MSTGI_DrawMinifrog: DrawMinifrog(play, row.rg); return;
        case MSTGI_DrawMilkRefill: DrawMilkRefill(play); return;
        case MSTGI_DrawDoubleDefense: DrawDoubleDefense(play); return;
        case MSTGI_DrawAlien: DrawAlien(play); return;
        case MSTGI_DrawArmos: DrawArmos(play); return;
        case MSTGI_DrawBat: DrawBat(play); return;
        case MSTGI_DrawBeamos: DrawBeamos(play); return;
        case MSTGI_DrawBoe: DrawBoe(play); return;
        case MSTGI_DrawBubble: DrawBubble(play); return;
        case MSTGI_DrawCaptainKeeta: DrawCaptainKeeta(play); return;
        case MSTGI_DrawChuchu: DrawChuchu(play); return;
        case MSTGI_DrawDeathArmos: DrawDeathArmos(play); return;
        case MSTGI_DrawDeepPython: DrawDeepPython(play); return;
        case MSTGI_DrawDekuBaba: DrawDekuBaba(play); return;
        case MSTGI_DrawDexihand: DrawDexihand(play); return;
        case MSTGI_DrawDinolfos: DrawDinolfos(play); return;
        case MSTGI_DrawDodongo: DrawDodongo(play); return;
        case MSTGI_DrawDragonfly: DrawDragonfly(play); return;
        case MSTGI_DrawEeno: DrawEeno(play); return;
        case MSTGI_DrawEyegore: DrawEyegore(play); return;
        case MSTGI_DrawFreezard: DrawFreezard(play); return;
        case MSTGI_DrawGaro: DrawGaro(play); return;
        case MSTGI_DrawGekko: DrawGekko(play); return;
        case MSTGI_DrawGiantBee: DrawGiantBee(play); return;
        case MSTGI_DrawGomess: DrawGomess(play); return;
        case MSTGI_DrawGuay: DrawGuay(play); return;
        case MSTGI_DrawHiploop: DrawHiploop(play); return;
        case MSTGI_DrawIgosDuIkana: DrawIgosDuIkana(play); return;
        case MSTGI_DrawIronKnuckle: DrawIronKnuckle(play); return;
        case MSTGI_DrawKeese: DrawKeese(play); return;
        case MSTGI_DrawLeever: DrawLeever(play); return;
        case MSTGI_DrawLikeLike: DrawLikeLike(play); return;
        case MSTGI_DrawMadScrub: DrawMadScrub(play); return;
        case MSTGI_DrawNejiron: DrawNejiron(play); return;
        case MSTGI_DrawOctorok: DrawOctorok(play); return;
        case MSTGI_DrawPeahat: DrawPeahat(play); return;
        case MSTGI_DrawPirate: DrawPirate(play); return;
        case MSTGI_DrawPoe: DrawPoe(play); return;
        case MSTGI_DrawRedead: DrawRedead(play); return;
        case MSTGI_DrawShellBlade: DrawShellBlade(play); return;
        case MSTGI_DrawSkullfish: DrawSkullfish(play); return;
        case MSTGI_DrawSkulltula: DrawSkulltula(play); return;
        case MSTGI_DrawSnapper: DrawSnapper(play); return;
        case MSTGI_DrawStalchild: DrawStalchild(play); return;
        case MSTGI_DrawTakkuri: DrawTakkuri(play); return;
        case MSTGI_DrawTektite: DrawTektite(play); return;
        case MSTGI_DrawWallmaster: DrawWallmaster(play); return;
        case MSTGI_DrawWart: DrawWart(play); return;
        case MSTGI_DrawWizrobe: DrawWizrobe(play); return;
        case MSTGI_DrawWolfos: DrawWolfos(play); return;
        case MSTGI_DrawGoht: DrawGoht(play); return;
        case MSTGI_DrawGyorg: DrawGyorg(play); return;
        case MSTGI_DrawMajora: DrawMajora(play); return;
        case MSTGI_DrawOdolwa: DrawOdolwa(play); return;
        case MSTGI_DrawTwinmold: DrawTwinmold(play); return;
    }
}

} // namespace

extern "C" void MergeShip_DrawGiTerminaItem(PlayState* play, GetItemEntry* getItemEntry) {
    // The entry's draw item: the item itself, or the item an ice trap looks like (MergeShipRandoMasks.cpp).
    const MsTerminaGiRow* row =
        (getItemEntry->drawModIndex == MOD_RANDOMIZER) ? FindRow(getItemEntry->drawItemId) : nullptr;
    if (row == nullptr) {
        row = FindRow(getItemEntry->getItemId);
    }
    if ((row == nullptr) || !MergeShip_IsMajorasMaskMounted()) {
        GetItem_Draw(play, (s16)getItemEntry->gid);
        return;
    }
    DrawRow(play, *row);
}

extern "C" int MergeShip_TerminaGi_Has(int rg) {
    return FindRow(rg) != nullptr;
}

extern "C" void MergeShip_DrawMMGetItem(PlayState* play, int mmGid) {
    if (MergeShip_IsMajorasMaskMounted()) {
        MMGi_Draw(play, (s16)mmGid);
    }
}
