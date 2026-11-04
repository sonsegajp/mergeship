// MergeShip2 -- Majora's Mask's equipment: the SoH hooks (see MergeShipMMEquipment.h).
//
// - Age: the Hero's Bow (and the magic arrows it shoots, as MM's human does) and MM's Hookshot are usable by young
//   Link while they are the variant their slot is switched to; with MergeShip.TimelessMMGear so are Ocarina of Time's
//   swords and shields that have MM partners (VB_ITEM_MEETS_AGE_REQ / VB_SLOT_MEETS_AGE_REQ, as SoH's
//   TimelessEquipment and AdultMasks do). The equipment page's own check is z_kaleido_scope.h CHECK_AGE_REQ_EQUIP.
// - Every player frame the save is kept consistent (MergeShip_MMEquip_Sanitize): which variant each slot uses, and
//   the B button following the sword slots.
// - A Like Like taking an MM shield (OnEquipmentDelete).
// - Quick play: with gGeneral.MergeShipAllMMEquipment set, every loaded file outside a randomizer seed owns the seven
//   items (for trying them before they have a place in Hyrule).

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "variables.h"
#include "functions.h"
#include "soh/MergeShip/MergeShipMMEquipment.h"
#include "soh/MergeShip/MergeShipForms.h"
extern PlayState* gPlayState;
}

#define CVAR_MM_EQUIPMENT_NAME CVAR_ENHANCEMENT("MergeShip.MMEquipment")

namespace {

void GiveAllMMEquipment() {
    if (!CVarGetInteger(CVAR_GENERAL("MergeShipAllMMEquipment"), 0) || IS_RANDO) {
        return;
    }
    for (s32 id = 0; id < MMEQUIP_MAX; id++) {
        if (!MergeShip_MMEquip_Owns(id)) {
            MergeShip_MMEquip_Give(nullptr, MERGESHIP_MMEQUIP_ITEM(id));
        }
    }
}

void RegisterMMEquipment() {
    bool enabled = CVarGetInteger(CVAR_MM_EQUIPMENT_NAME, 1) != 0;

    COND_VB_SHOULD(VB_ITEM_MEETS_AGE_REQ, enabled, {
        int item = va_arg(args, int);

        if (MergeShip_MMEquip_ItemIgnoresAge(item)) {
            *should = true;
        }
    });

    COND_VB_SHOULD(VB_SLOT_MEETS_AGE_REQ, enabled, {
        int slot = va_arg(args, int);

        if (MergeShip_MMEquip_InvSlotIgnoresAge(slot)) {
            *should = true;
        }
    });

    COND_HOOK(OnEquipmentDelete, enabled, [](int16_t equipmentType, uint16_t equipValue) {
        MergeShip_MMEquip_OnEquipmentDelete(equipmentType, equipValue);
    });

    // Always: with the feature off it turns an MM sword on B back into its Ocarina of Time partner.
    COND_HOOK(OnPlayerUpdate, true, []() { MergeShip_MMEquip_Sanitize(gPlayState); });

    COND_HOOK(OnLoadGame, true, [](int32_t fileNum) {
        (void)fileNum;
        GiveAllMMEquipment();
        MergeShip_MMEquip_Sanitize(nullptr);
    });
}

static RegisterShipInitFunc sInitFunc(RegisterMMEquipment, { CVAR_MM_EQUIPMENT_NAME });

// MergeShip2: "All Majora's Mask Equipment" takes effect the moment it is turned on (or "Majora's Mask Equipment",
// which MergeShip_MMEquip_Give needs, is): with a file loaded, the items are given now, as loading the file gives
// them. Turning it off takes nothing away (a reload does not either).
void ApplyAllMMEquipmentNow() {
    if (GameInteractor::IsSaveLoaded(true)) {
        GiveAllMMEquipment();
    }
}

static RegisterShipInitFunc sAllMMEquipmentNow(ApplyAllMMEquipmentNow,
                                               { CVAR_GENERAL("MergeShipAllMMEquipment"), CVAR_MM_EQUIPMENT_NAME });

} // namespace

// MergeShip2: a copy of a list cut where a texture starts being loaded (include/mergeship/DlCut.hpp): MM's first-person
// Hookshot without young Link's hand, for the adult (MergeShipMMEquipmentDraw.inc).
#include <mergeship/DlCut.hpp>

extern "C" Gfx* MergeShip_MMEquip_CutDLBeforeTexture(const char* path, const char* texture) {
    return (Gfx*)MsDl_CutBeforeTexture(path, texture);
}
