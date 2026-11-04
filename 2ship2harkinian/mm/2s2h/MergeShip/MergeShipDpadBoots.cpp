// MergeShip2 -- D-pad Boots: Ocarina of Time's tunics and boots (MergeShipOoTEquip.h) on the D-pad, for the human.
//
// A tunic or boots bound to a D-pad button on the Equipment (Hyrule) view (2S2H's D-pad Equips; the user, 2026-10-01:
// "Assure boots and tunics are able to be bound to dpad in MM") is worn from it here: on, or the Kokiri one again
// (MergeShip_OoTEquip_ToggleWearable). Taken here, not in Player_ProcessItemButtons, which MM does not run while Link
// swims -- where the Iron Boots and the Zora Tunic are put on.
//
// Ship of Harkinian's "Assignable Shields, Tunics and Boots" (SoH: soh/Enhancements/Items/AssignableTunicsAndBoots.cpp)
// puts boots on an item button, and pressing it puts them on, or back to the Kokiri Boots if they are already on. This is
// the same thing in Termina without a pause-menu assignment: D-pad Left is the Iron Boots, D-pad Right the Hover Boots
// (when owned), on a D-pad button that holds no item (D-pad Equips off, or that button empty). The option is shared with
// SoH's (include/mergeship/SettingsMap.def) in mergeship.exe; it works in 2ship.exe too, with oot-assets.o2r.

#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#define CVAR_NAME "gEnhancements.MergeShip.DpadBoots"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

namespace {

struct DpadBoots {
    u16 button;
    DpadEquipSlot slot;
    s32 boots; // MergeShipOoTEquipSave::boots: 1 Iron, 2 Hover
};

const DpadBoots sDpadBoots[] = {
    { BTN_DLEFT, EQUIP_SLOT_D_LEFT, 1 },
    { BTN_DRIGHT, EQUIP_SLOT_D_RIGHT, 2 },
};

// D-pad Equips' item on that button wins.
bool SlotFree(DpadEquipSlot slot) {
    return !CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0) || (DPAD_GET_CUR_FORM_BTN_ITEM(slot) == ITEM_NONE);
}

// SoH's UseTunicBoots: "boots equip despite state", except in cutscenes, talking, dead, playing the ocarina.
bool CanChangeBoots(PlayState* play, Player* player) {
    return (play->pauseCtx.state == PAUSE_STATE_OFF) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
           !Player_InCsMode(play) && (player->transformation == PLAYER_FORM_HUMAN) &&
           !(player->stateFlags1 & (PLAYER_STATE1_20 | PLAYER_STATE1_400 | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD |
                                    PLAYER_STATE1_20000000)) &&
           !(player->stateFlags2 & PLAYER_STATE2_USING_OCARINA);
}

void UpdateDpadBoots(Actor* actor) {
    PlayState* play = gPlayState;
    if ((play == nullptr) || (actor != &GET_PLAYER(play)->actor) || !MergeShip_OoTEquip_Enabled()) {
        return;
    }
    Player* player = (Player*)actor;
    if (!CanChangeBoots(play, player)) {
        return;
    }
    const Input* input = &play->state.input[0];
    // What the player bound (D-pad Equips on).
    if (CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0)) {
        static const u16 kButtons[] = { BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };
        static const DpadEquipSlot kSlots[] = { EQUIP_SLOT_D_RIGHT, EQUIP_SLOT_D_LEFT, EQUIP_SLOT_D_DOWN,
                                                EQUIP_SLOT_D_UP };
        for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); i++) {
            const u8 item = DPAD_GET_CUR_FORM_BTN_ITEM(kSlots[i]);
            if (CHECK_BTN_ALL(input->press.button, kButtons[i]) && MergeShip_OoTEquip_IsWearableItem(item)) {
                MergeShip_OoTEquip_ToggleWearable(play, player, item);
                return;
            }
        }
    }
    if (!CVAR) {
        return; // the fixed D-pad Boots below are their own option
    }
    for (const DpadBoots& entry : sDpadBoots) {
        if (!CHECK_BTN_ALL(input->press.button, entry.button) || !SlotFree(entry.slot)) {
            continue;
        }
        // On again: back to the Kokiri Boots (always owned), as SoH's toggle does.
        const s32 boots = (MergeShip_OoTEquip_Save()->boots == entry.boots) ? 0 : entry.boots;
        if (MergeShip_OoTEquip_EquipBoots(play, boots)) {
            Player_PlaySfx(player, (boots == 1) ? NA_SE_PL_LAND_HEAVYBOOTS : NA_SE_PL_CHANGE_ARMS);
        }
        break;
    }
}

void RegisterDpadBoots() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, CVAR || CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0),
                 UpdateDpadBoots);
}

static RegisterShipInitFunc initFunc(RegisterDpadBoots, { CVAR_NAME, "gEnhancements.Dpad.DpadEquips" });

} // namespace
