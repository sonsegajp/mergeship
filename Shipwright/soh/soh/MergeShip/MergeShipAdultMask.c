// MergeShip2 -- the Adult Mask in Hyrule (MergeShipAdultMask.h): its state, the shared cell's swap, and the
// transformation's notes.

#include "soh/MergeShip/MergeShipSingleplayer.h"
#include "global.h"
#include "soh/MergeShip/MergeShipAdultMask.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipTransformFx.h" // R_PLAY_FILL_SCREEN_*
void MergeShipTest_Log(const char* fmt, ...);
#include <libultraship/bridge/consolevariablebridge.h>
#include <mergeship/MaskFit.h> // the Child Mask on the adult's head

// MergeShipAdultMask.cpp: the art is mounted (include/mergeship/AdultMask.hpp).
s32 MergeShip_AdultMask_ArtMounted(void);
s32 MergeShip_AdultMask_ChildMounted(void);

static const char sAdultMaskDL[] = "__OTR__mergeship/adultmask/gAdultMaskDL";
static const char sAdultMaskScreamDL[] = "__OTR__mergeship/adultmask/gAdultMaskScreamDL";
static const char sAdultMaskNameTex[] = "__OTR__mergeship/adultmask/gAdultMaskNameENGTex";
static const char sAdultMaskIconTex[] = "__OTR__mergeship/adultmask/gAdultMaskIconTex";
static const char sChildMaskDL[] = "__OTR__mergeship/adultmask/gChildMaskDL";
static const char sChildMaskScreamDL[] = "__OTR__mergeship/adultmask/gChildMaskScreamDL";
static const char sChildMaskNameTex[] = "__OTR__mergeship/adultmask/gChildMaskNameENGTex";
static const char sChildMaskIconTex[] = "__OTR__mergeship/adultmask/gChildMaskIconTex";
// The Fierce Deity's Mask the Adult Mask is made from (z_player_lib.c sMMTransformationMaskDLs[0] and [4]): drawn in
// its place when its own art is not mounted (MergeShip_AdultMask_DL).
static const char sFierceDeityMaskDL[] = "__OTR__mm/objects/gameplay_keep/gFierceDeityMaskDL";
static const char sFierceDeityMaskScreamDL[] = "__OTR__mm/objects/object_mask_boy/object_mask_boy_DL_000900";

// The mask holds Link at the other age: gMergeShipSave.adultMaskTimelineAge names the timeline's age while he is not
// it. He is back at it by any way at all (the mask, the Master Sword's time travel): nothing is kept any more.
static s32 MergeShip_AdultMask_AgeKept(void) {
    if ((gMergeShipSave.adultMaskTimelineAge != 0xFF) &&
        (gMergeShipSave.adultMaskTimelineAge == (u8)gSaveContext.linkAge)) {
        gMergeShipSave.adultMaskTimelineAge = 0xFF;
    }
    return gMergeShipSave.adultMaskTimelineAge != 0xFF;
}

s32 MergeShip_AdultMask_Owned(void) {
    // While the mask holds Link at the other age it stays usable whatever the option says and whether its art is
    // mounted, so it can always be taken off: nothing else turns him back (the Master Sword needs the Prelude of Light
    // for an adult, which a real child never had), and an option turned off must not leave him stuck at its age.
    // Singleplayer: the traveling salesman sells it to an adult Link (MergeShipSingleplayer.h); until then it is not
    // Hyrule's.
    if (MergeShip_Singleplayer_Active() && !MergeShip_Singleplayer_AdultMaskBought() &&
        !MergeShip_AdultMask_AgeKept()) {
        return false;
    }
    return (CVarGetInteger(MERGESHIP_ADULT_MASK_CVAR, 1) && MergeShip_AdultMask_ArtMounted()) ||
           MergeShip_AdultMask_AgeKept();
}

s32 MergeShip_AdultMask_TimelineIsAdult(void) {
    if (MergeShip_AdultMask_AgeKept()) {
        return gMergeShipSave.adultMaskTimelineAge == LINK_AGE_ADULT;
    }
    return LINK_IS_ADULT;
}

s32 MergeShip_AdultMask_ShowsAdult(void) {
    return MergeShip_AdultMask_Owned() && (!MergeShip_HasTransformationMask(MM_FORM_FIERCE_DEITY) ||
                                           !(gMergeShipSave.ownedMasks & MERGESHIP_OWNED_MASKS_SLOT_FD));
}

s32 MergeShip_AdultMask_IsChildMask(void) {
    return LINK_IS_ADULT && MergeShip_AdultMask_ChildMounted();
}

const char* MergeShip_AdultMask_NameTexture(u16 item) {
    // (Owned without the art only while it takes Link back: its name was never made then.)
    if ((item != ITEM_MM_MASK_ADULT) || !MergeShip_AdultMask_Owned() || !MergeShip_AdultMask_ArtMounted()) {
        return NULL;
    }
    return MergeShip_AdultMask_IsChildMask() ? sChildMaskNameTex : sAdultMaskNameTex;
}

void MergeShip_AdultMask_SyncIcon(void) {
    // (z_inventory.c starts it as the Adult Mask's; without the Child Mask's art it stays that.)
    gItemIcons[ITEM_MM_MASK_ADULT] =
        (void*)(MergeShip_AdultMask_IsChildMask() ? sChildMaskIconTex : sAdultMaskIconTex);
}

s32 MergeShip_AdultMask_ScreamMounted(void); // MergeShipAdultMask.cpp

static u8 sChild = false; // the transformation's mask is the Child Mask (the adult put it on)

static const char* MergeShip_AdultMask_DLOf(s32 child, s32 scream) {
    if (!MergeShip_AdultMask_ArtMounted()) {
        // Only while the mask takes Link back to his own age without its art (MergeShip_AdultMask_Owned).
        return scream ? sFierceDeityMaskScreamDL : sFierceDeityMaskDL;
    }
    if (child) {
        return scream ? sChildMaskScreamDL : sChildMaskDL;
    }
    return (scream && MergeShip_AdultMask_ScreamMounted()) ? sAdultMaskScreamDL : sAdultMaskDL;
}

const char* MergeShip_AdultMask_DL(s32 scream) {
    return MergeShip_AdultMask_DLOf(sChild, scream);
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
            if (sRemoteMasks[i].kind != kind) {
                MergeShipTest_Log("ADULTMASK teammate's mask kind=%d", kind);
            }
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
        MergeShipTest_Log("ADULTMASK teammate's mask kind=%d", kind);
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

static float MergeShip_AdultMask_FitCVar(const char* name, float defaultValue) {
    return CVarGetFloat(name, defaultValue);
}

void MergeShip_AdultMask_ApplyHeadFit(Player* player) {
    MsMaskFit fit;

    // The adult's own head (a form's has its own mask placement, MM's sMMTransformationMaskHeadOffsets).
    if (!sChild || !MergeShip_AdultMask_Drawing(player) || (player->mergeShipForm != PLAYER_FORM_ADULT)) {
        return;
    }
    fit = MsMaskFit_Adult(MS_MASK_FIT_CHILD_MASK, MergeShip_AdultMask_FitCVar);
    Matrix_Translate(fit.x, fit.y, fit.z, MTXMODE_APPLY);
    if (fit.pitch != 0.0f) {
        // A backward tilt is a pitch about world X, which is the head's local Z.
        Matrix_RotateZ(BINANG_TO_RAD((s16)fit.pitch), MTXMODE_APPLY);
    }
    Matrix_Scale(fit.sx, fit.sy, fit.sz, MTXMODE_APPLY);
}

static void PlaySfx(u16 sfxId) {
    Audio_PlaySfxGeneral(sfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
}

// ---- the swap ---------------------------------------------------------------------------------------------------------

static u8 sSwapArmed = false;
static u8 sSwapWaitNeutral = false; // after a swap, the stick must be let go before the cursor moves again

s32 MergeShip_AdultMask_SwapArmed(void) {
    return sSwapArmed;
}

s32 MergeShip_AdultMask_SwapArrowRight(void) {
    return MergeShip_AdultMask_ShowsAdult();
}

// Buttons holding the cell's mask follow what it shows (masks are on buttons by item id, MergeShip's Masks page).
static void FollowCell(PlayState* play, u8 oldItem, u8 newItem) {
    s32 btn;

    for (btn = 1; btn < ARRAY_COUNT(gSaveContext.equips.buttonItems); btn++) {
        if (gSaveContext.equips.buttonItems[btn] == oldItem) {
            gSaveContext.equips.buttonItems[btn] = newItem;
            Interface_LoadItemIcon1(play, btn);
        }
    }
}

s32 MergeShip_AdultMask_UpdatePageSwap(PlayState* play, s32 onCell, s32 stickX, s32 stickY) {
    Input* input = &play->state.input[0];
    s32 right;
    s32 left;

    if (sSwapWaitNeutral) {
        if (ABS(stickX) > 30) {
            return true;
        }
        sSwapWaitNeutral = false;
    }
    if (!onCell || !MergeShip_AdultMask_Owned() || !MergeShip_HasTransformationMask(MM_FORM_FIERCE_DEITY)) {
        sSwapArmed = false;
        return false;
    }
    if (!sSwapArmed) {
        if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
            sSwapArmed = true;
            PlaySfx(NA_SE_SY_DECIDE);
            return true;
        }
        return false;
    }
    right = (stickX > 30) || CHECK_BTN_ALL(input->press.button, BTN_DRIGHT);
    left = (stickX < -30) || CHECK_BTN_ALL(input->press.button, BTN_DLEFT);
    if (MergeShip_AdultMask_SwapArrowRight() ? right : left) {
        u8 oldItem = MergeShip_AdultMask_ShowsAdult() ? ITEM_MM_MASK_ADULT : ITEM_MM_MASK_FIERCE_DEITY;

        gMergeShipSave.ownedMasks ^= MERGESHIP_OWNED_MASKS_SLOT_FD;
        FollowCell(play, oldItem, MergeShip_AdultMask_ShowsAdult() ? ITEM_MM_MASK_ADULT : ITEM_MM_MASK_FIERCE_DEITY);
        sSwapArmed = false;
        sSwapWaitNeutral = ABS(stickX) > 30;
        PlaySfx(NA_SE_SY_CURSOR);
        return true;
    }
    if ((input->press.button != 0) || (ABS(stickX) > 30) || (ABS(stickY) > 30)) {
        // Anything else lets it go (and does what it does).
        sSwapArmed = false;
        return false;
    }
    return true;
}

// ---- the transformation -------------------------------------------------------------------------------------------------

static u8 sUsing = false;       // the item being used is the Adult Mask (Player_UseItem -> MergeShip_UseTransformationMask)
static u8 sTransforming = false; // its transformation, until MergeShip_AdultMask_SwitchAge ends it at the flash

s32 MergeShip_AdultMask_UseItem(PlayState* play, Player* player, s32 item) {
    sUsing = (item == ITEM_MM_MASK_ADULT);
    // Refused only when the feature is off (or has no art) and the mask holds no age to give back (Owned).
    if (sUsing && !MergeShip_AdultMask_Owned()) {
        sUsing = false;
        PlaySfx(NA_SE_SY_ERROR);
        return true;
    }
    return false;
}

s32 MergeShip_AdultMask_TakeUsing(void) {
    s32 using = sUsing;

    sUsing = false;
    return using;
}

void MergeShip_AdultMask_BeginTransform(void) {
    sTransforming = true;
    // What the item is as it goes on: the Child Mask while Link is the adult (a form of his too).
    sChild = MergeShip_AdultMask_IsChildMask();
}

s32 MergeShip_AdultMask_Drawing(Player* player) {
    return sTransforming && (player != NULL) && (player->actor.id == ACTOR_PLAYER);
}

void MergeShip_AdultMask_OnSceneLoad(PlayState* play) {
    if (sTransforming) {
        sTransforming = false;
        R_PLAY_FILL_SCREEN_ON = 0;
        R_PLAY_FILL_SCREEN_ALPHA = 0;
    }
    MergeShip_AdultMask_SyncIcon(); // the age may have changed with the scene (the Master Sword, a file loaded)
}

// The transformation's flash: Link becomes the real other age, in place -- no scene reload (removed 2026-09-30; the
// player asked for the mask to work like the Master Sword does, in the current room, not SoH's SwitchAge). Ocarina
// of Time's own age-equipment swap (Inventory_SwapAgeEquipment, as src/code/z_play.c's Play_Destroy runs it for a
// scene's own age change) reads gSaveContext.equips.buttonItems[0] as "the current age's B item" while
// gSaveContext.linkAge still names the age being left, so both must be right first:
//   - if the player is mid-form (used from another form: MergeShip_UseTransformationMask already set
//     gMergeShipSave.mmForm to human, but player->mergeShipForm -- the field the draw/animation side still reads --
//     is still the form), MergeShip_SwapFormEquips parks that form's B item and loads the human one back into slot 0,
//     the same way ending any other form does (MergeShipPlayerTransform.inc's MergeShip_ReinitAsForm);
//   - player->mergeShipForm is then set to a human value, so that function's own MergeShip_SwapFormEquips call,
//     moments later, finds nothing left to park and does not shuffle the same button a second time.
// Unlike every other transformation this one also has to refresh the B/C/D-pad icons itself: nothing else does,
// since no other transformation touches them and a scene reload -- which would have -- no longer happens
// (src/code/z_construct.c's func_801109B0 loads them the same way when a scene starts).
void MergeShip_AdultMask_SwitchAge(PlayState* play, Player* player) {
    s32 i;
    // A flag per button, not the saved slot: the Masks page equips a mask with no inventory slot, so the mask's own
    // cButtonSlots entry is always SLOT_NONE (z_kaleido_mask.c) and cannot mark where it was.
    u8 hadMask[ARRAY_COUNT(gSaveContext.equips.buttonItems)];

    MergeShip_SwapFormEquips(player->mergeShipForm, (u8)gSaveContext.linkAge);
    player->mergeShipForm = (u8)gSaveContext.linkAge;

    // The buttons holding the mask keep it across the age change (each age has its own C and D-pad buttons in Ocarina
    // of Time): the new age takes it off with the button that put it on, as Majora's Mask's forms keep their masks.
    // BUG-0044: only the button the mask was used from (player->heldItemButton) -- not every button that held it.
    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
        hadMask[i] = (gSaveContext.equips.buttonItems[i] == ITEM_MM_MASK_ADULT) && (i == player->heldItemButton);
    }

    Inventory_SwapAgeEquipment();

    // Only onto a button the new age has free: whatever the new age keeps on that button (another transformation
    // mask, an item) stays (BUG-0006: the Goron Mask on the adult's C-Left became the Adult Mask).
    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
        if (hadMask[i] && (gSaveContext.equips.buttonItems[i] == ITEM_NONE)) {
            s32 j;

            gSaveContext.equips.buttonItems[i] = ITEM_MM_MASK_ADULT;
            gSaveContext.equips.cButtonSlots[i - 1] = SLOT_NONE; // as the Masks page equips it
            // BUG-0098: the mask stays where it was -- the new age's own earlier button for it (its button set is
            // its own) lets it go, so it is not on two buttons.
            for (j = 1; j < ARRAY_COUNT(gSaveContext.equips.buttonItems); j++) {
                if ((j != i) && (gSaveContext.equips.buttonItems[j] == ITEM_MM_MASK_ADULT)) {
                    gSaveContext.equips.buttonItems[j] = ITEM_NONE;
                    gSaveContext.equips.cButtonSlots[j - 1] = SLOT_NONE;
                }
            }
        }
    }

    // The world keeps the timeline it was in when the mask first changed Link's age (the user, 2026-09-30: the mask's
    // adult must not switch the timeline -- scenes choose their era on level load from the age): noted on the way
    // out of the real age, forgotten when he is back to it (MergeShip_AdultMask_TimelineIsAdult).
    if (gMergeShipSave.adultMaskTimelineAge == 0xFF) {
        gMergeShipSave.adultMaskTimelineAge = (u8)gSaveContext.linkAge;
    }
    gSaveContext.linkAge ^= 1; // LINK_AGE_ADULT (0) <-> LINK_AGE_CHILD (1)
    if (gMergeShipSave.adultMaskTimelineAge == (u8)gSaveContext.linkAge) {
        gMergeShipSave.adultMaskTimelineAge = 0xFF;
    }
    // The scene's own record of the age it loaded with follows: Player_Destroy puts gSaveContext.linkAge back to
    // play->linkAgeOnLoad when the scene ends, and Play_Destroy swaps the age equipment again whenever the two differ
    // -- left as it was, the next door would turn Link back into the age he left, holding this age's buttons.
    play->linkAgeOnLoad = gSaveContext.linkAge;
    MergeShip_SetPlayerForm((u8)gSaveContext.linkAge);
    MergeShip_AdultMask_SyncIcon(); // the Adult Mask's icon for the child, the Child Mask's for the adult

    for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
        if ((gSaveContext.equips.buttonItems[i] < 0xF0) ||
            ((i == 0) && (gSaveContext.equips.buttonItems[i] != 0xFF))) {
            Interface_LoadItemIcon1(play, i);
        }
    }

    // MM func_8012301C's counterpart for this transformation: it ends here, at the flash, rather than at a scene
    // load that no longer happens.
    sTransforming = false;
}
