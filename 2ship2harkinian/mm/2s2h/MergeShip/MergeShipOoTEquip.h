#pragma once

// MergeShip2 -- Ocarina of Time's equipment in Majora's Mask (2Ship): the Master Sword, Biggoron's Sword and the Giant's
// Knife, the Deku, Hylian and Mirror Shields, the Goron and Zora Tunics, the Iron and Hover Boots, the Goron's Bracelet
// and the Silver and Golden Gauntlets, the Silver and Golden Scales and the Bullet Bag.
//
// Item ids: z64item.h ITEM_OOT_SWORD_MASTER (0xCD) .. ITEM_OOT_BULLET_BAG (0xDF). Save: z64save.h MergeShipOoTEquipSave
// (ShipSaveInfo::mergeShipOoTEquip, "mergeShipOoTEquip"). Majora's Mask's own sword and shield stay where MM keeps them
// (ItemEquips::equipment), so a save still loads as plain MM with the option off.
//
// Only the human (MM's only human, Ocarina of Time's child proportions) uses them:
//   - swords on B: the Master Sword is the human's PLAYER_IA_SWORD_GILDED (one-handed, 4000 reach: OoT's own)
//     with Ocarina of Time's damage (2 / 4 spin and jump, MM D_8085D09C terms); Biggoron's Sword and the Giant's Knife
//     are PLAYER_IA_SWORD_TWO_HANDED (two-handed, 5500 reach, 4 / 8); the Giant's Knife breaks after 8 hits
//     (OoT func_80842B7C) and then does the Kokiri Sword's damage and cannot spin (OoT Player_HoldsBrokenKnife).
//   - shields: the Deku Shield burns (OoT func_8083819C), the Hylian Shield is the Hero's Shield's behaviour, Ocarina of
//     Time's Mirror Shield reflects as MM's (PLAYER_SHIELD_MIRROR_SHIELD).
//   - tunics: MM's own hazard update (z_parameter.c, a remnant of Ocarina of Time) reads EQUIP_TYPE_TUNIC: the Goron
//     Tunic stops the hot-room timer, the Zora Tunic the drowning timer.
//   - boots: PLAYER_BOOTS_IRON (MM's heavy-boots rules: sink, walk underwater) and PLAYER_BOOTS_HOVER (Ocarina of Time's
//     hover timer, traction and effect), with Ocarina of Time's boot data rows (z_player_lib.c D_801BFE14).
//   - strength: Player_GetStrength (MM PLAYER_STRENGTH_*): Silver Gauntlets = the Zora's strength (heavy push blocks),
//     Golden = the Goron's; either lifts the silver boulders MM keeps for the Goron, the Golden Gauntlets also the Rock
//     Sirloin. Powder Kegs stay the Goron's (MM's keg licence). MM's human already lifts Bomb Flowers.
//   - scales: the human's dive depth (MM 120, Ocarina of Time D_80854784: 120 / 240 / 360).
// Models: Ocarina of Time's own display lists (oot/ archive) on MM's human. MM's human is Ocarina of Time's child
// skeleton, so the child's lists (Deku Shield, Goron's Bracelet, Hylian Shield on the back) sit as modelled; the adult's
// (swords, Hylian and Mirror Shields in hand, boots, gauntlets) are fitted like SoH's ScaleAdultEquipmentAsChild (0.8 in
// the hand, the sheath offset 218,-100,62), with the adult's gloved hand replaced by MM's bare fist.
//
// Pause: "Equipment", Ocarina of Time's equipment page, is a view of the Quest face: C-Up cycles Termina's quest status
// -> Hyrule's quest status -> Hyrule's equipment -> Termina's (z_kaleido_mergeship.c). It shows only Ocarina of Time's
// items; A equips one (a sword on B, a shield, a tunic, boots). MM has no screen that equips its own sword or shield
// (its Quest page shows the ones in use), so A on the equipped Ocarina of Time sword or shield puts it away and MM's
// own is in use again -- the A button reads "Put Away" there. B goes back to Termina's quest status.
// (MS_OOTEQ_SWORD_GREAT_FAIRY, MM's Great Fairy's Sword on B, is no longer offered by the page -- it is MM's C item, as in
// MM; a save that has it keeps it until another sword is equipped.)
//
// Everything is gated by MERGESHIP_OOT_EQUIP_CVAR (default on) and needs oot-assets.o2r (MergeShipAssets.h).

#include "ultra64.h"
#include "z64item.h"

#define MERGESHIP_OOT_EQUIP_CVAR "gEnhancements.MergeShip.OoTEquipment"

// ITEM_IS_OOT_EQUIP / ITEM_IS_OOT_EQUIP_SWORD are in z64item.h.
// The Kokiri Sword (ITEM_OOT_SWORD_KOKIRI, 0xFA, outside the 0xCD.. run) takes bit 19, the first after the Bullet Bag's.
#define MS_OOTEQ_OWN_BIT(item)     (((item) == ITEM_OOT_SWORD_KOKIRI) ? (1u << 19) : (1u << ((item) - ITEM_OOT_SWORD_MASTER)))

// MergeShipOoTEquipSave::sword: the human's B sword.
typedef enum MsOoTEqSword {
    /* 0 */ MS_OOTEQ_SWORD_MM,          // MM's own (Kokiri / Razor / Gilded, ItemEquips::equipment)
    /* 1 */ MS_OOTEQ_SWORD_MASTER,      // ITEM_OOT_SWORD_MASTER
    /* 2 */ MS_OOTEQ_SWORD_GREAT_FAIRY, // MM's ITEM_SWORD_GREAT_FAIRY on B
    /* 3 */ MS_OOTEQ_SWORD_LONG,        // Biggoron's Sword / Giant's Knife / Broken Giant's Knife (bgsFlag, swordHealth)
    /* 4 */ MS_OOTEQ_SWORD_KOKIRI,      // ITEM_OOT_SWORD_KOKIRI: Ocarina of Time's Kokiri Sword (its model; MM's damage and
                                        // reach are the same sword's), never MM's own (2026-09-24)
    /* 5 */ MS_OOTEQ_SWORD_MAX
} MsOoTEqSword;

// MergeShipOoTEquipSave::shield.
typedef enum MsOoTEqShield {
    /* 0 */ MS_OOTEQ_SHIELD_MM, // MM's own (Hero's / Mirror)
    /* 1 */ MS_OOTEQ_SHIELD_DEKU,
    /* 2 */ MS_OOTEQ_SHIELD_HYLIAN,
    /* 3 */ MS_OOTEQ_SHIELD_MIRROR,
    /* 4 */ MS_OOTEQ_SHIELD_MAX
} MsOoTEqShield;

// MergeShipOoTEquipSave::variants: which item of each pair the equipment page used to show (the page's old A toggle
// between MM's and Ocarina of Time's items). Still kept up to date by the Equip* calls; nothing reads it now (kept for
// the save's layout).
#define MS_OOTEQ_VARIANT_MASTER (1 << 0)     // sword cell 1: the Master Sword (else MM's sword)
#define MS_OOTEQ_VARIANT_BIGGORON (1 << 1)   // sword cell 2: Biggoron's Sword / Giant's Knife (else Great Fairy's)
#define MS_OOTEQ_VARIANT_HYLIAN (1 << 2)     // shield cell 1: the Hylian Shield (else the Hero's Shield)
#define MS_OOTEQ_VARIANT_OOT_MIRROR (1 << 3) // shield cell 2: Ocarina of Time's Mirror Shield (else MM's)
// Bit 7 is taken: MERGESHIP_SHARED_BARE_START (MergeShipSharedSave.h), a Termina file the door made without MM's gear.

#define MS_OOTEQ_KNIFE_HEALTH 8 // OoT: gSaveContext.swordHealth = 8

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Player;
struct Actor;
struct PauseContext;
struct MergeShipOoTEquipSave;

// ---- state (MergeShipOoTEquip.c) ----------------------------------------------------------------------------------------
s32 MergeShip_OoTEquip_Enabled(void); // the option is on and the Ocarina of Time archive is mounted
struct MergeShipOoTEquipSave* MergeShip_OoTEquip_Save(void);
s32 MergeShip_OoTEquip_Owns(u8 item);
// The item a sword/shield selection stands for (ITEM_NONE for MM's own).
u8 MergeShip_OoTEquip_SwordItem(s32 sword);
u8 MergeShip_OoTEquip_ShieldItem(s32 shield);
// Item_GiveImpl / Item_CheckObtainabilityImpl for ITEM_OOT_SWORD_MASTER .. ITEM_OOT_BULLET_BAG.
u8 MergeShip_OoTEquip_Give(struct PlayState* play, u8 item);
u8 MergeShip_OoTEquip_CheckObtainability(u8 item);
// MM gave one of its own swords or shields: it is equipped, as in MM.
void MergeShip_OoTEquip_OnMMItemGiven(u8 item);
// Keeps the B button, the tunic, the boots and the shield in step with the save (every frame and after changes).
void MergeShip_OoTEquip_Update(struct PlayState* play);
// Player_SetEquipmentData: the human's shield and boots.
void MergeShip_OoTEquip_ApplyToPlayer(struct Player* player);
// Equip a selection (pause page, harness). Returns true when it changed anything.
s32 MergeShip_OoTEquip_EquipSword(struct PlayState* play, s32 sword);
s32 MergeShip_OoTEquip_EquipShield(struct PlayState* play, s32 shield);
s32 MergeShip_OoTEquip_EquipTunic(s32 tunic);
s32 MergeShip_OoTEquip_EquipBoots(struct PlayState* play, s32 boots);
// A tunic or boots on a D-pad button (bound on the Equipment (Hyrule) view, MergeShipOoTEquipPause.c) puts them on --
// or, when they are on already, the Kokiri Tunic / Boots again -- as Ship of Harkinian's "Assignable Tunics and Boots"
// does. The press is taken by the D-pad hook (MergeShipDpadBoots.cpp), which runs in every state (MM reads no item
// button while swimming, where the Iron Boots and the Zora Tunic are worn); Player_UseItem only swallows it (true for a
// tunic or boots), so a press toggles once.
s32 MergeShip_OoTEquip_IsWearableItem(u8 item);
s32 MergeShip_OoTEquip_UseWearable(struct PlayState* play, struct Player* player, u8 item);
s32 MergeShip_OoTEquip_ToggleWearable(struct PlayState* play, struct Player* player, u8 item);
// The button slot (DPAD_SLOT_EQUIP) of a tunic or boots on the D-pad: past MM's inventory slots and Hyrule's item cells
// (MS_OOT_BUTTON_SLOT), so no inventory lookup ever takes it for an item slot.
#define MS_OOT_EQUIP_BUTTON_SLOT 0x50

// ---- player (z_player.c, z_player_lib.c) --------------------------------------------------------------------------------
s32 MergeShip_OoTEquip_ItemAction(struct Player* player, u8 item); // Player_ItemToItemAction for these ids
// func_8083375C: Ocarina of Time's damage for the sword in hand (MM D_8085D09C terms). `strong`: spin / jump.
s32 MergeShip_OoTEquip_MeleeDamage(struct Player* player, s32 strong, s32 damage);
// func_8083FFEC: a hit with the Giant's Knife wears it (OoT func_80842B7C). True while holding the long sword.
s32 MergeShip_OoTEquip_SwordHit(struct PlayState* play, struct Player* player);
s32 MergeShip_OoTEquip_HoldsBrokenKnife(struct Player* player);
// BUG-0087: the shield Link holds is Ocarina of Time's Mirror Shield (Mir_Ray3 then draws its beam).
s32 MergeShip_OoTEquip_HoldsOoTMirrorShield(struct Player* player);
// func_808332A0: the spin attack wave's type (MM En_M_Thunder: 0 Kokiri, 1 Razor, 2 Gilded, 3 Great Fairy's; its damage
// is sDamages[type]). The Master Sword, which is MM's Gilded Sword's item action, waves as the Razor Sword (damage 2).
s32 MergeShip_OoTEquip_SpinWaveType(struct Player* player, s32 type);
// The swing sound (OoT func_80833A20: Biggoron's Sword swings like the Hammer unless broken).
s32 MergeShip_OoTEquip_IsBSwordItem(u8 item);
// BUG-0154/0155: the human has a sword to fight with: MM's own, or the sword on B (the Great Fairy's Sword, Hyrule's
// Biggoron's Sword / Giant's Knife / Master / Kokiri Sword), where MM's own sword value is "none". MM's checks for a sword
// (the Swordsman's School, the Clock Town guards) ask this.
s32 MergeShip_OoTEquip_HumanHasSword(void);
// Player_GetStrength: `strength` is MM's for the current form.
u8 MergeShip_OoTEquip_Strength(u8 strength);
// A human with gauntlets lifts what MM keeps for the Goron (see the top of the file).
s32 MergeShip_OoTEquip_CanLiftHeavy(struct Player* player, struct Actor* actor);
// Player_Action_59: how deep the human dives (MM 120).
f32 MergeShip_OoTEquip_DiveDepth(struct Player* player, f32 depth);
// OoT func_8083819C: fire burns the Deku Shield.
void MergeShip_OoTEquip_BurnDekuShield(struct PlayState* play, struct Player* player);
// Hover Boots (Ocarina of Time's hoverBootsTimer logic; MM's Player has no field for it).
s32 MergeShip_OoTEquip_HoverTimer(void);
void MergeShip_OoTEquip_HoverResetTimer(void);
s32 MergeShip_OoTEquip_HoverUpdateGround(struct PlayState* play, struct Player* player, s32 floorIsWaterOrSlippery);
s32 MergeShip_OoTEquip_HoverKeepsAirborne(struct Player* player);
s32 MergeShip_OoTEquip_HoverSlides(struct Player* player);
void MergeShip_OoTEquip_HoverSfx(struct Player* player);

// ---- drawing (MergeShipOoTEquipDraw.c) ----------------------------------------------------------------------------------
// Player_OverrideLimbDrawGameplayDefault: the sword and shield in hand and the sheath.
void MergeShip_OoTEquip_OverrideLimb(struct PlayState* play, struct Player* player, s32 limbIndex, Gfx** dList);
// BUG-0015: young Link holds Hyrule's Fairy Bow (the button he used holds the Items (Hyrule) bow cell's slot): Ocarina
// of Time's bow in MM's fist (the adult's, under the hand fit, as Hyrule's MM young Link holds it).
s32 MergeShip_OoTEquip_HoldsFairyBow(struct Player* player);
// A sword is in Link's left hand (BUG-0062: Biggoron's Sword is not also on his back then).
s32 MergeShip_OoTEquip_SwordInHand(struct Player* player);
// BUG-0059: the adult holds MM's Hero's Bow (drawn in his fist, MergeShipAdultLink.c), not Hyrule's Fairy Bow.
s32 MergeShip_OoTEquip_AdultHoldsHeroBow(struct Player* player);
// ... and in first person (MM's first-person right hand holds its Hero's Bow): MM's fist with Ocarina of Time's.
Gfx* MergeShip_OoTEquip_FairyBowFirstPersonDL(struct PlayState* play);
// BUG-0022: young Link with Hyrule's Hylian Shield guards as Ocarina of Time's young Link does -- hunched forward under
// the shield on his back (gPlayerAnim_clink_normal_defense_ALL), not crouched behind it in hand. Option
// MERGESHIP_HYLIAN_HUNCH_CVAR, default on. True while a guard frame of it was drawn this frame or the last.
#define MERGESHIP_HYLIAN_HUNCH_CVAR "gEnhancements.MergeShip.HylianShieldHunch"
// BUG-0021: Biggoron's Sword / the Giant's Knife put away is worn on the back, alone (no scabbard), in the Master
// Sword's place:
// Ocarina of Time's hand list moved as its Master Sword moves from the hand into the scabbard (fitted from oot.o2r's
// gLinkAdultLeftHandHoldingMasterSwordNearDL against gLinkAdultMasterSwordAndSheathNearDL: all 24 hilt vertices
// within 0.6 units, a pure translation). Ocarina of Time shows none; option MERGESHIP_BGS_ON_BACK_CVAR, default on.
#define MERGESHIP_BGS_ON_BACK_CVAR "gEnhancements.MergeShip.BgsOnBack"
#define MERGESHIP_BGS_ON_BACK_X (-714.409f)
#define MERGESHIP_BGS_ON_BACK_Y (-310.307f)
#define MERGESHIP_BGS_ON_BACK_Z (77.999f)
s32 MergeShip_OoTEquip_HylianHunch(struct Player* player);
// The guard animation's frame for young Link under the Hylian Shield (MergeShipAdultLink.cpp's frame hook).
void MergeShip_OoTEquip_NoteHylianHunch(void);
// Player_PostLimbDrawGameplay: the shield on the back (MM's `dList` otherwise).
Gfx* MergeShip_OoTEquip_BackShieldDL(struct PlayState* play, struct Player* player, Gfx* dList);
// GameInteractor OnPlayerPostLimbDraw: boots, gauntlets and bracelet on their limbs.
void MergeShip_OoTEquip_PostLimbDraw(struct PlayState* play, struct Player* player, s32 limbIndex);
// The Hover Boots' effect under the feet (OoT Player_Draw).
void MergeShip_OoTEquip_DrawHoverEffect(struct PlayState* play, struct Player* player);
// Ocarina of Time's get-item model (OoT z_draw.c) for `item`.
void MergeShip_OoTEquip_DrawGetItem(struct PlayState* play, u8 item);
const char* MergeShip_OoTEquip_IconTexture(u8 item); // 32x32 RGBA32 (OoT icon_item_static)
const char* MergeShip_OoTEquip_NameTexture(u8 item); // 128x16 IA4 (OoT item_name_static, English)
const char* MergeShip_OoTEquip_Name(u8 item);

// ---- display list surgery (MergeShipOoTEquip.cpp) -----------------------------------------------------------------------
#define MS_OOTEQ_DL_STRIP_HAND (1 << 0)   // drop the triangles textured as the adult's gloved hand
#define MS_OOTEQ_DL_STRIP_SHEATH (1 << 1) // drop the triangles textured as a scabbard
#define MS_OOTEQ_DL_REMAP_0D (1 << 2)     // matrices loaded from segment 0x0D are loaded from segment 0x0E instead
Gfx* MergeShip_OoTEquip_RewriteDL(const char* path, u32 flags);
// A copy of the list at `path` ending where the texture named `texture` starts being loaded (include/mergeship/DlCut.hpp);
// NULL when it has none.
Gfx* MergeShip_OoTEquip_CutDLBeforeTexture(const char* path, const char* texture);

// ---- messages and gives (MergeShipOoTEquip.cpp) -------------------------------------------------------------------------
// Link holds `item` up (Ocarina of Time's model) and Ocarina of Time's English get-item text shows.
void MergeShip_OoTEquip_GiveWithCutscene(u8 item);
void MergeShip_OoTEquip_ShowText(const char* mmCodedText); // OoT func_8083819C's "Your shield is gone!"

// ---- pause (MergeShipOoTEquipPause.c) ------------------------------------------------------------------------------------
// z_kaleido_collect.c: true while the Equipment view replaces the Quest page (its drawing / its cursor).
s32 MergeShip_OoTEquipPause_DrawQuestPage(struct PlayState* play);
s32 MergeShip_OoTEquipPause_UpdateQuestCursor(struct PlayState* play);
// Kaleido_LoadItemNameStatic: an OoT item's name texture (NULL for MM's items).
const char* MergeShip_OoTEquipPause_NameTexture(u32 item);
// The Quest page's sword and shield icons: the equipped item's (MM's own otherwise).
const char* MergeShip_OoTEquipPause_QuestIcon(s32 isShield, const char* mmIcon);
s32 MergeShip_OoTEquipPause_IsOpen(void);
void MergeShip_OoTEquipPause_DebugCursor(struct PlayState* play, s32 cell); // harness "ooteqcursor"
void MergeShip_OoTEquipPause_Close(void);
// The Quest face switches to the page (C-Up, z_kaleido_mergeship.c): the cursor on the equipped sword.
void MergeShip_OoTEquipPause_Open(struct PlayState* play);

#ifdef __cplusplus
}
#endif
