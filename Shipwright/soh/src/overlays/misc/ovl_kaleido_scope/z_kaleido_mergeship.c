// MergeShip2 -- Majora's Mask pages inside Ocarina of Time's pause menu, for MM's songs (MergeShipSongs.h,
// MergeShipTermina.h):
//
//  * the Song of Soaring's owl-warp map (MM z_kaleido_map.c KaleidoScope_DrawWorldMap / UpdateWorldMapCursor with
//    IS_PAUSE_STATE_OWL_WARP, z_kaleido_scope_NES.c PAUSE_STATE_OWL_WARP_SELECT / _CONFIRM,
//    KaleidoScope_UpdateOwlWarpNamePanel, KaleidoScope_DrawCursor), drawn on Ocarina of Time's map face;
//  * the Termina quest status sub-page (MM z_kaleido_collect.c KaleidoScope_DrawQuestStatus /
//    KaleidoScope_UpdateQuestCursor, MM's quest vertices from z_kaleido_scope_NES.c), drawn on Ocarina of Time's
//    quest face;
//  * the Termina equipment view of the Equipment face (MM's swords, shields and bags);
//  * C-Up, which switches the Quest, Equipment and Item faces between Ocarina of Time's screen and Majora's Mask's,
//    and its "[C-Up] Termina" / "[C-Up] Hyrule" label.
//
// Coordinates are MM's page coordinates. Ocarina of Time's pause pages share them (both draw a page of 3 x 5 panels
// of 80 x 32 around the origin, and Ocarina of Time's world map image sits at MM's x -108..108, y 58..-70), so MM's
// numbers are used as they are.

#include "z_kaleido_scope.h"
#include "textures/parameter_static/parameter_static.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "soh/MergeShip/MergeShipSingleplayer.h"
#include "soh/MergeShip/MergeShipSongs.h"
#include "soh/MergeShip/MergeShipTermina.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipHud.h"
#include "soh/MergeShip/MergeShipPause.h"
#include "soh/MergeShip/MergeShipMMEquipment.h"
#include "soh/MergeShip/MergeShipMMItems.h"
#include "soh/MergeShip/MergeShipOwnership.h"
#include "mm/interface/icon_item_field_static/icon_item_field_static.h"
#include "mm/archives/icon_item_static/icon_item_static_yar.h"
#include "mm/archives/item_name_static/item_name_static.h"
#include "mm/archives/map_name_static/map_name_static.h"
#include <string.h>
#include <libultraship/bridge/consolevariablebridge.h>

void MergeShipTest_Log(const char* fmt, ...);

// A quad of 4 vertices in page coordinates: top-left (x, y), w x h, texture tw x th.
static void MergeShip_Kaleido_SetQuad(Vtx* v, s16 x, s16 y, s16 w, s16 h, s16 tw, s16 th, u8 alpha) {
    s32 i;

    v[0].v.ob[0] = v[2].v.ob[0] = x;
    v[1].v.ob[0] = v[3].v.ob[0] = x + w;
    v[0].v.ob[1] = v[1].v.ob[1] = y;
    v[2].v.ob[1] = v[3].v.ob[1] = y - h;
    for (i = 0; i < 4; i++) {
        v[i].v.ob[2] = 0;
        v[i].v.flag = 0;
        v[i].v.cn[0] = v[i].v.cn[1] = v[i].v.cn[2] = 255;
        v[i].v.cn[3] = alpha;
    }
    v[0].v.tc[0] = v[0].v.tc[1] = v[1].v.tc[1] = v[2].v.tc[0] = 0;
    v[1].v.tc[0] = v[3].v.tc[0] = tw << 5;
    v[2].v.tc[1] = v[3].v.tc[1] = th << 5;
}

// ==== The owl-warp map ===========================================================================================

// MM sVtxPageMapWorldQuadsX / Y / Width / Height, entries "world map owl warp <place>" (24 x 12 each).
static s16 sOwlQuadX[MERGESHIP_OWL_MAX] = { -80, -64, -9, -3, -7, -16, -1, 23, 44, 54 };
static s16 sOwlQuadY[MERGESHIP_OWL_MAX] = { -8, -38, 39, 26, 1, -7, -28, -27, -1, 24 };

// MM sOwlWarpPauseItems -> map_name_static (Kaleido_LoadMapNameStatic).
static const char* sOwlNameTextures[MERGESHIP_OWL_MAX] = {
    mm_gMapPointGreatBayCoastENGTex,  mm_gMapPointZoraCapeENGTex,  mm_gMapPointSnowheadENGTex,
    mm_gMapPointMountainVillageENGTex, mm_gMapPointClockTownENGTex, mm_gMapPointMilkRoadENGTex,
    mm_gMapPointWoodfallENGTex,        mm_gMapPointSouthernSwampENGTex, mm_gMapPointIkanaCanyonENGTex,
    mm_gMapPointStoneTowerENGTex,
};

// The owl-warp name panel's namedItem values (past any item id).
#define MERGESHIP_OWL_NAMED_ITEM(owl) (0x300 + (owl))

static s32 sOwlWarpConfirming = false; // MM PAUSE_STATE_OWL_WARP_CONFIRM

// MM KaleidoScope_DrawCursor / UpdateCursorSize for the owl-warp map (cursorColorSet PAUSE_CURSOR_COLOR_SET_BLUE).
static f32 sOwlCursorShrinkRate = 0.0f;
static s16 sOwlCursorSpinPhase = 0;
static s16 sOwlCursorPrim[3] = { 100, 150, 255 };
static s16 sOwlCursorEnv[3] = { 0, 0, 100 };
static s16 sOwlCursorColorTimer = 10;
static s16 sOwlCursorColorTargetIndex = 0;
static s16 sOwlCursorPrimTargets[2][3] = { { 100, 150, 255 }, { 100, 255, 255 } }; // MM sCursorPrimColorTarget[4..5]
static s16 sOwlCursorEnvTargets[2][3] = { { 0, 0, 100 }, { 0, 150, 255 } };       // MM sCursorEnvColorTarget[4..5]
static s16 sOwlDarkAlpha = 0;                                                        // MM R_PAUSE_OWL_WARP_ALPHA

// MM KaleidoScope_UpdateWorldMapCursor, PAUSE_STATE_OWL_WARP_SELECT: left / right step to the previous / next
// activated statue, wrapping.
static void MergeShip_OwlWarp_UpdateCursor(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s32 dpad = CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0);
    u8 cursor = MergeShip_OwlWarp_GetCursorOwl();
    u8 old = cursor;
    s32 steps;

    if (MergeShip_Termina_GetOwlActivationFlags() == 0) {
        return;
    }
    if ((pauseCtx->stickRelX > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT))) {
        sOwlCursorShrinkRate = 4.0f;
        steps = 0;
        do {
            cursor = (cursor >= MERGESHIP_OWL_STONE_TOWER) ? MERGESHIP_OWL_GREAT_BAY_COAST : (cursor + 1);
        } while (!MergeShip_Termina_IsOwlActive(cursor) && (++steps < MERGESHIP_OWL_MAX));
    } else if ((pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT))) {
        sOwlCursorShrinkRate = 4.0f;
        steps = 0;
        do {
            cursor = (cursor <= MERGESHIP_OWL_GREAT_BAY_COAST) ? MERGESHIP_OWL_STONE_TOWER : (cursor - 1);
        } while (!MergeShip_Termina_IsOwlActive(cursor) && (++steps < MERGESHIP_OWL_MAX));
    }

    if (cursor != old) {
        MergeShip_OwlWarp_SetCursorOwl(cursor);
        Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

// MM KaleidoScope_DrawPages' cursor colour step, for the blue set.
static void MergeShip_OwlWarp_StepCursorColor(void) {
    s32 c;

    for (c = 0; c < 3; c++) {
        s16 step = ABS(sOwlCursorPrim[c] - sOwlCursorPrimTargets[sOwlCursorColorTargetIndex][c]) / sOwlCursorColorTimer;
        sOwlCursorPrim[c] += (sOwlCursorPrim[c] >= sOwlCursorPrimTargets[sOwlCursorColorTargetIndex][c]) ? -step : step;
        step = ABS(sOwlCursorEnv[c] - sOwlCursorEnvTargets[sOwlCursorColorTargetIndex][c]) / sOwlCursorColorTimer;
        sOwlCursorEnv[c] += (sOwlCursorEnv[c] >= sOwlCursorEnvTargets[sOwlCursorColorTargetIndex][c]) ? -step : step;
    }
    if (--sOwlCursorColorTimer == 0) {
        for (c = 0; c < 3; c++) {
            sOwlCursorPrim[c] = sOwlCursorPrimTargets[sOwlCursorColorTargetIndex][c];
            sOwlCursorEnv[c] = sOwlCursorEnvTargets[sOwlCursorColorTargetIndex][c];
        }
        sOwlCursorColorTargetIndex ^= 1;
        sOwlCursorColorTimer = 10;
    }
}

void MergeShip_OwlWarp_DrawWorldMap(PlayState* play, GraphicsContext* gfxCtx) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 selecting = (pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0) && (pauseCtx->pageIndex == PAUSE_MAP);
    Vtx* vtx;
    s16 i;
    s16 j;
    s16 t;
    s16 owl;

    if (selecting && !sOwlWarpConfirming) {
        MergeShip_OwlWarp_UpdateCursor(play);
    }

    // MM PAUSE_STATE_OWL_WARP_3 / _6: the darkening fades in (+20 a frame to 120) and out (-60 a frame).
    if (pauseCtx->state == 6) {
        sOwlDarkAlpha = CLAMP_MAX(sOwlDarkAlpha + 20, 120);
    } else if (pauseCtx->state >= 7) {
        sOwlDarkAlpha = CLAMP_MIN(sOwlDarkAlpha - 60, 0);
    } else {
        sOwlDarkAlpha = 0;
    }

    // Quads: the darkening, 10 owl faces, 4 cursor circles.
    vtx = Graph_Alloc(gfxCtx, (1 + MERGESHIP_OWL_MAX + 4) * 4 * sizeof(Vtx));

    OPEN_DISPS(gfxCtx);

    // MM's world map image, on Ocarina of Time's own world-map geometry (mapPageVtx[188..], 216 x 128 in rows of 9).
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_POINT);
    gDPLoadTLUT_pal256(POLY_OPA_DISP++, mm_gWorldMapImageTLUT);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_RGBA16);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gSPVertex(POLY_OPA_DISP++, &pauseCtx->mapPageVtx[188], 32, 0);

    for (j = t = i = 0; i < 8; i++, t++, j += 4) {
        gDPLoadMultiTile(POLY_OPA_DISP++, mm_gWorldMapImageTex, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, 216, 128,
                         0, t * 9, 216 - 1, (t + 1) * 9 - 1, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR,
                         G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPSetTileSize(POLY_OPA_DISP++, G_TX_RENDERTILE, 0, 0, (216 - 1) << G_TEXTURE_IMAGE_FRAC,
                       (9 - 1) << G_TEXTURE_IMAGE_FRAC);
        gSP1Quadrangle(POLY_OPA_DISP++, j, j + 2, j + 3, j + 1, 0);
    }

    gSPVertex(POLY_OPA_DISP++, &pauseCtx->mapPageVtx[220], 28, 0);

    for (j = i = 0; i < 6; i++, t++, j += 4) {
        gDPLoadMultiTile(POLY_OPA_DISP++, mm_gWorldMapImageTex, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, 216, 128,
                         0, t * 9, 216 - 1, (t + 1) * 9 - 1, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR,
                         G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPSetTileSize(POLY_OPA_DISP++, G_TX_RENDERTILE, 0, 0, (216 - 1) << G_TEXTURE_IMAGE_FRAC,
                       (9 - 1) << G_TEXTURE_IMAGE_FRAC);
        gSP1Quadrangle(POLY_OPA_DISP++, j, j + 2, j + 3, j + 1, 0);
    }

    gDPLoadMultiTile(POLY_OPA_DISP++, mm_gWorldMapImageTex, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, 216, 128, 0,
                     t * 9, 216 - 1, (t * 9 + 2) - 1, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR,
                     G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetTileSize(POLY_OPA_DISP++, G_TX_RENDERTILE, 0, 0, (216 - 1) << G_TEXTURE_IMAGE_FRAC,
                   (2 - 1) << G_TEXTURE_IMAGE_FRAC);
    gSP1Quadrangle(POLY_OPA_DISP++, j, j + 2, j + 3, j + 1, 0);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_BILERP);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);

    // MM (IS_PAUSE_STATE_OWL_WARP): the map is darkened (gDPFillRectangle 50, 62 .. 270, 190 in XLU with
    // R_PAUSE_OWL_WARP_ALPHA). Drawn here as a quad over the image in page coordinates, so it turns with the face
    // while Ocarina of Time's menu opens.
    Gfx_SetupDL_42Opa(gfxCtx);
    MergeShip_Kaleido_SetQuad(&vtx[0], -110, 59 + pauseCtx->offsetY, 220, 128, 0, 0, 255);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 0, (sOwlDarkAlpha * pauseCtx->alpha) / 255);
    gSPVertex(POLY_OPA_DISP++, &vtx[0], 4, 0);
    gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);

    // MM: an owl face on each activated statue (gWorldMapOwlFaceTex, RGBA32 24 x 12).
    Gfx_SetupDL_42Opa(gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPLoadTextureBlock(POLY_OPA_DISP++, mm_gWorldMapOwlFaceTex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 24, 12, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    for (owl = 0; owl < MERGESHIP_OWL_MAX; owl++) {
        Vtx* q = &vtx[(1 + owl) * 4];

        MergeShip_Kaleido_SetQuad(q, sOwlQuadX[owl], sOwlQuadY[owl] + pauseCtx->offsetY, 24, 12, 24, 12,
                                  pauseCtx->alpha);
        if (MergeShip_Termina_IsOwlActive((u8)owl)) {
            gSPVertex(POLY_OPA_DISP++, q, 4, 0);
            gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
        }
    }

    // MM KaleidoScope_DrawCursor while selecting (not while "Soar to ...?" is asked): four spinning circles around
    // the statue. MM draws them outside the page's 0.78 scale; in page units they are 16 / 0.78 wide on a
    // cursorWidth / 0.78 radius (MM UpdateCursorSize: 15, minus the shrink after a move).
    MergeShip_OwlWarp_StepCursorColor();
    if (selecting && !sOwlWarpConfirming && MergeShip_Termina_IsOwlActive(MergeShip_OwlWarp_GetCursorOwl())) {
        f32 radius = 15.0f;
        s16 cx;
        s16 cy;
        s16 size = 20;

        owl = MergeShip_OwlWarp_GetCursorOwl();
        cx = sOwlQuadX[owl] + 12;
        cy = sOwlQuadY[owl] - 6 + pauseCtx->offsetY;
        radius = (radius < sOwlCursorShrinkRate) ? 0.0f : (radius - sOwlCursorShrinkRate);
        if (sOwlCursorShrinkRate != 0.0f) {
            sOwlCursorShrinkRate -= 1.0f;
        }
        radius /= 0.78f;
        sOwlCursorSpinPhase += 0x300;

        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                          PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, sOwlCursorPrim[0], sOwlCursorPrim[1], sOwlCursorPrim[2], 255);
        gDPSetEnvColor(POLY_OPA_DISP++, sOwlCursorEnv[0], sOwlCursorEnv[1], sOwlCursorEnv[2], 255);
        gDPLoadTextureBlock(POLY_OPA_DISP++, mm_gPauseMenuCursorTex, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 16, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        for (i = 0; i < 4; i++) {
            Vtx* q = &vtx[(1 + MERGESHIP_OWL_MAX + i) * 4];
            s16 phase = sOwlCursorSpinPhase + i * 0x4000;
            s16 x = cx + (s16)(Math_SinS(phase) * radius) - size / 2;
            s16 y = cy + (s16)(Math_CosS(phase) * radius) + size / 2;

            MergeShip_Kaleido_SetQuad(q, x, y, size, size, 16, 16, 255);
            gSPVertex(POLY_OPA_DISP++, q, 4, 0);
            gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
        }
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    }

    CLOSE_DISPS(gfxCtx);
}

void MergeShip_OwlWarp_UpdateNamePanel(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    u8 owl = MergeShip_OwlWarp_GetCursorOwl();
    u16 namedItem = MergeShip_Termina_IsOwlActive(owl) ? MERGESHIP_OWL_NAMED_ITEM(owl) : PAUSE_ITEM_NONE;

    if (pauseCtx->namedItem != namedItem) {
        pauseCtx->namedItem = namedItem;
        if (namedItem != PAUSE_ITEM_NONE) {
            memcpy(pauseCtx->nameSegment, sOwlNameTextures[owl], strlen(sOwlNameTextures[owl]) + 1);
        }
        pauseCtx->nameDisplayTimer = 0;
    }
}

// MM KaleidoScope_Update, PAUSE_STATE_OWL_WARP_SELECT and PAUSE_STATE_OWL_WARP_CONFIRM.
void MergeShip_OwlWarp_UpdateMain(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    MessageContext* msgCtx = &play->msgCtx;
    Input* input = &play->state.input[0];

    if (sOwlWarpConfirming) {
        if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
            msgCtx->msgLength = 0;
            msgCtx->msgMode = MSGMODE_NONE;
            sOwlWarpConfirming = false;
            if (msgCtx->choiceIndex == 0) {
                MergeShip_OwlWarp_Confirmed(MergeShip_OwlWarp_GetCursorOwl());
                MergeShip_OwlWarp_ClosePause(play);
                Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                MergeShipTest_Log("OWLWARP confirm owl=%d", MergeShip_OwlWarp_GetCursorOwl());
            } else {
                MergeShip_SetMMAButtonDoAction(play, MM_DO_ACTION_WARP);
                Audio_PlaySfxGeneral(NA_SE_SY_MESSAGE_PASS, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        } else if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
            msgCtx->msgLength = 0;
            msgCtx->msgMode = MSGMODE_NONE;
            sOwlWarpConfirming = false;
            Audio_PlaySfxGeneral(NA_SE_SY_MESSAGE_PASS, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        } else if (CHECK_BTN_ALL(input->press.button, BTN_START)) {
            msgCtx->msgLength = 0;
            msgCtx->msgMode = MSGMODE_NONE;
            sOwlWarpConfirming = false;
            MergeShip_OwlWarp_ClosePause(play);
        } else {
            // MM's Play_Update keeps the message system running under the pause menu; Ocarina of Time's does not.
            Message_Update(play);
        }
        return;
    }

    MergeShip_SetMMAButtonDoAction(play, MM_DO_ACTION_WARP);

    if (CHECK_BTN_ALL(input->press.button, BTN_START) || CHECK_BTN_ALL(input->press.button, BTN_B)) {
        MergeShip_OwlWarp_ClosePause(play);
        MergeShipTest_Log("OWLWARP cancel");
    } else if (CHECK_BTN_ALL(input->press.button, BTN_A) &&
               MergeShip_Termina_IsOwlActive(MergeShip_OwlWarp_GetCursorOwl())) {
        Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        Message_StartTextbox(play, MERGESHIP_TEXT_SOAR_TO, NULL);
        sOwlWarpConfirming = true;
    }
    (void)pauseCtx;
}

// ==== The Termina quest status sub-page ==========================================================================

// MM QuestItem indices used as this page's cursor points (MM sCursorPointLinks is indexed by them).
enum {
    TQ_REMAINS_ODOLWA,
    TQ_REMAINS_GOHT,
    TQ_REMAINS_GYORG,
    TQ_REMAINS_TWINMOLD,
    TQ_SHIELD, // Ocarina of Time's quest page / equipment: not on this page
    TQ_SWORD,  // ditto
    TQ_SONG_SONATA,
    TQ_SONG_LULLABY,
    TQ_SONG_BOSSA_NOVA,
    TQ_SONG_ELEGY,
    TQ_SONG_OATH,
    TQ_SONG_SARIA, // MM never shows it
    TQ_SONG_TIME,
    TQ_SONG_HEALING,
    TQ_SONG_EPONA,
    TQ_SONG_SOARING,
    TQ_SONG_STORMS,
    TQ_SONG_SUN, // MM never shows it
    TQ_BOMBERS_NOTEBOOK,
    TQ_QUIVER,      // Ocarina of Time's
    TQ_BOMB_BAG,    // Ocarina of Time's
    TQ_SKULL_TOKEN, // Ocarina of Time's
    TQ_HEART_PIECE, // Ocarina of Time's
    TQ_MAX
};

#define TQ_CURSOR_TO_LEFT (-3)
#define TQ_CURSOR_TO_RIGHT (-2)
#define TQ_CURSOR_NONE (-1)

// MM KaleidoScope_UpdateQuestCursor sCursorPointLinks { up, down, left, right }.
static s8 sTerminaQuestLinks[TQ_MAX][4] = {
    { TQ_CURSOR_NONE, TQ_REMAINS_TWINMOLD, TQ_REMAINS_GYORG, TQ_REMAINS_GOHT },           // ODOLWA
    { TQ_REMAINS_ODOLWA, TQ_SHIELD, TQ_REMAINS_TWINMOLD, TQ_CURSOR_TO_RIGHT },            // GOHT
    { TQ_REMAINS_ODOLWA, TQ_SWORD, TQ_HEART_PIECE, TQ_REMAINS_TWINMOLD },                 // GYORG
    { TQ_REMAINS_ODOLWA, TQ_SHIELD, TQ_REMAINS_GYORG, TQ_REMAINS_GOHT },                  // TWINMOLD
    { TQ_REMAINS_GOHT, TQ_BOMB_BAG, TQ_SWORD, TQ_CURSOR_TO_RIGHT },                       // SHIELD
    { TQ_REMAINS_GYORG, TQ_QUIVER, TQ_SONG_STORMS, TQ_SHIELD },                           // SWORD
    { TQ_SONG_TIME, TQ_CURSOR_NONE, TQ_CURSOR_TO_LEFT, TQ_SONG_LULLABY },                 // SONATA
    { TQ_SONG_HEALING, TQ_CURSOR_NONE, TQ_SONG_SONATA, TQ_SONG_BOSSA_NOVA },              // LULLABY
    { TQ_SONG_EPONA, TQ_CURSOR_NONE, TQ_SONG_LULLABY, TQ_SONG_ELEGY },                    // BOSSA_NOVA
    { TQ_SONG_SOARING, TQ_CURSOR_NONE, TQ_SONG_BOSSA_NOVA, TQ_SONG_OATH },                // ELEGY
    { TQ_SONG_STORMS, TQ_CURSOR_NONE, TQ_SONG_ELEGY, TQ_QUIVER },                         // OATH
    { TQ_SONG_SUN, TQ_CURSOR_NONE, TQ_SONG_OATH, TQ_SWORD },                              // SARIA
    { TQ_BOMBERS_NOTEBOOK, TQ_SONG_SONATA, TQ_CURSOR_TO_LEFT, TQ_SONG_HEALING },          // TIME
    { TQ_BOMBERS_NOTEBOOK, TQ_SONG_LULLABY, TQ_SONG_TIME, TQ_SONG_EPONA },                // HEALING
    { TQ_HEART_PIECE, TQ_SONG_BOSSA_NOVA, TQ_SONG_HEALING, TQ_SONG_SOARING },             // EPONA
    { TQ_HEART_PIECE, TQ_SONG_ELEGY, TQ_SONG_EPONA, TQ_SONG_STORMS },                     // SOARING
    { TQ_HEART_PIECE, TQ_SONG_OATH, TQ_SONG_SOARING, TQ_SWORD },                          // STORMS
    { TQ_HEART_PIECE, TQ_SONG_SARIA, TQ_SONG_STORMS, TQ_SWORD },                          // SUN
    { TQ_CURSOR_NONE, TQ_SONG_TIME, TQ_CURSOR_TO_LEFT, TQ_HEART_PIECE },                  // BOMBERS_NOTEBOOK
    { TQ_SWORD, TQ_CURSOR_NONE, TQ_SONG_OATH, TQ_BOMB_BAG },                              // QUIVER
    { TQ_SHIELD, TQ_CURSOR_NONE, TQ_QUIVER, TQ_CURSOR_TO_RIGHT },                         // BOMB_BAG
    { TQ_QUIVER, TQ_SONG_TIME, TQ_CURSOR_TO_LEFT, TQ_HEART_PIECE },                       // SKULL_TOKEN
    { TQ_CURSOR_NONE, TQ_SONG_STORMS, TQ_BOMBERS_NOTEBOOK, TQ_REMAINS_GYORG },            // HEART_PIECE
};

// MM sQuestVtxRectLeft / Top / Widths / Heights for these points, and the eight "ocarina song button" quads.
static s16 sTerminaQuestLeft[TQ_MAX] = { 45,  78,  10,  45,  80,  11,  -109, -87, -65, -41, -19, -18,
                                         -109, -87, -65, -41, -19, -18, -103, 7,   82,  -110, -54 };
static s16 sTerminaQuestTop[TQ_MAX] = { 62, 42, 42, 20, -9, -9, -20, -20, -20, -20, -20, -20,
                                        2,  2,  2,  2,  2,  2,  54,  -44, -44, 34,  58 };
static s16 sTerminaQuestWidth[TQ_MAX] = { 32, 32, 32, 32, 32, 32, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 16, 32, 32, 32, 24, 48 };
static s16 sTerminaQuestHeight[TQ_MAX] = { 32, 32, 32, 32, 32, 32, 24, 24, 24, 24, 24, 24,
                                           24, 24, 24, 24, 24, 24, 32, 32, 32, 24, 48 };
static s16 sTerminaSongButtonLeft[8] = { -98, -86, -74, -62, -50, -38, -26, -14 };
// MM pauseCtx->ocarinaButtonsY, by OCARINA_BTN_*.
static s16 sTerminaSongButtonY[5] = { -62, -56, -49, -46, -41 };

// MM sQuestSongsPrimRed / Green / Blue (QUEST_SONG_SONATA .. QUEST_SONG_SUN).
static s16 sTerminaSongPrim[12][3] = {
    { 150, 255, 100 }, { 255, 80, 40 },   { 100, 150, 255 }, { 255, 160, 0 },   { 255, 100, 255 }, { 255, 240, 100 },
    { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 },
};

// MM sQuestRemainsEnvTargets / sQuestRemainsColorTimerInit and the remains' current env colours.
static s16 sTerminaRemainsEnvTargets[2][4][3] = {
    { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
    { { 0, 60, 0 }, { 90, 0, 0 }, { 0, 40, 110 }, { 80, 40, 0 } },
};
static s16 sTerminaRemainsTimerInit[4] = { 120, 60, 2, 80 };
static s16 sTerminaRemainsEnv[4][3] = { { 255, 255, 150 }, { 255, 255, 150 }, { 255, 255, 150 }, { 255, 255, 150 } };
static s16 sTerminaRemainsTimer = 20;
static s16 sTerminaRemainsTimerIndex = 0;

static s32 sTerminaQuestShown = false;
static s16 sTerminaQuestCursor = TQ_REMAINS_ODOLWA;
static u16 sSavedOoTQuestCursorItem = PAUSE_ITEM_NONE;
static u8 sTerminaPlayedButtons[8];
static s16 sTerminaPlayedButtonsAlpha[8];
static s16 sTerminaPlayedButtonsNum = 0;

// The points that belong on this page (MM's other points are Ocarina of Time's quest page and equipment).
static s32 MergeShip_QuestTermina_IsPoint(s16 point) {
    return ((point >= TQ_REMAINS_ODOLWA) && (point <= TQ_REMAINS_TWINMOLD)) ||
           ((point >= TQ_SONG_SONATA) && (point <= TQ_SONG_OATH)) ||
           ((point >= TQ_SONG_TIME) && (point <= TQ_SONG_STORMS)) || (point == TQ_BOMBERS_NOTEBOOK);
}

// The song a point plays, as an ocarina song index (0xFF: not learned / not a song).
static u8 MergeShip_QuestTermina_PointSong(s16 point) {
    switch (point) {
        case TQ_SONG_SONATA:
            return MergeShip_Songs_IsLearned(MERGESHIP_SONG_SONATA) ? MERGESHIP_SONG_SONATA : 0xFF;
        case TQ_SONG_LULLABY:
            // MM: the lullaby's place shows the intro until the full song is learned.
            if (MergeShip_Songs_IsLearned(MERGESHIP_SONG_GORON_LULLABY)) {
                return MERGESHIP_SONG_GORON_LULLABY;
            }
            return MergeShip_Songs_IsLearned(MERGESHIP_SONG_GORON_LULLABY_INTRO) ? MERGESHIP_SONG_GORON_LULLABY_INTRO
                                                                                  : 0xFF;
        case TQ_SONG_BOSSA_NOVA:
            return MergeShip_Songs_IsLearned(MERGESHIP_SONG_NEW_WAVE) ? MERGESHIP_SONG_NEW_WAVE : 0xFF;
        case TQ_SONG_ELEGY:
            return MergeShip_Songs_IsLearned(MERGESHIP_SONG_ELEGY) ? MERGESHIP_SONG_ELEGY : 0xFF;
        case TQ_SONG_OATH:
            return MergeShip_Songs_IsLearned(MERGESHIP_SONG_OATH) ? MERGESHIP_SONG_OATH : 0xFF;
        case TQ_SONG_TIME:
            return CHECK_QUEST_ITEM(QUEST_SONG_TIME) ? OCARINA_SONG_TIME : 0xFF;
        case TQ_SONG_HEALING:
            return MergeShip_Songs_IsLearned(MERGESHIP_SONG_HEALING) ? MERGESHIP_SONG_HEALING : 0xFF;
        case TQ_SONG_EPONA:
            return CHECK_QUEST_ITEM(QUEST_SONG_EPONA) ? OCARINA_SONG_EPONAS : 0xFF;
        case TQ_SONG_SOARING:
            return MergeShip_Songs_IsLearned(MERGESHIP_SONG_SOARING) ? MERGESHIP_SONG_SOARING : 0xFF;
        case TQ_SONG_STORMS:
            return CHECK_QUEST_ITEM(QUEST_SONG_STORMS) ? OCARINA_SONG_STORMS : 0xFF;
        default:
            return 0xFF;
    }
}

// MM's cursorItem for a point (its name panel), or PAUSE_ITEM_NONE.
static u16 MergeShip_QuestTermina_PointItem(s16 point) {
    u8 song;

    if ((point >= TQ_REMAINS_ODOLWA) && (point <= TQ_REMAINS_TWINMOLD)) {
        return MergeShip_Songs_HasItem(ITEM_MM_REMAINS_ODOLWA + point) ? (ITEM_MM_REMAINS_ODOLWA + point)
                                                                        : PAUSE_ITEM_NONE;
    }
    if (point == TQ_BOMBERS_NOTEBOOK) {
        return MergeShip_Songs_HasItem(ITEM_MM_BOMBERS_NOTEBOOK) ? ITEM_MM_BOMBERS_NOTEBOOK : PAUSE_ITEM_NONE;
    }
    song = MergeShip_QuestTermina_PointSong(point);
    switch (song) {
        case OCARINA_SONG_TIME:
            return ITEM_SONG_TIME;
        case OCARINA_SONG_EPONAS:
            return ITEM_SONG_EPONA;
        case OCARINA_SONG_STORMS:
            return ITEM_SONG_STORMS;
        case MERGESHIP_SONG_SONATA:
            return ITEM_MM_SONG_SONATA;
        case MERGESHIP_SONG_GORON_LULLABY:
            return ITEM_MM_SONG_GORON_LULLABY;
        case MERGESHIP_SONG_GORON_LULLABY_INTRO:
            return ITEM_MM_SONG_GORON_LULLABY_INTRO;
        case MERGESHIP_SONG_NEW_WAVE:
            return ITEM_MM_SONG_NEW_WAVE;
        case MERGESHIP_SONG_ELEGY:
            return ITEM_MM_SONG_ELEGY;
        case MERGESHIP_SONG_OATH:
            return ITEM_MM_SONG_OATH;
        case MERGESHIP_SONG_HEALING:
            return ITEM_MM_SONG_HEALING;
        case MERGESHIP_SONG_SOARING:
            return ITEM_MM_SONG_SOARING;
        default:
            return PAUSE_ITEM_NONE;
    }
}

void MergeShip_QuestTermina_Reset(void) {
    sTerminaQuestShown = false;
}

// The Quest face can switch to Termina's quest status (MM's songs are on, and the page option).
static s32 MergeShip_QuestTermina_Available(PlayState* play) {
    return MergeShip_Songs_Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMSongs.QuestPage"), 1) &&
           !play->pauseCtx.randoQuestMode;
}

// C-Up on the Quest face is the world switch whenever the Termina quest status can be shown; the randomizer's own
// C-Up page (misc collectibles, KaleidoScope_Update) then stays closed.
s32 MergeShip_QuestTermina_OwnsCUp(PlayState* play) {
    return MergeShip_Songs_Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMSongs.QuestPage"), 1);
}

s32 MergeShip_QuestTermina_IsShown(PlayState* play) {
    // C-Up switches it (MergeShip_Kaleido_UpdateWorldSwitch, below).
    return MergeShip_QuestTermina_Available(play) && sTerminaQuestShown;
}

// C-Up on the Quest face: Ocarina of Time's quest status <-> Termina's.
static void MergeShip_QuestTermina_Switch(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    sTerminaQuestShown ^= 1;
    if (sTerminaQuestShown) {
        sSavedOoTQuestCursorItem = pauseCtx->cursorItem[PAUSE_QUEST];
        if (!MergeShip_QuestTermina_IsPoint(sTerminaQuestCursor)) {
            sTerminaQuestCursor = TQ_REMAINS_ODOLWA;
        }
        pauseCtx->cursorItem[PAUSE_QUEST] = MergeShip_QuestTermina_PointItem(sTerminaQuestCursor);
    } else {
        pauseCtx->cursorItem[PAUSE_QUEST] = sSavedOoTQuestCursorItem;
        KaleidoScope_SetCursorVtx(pauseCtx, pauseCtx->cursorSlot[PAUSE_QUEST] * 4, pauseCtx->questVtx);
    }
}

// Walks MM's link table one direction from `from`, past the points that are not on this page.
static s16 MergeShip_QuestTermina_Walk(s16 from, s32 dir) {
    s16 next = sTerminaQuestLinks[from][dir];
    s32 guard = 0;

    while ((next >= 0) && !MergeShip_QuestTermina_IsPoint(next) && (++guard < TQ_MAX)) {
        next = sTerminaQuestLinks[next][dir];
    }
    return next;
}

// MM KaleidoScope_UpdateQuestCursor (idle), and the page-arrow positions on both sides.
static void MergeShip_QuestTermina_UpdateCursor(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s32 dpad = CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0);
    s16 old = sTerminaQuestCursor;
    s16 next = TQ_CURSOR_NONE;

    if (dpad) {
        if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
            pauseCtx->stickRelX = -35;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
            pauseCtx->stickRelX = 35;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
            pauseCtx->stickRelY = -35;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
            pauseCtx->stickRelY = 35;
        }
    }

    pauseCtx->cursorColorSet = 0;
    pauseCtx->nameColorSet = 0;

    if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
        if (pauseCtx->stickRelX > 30) {
            // MM: back from the left arrow onto the Bomber's Notebook.
            pauseCtx->cursorSpecialPos = 0;
            pauseCtx->nameDisplayTimer = 0;
            sTerminaQuestCursor = TQ_BOMBERS_NOTEBOOK;
            Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    } else if (pauseCtx->cursorSpecialPos != 0) {
        if (pauseCtx->stickRelX < -30) {
            // MM: back from the right arrow onto Goht's remains.
            pauseCtx->cursorSpecialPos = 0;
            pauseCtx->nameDisplayTimer = 0;
            sTerminaQuestCursor = TQ_REMAINS_GOHT;
            Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    } else {
        if (pauseCtx->stickRelX < -30) {
            next = MergeShip_QuestTermina_Walk(old, 2);
        } else if (pauseCtx->stickRelX > 30) {
            next = MergeShip_QuestTermina_Walk(old, 3);
        }
        if (next == TQ_CURSOR_TO_LEFT) {
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
            next = TQ_CURSOR_NONE;
        } else if (next == TQ_CURSOR_TO_RIGHT) {
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
            next = TQ_CURSOR_NONE;
        } else if (next < 0) {
            if (pauseCtx->stickRelY < -30) {
                next = MergeShip_QuestTermina_Walk(old, 1);
            } else if (pauseCtx->stickRelY > 30) {
                next = MergeShip_QuestTermina_Walk(old, 0);
            }
            if (next < 0) {
                next = TQ_CURSOR_NONE;
            }
        }
        if (next >= 0) {
            sTerminaQuestCursor = next;
            Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    }
    pauseCtx->cursorItem[PAUSE_QUEST] =
        (pauseCtx->cursorSpecialPos == 0) ? MergeShip_QuestTermina_PointItem(sTerminaQuestCursor) : PAUSE_ITEM_NONE;
}

// Ocarina of Time's four-corner cursor around a quad of this page (KaleidoScope_UpdateCursorSize's layout).
static void MergeShip_QuestTermina_SetCursorBox(PauseContext* pauseCtx, Vtx* quad, s16 in) {
    s16 x = quad[0].v.ob[0] - in;
    s16 y = quad[0].v.ob[1] + in;
    s16 w = (quad[1].v.ob[0] - quad[0].v.ob[0]) + 2 * in;
    s16 h = (quad[0].v.ob[1] - quad[2].v.ob[1]) + 2 * in;
    s16 corners[4][2] = { { x, y }, { x + w - 16, y }, { x, y - h + 16 }, { x + w - 16, y - h + 16 } };
    s32 c;

    for (c = 0; c < 4; c++) {
        Vtx* v = &pauseCtx->cursorVtx[c * 4];

        v[0].v.ob[0] = v[2].v.ob[0] = corners[c][0];
        v[1].v.ob[0] = v[3].v.ob[0] = corners[c][0] + 16;
        v[0].v.ob[1] = v[1].v.ob[1] = corners[c][1];
        v[2].v.ob[1] = v[3].v.ob[1] = corners[c][1] - 16;
    }
}

// MM's quiver and bomb bag icons by upgrade (MM sQuestUpgradeTextures): Ocarina of Time's shared capacities.
static const char* sTerminaQuiverIcons[3] = { mm_gItemIconQuiver30Tex, mm_gItemIconQuiver40Tex,
                                              mm_gItemIconQuiver50Tex };
static const char* sTerminaBombBagIcons[3] = { mm_gItemIconBombBag20Tex, mm_gItemIconBombBag30Tex,
                                               mm_gItemIconBombBag40Tex };

// The points of MM's quest page that show equipment.
static const s16 sTerminaGearPoints[] = { TQ_SHIELD, TQ_SWORD, TQ_QUIVER, TQ_BOMB_BAG };

// The icon MM's quest page shows at one of its equipment points (NULL: none).
static const char* MergeShip_QuestTermina_GearIcon(s16 point) {
    s32 value;
    u16 item;

    switch (point) {
        case TQ_SWORD:
            // MM: the sword on B. Here the equipped sword slot's item (its MM variant when switched to one).
            value = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
            if (value == EQUIP_VALUE_SWORD_NONE) {
                return NULL;
            }
            // (Ocarina of Time's Kokiri Sword shows its own icon, MM's its own: ITEM_MM_SWORD_KOKIRI, 2026-09-24.)
            item = MergeShip_MMEquip_PauseSlotItem(EQUIP_TYPE_SWORD, value, ITEM_SWORD_KOKIRI + value - 1);
            if ((item == ITEM_SWORD_BGS) && (gSaveContext.bgsFlag == 0) &&
                CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
                return gItemIconBrokenGiantsKnifeTex;
            }
            return (const char*)gItemIcons[item];
        case TQ_SHIELD:
            value = CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
            if (value == EQUIP_VALUE_SHIELD_NONE) {
                return NULL;
            }
            return (const char*)
                gItemIcons[MergeShip_MMEquip_PauseSlotItem(EQUIP_TYPE_SHIELD, value, ITEM_SHIELD_DEKU + value - 1)];
        // Majora's Mask's own quiver and bomb bag (MergeShipOwnership.h), never Ocarina of Time's.
        case TQ_QUIVER:
            value = MergeShip_Own_MMUpgrade(UPG_QUIVER);
            return (value != 0) ? sTerminaQuiverIcons[CLAMP_MAX(value, 3) - 1] : NULL;
        case TQ_BOMB_BAG:
            value = MergeShip_Own_MMUpgrade(UPG_BOMB_BAG);
            return (value != 0) ? sTerminaBombBagIcons[CLAMP_MAX(value, 3) - 1] : NULL;
        default:
            return NULL;
    }
}

void MergeShip_QuestTermina_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    Vtx* vtx = Graph_Alloc(gfxCtx, (TQ_MAX + 8) * 4 * sizeof(Vtx));
    OcarinaSongButtons* buttons;
    u8 song;
    s16 i;
    s16 k;

    // MM KaleidoScope_SetVertices, questVtx: points up to QUEST_SWORD as they are, the rest inset by 2.
    for (i = 0; i < TQ_MAX; i++) {
        if (i <= TQ_SWORD) {
            MergeShip_Kaleido_SetQuad(&vtx[i * 4], sTerminaQuestLeft[i], sTerminaQuestTop[i] + pauseCtx->offsetY,
                                      sTerminaQuestWidth[i], sTerminaQuestHeight[i], sTerminaQuestWidth[i],
                                      sTerminaQuestHeight[i], pauseCtx->alpha);
        } else {
            MergeShip_Kaleido_SetQuad(&vtx[i * 4], sTerminaQuestLeft[i] + 2, sTerminaQuestTop[i] + pauseCtx->offsetY - 2,
                                      sTerminaQuestWidth[i] - 4, sTerminaQuestHeight[i] - 4, sTerminaQuestWidth[i],
                                      sTerminaQuestHeight[i], pauseCtx->alpha);
        }
    }
    for (i = 0; i < 8; i++) {
        MergeShip_Kaleido_SetQuad(&vtx[(TQ_MAX + i) * 4], sTerminaSongButtonLeft[i] + 2, -52, 12, 12, 16, 16,
                                  pauseCtx->alpha);
    }

    // Input (Ocarina of Time's quest page reads it while drawing too).
    if ((pauseCtx->state == 6) && (pauseCtx->pageIndex == PAUSE_QUEST)) {
        if (pauseCtx->unk_1E4 == 0) {
            MergeShip_QuestTermina_UpdateCursor(play);
            song = MergeShip_QuestTermina_PointSong(sTerminaQuestCursor);
            // MM PAUSE_MAIN_STATE_SONG_PLAYBACK: A plays the song back, its buttons appearing as they sound. (MM then
            // lets the player play it along; see the report.)
            if ((pauseCtx->cursorSpecialPos == 0) && (song != 0xFF) && CHECK_BTN_ALL(input->press.button, BTN_A)) {
                for (k = 0; k < 8; k++) {
                    sTerminaPlayedButtons[k] = OCARINA_BTN_INVALID;
                    sTerminaPlayedButtonsAlpha[k] = 0;
                }
                sTerminaPlayedButtonsNum = 0;
                AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
                AudioOcarina_SetPlaybackSong(song + 1, 1);
                pauseCtx->ocarinaSongIdx = song;
                pauseCtx->ocarinaStaff = AudioOcarina_GetPlaybackStaff();
                pauseCtx->ocarinaStaff->pos = 0;
                pauseCtx->unk_1E4 = 2; // KaleidoScope_Update: waits for the playback, then 4
            }
        } else if (pauseCtx->unk_1E4 == 4) {
            pauseCtx->unk_1E4 = 0;
        }
    }

    OPEN_DISPS(gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);

    // MM: the remains, their glow pulsing (sQuestRemainsEnvTargets, sQuestRemainsColorTimerInit).
    sTerminaRemainsTimer--;
    for (i = 0; i < 4; i++) {
        if ((sTerminaRemainsTimerIndex != 1) && (sTerminaRemainsTimerIndex != 3)) {
            s16* target = sTerminaRemainsEnvTargets[(sTerminaRemainsTimerIndex != 0) ? 1 : 0][i];

            for (k = 0; k < 3; k++) {
                if (sTerminaRemainsTimer != 0) {
                    s16 step = ABS(sTerminaRemainsEnv[i][k] - target[k]) / sTerminaRemainsTimer;
                    sTerminaRemainsEnv[i][k] += (sTerminaRemainsEnv[i][k] >= target[k]) ? -step : step;
                } else {
                    sTerminaRemainsEnv[i][k] = target[k];
                }
            }
        }
        if (MergeShip_Songs_HasItem(ITEM_MM_REMAINS_ODOLWA + i)) {
            gDPPipeSync(POLY_OPA_DISP++);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
            gDPSetEnvColor(POLY_OPA_DISP++, sTerminaRemainsEnv[i][0], sTerminaRemainsEnv[i][1],
                           sTerminaRemainsEnv[i][2], 0);
            gSPVertex(POLY_OPA_DISP++, &vtx[(TQ_REMAINS_ODOLWA + i) * 4], 4, 0);
            KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, gItemIcons[ITEM_MM_REMAINS_ODOLWA + i], 32, 32, 0);
        }
    }
    if (sTerminaRemainsTimer == 0) {
        sTerminaRemainsTimer = sTerminaRemainsTimerInit[sTerminaRemainsTimerIndex];
        if (++sTerminaRemainsTimerIndex > 3) {
            sTerminaRemainsTimerIndex = 0;
        }
    }

    // MM: the songs (gItemIconSongNoteTex, IA8 16 x 24, MM's colours; the one under the cursor drawn larger).
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    gDPLoadTextureBlock(POLY_OPA_DISP++, mm_gItemIconSongNoteTex, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 24, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    for (i = TQ_SONG_SONATA; i <= TQ_SONG_SUN; i++) {
        Vtx* q = &vtx[i * 4];

        if (!MergeShip_QuestTermina_IsPoint(i) || (MergeShip_QuestTermina_PointSong(i) == 0xFF)) {
            continue;
        }
        if ((i == sTerminaQuestCursor) && (pauseCtx->cursorSpecialPos == 0)) {
            q[0].v.ob[0] = q[2].v.ob[0] = q[0].v.ob[0] - 2;
            q[1].v.ob[0] = q[3].v.ob[0] = q[1].v.ob[0] + 4;
            q[0].v.ob[1] = q[1].v.ob[1] = q[0].v.ob[1] + 2;
            q[2].v.ob[1] = q[3].v.ob[1] = q[2].v.ob[1] - 4;
        }
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, sTerminaSongPrim[i - TQ_SONG_SONATA][0],
                        sTerminaSongPrim[i - TQ_SONG_SONATA][1], sTerminaSongPrim[i - TQ_SONG_SONATA][2],
                        pauseCtx->alpha);
        gSPVertex(POLY_OPA_DISP++, q, 4, 0);
        gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
    }

    // MM: the Bomber's Notebook.
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    if (MergeShip_Songs_HasItem(ITEM_MM_BOMBERS_NOTEBOOK)) {
        gSPVertex(POLY_OPA_DISP++, &vtx[TQ_BOMBERS_NOTEBOOK * 4], 4, 0);
        KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, mm_gItemIconBombersNotebookTex, 32, 32, 0);
    }

    // MM: the shield and the sword in use, the quiver and the bomb bag, in the boxes MM's page has for them (shown
    // only: the Equipment face equips them, in Ocarina of Time's view or Termina's).
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    for (k = 0; k < (s16)ARRAY_COUNT(sTerminaGearPoints); k++) {
        const char* icon = MergeShip_QuestTermina_GearIcon(sTerminaGearPoints[k]);

        if (icon != NULL) {
            gSPVertex(POLY_OPA_DISP++, &vtx[sTerminaGearPoints[k] * 4], 4, 0);
            KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, (void*)icon, 32, 32, 0);
        }
    }

    // MM: the song's buttons under the page -- all of them while the cursor rests on a learned song
    // (PAUSE_MAIN_STATE_IDLE_CURSOR_ON_SONG), or one by one as the playback sounds them (_SONG_PLAYBACK).
    if ((pauseCtx->state == 6) && (pauseCtx->pageIndex == PAUSE_QUEST) && (pauseCtx->cursorSpecialPos == 0)) {
        static void* sButtonTextures[] = {
            gOcarinaBtnIconATex,     gOcarinaBtnIconCDownTex, gOcarinaBtnIconCRightTex,
            gOcarinaBtnIconCLeftTex, gOcarinaBtnIconCUpTex,
        };

        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

        if (pauseCtx->unk_1E4 == 2) {
            pauseCtx->ocarinaStaff = AudioOcarina_GetPlaybackStaff();
            if ((pauseCtx->ocarinaStaff->pos != 0) && (pauseCtx->ocarinaStaff->pos <= 8) &&
                (sTerminaPlayedButtonsNum == (pauseCtx->ocarinaStaff->pos - 1))) {
                sTerminaPlayedButtonsNum++;
                sTerminaPlayedButtons[pauseCtx->ocarinaStaff->pos - 1] = pauseCtx->ocarinaStaff->buttonIndex;
            }
            for (i = 0; i < 8; i++) {
                Vtx* q = &vtx[(TQ_MAX + i) * 4];
                u8 button = sTerminaPlayedButtons[i];

                if (button > OCARINA_BTN_C_UP) {
                    break;
                }
                sTerminaPlayedButtonsAlpha[i] = CLAMP_MAX(sTerminaPlayedButtonsAlpha[i] + 50, 255);
                q[0].v.ob[1] = q[1].v.ob[1] = sTerminaSongButtonY[button];
                q[2].v.ob[1] = q[3].v.ob[1] = q[0].v.ob[1] - 12;
                gDPPipeSync(POLY_OPA_DISP++);
                if (button == OCARINA_BTN_A) {
                    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 80, 150, 255, sTerminaPlayedButtonsAlpha[i]);
                } else {
                    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 50, sTerminaPlayedButtonsAlpha[i]);
                }
                gSPVertex(POLY_OPA_DISP++, q, 4, 0);
                POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, sButtonTextures[button], 16, 16, 0);
            }
        } else if ((pauseCtx->unk_1E4 == 0) &&
                   ((song = MergeShip_QuestTermina_PointSong(sTerminaQuestCursor)) != 0xFF) &&
                   ((buttons = MergeShip_Ocarina_GetSongButtons(song)) != NULL)) {
            for (i = 0; (i < buttons->numButtons) && (i < 8); i++) {
                Vtx* q = &vtx[(TQ_MAX + i) * 4];
                u8 button = buttons->buttonsIndex[i];

                q[0].v.ob[1] = q[1].v.ob[1] = sTerminaSongButtonY[button];
                q[2].v.ob[1] = q[3].v.ob[1] = q[0].v.ob[1] - 12;
                gDPPipeSync(POLY_OPA_DISP++);
                if (button == OCARINA_BTN_A) {
                    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 80, 150, 255, 200);
                } else {
                    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 50, 200);
                }
                gSPVertex(POLY_OPA_DISP++, q, 4, 0);
                POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, sButtonTextures[button], 16, 16, 0);
            }
        }
    }

    CLOSE_DISPS(gfxCtx);

    // The cursor (KaleidoScope_DrawPageFace draws it after this, around the quad set here).
    KaleidoScope_SetCursorVtx(pauseCtx, sTerminaQuestCursor * 4, vtx);
    MergeShip_QuestTermina_SetCursorBox(pauseCtx, &vtx[sTerminaQuestCursor * 4],
                                        (sTerminaQuestCursor <= TQ_SWORD) || (sTerminaQuestCursor == TQ_BOMBERS_NOTEBOOK)
                                            ? 3
                                            : 5);
}

// ==== The Termina equipment view ================================================================================
//
// Majora's Mask has no equipment page: its sword, shield, quiver and bomb bag sit on its quest page. C-Up on the
// Equipment face switches Ocarina of Time's equipment page to this view of MM's equipment, laid out on Ocarina of
// Time's equipment grid (KaleidoScope_InitVertices' equipVtx: a column left of Link's picture, three right of it) with
// MM's icons, MM's names and MM's page colour:
//   left column:  the Quiver and the Bomb Bag (MM's icons of the shared capacities; shown, not equipped);
//   first row:    the Kokiri Sword, the Razor Sword, the Gilded Sword;
//   second row:   the Hero's Shield, the Mirror Shield;
//   third row:    the Great Fairy's Sword.
// Only what the file owns is shown. A equips the item under the cursor: its slot switches to it
// (MergeShip_MMEquip_TerminaEquip) and the slot is equipped, a sword on B. This view is where MM's swords and shields
// are chosen; Ocarina of Time's own page shows and equips only Ocarina of Time's (A there switches a slot back from its
// MM variant). Each view outlines only the item actually in use, so the two agree. Link's picture is Ocarina of Time's
// pause Link, who wears the selected variants (MergeShip_MMEquip_OverrideLimbPause).

#define TE_COLS 4
#define TE_ROWS 4
#define TE_CELLS (TE_COLS * TE_ROWS)
#define TE_CELL(row, col) ((row) * TE_COLS + (col))
#define TE_ROW_Y(row) (58 - (row)*32) // OoT KaleidoScope_InitVertices, equipVtx rows

static const s16 sTerminaEquipColX[TE_COLS] = { -114, 12, 44, 76 }; // OoT D_8082B12C

// The equipment each cell holds (ITEM_NONE: an empty cell; the left column's are the upgrades, by their first item).
static const u16 sTerminaEquipCells[TE_CELLS] = {
    ITEM_QUIVER_30,   ITEM_MM_SWORD_KOKIRI,      ITEM_MM_SWORD_RAZOR,   ITEM_MM_SWORD_GILDED,
    ITEM_BOMB_BAG_20, ITEM_MM_SHIELD_HERO,       ITEM_MM_SHIELD_MIRROR, ITEM_NONE,
    ITEM_NONE,        ITEM_MM_SWORD_GREAT_FAIRY, ITEM_NONE,             ITEM_NONE,
    ITEM_NONE,        ITEM_NONE,                 ITEM_NONE,             ITEM_NONE,
};

static s32 sTerminaEquipShown = false;
static s16 sTerminaEquipCursor = TE_CELL(0, 1);

static s32 MergeShip_EquipTermina_Available(void) {
    return MergeShip_MMEquip_IsEnabled();
}

s32 MergeShip_EquipTermina_IsShown(PlayState* play) {
    (void)play;
    if (!MergeShip_EquipTermina_Available()) {
        sTerminaEquipShown = false;
    }
    return sTerminaEquipShown;
}

// The item a cell shows now (ITEM_NONE: nothing): the upgrades by their capacity, the swords and shields when owned.
static u16 MergeShip_EquipTermina_CellItem(s16 cell) {
    u16 item = sTerminaEquipCells[cell];
    s32 upg;

    switch (item) {
        case ITEM_NONE:
            return ITEM_NONE;
        // Majora's Mask's own quiver and bomb bag (MergeShipOwnership.h); Ocarina of Time's are on its own page.
        case ITEM_QUIVER_30:
        case ITEM_BOMB_BAG_20:
            upg = MergeShip_Own_MMUpgrade((item == ITEM_QUIVER_30) ? UPG_QUIVER : UPG_BOMB_BAG);
            return (upg > 0) ? (u16)(item + upg - 1) : ITEM_NONE;
        default:
            return MergeShip_MMEquip_TerminaOwns(item) ? item : ITEM_NONE;
    }
}

// A cell holds equipment A equips (not an upgrade).
static s32 MergeShip_EquipTermina_IsEquipCell(s16 cell) {
    return (cell % TE_COLS) != 0;
}

// MM's icon for a cell's item (icon_item_static_yar, 32x32 RGBA32).
static const char* MergeShip_EquipTermina_Icon(u16 item) {
    switch (item) {
        case ITEM_QUIVER_30:
        case ITEM_QUIVER_40:
        case ITEM_QUIVER_50:
            return sTerminaQuiverIcons[item - ITEM_QUIVER_30];
        case ITEM_BOMB_BAG_20:
        case ITEM_BOMB_BAG_30:
        case ITEM_BOMB_BAG_40:
            return sTerminaBombBagIcons[item - ITEM_BOMB_BAG_20];
        case ITEM_MM_SWORD_KOKIRI:
            return mm_gItemIconKokiriSwordTex;
        case ITEM_MM_SWORD_RAZOR:
            return mm_gItemIconRazorSwordTex;
        case ITEM_MM_SWORD_GILDED:
            return mm_gItemIconGildedSwordTex;
        case ITEM_MM_SWORD_GREAT_FAIRY:
            return mm_gItemIconGreatFairysSwordTex;
        case ITEM_MM_SHIELD_HERO:
            return mm_gItemIconHerosShieldTex;
        case ITEM_MM_SHIELD_MIRROR:
            return mm_gItemIconMirrorShieldTex;
        default:
            return NULL;
    }
}

// MM's name for a cell's item (item_name_static, IA4 128x16, English).
static const char* MergeShip_EquipTermina_Name(u16 item) {
    static const char* sQuiverNames[3] = { mm_gItemNameQuiver30ENGTex, mm_gItemNameQuiver40ENGTex,
                                           mm_gItemNameQuiver50ENGTex };
    static const char* sBombBagNames[3] = { mm_gItemNameBombBag20ENGTex, mm_gItemNameBombBag30ENGTex,
                                            mm_gItemNameBombBag40ENGTex };

    switch (item) {
        case ITEM_QUIVER_30:
        case ITEM_QUIVER_40:
        case ITEM_QUIVER_50:
            return sQuiverNames[item - ITEM_QUIVER_30];
        case ITEM_BOMB_BAG_20:
        case ITEM_BOMB_BAG_30:
        case ITEM_BOMB_BAG_40:
            return sBombBagNames[item - ITEM_BOMB_BAG_20];
        case ITEM_MM_SWORD_KOKIRI:
            return mm_gItemNameKokiriSwordENGTex;
        case ITEM_MM_SWORD_RAZOR:
            return mm_gItemNameRazorSwordENGTex;
        case ITEM_MM_SWORD_GILDED:
            return mm_gItemNameGildedSwordENGTex;
        case ITEM_MM_SWORD_GREAT_FAIRY:
            return mm_gItemNameGreatFairysSwordENGTex;
        case ITEM_MM_SHIELD_HERO:
            return mm_gItemNameHerosShieldENGTex;
        case ITEM_MM_SHIELD_MIRROR:
            return mm_gItemNameMirrorShieldENGTex;
        default:
            return NULL;
    }
}

// The item can be used now (an upgrade always; a sword or shield by this age and form).
static s32 MergeShip_EquipTermina_Usable(s16 cell, u16 item) {
    return !MergeShip_EquipTermina_IsEquipCell(cell) || MergeShip_MMEquip_TerminaCanEquip(item);
}

// The first cell with something in it, scanning the columns from `fromCol` in the direction `step` (each from the top),
// or -1 (Ocarina of Time's equipment page, leaving a page arrow).
static s16 MergeShip_EquipTermina_ScanColumns(s16 fromCol, s16 step) {
    s16 col;
    s16 row;

    for (col = fromCol; (col >= 0) && (col < TE_COLS); col += step) {
        for (row = 0; row < TE_ROWS; row++) {
            if (MergeShip_EquipTermina_CellItem(TE_CELL(row, col)) != ITEM_NONE) {
                return TE_CELL(row, col);
            }
        }
    }
    return -1;
}

// The cell with something in it of row `row` nearest to column `col`, or -1.
static s16 MergeShip_EquipTermina_NearestInRow(s16 row, s16 col) {
    s16 d;

    for (d = 0; d < TE_COLS; d++) {
        if ((col - d >= 0) && (MergeShip_EquipTermina_CellItem(TE_CELL(row, col - d)) != ITEM_NONE)) {
            return TE_CELL(row, col - d);
        }
        if ((col + d < TE_COLS) && (MergeShip_EquipTermina_CellItem(TE_CELL(row, col + d)) != ITEM_NONE)) {
            return TE_CELL(row, col + d);
        }
    }
    return -1;
}

// Where the cursor lands when the view is switched to: the equipped sword, else the first sword or shield owned.
static void MergeShip_EquipTermina_PlaceCursor(void) {
    s16 cell;

    for (cell = 0; cell < TE_CELLS; cell++) {
        u16 item = MergeShip_EquipTermina_CellItem(cell);

        if (MergeShip_EquipTermina_IsEquipCell(cell) && (item != ITEM_NONE) &&
            MERGESHIP_ITEM_IS_MM_SWORD(item) &&
            MergeShip_MMEquip_TerminaEquipped(item)) {
            sTerminaEquipCursor = cell;
            return;
        }
    }
    cell = MergeShip_EquipTermina_ScanColumns(1, 1);
    if (cell < 0) {
        cell = MergeShip_EquipTermina_ScanColumns(0, 1);
    }
    if (cell >= 0) {
        sTerminaEquipCursor = cell;
    }
}

// The cursor and A (Ocarina of Time's KaleidoScope_DrawEquipment input, on this view's cells).
static void MergeShip_EquipTermina_UpdateCursor(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s32 dpad = CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) && !CHECK_BTN_ALL(input->cur.button, BTN_CUP);
    s32 left = (pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT));
    s32 right = (pauseCtx->stickRelX > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT));
    s32 up = (pauseCtx->stickRelY > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DUP));
    s32 down = (pauseCtx->stickRelY < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DDOWN));
    s16 row = sTerminaEquipCursor / TE_COLS;
    s16 col = sTerminaEquipCursor % TE_COLS;
    s16 next = -1;
    s16 i;
    u16 item;

    if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
        if (right) {
            pauseCtx->cursorSpecialPos = 0;
            pauseCtx->nameDisplayTimer = 0;
            next = MergeShip_EquipTermina_ScanColumns(0, 1);
            if (next < 0) {
                KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
            }
        }
    } else if (pauseCtx->cursorSpecialPos != 0) {
        if (left) {
            pauseCtx->cursorSpecialPos = 0;
            pauseCtx->nameDisplayTimer = 0;
            next = MergeShip_EquipTermina_ScanColumns(TE_COLS - 1, -1);
            if (next < 0) {
                KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
            }
        }
    } else if (left || right) {
        s16 step = left ? -1 : 1;

        for (i = col + step; (i >= 0) && (i < TE_COLS); i += step) {
            if (MergeShip_EquipTermina_CellItem(TE_CELL(row, i)) != ITEM_NONE) {
                next = TE_CELL(row, i);
                break;
            }
        }
        if (next < 0) {
            KaleidoScope_MoveCursorToSpecialPos(play, left ? PAUSE_CURSOR_PAGE_LEFT : PAUSE_CURSOR_PAGE_RIGHT);
        }
    } else if (up || down) {
        s16 step = up ? -1 : 1;

        for (i = row + step; (i >= 0) && (i < TE_ROWS); i += step) {
            next = MergeShip_EquipTermina_NearestInRow(i, col);
            if (next >= 0) {
                break;
            }
        }
    }
    if (next >= 0) {
        sTerminaEquipCursor = next;
        Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }

    item = (pauseCtx->cursorSpecialPos == 0) ? MergeShip_EquipTermina_CellItem(sTerminaEquipCursor) : ITEM_NONE;

    // A equips a sword or shield (MergeShip_MMEquip_TerminaEquip); Ocarina of Time's error sound when it can't.
    if ((item != ITEM_NONE) && MergeShip_EquipTermina_IsEquipCell(sTerminaEquipCursor) &&
        CHECK_BTN_ALL(input->press.button, BTN_A)) {
        if (MergeShip_MMEquip_TerminaEquip(play, item)) {
            Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            MergeShipTest_Log("TERMINAEQUIP A cell=%d item=0x%02X sword=%d shield=%d B=0x%02X", sTerminaEquipCursor,
                              item, CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD), CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD),
                              gSaveContext.equips.buttonItems[0]);
        } else {
            Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    }

    // Ocarina of Time's equipment page: the cursor blue on equipment A equips, the name grey on what can't be used.
    pauseCtx->cursorItem[PAUSE_EQUIP] = (item != ITEM_NONE) ? item : PAUSE_ITEM_NONE;
    pauseCtx->cursorColorSet =
        ((item != ITEM_NONE) && MergeShip_EquipTermina_IsEquipCell(sTerminaEquipCursor)) ? 8 : 0;
    pauseCtx->nameColorSet =
        ((item != ITEM_NONE) && !MergeShip_EquipTermina_Usable(sTerminaEquipCursor, item)) ? 1 : 0;
}

void MergeShip_EquipTermina_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    PauseContext* pauseCtx = &play->pauseCtx;
    Vtx* vtx = Graph_Alloc(gfxCtx, TE_CELLS * 2 * 4 * sizeof(Vtx)); // the items' quads, then the outlines'
    s32 isCurrent = (pauseCtx->pageIndex == PAUSE_EQUIP);
    s16 cell;

    if (isCurrent && (pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0)) {
        MergeShip_EquipTermina_UpdateCursor(play);
    }

    // OoT KaleidoScope_InitVertices: the items 28 x 28 inside their 32 x 32 cell, the "equipped" outline the cell.
    for (cell = 0; cell < TE_CELLS; cell++) {
        s16 x = sTerminaEquipColX[cell % TE_COLS];
        s16 y = TE_ROW_Y(cell / TE_COLS) + pauseCtx->offsetY;

        MergeShip_Kaleido_SetQuad(&vtx[cell * 4], x + 2, y - 2, 28, 28, 32, 32, pauseCtx->alpha);
        MergeShip_Kaleido_SetQuad(&vtx[(TE_CELLS + cell) * 4], x, y, 32, 32, 32, 32, pauseCtx->alpha);
    }
    if (isCurrent) {
        // KaleidoScope_DrawPageFace draws the cursor after this, around the quad set here.
        KaleidoScope_SetCursorVtx(pauseCtx, sTerminaEquipCursor * 4, vtx);
    }

    OPEN_DISPS(gfxCtx);

    Gfx_SetupDL_42Opa(gfxCtx);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    // OoT KaleidoScope_DrawEquipment: the outline behind the equipped sword and shield.
    for (cell = 0; cell < TE_CELLS; cell++) {
        u16 item = MergeShip_EquipTermina_CellItem(cell);

        if ((item != ITEM_NONE) && MergeShip_EquipTermina_IsEquipCell(cell) &&
            MergeShip_MMEquip_TerminaEquipped(item)) {
            gSPVertex(POLY_OPA_DISP++, &vtx[(TE_CELLS + cell) * 4], 4, 0);
            POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, gEquippedItemOutlineTex, 32, 32, 0);
        }
    }

    // The items: the one under the cursor drawn larger, what this age or form can't use grey (as OoT's page).
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    for (cell = 0; cell < TE_CELLS; cell++) {
        u16 item = MergeShip_EquipTermina_CellItem(cell);
        s32 usable;

        if (item == ITEM_NONE) {
            continue;
        }
        usable = MergeShip_EquipTermina_Usable(cell, item);
        if (isCurrent && usable && (cell == sTerminaEquipCursor) && (pauseCtx->cursorSpecialPos == 0)) {
            MergeShip_Kaleido_SetQuad(&vtx[cell * 4], vtx[cell * 4].v.ob[0] - 2, vtx[cell * 4].v.ob[1] + 2, 32, 32, 32,
                                      32, pauseCtx->alpha);
        }
        if (!usable) {
            gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
            gSPGrayscale(POLY_OPA_DISP++, true);
        }
        gSPVertex(POLY_OPA_DISP++, &vtx[cell * 4], 4, 0);
        KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, (void*)MergeShip_EquipTermina_Icon(item), 32, 32, 0);
        if (!usable) {
            gSPGrayscale(POLY_OPA_DISP++, false);
        }
    }

    // What KaleidoScope_DrawEquipment does every frame for every face (the Equipment face is drawn every frame): the
    // name panel's texture is reloaded (nameSegment keeps its address while KaleidoScope_UpdateNamePanel rewrites it),
    // and the pause segments are set.
    gSPInvalidateTexCache(POLY_OPA_DISP++, pauseCtx->iconItemSegment);
    gSPInvalidateTexCache(POLY_OPA_DISP++, pauseCtx->nameSegment);
    gSPSegment(POLY_OPA_DISP++, 0x08, pauseCtx->iconItemSegment);
    gSPSegment(POLY_OPA_DISP++, 0x09, pauseCtx->iconItem24Segment);
    gSPSegment(POLY_OPA_DISP++, 0x0A, pauseCtx->nameSegment);
    gSPSegment(POLY_OPA_DISP++, 0x0B, play->interfaceCtx.mapSegment);

    CLOSE_DISPS(gfxCtx);

    // Link's picture (OoT KaleidoScope_DrawEquipment).
    KaleidoScope_DrawPlayerWork(play);
    Gfx_SetupDL_42Opa(gfxCtx);
    KaleidoScope_DrawEquipmentImage(play, pauseCtx->playerSegment, PAUSE_EQUIP_PLAYER_WIDTH, PAUSE_EQUIP_PLAYER_HEIGHT);
}

// KaleidoScope_UpdateNamePanel for the view: MM's names (as the Items (Termina) page's panel).
void MergeShip_EquipTermina_UpdateNamePanel(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    u16 item = (pauseCtx->cursorSpecialPos == 0) ? MergeShip_EquipTermina_CellItem(sTerminaEquipCursor) : ITEM_NONE;
    u16 namedItem = (item != ITEM_NONE) ? item : PAUSE_ITEM_NONE;

    if (pauseCtx->namedItem != namedItem) {
        const char* texture = (namedItem != PAUSE_ITEM_NONE) ? MergeShip_EquipTermina_Name(item) : NULL;

        pauseCtx->namedItem = namedItem;
        if (texture != NULL) {
            memcpy(pauseCtx->nameSegment, texture, strlen(texture) + 1);
        } else {
            pauseCtx->namedItem = PAUSE_ITEM_NONE;
        }
        pauseCtx->nameDisplayTimer = 0;
    } else if (pauseCtx->nameColorSet == 0) {
        pauseCtx->nameDisplayTimer++;
        if (pauseCtx->nameDisplayTimer > WREG(88)) {
            pauseCtx->nameDisplayTimer = 0;
        }
    } else {
        pauseCtx->nameDisplayTimer = 0;
    }
}

// ==== C-Up: Hyrule <-> Termina =================================================================================
//
// C-Up switches the page in view between Ocarina of Time's screen and Majora's Mask's, and back:
//   Select Item:   Ocarina of Time's <-> Majora's Mask's (z_kaleido_mm_item.c; MergeShip_Pause_SetMMItemView puts it
//                  in the Item face's place on the ring);
//   Quest Status:  Ocarina of Time's <-> Termina's (MM's quest status, above);
//   Equipment:     Ocarina of Time's <-> Termina's (MM's equipment, above).
// The Masks face stays a page of its own; the Map face has no Termina screen here. Each page's option gates it
// (MergeShip.MMItems, MergeShip.MMSongs + .QuestPage, MergeShip.MMEquipment). A label under the page, right of the
// name panel, shows the button and where it goes ("[C-Up] Termina" / "[C-Up] Hyrule"), in the message font.
// On the item pages C-Up also equips the item under the cursor, and on Ocarina of Time's Equipment page with SoH's
// Assignable Tunics and Boots it assigns the shield / tunic / boots under it: there equipping wins -- C-Up switches
// from an empty slot or a page arrow -- and the label is drawn grey while C-Up would equip. The views reset to
// Ocarina of Time's when the pause screen closes (MergeShip_Kaleido_ResetWorldViews).

typedef enum {
    MS_WORLD_SWITCH_NONE,   // the page has no other view
    MS_WORLD_SWITCH_SWITCH, // C-Up switches it
    MS_WORLD_SWITCH_EQUIPS, // C-Up equips the item under the cursor here instead
} MergeShipWorldSwitchMode;

static s32 sWorldSwitchFrame = -1;

// BUG-0017: the Map face's Termina view -- Majora's Mask's world map with its owl statues (the owl-warp map, drawn by
// MergeShip_OwlWarp_DrawWorldMap), to look at: the cursor steps between the statues Link has struck and the name panel
// names them; only the Song of Soaring warps (MergeShipTermina.c).
static s32 sTerminaMapShown = false;

static s32 MergeShip_TerminaMap_Available(void) {
    return MergeShip_Songs_Enabled();
}

s32 MergeShip_TerminaMap_IsShown(PlayState* play) {
    return MergeShip_TerminaMap_Available() && sTerminaMapShown && !MergeShip_OwlWarp_IsActive();
}

// SoH's Assignable Tunics and Boots: on Ocarina of Time's Equipment page C-Up puts the shield / tunic / boots under
// the cursor on the button (z_kaleido_equipment.c).
static s32 MergeShip_Kaleido_OoTEquipCDownAssigns(PauseContext* pauseCtx) {
    return CVarGetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"), 0) && (pauseCtx->cursorSpecialPos == 0) &&
           (pauseCtx->cursorX[PAUSE_EQUIP] != 0) && (pauseCtx->cursorY[PAUSE_EQUIP] > 0) &&
           CHECK_OWNED_EQUIP(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP] - 1);
}

// What C-Up does on the page in view, and the label naming where it goes.
static s32 MergeShip_Kaleido_WorldSwitchMode(PlayState* play, const char** label) {
    PauseContext* pauseCtx = &play->pauseCtx;

    // Singleplayer: Termina's pages open once Ocarina of Time is beaten.
    if (MergeShip_Singleplayer_TerminaLocked()) {
        return MS_WORLD_SWITCH_NONE;
    }
    switch (pauseCtx->pageIndex) {
        case PAUSE_ITEM:
            if (!MergeShip_MMItems_Enabled()) {
                return MS_WORLD_SWITCH_NONE;
            }
            *label = "Termina";
            return MS_WORLD_SWITCH_SWITCH; // C-Up equips nothing on the item pages
        case MERGESHIP_PAUSE_MM_ITEM:
            if (!MergeShip_Pause_IsMMItemViewShown()) {
                return MS_WORLD_SWITCH_NONE;
            }
            *label = "Hyrule";
            return MS_WORLD_SWITCH_SWITCH;
        case PAUSE_QUEST:
            if (!MergeShip_QuestTermina_Available(play)) {
                return MS_WORLD_SWITCH_NONE;
            }
            *label = sTerminaQuestShown ? "Hyrule" : "Termina";
            return MS_WORLD_SWITCH_SWITCH;
        case PAUSE_EQUIP:
            if (!MergeShip_EquipTermina_Available()) {
                return MS_WORLD_SWITCH_NONE;
            }
            *label = sTerminaEquipShown ? "Hyrule" : "Termina";
            return MS_WORLD_SWITCH_SWITCH;
        case PAUSE_MAP:
            // BUG-0017: the world map (not a dungeon's map, nor the Song of Soaring's owl-warp map).
            if (!MergeShip_TerminaMap_Available() || MergeShip_OwlWarp_IsActive() ||
                ((play->sceneNum >= SCENE_DEKU_TREE) && (play->sceneNum <= SCENE_TREASURE_BOX_SHOP))) {
                return MS_WORLD_SWITCH_NONE;
            }
            *label = sTerminaMapShown ? "Hyrule" : "Termina";
            return MS_WORLD_SWITCH_SWITCH;
        default:
            return MS_WORLD_SWITCH_NONE;
    }
}

void MergeShip_Kaleido_UpdateWorldSwitch(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    const char* label = NULL;
    s32 toTermina;

    // The page in view is one on the ring (the Item face's Majora's Mask view went away with its option).
    if ((pauseCtx->unk_1E4 == 0) && (pauseCtx->pageIndex == MERGESHIP_PAUSE_MM_ITEM) &&
        !MergeShip_Pause_IsMMItemViewShown()) {
        pauseCtx->pageIndex = PAUSE_ITEM;
    }
    if (!MergeShip_EquipTermina_Available()) {
        sTerminaEquipShown = false;
    }

    if ((pauseCtx->state != 6) || (pauseCtx->unk_1E4 != 0) || (pauseCtx->debugState != 0) ||
        (sWorldSwitchFrame == (s32)play->state.frames) || !CHECK_BTN_ALL(input->press.button, BTN_CUP) ||
        (MergeShip_Kaleido_WorldSwitchMode(play, &label) != MS_WORLD_SWITCH_SWITCH)) {
        return;
    }
    sWorldSwitchFrame = (s32)play->state.frames;

    switch (pauseCtx->pageIndex) {
        case PAUSE_ITEM:
            MergeShip_Pause_SetMMItemView(pauseCtx, true);
            toTermina = true;
            break;
        case MERGESHIP_PAUSE_MM_ITEM:
            MergeShip_Pause_SetMMItemView(pauseCtx, false);
            toTermina = false;
            break;
        case PAUSE_QUEST:
            MergeShip_QuestTermina_Switch(play);
            toTermina = sTerminaQuestShown;
            break;
        case PAUSE_MAP:
            sTerminaMapShown ^= 1;
            toTermina = sTerminaMapShown;
            break;
        default: // PAUSE_EQUIP
            sTerminaEquipShown ^= 1;
            toTermina = sTerminaEquipShown;
            if (sTerminaEquipShown) {
                MergeShip_EquipTermina_PlaceCursor();
            }
            break;
    }

    // Quest Status and Equipment: the cursor comes off a page arrow onto the view's cell (each view keeps its own). The
    // item pages keep it on the arrow, where C-Up switches straight back (on an item it would equip it).
    if (((pauseCtx->pageIndex == PAUSE_QUEST) || (pauseCtx->pageIndex == PAUSE_EQUIP)) &&
        (pauseCtx->cursorSpecialPos != 0)) {
        pauseCtx->cursorSpecialPos = 0;
    }
    // The new view's name loads (KaleidoScope_UpdateNamePanel reloads when namedItem differs).
    pauseCtx->namedItem = PAUSE_ITEM_NONE;
    pauseCtx->nameDisplayTimer = 0;
    // Nothing else on the page sees this press (the new view's own C-Up would equip).
    input->press.button &= ~BTN_CUP;

    // Ocarina of Time's page-turn sound (KaleidoScope_SwitchPage).
    Audio_PlaySfxGeneral(toTermina ? NA_SE_SY_WIN_SCROLL_RIGHT : NA_SE_SY_WIN_SCROLL_LEFT, &gSfxDefaultPos, 4,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    MergeShipTest_Log("WORLDSWITCH page=%d termina=%d", pauseCtx->pageIndex, toTermina);
}

void MergeShip_Kaleido_ResetWorldViews(PlayState* play) {
    sTerminaQuestShown = false;
    sTerminaEquipShown = false;
    sTerminaMapShown = false;
    MergeShip_Pause_SetMMItemView((play != NULL) ? &play->pauseCtx : NULL, false);
}

// ---- the label ---------------------------------------------------------------------------------------------------

extern const char* fontTbl[]; // z_kanfont.c: nes_font_static's characters, from ' '
extern f32 sFontWidths[];     // z_message_PAL.c: their advance at scale 1

// Under the page, right of the name panel (screen coordinates, 320 x 240), in the message font at 0.6 (OoT's text is
// 0.75). The C-Up glyph (nes_font_static 0xA6) is drawn in the C buttons' yellow, the text white; both grey while
// C-Up would equip instead.
#define MS_LABEL_RIGHT 282
#define MS_LABEL_TOP 204
#define MS_LABEL_SCALE 0.6f
#define MS_FONT_CHAR_CDOWN 0xA5 // the C-Up glyph (nes_font_static 0xA5)

static Gfx* MergeShip_Kaleido_DrawLabelChar(Gfx* gfx, u8 character, s32 x, s32 y, u8 r, u8 g, u8 b) {
    s32 size = (s32)(16.0f * MS_LABEL_SCALE);
    s32 step = (s32)(1024.0f / MS_LABEL_SCALE);

    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, (void*)fontTbl[character - ' '], G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    // The message box's drop shadow (Message_DrawTextChar).
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 255);
    gSPTextureRectangle(gfx++, (x + 1) << 2, (y + 1) << 2, (x + 1 + size) << 2, (y + 1 + size) << 2, G_TX_RENDERTILE,
                        0, 0, step, step);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, r, g, b, 255);
    gSPTextureRectangle(gfx++, x << 2, y << 2, (x + size) << 2, (y + size) << 2, G_TX_RENDERTILE, 0, 0, step, step);
    return gfx;
}

void MergeShip_Kaleido_DrawWorldSwitchLabel(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    const char* label = NULL;
    const char* c;
    s32 mode;
    s32 dim;
    f32 width;
    f32 x;

    if ((pauseCtx->state != 6) || (pauseCtx->unk_1E4 != 0) || (pauseCtx->debugState != 0)) {
        return;
    }
    mode = MergeShip_Kaleido_WorldSwitchMode(play, &label);
    if ((mode == MS_WORLD_SWITCH_NONE) || (label == NULL)) {
        return;
    }
    dim = (mode == MS_WORLD_SWITCH_EQUIPS);

    width = sFontWidths[MS_FONT_CHAR_CDOWN - ' '] + 2.0f;
    for (c = label; *c != '\0'; c++) {
        width += sFontWidths[(u8)*c - ' '];
    }
    x = MS_LABEL_RIGHT - width * MS_LABEL_SCALE;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    // Message_DrawMain's text: the colour is the primitive's, the glyph its alpha.
    gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    OVERLAY_DISP = MergeShip_Kaleido_DrawLabelChar(OVERLAY_DISP, MS_FONT_CHAR_CDOWN, (s32)x, MS_LABEL_TOP,
                                                   dim ? 120 : 255, dim ? 120 : 255, dim ? 60 : 50);
    x += (sFontWidths[MS_FONT_CHAR_CDOWN - ' '] + 2.0f) * MS_LABEL_SCALE;
    for (c = label; *c != '\0'; c++) {
        OVERLAY_DISP = MergeShip_Kaleido_DrawLabelChar(OVERLAY_DISP, (u8)*c, (s32)x, MS_LABEL_TOP, dim ? 120 : 255,
                                                       dim ? 120 : 255, dim ? 120 : 255);
        x += sFontWidths[(u8)*c - ' '] * MS_LABEL_SCALE;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
