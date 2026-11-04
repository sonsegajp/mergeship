#pragma once

// MergeShip2 -- items both games have that are one inventory slot in Ocarina of Time: bombs, bombchus, Deku Sticks,
// Deku Nuts, magic beans (and the bow, the ocarina), the quiver, bomb bag and stick / nut capacities, and the bottles.
// Which game owns each is in the shared save (include/mergeship/SharedSave.h MsSharedSave.hyruleOwn / terminaOwn /
// hyruleBags / terminaBags), written live by both engines as items are given (2Ship's side:
// 2s2h/MergeShip/MergeShipOwnership.h).
//
// The user's rule (2026-09-24): every item is its own game's -- on its own game's menu only, in both games -- and usable
// in either game as soon as it is found ("you can use ANY mm item and OOT item when you get them"; "you should ONLY get
// the MM version on ITS OWN menu"). So Hyrule holds Majora's Mask's bombs (and the rest) in Ocarina of Time's slot and
// shows them on the Items (Termina) page only; Ocarina of Time's Select Item page shows a slot only when Ocarina of Time
// owns its item (MergeShip_MMEquip_OoTPageSlotItem), its Equipment page Ocarina of Time's own bags
// (z_kaleido_equipment.c).
//
// The count is ONE shared pool (task 1, 2026-09-30: the user reversed the earlier per-game split -- bombs, bombchus,
// arrows, Deku Sticks and Deku Nuts are the same count in Hyrule and Termina, carried across the Lost Woods door).
// MsSharedSave.ammo holds it; its capacity is the better bag/quiver level either game has found (hyruleBags /
// terminaBags, MAX of the two) except the Bombchu Bag (chuBagLevel, its own progression, task 3). Both pages show that
// one count (MergeShip_Own_PageAmmo) -- there is no more "copy in use" to switch, so OnPageEquip / OnUseItem are now
// no-ops kept only so their call sites (z_kaleido_item.c, z_kaleido_mm_item.c, Player_UseItem) need no change. Majora's
// Mask's bottles are the shared save's `bottles`, on the Items (Termina) page and on a button as
// MS_MM_BOTTLE_BUTTON_SLOT; Ocarina of Time's four are its own slots. With the unified seed's "Combine Shared Items"
// both games' are one inventory slot too, as before.
//
// Outside mergeship.exe (no shared save) everything is Ocarina of Time's, as always.

#ifdef __cplusplus
extern "C" {
#endif
#include "z64.h"

// Button slots (gSaveContext.equips.cButtonSlots) past Ocarina of Time's 24 inventory slots: Majora's Mask's bottle
// `i` (0 .. 5), and Majora's Mask's copy of MsOwnBit MS_OWN_BOMB (0) .. MS_OWN_BEAN (4).
#define MS_MM_BOTTLE_BUTTON_SLOT_FIRST 0x40
#define MS_MM_BOTTLE_BUTTON_SLOT(i) (MS_MM_BOTTLE_BUTTON_SLOT_FIRST + (i))
#define MS_MM_BOTTLE_BUTTON_SLOT_IS(slot) \
    (((slot) >= MS_MM_BOTTLE_BUTTON_SLOT_FIRST) && ((slot) < MS_MM_BOTTLE_BUTTON_SLOT_FIRST + 6))
#define MS_MM_AMMO_BUTTON_SLOT_FIRST 0x48
#define MS_MM_AMMO_BUTTON_SLOT(ownBit) (MS_MM_AMMO_BUTTON_SLOT_FIRST + (ownBit))
#define MS_MM_AMMO_BUTTON_SLOT_IS(slot) \
    (((slot) >= MS_MM_AMMO_BUTTON_SLOT_FIRST) && ((slot) <= MS_MM_AMMO_BUTTON_SLOT_FIRST + 4))

// Ocarina of Time owns `item` (ITEM_BOMB, ITEM_BOMBCHU, ITEM_STICK, ITEM_NUT, ITEM_BEAN): its own copy, not only Majora's
// Mask's held in its slot. Any other item: whether its slot holds it.
s32 MergeShip_Own_OoTOwns(u8 item);
s32 MergeShip_Own_DebugSet(u8 item, s32 hyrule, s32 termina); // harness "ownbit"
// Majora's Mask owns it (Termina's copy, held here in Ocarina of Time's slot). False outside mergeship.exe.
s32 MergeShip_Own_MMOwns(u8 item);
// Each game's own quiver / bomb bag level (UPG_QUIVER, UPG_BOMB_BAG), 0 .. 3; any other upgrade: CUR_UPG_VALUE.
s32 MergeShip_Own_OoTUpgrade(s32 upgrade);
s32 MergeShip_Own_MMUpgrade(s32 upgrade);
// item.cpp (the randomizer's progressive bow and bomb bag): the level `logicValue` means for Ocarina of Time's next copy
// -- Ocarina of Time's own while the game plays (`saveContext` is gSaveContext), else `logicValue` (the seed
// generation's simulated save).
s32 MergeShip_Own_OoTUpgradeForLogic(s32 upgrade, s32 logicValue, const void* saveContext);

// Item_Give of an Ocarina of Time item (MergeShip_MMEquip_OnOoTItemGiven), before it is given: Ocarina of Time owns it
// (or its bag), and a new item or bag makes Ocarina of Time's copy the one in use, so the give fills Ocarina of Time's.
void MergeShip_Own_OnOoTItemGiven(u8 item);
// A Deku Baba's stick (BUG-0001): Hyrule's sticks, as OoT gives them -- owned, with a bag -- even while the slot holds only
// Termina's (a single stick from grass or a pot only adds to those, MergeShip_Own_OnOoTItemGiven).
void MergeShip_Own_GrantOoTSticks(void);
// Majora's Mask's copy given here (MsOwnBit), a Majora's Mask bag reaching `level` (UPG_QUIVER, UPG_BOMB_BAG), and
// Majora's Mask's count of MsOwnBit `ownBit` filled to its capacity (a new bag comes full): the unified randomizer's
// Termina items found in Hyrule (MergeShipUnified.cpp), MM's Hero's Bow (its quiver).
void MergeShip_Own_GiveMM(s32 ownBit);
void MergeShip_Own_RaiseMMUpgrade(s32 upgrade, s32 level);
void MergeShip_Own_FillMM(s32 ownBit);
// Every frame (MergeShip_MMEquip_Sanitize): the slots hold what either game owns, at the shared pool's capacity, and
// its one count is kept in the shared save.
void MergeShip_Own_Sync(void);
// An apply or a file load changed the shared save: the next Sync binds the pool's counts again.
void MergeShip_Own_Unbind(void);
// Vestigial since ammo became one pool (task 1): no-ops, kept so their call sites need no change.
void MergeShip_Own_OnPageEquip(u8 item, s32 mmPage);
void MergeShip_Own_OnUseItem(u8 item);
// The one shared count a page shows for `item` (ITEM_BOMB .. ITEM_BEAN, ITEM_BOW for the arrows); `mmCopy` is unused
// (both pages show the same count) and kept only for the call sites.
s16 MergeShip_Own_PageAmmo(s16 item, s32 mmCopy);

// ---- the Bombchu Bag (task 3) ----
// True while no bombchu ammo is obtainable anywhere (a chest, a check, a drop, a shop) in either game: the unified
// seed uses the Bombchu Bag (MsSharedSave.hyruleChuBag) and neither game has raised it yet (chuBagLevel == 0).
s32 MergeShip_Own_ChusLocked(void);
// SoH's own randomizer raised its Bombchu Bag to `level` (0-3, SeedContext.cpp): merge it into the shared save (only
// raises, never lowers). 2Ship has no progression of its own; it only reads chuBagLevel back. The bag's chus are
// Ocarina of Time's (hyruleOwn), and its count goes into the pool.
void MergeShip_Own_RaiseOoTChuBag(s32 level);
// The bombchu capacity in effect: the unified seed's Bombchu Bag level (the pool's), else SoH's own (the randomizer's
// progressive bag, or 50). Never the bomb bag's (Anchor's MS_AMMO_SYNC clamps to it).
s32 MergeShip_Own_ChuCapacity(void);

// ---- bottles ----
// Majora's Mask's bottles are the shared save's here (not combined, in mergeship.exe).
s32 MergeShip_Own_MMBottlesHere(void);
// Majora's Mask's bottle `index` (0 .. 5) as an item (ITEM_NONE: none).
u8 MergeShip_Own_MMBottle(s32 index);
// What a button slot holds: the inventory's slot, or Majora's Mask's bottle / copy for its button slot.
u8 MergeShip_Own_ButtonSlotItem(u8 slot);
// Inventory_UpdateBottleItem for Majora's Mask's bottle button slot: its content is now `item`.
void MergeShip_Own_SetButtonSlotBottle(u8 slot, u8 item);
// A new bottle of Majora's Mask's holding `content` (ITEM_BOTTLE: empty). False when it has six.
s32 MergeShip_Own_GiveMMBottle(u8 content);
// Item_Give of a bottle content when none of Ocarina of Time's bottles is empty: Majora's Mask's first empty bottle.
s32 MergeShip_Own_FillMMBottle(PlayState* play, u8 content);
s32 MergeShip_Own_MMHasEmptyBottle(void);
// Inventory_ConsumeFairy when Ocarina of Time's bottles hold none: Majora's Mask's.
s32 MergeShip_Own_ConsumeMMFairy(PlayState* play);
// Older builds put Majora's Mask's bottles in Hyrule's slots (MsSharedSave.hyruleBottleMM); Sync moves them.
s32 MergeShip_Own_HyruleBottleIsMM(s32 index);

// MergeShipSharedSave.cpp: bottle contents (MsBottle) and items, and the seed's "Combine Shared Items".
u8 MergeShipShared_ItemFromBottle(u8 bottle);
u8 MergeShipShared_BottleFromItem(u8 item);
s32 MergeShipShared_CombinedInGame(void);

#ifdef __cplusplus
}
#endif
