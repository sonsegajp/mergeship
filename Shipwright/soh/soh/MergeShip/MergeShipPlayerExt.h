#pragma once

// MergeShip2 -- player state Majora's Mask's forms need beyond the Player struct.
//
// There is only ever one Player, so state MM keeps on its Player that Ocarina of Time's does not is
// kept here instead of growing z64player.h, which nearly every translation unit includes. Only the
// player's own code and MergeShip's include this header.

#include "z64.h"

// MM PlayerBoots. MM gives each form a "boots" index into its movement table (D_801BFE14); Ocarina of
// Time's currentBoots keeps its own meaning (Kokiri/Iron/Hover) for OoT code and actors.
typedef enum MMPlayerBoots {
    /* 0 */ MM_BOOTS_FIERCE_DEITY,
    /* 1 */ MM_BOOTS_HYLIAN,
    /* 2 */ MM_BOOTS_GIANT,
    /* 3 */ MM_BOOTS_DEKU,
    /* 4 */ MM_BOOTS_ZORA_LAND,
    /* 5 */ MM_BOOTS_ZORA_UNDERWATER,
    /* 6 */ MM_BOOTS_GORON,
    /* 7 */ MM_BOOTS_7,
    /* 8 */ MM_BOOTS_MAX
} MMPlayerBoots;

// Player_ActionToMeleeWeapon's value for PLAYER_IA_MM_SWORD_TWO_HANDED (Fierce Deity's sword, MM
// PLAYER_MELEEWEAPON_SWORD_TWO_HANDED). Ocarina of Time's own values are 0..5; the tables indexed by it
// (sMeleeWeaponLengths, sSwordTypes, z_player.c D_80854488) carry a 7th entry.
#define MERGESHIP_MELEEWEAPON_MM_TWO_HANDED 6
// ...and for MM's Razor and Gilded Swords (MM PLAYER_MELEEWEAPON_SWORD_RAZOR / _GILDED; MergeShipMMEquipment.h). The
// Great Fairy's Sword is MM's two-handed sword, MERGESHIP_MELEEWEAPON_MM_TWO_HANDED.
#define MERGESHIP_MELEEWEAPON_MM_RAZOR 7
#define MERGESHIP_MELEEWEAPON_MM_GILDED 8

typedef struct MergeShipPlayerExt {
    u8 mmBoots; // MM Player::currentBoots, valid while the player is an MM form
} MergeShipPlayerExt;

#ifdef __cplusplus
extern "C" {
#endif

extern MergeShipPlayerExt gMergeShipPlayerExt;

// MM func_80123140 (OoT's Player_SetBootData) for an MM form.
void MergeShip_SetFormBootData(PlayState* play, Player* player);

#ifdef __cplusplus
}
#endif
