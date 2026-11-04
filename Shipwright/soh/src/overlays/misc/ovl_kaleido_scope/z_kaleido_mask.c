/*
 * MergeShip2 -- Pause Menu, Masks page (the carousel's fifth face).
 *
 * Ported from Majora's Mask's z_kaleido_mask.c (2ship2harkinian/mm/src/overlays/kaleido_scope/
 * ovl_kaleido_scope): KaleidoScope_DrawMaskSelect, KaleidoScope_UpdateMaskCursor and
 * KaleidoScope_UpdateMaskEquip, with MM's 6x4 mask grid, its per-form slot restrictions
 * (gMaskPlayerFormSlotRestrictions) and its rules on equipping masks.
 *
 * Hyrule's own masks (Keaton ... Truth) stay Ocarina of Time's child trade items on the Select Item page,
 * so this page holds MM's four transformation masks in MM's slots, when owned. They have no inventory
 * slot: on a C or D-pad button they carry SLOT_NONE, which every inventory lookup in the kaleido code
 * already skips, and they are told apart by item id rather than by slot.
 */

#include "z_kaleido_scope.h"
#include "textures/parameter_static/parameter_static.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipItems.h"
#include "soh/MergeShip/MergeShipPause.h"
#include "soh/MergeShip/MergeShipStrayFairy.h"
#include "mm/archives/item_name_static/item_name_static.h"
#include <string.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/MergeShip/MergeShipAdultMask.h"

// MergeShip2: Majora's Mask's C-Right and C-Left arrows, the Adult Mask swap's prompt.
static const ALIGN_ASSET(2) char sMergeShipRightArrowTex[] = "__OTR__mm/parameter_static/gEmptyCRightArrowTex";
static const ALIGN_ASSET(2) char sMergeShipLeftArrowTex[] = "__OTR__mm/parameter_static/gEmptyCLeftArrowTex";

// MM MASK_NUM_SLOTS, laid out as MM's 6 x 4 grid (and Ocarina of Time's item grid, the same geometry).
#define MASK_NUM_SLOTS 24
#define MASK_GRID_COLUMNS 6
#define MASK_GRID_ROWS 4

// MM's slot of each transformation mask (SLOT_MASK_x - ITEM_NUM_SLOTS): the grid's right column.
#define MASK_SLOT_DEKU 5
#define MASK_SLOT_GORON 11
#define MASK_SLOT_ZORA 17
#define MASK_SLOT_FIERCE_DEITY 23

// MM gMaskPlayerFormSlotRestrictions, the four transformation masks' columns (every other column is
// false for the transformed forms and true for the human). Indexed by MMPlayerForm.
static const u8 sMaskPlayerFormSlotRestrictions[MM_FORM_MAX][4] = {
    //  Deku   Goron  Zora   Fierce Deity
    { false, false, false, true }, // MM_FORM_FIERCE_DEITY
    { true, true, true, true },    // MM_FORM_GORON
    { true, true, true, true },    // MM_FORM_ZORA
    { true, true, true, true },    // MM_FORM_DEKU
    { true, true, true, true },    // MM_FORM_HUMAN
};
MERGESHIP_ASSERT_MM_FORM_TABLE(sMaskPlayerFormSlotRestrictions);

// MM sMaskPlayerFormItems: the mask that is the form (what cannot be taken off its button).
static const u8 sMaskPlayerFormItems[MM_FORM_MAX] = {
    ITEM_MM_MASK_FIERCE_DEITY, // MM_FORM_FIERCE_DEITY
    ITEM_MM_MASK_GORON,        // MM_FORM_GORON
    ITEM_MM_MASK_ZORA,         // MM_FORM_ZORA
    ITEM_MM_MASK_DEKU,         // MM_FORM_DEKU
    ITEM_NONE,                 // MM_FORM_HUMAN
};
MERGESHIP_ASSERT_MM_FORM_TABLE(sMaskPlayerFormItems);

// MM's page cursor (PauseContext::cursorPoint/cursorXIndex/cursorYIndex/cursorItem/cursorSlot
// [PAUSE_MASK]). Ocarina of Time's PauseContext has no room for a fifth page and lives in z64.h.
static s16 sMaskCursorPoint = MASK_SLOT_DEKU;
static s16 sMaskCursorX = MASK_SLOT_DEKU % MASK_GRID_COLUMNS;
static s16 sMaskCursorY = MASK_SLOT_DEKU / MASK_GRID_COLUMNS;
static u16 sMaskCursorItem = PAUSE_ITEM_NONE;

// MM's mask slots (SLOT_MASK_POSTMAN .. SLOT_MASK_FIERCE_DEITY, minus ITEM_NUM_SLOTS) and the mask in each.
static const u8 sMaskSlotItems[MASK_NUM_SLOTS] = {
    ITEM_MM_MASK_POSTMAN,     ITEM_MM_MASK_ALL_NIGHT,   ITEM_MM_MASK_BLAST,       ITEM_MM_MASK_STONE,
    ITEM_MM_MASK_GREAT_FAIRY, ITEM_MM_MASK_DEKU,        ITEM_MM_MASK_KEATON,      ITEM_MM_MASK_BREMEN,
    ITEM_MM_MASK_BUNNY,       ITEM_MM_MASK_DON_GERO,    ITEM_MM_MASK_SCENTS,      ITEM_MM_MASK_GORON,
    ITEM_MM_MASK_ROMANI,      ITEM_MM_MASK_CIRCUS_LEADER, ITEM_MM_MASK_KAFEIS_MASK, ITEM_MM_MASK_COUPLE,
    ITEM_MM_MASK_TRUTH,       ITEM_MM_MASK_ZORA,        ITEM_MM_MASK_KAMARO,      ITEM_MM_MASK_GIBDO,
    ITEM_MM_MASK_GARO,        ITEM_MM_MASK_CAPTAIN,     ITEM_MM_MASK_GIANT,       ITEM_MM_MASK_FIERCE_DEITY,
};

// The item in a mask slot, or ITEM_NONE (MM: gSaveContext.save.saveInfo.inventory.items[slot + 24]).
static u8 MergeShip_Kaleido_MaskInSlot(s16 slot) {
    u8 item;

    if ((slot < 0) || (slot >= MASK_NUM_SLOTS)) {
        return ITEM_NONE;
    }
    // MergeShip2: the Adult Mask shares the Fierce Deity's Mask's cell (MergeShipAdultMask.h).
    if ((slot == MASK_SLOT_FIERCE_DEITY) && MergeShip_AdultMask_ShowsAdult()) {
        return ITEM_MM_MASK_ADULT;
    }
    item = sMaskSlotItems[slot];
    return MergeShip_HasMMMaskItem(item) ? item : ITEM_NONE;
}

// The slot a mask item lives in, or -1.
static s16 MergeShip_Kaleido_SlotOfMask(u8 item) {
    s16 slot;

    if (item == ITEM_MM_MASK_ADULT) {
        return MASK_SLOT_FIERCE_DEITY; // MergeShip2: the Adult Mask's cell (MergeShipAdultMask.h)
    }

    for (slot = 0; slot < MASK_NUM_SLOTS; slot++) {
        if (sMaskSlotItems[slot] == item) {
            return slot;
        }
    }
    return -1;
}

// MM gMaskPlayerFormSlotRestrictions[GET_PLAYER_FORM][slot].
static s32 MergeShip_Kaleido_FormMayUseMaskSlot(s16 slot) {
    u8 mmForm = MergeShip_FormToMM(MergeShip_GetPlayerForm());

    switch (slot) {
        case MASK_SLOT_DEKU:
            return sMaskPlayerFormSlotRestrictions[mmForm][0];
        case MASK_SLOT_GORON:
            return sMaskPlayerFormSlotRestrictions[mmForm][1];
        case MASK_SLOT_ZORA:
            return sMaskPlayerFormSlotRestrictions[mmForm][2];
        case MASK_SLOT_FIERCE_DEITY:
            return sMaskPlayerFormSlotRestrictions[mmForm][3];
        default:
            return mmForm == MM_FORM_HUMAN;
    }
}

// MM Player_GetCurMaskItemId: the item of the mask being worn.
static u8 MergeShip_Kaleido_WornMaskItem(PlayState* play) {
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

static const char* MergeShip_Kaleido_MaskNameTexture(u16 item) {
    // MergeShip2: the Adult Mask's name, made at startup (MergeShipAdultMask.h).
    if (MergeShip_AdultMask_NameTexture(item) != NULL) {
        return MergeShip_AdultMask_NameTexture(item);
    }
    // MM item_name_static; 2S2H's archive carries the English names only.
    switch (item) {
        case ITEM_MM_MASK_DEKU:
            return mm_gItemNameDekuMaskENGTex;
        case ITEM_MM_MASK_GORON:
            return mm_gItemNameGoronMaskENGTex;
        case ITEM_MM_MASK_ZORA:
            return mm_gItemNameZoraMaskENGTex;
        case ITEM_MM_MASK_FIERCE_DEITY:
            return mm_gItemNameFierceDeitysMaskENGTex;
        case ITEM_MM_MASK_TRUTH:
            return mm_gItemNameMaskOfTruthENGTex;
        case ITEM_MM_MASK_KAFEIS_MASK:
            return mm_gItemNameKafeisMaskENGTex;
        case ITEM_MM_MASK_ALL_NIGHT:
            return mm_gItemNameAllNightMaskENGTex;
        case ITEM_MM_MASK_BUNNY:
            return mm_gItemNameBunnyHoodENGTex;
        case ITEM_MM_MASK_KEATON:
            return mm_gItemNameKeatonMaskENGTex;
        case ITEM_MM_MASK_GARO:
            return mm_gItemNameGarosMaskENGTex;
        case ITEM_MM_MASK_ROMANI:
            return mm_gItemNameRomanisMaskENGTex;
        case ITEM_MM_MASK_CIRCUS_LEADER:
            return mm_gItemNameCircusLeadersMaskENGTex;
        case ITEM_MM_MASK_POSTMAN:
            return mm_gItemNamePostmansHatENGTex;
        case ITEM_MM_MASK_COUPLE:
            return mm_gItemNameCouplesMaskENGTex;
        case ITEM_MM_MASK_GREAT_FAIRY:
            return mm_gItemNameGreatFairysMaskENGTex;
        case ITEM_MM_MASK_GIBDO:
            return mm_gItemNameGibdoMaskENGTex;
        case ITEM_MM_MASK_DON_GERO:
            return mm_gItemNameDonGerosMaskENGTex;
        case ITEM_MM_MASK_KAMARO:
            return mm_gItemNameKamarosMaskENGTex;
        case ITEM_MM_MASK_CAPTAIN:
            return mm_gItemNameCaptainsHatENGTex;
        case ITEM_MM_MASK_STONE:
            return mm_gItemNameStoneMaskENGTex;
        case ITEM_MM_MASK_BREMEN:
            return mm_gItemNameBremenMaskENGTex;
        case ITEM_MM_MASK_BLAST:
            return mm_gItemNameBlastMaskENGTex;
        case ITEM_MM_MASK_SCENTS:
            return mm_gItemNameMaskOfScentsENGTex;
        case ITEM_MM_MASK_GIANT:
            return mm_gItemNameGiantsMaskENGTex;
        default:
            return NULL;
    }
}

static void MergeShip_Kaleido_SetQuad(Vtx* vtx, s16 x, s16 y, s16 w, s16 h, u8 alpha) {
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
    vtx[1].v.tc[0] = vtx[2].v.tc[1] = vtx[3].v.tc[0] = vtx[3].v.tc[1] = 32 << 5;
}

// MM pauseCtx->maskVtx: 24 slots (Ocarina of Time's item-grid geometry, KaleidoScope_InitVertices) and
// one "equipped" outline per item button.
static Vtx* MergeShip_Kaleido_BuildMaskVtx(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Vtx* vtx = Graph_Alloc(play->state.gfxCtx, (MASK_NUM_SLOTS + 7) * 4 * sizeof(Vtx));
    s32 i;

    for (i = 0; i < MASK_NUM_SLOTS; i++) {
        s16 x = -96 + (i % MASK_GRID_COLUMNS) * 32 + 2;
        s16 y = 58 - (i / MASK_GRID_COLUMNS) * 32 + pauseCtx->offsetY - 2;

        MergeShip_Kaleido_SetQuad(&vtx[i * 4], x, y, 28, 28, 255);
    }
    for (i = 0; i < 7; i++) {
        MergeShip_Kaleido_SetQuad(&vtx[(MASK_NUM_SLOTS + i) * 4], -300, 300, 32, 32, pauseCtx->alpha);
    }
    return vtx;
}

// MM KaleidoScope_UpdateMaskCursor, cursor movement (MM lets the cursor rest on empty slots).
static void MergeShip_Kaleido_UpdateMaskCursor(PlayState* play) {
    Input* input = &play->state.input[0];
    PauseContext* pauseCtx = &play->pauseCtx;
    bool dpad = (CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) && !CHECK_BTN_ALL(input->cur.button, BTN_CUP));
    s16 oldCursorPoint = sMaskCursorPoint;
    s16 moveCursorResult = 0; // MM PAUSE_CURSOR_RESULT_NONE / _SLOT (1) / _SPECIAL_POS (2)
    s16 cursorPoint;
    s16 cursorX;
    s16 cursorY;
    s32 left = (pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT));
    s32 right = (pauseCtx->stickRelX > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT));
    s32 up = (pauseCtx->stickRelY > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DUP));
    s32 down = (pauseCtx->stickRelY < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DDOWN));

    // MergeShip2: A then Right on the Fierce Deity's cell swaps it with the Adult Mask (MergeShipAdultMask.h).
    if (MergeShip_AdultMask_UpdatePageSwap(
            play, (pauseCtx->cursorSpecialPos == 0) && (sMaskCursorPoint == MASK_SLOT_FIERCE_DEITY),
            pauseCtx->stickRelX, pauseCtx->stickRelY)) {
        if (pauseCtx->cursorSpecialPos == 0) {
            sMaskCursorItem = MergeShip_Kaleido_MaskInSlot(sMaskCursorPoint);
        }
        return;
    }

    if (pauseCtx->cursorSpecialPos == 0) {
        if (left || right) {
            cursorPoint = sMaskCursorPoint;
            cursorX = sMaskCursorX;
            cursorY = sMaskCursorY;

            while (moveCursorResult == 0) {
                if (left) {
                    if (sMaskCursorX != 0) {
                        sMaskCursorX--;
                        sMaskCursorPoint--;
                        moveCursorResult = 1;
                    } else {
                        sMaskCursorY++;
                        if (sMaskCursorY >= MASK_GRID_ROWS) {
                            sMaskCursorY = 0;
                        }
                        sMaskCursorPoint = sMaskCursorX + (sMaskCursorY * MASK_GRID_COLUMNS);
                        if (sMaskCursorPoint >= MASK_NUM_SLOTS) {
                            sMaskCursorPoint = sMaskCursorX;
                        }
                        if (cursorY == sMaskCursorY) {
                            sMaskCursorX = cursorX;
                            sMaskCursorPoint = cursorPoint;
                            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
                            moveCursorResult = 2;
                        }
                    }
                } else {
                    if (sMaskCursorX <= 4) {
                        sMaskCursorX++;
                        sMaskCursorPoint++;
                        moveCursorResult = 1;
                    } else {
                        sMaskCursorY++;
                        if (sMaskCursorY >= MASK_GRID_ROWS) {
                            sMaskCursorY = 0;
                        }
                        sMaskCursorPoint = sMaskCursorX + (sMaskCursorY * MASK_GRID_COLUMNS);
                        if (sMaskCursorPoint >= MASK_NUM_SLOTS) {
                            sMaskCursorPoint = sMaskCursorX;
                        }
                        if (cursorY == sMaskCursorY) {
                            sMaskCursorX = cursorX;
                            sMaskCursorPoint = cursorPoint;
                            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                            moveCursorResult = 2;
                        }
                    }
                }
            }
        }
    } else if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
        if (right) {
            // MM KaleidoScope_MoveCursorFromSpecialPos, then search from the top-left slot.
            pauseCtx->nameDisplayTimer = 0;
            pauseCtx->cursorSpecialPos = 0;
            Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            cursorX = cursorY = cursorPoint = 0;
            while (true) {
                if (MergeShip_Kaleido_MaskInSlot(cursorPoint) != ITEM_NONE) {
                    sMaskCursorPoint = cursorPoint;
                    sMaskCursorX = cursorX;
                    sMaskCursorY = cursorY;
                    moveCursorResult = 1;
                    break;
                }
                cursorY++;
                cursorPoint += MASK_GRID_COLUMNS;
                if (cursorY < MASK_GRID_ROWS) {
                    continue;
                }
                cursorY = 0;
                cursorPoint = cursorX + 1;
                cursorX = cursorPoint;
                if (cursorX < MASK_GRID_COLUMNS) {
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
        cursorX = cursorPoint = MASK_GRID_COLUMNS - 1;
        cursorY = 0;
        while (true) {
            if (MergeShip_Kaleido_MaskInSlot(cursorPoint) != ITEM_NONE) {
                sMaskCursorPoint = cursorPoint;
                sMaskCursorX = cursorX;
                sMaskCursorY = cursorY;
                moveCursorResult = 1;
                break;
            }
            cursorY++;
            cursorPoint += MASK_GRID_COLUMNS;
            if (cursorY < MASK_GRID_ROWS) {
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
        if (up || down) {
            cursorPoint = sMaskCursorPoint;
            cursorY = sMaskCursorY;
            if (up) {
                if (sMaskCursorY != 0) {
                    sMaskCursorY--;
                    sMaskCursorPoint -= MASK_GRID_COLUMNS;
                } else {
                    sMaskCursorY = cursorY;
                    sMaskCursorPoint = cursorPoint;
                }
            } else {
                if (sMaskCursorY < MASK_GRID_ROWS - 1) {
                    sMaskCursorY++;
                    sMaskCursorPoint += MASK_GRID_COLUMNS;
                } else {
                    sMaskCursorY = cursorY;
                    sMaskCursorPoint = cursorPoint;
                }
            }
        }

        sMaskCursorItem = MergeShip_Kaleido_MaskInSlot(sMaskCursorPoint);
        pauseCtx->cursorColorSet = 4; // MM PAUSE_CURSOR_COLOR_SET_YELLOW
        if (sMaskCursorItem == ITEM_NONE) {
            sMaskCursorItem = PAUSE_ITEM_NONE;
            pauseCtx->cursorColorSet = 0; // MM PAUSE_CURSOR_COLOR_SET_WHITE
        }
    } else {
        sMaskCursorItem = PAUSE_ITEM_NONE;
    }

    if (oldCursorPoint != sMaskCursorPoint) {
        Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

// MM KaleidoScope_UpdateMaskCursor, "Equip item to the C buttons".
static void MergeShip_Kaleido_TryEquipMask(PlayState* play, Vtx* maskVtx) {
    Input* input = &play->state.input[0];
    PauseContext* pauseCtx = &play->pauseCtx;
    u16 buttons = BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT;
    s32 targetButton = 0; // gSaveContext.equips.buttonItems index
    u8 wornItem;
    u8 formItem;
    s32 envHazard;

    if ((sMaskCursorItem == PAUSE_ITEM_NONE) || (pauseCtx->debugState != 0) || (pauseCtx->state != 6) ||
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

    // The same button order as KaleidoScope_SetupItemEquip's equipTargetCBtn, plus one.
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

    // MM: a mask that is being worn -- or that is the form the player is in -- cannot be taken off its
    // button.
    wornItem = MergeShip_Kaleido_WornMaskItem(play);
    formItem = sMaskPlayerFormItems[MergeShip_FormToMM(MergeShip_GetPlayerForm())];
    if (((wornItem != ITEM_NONE) && (wornItem == gSaveContext.equips.buttonItems[targetButton])) ||
        ((formItem != ITEM_NONE) && (formItem == gSaveContext.equips.buttonItems[targetButton]))) {
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }

    // MM: the Deku and Goron masks cannot be equipped in the water.
    envHazard = MergeShip_GetEnvironmentalHazard(play);
    if ((envHazard >= PLAYER_ENV_HAZARD_UNDERWATER_FLOOR) && (envHazard <= PLAYER_ENV_HAZARD_UNDERWATER_FREE) &&
        ((sMaskCursorPoint == MASK_SLOT_DEKU) || (sMaskCursorPoint == MASK_SLOT_GORON))) {
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }

    // Ocarina of Time's equip animation (the icon flies to the button); MM's swap rule when it lands
    // (MergeShip_Kaleido_FinishMaskEquip). A mask has no inventory slot.
    KaleidoScope_SetupItemEquip(play, sMaskCursorItem, SLOT_NONE, maskVtx[sMaskCursorPoint * 4].v.ob[0] * 10,
                                maskVtx[sMaskCursorPoint * 4].v.ob[1] * 10);
}

void MergeShip_Kaleido_DrawMaskSelect(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Vtx* maskVtx = MergeShip_Kaleido_BuildMaskVtx(play);
    s32 isCurrent = (pauseCtx->pageIndex == MERGESHIP_PAUSE_MASK);
    s32 i;
    s32 j;

    OPEN_DISPS(play->state.gfxCtx);

    if (isCurrent) {
        pauseCtx->cursorColorSet = 0;
        pauseCtx->nameColorSet = 0;

        if ((pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0)) {
            MergeShip_Kaleido_UpdateMaskCursor(play);
            MergeShip_Kaleido_TryEquipMask(play, maskVtx);
        } else if (pauseCtx->unk_1E4 == 3) {
            pauseCtx->cursorColorSet = 4; // equipping
        }
        KaleidoScope_SetCursorVtx(pauseCtx, sMaskCursorPoint * 4, maskVtx);
    }

    Gfx_SetupDL_42Opa(play->state.gfxCtx);

    // MM: a white box around the masks that are on the C buttons (and SoH's D-pad).
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    for (i = 1; i < 8; i++) {
        u8 item = gSaveContext.equips.buttonItems[i];
        s16 slot;
        Vtx* outline = &maskVtx[(MASK_NUM_SLOTS + i - 1) * 4];

        if ((i >= 4) && !CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0)) {
            break;
        }
        slot = MergeShip_Kaleido_SlotOfMask(item);
        if (slot < 0) {
            continue;
        }
        MergeShip_Kaleido_SetQuad(outline, maskVtx[slot * 4].v.ob[0] - 2, maskVtx[slot * 4].v.ob[1] + 2, 32, 32,
                                  pauseCtx->alpha);
        gSPVertex(POLY_OPA_DISP++, outline, 4, 0);
        POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, gEquippedItemOutlineTex, 32, 32, 0);
    }

    gDPPipeSync(POLY_OPA_DISP++);

    // MM: the mask icons; the one under the cursor is drawn larger when the current form may use it.
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (i = 0, j = 0; i < MASK_NUM_SLOTS; i++, j += 4) {
        u8 item = MergeShip_Kaleido_MaskInSlot(i);

        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
        if (item == ITEM_NONE) {
            continue;
        }
        if (isCurrent && (pauseCtx->unk_1E4 == 0) && (pauseCtx->cursorSpecialPos == 0) &&
            MergeShip_Kaleido_FormMayUseMaskSlot(i) && (i == sMaskCursorPoint)) {
            MergeShip_Kaleido_SetQuad(&maskVtx[j], maskVtx[j].v.ob[0] - 2, maskVtx[j].v.ob[1] + 2, 32, 32, 255);
        }
        gSPVertex(POLY_OPA_DISP++, &maskVtx[j], 4, 0);
        KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIcons[item], 32, 32, 0);
    }

    // MergeShip2: the Adult Mask swap's arrow, beside the shared cell while A has armed it (MergeShipAdultMask.h).
    if (isCurrent && MergeShip_AdultMask_SwapArmed()) {
        static Vtx sArrowVtx[4];
        u8 alpha = (u8)(pauseCtx->alpha * (0.6f + 0.4f * Math_SinS((s16)(play->gameplayFrames * 0x800))));

        s32 toRight = MergeShip_AdultMask_SwapArrowRight();

        // Right of the cell toward the Fierce Deity's Mask, left of it back toward the Adult Mask.
        MergeShip_Kaleido_SetQuad(sArrowVtx, maskVtx[MASK_SLOT_FIERCE_DEITY * 4].v.ob[0] + (toRight ? 28 : -28),
                                  maskVtx[MASK_SLOT_FIERCE_DEITY * 4].v.ob[1], 32, 32, alpha);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, alpha);
        gSPVertex(POLY_OPA_DISP++, sArrowVtx, 4, 0);
        POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(
            POLY_OPA_DISP, toRight ? sMergeShipRightArrowTex : sMergeShipLeftArrowTex, 32, 32, 0);
    }

    // MergeShip2: MM's stray fairy counts, one per temple, below the grid (MergeShipStrayFairy.h).
    MergeShip_StrayFairy_DrawMaskPagePanel(play);

    if (isCurrent && (pauseCtx->cursorSpecialPos == 0)) {
        KaleidoScope_DrawCursor(play, MERGESHIP_PAUSE_MASK);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// KaleidoScope_UpdateNamePanel for the Masks page (MM item names from mm.o2r's item_name_static).
void MergeShip_Kaleido_UpdateMaskNamePanel(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    if (pauseCtx->namedItem != sMaskCursorItem) {
        const char* texture;

        pauseCtx->namedItem = sMaskCursorItem;
        texture = MergeShip_Kaleido_MaskNameTexture(pauseCtx->namedItem);
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

u16 MergeShip_Kaleido_GetMaskCursorItem(void) {
    return sMaskCursorItem;
}

// MM KaleidoScope_UpdateMaskEquip's landing: the mask goes on the target button; if it was already on
// another button, that button takes the target's previous item (MM swaps by mask slot; the masks here
// have no slot, so by item).
void MergeShip_Kaleido_FinishMaskEquip(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 target = pauseCtx->equipTargetCBtn + 1;
    s32 lastButton = CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) ? 7 : 3;
    s32 other;

    for (other = 1; other <= lastButton; other++) {
        if ((other == target) || (gSaveContext.equips.buttonItems[other] != pauseCtx->equipTargetItem)) {
            continue;
        }
        if (gSaveContext.equips.buttonItems[target] != ITEM_NONE) {
            gSaveContext.equips.buttonItems[other] = gSaveContext.equips.buttonItems[target];
            gSaveContext.equips.cButtonSlots[other - 1] = gSaveContext.equips.cButtonSlots[target - 1];
            Interface_LoadItemIcon2(play, other);
        } else {
            gSaveContext.equips.buttonItems[other] = ITEM_NONE;
            gSaveContext.equips.cButtonSlots[other - 1] = SLOT_NONE;
        }
    }

    gSaveContext.equips.buttonItems[target] = pauseCtx->equipTargetItem;
    gSaveContext.equips.cButtonSlots[target - 1] = SLOT_NONE;
    Interface_LoadItemIcon1(play, target);
}
