#include "global.h"
#include "soh/MergeShip/MergeShipMaskReactions.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

// MergeShip2 -- see the header. The gates every MM mask reaction in an OoT actor goes through.

s32 MergeShip_CaptainsHatWorn(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return MERGESHIP_WEARS_MM_MASK(player, MM_MASK_CAPTAIN) &&
           CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.CaptainsHat"), 1);
}

s32 MergeShip_GibdoMaskWorn(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return MERGESHIP_WEARS_MM_MASK(player, MM_MASK_GIBDO) && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.GibdoMask"), 1);
}

// MM EnRd_ShouldNotDance, inverted:
//     if ((Player_GetMask(play) == PLAYER_MASK_GIBDO) || (Player_GetMask(play) == PLAYER_MASK_CAPTAIN) ||
//         (Player_GetMask(play) == PLAYER_MASK_GARO)) {
//         return false;
//     }
//     return true;
s32 MergeShip_RedeadsShouldDance(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.RedeadDance"), 1)) {
        return false;
    }
    return MERGESHIP_WEARS_MM_MASK(player, MM_MASK_GIBDO) || MERGESHIP_WEARS_MM_MASK(player, MM_MASK_CAPTAIN) ||
           MERGESHIP_WEARS_MM_MASK(player, MM_MASK_GARO);
}

s32 MergeShip_GuardWantsMMKeaton(PlayState* play) {
    return MERGESHIP_WEARS_MM_MASK(GET_PLAYER(play), MM_MASK_KEATON) && !gMergeShipSave.guardSawMMKeaton;
}

void MergeShip_GuardTakesMMKeaton(void) {
    gMergeShipSave.guardSawMMKeaton = 1;
}

s32 MergeShip_ReadsGossipStones(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (Player_GetMask(play) == PLAYER_MASK_TRUTH) {
        return true;
    }
    return MERGESHIP_WEARS_MM_MASK(player, MM_MASK_TRUTH) &&
           CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.TruthGossipStones"), 1);
}
