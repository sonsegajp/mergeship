// MergeShip2 -- "Termina Equipment": Majora's Mask's swords and shields, on a view of 2Ship's Quest face. See
// MergeShipTerminaEquip.h.
//
// The port of SoH's Termina view of its Equipment page (Shipwright soh/src/overlays/misc/ovl_kaleido_scope/
// z_kaleido_mergeship.c, "The Termina equipment view"): the same grid, cells, cursor, A, sounds, names and log line.
// It is laid out on Ocarina of Time's equipment grid (OoT KaleidoScope_InitVertices' equipVtx: a column left of where
// Ocarina of Time draws Link, three right of it), on Ocarina of Time's equipment page panels in Majora's Mask's page
// colour, with Majora's Mask's icons, item names and cursor:
//   left column:  the Quiver and the Bomb Bag (MM's icons of the capacities; shown, not equipped);
//   first row:    the Kokiri Sword, the Razor Sword, the Gilded Sword;
//   second row:   the Hero's Shield, the Mirror Shield;
//   third row:    the Great Fairy's Sword.
// Only what Termina holds is shown. A equips the item under the cursor (MergeShip_TerminaEquip_Equip; MM's error sound
// when it can't be now), and the equipped outline marks the sword and shield in use -- none while an Ocarina of Time
// sword / shield is (Hyrule's equipment view outlines that one), so the two views agree. What can't be used now is
// drawn grey with a grey name. Majora's Mask has no pause Link to draw (Ocarina of Time's page draws Link's picture
// between the columns), so that place stays empty, as on Hyrule's equipment view (MergeShipOoTEquipPause.c).
//
// It is a view of the Quest face (MM has no Equipment face): C-Up reaches it after Hyrule's equipment and leaves it for
// Termina's quest status; B goes back to Termina's quest status (z_kaleido_mergeship.c MergeShip_QuestPage_*).

#include "global.h"
#include "2s2h/MergeShip/MergeShipTerminaEquip.h"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipOoTEquipAssets.h"
#include "2s2h/MergeShip/MergeShipPauseLink.h"
#include "archives/icon_item_static/icon_item_static_yar.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"

#define OOT_EQUIP (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip)

void MergeShipTest_Log(const char* fmt, ...);

#define TE_COLS 4
#define TE_ROWS 4
#define TE_CELLS (TE_COLS * TE_ROWS)
#define TE_CELL(row, col) ((row) * TE_COLS + (col))

// Page coordinates of the cells' top-left corners: Ocarina of Time's equipment grid (OoT KaleidoScope_InitVertices,
// D_8082B12C and rows of 32 from 58), as SoH's view and Hyrule's equipment view. MM's page coordinates are the same.
static const s16 sCellX[TE_COLS] = { -114, 12, 44, 76 };
static const s16 sCellY[TE_ROWS] = { 58, 26, -6, -38 };

// The equipment each cell holds (ITEM_NONE: an empty cell; the left column's are the upgrades, by their first item).
static const u8 sCells[TE_CELLS] = {
    ITEM_QUIVER_30,   ITEM_SWORD_KOKIRI,      ITEM_SWORD_RAZOR,  ITEM_SWORD_GILDED,
    ITEM_BOMB_BAG_20, ITEM_SHIELD_HERO,       ITEM_SHIELD_MIRROR, ITEM_NONE,
    ITEM_NONE,        ITEM_SWORD_GREAT_FAIRY, ITEM_NONE,          ITEM_NONE,
    ITEM_NONE,        ITEM_NONE,              ITEM_NONE,          ITEM_NONE,
};

// MM's icons of the capacities (icon_item_static, as MM's Quest page draws them).
static TexturePtr sQuiverIcons[3] = { (TexturePtr)gItemIconQuiver30Tex, (TexturePtr)gItemIconQuiver40Tex,
                                      (TexturePtr)gItemIconQuiver50Tex };
static TexturePtr sBombBagIcons[3] = { (TexturePtr)gItemIconBombBag20Tex, (TexturePtr)gItemIconBombBag30Tex,
                                       (TexturePtr)gItemIconBombBag40Tex };

static s32 sOpen = false;
static s16 sCursor = TE_CELL(0, 1);
static Vtx sCellVtx[TE_CELLS * 4];    // the items, 28 x 28 inside their cell (the one under the cursor 32 x 32)
static Vtx sOutlineVtx[TE_CELLS * 4]; // the equipped outline, the cell

s32 MergeShip_TerminaEquipPause_IsOpen(void) {
    return sOpen;
}

void MergeShip_TerminaEquipPause_Close(void) {
    sOpen = false;
}

// ---- cells ----------------------------------------------------------------------------------------------------------------

// The item a cell shows now (ITEM_NONE: nothing): the upgrades by their capacity, the swords and shields when held.
static u8 MsTeqP_CellItem(s16 cell) {
    u8 item = sCells[cell];
    s32 upg;

    switch (item) {
        case ITEM_NONE:
            return ITEM_NONE;
        case ITEM_QUIVER_30:
            upg = CUR_UPG_VALUE(UPG_QUIVER);
            return (upg != 0) ? (u8)(ITEM_QUIVER_30 + CLAMP_MAX(upg, 3) - 1) : ITEM_NONE;
        case ITEM_BOMB_BAG_20:
            upg = CUR_UPG_VALUE(UPG_BOMB_BAG);
            return (upg != 0) ? (u8)(ITEM_BOMB_BAG_20 + CLAMP_MAX(upg, 3) - 1) : ITEM_NONE;
        default:
            return MergeShip_TerminaEquip_Owns(item) ? item : ITEM_NONE;
    }
}

// A cell holds equipment A equips (not an upgrade).
static s32 MsTeqP_IsEquipCell(s16 cell) {
    return (cell % TE_COLS) != 0;
}

// The item can be used now (an upgrade always; a sword or shield by MergeShip_TerminaEquip_CanEquip).
static s32 MsTeqP_Usable(s16 cell, u8 item) {
    return !MsTeqP_IsEquipCell(cell) || MergeShip_TerminaEquip_CanEquip(item);
}

// MM's icon (32 x 32 RGBA32).
static TexturePtr MsTeqP_Icon(u8 item) {
    if ((item >= ITEM_QUIVER_30) && (item <= ITEM_QUIVER_50)) {
        return sQuiverIcons[item - ITEM_QUIVER_30];
    }
    if ((item >= ITEM_BOMB_BAG_20) && (item <= ITEM_BOMB_BAG_40)) {
        return sBombBagIcons[item - ITEM_BOMB_BAG_20];
    }
    return gItemIcons[item];
}

// The first cell with something in it, scanning the columns from `fromCol` in the direction `step` (each from the top),
// or -1 (SoH MergeShip_EquipTermina_ScanColumns).
static s16 MsTeqP_ScanColumns(s16 fromCol, s16 step) {
    s16 col;
    s16 row;

    for (col = fromCol; (col >= 0) && (col < TE_COLS); col += step) {
        for (row = 0; row < TE_ROWS; row++) {
            if (MsTeqP_CellItem(TE_CELL(row, col)) != ITEM_NONE) {
                return TE_CELL(row, col);
            }
        }
    }
    return -1;
}

// The cell with something in it of row `row` nearest to column `col`, or -1.
static s16 MsTeqP_NearestInRow(s16 row, s16 col) {
    s16 d;

    for (d = 0; d < TE_COLS; d++) {
        if ((col - d >= 0) && (MsTeqP_CellItem(TE_CELL(row, col - d)) != ITEM_NONE)) {
            return TE_CELL(row, col - d);
        }
        if ((col + d < TE_COLS) && (MsTeqP_CellItem(TE_CELL(row, col + d)) != ITEM_NONE)) {
            return TE_CELL(row, col + d);
        }
    }
    return -1;
}

// Where the cursor lands when the view is switched to: the sword in use, else the first sword or shield held.
static void MsTeqP_PlaceCursor(void) {
    s16 cell;

    for (cell = 0; cell < TE_CELLS; cell++) {
        u8 item = MsTeqP_CellItem(cell);

        if (MsTeqP_IsEquipCell(cell) && (cell / TE_COLS != 1) && (item != ITEM_NONE) &&
            MergeShip_TerminaEquip_IsEquipped(item)) {
            sCursor = cell;
            return;
        }
    }
    cell = MsTeqP_ScanColumns(1, 1);
    if (cell < 0) {
        cell = MsTeqP_ScanColumns(0, 1);
    }
    if (cell >= 0) {
        sCursor = cell;
    }
}

// The cursor's item, colour and name colour (SoH's view: the cursor blue on equipment A equips, the name grey on what
// can't be used now).
static void MsTeqP_SetCursorItem(PauseContext* pauseCtx) {
    u8 item = (pauseCtx->cursorSpecialPos == 0) ? MsTeqP_CellItem(sCursor) : ITEM_NONE;

    pauseCtx->cursorItem[PAUSE_QUEST] = (item != ITEM_NONE) ? item : PAUSE_ITEM_NONE;
    pauseCtx->cursorColorSet = ((item != ITEM_NONE) && MsTeqP_IsEquipCell(sCursor)) ? PAUSE_CURSOR_COLOR_SET_BLUE
                                                                                     : PAUSE_CURSOR_COLOR_SET_WHITE;
    pauseCtx->nameColorSet = ((item != ITEM_NONE) && !MsTeqP_Usable(sCursor, item)) ? PAUSE_NAME_COLOR_SET_GREY
                                                                                     : PAUSE_NAME_COLOR_SET_WHITE;
}

void MergeShip_TerminaEquipPause_Open(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    if (!MergeShip_TerminaEquip_Enabled()) {
        return;
    }
    MergeShip_TerminaEquip_Observe(); // what MM holds this very frame
    sOpen = true;
    MsTeqP_PlaceCursor();
    pauseCtx->cursorShrinkRate = 4.0f;
    MsTeqP_SetCursorItem(pauseCtx);
    Interface_SetAButtonDoAction(play, DO_ACTION_DECIDE);
    MergeShipTest_Log("TERMINAEQUIP open cell=%d swords=%X shields=%X", sCursor, OOT_EQUIP.terminaSwords,
                      OOT_EQUIP.terminaShields);
}

// ---- the cursor and A (SoH MergeShip_EquipTermina_UpdateCursor, with MM's stick and page arrows) --------------------------

s32 MergeShip_TerminaEquipPause_UpdateQuestCursor(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = CONTROLLER1(&play->state);
    s32 left;
    s32 right;
    s32 up;
    s32 down;
    s16 row;
    s16 col;
    s16 next = -1;
    s16 i;
    s32 fromArrow = (play->pauseCtx.cursorSpecialPos != 0);
    u8 item;

    if (!MergeShip_TerminaEquip_Enabled()) {
        sOpen = false;
        return false;
    }
    if (!sOpen) {
        return false;
    }
    if ((pauseCtx->state != PAUSE_STATE_MAIN) || (pauseCtx->mainState != PAUSE_MAIN_STATE_IDLE) ||
        (pauseCtx->pageIndex != PAUSE_QUEST) || pauseCtx->itemDescriptionOn) {
        MsTeqP_SetCursorItem(pauseCtx);
        return true;
    }

    left = pauseCtx->stickAdjX < -30;
    right = pauseCtx->stickAdjX > 30;
    up = pauseCtx->stickAdjY > 30;
    down = pauseCtx->stickAdjY < -30;
    row = sCursor / TE_COLS;
    col = sCursor % TE_COLS;

    if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
        // Back from the left page arrow onto the first column with something in it.
        if (right) {
            next = MsTeqP_ScanColumns(0, 1);
            if (next >= 0) {
                KaleidoScope_MoveCursorFromSpecialPos(play);
            }
        }
    } else if (pauseCtx->cursorSpecialPos != 0) {
        if (left) {
            next = MsTeqP_ScanColumns(TE_COLS - 1, -1);
            if (next >= 0) {
                KaleidoScope_MoveCursorFromSpecialPos(play);
            }
        }
    } else if (left || right) {
        s16 step = left ? -1 : 1;

        for (i = col + step; (i >= 0) && (i < TE_COLS); i += step) {
            if (MsTeqP_CellItem(TE_CELL(row, i)) != ITEM_NONE) {
                next = TE_CELL(row, i);
                break;
            }
        }
        if (next < 0) {
            // Off the page's edge: MM's page arrow.
            KaleidoScope_MoveCursorToSpecialPos(play, left ? PAUSE_CURSOR_PAGE_LEFT : PAUSE_CURSOR_PAGE_RIGHT);
        }
    } else if (up || down) {
        s16 step = up ? -1 : 1;

        for (i = row + step; (i >= 0) && (i < TE_ROWS); i += step) {
            next = MsTeqP_NearestInRow(i, col);
            if (next >= 0) {
                break;
            }
        }
    }
    if (next >= 0) {
        // (Off a page arrow, KaleidoScope_MoveCursorFromSpecialPos has played MM's cursor sound.)
        if (!fromArrow && (next != sCursor)) {
            pauseCtx->cursorShrinkRate = 4.0f;
            Audio_PlaySfx(NA_SE_SY_CURSOR);
        }
        sCursor = next;
    }

    if (gSaveContext.buttonStatus[EQUIP_SLOT_A] == BTN_DISABLED) {
        gSaveContext.buttonStatus[EQUIP_SLOT_A] = BTN_ENABLED;
        gSaveContext.hudVisibility = HUD_VISIBILITY_IDLE;
        Interface_SetHudVisibility(HUD_VISIBILITY_ALL);
    }

    item = (pauseCtx->cursorSpecialPos == 0) ? MsTeqP_CellItem(sCursor) : ITEM_NONE;

    // A equips a sword or shield; MM's error sound when it can't be now. (B, back to Termina's quest status, is taken
    // before the pause screen's own B: MergeShip_QuestPage_PreUpdate.)
    if ((item != ITEM_NONE) && MsTeqP_IsEquipCell(sCursor) && CHECK_BTN_ALL(input->press.button, BTN_A)) {
        if (MergeShip_TerminaEquip_Equip(play, item)) {
            Audio_PlaySfx(NA_SE_SY_DECIDE);
            MergeShipTest_Log("TERMINAEQUIP A cell=%d item=0x%02X sword=%d shield=%d B=0x%02X ootSword=%d "
                              "ootShield=%d razor=%d",
                              sCursor, item, GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD),
                              GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD), BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B),
                              OOT_EQUIP.sword, OOT_EQUIP.shield, gSaveContext.save.saveInfo.playerData.swordHealth);
        } else {
            Audio_PlaySfx(NA_SE_SY_ERROR);
            MergeShipTest_Log("TERMINAEQUIP A cell=%d item=0x%02X refused (form=%d)", sCursor, item, GET_PLAYER_FORM);
        }
    }

    MsTeqP_SetCursorItem(pauseCtx);
    Interface_SetAButtonDoAction(play, DO_ACTION_DECIDE);
    return true;
}

// ---- drawing ----------------------------------------------------------------------------------------------------------------

static void MsTeqP_SetQuad(Vtx* v, s16 x, s16 y, s16 size, u8 alpha) {
    s32 i;

    for (i = 0; i < 4; i++) {
        v[i].v.ob[0] = x + ((i & 1) ? size : 0);
        v[i].v.ob[1] = y - ((i & 2) ? size : 0);
        v[i].v.ob[2] = 0;
        v[i].v.flag = 0;
        v[i].v.tc[0] = (i & 1) ? (32 << 5) : 0; // the 32 x 32 texture over the quad
        v[i].v.tc[1] = (i & 2) ? (32 << 5) : 0;
        v[i].v.cn[0] = v[i].v.cn[1] = v[i].v.cn[2] = 255;
        v[i].v.cn[3] = alpha;
    }
}

s32 MergeShip_TerminaEquipPause_DrawQuestPage(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 isCurrent = (pauseCtx->pageIndex == PAUSE_QUEST);
    s16 cell;

    if (!sOpen || !MergeShip_TerminaEquip_Enabled()) {
        return false;
    }

    // OoT KaleidoScope_InitVertices: the items 28 x 28 inside their cell, the equipped outline the 32 x 32 cell; the
    // item under the cursor drawn at 32 x 32 when it can be used (OoT KaleidoScope_DrawEquipment).
    for (cell = 0; cell < TE_CELLS; cell++) {
        s16 x = sCellX[cell % TE_COLS];
        s16 y = sCellY[cell / TE_COLS] + pauseCtx->offsetY;
        u8 item = MsTeqP_CellItem(cell);

        if (isCurrent && (cell == sCursor) && (pauseCtx->cursorSpecialPos == 0) && (item != ITEM_NONE) &&
            MsTeqP_Usable(cell, item)) {
            MsTeqP_SetQuad(&sCellVtx[cell * 4], x, y, 32, pauseCtx->alpha);
        } else {
            MsTeqP_SetQuad(&sCellVtx[cell * 4], x + 2, y - 2, 28, pauseCtx->alpha);
        }
        MsTeqP_SetQuad(&sOutlineVtx[cell * 4], x, y, 32, pauseCtx->alpha);
    }
    if (isCurrent) {
        // MM's cursor goes round the cell's item (KaleidoScope_UpdateCursorSize, MergeShip_QuestPage_SetCursorPos).
        MergeShip_QuestPage_SetCursorQuad(sCellX[sCursor % TE_COLS] + 2, sCellY[sCursor / TE_COLS] - 2, 28, 28);
    }

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    for (cell = 0; cell < TE_CELLS; cell++) {
        u8 item = MsTeqP_CellItem(cell);
        s32 usable;

        if (item == ITEM_NONE) {
            continue;
        }
        if (MsTeqP_IsEquipCell(cell) && MergeShip_TerminaEquip_IsEquipped(item)) {
            // OoT KaleidoScope_DrawEquipment: the equipped item's outline behind it (IA8, OoT parameter_static).
            gSPVertex(POLY_OPA_DISP++, &sOutlineVtx[cell * 4], 4, 0);
            gDPPipeSync(POLY_OPA_DISP++);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
            POLY_OPA_DISP = Gfx_DrawTexQuadIA8(POLY_OPA_DISP, (TexturePtr)gOoTEqEquippedOutlineTex, 32, 32, 0);
        }
        // What can't be used now is grey (OoT's page; 2S2H gSPGrayscale as MM's Select Item).
        usable = MsTeqP_Usable(cell, item);
        if (!usable) {
            gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
            gSPGrayscale(POLY_OPA_DISP++, true);
        }
        gSPVertex(POLY_OPA_DISP++, &sCellVtx[cell * 4], 4, 0);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
        KaleidoScope_DrawTexQuadRGBA32(play->state.gfxCtx, MsTeqP_Icon(item), 32, 32, 0);
        if (!usable) {
            gSPGrayscale(POLY_OPA_DISP++, false);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);

    // Link in the centre panel, where Ocarina of Time's equipment page draws him (MergeShipPauseLink.h).
    MergeShip_PauseLink_Draw(play, "termina");
    return true;
}
