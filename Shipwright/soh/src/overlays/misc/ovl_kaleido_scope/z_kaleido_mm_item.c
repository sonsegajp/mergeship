#include "soh/MergeShip/MergeShipOwnership.h" // MergeShip2: each game's own counts and bottles
/*
 * MergeShip2 -- Pause Menu, "Items (Termina)" page (MERGESHIP_PAUSE_MM_ITEM, MergeShipPause.h): the Item face's
 * Majora's Mask view, switched to and from with C-Down (z_kaleido_mergeship.c MergeShip_Kaleido_UpdateWorldSwitch).
 *
 * Majora's Mask's item page (2ship2harkinian/mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_item.c:
 * KaleidoScope_DrawItemSelect, KaleidoScope_UpdateItemCursor, KaleidoScope_DrawAmmoCount) for the MM items that have
 * no slot in Ocarina of Time's full 24-slot grid. It is laid out as MM's page -- the same 6 x 4 grid, each item in
 * its MM slot -- and holds the Powder Keg (MM SLOT_POWDER_KEG), the Pictograph Box (SLOT_PICTOGRAPH_BOX) and MM's three
 * trade slots, which show the latest item of each chain as MM does (SLOT_TRADE_DEED: Moon's Tear .. Ocean Title Deed;
 * SLOT_TRADE_KEY_MAMA: Room Key / Letter to Mama; SLOT_TRADE_COUPLE: Letter to Kafei / Pendant of Memories).
 * MM's other slots hold items that live in Ocarina of Time's grid (bow, bombs, bottles ...) and stay empty here; MM
 * items other ports add without a Hyrule slot appear by adding their slot to MergeShip_MMItems_PageSlotItem
 * (MergeShipMMItems.c) -- this page needs no change.
 *
 * As on the Masks face, the items have no inventory slot: on a C or D-pad button they carry SLOT_NONE and are told
 * apart by item id (MergeShip_Kaleido_FinishMaskEquip lands them, z_kaleido_item.c).
 *
 * Not ported: MM's A button, which opens the item's description (itemDescriptionOn, MM texts 0x1700 + slot).
 * Ocarina of Time's pause menu has no description box; the name panel names the item.
 */

#include "z_kaleido_scope.h"
#include "textures/parameter_static/parameter_static.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipItems.h"
#include "soh/MergeShip/MergeShipPause.h"
#include "soh/MergeShip/MergeShipMMItems.h"
#include "soh/MergeShip/MergeShipMMEquipment.h" // MergeShip_MMEquip_Owns(MMEQUIP_BOW): MM's arrows need MM's bow
#include "soh/MergeShip/MergeShipPicto.h"
#include <string.h>
#include <libultraship/bridge/consolevariablebridge.h>

extern const char* _gAmmoDigit0Tex[];

// MM ITEM_NUM_SLOTS, 6 x 4 (the geometry of Ocarina of Time's item grid too).
#define MMITEM_NUM_SLOTS MERGESHIP_MM_ITEM_PAGE_SLOTS
#define MMITEM_GRID_COLUMNS 6
#define MMITEM_GRID_ROWS 4

// MM's page cursor (PauseContext::cursorPoint/XIndex/YIndex/Item/Slot[PAUSE_ITEM]); Ocarina of Time's PauseContext
// has no room for another page. MM opens its item page on SLOT_OCARINA (0).
const char* MergeShip_Kaleido_OoTItemNameTexture(u8 item);

static s16 sMMItemCursorPoint = 0;
static s16 sMMItemCursorX = 0;
static s16 sMMItemCursorY = 0;
static u16 sMMItemCursorItem = PAUSE_ITEM_NONE;

static u8 MergeShip_Kaleido_MMItemInSlot(s16 slot) {
    if ((slot < 0) || (slot >= MMITEM_NUM_SLOTS)) {
        return ITEM_NONE;
    }
    return MergeShip_MMItems_PageSlotItem(slot);
}

// MM gPlayerFormItemRestrictions[GET_PLAYER_FORM][item]: the MM forms' table (MergeShip_FormCanUseItem); MM's human
// column for Link of either age, where only the keg is the Goron's.
static s32 MergeShip_Kaleido_FormMayUseMMItem(u8 item) {
    u8 form = MergeShip_GetPlayerForm();

    if (!MERGESHIP_FORM_IS_MM(form)) {
        return item != ITEM_MM_POWDER_KEG;
    }
    return MergeShip_FormCanUseItem(form, item);
}

// MM Player_GetCurMaskItemId, for the rule that a worn mask cannot be taken off its button.
static u8 MergeShip_Kaleido_MMItemWornMask(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (PLAYER_MASK_IS_MM_TRANSFORMATION(player->currentMask)) {
        return ITEM_MM_MASK_FIERCE_DEITY + (player->currentMask - PLAYER_MASK_MM_FIERCE_DEITY);
    }
    if (PLAYER_MASK_IS_MM_REGULAR(player->currentMask)) {
        return ITEM_MM_MASK_TRUTH + (PLAYER_MASK_TO_MM_MASK(player->currentMask) - MM_MASK_TRUTH);
    }
    if ((player->currentMask != PLAYER_MASK_NONE) && (player->currentMask < PLAYER_MASK_MAX)) {
        return ITEM_MASK_KEATON + player->currentMask - PLAYER_MASK_KEATON;
    }
    return ITEM_NONE;
}

// MM sPlayerFormItems: the mask that is the current form (it cannot be taken off its button).
static u8 MergeShip_Kaleido_MMItemFormMask(void) {
    switch (MergeShip_GetPlayerForm()) {
        case PLAYER_FORM_DEKU:
            return ITEM_MM_MASK_DEKU;
        case PLAYER_FORM_GORON:
            return ITEM_MM_MASK_GORON;
        case PLAYER_FORM_ZORA:
            return ITEM_MM_MASK_ZORA;
        case PLAYER_FORM_FIERCE_DEITY:
            return ITEM_MM_MASK_FIERCE_DEITY;
        default:
            return ITEM_NONE;
    }
}

static void MergeShip_Kaleido_MMItemSetQuad(Vtx* vtx, s16 x, s16 y, s16 w, s16 h, u8 alpha) {
    s32 k;

    vtx[0].v.ob[0] = vtx[2].v.ob[0] = x;
    vtx[1].v.ob[0] = vtx[3].v.ob[0] = x + w;
    vtx[0].v.ob[1] = vtx[1].v.ob[1] = y;
    vtx[2].v.ob[1] = vtx[3].v.ob[1] = y - h;
    for (k = 0; k < 4; k++) {
        vtx[k].v.ob[2] = 0;
        vtx[k].v.flag = 0;
        vtx[k].v.cn[0] = vtx[k].v.cn[1] = vtx[k].v.cn[2] = 255;
        vtx[k].v.cn[3] = alpha;
    }
    vtx[0].v.tc[0] = vtx[0].v.tc[1] = vtx[1].v.tc[1] = vtx[2].v.tc[0] = 0;
    vtx[1].v.tc[0] = vtx[2].v.tc[1] = vtx[3].v.tc[0] = vtx[3].v.tc[1] = w << 5;
}

// 24 slots (Ocarina of Time's item-grid geometry, KaleidoScope_InitVertices), one "equipped" outline per item
// button, and two ammo digits (MM draws the keg's and the Pictograph Box's counts).
#define MMITEM_VTX_OUTLINES MMITEM_NUM_SLOTS
#define MMITEM_VTX_DIGITS (MMITEM_VTX_OUTLINES + 7)
#define MMITEM_VTX_COUNT (MMITEM_VTX_DIGITS + 2)

static Vtx* MergeShip_Kaleido_BuildMMItemVtx(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Vtx* vtx = Graph_Alloc(play->state.gfxCtx, MMITEM_VTX_COUNT * 4 * sizeof(Vtx));
    s32 i;

    for (i = 0; i < MMITEM_NUM_SLOTS; i++) {
        s16 x = -96 + (i % MMITEM_GRID_COLUMNS) * 32 + 2;
        s16 y = 58 - (i / MMITEM_GRID_COLUMNS) * 32 + pauseCtx->offsetY - 2;

        MergeShip_Kaleido_MMItemSetQuad(&vtx[i * 4], x, y, 28, 28, 255);
        // BUG-0037 / BUG-0040: the whole 32x32 icon in the 28-unit cell, as Ocarina of Time's grid does
        // (KaleidoScope_InitVertices: 0x1C wide, tc 32 << 5) -- with tc 28 << 5 each icon lost its right and bottom
        // four texels.
        vtx[i * 4 + 1].v.tc[0] = vtx[i * 4 + 2].v.tc[1] = vtx[i * 4 + 3].v.tc[0] = vtx[i * 4 + 3].v.tc[1] = 32 << 5;
    }
    for (i = MMITEM_VTX_OUTLINES; i < MMITEM_VTX_COUNT; i++) {
        MergeShip_Kaleido_MMItemSetQuad(&vtx[i * 4], -300, 300, 32, 32, pauseCtx->alpha);
    }
    return vtx;
}

// MM KaleidoScope_UpdateItemCursor, cursor movement: the cursor rests on empty slots too; leaving a page edge
// searches the columns for an item.
static void MergeShip_Kaleido_UpdateMMItemCursor(PlayState* play) {
    Input* input = &play->state.input[0];
    PauseContext* pauseCtx = &play->pauseCtx;
    bool dpad = (CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) && !CHECK_BTN_ALL(input->cur.button, BTN_CUP));
    s16 oldCursorPoint = sMMItemCursorPoint;
    s16 moveCursorResult = 0; // MM PAUSE_CURSOR_RESULT_NONE / _SLOT (1) / _SPECIAL_POS (2)
    s16 cursorPoint;
    s16 cursorX;
    s16 cursorY;
    s32 left = (pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT));
    s32 right = (pauseCtx->stickRelX > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT));
    s32 up = (pauseCtx->stickRelY > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DUP));
    s32 down = (pauseCtx->stickRelY < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DDOWN));

    if (pauseCtx->cursorSpecialPos == 0) {
        if (left || right) {
            cursorPoint = sMMItemCursorPoint;
            cursorX = sMMItemCursorX;
            cursorY = sMMItemCursorY;

            while (moveCursorResult == 0) {
                if (left) {
                    if (sMMItemCursorX != 0) {
                        sMMItemCursorX--;
                        sMMItemCursorPoint--;
                        moveCursorResult = 1;
                    } else {
                        sMMItemCursorX = cursorX;
                        sMMItemCursorY++;
                        if (sMMItemCursorY >= MMITEM_GRID_ROWS) {
                            sMMItemCursorY = 0;
                        }
                        sMMItemCursorPoint = sMMItemCursorX + (sMMItemCursorY * MMITEM_GRID_COLUMNS);
                        if (sMMItemCursorPoint >= MMITEM_NUM_SLOTS) {
                            sMMItemCursorPoint = sMMItemCursorX;
                        }
                        if (cursorY == sMMItemCursorY) {
                            sMMItemCursorX = cursorX;
                            sMMItemCursorPoint = cursorPoint;
                            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
                            moveCursorResult = 2;
                        }
                    }
                } else {
                    if (sMMItemCursorX <= 4) {
                        sMMItemCursorX++;
                        sMMItemCursorPoint++;
                        moveCursorResult = 1;
                    } else {
                        sMMItemCursorX = cursorX;
                        sMMItemCursorY++;
                        if (sMMItemCursorY >= MMITEM_GRID_ROWS) {
                            sMMItemCursorY = 0;
                        }
                        sMMItemCursorPoint = sMMItemCursorX + (sMMItemCursorY * MMITEM_GRID_COLUMNS);
                        if (sMMItemCursorPoint >= MMITEM_NUM_SLOTS) {
                            sMMItemCursorPoint = sMMItemCursorX;
                        }
                        if (cursorY == sMMItemCursorY) {
                            sMMItemCursorX = cursorX;
                            sMMItemCursorPoint = cursorPoint;
                            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                            moveCursorResult = 2;
                        }
                    }
                }
            }
        }
    } else if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
        if (right) {
            // MM KaleidoScope_MoveCursorFromSpecialPos, then search down each column from the top-left slot.
            pauseCtx->nameDisplayTimer = 0;
            pauseCtx->cursorSpecialPos = 0;
            Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            cursorX = cursorY = cursorPoint = 0;
            while (true) {
                if (MergeShip_Kaleido_MMItemInSlot(cursorPoint) != ITEM_NONE) {
                    sMMItemCursorPoint = cursorPoint;
                    sMMItemCursorX = cursorX;
                    sMMItemCursorY = cursorY;
                    moveCursorResult = 1;
                    break;
                }
                cursorY++;
                cursorPoint += MMITEM_GRID_COLUMNS;
                if (cursorY < MMITEM_GRID_ROWS) {
                    continue;
                }
                cursorY = 0;
                cursorPoint = cursorX + 1;
                cursorX = cursorPoint;
                if (cursorX < MMITEM_GRID_COLUMNS) {
                    continue;
                }
                KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                break;
            }
        }
    } else if (left) { // PAUSE_CURSOR_PAGE_RIGHT
        pauseCtx->nameDisplayTimer = 0;
        pauseCtx->cursorSpecialPos = 0;
        Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        cursorX = cursorPoint = MMITEM_GRID_COLUMNS - 1; // MM: the top-right slot, SLOT_TRADE_DEED
        cursorY = 0;
        while (true) {
            if (MergeShip_Kaleido_MMItemInSlot(cursorPoint) != ITEM_NONE) {
                sMMItemCursorPoint = cursorPoint;
                sMMItemCursorX = cursorX;
                sMMItemCursorY = cursorY;
                moveCursorResult = 1;
                break;
            }
            cursorY++;
            cursorPoint += MMITEM_GRID_COLUMNS;
            if (cursorY < MMITEM_GRID_ROWS) {
                continue;
            }
            cursorY = 0;
            cursorPoint = cursorX - 1;
            cursorX = cursorPoint;
            if (cursorX >= 0) {
                continue;
            }
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
            break;
        }
    }

    if (pauseCtx->cursorSpecialPos == 0) {
        if (up) {
            if (sMMItemCursorY != 0) {
                sMMItemCursorY--;
                sMMItemCursorPoint -= MMITEM_GRID_COLUMNS;
            }
        } else if (down) {
            if (sMMItemCursorY < MMITEM_GRID_ROWS - 1) {
                sMMItemCursorY++;
                sMMItemCursorPoint += MMITEM_GRID_COLUMNS;
            }
        }

        sMMItemCursorItem = MergeShip_Kaleido_MMItemInSlot(sMMItemCursorPoint);
        pauseCtx->cursorColorSet = 4; // MM PAUSE_CURSOR_COLOR_SET_YELLOW
        if (sMMItemCursorItem == ITEM_NONE) {
            sMMItemCursorItem = PAUSE_ITEM_NONE;
            pauseCtx->cursorColorSet = 0; // MM PAUSE_CURSOR_COLOR_SET_WHITE
        }
    } else {
        sMMItemCursorItem = PAUSE_ITEM_NONE;
    }

    if (oldCursorPoint != sMMItemCursorPoint) {
        Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

// MM KaleidoScope_UpdateItemCursor, "Equip item to the C buttons" (and SoH's D-pad): MM equips any item on its page;
// only a worn mask, or the mask that is the current form, keeps its button.
static void MergeShip_Kaleido_TryEquipMMItem(PlayState* play, Vtx* itemVtx) {
    Input* input = &play->state.input[0];
    PauseContext* pauseCtx = &play->pauseCtx;
    u16 buttons = BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT;
    s32 targetButton = 0; // gSaveContext.equips.buttonItems index
    u8 wornItem;
    u8 formItem;

    if ((sMMItemCursorItem == PAUSE_ITEM_NONE) || (pauseCtx->debugState != 0) || (pauseCtx->state != 6) ||
        (pauseCtx->unk_1E4 != 0)) {
        return;
    }
    if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) &&
        (!CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) || CHECK_BTN_ALL(input->cur.button, BTN_CUP))) {
        buttons |= BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT;
    }
    if (!CHECK_BTN_ANY(input->press.button, buttons)) {
        return;
    }

    // KaleidoScope_SetupItemEquip's equipTargetCBtn order, plus one.
    if (CHECK_BTN_ALL(input->press.button, BTN_CLEFT)) {
        targetButton = 1;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CDOWN)) {
        targetButton = 2;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CRIGHT)) {
        targetButton = 3;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
        targetButton = 4;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
        targetButton = 5;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        targetButton = 6;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
        targetButton = 7;
    }

    // MergeShip2: MM's ocarina plays in Hyrule too (the user, 2026-09-30: "I can't use MM ocarina in OOT"). Without a
    // Hyrule Ocarina of Time it lands on the button with no slot (MergeShip_MMItems_IsPageItem), and
    // MergeShip_MMItems_Owns keeps it there only while Termina owns it; with one, it is that slot's item. Refused only
    // when neither game holds it (the cell shows it only while Termina owns it, so this is a guard, not a rule).
    if ((sMMItemCursorItem == ITEM_OCARINA_TIME) && !MergeShip_MMItems_Owns(ITEM_OCARINA_TIME) &&
        (INV_CONTENT(ITEM_OCARINA_TIME) != ITEM_OCARINA_TIME)) {
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    // MergeShip2 (task 5): MM's magic arrows need MM's Hero's Bow first (a unified seed can find them out of order), as
    // Ocarina of Time's page asks its own bow for its arrows (z_kaleido_item.c).
    if (((sMMItemCursorItem == ITEM_ARROW_FIRE) || (sMMItemCursorItem == ITEM_ARROW_ICE) ||
         (sMMItemCursorItem == ITEM_ARROW_LIGHT)) &&
        !MergeShip_MMEquip_Owns(MMEQUIP_BOW)) {
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }

    wornItem = MergeShip_Kaleido_MMItemWornMask(play);
    formItem = MergeShip_Kaleido_MMItemFormMask();
    if (((wornItem != ITEM_NONE) && (wornItem == gSaveContext.equips.buttonItems[targetButton])) ||
        ((formItem != ITEM_NONE) && (formItem == gSaveContext.equips.buttonItems[targetButton]))) {
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }

    // Ocarina of Time's equip animation (the icon flies to the button); MergeShip_Kaleido_FinishMaskEquip lands it.
    // MergeShip2: an item held in Ocarina of Time's grid (ocarina, ammo items, bottles) is equipped through its slot.
    MergeShip_Own_OnPageEquip(sMMItemCursorItem, 1); // Majora's Mask's copy is the one in use
    KaleidoScope_SetupItemEquip(play, sMMItemCursorItem, MergeShip_MMItems_PageSlotOoTSlot(sMMItemCursorPoint),
                                itemVtx[sMMItemCursorPoint * 4].v.ob[0] * 10,
                                itemVtx[sMMItemCursorPoint * 4].v.ob[1] * 10);
}

// MM KaleidoScope_DrawAmmoCount for the keg (AMMO(ITEM_POWDER_KEG)) and the Pictograph Box (1 while a photo is kept).
static void MergeShip_Kaleido_DrawMMItemAmmo(PlayState* play, Vtx* itemVtx, s16 slot, u8 item) {
    PauseContext* pauseCtx = &play->pauseCtx;
    // BUG-0040: each count its own two digits' vertices -- the renderer reads vertices when the frame is drawn, so one
    // shared set left every count at the last item's place (only one count was ever seen).
    Vtx* digitVtx = Graph_Alloc(play->state.gfxCtx, 8 * sizeof(Vtx));
    s16 ammo;
    s16 upper;
    s16 x = itemVtx[slot * 4].v.ob[0];
    s16 y = itemVtx[slot * 4].v.ob[1] - 22;

    OPEN_DISPS(play->state.gfxCtx);

    if (item == ITEM_MM_PICTOGRAPH_BOX) {
        ammo = MergeShip_Picto_HasPhoto() ? 1 : 0;
    } else if (item == ITEM_MM_POWDER_KEG) {
        ammo = MergeShip_MMItems_GetKegAmmo();
    } else if (item == ITEM_MM_BOW_HERO) {
        ammo = MergeShip_Own_PageAmmo(ITEM_BOW, 1); // MergeShip2: Majora's Mask's own counts (MergeShipOwnership.h)
    } else {
        ammo = MergeShip_Own_PageAmmo(item, 1);
    }

    gDPPipeSync(POLY_OPA_DISP++);
    if (!MergeShip_Kaleido_FormMayUseMMItem(item)) {
        // MM: the ammo of a restricted item.
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 100, 100, 100, pauseCtx->alpha);
    } else if (ammo == 0) {
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 130, 130, 130, pauseCtx->alpha);
    } else if ((ammo == 1) && ((item == ITEM_MM_POWDER_KEG) || (item == ITEM_MM_PICTOGRAPH_BOX))) {
        // MM: the keg and the box at capacity (1).
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 120, 255, 0, pauseCtx->alpha);
    } else {
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    }

    for (upper = 0; ammo >= 10; upper++) {
        ammo -= 10;
    }

    if (upper != 0) {
        MergeShip_Kaleido_MMItemSetQuad(&digitVtx[0], x, y, 8, 8, pauseCtx->alpha);
        gSPVertex(POLY_OPA_DISP++, &digitVtx[0], 4, 0);
        gDPLoadTextureBlock(POLY_OPA_DISP++, ((u8*)_gAmmoDigit0Tex[upper]), G_IM_FMT_IA, G_IM_SIZ_8b, 8, 8, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                            G_TX_NOLOD);
        gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
    }

    MergeShip_Kaleido_MMItemSetQuad(&digitVtx[4], x + 6, y, 8, 8, pauseCtx->alpha);
    gSPVertex(POLY_OPA_DISP++, &digitVtx[4], 4, 0);
    gDPLoadTextureBlock(POLY_OPA_DISP++, ((u8*)_gAmmoDigit0Tex[ammo]), G_IM_FMT_IA, G_IM_SIZ_8b, 8, 8, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);

    CLOSE_DISPS(play->state.gfxCtx);
}

// The slot showing `item`, or -1.
static s16 MergeShip_Kaleido_SlotOfMMItem(u8 item) {
    s16 slot;

    if (item == ITEM_NONE) {
        return -1;
    }
    for (slot = 0; slot < MMITEM_NUM_SLOTS; slot++) {
        if (MergeShip_Kaleido_MMItemInSlot(slot) == item) {
            return slot;
        }
    }
    return -1;
}

// MM KaleidoScope_DrawItemSelect.
void MergeShip_Kaleido_DrawMMItemSelect(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Vtx* itemVtx = MergeShip_Kaleido_BuildMMItemVtx(play);
    s32 isCurrent = (pauseCtx->pageIndex == MERGESHIP_PAUSE_MM_ITEM);
    s32 i;
    s32 j;

    OPEN_DISPS(play->state.gfxCtx);

    if (isCurrent) {
        pauseCtx->cursorColorSet = 0;
        pauseCtx->nameColorSet = 0;

        if ((pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0)) {
            MergeShip_Kaleido_UpdateMMItemCursor(play);
            // MergeShip2: A on a trade slot (the title deeds, Room Key / Letter to Mama, Letter to Kafei / Pendant)
            // shows the next one owned (the user, 2026-09-24).
            if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) && (pauseCtx->cursorSpecialPos == 0) &&
                MergeShip_MMItems_TradeCycle(play, sMMItemCursorPoint)) {
                Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                sMMItemCursorItem = MergeShip_MMItems_PageSlotItem(sMMItemCursorPoint);
            }
            MergeShip_Kaleido_TryEquipMMItem(play, itemVtx);
        } else if (pauseCtx->unk_1E4 == 3) {
            pauseCtx->cursorColorSet = 4; // equipping
        }
        KaleidoScope_SetCursorVtx(pauseCtx, sMMItemCursorPoint * 4, itemVtx);
    }

    Gfx_SetupDL_42Opa(play->state.gfxCtx);

    // MM: a white box around the items that are on the C buttons (and SoH's D-pad).
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    for (i = 1; i < 8; i++) {
        s16 slot;
        Vtx* outline = &itemVtx[(MMITEM_VTX_OUTLINES + i - 1) * 4];

        if ((i >= 4) && !CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0)) {
            break;
        }
        if (gSaveContext.equips.cButtonSlots[i - 1] != SLOT_NONE) {
            // An Ocarina of Time slot's item: its cell here, if this page shows it (ocarina, ammo items, bottles).
            for (slot = 0; slot < MMITEM_NUM_SLOTS; slot++) {
                if (MergeShip_MMItems_PageSlotOoTSlot(slot) == gSaveContext.equips.cButtonSlots[i - 1]) {
                    break;
                }
            }
            if (slot >= MMITEM_NUM_SLOTS) {
                continue;
            }
        } else {
            slot = MergeShip_Kaleido_SlotOfMMItem(gSaveContext.equips.buttonItems[i]);
        }
        if (slot < 0) {
            continue;
        }
        MergeShip_Kaleido_MMItemSetQuad(outline, itemVtx[slot * 4].v.ob[0] - 2, itemVtx[slot * 4].v.ob[1] + 2, 32, 32,
                                        pauseCtx->alpha);
        gSPVertex(POLY_OPA_DISP++, outline, 4, 0);
        POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, gEquippedItemOutlineTex, 32, 32, 0);
    }

    gDPPipeSync(POLY_OPA_DISP++);

    // MM: the item icons; the one under the cursor is drawn larger when the current form may use it, and an item the
    // form may not use is drawn grey (2S2H's gSPGrayscale, as SoH's own item page).
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (i = 0, j = 0; i < MMITEM_NUM_SLOTS; i++, j += 4) {
        u8 item = MergeShip_Kaleido_MMItemInSlot(i);
        s32 restricted;

        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
        if (item == ITEM_NONE) {
            continue;
        }
        restricted = !MergeShip_Kaleido_FormMayUseMMItem(item);
        if (isCurrent && (pauseCtx->unk_1E4 == 0) && (pauseCtx->cursorSpecialPos == 0) && !restricted &&
            (i == sMMItemCursorPoint)) {
            MergeShip_Kaleido_MMItemSetQuad(&itemVtx[j], itemVtx[j].v.ob[0] - 2, itemVtx[j].v.ob[1] + 2, 32, 32, 255);
        }
        if (restricted) {
            gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
            gSPGrayscale(POLY_OPA_DISP++, true);
        }
        gSPVertex(POLY_OPA_DISP++, &itemVtx[j], 4, 0);
        KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIcons[item], 32, 32, 0);
        if (restricted) {
            gSPGrayscale(POLY_OPA_DISP++, false);
        }
    }

    // MM: the ammo digits (the keg's and the Pictograph Box's), while the page is being looked at.
    if (isCurrent && (pauseCtx->state == 6) && ((pauseCtx->unk_1E4 == 0) || (pauseCtx->unk_1E4 == 3))) {
        Gfx_SetupDL_39Opa(play->state.gfxCtx);
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        for (i = 0; i < MMITEM_NUM_SLOTS; i++) {
            u8 item = MergeShip_Kaleido_MMItemInSlot(i);

            if ((item == ITEM_MM_POWDER_KEG) || (item == ITEM_MM_PICTOGRAPH_BOX) || (item == ITEM_MM_BOW_HERO) ||
                (item == ITEM_BOMB) || (item == ITEM_BOMBCHU) || (item == ITEM_STICK) || (item == ITEM_NUT) ||
                (item == ITEM_BEAN)) {
                MergeShip_Kaleido_DrawMMItemAmmo(play, itemVtx, i, item);
                // The two digits' vertices are reused: flush before the next item's.
                gDPPipeSync(POLY_OPA_DISP++);
            }
        }
        Gfx_SetupDL_42Opa(play->state.gfxCtx);
    }

    if (isCurrent && (pauseCtx->cursorSpecialPos == 0)) {
        KaleidoScope_DrawCursor(play, MERGESHIP_PAUSE_MM_ITEM);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// KaleidoScope_UpdateNamePanel for the Items (Termina) face (MM item names from mm.o2r's item_name_static).
void MergeShip_Kaleido_UpdateMMItemNamePanel(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    if (pauseCtx->namedItem != sMMItemCursorItem) {
        const char* texture;

        pauseCtx->namedItem = sMMItemCursorItem;
        texture = (sMMItemCursorItem != PAUSE_ITEM_NONE) ? MergeShip_MMItems_NameTexture((u8)sMMItemCursorItem) : NULL;
        if ((texture == NULL) && (sMMItemCursorItem != PAUSE_ITEM_NONE)) {
            // BUG-0043: an Ocarina of Time item here (an MM bottle refilled with Bugs, a Fish ...): its own name.
            texture = MergeShip_Kaleido_OoTItemNameTexture((u8)sMMItemCursorItem);
        }
        if (texture != NULL) {
            memcpy(pauseCtx->nameSegment, texture, strlen(texture) + 1);
        } else {
            pauseCtx->namedItem = PAUSE_ITEM_NONE;
        }
        pauseCtx->nameDisplayTimer = 0;
    } else if (pauseCtx->nameColorSet == 0) {
        // As the Select Item page.
        pauseCtx->nameDisplayTimer++;
        if (pauseCtx->nameDisplayTimer > WREG(88)) {
            pauseCtx->nameDisplayTimer = 0;
        }
    } else {
        pauseCtx->nameDisplayTimer = 0;
    }
}

u16 MergeShip_Kaleido_GetMMItemCursorItem(void) {
    return sMMItemCursorItem;
}
