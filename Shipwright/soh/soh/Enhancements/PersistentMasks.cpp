#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/AdultMasks.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
}

#define CVAR_PERSISTENT_MASKS_NAME CVAR_ENHANCEMENT("PersistentMasks")

static void RegisterPersistentMasks() {
    bool persistentMasks = CVarGetInteger(CVAR_PERSISTENT_MASKS_NAME, 0);

    // A mask normally comes off the moment it leaves the buttons, keep it on instead
    COND_VB_SHOULD(VB_PLAYER_UNEQUIP_MASK_WITHOUT_BUTTON, persistentMasks, { *should = false; });

    // Put the remembered mask back on whenever the player actor is built, so it
    // survives loading zones, deaths and save loads
    COND_ID_HOOK(OnActorInit, ACTOR_PLAYER, persistentMasks, [](void* actorPtr) {
        Player* player = (Player*)actorPtr;

        // MergeShip2: a Majora's Mask form wears its own mask; the remembered one is a human's.
        if (MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
            return;
        }
        // MergeShip2: Player_Init already wore Majora's Mask's other masks back on, from
        // gMergeShipSave.equippedMMMask (MergeShip_InitFormOnSceneLoad -> MergeShip_MMMasks_RestoreOnSceneLoad,
        // soh/MergeShip/MergeShipMMMasks.c), before this hook runs (GameInteractor_ExecuteOnActorInit fires after
        // actor->init() returns). Those masks persist unconditionally, the way Majora's Mask itself keeps them, not
        // gated by this CVar -- gSaveContext.ship.maskMemory is Ocarina of Time's own mask memory, and MergeShip's
        // masks keep it at PLAYER_MASK_NONE while worn (MergeShip_MMMasks_Update), so overwriting currentMask here
        // would take the restored mask back off, and MergeShip_MMMasks_Update would then persist that loss into
        // equippedMMMask on the very next frame.
        if (PLAYER_MASK_IS_MM_REGULAR(player->currentMask)) {
            return;
        }
        // Forget the mask if it's gone from the inventory, or if adult can't wear it
        if (INV_CONTENT(ITEM_TRADE_CHILD) == ITEM_SOLD_OUT || (LINK_IS_ADULT && !Ship_MasksEquippableAsAdult())) {
            gSaveContext.ship.maskMemory = PLAYER_MASK_NONE;
        }
        player->currentMask = gSaveContext.ship.maskMemory;
    });
}

static RegisterShipInitFunc initFunc(RegisterPersistentMasks, { CVAR_PERSISTENT_MASKS_NAME });
