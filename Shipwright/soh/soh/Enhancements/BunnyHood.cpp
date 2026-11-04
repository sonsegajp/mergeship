#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/BunnyHood.h"
#include "soh/Enhancements/gameplaystats.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipForms.h" // MergeShip2: Majora's Mask's Bunny Hood
#include "soh/MergeShip/MergeShipSohFd.h" // MergeShip2 (soh_fd): Hyrule's masks worn over a form
extern PlayState* gPlayState;
}

// rando's Bunny Hood Effect setting forces the enhancement on, as logic may require it
BunnyHoodMode Ship_GetBunnyHoodMode() {
    if (IS_RANDO && RAND_GET_OPTION(RSK_BUNNY_HOOD)) {
        return BUNNY_HOOD_FAST_AND_JUMP;
    }
    return (BunnyHoodMode)CVarGetInteger(CVAR_BUNNY_HOOD_NAME, BUNNY_HOOD_VANILLA);
}

// MergeShip2: Majora's Mask's Bunny Hood is MM's own: Player_Action_13 runs at `speedTarget *= 1.5f`, and the
// speed carries into the jump (SoH's "Faster Run & Longer Jump" is that behaviour), whatever this setting says.
bool Ship_WearsMMBunnyHood(Player* player) {
    return player->currentMask == PLAYER_MASK_MM_REGULAR(MM_MASK_BUNNY);
}

// MergeShip2 (soh_fd): Hyrule's Bunny Hood, on Link or worn over a form ("Forms Wear Trade Masks"). A form's
// currentMask is the form's own mask, so the hood over it is asked of MergeShip_SohFd_WornHyruleMask.
static bool WearsHyruleBunnyHood(Player* player) {
    return (gPlayState != nullptr) ? (MergeShip_SohFd_WornHyruleMask(gPlayState, player) == PLAYER_MASK_BUNNY)
                                   : (player->currentMask == PLAYER_MASK_BUNNY);
}

float Ship_GetBunnyHoodRunFactor(Player* player) {
    if ((Ship_GetBunnyHoodMode() != BUNNY_HOOD_VANILLA && WearsHyruleBunnyHood(player)) ||
        Ship_WearsMMBunnyHood(player)) {
        return 1.5f;
    }
    return 1.0f;
}

// MergeShip2: the hood's run factor for C (z_player.c's sprint: a hood whose speed this does not give stacks there).
extern "C" float MergeShip_BunnyHoodRunFactor(Player* player) {
    return Ship_GetBunnyHoodRunFactor(player);
}

float Ship_GetBunnyHoodJumpFactor(Player* player) {
    if ((Ship_GetBunnyHoodMode() == BUNNY_HOOD_FAST_AND_JUMP && WearsHyruleBunnyHood(player)) ||
        Ship_WearsMMBunnyHood(player)) {
        return 1.5f;
    }
    return 1.0f;
}

static void RegisterBunnyHood() {
    bool bunnyHoodActive = Ship_GetBunnyHoodMode() != BUNNY_HOOD_VANILLA;

    // Wearing the hood shouldn't lock you out of NPC interactions, so NPCs ignore it
    COND_VB_SHOULD(VB_NPC_REACT_TO_MASK, bunnyHoodActive, {
        u8 currentMask = va_arg(args, int);
        if (currentMask == PLAYER_MASK_BUNNY) {
            *should = false;
        }
    });

    // Masks stay usable where trade items are restricted, so the hood can be equipped there
    COND_VB_SHOULD(VB_DISABLE_TRADE_ITEM_BUTTON, bunnyHoodActive, {
        u8 item = va_arg(args, int);
        if (item >= ITEM_MASK_KEATON && item <= ITEM_MASK_TRUTH) {
            *should = false;
        }
    });

    // Same speed boost as running, for the "Move in first person" setting (MergeShip2: 2S2H does this for MM's
    // Bunny Hood too, in Ship_HandleFirstPersonAiming)
    COND_VB_SHOULD(VB_PLAYER_MODIFY_FIRST_PERSON_SPEED, bunnyHoodActive || CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMMasks"), 1), {
        Player* player = va_arg(args, Player*);
        f32* movementSpeed = va_arg(args, f32*);
        *movementSpeed *= Ship_GetBunnyHoodRunFactor(player);
    });

    // Gameplay stat: time spent wearing the hood
    COND_HOOK(OnPlayerUpdate, bunnyHoodActive, []() {
        if (gSaveContext.ship.stats.gameComplete || (IS_BOSS_RUSH && gSaveContext.ship.quest.data.bossRush.isPaused)) {
            return;
        }
        if (GET_PLAYER(gPlayState)->currentMask == PLAYER_MASK_BUNNY) {
            gSaveContext.ship.stats.count[COUNT_TIME_BUNNY_HOOD]++;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterBunnyHood,
                                     { "IS_RANDO", CVAR_BUNNY_HOOD_NAME, CVAR_ENHANCEMENT("MergeShip.MMMasks") });
