/*
 * MergeShip2 -- Pause Menu, "Items (Hyrule)" face (MERGESHIP_PAUSE_OOT_ITEM, 2s2h/MergeShip/MergeShipPause.h).
 *
 * Majora's Mask's 24-slot item grid is full, so Ocarina of Time's C-button, mask and trade items (MergeShipOoTItems.h)
 * get a page of their own: the Item face's Ocarina of Time view, switched to and back with C-Up on the Item face (as
 * SoH's Item face switches to Majora's Mask's; z_kaleido_mergeship.c, MergeShip_Pause_SetOoTItemView), drawn in the
 * Item face's place on the pause carousel. Each pause screen opens on MM's own Select Item. It is Ocarina of Time's
 * item page -- its "Select Item" background from oot-assets.o2r, its 6 x 4 grid in OoT's slot order
 * (MergeShipOoTSlot), OoT's icons and names -- run by Majora's Mask's item-page code: this file is MM's KaleidoScope_DrawItemSelect,
 * KaleidoScope_UpdateItemCursor and KaleidoScope_DrawAmmoCount (z_kaleido_item.c) for that grid, and equipping hands
 * over to MM's own KaleidoScope_UpdateItemEquip (PAUSE_MAIN_STATE_EQUIP_ITEM) with the cell's button slot
 * MS_OOT_BUTTON_SLOT(cell), which is unique, so MM's swap-by-slot between buttons works unchanged. Hyrule's magic
 * arrows fly onto this page's bow cell and go to the button as the bow with that arrow, as in Ocarina of Time
 * (MergeShip_Kaleido_OoTMagicArrowBowCell / MergeShip_Kaleido_MagicArrowBowSlot).
 *
 * PauseContext's per-page arrays have no entry for this face (PAUSE_PAGE_MAX), so its cursor lives here.
 *
 * Not ported: MM's A button on an item opens its description (message 0x1700 + item); Majora's Mask has no text for
 * these items, so A does nothing here (it is shown disabled) -- except on the two trade cells, which hold one item of
 * their chain at a time: there A shows the next owned one (MergeShip_OoTItems_CycleTradeSlot).
 */

#include "2s2h/MergeShip/MergeShipOwnership.h" // MergeShip2: each game's own counts and bottles
#include "z_kaleido_scope.h"
#include "interface/parameter_static/parameter_static.h"
#include "interface/icon_item_jpn_static/icon_item_jpn_static.h"
#include "align_asset_macro.h"
#include "2s2h_assets.h"
#include "2s2h/BenGui/CosmeticEditor.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/MergeShip/MergeShipAssets.h"
#include "2s2h/MergeShip/MergeShipOoTItems.h"
#include "2s2h/MergeShip/MergeShipPause.h"
#include <libultraship/bridge/consolevariablebridge.h>

// z_kaleido_scope_NES.c
extern f32 sPauseMenuVerticalOffset;
extern f32 sItemMaskCursorsX[];
extern f32 sItemMaskCursorsY[];
s16 KaleidoScope_SetPageVertices(PlayState* play, Vtx* vtx, s16 vtxPage, s16 numQuads);
Gfx* KaleidoScope_DrawPageSections(Gfx* gfx, Vtx* vertices, TexturePtr* textures);
// z_kaleido_item.c (KaleidoScope_UpdateItemEquip's state)
extern s16 sEquipState;
extern s16 sEquipMagicArrowSlotHoldTimer;
extern s16 sEquipAnimTimer;
// z_parameter.c
extern const char* gAmmoDigitTextures[10];

// Ocarina of Time's "Select Item" page background (OoT icon_item_static / icon_item_nes_static, IA8 80x32), in MM
// KaleidoScope_DrawPageSections' order: column 0 rows 0..4, column 1, column 2.
static const ALIGN_ASSET(2) char sOoTPauseSelectItem00Tex[] =
    OOT_ASSET("textures/icon_item_nes_static/gPauseSelectItem00ENGTex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem01Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem01Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem02Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem02Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem03Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem03Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem04Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem04Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem10Tex[] =
    OOT_ASSET("textures/icon_item_nes_static/gPauseSelectItem10ENGTex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem11Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem11Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem12Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem12Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem13Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem13Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem14Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem14Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem20Tex[] =
    OOT_ASSET("textures/icon_item_nes_static/gPauseSelectItem20ENGTex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem21Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem21Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem22Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem22Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem23Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem23Tex");
static const ALIGN_ASSET(2) char sOoTPauseSelectItem24Tex[] = OOT_ASSET("textures/icon_item_static/gPauseSelectItem24Tex");

static TexturePtr sOoTItemPageBgTextures[] = {
    sOoTPauseSelectItem00Tex, sOoTPauseSelectItem01Tex, sOoTPauseSelectItem02Tex, sOoTPauseSelectItem03Tex,
    sOoTPauseSelectItem04Tex, sOoTPauseSelectItem10Tex, sOoTPauseSelectItem11Tex, sOoTPauseSelectItem12Tex,
    sOoTPauseSelectItem13Tex, sOoTPauseSelectItem14Tex, sOoTPauseSelectItem20Tex, sOoTPauseSelectItem21Tex,
    sOoTPauseSelectItem22Tex, sOoTPauseSelectItem23Tex, sOoTPauseSelectItem24Tex,
};

// The face's "To <page>" label on its neighbours' Z / R side: Ocarina of Time's own "To Select Item" (OoT
// icon_item_nes_static, IA8 128x16; MM's labels are IA4 128x16).
static const ALIGN_ASSET(2) char sOoTPauseToSelectItemTex[] =
    OOT_ASSET("textures/icon_item_nes_static/gPauseToSelectItemENGTex");

#define OOT_ITEM_GRID_COLS 6
#define OOT_ITEM_GRID_ROWS 4
#define OOT_ITEM_NUM_CELLS (OOT_ITEM_GRID_COLS * OOT_ITEM_GRID_ROWS) // MS_OOT_SLOT_MAX

// The face's vertices: the grid's cells, then one "equipped" outline per item button (C-Left..C-Right, then 2S2H's
// D-pad Right..Up).
#define OOT_ITEM_QUAD_OUTLINE_FIRST OOT_ITEM_NUM_CELLS
#define OOT_ITEM_QUAD_OUTLINES 7
#define OOT_ITEM_QUAD_MAX (OOT_ITEM_QUAD_OUTLINE_FIRST + OOT_ITEM_QUAD_OUTLINES)

// PauseContext::namedItem while this face names a cell: past every value MM's pages put there (item ids, map and quest
// indices, PAUSE_ITEM_NONE), so MM's name panel always reloads its own name after a page switch.
#define OOT_ITEM_NAMED_ITEM(item) (0x400 + (item))

// MM KaleidoScope_DrawAmmoCount's screen rectangles for the item grid (sAmmoRectLeft/Height): column 0 at 62, 33 apart;
// rows at 85, 117, 150 (MM has no row-3 ammo; 182 continues the step).
static s16 sOoTAmmoRectLeft[OOT_ITEM_GRID_COLS] = { 62, 95, 128, 161, 194, 227 };
static s16 sOoTAmmoRectTop[OOT_ITEM_GRID_ROWS] = { 85, 117, 150, 182 };

// MM PauseContext::cursorPoint/XIndex/YIndex/Item/Slot[PAUSE_ITEM] for this face.
static s16 sOoTItemCursorPoint = 0;
static s16 sOoTItemCursorXIndex = 0;
static s16 sOoTItemCursorYIndex = 0;
static u16 sOoTItemCursorItem = PAUSE_ITEM_NONE;
static u16 sOoTItemCursorSlot = 0;

static u8 MergeShip_Kaleido_OoTCellItem(s16 cell) {
    if ((cell < 0) || (cell >= OOT_ITEM_NUM_CELLS)) {
        return ITEM_NONE;
    }
    return MergeShip_OoTItems_GetSlotItem(cell);
}

static s32 MergeShip_Kaleido_OoTCellIsTrade(s16 cell) {
    // MergeShip2 (BUG-0014): Nayru's Love's cell holds the Roc's Feather too, A switching between them.
    return (cell == MS_OOT_SLOT_TRADE_ADULT) || (cell == MS_OOT_SLOT_TRADE_CHILD) || (cell == MS_OOT_SLOT_NAYRUS_LOVE);
}

// MM's item-restriction test (z_kaleido_item.c, KaleidoScope_DrawItemSelect).
static s32 MergeShip_Kaleido_OoTItemRestricted(u8 item) {
    ItemId itemId = item;

    return GameInteractor_Should(VB_ITEM_BE_RESTRICTED, !gPlayerFormItemRestrictions[GET_PLAYER_FORM][item], &itemId);
}

// MM KaleidoScope_SetVertices' item-grid quad of `cell` (ITEM_GRID_* in z_kaleido_scope.h).
static void MergeShip_Kaleido_OoTCellQuadPos(PauseContext* pauseCtx, s16 cell, s16* x, s16* y) {
    *x = -(ITEM_GRID_COLS * ITEM_GRID_CELL_WIDTH) / 2 + (cell % OOT_ITEM_GRID_COLS) * ITEM_GRID_CELL_WIDTH +
         ITEM_GRID_QUAD_MARGIN;
    *y = (ITEM_GRID_ROWS * ITEM_GRID_CELL_HEIGHT) / 2 - 6 - (cell / OOT_ITEM_GRID_COLS) * ITEM_GRID_CELL_HEIGHT +
         pauseCtx->offsetY - ITEM_GRID_QUAD_MARGIN;
}

static void MergeShip_Kaleido_OoTSetQuad(Vtx* vtx, s16 x, s16 y, s16 width, s16 height, s16 texSize, u8 alpha) {
    s32 k;

    vtx[0].v.ob[0] = vtx[2].v.ob[0] = x;
    vtx[1].v.ob[0] = vtx[3].v.ob[0] = x + width;
    vtx[0].v.ob[1] = vtx[1].v.ob[1] = y;
    vtx[2].v.ob[1] = vtx[3].v.ob[1] = y - height;
    for (k = 0; k < 4; k++) {
        vtx[k].v.ob[2] = 0;
        vtx[k].v.flag = 0;
        vtx[k].v.cn[0] = vtx[k].v.cn[1] = vtx[k].v.cn[2] = 255;
        vtx[k].v.cn[3] = alpha;
    }
    vtx[0].v.tc[0] = vtx[0].v.tc[1] = vtx[1].v.tc[1] = vtx[2].v.tc[0] = 0;
    vtx[1].v.tc[0] = vtx[2].v.tc[1] = vtx[3].v.tc[0] = vtx[3].v.tc[1] = texSize * (1 << 5);
}

// MM KaleidoScope_SetVertices for the item page (the grid and the equipped outlines).
static Vtx* MergeShip_Kaleido_BuildOoTItemVtx(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Vtx* vtx = GRAPH_ALLOC(play->state.gfxCtx, (OOT_ITEM_QUAD_MAX * 4) * sizeof(Vtx));
    s16 cell;
    s16 x;
    s16 y;
    s16 i;

    for (cell = 0; cell < OOT_ITEM_NUM_CELLS; cell++) {
        MergeShip_Kaleido_OoTCellQuadPos(pauseCtx, cell, &x, &y);
        MergeShip_Kaleido_OoTSetQuad(&vtx[cell * 4], x, y, ITEM_GRID_QUAD_WIDTH, ITEM_GRID_QUAD_HEIGHT,
                                     ITEM_GRID_QUAD_TEX_SIZE, 255);
    }
    for (i = OOT_ITEM_QUAD_OUTLINE_FIRST; i < OOT_ITEM_QUAD_MAX; i++) {
        // Out of view until a button holds one of this face's cells (MM: -300, 300).
        MergeShip_Kaleido_OoTSetQuad(&vtx[i * 4], -300, 300, ITEM_GRID_SELECTED_QUAD_WIDTH,
                                     ITEM_GRID_SELECTED_QUAD_HEIGHT, ITEM_GRID_SELECTED_QUAD_TEX_SIZE, pauseCtx->alpha);
    }
    return vtx;
}

// MM's white box around a cell that is on an item button.
static void MergeShip_Kaleido_DrawOoTOutline(PlayState* play, Vtx* vtx, s16 quad, u8 buttonSlot, u8 buttonItem,
                                             s32 button, s32 isDpad) {
    PauseContext* pauseCtx = &play->pauseCtx;
    ItemId item = buttonItem;
    s16 cell;
    Vtx* outline = &vtx[quad * 4];

    if ((buttonItem == ITEM_NONE) || !MS_OOT_BUTTON_SLOT_IS_OOT(buttonSlot)) {
        return;
    }
    if (!GameInteractor_Should(VB_DRAW_ITEM_EQUIPPED_OUTLINE, true, &item, button, isDpad, MERGESHIP_PAUSE_OOT_ITEM)) {
        return;
    }
    cell = buttonSlot - MS_OOT_BUTTON_SLOT_FIRST;
    MergeShip_Kaleido_OoTSetQuad(outline, vtx[cell * 4].v.ob[0] + ITEM_GRID_SELECTED_QUAD_MARGIN,
                                 vtx[cell * 4].v.ob[1] - ITEM_GRID_SELECTED_QUAD_MARGIN, ITEM_GRID_SELECTED_QUAD_WIDTH,
                                 ITEM_GRID_SELECTED_QUAD_HEIGHT, ITEM_GRID_SELECTED_QUAD_TEX_SIZE, pauseCtx->alpha);

    OPEN_DISPS(play->state.gfxCtx);
    gSPVertex(POLY_OPA_DISP++, outline, 4, 0);
    POLY_OPA_DISP = Gfx_DrawTexQuadIA8(POLY_OPA_DISP, gEquippedItemOutlineTex, 32, 32, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM KaleidoScope_DrawAmmoCount for the Slingshot cell (OoT's Deku Seeds) and Hyrule's copies of the counted items both
// games have (bombs .. beans, the bow's arrows: Ocarina of Time's own counts, MergeShipOwnership.h).
static void MergeShip_Kaleido_DrawOoTItemAmmo(PlayState* play, s16 cell, u8 item) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s16 ammo = (cell == MS_OOT_SLOT_SLINGSHOT) ? gSaveContext.save.shipSaveInfo.mergeShipOoTItems.seeds
                                               : MergeShip_Own_PageAmmo(item, 0);
    s16 capacity = (cell == MS_OOT_SLOT_SLINGSHOT) ? MergeShip_OoTItems_SeedCapacity()
                   : (item == ITEM_BOMBCHU)        ? 50
                   : (item == ITEM_MAGIC_BEANS)    ? 20
                                                   : MergeShip_Own_PageCapacity(item, 0);
    s16 ammoUpperDigit;
    s16 left = sOoTAmmoRectLeft[cell % OOT_ITEM_GRID_COLS];
    s16 top = sOoTAmmoRectTop[cell / OOT_ITEM_GRID_COLS];

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);

    if (MergeShip_Kaleido_OoTItemRestricted(item)) {
        // Ammo item is restricted
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 100, 100, 100, pauseCtx->alpha);
    } else {
        // Default ammo
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
        if (ammo == 0) {
            // Out of ammo
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 130, 130, 130, pauseCtx->alpha);
        } else if (ammo == capacity) {
            // Ammo at capacity
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 120, 255, 0, pauseCtx->alpha);
        }
    }

    // Separate ammo into upper and lower digits
    for (ammoUpperDigit = 0; ammo >= 10; ammoUpperDigit++) {
        ammo -= 10;
    }

    gDPPipeSync(POLY_OPA_DISP++);

    // Draw upper digit
    if (ammoUpperDigit != 0) {
        POLY_OPA_DISP = Gfx_DrawTexRectIA8(POLY_OPA_DISP, gAmmoDigitTextures[ammoUpperDigit % 10], 8, 8, left, top, 8,
                                           8, 1 << 10, 1 << 10);
    }

    // Draw lower digit
    POLY_OPA_DISP =
        Gfx_DrawTexRectIA8(POLY_OPA_DISP, gAmmoDigitTextures[ammo], 8, 8, left + 6, top, 8, 8, 1 << 10, 1 << 10);

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM KaleidoScope_DrawItemSelect.
static void MergeShip_Kaleido_DrawOoTItemSelect(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Vtx* vtx = MergeShip_Kaleido_BuildOoTItemVtx(play);
    s16 quad;
    s16 i;
    s16 j;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL42_Opa(play->state.gfxCtx);

    // Draw a white box around the cells that are equipped on the C buttons (and 2S2H's D-pad)
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    quad = OOT_ITEM_QUAD_OUTLINE_FIRST;
    for (i = EQUIP_SLOT_C_LEFT; i <= EQUIP_SLOT_C_RIGHT; i++, quad++) {
        MergeShip_Kaleido_DrawOoTOutline(play, vtx, quad, GET_CUR_FORM_BTN_SLOT(i), GET_CUR_FORM_BTN_ITEM(i), i, 0);
    }
    if (CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0)) {
        for (i = EQUIP_SLOT_D_RIGHT; i <= EQUIP_SLOT_D_UP; i++, quad++) {
            MergeShip_Kaleido_DrawOoTOutline(play, vtx, quad, DPAD_GET_CUR_FORM_BTN_SLOT(i),
                                             DPAD_GET_CUR_FORM_BTN_ITEM(i), i, 1);
        }
    }

    gDPPipeSync(POLY_OPA_DISP++);

    // Draw the item icons
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (i = 0, j = 0; i < OOT_ITEM_NUM_CELLS; i++, j += 4) {
        u8 item = MergeShip_Kaleido_OoTCellItem(i);
        const char* icon;
        u8 itemRestricted;

        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

        if (item == ITEM_NONE) {
            continue;
        }
        icon = MergeShip_OoTItems_IconTexture(item);
        if (icon == NULL) {
            continue;
        }
        itemRestricted = MergeShip_Kaleido_OoTItemRestricted(item);

        if ((pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE) && (pauseCtx->pageIndex == MERGESHIP_PAUSE_OOT_ITEM) &&
            (pauseCtx->cursorSpecialPos == 0) && !itemRestricted && (i == sOoTItemCursorSlot)) {
            // Increase the size of the selected item
            vtx[j + 0].v.ob[0] = vtx[j + 2].v.ob[0] = vtx[j + 0].v.ob[0] - ITEM_GRID_QUAD_ENLARGE_OFFSET;
            vtx[j + 1].v.ob[0] = vtx[j + 3].v.ob[0] = vtx[j + 0].v.ob[0] + ITEM_GRID_SELECTED_QUAD_WIDTH;
            vtx[j + 0].v.ob[1] = vtx[j + 1].v.ob[1] = vtx[j + 0].v.ob[1] + ITEM_GRID_QUAD_ENLARGE_OFFSET;
            vtx[j + 2].v.ob[1] = vtx[j + 3].v.ob[1] = vtx[j + 0].v.ob[1] - ITEM_GRID_SELECTED_QUAD_HEIGHT;
        }

        // 2S2H [Port] the restricted items are drawn grey with gSPGrayscale
        if (itemRestricted) {
            gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
            gSPGrayscale(POLY_OPA_DISP++, true);
        }
        gSPVertex(POLY_OPA_DISP++, &vtx[j + 0], 4, 0);
        KaleidoScope_DrawTexQuadRGBA32(play->state.gfxCtx, (TexturePtr)icon, 32, 32, 0);
        if (itemRestricted) {
            gSPGrayscale(POLY_OPA_DISP++, false);
        }
    }

    // Draw the ammo digits (the Slingshot's Deku Seeds)
    if (pauseCtx->pageIndex == MERGESHIP_PAUSE_OOT_ITEM) {
        if ((pauseCtx->state == PAUSE_STATE_MAIN) &&
            ((pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE) || (pauseCtx->mainState == PAUSE_MAIN_STATE_EQUIP_ITEM)) &&
            (pauseCtx->state != PAUSE_STATE_SAVEPROMPT) && !IS_PAUSE_STATE_GAMEOVER(pauseCtx)) {
            static const s16 sAmmoCells[] = { MS_OOT_SLOT_SLINGSHOT, MS_OOT_SLOT_DEKU_STICK, MS_OOT_SLOT_DEKU_NUT,
                                               MS_OOT_SLOT_BOMB,      MS_OOT_SLOT_BOW,        MS_OOT_SLOT_BOMBCHU,
                                               MS_OOT_SLOT_BEAN };
            s32 k;

            for (k = 0; k < ARRAY_COUNT(sAmmoCells); k++) {
                u8 item = MergeShip_Kaleido_OoTCellItem(sAmmoCells[k]);

                if (item != ITEM_NONE) {
                    Gfx_SetupDL39_Opa(play->state.gfxCtx);
                    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                    MergeShip_Kaleido_DrawOoTItemAmmo(play, sAmmoCells[k], item);
                    Gfx_SetupDL42_Opa(play->state.gfxCtx);
                }
            }
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void MergeShip_Kaleido_DrawOoTItemPage(PlayState* play, s32 current) {
    PauseContext* pauseCtx = &play->pauseCtx;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Vtx* pageVtx;

    if (((pauseCtx->pageIndex == MERGESHIP_PAUSE_OOT_ITEM) != (current != 0)) ||
        !MergeShip_Pause_PageOnRing(MERGESHIP_PAUSE_OOT_ITEM)) {
        return;
    }
    // MM's draw rule for the Item face's place: not while the face opposite it, Quest Status, is the current one.
    if (!current && (pauseCtx->pageIndex == PAUSE_QUEST)) {
        return;
    }

    // MM KaleidoScope_SetVertices / KaleidoScope_SetPageVertices for the page background.
    pageVtx = GRAPH_ALLOC(gfxCtx, ((PAGE_BG_QUADS + VTX_PAGE_ITEM_QUADS) * 4) * sizeof(Vtx));
    KaleidoScope_SetPageVertices(play, pageVtx, VTX_PAGE_ITEM, VTX_PAGE_ITEM_QUADS);

    OPEN_DISPS(gfxCtx);

    // As MM's face blocks in KaleidoScope_DrawPages.
    gDPPipeSync(POLY_OPA_DISP++);

    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_BILERP);

    gDPSetCombineLERP(POLY_OPA_DISP++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, SHADE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0,
                      SHADE, 0);

    gDPSetPrimColorOverride(POLY_OPA_DISP++, 0, 0, 180, 180, 120, 255, COSMETIC_ID("Menus.PausePanels"));

    MergeShip_Pause_LoadPageMatrix(MERGESHIP_PAUSE_OOT_ITEM, sPauseMenuVerticalOffset / 100.0f,
                                   MergeShip_Pause_FaceRoll(pauseCtx, MERGESHIP_PAUSE_OOT_ITEM));

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gfxCtx);

    POLY_OPA_DISP = KaleidoScope_DrawPageSections(POLY_OPA_DISP, pageVtx, sOoTItemPageBgTextures);

    GameInteractor_ExecuteBeforeKaleidoDrawPage(pauseCtx, MERGESHIP_PAUSE_OOT_ITEM);
    MergeShip_Kaleido_DrawOoTItemSelect(play);
    GameInteractor_ExecuteAfterKaleidoDrawPage(pauseCtx, MERGESHIP_PAUSE_OOT_ITEM);

    CLOSE_DISPS(gfxCtx);
}

// MM KaleidoScope_UpdateItemCursor, "Equip item to the C buttons" (and 2S2H's D-pad).
static void MergeShip_Kaleido_TryEquipOoTItem(PlayState* play, u16 cursorItem, u16 cursorSlot) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = CONTROLLER1(&play->state);
    u8 wornMask = MergeShip_OoTItems_WornMask();
    u8 targetItem = ITEM_NONE;
    s16 x;
    s16 y;

    // Only an item the current form can use; Deku Seeds are ammo, never a button item.
    if (MergeShip_Kaleido_OoTItemRestricted(cursorItem) || (cursorItem == ITEM_OOT_DEKU_SEEDS)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    // MergeShip2 (task 5): Hyrule's magic arrows need Hyrule's Fairy Bow owned first, as Majora's Mask's page needs its
    // Hero's Bow (z_kaleido_item.c): a unified seed can find them out of order.
    if (((cursorItem == ITEM_ARROW_FIRE) || (cursorItem == ITEM_ARROW_ICE) || (cursorItem == ITEM_ARROW_LIGHT)) &&
        !MergeShip_Own_HasBowForArrows(false)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }

    if (CHECK_BTN_ALL(input->press.button, BTN_CLEFT)) {
        targetItem = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT);
        pauseCtx->equipTargetCBtn = PAUSE_EQUIP_C_LEFT;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CDOWN)) {
        targetItem = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_DOWN);
        pauseCtx->equipTargetCBtn = PAUSE_EQUIP_C_DOWN;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CRIGHT)) {
        targetItem = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_RIGHT);
        pauseCtx->equipTargetCBtn = PAUSE_EQUIP_C_RIGHT;
    } else if (CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0)) {
        // #region 2S2H [Dpad]
        if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
            targetItem = DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_RIGHT);
            pauseCtx->equipTargetCBtn = PAUSE_EQUIP_D_RIGHT;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
            targetItem = DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_LEFT);
            pauseCtx->equipTargetCBtn = PAUSE_EQUIP_D_LEFT;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
            targetItem = DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_DOWN);
            pauseCtx->equipTargetCBtn = PAUSE_EQUIP_D_DOWN;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
            targetItem = DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_UP);
            pauseCtx->equipTargetCBtn = PAUSE_EQUIP_D_UP;
        } else {
            return;
        }
        // #endregion
    } else {
        return;
    }

    // Ensure that a mask can not be unequipped while being worn (MM's masks, and Ocarina of Time's on the human)
    if (((Player_GetCurMaskItemId(play) != ITEM_NONE) && (Player_GetCurMaskItemId(play) == targetItem)) ||
        ((wornMask != 0) && (wornMask != ITEM_NONE) && (wornMask == targetItem))) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }

    if (!GameInteractor_Should(VB_KALEIDO_EQUIP_ITEM_TO_BUTTON, true, MS_OOT_BUTTON_SLOT(cursorSlot), cursorItem)) {
        return;
    }

    // Equip item to the C buttons: MM's KaleidoScope_UpdateItemEquip flies the icon to the button and lands it.
    // The start point is the cell's quad as the selected (enlarged) cell is drawn.
    MergeShip_Kaleido_OoTCellQuadPos(pauseCtx, cursorSlot, &x, &y);
    MergeShip_Own_OnPageEquip(cursorItem, 0); // MergeShip2: Ocarina of Time's copy is the one in use
    pauseCtx->equipTargetItem = cursorItem;
    pauseCtx->equipTargetSlot = MS_OOT_BUTTON_SLOT(cursorSlot);
    pauseCtx->mainState = PAUSE_MAIN_STATE_EQUIP_ITEM;
    pauseCtx->equipAnimX = (x - ITEM_GRID_QUAD_ENLARGE_OFFSET) * 10;
    pauseCtx->equipAnimY = (y + ITEM_GRID_QUAD_ENLARGE_OFFSET) * 10;
    pauseCtx->equipAnimAlpha = 255;
    sEquipMagicArrowSlotHoldTimer = 0;
    sEquipState = EQUIP_STATE_MOVE_TO_C_BTN;
    sEquipAnimTimer = 10;

    // Hyrule's magic arrows equip as Ocarina of Time's do (OoT KaleidoScope_SetupItemEquip; MM's own page does the
    // same, KaleidoScope_UpdateItemCursor): the arrow's orb grows on its cell, flies onto Hyrule's bow -- this page's
    // bow cell (MergeShip_Kaleido_OoTMagicArrowBowCell) -- and the bow with that arrow, ITEM_BOW_FIRE .. with the bow
    // cell's button slot, goes to the button (KaleidoScope_UpdateItemEquip). Putting the bare arrow on the button
    // showed only the arrow's icon (the reported bug, 2026-09-30: "OOT fire arrows dont equip onto the bow normally in
    // MM, just places arrow icon there").
    if ((cursorItem == ITEM_ARROW_FIRE) || (cursorItem == ITEM_ARROW_ICE) || (cursorItem == ITEM_ARROW_LIGHT)) {
        s16 magicArrowIndex = 0;

        if (cursorItem == ITEM_ARROW_ICE) {
            magicArrowIndex = 1;
        }
        if (cursorItem == ITEM_ARROW_LIGHT) {
            magicArrowIndex = 2;
        }
        Audio_PlaySfx(NA_SE_SY_SET_FIRE_ARROW + magicArrowIndex);
        pauseCtx->equipTargetItem = 0xB5 + magicArrowIndex;
        pauseCtx->equipAnimAlpha = sEquipState = EQUIP_STATE_MAGIC_ARROW_GROW_ORB;
        sEquipAnimTimer = 6;
        return;
    }

    Audio_PlaySfx(NA_SE_SY_DECIDE);
}

// KaleidoScope_UpdateItemEquip, a magic arrow equipped from this page (its button slot is this page's,
// MS_OOT_BUTTON_SLOT): it flies onto Hyrule's Fairy Bow, this page's bow cell, as Ocarina of Time's flies onto its bow
// (OoT KaleidoScope_UpdateItemEquip: pauseCtx->itemVtx[SLOT_BOW * 4], OoT's SLOT_BOW being this grid's
// MS_OOT_SLOT_BOW) -- not onto Majora's Mask's (MM's pauseCtx->itemVtx[SLOT_BOW * 4] is another cell of this grid).
// False, leaving x / y alone, for Majora's Mask's own arrows.
s32 MergeShip_Kaleido_OoTMagicArrowBowCell(PauseContext* pauseCtx, s16* x, s16* y) {
    if (!MS_OOT_BUTTON_SLOT_IS_OOT(pauseCtx->equipTargetSlot)) {
        return false;
    }
    // The cell's quad as MergeShip_Kaleido_BuildOoTItemVtx builds it (MM's target is its unenlarged quad too).
    MergeShip_Kaleido_OoTCellQuadPos(pauseCtx, MS_OOT_SLOT_BOW, x, y);
    return true;
}

// KaleidoScope_UpdateItemEquip, once the arrow is on the bow: the slot the bow with that arrow goes to the button with.
// Ocarina of Time puts ITEM_BOW_ARROW_FIRE .. there with SLOT_BOW, the bow's own slot, so the bow is on one button only
// (MM's swap-by-slot); a Hyrule arrow's is Hyrule's bow cell's, MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOW), the slot the Fairy
// Bow itself is equipped with (MergeShip_Own_ButtonBowIsOoT), and Majora's Mask's own keep MM's SLOT_BOW.
u16 MergeShip_Kaleido_MagicArrowBowSlot(PauseContext* pauseCtx) {
    return MS_OOT_BUTTON_SLOT_IS_OOT(pauseCtx->equipTargetSlot) ? MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOW) : SLOT_BOW;
}

// MM KaleidoScope_UpdateItemCursor.
void MergeShip_Kaleido_UpdateOoTItemCursor(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    MessageContext* msgCtx = &play->msgCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Input* input = CONTROLLER1(&play->state);
    u16 cursorItem;
    u16 cursorSlot;
    s16 cursorPoint;
    s16 cursorXIndex;
    s16 cursorYIndex;
    s16 oldCursorPoint;
    s16 moveCursorResult;
    s32 aUsable;

    if (pauseCtx->pageIndex != MERGESHIP_PAUSE_OOT_ITEM) {
        return;
    }

    pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_WHITE;
    pauseCtx->nameColorSet = PAUSE_NAME_COLOR_SET_WHITE;

    if ((pauseCtx->state == PAUSE_STATE_MAIN) && (pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE) &&
        !pauseCtx->itemDescriptionOn) {
        moveCursorResult = PAUSE_CURSOR_RESULT_NONE;
        oldCursorPoint = sOoTItemCursorPoint;

        cursorItem = sOoTItemCursorItem;

        // Move cursor left/right
        if (pauseCtx->cursorSpecialPos == 0) {
            // cursor is currently on a slot
            pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_YELLOW;

            if (ABS_ALT(pauseCtx->stickAdjX) > 30) {
                cursorPoint = sOoTItemCursorPoint;
                cursorXIndex = sOoTItemCursorXIndex;
                cursorYIndex = sOoTItemCursorYIndex;

                // Search for slot to move to
                while (moveCursorResult == PAUSE_CURSOR_RESULT_NONE) {
                    if (pauseCtx->stickAdjX < -30) {
                        // move cursor left
                        pauseCtx->cursorShrinkRate = 4.0f;
                        if (sOoTItemCursorXIndex != 0) {
                            sOoTItemCursorXIndex--;
                            sOoTItemCursorPoint--;
                            moveCursorResult = PAUSE_CURSOR_RESULT_SLOT;
                        } else {
                            sOoTItemCursorXIndex = cursorXIndex;
                            sOoTItemCursorYIndex++;

                            if (sOoTItemCursorYIndex >= OOT_ITEM_GRID_ROWS) {
                                sOoTItemCursorYIndex = 0;
                            }

                            sOoTItemCursorPoint = sOoTItemCursorXIndex + (sOoTItemCursorYIndex * OOT_ITEM_GRID_COLS);

                            if (sOoTItemCursorPoint >= OOT_ITEM_NUM_CELLS) {
                                sOoTItemCursorPoint = sOoTItemCursorXIndex;
                            }

                            if (cursorYIndex == sOoTItemCursorYIndex) {
                                sOoTItemCursorXIndex = cursorXIndex;
                                sOoTItemCursorPoint = cursorPoint;

                                KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);

                                moveCursorResult = PAUSE_CURSOR_RESULT_SPECIAL_POS;
                            }
                        }
                    } else if (pauseCtx->stickAdjX > 30) {
                        // move cursor right
                        pauseCtx->cursorShrinkRate = 4.0f;
                        if (sOoTItemCursorXIndex <= OOT_ITEM_GRID_COLS - 2) {
                            sOoTItemCursorXIndex++;
                            sOoTItemCursorPoint++;
                            moveCursorResult = PAUSE_CURSOR_RESULT_SLOT;
                        } else {
                            sOoTItemCursorXIndex = cursorXIndex;
                            sOoTItemCursorYIndex++;

                            if (sOoTItemCursorYIndex >= OOT_ITEM_GRID_ROWS) {
                                sOoTItemCursorYIndex = 0;
                            }

                            sOoTItemCursorPoint = sOoTItemCursorXIndex + (sOoTItemCursorYIndex * OOT_ITEM_GRID_COLS);

                            if (sOoTItemCursorPoint >= OOT_ITEM_NUM_CELLS) {
                                sOoTItemCursorPoint = sOoTItemCursorXIndex;
                            }

                            if (cursorYIndex == sOoTItemCursorYIndex) {
                                sOoTItemCursorXIndex = cursorXIndex;
                                sOoTItemCursorPoint = cursorPoint;

                                KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);

                                moveCursorResult = PAUSE_CURSOR_RESULT_SPECIAL_POS;
                            }
                        }
                    }
                }

                if (moveCursorResult == PAUSE_CURSOR_RESULT_SLOT) {
                    cursorItem = MergeShip_Kaleido_OoTCellItem(sOoTItemCursorPoint);
                }
            }
        } else if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
            if (pauseCtx->stickAdjX > 30) {
                KaleidoScope_MoveCursorFromSpecialPos(play);
                cursorYIndex = 0;
                cursorXIndex = 0;
                cursorPoint = 0; // top row, left column (OoT SLOT_DEKU_STICK)

                // Search for slot to move to
                while (true) {
                    // Check if current cursor has an item in its slot
                    if (MergeShip_Kaleido_OoTCellItem(cursorPoint) != ITEM_NONE) {
                        sOoTItemCursorPoint = cursorPoint;
                        sOoTItemCursorXIndex = cursorXIndex;
                        sOoTItemCursorYIndex = cursorYIndex;
                        moveCursorResult = PAUSE_CURSOR_RESULT_SLOT;
                        break;
                    }

                    // move 1 row down and retry
                    cursorYIndex++;
                    cursorPoint += OOT_ITEM_GRID_COLS;
                    if (cursorYIndex < OOT_ITEM_GRID_ROWS) {
                        continue;
                    }

                    // move 1 column right and retry
                    cursorYIndex = 0;
                    cursorPoint = cursorXIndex + 1;
                    cursorXIndex = cursorPoint;
                    if (cursorXIndex < OOT_ITEM_GRID_COLS) {
                        continue;
                    }

                    // No item available
                    KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                    break;
                }
            }
        } else { // PAUSE_CURSOR_PAGE_RIGHT
            if (pauseCtx->stickAdjX < -30) {
                KaleidoScope_MoveCursorFromSpecialPos(play);
                cursorXIndex = OOT_ITEM_GRID_COLS - 1;
                cursorPoint = OOT_ITEM_GRID_COLS - 1; // top row, right column (OoT SLOT_DINS_FIRE)
                cursorYIndex = 0;

                // Search for slot to move to
                while (true) {
                    // Check if current cursor has an item in its slot
                    if (MergeShip_Kaleido_OoTCellItem(cursorPoint) != ITEM_NONE) {
                        sOoTItemCursorPoint = cursorPoint;
                        sOoTItemCursorXIndex = cursorXIndex;
                        sOoTItemCursorYIndex = cursorYIndex;
                        moveCursorResult = PAUSE_CURSOR_RESULT_SLOT;
                        break;
                    }

                    // move 1 row down and retry
                    cursorYIndex++;
                    cursorPoint += OOT_ITEM_GRID_COLS;
                    if (cursorYIndex < OOT_ITEM_GRID_ROWS) {
                        continue;
                    }

                    // move 1 column left and retry
                    cursorYIndex = 0;
                    cursorPoint = cursorXIndex - 1;
                    cursorXIndex = cursorPoint;
                    if (cursorXIndex >= 0) {
                        continue;
                    }

                    // No item available
                    KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
                    break;
                }
            }
        }

        if (pauseCtx->cursorSpecialPos == 0) {
            // move cursor up/down
            if (ABS_ALT(pauseCtx->stickAdjY) > 30) {
                moveCursorResult = PAUSE_CURSOR_RESULT_NONE;

                cursorPoint = sOoTItemCursorPoint;
                cursorYIndex = sOoTItemCursorYIndex;

                while (moveCursorResult == PAUSE_CURSOR_RESULT_NONE) {
                    if (pauseCtx->stickAdjY > 30) {
                        // move cursor up
                        moveCursorResult = PAUSE_CURSOR_RESULT_SPECIAL_POS;
                        if (sOoTItemCursorYIndex != 0) {
                            sOoTItemCursorYIndex--;
                            pauseCtx->cursorShrinkRate = 4.0f;
                            sOoTItemCursorPoint -= OOT_ITEM_GRID_COLS;
                            moveCursorResult = PAUSE_CURSOR_RESULT_SLOT;
                        } else {
                            sOoTItemCursorYIndex = cursorYIndex;
                            sOoTItemCursorPoint = cursorPoint;
                        }
                    } else if (pauseCtx->stickAdjY < -30) {
                        // move cursor down
                        moveCursorResult = PAUSE_CURSOR_RESULT_SPECIAL_POS;
                        if (sOoTItemCursorYIndex < OOT_ITEM_GRID_ROWS - 1) {
                            sOoTItemCursorYIndex++;
                            pauseCtx->cursorShrinkRate = 4.0f;
                            sOoTItemCursorPoint += OOT_ITEM_GRID_COLS;
                            moveCursorResult = PAUSE_CURSOR_RESULT_SLOT;
                        } else {
                            sOoTItemCursorYIndex = cursorYIndex;
                            sOoTItemCursorPoint = cursorPoint;
                        }
                    }
                }
            }

            cursorSlot = sOoTItemCursorPoint;
            pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_YELLOW;

            if (moveCursorResult == PAUSE_CURSOR_RESULT_SLOT) {
                cursorItem = MergeShip_Kaleido_OoTCellItem(sOoTItemCursorPoint);
            } else if (moveCursorResult != PAUSE_CURSOR_RESULT_SPECIAL_POS) {
                cursorItem = MergeShip_Kaleido_OoTCellItem(sOoTItemCursorPoint);
            }

            if (cursorItem == ITEM_NONE) {
                cursorItem = PAUSE_ITEM_NONE;
                pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_WHITE;
            }

            // MM enables A over any item (its description); here A only acts on the trade cells.
            aUsable = (cursorItem != PAUSE_ITEM_NONE) && MergeShip_Kaleido_OoTCellIsTrade(cursorSlot);
            if (aUsable && (msgCtx->msgLength == 0)) {
                if (interfaceCtx->aButtonDoActionDelayed != DO_ACTION_DECIDE) {
                    Interface_SetAButtonDoAction(play, DO_ACTION_DECIDE);
                }
                if (gSaveContext.buttonStatus[EQUIP_SLOT_A] == BTN_DISABLED) {
                    gSaveContext.buttonStatus[EQUIP_SLOT_A] = BTN_ENABLED;
                    gSaveContext.hudVisibility = HUD_VISIBILITY_IDLE;
                    Interface_SetHudVisibility(HUD_VISIBILITY_ALL);
                }
            } else {
                if (interfaceCtx->aButtonDoActionDelayed == DO_ACTION_DECIDE) {
                    Interface_SetAButtonDoAction(play, DO_ACTION_INFO);
                }
                if (gSaveContext.buttonStatus[EQUIP_SLOT_A] != BTN_DISABLED) {
                    gSaveContext.buttonStatus[EQUIP_SLOT_A] = BTN_DISABLED;
                    gSaveContext.hudVisibility = HUD_VISIBILITY_IDLE;
                    Interface_SetHudVisibility(HUD_VISIBILITY_ALL);
                }
            }

            sOoTItemCursorItem = cursorItem;
            sOoTItemCursorSlot = cursorSlot;
            if (cursorItem != PAUSE_ITEM_NONE) {
                if ((pauseCtx->debugEditor == DEBUG_EDITOR_NONE) && !pauseCtx->itemDescriptionOn &&
                    (pauseCtx->state == PAUSE_STATE_MAIN) && (pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE) &&
                    CHECK_BTN_ANY(input->press.button, BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT | BTN_DPAD_EQUIP)) {
                    MergeShip_Kaleido_TryEquipOoTItem(play, cursorItem, cursorSlot);
                } else if ((pauseCtx->debugEditor == DEBUG_EDITOR_NONE) && (pauseCtx->state == PAUSE_STATE_MAIN) &&
                           (pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE) &&
                           CHECK_BTN_ALL(input->press.button, BTN_A) && (msgCtx->msgLength == 0) && aUsable) {
                    // The trade cells hold one item of their chain at a time; A shows the next owned one (and
                    // updates any button holding the cell's item).
                    MergeShip_OoTItems_CycleTradeSlot(cursorSlot);
                    Audio_PlaySfx(NA_SE_SY_DECIDE);
                    sOoTItemCursorItem = MergeShip_Kaleido_OoTCellItem(cursorSlot);
                    if (sOoTItemCursorItem == ITEM_NONE) {
                        sOoTItemCursorItem = PAUSE_ITEM_NONE;
                    }
                }
            }
        } else {
            sOoTItemCursorItem = PAUSE_ITEM_NONE;
            if (interfaceCtx->aButtonDoActionDelayed == DO_ACTION_DECIDE) {
                Interface_SetAButtonDoAction(play, DO_ACTION_INFO);
            }
        }

        if (oldCursorPoint != sOoTItemCursorPoint) {
            Audio_PlaySfx(NA_SE_SY_CURSOR);
        }
    } else if (pauseCtx->mainState == PAUSE_MAIN_STATE_EQUIP_ITEM) {
        pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_YELLOW;
    }
}

void Kaleido_LoadItemNameStatic(void** segment, u32 texIndex); // z_kaleido_scope_NES.c

// KaleidoScope_UpdateNamePanel for this face: Ocarina of Time's item names (OoT item_name_static, IA4 128x16, the
// format MM's panel draws), loaded explicitly -- MM's Kaleido_LoadItemNameStatic indexes by MM item id.
s32 MergeShip_Kaleido_UpdateOoTItemNamePanel(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    u16 namedItem;

    if (pauseCtx->pageIndex != MERGESHIP_PAUSE_OOT_ITEM) {
        return false;
    }

    namedItem = (sOoTItemCursorItem == PAUSE_ITEM_NONE) ? PAUSE_ITEM_NONE : OOT_ITEM_NAMED_ITEM(sOoTItemCursorItem);
    if (pauseCtx->namedItem != namedItem) {
        pauseCtx->namedItem = namedItem;

        if (namedItem != PAUSE_ITEM_NONE) {
            const char* name = MergeShip_OoTItems_NameTexture((u8)sOoTItemCursorItem);

            if (name != NULL) {
                pauseCtx->nameSegment = (u8*)name;
            } else {
                // An item of Majora's Mask's held here -- bottled in one of Ocarina of Time's bottles (a Magic
                // Mushroom, Chateau Romani...): its own name, as MM's pages load it (BUG-0024: it had none).
                Kaleido_LoadItemNameStatic((void**)&pauseCtx->nameSegment, (u8)sOoTItemCursorItem);
            }
            pauseCtx->nameDisplayTimer = 0;
        }
    } else if (pauseCtx->nameColorSet == PAUSE_NAME_COLOR_SET_WHITE) {
        // As MM's Item and Masks pages
        pauseCtx->nameDisplayTimer++;
        if (pauseCtx->nameDisplayTimer > 70) {
            pauseCtx->nameDisplayTimer = 0;
        }
    } else {
        pauseCtx->nameDisplayTimer = 0;
    }
    return true;
}

// KaleidoScope_UpdateCursorSize, as MM's PAUSE_ITEM case.
void MergeShip_Kaleido_SetOoTItemCursorPos(PauseContext* pauseCtx) {
    pauseCtx->cursorX = sItemMaskCursorsX[sOoTItemCursorXIndex];
    pauseCtx->cursorY = sItemMaskCursorsY[sOoTItemCursorYIndex];
    pauseCtx->cursorWidth = 15.0f;
    pauseCtx->cursorHeight = 15.0f;
}

// KaleidoScope_DrawInfoPanel's "To <page>" labels (MM D_8082B998 / D_8082B9A8, indexed by the current page, are the
// labels of its left / right neighbour on MM's box).
Gfx* MergeShip_Kaleido_DrawToPageLabel(Gfx* gfx, u16 page, s32 toRight) {
    static TexturePtr sMMToPageTextures[] = {
        gPauseToSelectItemENGTex,  // PAUSE_ITEM
        gPauseToMapENGTex,         // PAUSE_MAP
        gPauseToQuestStatusENGTex, // PAUSE_QUEST
        gPauseToMasksENGTex,       // PAUSE_MASK
    };
    u16 toPage = MergeShip_Pause_NextPage(page, toRight);

    if (MERGESHIP_PAUSE_IS_MM_PAGE(toPage)) {
        return Gfx_DrawTexQuad4b(gfx, sMMToPageTextures[toPage], G_IM_FMT_IA, 128, 16, 0);
    }
    if (toPage == MERGESHIP_PAUSE_OOT_ITEM) {
        return Gfx_DrawTexQuadIA8(gfx, sOoTPauseToSelectItemTex, 128, 16, 0);
    }
    return gfx;
}
