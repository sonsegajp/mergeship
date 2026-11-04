#include "soh/MergeShip/MergeShipMMScene.h"

#include "global.h"

// MM Scene_SetRenderModeXlu. The entries after each gsSPEndDisplayList are reached through
// G_DL_INDEX (MM's player and object display lists call 0x0C at index 2).
void MergeShip_Scene_SetRenderModeXlu(PlayState* play, s32 index, u32 flags) {
    static Gfx renderModeSetNoneDL[] = {
        gsSPEndDisplayList(),
        gsSPEndDisplayList(),
        gsSPEndDisplayList(),
        gsSPEndDisplayList(),
    };
    static Gfx renderModeSetXluSingleCycleDL[] = {
        gsDPSetRenderMode(AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1),
                          G_RM_AA_ZB_XLU_SURF2),
        gsSPEndDisplayList(),
        gsDPSetRenderMode(AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA),
                          G_RM_AA_ZB_XLU_SURF2),
        gsSPEndDisplayList(),
    };
    static Gfx renderModeSetXluTwoCycleDL[] = {
        gsDPSetRenderMode(AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1),
                          AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
        gsSPEndDisplayList(),
        gsDPSetRenderMode(AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA),
                          AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
        gsSPEndDisplayList(),
    };
    static Gfx* dLists[] = {
        renderModeSetNoneDL,
        renderModeSetXluSingleCycleDL,
        renderModeSetXluTwoCycleDL,
    };
    Gfx* dList = dLists[index];

    OPEN_DISPS(play->state.gfxCtx);

    if (flags & 1) {
        gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)dList);
    }

    if (flags & 2) {
        gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)dList);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
