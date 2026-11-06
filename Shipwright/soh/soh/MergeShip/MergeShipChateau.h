#pragma once

// MergeShip2 -- Majora's Mask's Chateau Romani in Hyrule (MergeShipChateau.cpp).
//
// The item: ITEM_MM_CHATEAU (MM ITEM_CHATEAU), drunk as PLAYER_IA_MM_BOTTLE_CHATEAU (MM PLAYER_IA_BOTTLE_CHATEAU).
// Ocarina of Time enumerates bottle contents as ranges (ITEM_BOTTLE..ITEM_POE, PLAYER_IA_BOTTLE..
// PLAYER_IA_BOTTLE_FAIRY); Chateau Romani comes after MergeShip's masks, so every such range also takes it
// (MERGESHIP_ITEM_IS_BOTTLE, Player_ActionToBottle -> PLAYER_BOTTLE_MM_CHATEAU).
//
// Where it comes from (a MergeShip feature, CVAR_ENHANCEMENT("MergeShip.RomaniChateau"), on by default): playing
// Epona's Song to a cow while wearing MM's Romani's Mask, with an empty bottle, fills the bottle with Chateau Romani
// instead of milk (MM GI_CHATEAU, "Your bottle was filled with Chateau Romani!"). A randomizer cow still gives its
// shuffled item first; once that is collected the cow follows the same rule.
//
// Drinking it (MM Player_Action_67): full health and full magic (D_8085D790's 1 | 2), then MM's
// WEEKEVENTREG_DRANK_CHATEAU_ROMANI: magic is not used up (Magic_Consume, Magic_Update, the Goron/Zora drains, the
// Fierce Deity's beam), the meter is drawn blue and its border keeps flashing. In MM the flag lasts until the
// three-day cycle is reset. Hyrule has no cycle, so here it lasts until the file is loaded again (it is never saved)
// or Link dies (the game over screen; a fairy's revival does not end it).
// CVAR_ENHANCEMENT("MergeShip.ChateauMagic") (on by default) turns that lasting effect off; the drink then only
// refills health and magic.

#include <stdint.h>
#include <libultraship/libultra.h>
#include "soh/Enhancements/item-tables/ItemTableTypes.h"

struct PlayState;
struct Actor;

// Player_ActionToBottle's index for PLAYER_IA_MM_BOTTLE_CHATEAU (MM PLAYER_BOTTLE_CHATEAU); sBottleColors has its
// colour (MM sPlayerBottleColors { 255, 255, 255 }).
#define PLAYER_BOTTLE_MM_CHATEAU 13

// Any bottle, empty or filled: Ocarina of Time's ITEM_BOTTLE..ITEM_POE, Chateau Romani, and MM's other bottle contents
// (ITEM_MM_DEKU_PRINCESS..ITEM_MM_HOT_SPRING_WATER, MergeShipMMItems.h).
#define MERGESHIP_ITEM_IS_BOTTLE(item)                                                                 \
    ((((item) >= ITEM_BOTTLE) && ((item) <= ITEM_POE)) || ((item) == ITEM_MM_CHATEAU) ||               \
     (((item) >= ITEM_MM_DEKU_PRINCESS) && ((item) <= ITEM_MM_HOT_SPRING_WATER)))

// The get-item entry's id in the MOD_NONE item table (registered at run time, outside GI_MAX so no vanilla GetItemID
// or table slot is taken), and its message.
#define MERGESHIP_GI_MM_CHATEAU 0x0180
#define MERGESHIP_TEXT_CHATEAU_FILLED 0x9390 // MM 0x91 (GI_CHATEAU)

#ifdef __cplusplus
extern "C" {
#endif

// MM CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI). Also for the Giant's Mask drain (MM Magic_Update's
// MAGIC_STATE_CONSUME_GIANTS_MASK takes nothing while it is set).
s32 MergeShip_Chateau_IsActive(void);
// Ends it (a file loaded, Link dead).
void MergeShip_Chateau_Reset(void);
// BUG-0153: drunk or not, as the other game left it (the shared save's chateauMagic, MergeShipSharedSave.cpp): the
// drink's endless magic follows Link between the worlds. Set (true) or ended (false).
s32 MergeShip_Chateau_Drunk(void);
void MergeShip_Chateau_SetDrunk(s32 drunk);

// MM GI_CHATEAU's get-item entry (MOD_NONE, getItemId MERGESHIP_GI_MM_CHATEAU), drawn with MM's model.
GetItemEntry MergeShip_Chateau_GetItemEntry(void);

// MM Player_Action_67 for PLAYER_IA_BOTTLE_CHATEAU: health and magic refilled, SET_WEEKEVENTREG(DRANK_CHATEAU_ROMANI).
void MergeShip_Chateau_Drink(struct PlayState* play);

// Item_Give(ITEM_MM_CHATEAU): MM Item_Give's ITEM_CHATEAU_2, filling the first empty bottle. Returns ITEM_NONE when a
// bottle was filled, ITEM_MM_CHATEAU otherwise (and runs the OnItemReceive hooks, as Item_Give's Return_Item does).
u8 MergeShip_Chateau_Give(struct PlayState* play);

// Item_CheckObtainability(ITEM_MM_CHATEAU): ITEM_NONE while an empty bottle can take it.
u8 MergeShip_Chateau_CheckObtainability(void);

// En_Cow: with Epona's Song played, the Romani's Mask worn and an empty bottle, the cow offers Chateau Romani in place
// of GI_MILK. Returns true when it did (the caller then does not offer milk).
s32 MergeShip_Chateau_CowOffer(struct Actor* cow, struct PlayState* play);

#ifdef __cplusplus
}
#endif
