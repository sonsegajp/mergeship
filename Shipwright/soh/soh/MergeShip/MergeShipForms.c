#include "global.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipAssets.h"

// MergeShip2 -- which form the player is in, and what a form owns.
//
// MM keeps the form in the save (gSaveContext.save.playerForm): it survives scene changes, and an
// owl save made as a Goron reloads as a Goron. This does the same, in a save section of its own
// (MergeShipSave.cpp) so Ocarina of Time's save layout is untouched.

// MM_FORM_FIERCE_DEITY is 0, so a zeroed save would mean "Fierce Deity". Start human.
MergeShipSaveData gMergeShipSave = { .mmForm = MM_FORM_HUMAN };

static u8 sDebugForm = 0xFF;

static const u8 sFormToMM[PLAYER_FORM_MAX] = {
    MM_FORM_HUMAN,        // PLAYER_FORM_ADULT
    MM_FORM_HUMAN,        // PLAYER_FORM_CHILD
    MM_FORM_DEKU,         // PLAYER_FORM_DEKU
    MM_FORM_GORON,        // PLAYER_FORM_GORON
    MM_FORM_ZORA,         // PLAYER_FORM_ZORA
    MM_FORM_FIERCE_DEITY, // PLAYER_FORM_FIERCE_DEITY
};
MERGESHIP_ASSERT_FORM_TABLE(sFormToMM);

u8 MergeShip_FormToMM(u8 form) {
    return (form < PLAYER_FORM_MAX) ? sFormToMM[form] : MM_FORM_HUMAN;
}

u8 MergeShip_FormFromMM(u8 mmForm) {
    switch (mmForm) {
        case MM_FORM_FIERCE_DEITY:
            return PLAYER_FORM_FIERCE_DEITY;
        case MM_FORM_GORON:
            return PLAYER_FORM_GORON;
        case MM_FORM_ZORA:
            return PLAYER_FORM_ZORA;
        case MM_FORM_DEKU:
            return PLAYER_FORM_DEKU;
        default:
            return (u8)gSaveContext.linkAge;
    }
}

u8 MergeShip_GetPlayerForm(void) {
    u8 form = MergeShip_FormFromMM(gMergeShipSave.mmForm);

    if (MERGESHIP_FORM_IS_MM(form) && !MergeShip_IsMajorasMaskMounted()) {
        // Deku, Goron, Zora and Fierce Deity are drawn from mm.o2r. Without that archive their
        // skeletons do not exist, and asking for one would fault on a NULL resource rather than
        // report a missing file.
        form = (u8)gSaveContext.linkAge;
    }

    {
        static s32 sLastLogged = -1;
        if ((s32)form != sLastLogged) {
            sLastLogged = form;
            MergeShip_LogFormSelected(form, MergeShip_FormFromMM(gMergeShipSave.mmForm));
        }
    }
    return form;
}

void MergeShip_SetPlayerForm(u8 form) {
    if (form >= PLAYER_FORM_MAX) {
        return;
    }
    gMergeShipSave.mmForm = MergeShip_FormToMM(form);
}

s32 MergeShip_HasTransformationMask(u8 mmForm) {
    return (mmForm < MM_FORM_HUMAN) && (gMergeShipSave.ownedMasks & (1 << mmForm));
}

void MergeShip_GiveTransformationMask(u8 mmForm) {
    if (mmForm < MM_FORM_HUMAN) {
        gMergeShipSave.ownedMasks |= (1 << mmForm);
    }
}

s32 MergeShip_HasMMMask(u8 mmMask) {
    return (mmMask > MM_MASK_NONE) && (mmMask < MM_MASK_MAX) && (gMergeShipSave.ownedMMMasks & (1u << mmMask));
}

void MergeShip_GiveMMMask(u8 mmMask) {
    if ((mmMask > MM_MASK_NONE) && (mmMask < MM_MASK_MAX)) {
        gMergeShipSave.ownedMMMasks |= (1u << mmMask);
    }
}

s32 MergeShip_HasMMMaskItem(u8 item) {
    if (MERGESHIP_ITEM_IS_TRANSFORMATION_MASK(item)) {
        return MergeShip_HasTransformationMask(item - ITEM_MM_MASK_FIERCE_DEITY);
    }
    if (MERGESHIP_ITEM_IS_MM_REGULAR_MASK(item)) {
        return MergeShip_HasMMMask(item - ITEM_MM_MASK_TRUTH + MM_MASK_TRUTH);
    }
    return false;
}

// MM keeps one B button per form and one C-button set for all of them (GET_CUR_FORM_BTN_ITEM). The
// human's B is whichever age is current, so it stays in gSaveContext.equips and OoT's own age swap
// keeps working; the MM forms' B items are parked in gMergeShipSave while the player is elsewhere.
void MergeShip_SwapFormEquips(u8 oldForm, u8 newForm) {
    u8 oldMM = MergeShip_FormToMM(oldForm);
    u8 newMM = MergeShip_FormToMM(newForm);

    if (oldMM == newMM) {
        return;
    }
    gMergeShipSave.bButtonItem[oldMM] = gSaveContext.equips.buttonItems[0];
    // BUG-0042: the Goron's and the Zora's B are fixed, as in MM (their punches; the Zora's fin boomerangs): whatever
    // reached their slot (a real sword, from an older save or an age change made mid-form), they get theirs back.
    if ((newMM == MM_FORM_GORON) || (newMM == MM_FORM_ZORA)) {
        gMergeShipSave.bButtonItem[newMM] = ITEM_SWORD_KOKIRI;
    }
    gSaveContext.equips.buttonItems[0] = gMergeShipSave.bButtonItem[newMM];
}

void MergeShip_DebugSetForm(u8 form) {
    if (form < PLAYER_FORM_MAX) {
        sDebugForm = form;
    }
}

u8 MergeShip_TakeDebugForm(void) {
    u8 form = sDebugForm;
    sDebugForm = 0xFF;
    return form;
}
