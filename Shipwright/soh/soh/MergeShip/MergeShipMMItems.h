#pragma once

// MergeShip2 -- Majora's Mask's remaining C-button items, bottle contents and trade items in Hyrule.
//
//   MergeShipMMItems.c    the items' rules: Item_Give, the save data they live in, the bottle contents (colours,
//                         pouring, showing, the Hot Spring Water's bottle timer), the trade items, the Powder Keg's
//                         player side (MM Player_InitExplosiveIA / sPlayerExplosiveInfo, ammo, the keg timer).
//   MergeShipMMItems.cpp  the SoH side: the "mergeShipMMItems" save section, get-item entries (MM models from
//                         mm.o2r) and get-item text, pause-menu names, the hooks.
//   z_kaleido_mm_item.c   the pause menu's "Items (Termina)" face (MERGESHIP_PAUSE_MM_ITEM, MergeShipPause.h).
//   ovl_MM_En_Bom_Keg     MM En_Bom's powder-keg branch as an actor of its own (MergeShipActorMMEnBomKeg.cpp).
//   ovl_MM_Obj_Aqua       MM Obj_Aqua, the water poured out of a bottle (MergeShipActorMMObjAqua.cpp).
//   MergeShipPicto.*      the Pictograph Box: MM's first-person picto box, the photo, its display.
//
// Everything is a MergeShip option: CVAR_ENHANCEMENT("MergeShip.MMItems") (default on) and its sub-options
// (MergeShip_MMItems_Option*). With the option off the items are still kept in the save (nothing is lost), but
// Item_Give refuses them, the Termina items face is not drawn and the player does not use them.

#include <libultraship/libultra.h>
#include "z64item.h"
#include "z64player.h"
#include "soh/Enhancements/item-tables/ItemTableTypes.h"

struct PlayState;
struct Player;
struct Actor;

// ---- the items ------------------------------------------------------------------------------------------------

// MM's bottle contents, in item-id order (ITEM_MM_DEKU_PRINCESS .. ITEM_MM_HOT_SPRING_WATER), and their item actions
// (PLAYER_IA_MM_BOTTLE_DEKU_PRINCESS .. PLAYER_IA_MM_BOTTLE_HOT_SPRING_WATER, the same order).
#define MERGESHIP_ITEM_IS_MM_BOTTLE_CONTENT(item) \
    (((item) >= ITEM_MM_DEKU_PRINCESS) && ((item) <= ITEM_MM_HOT_SPRING_WATER))
#define MERGESHIP_IA_IS_MM_BOTTLE_CONTENT(ia) \
    (((ia) >= PLAYER_IA_MM_BOTTLE_DEKU_PRINCESS) && ((ia) <= PLAYER_IA_MM_BOTTLE_HOT_SPRING_WATER))

// MM's trade items (Moon's Tear .. Pendant of Memories) and their item actions.
#define MERGESHIP_ITEM_IS_MM_TRADE(item) (((item) >= ITEM_MM_MOONS_TEAR) && ((item) <= ITEM_MM_PENDANT_OF_MEMORIES))
#define MERGESHIP_IA_IS_MM_TRADE(ia) (((ia) >= PLAYER_IA_MM_MOONS_TEAR) && ((ia) <= PLAYER_IA_MM_PENDANT_OF_MEMORIES))

// Player_ActionToBottle's indices of MM's bottle contents: after Ocarina of Time's thirteen and Chateau Romani's
// (PLAYER_BOTTLE_MM_CHATEAU 13), in item-action order. sBottleColors (z_player_lib.c) has MM's colour for each
// (MM sPlayerBottleColors).
#define PLAYER_BOTTLE_MM_FIRST 14
#define PLAYER_BOTTLE_MM_DEKU_PRINCESS 14
#define PLAYER_BOTTLE_MM_ZORA_EGG 15
#define PLAYER_BOTTLE_MM_GOLD_DUST 16
#define PLAYER_BOTTLE_MM_MUSHROOM 17
#define PLAYER_BOTTLE_MM_SEAHORSE 18
#define PLAYER_BOTTLE_MM_HYLIAN_LOACH 19
#define PLAYER_BOTTLE_MM_SPRING_WATER 20
#define PLAYER_BOTTLE_MM_HOT_SPRING_WATER 21
#define PLAYER_BOTTLE_MM_LAST PLAYER_BOTTLE_MM_HOT_SPRING_WATER
#define MERGESHIP_IA_TO_MM_BOTTLE(ia) (PLAYER_BOTTLE_MM_FIRST + ((ia) - PLAYER_IA_MM_BOTTLE_DEKU_PRINCESS))

// MM's Powder Keg is the Goron's third explosive (MM PLAYER_EXPLOSIVE_POWDER_KEG). Player_ActionToExplosive returns
// this for PLAYER_IA_MM_POWDER_KEG, after Ocarina of Time's bomb (0) and bombchu (1).
#define MERGESHIP_PLAYER_EXPLOSIVE_POWDER_KEG 2

// MM ITEM_POWDER_KEG's capacity (MM CUR_CAPACITY(UPG_...)-less: Item_Give sets AMMO to 1).
#define MERGESHIP_POWDER_KEG_MAX_AMMO 1

// MM's item page (z_kaleido_item.c, 6 x 4). The Termina items face is laid out the same way; the slots that hold
// items living in Ocarina of Time's grid (bow, bombs, bottles ...) stay empty there.
#define MERGESHIP_MM_ITEM_PAGE_SLOTS 24
#define MERGESHIP_MM_SLOT_OCARINA 0         // MM SLOT_OCARINA: the ocarina (one for both worlds)
#define MERGESHIP_MM_SLOT_BOW 1             // MM SLOT_BOW: MM's Hero's Bow
#define MERGESHIP_MM_SLOT_ARROW_FIRE 2      // MM SLOT_ARROW_FIRE .. SLOT_ARROW_LIGHT (4)
#define MERGESHIP_MM_SLOT_BOMB 6            // MM SLOT_BOMB, SLOT_BOMBCHU (7), SLOT_DEKU_STICK (8), SLOT_DEKU_NUT (9),
                                            // SLOT_MAGIC_BEANS (10)
#define MERGESHIP_MM_SLOT_LENS 14           // MM SLOT_LENS_OF_TRUTH
#define MERGESHIP_MM_SLOT_BOTTLE_1 18       // MM SLOT_BOTTLE_1 .. SLOT_BOTTLE_6 (23)
#define MERGESHIP_MM_SLOT_POWDER_KEG 12     // MM SLOT_POWDER_KEG
#define MERGESHIP_MM_SLOT_PICTOGRAPH_BOX 13 // MM SLOT_PICTOGRAPH_BOX
#define MERGESHIP_MM_SLOT_HOOKSHOT 15       // MM SLOT_HOOKSHOT (hook for ITEM_MM_HOOKSHOT's port)
#define MERGESHIP_MM_SLOT_SWORD_GREAT_FAIRY 16 // MM SLOT_SWORD_GREAT_FAIRY (hook for its port)
#define MERGESHIP_MM_SLOT_TRADE_DEED 5      // MM SLOT_TRADE_DEED: Moon's Tear -> Land -> Swamp -> Mountain -> Ocean
#define MERGESHIP_MM_SLOT_TRADE_KEY_MAMA 11 // MM SLOT_TRADE_KEY_MAMA: Room Key, Letter to Mama
#define MERGESHIP_MM_SLOT_TRADE_COUPLE 17   // MM SLOT_TRADE_COUPLE: Letter to Kafei, Pendant of Memories

// The get-item entries (MOD_NONE item table, registered at run time above GI_MAX like MERGESHIP_GI_MM_CHATEAU 0x180).
// This workstream owns get-item ids 0x0190-0x019F (16 ids) for 19 items: MergeShip_MMItems_GetItemId gives them to the
// items an actor can hand over; Spring Water, Hot Spring Water (MM scoops both, it never hands them over) and the
// Hylian Loach (unused in MM, its get-item texts are empty) have none (GI_NONE) and come only through Item_Give.
#define MERGESHIP_GI_MM_ITEMS_BASE 0x0190
#define MERGESHIP_GI_MM_ITEMS_LAST 0x019F
// Their get-item texts: MERGESHIP_TEXT_MM_ITEMS_BASE + (item - ITEM_MM_POWDER_KEG), 0x93A0-0x93B2. This workstream owns
// text ids 0x93A0-0x93BF.
#define MERGESHIP_TEXT_MM_ITEMS_BASE 0x93A0
// MM 0xFA: "Your Hot Spring Water cooled..." (Interface_UpdateBottleTimers).
#define MERGESHIP_TEXT_MM_HOT_WATER_COOLED 0x93BE
// MM 0xFE: "This item doesn't seem to work here..." (MM's text when an item is shown to nobody).
#define MERGESHIP_TEXT_MM_ITEM_NO_EFFECT 0x93BF

#ifdef __cplusplus
extern "C" {
#endif

// ---- options --------------------------------------------------------------------------------------------------

// CVAR_ENHANCEMENT("MergeShip.MMItems"), default on, and mm.o2r mounted.
s32 MergeShip_MMItems_Enabled(void);
// CVAR_ENHANCEMENT("MergeShip.MMItems.HotWaterCools"), default on: MM's 60-second bottle timer.
s32 MergeShip_MMItems_HotWaterCools(void);
// CVAR_ENHANCEMENT("MergeShip.MMItems.KegCarriesOver"), default on: MM Player_Init re-lights a keg carried through a
// loading zone (gSaveContext.powderKegTimer).
s32 MergeShip_MMItems_KegCarriesOver(void);

// ---- ownership (save section "mergeShipMMItems") ---------------------------------------------------------------

// What MM keeps in its item grid for the items that have no Ocarina of Time slot.
typedef struct MergeShipMMItemsSave {
    u8 powderKeg;     // MM INV_CONTENT(ITEM_POWDER_KEG): ITEM_MM_POWDER_KEG or ITEM_NONE
    u8 powderKegAmmo; // MM AMMO(ITEM_POWDER_KEG)
    u8 pictographBox; // MM INV_CONTENT(ITEM_PICTOGRAPH_BOX)
    u8 tradeDeed;     // MM INV_CONTENT(SLOT_TRADE_DEED): the latest of Moon's Tear .. Ocean Title Deed, or ITEM_NONE
    u8 tradeKeyMama;  // MM INV_CONTENT(SLOT_TRADE_KEY_MAMA): Room Key or Letter to Mama
    u8 tradeCouple;   // MM INV_CONTENT(SLOT_TRADE_COUPLE): Letter to Kafei or Pendant of Memories
    u16 tradeOwned;   // every trade item owned, bit (item - ITEM_MM_MOONS_TEAR): A on a trade slot cycles through them
} MergeShipMMItemsSave;
extern MergeShipMMItemsSave gMergeShipMMItemsSave;
// MM's new-file values (nothing owned).
void MergeShip_MMItems_InitSave(void);
// The trade items (Moon's Tear .. Pendant of Memories): each owned one is kept (tradeOwned), the slot shows one of them.
#define MERGESHIP_MM_TRADE_BIT(item) (1u << ((item) - ITEM_MM_MOONS_TEAR))
// A on a trade slot of the Termina items face (MERGESHIP_MM_SLOT_TRADE_*): the slot's next owned item, and the buttons
// that held the old one hold it (the user, 2026-09-24: "press a + swap" for the title deeds and the other shared
// slots). True when it changed.
s32 MergeShip_MMItems_TradeCycle(struct PlayState* play, s16 pageSlot);

// The item held in a Termina-items slot (MERGESHIP_MM_SLOT_*), or ITEM_NONE.
// Hook for other MM items without an Ocarina of Time slot (Great Fairy's Sword, MM Hookshot ...): add the slot's
// case in MergeShip_MMItems_PageSlotItem (MergeShipMMItems.c) and the page shows and equips it.
u8 MergeShip_MMItems_PageSlotItem(s16 slot);
// The Ocarina of Time inventory slot a Termina-items cell's item is equipped through (its own slot in Ocarina of Time's
// grid: the ocarina, bombs, bottles ...), or SLOT_NONE for an item that has none there (keg, box, MM's gear ...).
s16 MergeShip_MMItems_PageSlotOoTSlot(s16 slot);
// True when the item is one this module gives a Termina-items slot (keg, box, trade items).
s32 MergeShip_MMItems_IsPageItem(u8 item);
// Owns the item (a page item; bottle contents are in Ocarina of Time's bottle slots).
s32 MergeShip_MMItems_Owns(u8 item);

// MM AMMO(ITEM_POWDER_KEG).
u8 MergeShip_MMItems_GetKegAmmo(void);
void MergeShip_MMItems_ChangeKegAmmo(s16 change);

// The MM actors these items spawn, registered with ActorDB at boot (-1 until then): ovl_MM_En_Bom_Keg (the keg) and
// ovl_MM_Obj_Aqua (poured water).
extern s16 gMergeShipActorId_MM_En_Bom_Keg;
extern s16 gMergeShipActorId_MM_Obj_Aqua;

// MM gSaveContext.powderKegTimer: the fuse of the keg being carried (frames, 0 = none). Read and written by the
// keg actor (ovl_MM_En_Bom_Keg) exactly as MM En_Bom does, and by the player (MM Player_InitExplosiveIA,
// Player_Init, the death branch).
extern u16 gMergeShipPowderKegTimer;

// ---- Item_Give / Item_CheckObtainability (z_parameter.c) ------------------------------------------------------

// True for every item this module gives (the hook's condition).
s32 MergeShip_MMItems_Handles(u8 item);
// MM Item_Give for the item. Returns what Item_Give returns (ITEM_NONE when given).
u8 MergeShip_MMItems_Give(struct PlayState* play, u8 item);
// MM Item_CheckObtainability.
u8 MergeShip_MMItems_CheckObtainability(u8 item);
// The item's MOD_NONE get-item entry, drawn with MM's model from mm.o2r.
GetItemEntry MergeShip_MMItems_GetItemEntry(u8 item);
// The item's get-item id (MERGESHIP_GI_MM_ITEMS_BASE..LAST), or GI_NONE for the three without one (see above).
s16 MergeShip_MMItems_GetItemId(u8 item);

// ---- the player (z_player.c / z_player_lib.c) --------------------------------------------------------------------

// Player_ItemToItemAction for this module's items (PLAYER_IA_NONE when the option is off). -1 when not one of them.
s32 MergeShip_MMItems_ItemToItemAction(u8 item);
// Player_ActionToBottle for MM's bottle contents; -1 otherwise.
s32 MergeShip_MMItems_ActionToBottle(s32 itemAction);

// MM Player_UseItem's "cutscene item" test: bottles and trade items go through Player_ActionHandler_13.
s32 MergeShip_MMItems_IsCutsceneItemAction(s32 itemAction);
// Player_Action_ExchangeItem for a shown MM item (exchangeItemId MERGESHIP_EXCH_ITEM_MM): its get-item entry (the
// model held up; gi 0 for a bottle, which stays in the hand). False when the exchange is not one of these.
s32 MergeShip_MMItems_ExchangeGetItemEntry(struct PlayState* play, struct Player* player, GetItemEntry* entry);
// MM Player_Action_70 (pouring Spring Water / Hot Spring Water): the actor to spawn at frame 76. False when the
// bottle holds no MM content that is poured.
s32 MergeShip_MMItems_PourActor(s32 itemAction, s16* actorId, s16* params);

// Powder Keg: MM Player_InitExplosiveIA's keg branch. The caller has already checked PLAYER_STATE1_CARRYING_ACTOR.
void MergeShip_MMItems_InitKegIA(struct PlayState* play, struct Player* player);
// The player died (MM func_80831F34): gMergeShipPowderKegTimer = 0.
void MergeShip_MMItems_OnPlayerDeath(void);

// Defined in MergeShipMMItemsPlayer.inc (included at the end of z_player.c, for its static helpers):
// MM Player_ActionHandler_13 for MM's bottle contents and trade items (unk_6AD == 4). Returns true when it handled
// the item action (the caller returns 1, as its own branches do).
s32 MergeShip_MMItems_ActionHandler13(struct PlayState* play, struct Player* player);
// MM Player_Init: a keg carried through a loading zone (gMergeShipPowderKegTimer != 0) is picked up again.
void MergeShip_MMItems_PlayerInit(struct PlayState* play, struct Player* player);

// Player::exchangeItemId while an MM item is shown (past Ocarina of Time's EXCH_ITEM_MAX, so no Hyrule actor takes
// it for one of its own).
#define MERGESHIP_EXCH_ITEM_MM 0x40

// The pause menu's magic-arrow equip effect kept its arrows in pauseCtx->equipTargetItem as 0xBF..0xC1 while they
// flew to the bow; those ids are MM items now (ITEM_MM_POWDER_KEG ..), so the effect uses ids no item has.
#define MERGESHIP_MAGIC_ARROW_EQUIP_ITEM 0xF9

// ---- the interface (z_parameter.c) --------------------------------------------------------------------------------

// MM Interface_UpdateBottleTimers: Hot Spring Water cools into Spring Water after 60 unpaused seconds. Every frame.
void MergeShip_MMItems_UpdateBottleTimers(struct PlayState* play);
// MM Interface_StartBottleTimer(60, bottle): Hot Spring Water went into bottle `bottleIndex` (0..3).
void MergeShip_MMItems_StartBottleTimer(s16 bottleIndex);
// A file was loaded: no bottle timer runs (MM keeps them outside the save).
void MergeShip_MMItems_ResetBottleTimers(void);
// Ocarina of Time's human buttons (Interface_UpdateButtons, OoT's branch): MM's items follow MM's human column of
// gPlayerFormItemRestrictions (the keg is the Goron's) and the trade items Ocarina of Time's trade-item class.
void MergeShip_MMItems_UpdateHumanButtons(struct PlayState* play, s16* restoreHud);
// Ammo drawn on a C button (MM draws the keg's count as it draws bombs').
s32 MergeShip_MMItems_HasButtonAmmo(u8 item, s16* ammo);

// ---- pause menu ---------------------------------------------------------------------------------------------

// MM item_name_static texture of the item (English; 2S2H's archive carries only English), or NULL.
const char* MergeShip_MMItems_NameTexture(u8 item);

#ifdef __cplusplus
}
#endif
