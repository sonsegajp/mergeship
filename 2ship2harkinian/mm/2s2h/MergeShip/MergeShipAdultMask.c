// MergeShip2 -- the Adult Mask in Termina (MergeShipAdultMask.h): its state, its cell on the Masks page and the swap
// with the Fierce Deity's Mask.

#include "global.h"
#include "2s2h/MergeShip/MergeShipAdultMask.h"
#include "2s2h/MergeShip/MergeShipAdultLink.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"
#include "interface/parameter_static/parameter_static.h"
#include <libultraship/bridge/consolevariablebridge.h>

// MergeShipAdultMask.cpp: the art is mounted (include/mergeship/AdultMask.hpp).
s32 MergeShip_AdultMask_ArtMounted(void);
s32 MergeShip_AdultMask_ChildMounted(void);

static const ALIGN_ASSET(2) char sAdultMaskDL[] = "__OTR__mergeship/adultmask/gAdultMaskDL";
static const ALIGN_ASSET(2) char sAdultMaskScreamDL[] = "__OTR__mergeship/adultmask/gAdultMaskScreamDL";
static const ALIGN_ASSET(2) char sAdultMaskNameTex[] = "__OTR__mergeship/adultmask/gAdultMaskNameENGTex";
static const ALIGN_ASSET(2) char sAdultMaskIconTex[] = "__OTR__mergeship/adultmask/gAdultMaskIconTex";
static const ALIGN_ASSET(2) char sChildMaskDL[] = "__OTR__mergeship/adultmask/gChildMaskDL";
static const ALIGN_ASSET(2) char sChildMaskScreamDL[] = "__OTR__mergeship/adultmask/gChildMaskScreamDL";
static const ALIGN_ASSET(2) char sChildMaskNameTex[] = "__OTR__mergeship/adultmask/gChildMaskNameENGTex";
static const ALIGN_ASSET(2) char sChildMaskIconTex[] = "__OTR__mergeship/adultmask/gChildMaskIconTex";

#define MS_FD_MASK_SLOT (SLOT_MASK_FIERCE_DEITY - ITEM_NUM_SLOTS) // the shared cell, as a Masks page slot

static u8* HumanAgeByte(void) {
    return &gSaveContext.save.shipSaveInfo.mergeShipHumanAge;
}

s32 MergeShip_AdultMask_Owned(void) {
    return CVarGetInteger(MERGESHIP_ADULT_MASK_CVAR, 1) && MergeShip_AdultLink_Enabled() &&
           MergeShip_AdultMask_ArtMounted();
}

static s32 FierceDeityOwned(void) {
    return INV_CONTENT(ITEM_MASK_FIERCE_DEITY) == ITEM_MASK_FIERCE_DEITY;
}

s32 MergeShip_AdultMask_ShowsAdult(void) {
    return MergeShip_AdultMask_Owned() && (!FierceDeityOwned() || !(*HumanAgeByte() & MERGESHIP_HUMAN_AGE_BIT_SLOT_FD));
}

u8 MergeShip_AdultMask_MaskPageItem(s32 maskSlot) {
    if ((maskSlot == MS_FD_MASK_SLOT) && MergeShip_AdultMask_ShowsAdult()) {
        return ITEM_MS_MASK_ADULT;
    }
    return gSaveContext.save.saveInfo.inventory.items[maskSlot + ITEM_NUM_SLOTS];
}

s32 MergeShip_AdultMask_IsChildMask(void) {
    return (MergeShip_AdultLink_GetSaveAge() == MERGESHIP_HUMAN_AGE_ADULT) && MergeShip_AdultMask_ChildMounted();
}

const char* MergeShip_AdultMask_NameTexture(u32 item) {
    if ((item != ITEM_MS_MASK_ADULT) || !MergeShip_AdultMask_Owned()) {
        return NULL;
    }
    return MergeShip_AdultMask_IsChildMask() ? sChildMaskNameTex : sAdultMaskNameTex;
}

s32 MergeShip_AdultMask_ScreamMounted(void); // MergeShipAdultMask.cpp

static u8 sChild = false; // the transformation's mask is the Child Mask (the adult put it on)

static const char* MergeShip_AdultMask_DLOf(s32 child, s32 scream) {
    if (child) {
        return scream ? sChildMaskScreamDL : sChildMaskDL;
    }
    return (scream && MergeShip_AdultMask_ScreamMounted()) ? sAdultMaskScreamDL : sAdultMaskDL;
}

const char* MergeShip_AdultMask_DL(s32 scream) {
    return MergeShip_AdultMask_DLOf(sChild, scream);
}

// The item's icon follows the human's age; the buttons holding it are given the new one (each keeps its own pointer).
void MergeShip_AdultMask_SyncIcon(PlayState* play) {
    TexturePtr icon = (TexturePtr)(MergeShip_AdultMask_IsChildMask() ? sChildMaskIconTex : sAdultMaskIconTex);
    s32 btn;

    if (!MergeShip_AdultMask_ArtMounted() || (gItemIcons[ITEM_MS_MASK_ADULT] == icon)) {
        return;
    }
    gItemIcons[ITEM_MS_MASK_ADULT] = icon;
    for (btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
        if (BUTTON_ITEM_EQUIP(0, btn) == ITEM_MS_MASK_ADULT) {
            Interface_LoadItemIcon(play, btn);
        }
    }
    for (btn = EQUIP_SLOT_D_RIGHT; btn < EQUIP_SLOT_D_MAX; btn++) {
        if (DPAD_BUTTON_ITEM_EQUIP(0, btn) == ITEM_MS_MASK_ADULT) {
            Interface_Dpad_LoadItemIcon(play, btn);
        }
    }
}

// ---- the swap ---------------------------------------------------------------------------------------------------------

static u8 sSwapArmed = false;
static u8 sSwapWaitNeutral = false; // after a swap, the stick's Right must be let go before the cursor moves again

// Buttons equipped from the shared cell follow what it shows.
static void FollowCell(PlayState* play, u8 oldItem, u8 newItem) {
    s32 btn;

    for (btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
        if ((C_SLOT_EQUIP(0, btn) == SLOT_MASK_FIERCE_DEITY) && (BUTTON_ITEM_EQUIP(0, btn) == oldItem)) {
            BUTTON_ITEM_EQUIP(0, btn) = newItem;
            Interface_LoadItemIcon(play, btn);
        }
    }
    for (btn = EQUIP_SLOT_D_RIGHT; btn < EQUIP_SLOT_D_MAX; btn++) {
        if ((DPAD_SLOT_EQUIP(0, btn) == SLOT_MASK_FIERCE_DEITY) && (DPAD_BUTTON_ITEM_EQUIP(0, btn) == oldItem)) {
            DPAD_BUTTON_ITEM_EQUIP(0, btn) = newItem;
            Interface_Dpad_LoadItemIcon(play, btn);
        }
    }
}

s32 MergeShip_AdultMask_UpdatePageSwap(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = CONTROLLER1(&play->state);
    s32 onCell = (pauseCtx->cursorSpecialPos == 0) && (pauseCtx->cursorPoint[PAUSE_MASK] == MS_FD_MASK_SLOT);
    s32 right;
    s32 left;

    if (sSwapWaitNeutral) {
        if (ABS_ALT(pauseCtx->stickAdjX) > 30) {
            return true;
        }
        sSwapWaitNeutral = false;
    }
    if (!onCell || !MergeShip_AdultMask_Owned() || !FierceDeityOwned()) {
        sSwapArmed = false;
        return false;
    }
    if (!sSwapArmed) {
        if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
            sSwapArmed = true;
            Audio_PlaySfx(NA_SE_SY_DECIDE);
            return true;
        }
        return false;
    }
    // The arrow points the way to the other mask: Right from the Adult Mask, Left from the Fierce Deity's Mask.
    right = (pauseCtx->stickAdjX > 30) || CHECK_BTN_ALL(input->press.button, BTN_DRIGHT);
    left = (pauseCtx->stickAdjX < -30) || CHECK_BTN_ALL(input->press.button, BTN_DLEFT);
    if (MergeShip_AdultMask_ShowsAdult() ? right : left) {
        u8 oldItem = MergeShip_AdultMask_MaskPageItem(MS_FD_MASK_SLOT);

        *HumanAgeByte() ^= MERGESHIP_HUMAN_AGE_BIT_SLOT_FD;
        FollowCell(play, oldItem, MergeShip_AdultMask_MaskPageItem(MS_FD_MASK_SLOT));
        pauseCtx->cursorItem[PAUSE_MASK] = MergeShip_AdultMask_MaskPageItem(MS_FD_MASK_SLOT);
        sSwapArmed = false;
        sSwapWaitNeutral = ABS_ALT(pauseCtx->stickAdjX) > 30;
        Audio_PlaySfx(NA_SE_SY_CURSOR);
        return true;
    }
    if ((input->press.button != 0) || (ABS_ALT(pauseCtx->stickAdjX) > 30) || (ABS_ALT(pauseCtx->stickAdjY) > 30)) {
        // Anything else lets it go (and does what it does).
        sSwapArmed = false;
        return false;
    }
    return true;
}

void MergeShip_AdultMask_DrawPagePrompt(PlayState* play) {
    static Vtx sArrowVtx[4];
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 i;

    if (!sSwapArmed || (pauseCtx->pageIndex != PAUSE_MASK) || (pauseCtx->maskVtx == NULL)) {
        return;
    }
    // A copy of the cell's quad beside it: right toward the Fierce Deity's Mask, left back toward the Adult Mask.
    for (i = 0; i < 4; i++) {
        sArrowVtx[i] = pauseCtx->maskVtx[MS_FD_MASK_SLOT * 4 + i];
        sArrowVtx[i].v.ob[0] += MergeShip_AdultMask_ShowsAdult() ? 28 : -28;
    }

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255,
                    (u8)(pauseCtx->alpha * (0.6f + 0.4f * Math_SinS((s16)(play->gameplayFrames * 0x800)))));
    gSPVertex(POLY_OPA_DISP++, sArrowVtx, 4, 0);
    gDPLoadTextureBlock(POLY_OPA_DISP++, MergeShip_AdultMask_ShowsAdult() ? gEmptyCRightArrowTex : gEmptyCLeftArrowTex,
                        G_IM_FMT_IA, G_IM_SIZ_8b, 32, 32, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- the transformation -------------------------------------------------------------------------------------------------

static s8 sPendingAdult = -1; // the transformation's target (1 adult, 0 young) until the player is rebuilt
static u8 sTransforming = false; // the transformation's cutscene (Player_Action_86, then _87 after the rebuild)

void MergeShip_AdultMask_BeginTransform(s32 toAdult) {
    sPendingAdult = toAdult ? 1 : 0;
    sTransforming = true;
    // What the item is as it goes on: the Child Mask while the human is the adult (a form of his too).
    sChild = MergeShip_AdultMask_IsChildMask();
}

void MergeShip_AdultMask_Sync(void) {
    *HumanAgeByte() &= ~MERGESHIP_HUMAN_AGE_BIT_BY_MASK; // (a bit an earlier build kept; the age is bit 0 alone)
    if (sPendingAdult >= 0) {
        MergeShip_AdultLink_SetSaveAge(sPendingAdult ? MERGESHIP_HUMAN_AGE_ADULT : MERGESHIP_HUMAN_AGE_YOUNG);
        sPendingAdult = -1;
    }
}

s32 MergeShip_AdultMask_Drawing(Player* player) {
    return sTransforming && (player != NULL) && (player->actor.id == ACTOR_PLAYER);
}

// BUG-0057: Anchor teammates' dummies wearing it (their packets' "adultMask": 1 the Adult Mask, 2 the Child Mask).
// The pointers are only compared, never followed.
static struct {
    Player* dummy;
    u8 kind;
} sRemoteMasks[8];

void MergeShip_AdultMask_SetRemote(Player* dummy, u8 kind) {
    s32 i;
    s32 freeSlot = -1;

    for (i = 0; i < ARRAY_COUNT(sRemoteMasks); i++) {
        if (sRemoteMasks[i].dummy == dummy) {
            sRemoteMasks[i].kind = kind;
            if (kind == 0) {
                sRemoteMasks[i].dummy = NULL;
            }
            return;
        }
        if ((freeSlot < 0) && (sRemoteMasks[i].dummy == NULL)) {
            freeSlot = i;
        }
    }
    if ((kind != 0) && (freeSlot >= 0)) {
        sRemoteMasks[freeSlot].dummy = dummy;
        sRemoteMasks[freeSlot].kind = kind;
    }
}

u8 MergeShip_AdultMask_DrawingKind(Player* player) {
    s32 i;

    if (MergeShip_AdultMask_Drawing(player)) {
        return sChild ? 2 : 1;
    }
    for (i = 0; (player != NULL) && (i < ARRAY_COUNT(sRemoteMasks)); i++) {
        if (sRemoteMasks[i].dummy == player) {
            return sRemoteMasks[i].kind;
        }
    }
    return 0;
}

const char* MergeShip_AdultMask_DLFor(Player* player, s32 scream) {
    return MergeShip_AdultMask_DLOf(MergeShip_AdultMask_DrawingKind(player) == 2, scream);
}

s32 MergeShip_AdultMask_DrawingChild(Player* player) {
    return MergeShip_AdultMask_DrawingKind(player) == 2;
}

// Called from MergeShipAdultMaskPlayer.inc once the cutscene's actions are over.
void MergeShip_AdultMask_EndTransform(void) {
    sTransforming = false;
}
