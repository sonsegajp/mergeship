#pragma once

// MergeShip2 -- Majora's Mask's stray fairies in Hyrule (CVAR_ENHANCEMENT("MergeShip.StrayFairies"), on by default).
//
// The fairy is MM's En_Elforg, ported as ovl_MM_En_Elforg (every state: free floating, fairy fountain, trapped in a
// bubble, held by an enemy, hidden by a collider, from a chest, from a pot/crate/beehive, turned in; the collection
// sequence, its sounds and MM's texts). While one is free, or while something still holds one, it raises
// gMergeShipStrayFairyNearby (MM's ACTORCTX_FLAG_3), which the Great Fairy's Mask's hair reads; free fairies fly to
// the player's waist while that mask is worn (MM EnElforg_FreeFloating).
//
// The counts are MM's inventory.strayFairies, kept in gMergeShipSave.strayFairies[MMStrayFairyArea] (Clock Town's one
// fairy, then 15 per temple). Hyrule has none of MM's temples, so where MM uses "the dungeon the player is in"
// (gSaveContext.dungeonSceneSharedIndex) the area is carried explicitly: in the fairy's params, or given to the API.
//
// For a randomizer (nothing here is randomizer-specific):
//   * put a fairy anywhere:  MergeShip_StrayFairy_Spawn / MergeShip_StrayFairy_SpawnEx;
//   * put one in any actor that breaks or opens (a pot, crate, beehive, chest, or any other):
//                            MergeShip_StrayFairy_AttachToContainer (released as MM releases it);
//   * give one without an actor (an NPC, a shop, a get-item):
//                            MergeShip_StrayFairy_Give(play, area), or Item_Give(ITEM_MM_STRAY_FAIRY) after
//                            MergeShip_StrayFairy_SetGiveArea(area), or the get-item entries
//                            MERGESHIP_GI_MM_STRAY_FAIRY(area) (MM's text, the fairy drawn above Link's head);
//   * decide what a collected fairy gives: MergeShip_StrayFairy_SetCollectOverride (MM 2S2H's
//                            VB_GIVE_ITEM_FROM_ELFORG); each fairy carries a u32 `userData` for it.

#include "z64.h"
#include "soh/Enhancements/item-tables/ItemTableTypes.h"

// MM StrayFairyArea (the Great Fairy types), the index of gMergeShipSave.strayFairies.
typedef enum MMStrayFairyArea {
    /* 0 */ MM_STRAY_FAIRY_AREA_CLOCK_TOWN,
    /* 1 */ MM_STRAY_FAIRY_AREA_WOODFALL,
    /* 2 */ MM_STRAY_FAIRY_AREA_SNOWHEAD,
    /* 3 */ MM_STRAY_FAIRY_AREA_GREAT_BAY,
    /* 4 */ MM_STRAY_FAIRY_AREA_STONE_TOWER,
    /* 5 */ MM_STRAY_FAIRY_AREA_MAX
} MMStrayFairyArea;

// MM StrayFairyType (z_en_elforg.h).
typedef enum MMStrayFairyType {
    /* 0 */ MM_STRAY_FAIRY_TYPE_FREE_FLOATING,
    /* 1 */ MM_STRAY_FAIRY_TYPE_FAIRY_FOUNTAIN,
    /* 2 */ MM_STRAY_FAIRY_TYPE_BUBBLE,
    /* 3 */ MM_STRAY_FAIRY_TYPE_CLOCK_TOWN,
    /* 4 */ MM_STRAY_FAIRY_TYPE_ENEMY,
    /* 5 */ MM_STRAY_FAIRY_TYPE_COLLIDER,
    /* 6 */ MM_STRAY_FAIRY_TYPE_CHEST,
    /* 7 */ MM_STRAY_FAIRY_TYPE_COLLECTIBLE,
    /* 8 */ MM_STRAY_FAIRY_TYPE_RETURNING_TO_FOUNTAIN
} MMStrayFairyType;

// MM STRAY_FAIRY_SCATTERED_TOTAL: the fairies of one temple.
#define MM_STRAY_FAIRY_SCATTERED_TOTAL 15

// MM STRAY_FAIRY_PARAMS(flag, nonDungeonArea, type). `flag` is the scene flag the fairy is saved in: a collectible flag
// for MM_STRAY_FAIRY_TYPE_COLLECTIBLE, a treasure flag for MM_STRAY_FAIRY_TYPE_CHEST, a switch flag otherwise (MM).
// MergeShip addition: MERGESHIP_STRAY_FAIRY_FLAG_NONE (MM's 7-bit field at its maximum) means "no scene flag" -- the
// fairy is never removed by a flag and sets none; whoever spawned it tracks it (userData + the collect override).
#define MERGESHIP_STRAY_FAIRY_FLAG_NONE 0x7F
#define MM_STRAY_FAIRY_PARAMS(flag, area, type) ((((flag) & 0x7F) << 9) | (((area) & 7) << 6) | ((type) & 0xF))

// Messages (MM's English text from mm.o2r's message_data_static, in Ocarina of Time's control codes).
#define MERGESHIP_TEXT_STRAY_FAIRY_FOUND 0x93E0      // MM 0x11 "You found a Stray Fairy! This is your Nth one!"
#define MERGESHIP_TEXT_STRAY_FAIRY_FOUND_ALL 0x93E1  // MM 0xF3 (MM's 0x11 on the 15th)
#define MERGESHIP_TEXT_STRAY_FAIRY_CLOCK_TOWN 0x93E2 // MM 0x579 "Please, hear my plea! ..."

// Get-item entries in the MOD_NONE table, one per area (registered at run time above GI_MAX, after Chateau's 0x180).
// MM GI_STRAY_FAIRY: no model and text 0x11; here the fairy is drawn above Link's head (2S2H's DrawStrayFairy).
#define MERGESHIP_GI_MM_STRAY_FAIRY_BASE 0x0181
#define MERGESHIP_GI_MM_STRAY_FAIRY(area) (MERGESHIP_GI_MM_STRAY_FAIRY_BASE + (area))
#define MERGESHIP_GI_IS_MM_STRAY_FAIRY(gi) \
    (((gi) >= MERGESHIP_GI_MM_STRAY_FAIRY_BASE) && ((gi) < MERGESHIP_GI_MM_STRAY_FAIRY_BASE + MM_STRAY_FAIRY_AREA_MAX))

#ifdef __cplusplus
extern "C" {
#endif

struct MMEnElforg;

// ---- state ------------------------------------------------------------------------------------------------------

// CVAR_ENHANCEMENT("MergeShip.StrayFairies") (default 1) and mm.o2r mounted.
s32 MergeShip_StrayFairy_IsEnabled(void);
// Found so far in `area` (MM inventory.strayFairies[area]; Clock Town is 0 or 1).
s32 MergeShip_StrayFairy_GetCount(s32 area);
// The most a player can hold in `area`: 1 for Clock Town, 15 for a temple.
s32 MergeShip_StrayFairy_GetMax(s32 area);

// ---- spawning ----------------------------------------------------------------------------------------------------

// A free-floating stray fairy of `area` at `pos` (MM STRAY_FAIRY_TYPE_FREE_FLOATING), saved in switch flag `flag` of
// the current scene (or MERGESHIP_STRAY_FAIRY_FLAG_NONE). NULL when stray fairies are off, the flag is already set,
// or the actor could not be spawned.
Actor* MergeShip_StrayFairy_Spawn(PlayState* play, Vec3f* pos, s32 area, s32 flag);
// Any type, with the u32 the collect override receives. For MM_STRAY_FAIRY_TYPE_BUBBLE the fairy follows `parent`
// (MM spawns it as the bubble's child); for MM_STRAY_FAIRY_TYPE_ENEMY `pos` must be the enemy's home position (MM
// finds the enemy by it; MergeShip_StrayFairy_AttachToEnemy does that for you).
Actor* MergeShip_StrayFairy_SpawnEx(PlayState* play, Vec3f* pos, s32 area, s32 type, s32 flag, Actor* parent,
                                    u32 userData);
// MM STRAY_FAIRY_TYPE_ENEMY: the fairy is held by `enemy` (an ACTORCAT_ENEMY actor) and freed when it dies.
Actor* MergeShip_StrayFairy_AttachToEnemy(PlayState* play, Actor* enemy, s32 area, s32 flag, u32 userData);

// ---- containers --------------------------------------------------------------------------------------------------

// `container` holds a stray fairy of `area`, as MM's pots, barrels, crates, beehives (Item_CanDropBigFairy) and
// chests (GI_STRAY_FAIRY) do: while it does, gMergeShipStrayFairyNearby is raised every frame it updates, and its
// update is no longer culled (MM ACTOR_FLAG_UPDATE_CULLING_DISABLED).
//   * An En_Box chest releases the fairy when its lid has opened (MM EnBox_Open, STRAY_FAIRY_TYPE_CHEST, saved in the
//     chest's treasure flag, `flag` ignored). The chest gives no item of its own: Link kicks it open and does not
//     hold anything up (MM GI_STRAY_FAIRY). Refused (false) if the chest was already opened.
//   * Any other actor releases it when it is broken (Actor_Kill), 40 units above it (MM Item_DropCollectible's
//     ITEM00_BIG_FAIRY: STRAY_FAIRY_TYPE_COLLECTIBLE, collectible flag `flag`, NA_SE_EV_BUTTERFRY_TO_FAIRY). Refused
//     (false) if collectible flag `flag` is already set.
// Nothing is saved: attach again whenever the container is spawned (e.g. from OnActorInit). Returns false when stray
// fairies are off or the table (32 containers per scene) is full.
s32 MergeShip_StrayFairy_AttachToContainer(Actor* container, s32 area, s32 flag, u32 userData);
// Forget `container`'s fairy (it will not be released).
void MergeShip_StrayFairy_DetachFromContainer(Actor* container);
// True while `container` holds a fairy.
s32 MergeShip_StrayFairy_ContainerHoldsFairy(Actor* container);

// ---- giving ------------------------------------------------------------------------------------------------------

// A stray fairy of `area` is added to the player's (Item_Give(ITEM_MM_STRAY_FAIRY)). No text: a get-item's own
// message, or the fairy actor's, shows MM's.
void MergeShip_StrayFairy_Give(PlayState* play, s32 area);
// The area the next Item_Give(ITEM_MM_STRAY_FAIRY) counts in (used once). Without it: the area of the stray fairy
// get-item the player is holding up, else the last area given.
void MergeShip_StrayFairy_SetGiveArea(s32 area);
// Item_Give's ITEM_MM_STRAY_FAIRY branch (z_parameter.c). Returns ITEM_NONE (the item-receive hooks run).
u8 MergeShip_StrayFairy_GiveItem(PlayState* play);
// MM's get-item entry for `area` (MOD_NONE, getItemId MERGESHIP_GI_MM_STRAY_FAIRY(area)).
GetItemEntry MergeShip_StrayFairy_GetItemEntry(s32 area);

// A collected fairy (the player touched it) asks this first. Return true when you gave something yourself: MM's own
// give (the count, "You found a Stray Fairy!", the Clock Town plea) is then skipped. The fairy's scene flag is set and
// the collect animation plays either way (MM EnElforg_FreeFloating with VB_GIVE_ITEM_FROM_ELFORG false).
typedef s32 (*MergeShipStrayFairyCollectFunc)(PlayState* play, Actor* fairy, s32 area, s32 type, u32 userData);
void MergeShip_StrayFairy_SetCollectOverride(MergeShipStrayFairyCollectFunc func);

// ---- used by the actor and the hooks (MergeShipStrayFairy.c / .cpp) -----------------------------------------------

// The fairy was touched: MM EnElforg_FreeFloating's give. Returns true when the actor must wait for the Clock Town
// textbox (MM EnElforg_ClockTownFairyCollected).
s32 MergeShip_StrayFairy_OnCollected(PlayState* play, Actor* fairy, s32 area, s32 type, u32 userData);
// Per frame for every updating actor (OnActorUpdate): containers raise the flag and release their fairy.
void MergeShip_StrayFairy_OnActorUpdate(Actor* actor);
// OnActorKill / OnActorDestroy.
void MergeShip_StrayFairy_OnActorKill(Actor* actor);
void MergeShip_StrayFairy_OnActorDestroy(Actor* actor);
// OnVanillaBehavior VB_GIVE_ITEM_FROM_CHEST for a chest holding a fairy (MergeShip_StrayFairy_ContainerHoldsFairy):
// Link kicks the lid open and holds nothing up (MM Player_Action_65's GI_STRAY_FAIRY branch, func_80839E74).
void MergeShip_StrayFairy_KickOpenChest(PlayState* play);
// The text the fairy's messages show: the area and count of the fairy just found (MM's MESSAGE_STRAY_FAIRIES reads
// the current dungeon's count). A get-item's text opens before Item_Give counts the fairy, so then it is count + 1.
void MergeShip_StrayFairy_GetMessageArea(s32* area, s32* count);
// MM gStrayFairyGlowingCircleIconTex colours of `area` (z_message.c sStrayFairyIconPrim/EnvColors).
void MergeShip_StrayFairy_GetIconColors(s32 area, Color_RGB8* prim, Color_RGB8* env);
// A textbox's stray fairy icon as MM draws it: the area's glowing circle (drawn at x, y) and the area's fairy, loaded for
// the caller's 32 x 24 rectangle (Message_DrawItemIcon's).
void MergeShip_StrayFairy_DrawTextIcon(Gfx** gfxP, s32 area, s32 x, s32 y, u8 alpha);

// 2S2H DrawStrayFairy: the fairy of `area` at the current matrix (the get-item model).
void MergeShip_StrayFairy_DrawModel(PlayState* play, s32 area);

// The Masks page's stray fairy panel (MM KaleidoScope_DrawDungeonMap's icon and KaleidoScope_DrawDungeonStrayFairyCount,
// for the four temples), drawn in the page's model space below the mask grid.
void MergeShip_StrayFairy_DrawMaskPagePanel(PlayState* play);

// The actor's runtime id (ovl_MM_En_Elforg), -1 until registered.
extern s16 gMergeShipActorId_MM_En_Elforg;

#ifdef __cplusplus
}
#endif
