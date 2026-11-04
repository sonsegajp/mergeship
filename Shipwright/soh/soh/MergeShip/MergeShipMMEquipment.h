#pragma once

// MergeShip2 -- Majora's Mask's equipment in Ocarina of Time: the Razor, Gilded and Great Fairy's Swords, the Hero's
// and Mirror Shields, the Hero's Bow and MM's Hookshot (2ship2harkinian/mm).
//
// Each MM item is a variant of the Ocarina of Time item whose slot it shares; the slot's selection says which of them
// is in use:
//   - swords and shields: the Equipment face has two views, switched with C-Up (z_kaleido_mergeship.c). Ocarina of
//     Time's own Equipment page shows and equips only Ocarina of Time's swords and shields -- A on one equips it and
//     switches its slot back from an MM variant (MergeShip_MMEquip_PauseSelectOoT). The "Termina" view shows MM's --
//     the Kokiri Sword, Razor Sword, Gilded Sword, Great Fairy's Sword, Hero's Shield and MM Mirror Shield -- and A
//     there switches the slot to it and equips it (MergeShip_MMEquip_TerminaEquip). Each view draws the "equipped"
//     outline only on the item in use, so the two agree. Slot pairs: Kokiri Sword / Razor / Gilded (on B), Biggoron's
//     Sword (or Giant's Knife) / Great Fairy's Sword (on B), Hylian / Hero's Shield, Mirror / MM Mirror Shield (the
//     Deku Shield has no MM pair).
//   - Items page: Fairy Bow <-> Hero's Bow, Hookshot/Longshot <-> MM Hookshot (A, then left/right, as the Deku
//     Beans switch).
// A slot's Ocarina of Time equipment value and inventory item stay what they were, so everything in Hyrule that asks
// "is a Hylian Shield / Mirror Shield / Bow / Hookshot equipped" keeps working; which of the pair it is decides its
// model, damage, reach and age rule. A sword variant is its own B item (ITEM_MM_SWORD_*), with MM's item action.
//
// An MM item owned without its Ocarina of Time partner occupies the partner's slot (the slot's bit / inventory item is
// set, and MergeShipSaveData.mmEquipSlotOnlyMM remembers that the partner itself is not owned). Behaviour:
//   - swords: MM's item actions (PLAYER_IA_MM_SWORD_*), MM's damage (MM D_8085D09C: Razor 2/4 = OoT's Master Sword
//     flags, Gilded 3/6 = the Master Sword's table entry x1.5, Great Fairy's 4/8 = Biggoron), MM's collider lengths,
//     MM's spin-attack wave (ovl_MM_En_M_Thunder, its Razor/Gilded/Great Fairy's type), the Razor Sword's 100 hits
//     (MM func_8083FFEC), the Great Fairy's Sword two-handed (MM PLAYER_IA_SWORD_TWO_HANDED);
//   - shields: MM's models; the Hero's Shield is held in front by young Link (MM's human) where OoT's child carries
//     the Hylian Shield on his back; the MM Mirror Shield reflects light as Ocarina of Time's (it is one to Hyrule);
//   - Hero's Bow: the Fairy Bow's mechanics (and its magic arrows) with MM's bow and string;
//   - MM Hookshot: MM's reach (26, the Longshot's) and MM's hookshot, tip and chain.
// MM's models are drawn on young Link natively (MM's human is Ocarina of Time's child skeleton), and on adult Link with
// a per-hand fit (MergeShipMMEquipmentDraw.inc). MM variants are usable by both ages.
//
// Options: CVAR_ENHANCEMENT("MergeShip.MMEquipment") (default 1) gates everything; ("MergeShip.TimelessMMGear")
// (default 0) also lifts Ocarina of Time's age rules from the swords and shields that have MM partners (Kokiri,
// Master, Biggoron, Hylian, Mirror), so either age can switch between any of them; ("MergeShip.UnbreakableRazorSword")
// (default 0, 2S2H gCheats.UnbreakableRazorSword; always on in a randomizer seed, as 2S2H).

#ifdef __cplusplus
extern "C" {
#endif
#include "z64.h"
#ifdef __cplusplus
}
#endif

// The seven MM items, in item-id order (ITEM_MM_SWORD_RAZOR + id). Bits of MergeShipSaveData.mmEquipOwned and
// .mmEquipSelected.
typedef enum MMEquipId {
    /* 0 */ MMEQUIP_RAZOR,
    /* 1 */ MMEQUIP_GILDED,
    /* 2 */ MMEQUIP_GREAT_FAIRY,
    /* 3 */ MMEQUIP_HERO_SHIELD,
    /* 4 */ MMEQUIP_MIRROR_SHIELD,
    /* 5 */ MMEQUIP_HOOKSHOT,
    /* 6 */ MMEQUIP_BOW,
    /* 7 */ MMEQUIP_KOKIRI, // MM's own Kokiri Sword (ITEM_MM_SWORD_KOKIRI, 0xE0: outside the run above; 2026-09-24)
    /* 8 */ MMEQUIP_MAX
} MMEquipId;

// The Ocarina of Time slots the MM items share. Bits of MergeShipSaveData.mmEquipSlotOnlyMM.
typedef enum MMEquipSlot {
    /* 0 */ MMEQUIP_SLOT_KOKIRI_SWORD, // Razor, Gilded
    /* 1 */ MMEQUIP_SLOT_BIGGORON,     // Great Fairy's Sword
    /* 2 */ MMEQUIP_SLOT_HYLIAN,       // Hero's Shield
    /* 3 */ MMEQUIP_SLOT_MIRROR,       // MM Mirror Shield
    /* 4 */ MMEQUIP_SLOT_HOOKSHOT,     // MM Hookshot (with the Hookshot or the Longshot)
    /* 5 */ MMEQUIP_SLOT_BOW,          // Hero's Bow
    /* 6 */ MMEQUIP_SLOT_MAX
} MMEquipSlot;

#define MERGESHIP_ITEM_IS_MM_EQUIPMENT(item)     ((((item) >= ITEM_MM_SWORD_RAZOR) && ((item) <= ITEM_MM_BOW_HERO)) || ((item) == ITEM_MM_SWORD_KOKIRI))
#define MERGESHIP_ITEM_IS_MM_SWORD(item)     ((((item) >= ITEM_MM_SWORD_RAZOR) && ((item) <= ITEM_MM_SWORD_GREAT_FAIRY)) || ((item) == ITEM_MM_SWORD_KOKIRI))
// An MM equipment id's item and back (the run ITEM_MM_SWORD_RAZOR + id, and MM's Kokiri Sword apart from it).
#define MERGESHIP_MMEQUIP_ITEM(id) (((id) == MMEQUIP_KOKIRI) ? ITEM_MM_SWORD_KOKIRI : (ITEM_MM_SWORD_RAZOR + (id)))
#define MERGESHIP_MMEQUIP_ID(item) (((item) == ITEM_MM_SWORD_KOKIRI) ? MMEQUIP_KOKIRI : ((item) - ITEM_MM_SWORD_RAZOR))
#define MERGESHIP_IA_IS_MM_SWORD(ia) (((ia) >= PLAYER_IA_MM_SWORD_RAZOR) && ((ia) <= PLAYER_IA_MM_SWORD_GREAT_FAIRY))

// A B-button item that is a sword: Ocarina of Time's four and MM's three (for code that restores a "temporary B").
#define MERGESHIP_ITEM_IS_B_SWORD(item)                                                                       \
    (((item) == ITEM_SWORD_KOKIRI) || ((item) == ITEM_SWORD_MASTER) || ((item) == ITEM_SWORD_BGS) ||         \
     ((item) == ITEM_SWORD_KNIFE) || MERGESHIP_ITEM_IS_MM_SWORD(item))

#ifdef __cplusplus
extern "C" {
#endif

// ---- state (MergeShipMMEquipment.c) ------------------------------------------------------------------------------
// The feature is on (CVar) and mm.o2r, which holds every MM model, icon and name, is mounted.
s32 MergeShip_MMEquip_IsEnabled(void);
s32 MergeShip_MMEquip_Owns(s32 id);
// The MM item is owned, the feature is on, and it is the variant its slot is switched to.
s32 MergeShip_MMEquip_IsSelected(s32 id);
// BUG-0077: the MMEQUIP_BIT set of what IsSelected says (for Anchor), and an Anchor teammate's set in its place while his
// dummy is drawn (the draw then treats the dummy as a player).
u32 MergeShip_MMEquip_SelectedBits(void);
void MergeShip_MMEquip_SetDrawOverride(s32 on, u32 bits);
s32 MergeShip_MMEquip_DrawOverrideActive(void);
// The Ocarina of Time item of a slot is really owned (not just holding the slot for an MM variant).
s32 MergeShip_MMEquip_OoTItemOwned(s32 slot);
// Ocarina of Time's Select Item page reads its slots through this: ITEM_NONE where the slot is held only for MM's
// variant (the Hero's Bow, MM's Hookshot), which is on the MM item menu (z_kaleido_mm_item.c) instead.
u8 MergeShip_MMEquip_OoTPageSlotItem(s32 invSlot);
// Keeps the save consistent: slots, selections and the B button (run every frame by the player and after changes).
void MergeShip_MMEquip_Sanitize(PlayState* play);

// Item_Give of one of the seven items (z_parameter.c); and of an Ocarina of Time partner, which then really is owned.
void MergeShip_MMEquip_Give(PlayState* play, u8 item);
void MergeShip_MMEquip_OnOoTItemGiven(u8 item);
// Ocarina of Time's Inventory_DeleteEquipment took the equipped sword or shield (a Like Like): if it was an MM
// variant, that one is gone until the enemy's dropped shield is picked up again.
void MergeShip_MMEquip_OnEquipmentDelete(s32 equipType, s32 equipValue);

// MM sItemActions for the swords (PLAYER_IA_NONE for the others, which are variants of Ocarina of Time items).
s8 MergeShip_MMEquip_ItemToItemAction(s32 item);
// Player_UseItem: the Bow / Hookshot slot's version follows the item used (MM's from the Termina items page).
void MergeShip_MMEquip_OnUseItem(s32 item);

// ---- age rules --------------------------------------------------------------------------------------------------
// Ocarina of Time's Equipment page (z_kaleido_scope.h CHECK_AGE_REQ_EQUIP), which shows only Ocarina of Time's swords
// and shields: TimelessMMGear. (MM's variants are usable at either age; the Termina view checks them itself.)
s32 MergeShip_MMEquip_EquipSlotIgnoresAge(s32 equipType, s32 equipValue);
// Items page and item icons (VB_ITEM_MEETS_AGE_REQ / VB_SLOT_MEETS_AGE_REQ, MergeShipMMEquipment.cpp).
s32 MergeShip_MMEquip_ItemIgnoresAge(s32 item);
s32 MergeShip_MMEquip_InvSlotIgnoresAge(s32 slot);

// ---- pause screen -----------------------------------------------------------------------------------------------
// The item a sword or shield slot is in use as (the MM variant's id while it is selected); `ootItem` otherwise, and for
// other slots. (MM's Quest Status view, z_kaleido_mergeship.c, shows the sword and shield in use with it.)
u16 MergeShip_MMEquip_PauseSlotItem(s32 equipType, s32 equipValue, u16 ootItem);
// Ocarina of Time's Equipment page (z_kaleido_equipment.c) shows only Ocarina of Time's swords and shields:
//   - SlotHidden: the slot's bit is set only to hold it for MM gear (its Ocarina of Time item is not owned), so the
//     page shows it empty (no icon, the cursor does not stop there, A does nothing);
//   - EquippedIsMM: the equipped sword / shield of `equipType` is the MM variant its slot is switched to, so the page
//     draws no "equipped" outline on its row (the Termina view does, on the MM item);
//   - SelectOoT: A on one of its swords or shields -- the slot goes back to its Ocarina of Time item (a selected MM
//     variant is deselected; the page then equips it as Ocarina of Time does). True when it was switched from an MM
//     variant (the press equips the Ocarina of Time item then; SoH's Equipment Can Be Removed does not remove it).
s32 MergeShip_MMEquip_PauseSlotHidden(s32 equipType, s32 equipValue);
s32 MergeShip_MMEquip_PauseEquippedIsMM(s32 equipType);
s32 MergeShip_MMEquip_PauseSelectOoT(s32 equipType, s32 equipValue);
// The Ocarina of Time item whose slot an MM item shares (`item` itself for any other).
u16 MergeShip_MMEquip_OoTPartnerItem(u16 item);
// Items page (SLOT_BOW, SLOT_HOOKSHOT): the slot can be switched, switch it, and the icon of the other variant.
s32 MergeShip_MMEquip_ItemSlotCanCycle(s32 slot);
void MergeShip_MMEquip_ItemSlotCycle(s32 slot);
void* MergeShip_MMEquip_ItemSlotOtherIcon(s32 slot);
s32 MergeShip_MMEquip_ItemSlotOtherIsGray(s32 slot);
// Icon to draw for `texture`: the Hero's Bow / MM Hookshot icon in place of the Fairy Bow / Hookshot / Longshot icon
// while those are selected.
void* MergeShip_MMEquip_SwapIcon(void* texture);
// Name panel: MM's name texture for an MM variant under the cursor (the Items page's Hero's Bow / MM Hookshot, the
// Equipment face's Termina view), `nameTexture` otherwise.
void* MergeShip_MMEquip_NameTexture(PlayState* play, void* nameTexture);
// Equipment page, "Termina" view (C-Up, z_kaleido_mergeship.c): MM's equipment by item -- ITEM_SWORD_KOKIRI (the
// Kokiri Sword, MM's first sword), ITEM_MM_SWORD_RAZOR, _GILDED, _GREAT_FAIRY, ITEM_MM_SHIELD_HERO, _MIRROR.
// Owned and usable now (the Kokiri Sword: really owned, not only holding its slot for an MM sword).
s32 MergeShip_MMEquip_TerminaOwns(u16 item);
// Its slot is the equipped sword / shield and is switched to it.
s32 MergeShip_MMEquip_TerminaEquipped(u16 item);
// It can be equipped now: owned; the Kokiri Sword by the age that can use it (or TimelessMMGear / TimelessEquipment);
// a sword not while transformed (the B button is the form's).
s32 MergeShip_MMEquip_TerminaCanEquip(u16 item);
// A on it: its slot switches to it (the slot's selected variant, which Ocarina of Time's page and the player read, so
// both views agree) and it is equipped (a sword on B). This view is the only way to choose an MM sword or shield.
// Returns false, changing nothing, when it can't be equipped now.
s32 MergeShip_MMEquip_TerminaEquip(PlayState* play, u16 item);

// ---- player (MergeShipMMEquipment.inc, z_player.c) --------------------------------------------------------------
// MM func_8083FFEC: a hit with the Razor Sword wears it; at 0 it is Kokiri again. True while holding the Razor Sword.
s32 MergeShip_MMEquip_RazorWear(PlayState* play, Player* player);
// MM func_808332A0: the spin attack's wave for an MM sword (ovl_MM_En_M_Thunder). True when handled.
s32 MergeShip_MMEquip_SpawnSpinWave(PlayState* play, Player* player, s32 magicCost);
// MM func_8082FA5C: a swing with an MM sword resets the trail to MM's colours (Player_OverrideBlureColors 0, 4).
void MergeShip_MMEquip_OnSwing(Player* player);

// ---- drawing (MergeShipMMEquipmentDraw.inc, z_player_lib.c) -----------------------------------------------------
// Player_DrawImpl's tunic environment colour, restored after an MM model that changes it.
void MergeShip_MMEquip_SetTunicEnvColor(u8 r, u8 g, u8 b);
// Player_OverrideLimbDrawGameplayDefault: an MM sword, shield, bow or hookshot in hand, and MM's sheath and back
// shield. True when it drew the limb (the MM-child hand/sheath override is then skipped).
s32 MergeShip_MMEquip_OverrideLimb(PlayState* play, Player* player, s32 limbIndex, Gfx** dList, Vec3f* pos);
// Player_OverrideLimbDrawGameplayFirstPerson: MM's first-person bow and hookshot.
void MergeShip_MMEquip_OverrideLimbFirstPerson(PlayState* play, Player* player, s32 limbIndex, Gfx** dList);
// Player_OverrideLimbDrawPause: the equipment screen's Link with the selected variants.
s32 MergeShip_MMEquip_OverrideLimbPause(PlayState* play, s32 limbIndex, s32 type, Gfx** dList, u8* swordAndShield);
// Pause: the child holds the Hero's Shield in front (not the Hylian Shield on his back).
s32 MergeShip_MMEquip_ChildHoldsHeroShield(void);
// Player_PostLimbDrawGameplay: the sword collider's length scale (the adult fit of an MM sword), the Hero's Bow string
// (true when drawn), and the MM Hookshot's hand/tip offsets and reach.
f32 MergeShip_MMEquip_MeleeLengthScale(Player* player);
// The Hero's Bow is what the player holds (young Link at the shooting gallery keeps Ocarina of Time's slingshot).
s32 MergeShip_MMEquip_HeroBowInUse(PlayState* play, Player* player);
s32 MergeShip_MMEquip_DrawBowString(PlayState* play, Player* player, Vec3f* prevBodyPartPos);
// MM D_801C0D6C: where MM's human holds the arrow (Ocarina of Time's sLeftHandArrowVec3 otherwise).
Vec3f* MergeShip_MMEquip_ArrowOffset(PlayState* play, Player* player, Vec3f* ootOffset);
Vec3f* MergeShip_MMEquip_HookshotOffset(Player* player, Vec3f* ootOffset, s32 tip);
f32 MergeShip_MMEquip_HookshotReticleRange(Player* player, f32 ootRange);
// ArmsHook: MM's reach and models.
s32 MergeShip_MMEquip_HookshotIsMM(Player* player);
s32 MergeShip_MMEquip_HookshotTimer(Player* player, s32 ootTimer);
const char* MergeShip_MMEquip_HookshotTipDL(Player* player);
const char* MergeShip_MMEquip_HookshotChainDL(Player* player);
f32 MergeShip_MMEquip_HookshotTipScale(Player* player);
// A copy of the list at `path` ending where the texture named `texture` starts being loaded (include/mergeship/DlCut.hpp);
// NULL when it has none.
Gfx* MergeShip_MMEquip_CutDLBeforeTexture(const char* path, const char* texture);

// ---- collision (z_collision_check.c) ----------------------------------------------------------------------------
// MM damage for the Gilded Sword (3/6 against the Master Sword's 2/4): Ocarina of Time's damage tables hold the
// Master Sword's damage, so a Gilded hit, blade or spin wave, does 1.5 times that.
f32 MergeShip_MMEquip_AdjustDamage(Actor* attacker, ColliderInfo* attackerInfo, DamageTable* tbl, ColliderInfo* info,
                                   f32 damage);
// MM's own Kokiri Sword (ITEM_MM_SWORD_KOKIRI on B; its item action is Ocarina of Time's Kokiri Sword's).
s32 MergeShip_MMEquip_HoldsMMKokiriSword(Player* player);

#ifdef __cplusplus
}
#endif
