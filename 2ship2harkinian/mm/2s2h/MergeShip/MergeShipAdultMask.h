#pragma once

// MergeShip2 -- the Adult Mask in Termina (the player's request, 2026-09-28; SoH has the same mask in Hyrule).
//
// The mask switches the human between the two ages Termina already has (MergeShipAdultLink.h: young Link, and Ocarina
// of Time's adult Link; the age is the save's and crosses the door to Hyrule both ways), with Majora's Mask's
// transformation cutscene: the mask on his face, the flash, and he is rebuilt as the other age -- young Link becomes
// the adult, the adult young Link, and a form (Deku, Goron, Zora, the Fierce Deity) ends with it, as MM's
// transformation masks end each other. The mask is the Fierce Deity mask wearing adult Link's face
// (include/mergeship/AdultMask.hpp makes it, its name and its icon at startup from the player's own archives).
//
// While the human is the adult it is the Child Mask (the user, 2026-10-01): the Skull Kid's Link mask, its own icon and
// name ("Child Mask"), the same item. The adult puts it on as young Link puts on the Adult Mask -- MM's whole human
// transformation cutscene, every time (skippable once seen, as MM's), the mask on his face (fitted to his head,
// MaskFit.h MS_MASK_FIT_CHILD_MASK), its scream model as he screams -- and at the flash he is young Link.
//
// Every file starts with it. It sits on the Fierce Deity mask's cell of the Masks page: that cell shows the Adult Mask,
// and once the Fierce Deity's Mask is owned too, A arms a swap -- an arrow beside the cell, Right on the Adult Mask, Left
// on the Fierce Deity's Mask -- and that direction swaps which of the two the cell shows (and which one a button
// equipped from it holds). The inventory keeps the Fierce Deity's Mask exactly as Majora's Mask does (its slot is
// ITEM_NONE until it is found), so everything that asks about it is unchanged.
//
// On buttons the mask is ITEM_MS_MASK_ADULT (ITEM_CB, unused by Majora's Mask outside its message icons, which never
// show this item; not 0xB5..0xB7, which the pause screen uses for its magic arrows). Player_ItemToItemAction makes it
// PLAYER_IA_MASK_FIERCE_DEITY, so Majora's Mask's own rules for using a transformation mask apply; the player hooks
// (MergeShipAdultMaskPlayer.inc) turn the Fierce Deity's transformation into the adult's.
//
// State: ShipSaveInfo::mergeShipHumanAge, whose bit 0 is the human's age (MergeShipAdultLink.h), keeps one more bit:
// MERGESHIP_HUMAN_AGE_BIT_SLOT_FD (the cell shows the Fierce Deity's Mask). MERGESHIP_HUMAN_AGE_BIT_BY_MASK is a bit an
// earlier build used; it is cleared.
// Gated by MERGESHIP_ADULT_MASK_CVAR (default on) and the adult (MergeShip_AdultLink_Enabled).

#include "ultra64.h"

#define MERGESHIP_ADULT_MASK_CVAR "gEnhancements.MergeShip.AdultMask"

#define ITEM_MS_MASK_ADULT ITEM_CB

#define MERGESHIP_HUMAN_AGE_BIT_BY_MASK 0x02
#define MERGESHIP_HUMAN_AGE_BIT_SLOT_FD 0x04

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Player;

// ---- state (MergeShipAdultMask.c) -------------------------------------------------------------------------------------
s32 MergeShip_AdultMask_Owned(void);      // the option is on, the adult exists here, and the mask's art is mounted
s32 MergeShip_AdultMask_ShowsAdult(void); // the Fierce Deity cell shows the Adult Mask
// What a Masks page cell shows (maskSlot 0..MASK_NUM_SLOTS - 1): the inventory's item, or the Adult Mask on the Fierce
// Deity's cell.
u8 MergeShip_AdultMask_MaskPageItem(s32 maskSlot);
// The item is the Child Mask now: the human is the adult (and its art is mounted).
s32 MergeShip_AdultMask_IsChildMask(void);
// The pause name (Kaleido_LoadItemNameStatic), or NULL for any other item.
const char* MergeShip_AdultMask_NameTexture(u32 item);

// ---- the Masks page (z_kaleido_mask.c) ----------------------------------------------------------------------------------
// KaleidoScope_UpdateMaskCursor, before the cursor moves: A on the shared cell arms the swap, then the arrow's way swaps. Returns
// true when it used this frame's input (the cursor stays put).
s32 MergeShip_AdultMask_UpdatePageSwap(struct PlayState* play);
// KaleidoScope_DrawMaskSelect, after the icons: the arrow beside the cell while the swap is armed.
void MergeShip_AdultMask_DrawPagePrompt(struct PlayState* play);

// ---- the player (MergeShipAdultMaskPlayer.inc, z_player.c, z_player_lib.c) ----------------------------------------------
// Player_UseItem, first: remembers whether the item being used is the Adult Mask. Returns true when it refused it (the
// error sound is played).
s32 MergeShip_AdultMask_UseItem(struct PlayState* play, struct Player* player, u8 item);
// Player_ActionHandler_13 with a mask to put on: the Adult Mask's transformation. Returns true when it started it.
s32 MergeShip_AdultMask_ActionHandler13(struct Player* player, struct PlayState* play);
// Every frame the player updates: the transformation's cutscene is over; the item's icon follows the human's age (the
// Adult Mask's, or the Child Mask's while he is the adult: gItemIcons, and the buttons holding it).
void MergeShip_AdultMask_PlayerUpdate(struct PlayState* play, struct Player* player);
// The Fierce Deity mask being drawn on (or in the hand of) `player` is the Adult Mask; its list then.
s32 MergeShip_AdultMask_Drawing(struct Player* player);
// ... and it is the Child Mask (the adult put it on).
s32 MergeShip_AdultMask_DrawingChild(struct Player* player);
// The mask's list; `scream`: as Link screams (MM's own masks swap to their scream model then), when it was made.
const char* MergeShip_AdultMask_DL(s32 scream);
// BUG-0057: what `player` (Link, or an Anchor teammate's dummy) wears as the Fierce Deity's mask slot: 0 that mask, 1 the
// Adult Mask, 2 the Child Mask; its list; and a dummy's, set from its packets.
u8 MergeShip_AdultMask_DrawingKind(struct Player* player);
const char* MergeShip_AdultMask_DLFor(struct Player* player, s32 scream);
void MergeShip_AdultMask_SetRemote(struct Player* dummy, u8 kind);
// The transformation began (target: adult or young); applied when the player is rebuilt (MergeShip_AdultMask_Sync).
void MergeShip_AdultMask_BeginTransform(s32 toAdult);
// MergeShip_AdultLink_Sync (Player_Init), first: the transformation's result into the save, before the age is read.
void MergeShip_AdultMask_Sync(void);

#ifdef __cplusplus
}
#endif
