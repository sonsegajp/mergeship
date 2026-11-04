// MergeShip2 -- the local player's MM state that the forms' drawing reads, swapped out while Anchor draws
// another player in a form (Network/Anchor/MergeShipAnchor.cpp). The form headers are C-only (their
// prototypes name a parameter `this`), so the swap lives in C.

#include "z64.h"
#include "soh/MergeShip/MergeShipPlayerExt.h"
#include "soh/MergeShip/forms/MergeShipGoron.h"
#include "soh/MergeShip/forms/MergeShipZora.h"

typedef struct MergeShipAnchorDrawState {
    u8 mmBoots;
    s8 goronMeleeWeaponAnimation;
    u8 goronTireMarkActive;
    s8 zoraMeleeWeaponAnimation;
    u8 zoraBlureValid;
} MergeShipAnchorDrawState;

static MergeShipAnchorDrawState sSaved;
static u8 sSwapped = false;

// Before a remote player's form is drawn: its MM boots, and none of the local player's trails, colliders
// or tire mark.
void MergeShipAnchor_SwapDrawState(u8 mmBoots) {
    if (sSwapped) {
        return;
    }
    sSaved.mmBoots = gMergeShipPlayerExt.mmBoots;
    sSaved.goronMeleeWeaponAnimation = gMergeShipGoron.meleeWeaponAnimation;
    sSaved.goronTireMarkActive = gMergeShipGoron.tireMarkActive;
    sSaved.zoraMeleeWeaponAnimation = gMergeShipZora.meleeWeaponAnimation;
    sSaved.zoraBlureValid = gMergeShipZora.blureValid;
    sSwapped = true;

    gMergeShipPlayerExt.mmBoots = mmBoots;
    gMergeShipGoron.meleeWeaponAnimation = -1; // no punch collider or trail on a remote player's fist
    gMergeShipGoron.tireMarkActive = false;    // the tire mark is the local player's
    gMergeShipZora.meleeWeaponAnimation = -1;
    gMergeShipZora.blureValid = false; // the second fin trail is the local player's
}

void MergeShipAnchor_RestoreDrawState(void) {
    if (!sSwapped) {
        return;
    }
    gMergeShipPlayerExt.mmBoots = sSaved.mmBoots;
    gMergeShipGoron.meleeWeaponAnimation = sSaved.goronMeleeWeaponAnimation;
    gMergeShipGoron.tireMarkActive = sSaved.goronTireMarkActive;
    gMergeShipZora.meleeWeaponAnimation = sSaved.zoraMeleeWeaponAnimation;
    gMergeShipZora.blureValid = sSaved.zoraBlureValid;
    sSwapped = false;
}
