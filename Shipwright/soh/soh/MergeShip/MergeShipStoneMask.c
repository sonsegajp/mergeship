#include "global.h"
#include "soh/MergeShip/MergeShipStoneMask.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

// MergeShip2 -- see the header. The single gate every actor's Stone Mask check goes through.
s32 MergeShip_StoneMaskHides(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return MERGESHIP_WEARS_MM_MASK(player, MM_MASK_STONE) &&
           CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.StoneMask"), 1);
}
