#pragma once

// MergeShip2 -- the Adult Mask in Hyrule (the player's request, 2026-09-28; 1:1 with 2Ship's in Termina,
// 2s2h/MergeShip/MergeShipAdultMask.h).
//
// The mask switches Link between Ocarina of Time's own ages -- the real adult and the real child, as the Master Sword
// does -- with Majora's Mask's transformation cutscene, in place: no scene reload. Put on, it plays the transformation
// (the mask on his face, the flash); at the flash gSaveContext.linkAge flips and Ocarina of Time's own age-equipment
// swap runs (MergeShip_AdultMask_SwitchAge: Inventory_SwapAgeEquipment, the same call a scene's own age change makes
// in src/code/z_play.c), then the player is rebuilt in place as the new age exactly the way MergeShip rebuilds it
// after any other transformation (MergeShipPlayerTransform.inc's MergeShip_ReinitAsForm), and the scene fades back in
// from the flash's white. It works both ways (adult Link wearing it becomes the child, in the current room, standing
// in whatever era's room he's in) and from every form (the form ends with it: Link comes back as the human of the
// other age). The mask is the Fierce Deity mask wearing adult Link's face (include/mergeship/AdultMask.hpp makes it,
// its name and its icon at startup from the player's own archives).
//
// While Link is the adult it is the Child Mask (the user, 2026-10-01): the Skull Kid's Link mask, its own icon and
// name ("Child Mask"), the same item. The adult puts it on as young Link puts on the Adult Mask -- MM's whole
// transformation cutscene, every time (skippable once seen, as MM's), the mask on his face (fitted to his head,
// MaskFit.h MS_MASK_FIT_CHILD_MASK), its scream model as he screams -- and at the flash he is the child.
//
// Every file starts with it. It sits on the Fierce Deity's Mask's cell of the Masks page: the cell shows the Adult Mask,
// and once the Fierce Deity's Mask is owned too, A arms a swap -- an arrow beside the cell, Right on the Adult Mask, Left
// on the Fierce Deity's Mask -- and that direction swaps which of the two the cell shows (and which a button equipped
// from it holds). The item is ITEM_MM_MASK_ADULT; Player_ItemToItemAction makes it PLAYER_IA_MM_MASK_FIERCE_DEITY, so
// the transformation mask's rules apply, and MergeShip_UseTransformationMask leads the transformation to the age change.
//
// State: gMergeShipSave.ownedMasks, whose bits 0..3 are the transformation masks (MMPlayerForm; the shared save carries
// only those), keeps MERGESHIP_OWNED_MASKS_SLOT_FD (the cell shows the Fierce Deity's Mask). The age is the game's own.
// Gated by MERGESHIP_ADULT_MASK_CVAR (default on) and the mask's art being mounted.

#include <libultraship/libultra/types.h>

#define MERGESHIP_ADULT_MASK_CVAR "gEnhancements.MergeShip.AdultMask"

#define MERGESHIP_OWNED_MASKS_SLOT_FD 0x40

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Player;

// ---- state (MergeShipAdultMask.c) -------------------------------------------------------------------------------------
// The option is on and the mask's art is mounted -- or the mask holds Link at the other age, so it can always be taken
// off (drawn as the Fierce Deity's Mask it is made from when its art is missing).
s32 MergeShip_AdultMask_Owned(void);
s32 MergeShip_AdultMask_ShowsAdult(void); // the Fierce Deity cell shows the Adult Mask
// The item is the Child Mask now: Link is the adult (and its art is mounted).
s32 MergeShip_AdultMask_IsChildMask(void);
// The pause name (the Masks page), or NULL for any other item.
const char* MergeShip_AdultMask_NameTexture(u16 item);
// The item's icon follows Link's age (the Adult Mask's, or the Child Mask's while he is the adult): gItemIcons, which
// the HUD and the pause screen draw from. Called whenever the age may have changed (the age change, Player_Init).
void MergeShip_AdultMask_SyncIcon(void);

// ---- the Masks page (z_kaleido_mask.c) ----------------------------------------------------------------------------------
// The shared cell's swap: A arms it, then the arrow's direction swaps (Right from the Adult Mask, Left from the Fierce
// Deity's Mask). `onCell`: the cursor is on the Fierce Deity's cell. Returns true when it used this frame's input (the
// cursor stays put).
s32 MergeShip_AdultMask_UpdatePageSwap(struct PlayState* play, s32 onCell, s32 stickX, s32 stickY);
s32 MergeShip_AdultMask_SwapArmed(void);
// The armed arrow points right (the cell shows the Adult Mask) or left (the Fierce Deity's Mask).
s32 MergeShip_AdultMask_SwapArrowRight(void);

// ---- the player (z_player.c, MergeShipPlayerTransform.inc, z_player_lib.c) ---------------------------------------------
// Player_UseItem, first: remembers whether the item being used is the Adult Mask. Returns true when it refused it.
s32 MergeShip_AdultMask_UseItem(struct PlayState* play, struct Player* player, s32 item);
// MergeShip_UseTransformationMask, first: the Adult Mask was used. Clears the note.
s32 MergeShip_AdultMask_TakeUsing(void);
// The transformation began; the Fierce Deity mask being drawn on (or in the hand of) `player` is then the Adult Mask
// (the Child Mask, when the adult put it on).
void MergeShip_AdultMask_BeginTransform(void);
s32 MergeShip_AdultMask_Drawing(struct Player* player);
// The mask's list; `scream`: as Link screams (MM's own masks swap to their scream model then), when it was made.
const char* MergeShip_AdultMask_DL(s32 scream);
// BUG-0057: what `player` (Link, or an Anchor teammate's dummy) wears as the Fierce Deity's mask slot: 0 that mask, 1 the
// Adult Mask, 2 the Child Mask; its list; and a dummy's, set from its packets.
u8 MergeShip_AdultMask_DrawingKind(struct Player* player);
const char* MergeShip_AdultMask_DLFor(struct Player* player, s32 scream);
void MergeShip_AdultMask_SetRemote(struct Player* dummy, u8 kind);
// z_player_lib.c, the mask on the head: the Child Mask is modelled for the Skull Kid's head; on the adult's it is
// placed by its fit (MaskFit.h MS_MASK_FIT_CHILD_MASK), applied to the matrix stack here. Nothing for any other mask.
void MergeShip_AdultMask_ApplyHeadFit(struct Player* player);
// The transformation's flash (MergeShipPlayerTransform.inc's MergeShip_Action_PutOnMask): flips gSaveContext.linkAge,
// runs Ocarina of Time's own age-equipment swap (Inventory_SwapAgeEquipment) and refreshes the B/C/D-pad icons for
// it, then ends the transforming state -- MergeShip_ReinitAsForm (called right after, the same as any other
// transformation) rebuilds the player in place as the new age.
void MergeShip_AdultMask_SwitchAge(struct PlayState* play, struct Player* player);
// Play_Init's scene setup: whether the world loads its adult era. While the Adult Mask has changed Link's age the
// world keeps the timeline it was in when the mask went on (gMergeShipSave.adultMaskTimelineAge); otherwise Link's age
// (LINK_IS_ADULT), as Ocarina of Time chooses it.
s32 MergeShip_AdultMask_TimelineIsAdult(void);
// The world's era, for every test that decides the state of the world rather than Link's own body (the user,
// 2026-09-30: the mask must never change the world's era): the scene layers (z_play.c), the cutscenes a scene starts
// with (z_demo.c), the scene's era visuals (z_scene_table.c), and what the actors of an era are -- who is where, what is
// flooded, frozen, drained or broken, which story state an NPC is in. LINK_IS_ADULT stays for Link himself: his size,
// his equipment, what an NPC says about how he looks, which way the Master Sword sends him. Without an Adult Mask age
// it is LINK_IS_ADULT, so play without the mask is Ocarina of Time's.
#define MERGESHIP_WORLD_IS_ADULT() (MergeShip_AdultMask_TimelineIsAdult())
// Player_Init: a safety net now that the age change no longer reloads the scene on its own (kept in case something
// else leaves the transforming state set, e.g. a save made mid-transformation) -- lifts the flash's white.
void MergeShip_AdultMask_OnSceneLoad(struct PlayState* play);

// ---- the art (MergeShipAdultMask.cpp) ------------------------------------------------------------------------------------
// OTRGlobals, after Majora's Mask's archive is mounted: the mask's art from the player's archives.
void MergeShip_AdultMask_Mount(void);

#ifdef __cplusplus
}
#endif
