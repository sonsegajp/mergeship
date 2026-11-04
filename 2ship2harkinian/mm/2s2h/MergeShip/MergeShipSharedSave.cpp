// MergeShip2 -- 2 Ship 2 Harkinian's projector for the host's shared save (MergeShipSharedSave.h, SharedSave.h).

// Only in 2ship.dll (MERGESHIP_HOSTED): 2ship.exe has no shared save.
#ifdef MERGESHIP_HOSTED

#include <vector>
#include "2s2h/MergeShip/MergeShipSharedSave.h"
#include "2s2h/MergeShip/MergeShipHost.h"
#include "2s2h/MergeShip/MergeShipHyrule.h"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipOoTItems.h"
#include "2s2h/MergeShip/MergeShipTerminaEquip.h"
#include "2s2h/Rando/MergeShip/UnifiedDelivery.h"
#include "2s2h/Rando/Types.h"
#include "2s2h/Rando/Rando.h" // IS_RANDO

#include <mergeship/EngineApi.h>
#include <mergeship/MsDelivery.h> // MsDelivery_BottleOnePerWorld
#include <mergeship/SharedSave.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "2s2h/MergeShip/MergeShipOwnership.h"
extern PlayState* gPlayState;
}

namespace {

constexpr u32 Bit(s32 n) {
    return 1u << n;
}

// ---- bottles --------------------------------------------------------------------------------------------------

u8 BottleFromItem(u8 item) {
    switch (item) {
        case ITEM_BOTTLE: return MS_BOTTLE_EMPTY;
        case ITEM_POTION_RED: return MS_BOTTLE_RED_POTION;
        case ITEM_POTION_GREEN: return MS_BOTTLE_GREEN_POTION;
        case ITEM_POTION_BLUE: return MS_BOTTLE_BLUE_POTION;
        case ITEM_FAIRY: return MS_BOTTLE_FAIRY;
        case ITEM_DEKU_PRINCESS: return MS_BOTTLE_DEKU_PRINCESS;
        case ITEM_MILK_BOTTLE: return MS_BOTTLE_MILK;
        case ITEM_MILK_HALF: return MS_BOTTLE_MILK_HALF;
        case ITEM_FISH: return MS_BOTTLE_FISH;
        case ITEM_BUG: return MS_BOTTLE_BUG;
        case ITEM_BLUE_FIRE: return MS_BOTTLE_BLUE_FIRE;
        case ITEM_POE: return MS_BOTTLE_POE;
        case ITEM_BIG_POE: return MS_BOTTLE_BIG_POE;
        case ITEM_SPRING_WATER: return MS_BOTTLE_SPRING_WATER;
        case ITEM_HOT_SPRING_WATER: return MS_BOTTLE_HOT_SPRING_WATER;
        case ITEM_ZORA_EGG: return MS_BOTTLE_ZORA_EGG;
        case ITEM_GOLD_DUST: return MS_BOTTLE_GOLD_DUST;
        case ITEM_MUSHROOM: return MS_BOTTLE_MUSHROOM;
        case ITEM_SEAHORSE: return MS_BOTTLE_SEAHORSE;
        case ITEM_CHATEAU: return MS_BOTTLE_CHATEAU_ROMANI;
        case ITEM_HYLIAN_LOACH: return MS_BOTTLE_HYLIAN_LOACH;
        case ITEM_OBABA_DRINK: return MS_BOTTLE_MYSTERY_MILK;
        case ITEM_OOT_LETTER_RUTO: return MS_BOTTLE_RUTOS_LETTER; // MergeShipOoTItems.h keeps it in a bottle
        default: return MS_BOTTLE_NONE;
    }
}

// ITEM_NONE: nothing a Termina bottle can hold: the slot is left alone.
u8 ItemFromBottle(u8 bottle) {
    switch (bottle) {
        case MS_BOTTLE_EMPTY: return ITEM_BOTTLE;
        case MS_BOTTLE_RED_POTION: return ITEM_POTION_RED;
        case MS_BOTTLE_GREEN_POTION: return ITEM_POTION_GREEN;
        case MS_BOTTLE_BLUE_POTION: return ITEM_POTION_BLUE;
        case MS_BOTTLE_FAIRY: return ITEM_FAIRY;
        case MS_BOTTLE_DEKU_PRINCESS: return ITEM_DEKU_PRINCESS;
        case MS_BOTTLE_MILK: return ITEM_MILK_BOTTLE;
        case MS_BOTTLE_MILK_HALF: return ITEM_MILK_HALF;
        case MS_BOTTLE_FISH: return ITEM_FISH;
        case MS_BOTTLE_BUG: return ITEM_BUG;
        case MS_BOTTLE_BLUE_FIRE: return ITEM_BLUE_FIRE;
        case MS_BOTTLE_POE: return ITEM_POE;
        case MS_BOTTLE_BIG_POE: return ITEM_BIG_POE;
        case MS_BOTTLE_SPRING_WATER: return ITEM_SPRING_WATER;
        case MS_BOTTLE_HOT_SPRING_WATER: return ITEM_HOT_SPRING_WATER;
        case MS_BOTTLE_ZORA_EGG: return ITEM_ZORA_EGG;
        case MS_BOTTLE_GOLD_DUST: return ITEM_GOLD_DUST;
        case MS_BOTTLE_MUSHROOM: return ITEM_MUSHROOM;
        case MS_BOTTLE_SEAHORSE: return ITEM_SEAHORSE;
        case MS_BOTTLE_CHATEAU_ROMANI: return ITEM_CHATEAU;
        case MS_BOTTLE_HYLIAN_LOACH: return ITEM_HYLIAN_LOACH;
        case MS_BOTTLE_MYSTERY_MILK: return ITEM_OBABA_DRINK;
        case MS_BOTTLE_RUTOS_LETTER: return ITEM_OOT_LETTER_RUTO; // MergeShipOoTItems.h keeps it in a bottle
        default: return ITEM_NONE;
    }
}

// ---- masks, items, songs --------------------------------------------------------------------------------------

// Transformation mask items by MM form (0 Fierce Deity, 1 Goron, 2 Zora, 3 Deku).
const u8 kTransformationMasks[4] = { ITEM_MASK_FIERCE_DEITY, ITEM_MASK_GORON, ITEM_MASK_ZORA, ITEM_MASK_DEKU };
// MM PlayerMask 1 (Truth) .. 20 (Giant's) is item ITEM_MASK_TRUTH .. ITEM_MASK_GIANT.
u8 RegularMaskItem(s32 mask) {
    return (u8)(ITEM_MASK_TRUTH + (mask - 1));
}

struct ItemMap {
    u8 bit;  // MsItem
    u8 item; // MM ItemId (its own slot)
};
// The bow, the magic arrows and the Lens are MM's own versions (itemsModel 2, SharedSave.h MsShared_VersionBit):
// Hyrule's Fairy Bow, arrows and Lens are not MM's unless the unified seed combines them (MsSharedSave.itemsCombined).
const ItemMap kItems[] = {
    { MS_ITEM_OCARINA_OF_TIME, ITEM_OCARINA_OF_TIME },   { MS_ITEM_MM_BOW, ITEM_BOW },
    { MS_ITEM_MM_ARROW_FIRE, ITEM_ARROW_FIRE },          { MS_ITEM_MM_ARROW_ICE, ITEM_ARROW_ICE },
    { MS_ITEM_MM_ARROW_LIGHT, ITEM_ARROW_LIGHT },        { MS_ITEM_HOOKSHOT, ITEM_HOOKSHOT },
    { MS_ITEM_BOMB_BAG, ITEM_BOMB },                     { MS_ITEM_BOMBCHU, ITEM_BOMBCHU },
    { MS_ITEM_DEKU_STICK, ITEM_DEKU_STICK },             { MS_ITEM_DEKU_NUT, ITEM_DEKU_NUT },
    { MS_ITEM_MAGIC_BEANS, ITEM_MAGIC_BEANS },           { MS_ITEM_MM_LENS_OF_TRUTH, ITEM_LENS_OF_TRUTH },
    { MS_ITEM_POWDER_KEG, ITEM_POWDER_KEG },             { MS_ITEM_PICTOGRAPH_BOX, ITEM_PICTOGRAPH_BOX },
    { MS_ITEM_GREAT_FAIRYS_SWORD, ITEM_SWORD_GREAT_FAIRY },
};

// Ocarina of Time's items Termina now has natively (MergeShipOoTItems.h): given through its own inventory.
const ItemMap kOoTItems[] = {
    { MS_ITEM_SLINGSHOT, ITEM_SLINGSHOT },          { MS_ITEM_BOOMERANG, ITEM_OOT_BOOMERANG },
    { MS_ITEM_MEGATON_HAMMER, ITEM_OOT_HAMMER },     { MS_ITEM_LONGSHOT, ITEM_LONGSHOT },
    { MS_ITEM_DINS_FIRE, ITEM_OOT_DINS_FIRE },      { MS_ITEM_FARORES_WIND, ITEM_OOT_FARORES_WIND },
    { MS_ITEM_NAYRUS_LOVE, ITEM_OOT_NAYRUS_LOVE },
};

struct SongMap {
    u8 song;  // MsSong
    u8 quest; // MM QuestItem
};
const SongMap kSongs[] = {
    { MS_SONG_TIME, QUEST_SONG_TIME },
    { MS_SONG_EPONA, QUEST_SONG_EPONA },
    { MS_SONG_STORMS, QUEST_SONG_STORMS },
    { MS_SONG_SONATA, QUEST_SONG_SONATA },
    { MS_SONG_GORON_LULLABY, QUEST_SONG_LULLABY },
    { MS_SONG_GORON_LULLABY_INTRO, QUEST_SONG_LULLABY_INTRO },
    { MS_SONG_NEW_WAVE, QUEST_SONG_BOSSA_NOVA },
    { MS_SONG_ELEGY, QUEST_SONG_ELEGY },
    { MS_SONG_OATH, QUEST_SONG_OATH },
    { MS_SONG_HEALING, QUEST_SONG_HEALING },
    { MS_SONG_SOARING, QUEST_SONG_SOARING },
    // MergeShip2 M3: the Sun's Song and Saria's Song are both games' (UNIFIED_RANDO_DESIGN "Shared progressives")
    { MS_SONG_SUN, QUEST_SONG_SUN },
    { MS_SONG_SARIA, QUEST_SONG_SARIA },
};

// Ocarina of Time's songs Majora's Mask has no quest bit for: Termina's Hyrule quest status (MergeShipHyrule.h).
const SongMap kHyruleSongs[] = {
    { MS_SONG_MINUET, MERGESHIP_OOT_QUEST_SONG_MINUET },     { MS_SONG_BOLERO, MERGESHIP_OOT_QUEST_SONG_BOLERO },
    { MS_SONG_SERENADE, MERGESHIP_OOT_QUEST_SONG_SERENADE }, { MS_SONG_REQUIEM, MERGESHIP_OOT_QUEST_SONG_REQUIEM },
    { MS_SONG_NOCTURNE, MERGESHIP_OOT_QUEST_SONG_NOCTURNE }, { MS_SONG_PRELUDE, MERGESHIP_OOT_QUEST_SONG_PRELUDE },
    { MS_SONG_ZELDAS_LULLABY, MERGESHIP_OOT_QUEST_SONG_LULLABY },
};

// MsHyruleItem -> Termina's item for it (MergeShipOoTItems.h), ITEM_NONE where Termina has none (the Broken Goron's
// Sword). Ocarina of Time's Keaton, Bunny, Goron, Zora and Truth masks are not Majora's Mask's: Termina holds Hyrule's
// own (BUG-0066), in the Hyrule child trade cell with the other masks.
const u8 kHyruleItems[MS_HYRULE_ITEM_COUNT] = {
    /* MS_HYRULE_WEIRD_EGG */ ITEM_OOT_WEIRD_EGG,
    /* MS_HYRULE_CHICKEN */ ITEM_OOT_CHICKEN,
    /* MS_HYRULE_ZELDAS_LETTER */ ITEM_OOT_LETTER_ZELDA,
    /* MS_HYRULE_MASK_KEATON */ ITEM_OOT_MASK_KEATON,
    /* MS_HYRULE_MASK_SKULL */ ITEM_OOT_MASK_SKULL,
    /* MS_HYRULE_MASK_SPOOKY */ ITEM_OOT_MASK_SPOOKY,
    /* MS_HYRULE_MASK_BUNNY */ ITEM_OOT_MASK_BUNNY,
    /* MS_HYRULE_MASK_GORON */ ITEM_OOT_MASK_GORON,
    /* MS_HYRULE_MASK_ZORA */ ITEM_OOT_MASK_ZORA,
    /* MS_HYRULE_MASK_GERUDO */ ITEM_OOT_MASK_GERUDO,
    /* MS_HYRULE_MASK_TRUTH */ ITEM_OOT_MASK_TRUTH,
    /* MS_HYRULE_POCKET_EGG */ ITEM_OOT_POCKET_EGG,
    /* MS_HYRULE_POCKET_CUCCO */ ITEM_OOT_POCKET_CUCCO,
    /* MS_HYRULE_COJIRO */ ITEM_OOT_COJIRO,
    /* MS_HYRULE_ODD_MUSHROOM */ ITEM_OOT_ODD_MUSHROOM,
    /* MS_HYRULE_ODD_POTION */ ITEM_OOT_ODD_POTION,
    /* MS_HYRULE_POACHERS_SAW */ ITEM_OOT_SAW,
    /* MS_HYRULE_BROKEN_SWORD */ ITEM_NONE,
    /* MS_HYRULE_PRESCRIPTION */ ITEM_OOT_PRESCRIPTION,
    /* MS_HYRULE_EYEBALL_FROG */ ITEM_OOT_FROG,
    /* MS_HYRULE_EYEDROPS */ ITEM_OOT_EYEDROPS,
    /* MS_HYRULE_CLAIM_CHECK */ ITEM_OOT_CLAIM_CHECK,
};

// The MsHyruleItem of Termina's item, or -1.
s32 HyruleItemOf(u8 item) {
    for (s32 i = 0; i < MS_HYRULE_ITEM_COUNT; i++) {
        if ((kHyruleItems[i] != ITEM_NONE) && (kHyruleItems[i] == item)) {
            return i;
        }
    }
    return -1;
}

// Ocarina of Time's inventory.equipment and inventory.upgrades (the shared save's ootOwnedEquipment / ootUpgrades).
constexpr u16 kOoTOwnedBiggoronSlot = 1u << 2;   // EQUIP_INV_SWORD_BIGGORON
constexpr u16 kOoTOwnedBrokenKnife = 1u << 3;    // EQUIP_INV_SWORD_BROKENGIANTKNIFE
constexpr u16 kOoTOwnedMirrorShield = 1u << 6;   // EQUIP_INV_SHIELD_MIRROR
constexpr s32 kOoTUpgradeShiftStrength = 6;
constexpr s32 kOoTUpgradeShiftScale = 9;
constexpr s32 kOoTUpgradeShiftBulletBag = 14;
struct EquipBitMap {
    u16 ootBit; // bit of Ocarina of Time's inventory.equipment
    u8 item;    // Termina's ITEM_OOT_*
};
const EquipBitMap kTunicsBoots[] = {
    { 1u << 9, ITEM_OOT_TUNIC_GORON },
    { 1u << 10, ITEM_OOT_TUNIC_ZORA },
    { 1u << 13, ITEM_OOT_BOOTS_IRON },
    { 1u << 14, ITEM_OOT_BOOTS_HOVER },
};

u8 OoTUpgrade(u32 upgrades, s32 shift) {
    return (u8)((upgrades >> shift) & 7);
}

void RaiseOoTUpgrade(uint32_t& upgrades, s32 shift, u8 level) {
    if (level > OoTUpgrade(upgrades, shift)) {
        upgrades = (upgrades & ~(7u << shift)) | ((u32)(level & 7) << shift);
    }
}

// ---- helpers --------------------------------------------------------------------------------------------------

u8 Max(u8 a, u8 b) {
    return (a > b) ? a : b;
}

u8 ClampAmmo(u8 count, s32 upgrade) {
    const s32 capacity = CUR_CAPACITY(upgrade);
    return (count > capacity) ? (u8)capacity : count;
}

void RaiseUpgrade(s32 upgrade, u8 level) {
    if (level > 3) {
        level = 3;
    }
    if ((u8)CUR_UPG_VALUE(upgrade) < level) {
        Inventory_ChangeUpgrade(upgrade, level);
    }
}

// C buttons (every form's set) showing the item in `slot` show its new content.
void RefreshButtons(s32 slot, u8 item) {
    for (s32 form = 0; form < 4; form++) {
        for (s32 button = EQUIP_SLOT_C_LEFT; button <= EQUIP_SLOT_C_RIGHT; button++) {
            if (C_SLOT_EQUIP(form, button) == slot) {
                BUTTON_ITEM_EQUIP(form, button) = item;
            }
        }
    }
}

// A content Majora's Mask keeps one of per world (MsDelivery.h MsDelivery_BottleOnePerWorld: the Deku Princess, the
// Seahorse) is in one of Termina's six bottles at most, and the Deku Princess in none while she is back in the Deku
// King's chamber this cycle (MM EnDnp_Init: Woodfall Temple's princess is there only while neither is so,
// WEEKEVENTREG_23_20). Hyrule cannot see Termina's week events, and a bottle the shared save brings (a Hyrule check's
// "Bottle with Deku Princess", a teammate's, one a "Combine Shared Items" seed or an older build gave twice) may be a
// second one: its bottle stays, empty (the user, 2026-09-30: "Stuff like deku princess absolutely should not do that").
void KeepOnePerWorld() {
    auto& inventory = gSaveContext.save.saveInfo.inventory;
    for (s32 i = 0; i < MS_SHARED_BOTTLE_SLOTS; i++) {
        const u8 item = inventory.items[SLOT_BOTTLE_1 + i];
        if (!MsDelivery_BottleOnePerWorld(BottleFromItem(item))) {
            continue;
        }
        bool second = (item == ITEM_DEKU_PRINCESS) && CHECK_WEEKEVENTREG(WEEKEVENTREG_23_20);
        for (s32 j = 0; (j < i) && !second; j++) {
            second = (inventory.items[SLOT_BOTTLE_1 + j] == item);
        }
        if (second) {
            inventory.items[SLOT_BOTTLE_1 + i] = ITEM_BOTTLE;
            RefreshButtons(SLOT_BOTTLE_1 + i, ITEM_BOTTLE);
            MergeShipHostTest_Log("SHARED bottle %d: a second 0x%02X, emptied (one per world)", i, item);
        }
    }
}

// ---- Hyrule's share: songs, quest status, items, equipment (Termina's Hyrule views) ----------------------------

MergeShipOoTItemsSave& OoTItems() {
    return gSaveContext.save.shipSaveInfo.mergeShipOoTItems;
}

MergeShipOoTEquipSave& OoTEquip() {
    return gSaveContext.save.shipSaveInfo.mergeShipOoTEquip;
}

// Termina's Hyrule views -> the shared save.
void CaptureHyrule(MsSharedSave* shared) {
    // Ocarina of Time's songs Majora's Mask has no bit for, and the rest of Hyrule's quest status: added.
    for (const SongMap& map : kHyruleSongs) {
        if (MergeShip_Hyrule_HasQuestItem(map.quest)) {
            shared->songs |= Bit(map.song);
        }
    }
    shared->hyruleQuest |= MergeShip_Hyrule_GetQuestItems() & MS_HYRULE_QUEST_BITS;

    // Masks: added. Trade items (Termina's own copies of Hyrule's): set, where Termina has the item at all.
    u32 hyruleItems = shared->hyruleItems;
    for (s32 i = 0; i < MS_HYRULE_ITEM_COUNT; i++) {
        const u8 item = kHyruleItems[i];
        if (item == ITEM_NONE) {
            continue;
        }
        if (MergeShip_OoTItems_Owns(item)) {
            hyruleItems |= Bit(i);
        } else if (!(MS_HYRULE_MASK_BITS & Bit(i))) {
            hyruleItems &= ~Bit(i);
        }
    }
    shared->hyruleItems = hyruleItems;

    // Hyrule's equipment Termina holds: added (the Deku Shield, which burns, is set).
    MergeShipOoTEquipSave& equip = OoTEquip();
    if (MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_KOKIRI)) {
        shared->swords |= (u8)Bit(MS_SWORD_KOKIRI); // Hyrule's own Kokiri Sword
    }
    if (MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_MASTER)) {
        shared->swords |= (u8)Bit(MS_SWORD_MASTER);
    }
    if (MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_BIGGORON)) {
        shared->swords |= (u8)Bit(MS_SWORD_BIGGORON);
    } else if (MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_GIANTS_KNIFE) ||
               MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_BROKEN)) {
        shared->swords |= (u8)Bit(MS_SWORD_GIANTS_KNIFE);
    }
    if (MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_DEKU)) {
        shared->shields |= (u8)Bit(MS_SHIELD_DEKU);
    } else {
        shared->shields &= (u8)~Bit(MS_SHIELD_DEKU);
    }
    if (MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_HYLIAN)) {
        shared->shields |= (u8)Bit(MS_SHIELD_HYLIAN);
    }
    if (MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_MIRROR)) {
        shared->shields |= (u8)Bit(MS_SHIELD_MIRROR);
        shared->ootOwnedEquipment |= kOoTOwnedMirrorShield;
    }
    for (const EquipBitMap& map : kTunicsBoots) {
        if (MergeShip_OoTEquip_Owns(map.item)) {
            shared->ootOwnedEquipment |= map.ootBit;
        }
    }
    RaiseOoTUpgrade(shared->ootUpgrades, kOoTUpgradeShiftStrength, equip.strength);
    RaiseOoTUpgrade(shared->ootUpgrades, kOoTUpgradeShiftScale, equip.scale);
    RaiseOoTUpgrade(shared->ootUpgrades, kOoTUpgradeShiftBulletBag, OoTItems().bulletBag);

    shared->hyruleBlock |= MS_HYRULE_BLOCK_2SHIP;
}

// The shared save -> Termina's Hyrule views. Writes the MergeShip save sections directly: nothing is equipped and no
// get-item text shows (the items were Link's already).
void ApplyHyrule(const MsSharedSave* shared) {
    // Songs and quest status: added.
    for (const SongMap& map : kHyruleSongs) {
        if (shared->songs & Bit(map.song)) {
            MergeShip_Hyrule_SetQuestItem(map.quest);
        }
    }
    const u32 quest = shared->hyruleQuest & MS_HYRULE_QUEST_BITS;
    for (u8 bit = 0; bit < MERGESHIP_OOT_QUEST_MAX; bit++) {
        if ((quest & Bit(bit)) && !MergeShip_Hyrule_HasQuestItem(bit)) {
            MergeShip_Hyrule_SetQuestItem(bit);
        }
    }

    // Equipment: added (the Deku Shield: set). Hyrule's swords and shields are Hyrule's here, on its own menu
    // (Equipment (Hyrule)) and usable, never MM's (the user, 2026-09-24: an item is only ever its own game's version,
    // on its own game's menu, in both games). With "Combine Shared Items" MM's give Hyrule's too
    // (SharedSave.h MsShared_CombineShields / _CombineSwords) and MergeShipShared_Apply gives MM's for Hyrule's.
    MergeShipOoTEquipSave& equip = OoTEquip();
    const bool combined = shared->itemsCombined != 0;
    if (MsShared_CombineSwords(shared->swords, combined ? 1 : 0) & Bit(MS_SWORD_KOKIRI)) {
        equip.owned |= MS_OOTEQ_OWN_BIT(ITEM_OOT_SWORD_KOKIRI);
    }
    if (shared->swords & Bit(MS_SWORD_MASTER)) {
        equip.owned |= MS_OOTEQ_OWN_BIT(ITEM_OOT_SWORD_MASTER);
    }
    const u32 longSword =
        MS_OOTEQ_OWN_BIT(ITEM_OOT_SWORD_BIGGORON) | MS_OOTEQ_OWN_BIT(ITEM_OOT_SWORD_GIANTS_KNIFE);
    if ((shared->swords & Bit(MS_SWORD_BIGGORON)) && !MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_BIGGORON)) {
        equip.owned |= MS_OOTEQ_OWN_BIT(ITEM_OOT_SWORD_BIGGORON);
        equip.bgsFlag = true;
        equip.swordHealth = MS_OOTEQ_KNIFE_HEALTH;
    } else if ((shared->swords & Bit(MS_SWORD_GIANTS_KNIFE)) && !(equip.owned & longSword)) {
        // Hyrule's knife is broken when it holds the Broken Giant's Knife and not Biggoron's Sword's slot.
        const bool broken = (shared->ootOwnedEquipment & kOoTOwnedBrokenKnife) &&
                            !(shared->ootOwnedEquipment & kOoTOwnedBiggoronSlot);
        equip.owned |= MS_OOTEQ_OWN_BIT(ITEM_OOT_SWORD_GIANTS_KNIFE);
        equip.bgsFlag = false;
        equip.swordHealth = broken ? 0 : MS_OOTEQ_KNIFE_HEALTH;
    } else if ((shared->swords & Bit(MS_SWORD_GIANTS_KNIFE)) &&
               MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_GIANTS_KNIFE) && !equip.bgsFlag &&
               (shared->ootOwnedEquipment & kOoTOwnedBrokenKnife) &&
               !(shared->ootOwnedEquipment & kOoTOwnedBiggoronSlot)) {
        // BUG-0088: the knife Termina already holds broke in Hyrule since -- it is broken here too (its blade and reach,
        // MergeShip_OoTEquip_HoldsBrokenKnife), not Biggoron's Sword's length.
        equip.swordHealth = 0;
    }
    if (shared->shields & Bit(MS_SHIELD_DEKU)) {
        equip.owned |= MS_OOTEQ_OWN_BIT(ITEM_OOT_SHIELD_DEKU);
    } else if (equip.owned & MS_OOTEQ_OWN_BIT(ITEM_OOT_SHIELD_DEKU)) {
        equip.owned &= ~MS_OOTEQ_OWN_BIT(ITEM_OOT_SHIELD_DEKU); // it burned in Hyrule
        if (equip.shield == MS_OOTEQ_SHIELD_DEKU) {
            equip.shield = MS_OOTEQ_SHIELD_MM;
        }
    }
    // (The old model's Hylian / Mirror bits could be MM's shields: only the split model's are Hyrule's.)
    const bool splitModel = shared->itemsModel >= MS_SHARED_ITEMS_MODEL_SPLIT;
    const u8 shields = MsShared_CombineShields(shared->shields, combined ? 1 : 0);
    if (splitModel && (shields & Bit(MS_SHIELD_HYLIAN))) {
        equip.owned |= MS_OOTEQ_OWN_BIT(ITEM_OOT_SHIELD_HYLIAN);
    }
    if (splitModel &&
        (((shared->shields & Bit(MS_SHIELD_MIRROR)) && (shared->ootOwnedEquipment & kOoTOwnedMirrorShield)) ||
         (combined && (shared->shields & Bit(MS_SHIELD_MM_MIRROR))))) {
        equip.owned |= MS_OOTEQ_OWN_BIT(ITEM_OOT_SHIELD_MIRROR);
    }
    for (const EquipBitMap& map : kTunicsBoots) {
        if (shared->ootOwnedEquipment & map.ootBit) {
            equip.owned |= MS_OOTEQ_OWN_BIT(map.item);
        }
    }
    equip.strength = Max(equip.strength, OoTUpgrade(shared->ootUpgrades, kOoTUpgradeShiftStrength));
    equip.scale = Max(equip.scale, (u8)CLAMP_MAX(OoTUpgrade(shared->ootUpgrades, kOoTUpgradeShiftScale), 2));

    // Link arrives holding what he held in Hyrule when Termina has nothing in hand (nothing is given: only what he
    // owns). Hyrule's equipped sword and shield (ootEquipment: EQUIP_VALUE_* nibbles, sword 2 Master / 3 Biggoron's,
    // shield 1 Deku / 2 Hylian / 3 Mirror) are selected in Equipment (Hyrule); MergeShip_OoTEquip_Update puts them in
    // hand (B, the shield) on the next frame.
    if (shared->lastWriter == MS_ENGINE_SOH) {
        const u8 hyruleSword = (u8)(shared->ootEquipment & 0xF);
        const u8 hyruleShield = (u8)((shared->ootEquipment >> 4) & 0xF);
        if ((GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_NONE) && (equip.sword == MS_OOTEQ_SWORD_MM)) {
            if ((hyruleSword == 1) && MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_KOKIRI)) {
                equip.sword = MS_OOTEQ_SWORD_KOKIRI;
            } else if ((hyruleSword == 2) && MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_MASTER)) {
                equip.sword = MS_OOTEQ_SWORD_MASTER;
            } else if ((hyruleSword == 3) && (equip.owned & longSword)) {
                equip.sword = MS_OOTEQ_SWORD_LONG;
            }
        }
        if ((GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_NONE) && (equip.shield == MS_OOTEQ_SHIELD_MM)) {
            if ((hyruleShield == 3) && MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_MIRROR)) {
                equip.shield = MS_OOTEQ_SHIELD_MIRROR;
            } else if ((hyruleShield == 2) && MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_HYLIAN)) {
                equip.shield = MS_OOTEQ_SHIELD_HYLIAN;
            } else if ((hyruleShield == 1) && MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_DEKU)) {
                equip.shield = MS_OOTEQ_SHIELD_DEKU;
            }
        }
    }

    // Items (after MergeShipShared_Apply gave the Slingshot): the bullet bag raised, the seeds set.
    MergeShipOoTItemsSave& items = OoTItems();
    items.bulletBag = Max(items.bulletBag, (u8)CLAMP_MAX(OoTUpgrade(shared->ootUpgrades, kOoTUpgradeShiftBulletBag), 3));
    if (!(shared->hyruleBlock & MS_HYRULE_BLOCK_SOH) && !(shared->hyruleBlock & MS_HYRULE_BLOCK_2SHIP)) {
        return; // nobody has written the rest of the Hyrule block yet: its zeros are not "none"
    }
    // Deku Seeds are Termina's own (BUG-0105): MergeShip_OoTItems_Give filled them when the Slingshot came.

    // Masks: added. Trade items: set (one Hyrule traded away is gone here too).
    for (s32 i = 0; i < MS_HYRULE_ITEM_COUNT; i++) {
        const u8 item = kHyruleItems[i];
        if (item == ITEM_NONE) {
            continue;
        }
        const bool inShared = (shared->hyruleItems & Bit(i)) != 0;
        if (inShared && !MergeShip_OoTItems_Owns(item)) {
            MergeShip_OoTItems_SetOwned(item, true); // BUG-0066: the five masks own bits past `owned`
        } else if (!inShared && !(MS_HYRULE_MASK_BITS & Bit(i)) && MergeShip_OoTItems_Owns(item)) {
            MergeShip_OoTItems_Remove(item);
        }
    }
    // The trade cells show what Hyrule's slots hold (0: the latest owned one).
    const u8 child = (shared->hyruleTradeChild < MS_HYRULE_ITEM_COUNT) ? kHyruleItems[shared->hyruleTradeChild] : 0;
    const u8 adult = (shared->hyruleTradeAdult < MS_HYRULE_ITEM_COUNT) ? kHyruleItems[shared->hyruleTradeAdult] : 0;
    items.tradeChild = ((child != ITEM_NONE) && (child != 0) && MergeShip_OoTItems_Owns(child)) ? child : 0;
    items.tradeAdult = ((adult != ITEM_NONE) && (adult != 0) && MergeShip_OoTItems_Owns(adult)) ? adult : 0;
}

} // namespace

// ---- BUG-0101: the C and D-pad buttons carried between the worlds ---------------------------------------------------

// A button's item (and its slot: Hyrule's cells, MS_OOT_BUTTON_SLOT, hold Hyrule's items) as an MsButtonItem.
static u8 ButtonItemOf(u8 item, u8 slot) {
    if (MS_OOT_BUTTON_SLOT_IS_OOT(slot)) {
        switch (item) {
            case ITEM_BOW:
                return MS_BTN_BOW;
            case ITEM_ARROW_FIRE:
            case ITEM_BOW_FIRE:
                return MS_BTN_ARROW_FIRE;
            case ITEM_ARROW_ICE:
            case ITEM_BOW_ICE:
                return MS_BTN_ARROW_ICE;
            case ITEM_ARROW_LIGHT:
            case ITEM_BOW_LIGHT:
                return MS_BTN_ARROW_LIGHT;
            case ITEM_SLINGSHOT:
                return MS_BTN_SLINGSHOT;
            case ITEM_LONGSHOT:
                return MS_BTN_HOOKSHOT;
            case ITEM_OOT_BOOMERANG:
                return MS_BTN_BOOMERANG;
            case ITEM_OOT_HAMMER:
                return MS_BTN_HAMMER;
            case ITEM_OOT_DINS_FIRE:
                return MS_BTN_DINS_FIRE;
            case ITEM_OOT_FARORES_WIND:
                return MS_BTN_FARORES_WIND;
            case ITEM_OOT_NAYRUS_LOVE:
                return MS_BTN_NAYRUS_LOVE;
            case ITEM_LENS_OF_TRUTH:
                return MS_BTN_LENS;
            default:
                return MS_BTN_NONE;
        }
    }
    switch (item) {
        case ITEM_DEKU_STICK:
            return MS_BTN_STICK;
        case ITEM_DEKU_NUT:
            return MS_BTN_NUT;
        case ITEM_BOMB:
            return MS_BTN_BOMB;
        case ITEM_BOW:
            return MS_BTN_BOW;
        case ITEM_ARROW_FIRE:
        case ITEM_BOW_FIRE:
            return MS_BTN_ARROW_FIRE;
        case ITEM_ARROW_ICE:
        case ITEM_BOW_ICE:
            return MS_BTN_ARROW_ICE;
        case ITEM_ARROW_LIGHT:
        case ITEM_BOW_LIGHT:
            return MS_BTN_ARROW_LIGHT;
        case ITEM_OCARINA_OF_TIME:
            return MS_BTN_OCARINA;
        case ITEM_BOMBCHU:
            return MS_BTN_BOMBCHU;
        case ITEM_HOOKSHOT:
            return MS_BTN_HOOKSHOT;
        case ITEM_LENS_OF_TRUTH:
            return MS_BTN_LENS;
        case ITEM_MAGIC_BEANS:
            return MS_BTN_BEAN;
        default:
            if ((item >= ITEM_MASK_DEKU) && (item <= ITEM_MASK_GIANT)) {
                return (u8)(MS_BTN_MM_MASK_FIRST + (item - ITEM_MASK_DEKU));
            }
            return MS_BTN_NONE;
    }
}

// Termina's item and button slot for a carried button item, or false when Termina has none of it.
static bool ButtonItemHere(u8 btnItem, u8* item, u8* slot) {
    u8 mmItem = ITEM_NONE;
    u8 ootItem = ITEM_NONE;

    switch (btnItem) {
        case MS_BTN_STICK: mmItem = ITEM_DEKU_STICK; break;
        case MS_BTN_NUT: mmItem = ITEM_DEKU_NUT; break;
        case MS_BTN_BOMB: mmItem = ITEM_BOMB; break;
        case MS_BTN_BOW: mmItem = ITEM_BOW; break; // MM's bow slot holds either game's bow
        case MS_BTN_OCARINA: mmItem = ITEM_OCARINA_OF_TIME; break;
        case MS_BTN_BOMBCHU: mmItem = ITEM_BOMBCHU; break;
        case MS_BTN_LENS: mmItem = ITEM_LENS_OF_TRUTH; break;
        case MS_BTN_BEAN: mmItem = ITEM_MAGIC_BEANS; break;
        case MS_BTN_SLINGSHOT: ootItem = ITEM_SLINGSHOT; break;
        case MS_BTN_BOOMERANG: ootItem = ITEM_OOT_BOOMERANG; break;
        case MS_BTN_HAMMER: ootItem = ITEM_OOT_HAMMER; break;
        case MS_BTN_DINS_FIRE: ootItem = ITEM_OOT_DINS_FIRE; break;
        case MS_BTN_FARORES_WIND: ootItem = ITEM_OOT_FARORES_WIND; break;
        case MS_BTN_NAYRUS_LOVE: ootItem = ITEM_OOT_NAYRUS_LOVE; break;
        case MS_BTN_HOOKSHOT:
            if (INV_CONTENT(ITEM_HOOKSHOT) == ITEM_HOOKSHOT) {
                mmItem = ITEM_HOOKSHOT;
            } else if (MergeShip_OoTItems_HookshotLevel() > 0) {
                ootItem = ITEM_LONGSHOT; // Hyrule's Hookshot or Longshot cell
            }
            break;
        case MS_BTN_ARROW_FIRE:
        case MS_BTN_ARROW_ICE:
        case MS_BTN_ARROW_LIGHT: {
            // The pause screen puts the bow and the arrow on the button (ITEM_BOW_*), from the arrow's slot.
            const u8 arrow = (u8)(ITEM_ARROW_FIRE + (btnItem - MS_BTN_ARROW_FIRE));
            if ((INV_CONTENT(arrow) != arrow) || (INV_CONTENT(ITEM_BOW) != ITEM_BOW)) {
                return false;
            }
            *item = (u8)(ITEM_BOW_FIRE + (btnItem - MS_BTN_ARROW_FIRE));
            *slot = SLOT(arrow);
            return true;
        }
        default:
            if ((btnItem >= MS_BTN_MM_MASK_FIRST) && (btnItem <= MS_BTN_MM_MASK_LAST)) {
                mmItem = (u8)(ITEM_MASK_DEKU + (btnItem - MS_BTN_MM_MASK_FIRST));
            }
            break;
    }
    if ((mmItem != ITEM_NONE) && (INV_CONTENT(mmItem) == mmItem)) {
        *item = mmItem;
        *slot = SLOT(mmItem);
        return true;
    }
    if ((ootItem != ITEM_NONE) && MergeShip_OoTItems_Owns(ootItem) && (MergeShip_OoTItems_SlotOfItem(ootItem) >= 0)) {
        *item = ootItem;
        *slot = (u8)MS_OOT_BUTTON_SLOT(MergeShip_OoTItems_SlotOfItem(ootItem));
        return true;
    }
    return false;
}

// MsSharedSave.buttonItems' order (C-left, C-down, C-right, D-pad up, down, left, right) as Termina's buttons.
struct ButtonRef {
    bool dpad;
    s32 btn;
};
static const ButtonRef kButtons[MS_SHARED_BUTTONS] = {
    { false, EQUIP_SLOT_C_LEFT }, { false, EQUIP_SLOT_C_DOWN }, { false, EQUIP_SLOT_C_RIGHT },
    { true, EQUIP_SLOT_D_UP },    { true, EQUIP_SLOT_D_DOWN },  { true, EQUIP_SLOT_D_LEFT },
    { true, EQUIP_SLOT_D_RIGHT },
};

static u8& ButtonItemRef(const ButtonRef& b) {
    return b.dpad ? DPAD_BUTTON_ITEM_EQUIP(0, b.btn) : BUTTON_ITEM_EQUIP(0, b.btn);
}
static u8& ButtonSlotRef(const ButtonRef& b) {
    return b.dpad ? DPAD_SLOT_EQUIP(0, b.btn) : C_SLOT_EQUIP(0, b.btn);
}

static void CaptureButtons(MsSharedSave* shared) {
    for (s32 i = 0; i < MS_SHARED_BUTTONS; i++) {
        const u8 slot = ButtonSlotRef(kButtons[i]);
        // A bottle's content is the bottle's, not an item the other world has.
        const bool bottle = (slot >= SLOT_BOTTLE_1) && (slot <= SLOT_BOTTLE_6);
        shared->buttonItems[i] = bottle ? (u8)MS_BTN_NONE : ButtonItemOf(ButtonItemRef(kButtons[i]), slot);
    }
    shared->buttonsWriter = MS_ENGINE_2SHIP;
}

static void ApplyButtons(const MsSharedSave* shared) {
    if (shared->buttonsWriter != MS_ENGINE_SOH) {
        return;
    }
    for (s32 i = 0; i < MS_SHARED_BUTTONS; i++) {
        u8 item;
        u8 slot;
        if ((shared->buttonItems[i] == MS_BTN_NONE) || !ButtonItemHere(shared->buttonItems[i], &item, &slot)) {
            continue; // nothing carried, or Termina has none of it: the button keeps what Termina had there
        }
        // The item leaves any other button of Termina's (one item, one button).
        for (s32 other = 0; other < MS_SHARED_BUTTONS; other++) {
            if ((other != i) &&
                (ButtonItemOf(ButtonItemRef(kButtons[other]), ButtonSlotRef(kButtons[other])) == shared->buttonItems[i])) {
                ButtonItemRef(kButtons[other]) = ITEM_NONE;
                ButtonSlotRef(kButtons[other]) = SLOT_NONE;
            }
        }
        ButtonItemRef(kButtons[i]) = item;
        ButtonSlotRef(kButtons[i]) = slot;
    }
    const_cast<MsSharedSave*>(shared)->buttonsWriter = 0; // once (the shared save the host holds is the player's)
    MergeShipHostTest_Log("BUTTONS from Hyrule: C %02X %02X %02X D %02X %02X %02X %02X",
                          BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT), BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_DOWN),
                          BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_RIGHT), DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_UP),
                          DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_DOWN), DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_LEFT),
                          DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_RIGHT));
}

extern "C" void MergeShipShared_Capture(MsSharedSave* shared) {
    if (shared == nullptr) {
        return;
    }
    auto& player = gSaveContext.save.saveInfo.playerData;
    auto& inventory = gSaveContext.save.saveInfo.inventory;

    // Life, magic, money: set (capacities never go down).
    shared->healthCapacity = (u16)player.healthCapacity;
    shared->health = player.health;
    shared->heartPieces = (u8)((inventory.questItems & 0xF0000000) >> QUEST_HEART_PIECE_COUNT);
    shared->doubleDefense = Max(shared->doubleDefense, player.doubleDefense ? 1 : 0);
    shared->magicLevel = Max(shared->magicLevel, player.isMagicAcquired ? (player.isDoubleMagicAcquired ? 2 : 1) : 0);
    shared->magicAmount = (u8)player.magic;
    shared->rupees = (u16)player.rupees;
    shared->walletLevel = Max(shared->walletLevel, (u8)CUR_UPG_VALUE(UPG_WALLET));
    // The bank's balance, with the rupees (SharedSave.h bankRupees, BUG-0030).
    shared->bankRupees[0] = (u8)(HS_GET_BANK_RUPEES() & 0xFF);
    shared->bankRupees[1] = (u8)((HS_GET_BANK_RUPEES() >> 8) & 0xFF);
    shared->bankValid = 1;

    // Masks: added.
    for (s32 form = 0; form < 4; form++) {
        if (INV_CONTENT(kTransformationMasks[form]) == kTransformationMasks[form]) {
            shared->transformationMasks |= (u8)Bit(form);
        }
    }
    for (s32 mask = 1; mask <= 20; mask++) {
        if (INV_CONTENT(RegularMaskItem(mask)) == RegularMaskItem(mask)) {
            shared->mmMasks |= Bit(mask);
        }
    }

    // Items: added; counts set.
    if (shared->itemsModel < MS_SHARED_ITEMS_MODEL_SPLIT) {
        // A shared save of the old model, where one bit named either game's Hookshot / Hylian-Hero's / Mirror Shield
        // (and MM's ocarina was the Ocarina of Time): dropped before Termina writes its own (SoH's next capture writes
        // Hyrule's again; CaptureHyrule re-adds Hyrule's shields Termina holds).
        shared->items &= ~(Bit(MS_ITEM_HOOKSHOT) | Bit(MS_ITEM_OCARINA_OF_TIME));
        shared->shields &= (u8)~(Bit(MS_SHIELD_MIRROR) | Bit(MS_SHIELD_HYLIAN));
    }
    if (shared->itemsModel < MS_SHARED_ITEMS_MODEL_SPLIT_ALL) {
        // Model 1 named the bow (while Hyrule had a Fairy Bow), the magic arrows and the Lens of both games with
        // Hyrule's bit: dropped before Termina writes MM's own (SoH's next capture writes Hyrule's again).
        for (s32 i = 0; i < MS_SHARED_ITEM_VERSION_COUNT; i++) {
            shared->items &= ~MsShared_VersionBit(i, 0);
        }
    }
    // Majora's Mask's one ocarina is an ocarina to Hyrule: level 1 of the progressive ocarina (Hyrule's Fairy Ocarina,
    // as the unified seed counts it, M3). It never makes Hyrule's the Ocarina of Time, which only Hyrule gives (an MM
    // item never grants its Ocarina of Time counterpart).
    // The bow is MM's Hero's Bow (MS_ITEM_MM_BOW: Hyrule gets MM's Hero's Bow for it, not the Fairy Bow), the magic
    // arrows and the Lens MM's own (MS_ITEM_MM_*: Hyrule cannot hold them).
    for (const ItemMap& map : kItems) {
        // MM's bow slot also holds Hyrule's Fairy Bow for use here (MergeShip_Own_Sync, MergeShipOwnership.h): only
        // Termina's own Hero's Bow is MS_ITEM_MM_BOW. Read off the slot, Hyrule's Fairy Bow went through the door as a
        // Hero's Bow -- SoH's apply gave Hyrule ITEM_MM_BOW_HERO and marked terminaOwn's MS_OWN_BOW, and back here the
        // Fairy Bow's C button turned into the Hero's Bow (the reported bug, 2026-09-30).
        if ((map.bit == MS_ITEM_MM_BOW) && !MergeShip_Own_MMOwns(ITEM_BOW)) {
            continue;
        }
        if (INV_CONTENT(map.item) == map.item) {
            shared->items |= Bit((map.bit == MS_ITEM_OCARINA_OF_TIME) ? MS_ITEM_OCARINA_FAIRY : map.bit);
        }
    }
    // MM's own ocarina, for Hyrule's MM item menu (never Hyrule's ocarina, 2026-09-24).
    if (INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_OCARINA_OF_TIME) {
        shared->items |= Bit(MS_ITEM_MM_OCARINA);
    } else {
        shared->items &= ~Bit(MS_ITEM_MM_OCARINA);
    }
    if (CHECK_QUEST_ITEM(QUEST_BOMBERS_NOTEBOOK)) {
        shared->items |= Bit(MS_ITEM_BOMBERS_NOTEBOOK);
    }
    for (const ItemMap& map : kOoTItems) {
        if ((map.bit != MS_ITEM_LONGSHOT) && MergeShip_OoTItems_Owns(map.item)) {
            shared->items |= Bit(map.bit);
        }
    }
    // Hyrule's Hookshot / Longshot (the Items (Hyrule) hookshot cell): Hyrule's own, never MM's Hookshot.
    const s32 hookshotLevel = MergeShip_OoTItems_HookshotLevel();
    shared->items |= (hookshotLevel >= 1) ? Bit(MS_ITEM_OOT_HOOKSHOT) : 0;
    shared->items |= (hookshotLevel >= 2) ? Bit(MS_ITEM_LONGSHOT) : 0;
    shared->quiverLevel = Max(shared->quiverLevel, (u8)CUR_UPG_VALUE(UPG_QUIVER));
    shared->bombBagLevel = Max(shared->bombBagLevel, (u8)CUR_UPG_VALUE(UPG_BOMB_BAG));
    // Majora's Mask starts every file with room for sticks and nuts (sSaveDefaultInventory): that room is Hyrule's
    // only with the sticks / nuts themselves (a Deku Stick / Nut bag is a randomizer item in Hyrule).
    if (INV_CONTENT(ITEM_DEKU_STICK) == ITEM_DEKU_STICK) {
        shared->stickLevel = Max(shared->stickLevel, (u8)CUR_UPG_VALUE(UPG_DEKU_STICKS));
    }
    if (INV_CONTENT(ITEM_DEKU_NUT) == ITEM_DEKU_NUT) {
        shared->nutLevel = Max(shared->nutLevel, (u8)CUR_UPG_VALUE(UPG_DEKU_NUTS));
    }
    shared->arrows = (INV_CONTENT(ITEM_BOW) == ITEM_BOW) ? (u8)AMMO(ITEM_BOW) : shared->arrows;
    shared->bombs = (INV_CONTENT(ITEM_BOMB) == ITEM_BOMB) ? (u8)AMMO(ITEM_BOMB) : shared->bombs;
    shared->bombchus = (INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU) ? (u8)AMMO(ITEM_BOMBCHU) : shared->bombchus;
    shared->sticks = (INV_CONTENT(ITEM_DEKU_STICK) == ITEM_DEKU_STICK) ? (u8)AMMO(ITEM_DEKU_STICK) : shared->sticks;
    shared->nuts = (INV_CONTENT(ITEM_DEKU_NUT) == ITEM_DEKU_NUT) ? (u8)AMMO(ITEM_DEKU_NUT) : shared->nuts;
    shared->magicBeans =
        (INV_CONTENT(ITEM_MAGIC_BEANS) == ITEM_MAGIC_BEANS) ? (u8)AMMO(ITEM_MAGIC_BEANS) : shared->magicBeans;
    shared->powderKegs = (INV_CONTENT(ITEM_POWDER_KEG) == ITEM_POWDER_KEG) ? (u8)AMMO(ITEM_POWDER_KEG) : 0;

    // Bottles: the content is set; a bottle is never taken away.
    for (s32 i = 0; i < MS_SHARED_BOTTLE_SLOTS; i++) {
        const u8 item = inventory.items[SLOT_BOTTLE_1 + i];
        if (item != ITEM_NONE) {
            shared->bottles[i] = BottleFromItem(item);
        }
    }

    // Equipment: Majora's Mask keeps only the equipped sword and shield. Its upgrades are added. The Kokiri Sword is
    // the one sword both games share (Hyrule's; MM's Kokiri Sword is the one Link brought), never added from here: a
    // Termina file made by the door starts without it (MergeShipHost.cpp), and an older file's starting one is not
    // Hyrule's. MM's shields are MM's (MS_SHIELD_HERO / MS_SHIELD_MM_MIRROR): Hyrule gets MM's Hero's / Mirror Shield
    // for them, never its own Hylian / Mirror Shield. An older file's starting Hero's Shield is not added.
    const s32 sword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    // MM's own Kokiri Sword (2026-09-24: its own bit; Hyrule shows it as MM's, never as Ocarina of Time's).
    if (sword == EQUIP_VALUE_SWORD_KOKIRI) {
        shared->swords |= (u8)Bit(MS_SWORD_MM_KOKIRI);
    }
    if (sword == EQUIP_VALUE_SWORD_RAZOR) {
        shared->swords |= (u8)Bit(MS_SWORD_RAZOR);
        shared->razorSwordHealth = (u8)player.swordHealth;
    } else if (sword == EQUIP_VALUE_SWORD_GILDED) {
        shared->swords |= (u8)(Bit(MS_SWORD_RAZOR) | Bit(MS_SWORD_GILDED));
    }
    const s32 mmShield = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    if ((mmShield == EQUIP_VALUE_SHIELD_MIRROR) || MergeShip_TerminaEquip_Owns(ITEM_SHIELD_MIRROR)) {
        shared->shields |= (u8)Bit(MS_SHIELD_MM_MIRROR);
    }
    if (((mmShield == EQUIP_VALUE_SHIELD_HERO) || MergeShip_TerminaEquip_Owns(ITEM_SHIELD_HERO)) &&
        (OoTEquip().variants & MERGESHIP_SHARED_BARE_START)) {
        shared->shields |= (u8)Bit(MS_SHIELD_HERO);
    }

    // Songs, remains, owls: added. Stray fairies and trade slots: set.
    for (const SongMap& map : kSongs) {
        if (CHECK_QUEST_ITEM(map.quest)) {
            shared->songs |= Bit(map.song);
        }
    }
    for (s32 remains = 0; remains < 4; remains++) {
        if (CHECK_QUEST_ITEM(QUEST_REMAINS_ODOLWA + remains)) {
            shared->remains |= (u8)Bit(remains);
        }
    }
    shared->owlActivationFlags |= player.owlActivationFlags;
    shared->strayFairies[0] = CHECK_WEEKEVENTREG(WEEKEVENTREG_08_80) ? 1 : 0;
    for (s32 area = 0; area < 4; area++) {
        shared->strayFairies[1 + area] = (u8)inventory.strayFairies[area];
    }
    shared->mmTradeDeed = inventory.items[SLOT_TRADE_DEED];
    shared->mmTradeKeyMama = inventory.items[SLOT_TRADE_KEY_MAMA];
    shared->mmTradeCouple = inventory.items[SLOT_TRADE_COUPLE];
    // Every trade item owned (the slots cycle through them, 2026-09-24): a randomizer file's obtained flags, and what
    // the slots show.
    u16 tradeOwned = 0;
    for (s32 i = 0; i <= ITEM_PENDANT_OF_MEMORIES - ITEM_MOONS_TEAR; i++) {
        const u8 item = (u8)(ITEM_MOONS_TEAR + i);
        if ((IS_RANDO && Flags_GetRandoInf(RANDO_INF_OBTAINED_MOONS_TEAR + i)) ||
            (inventory.items[SLOT_TRADE_DEED] == item) || (inventory.items[SLOT_TRADE_KEY_MAMA] == item) ||
            (inventory.items[SLOT_TRADE_COUPLE] == item)) {
            tradeOwned |= (u16)(1u << i);
        }
    }
    shared->mmTradeOwned = tradeOwned;

    // Hyrule's songs, quest status, items and equipment as Termina holds them.
    CaptureHyrule(shared);
    // Termina Equipment: MM's swords and shields Termina holds and the ones in use (MergeShipTerminaEquip.h). Last:
    // it notes the sword and shield lists as this capture leaves them.
    MergeShip_TerminaEquip_CaptureShared(shared);

    shared->itemsModel = MS_SHARED_ITEMS_MODEL_SPLIT_ALL;
    CaptureButtons(shared); // BUG-0101
    MergeShip_TerminaEquip_CaptureBSword(shared); // BUG-0120
    shared->generation++;
    shared->lastWriter = MS_ENGINE_2SHIP;
    MergeShipHostTest_Log("SHARED capture gen=%u hp=%d/%d rupees=%d bank=%d masks=0x%X/0x%08X items=0x%08X songs=0x%08X",
                          shared->generation, shared->health, shared->healthCapacity, shared->rupees,
                          shared->bankValid ? (shared->bankRupees[0] | (shared->bankRupees[1] << 8)) : -1,
                          shared->transformationMasks, shared->mmMasks, shared->items, shared->songs);
    MergeShipHostTest_Log("SHARED capture hyrule quest=0x%06X items=0x%06X seeds=%d swords=0x%02X shields=0x%02X "
                          "equipment=0x%04X upgrades=0x%06X",
                          shared->hyruleQuest, shared->hyruleItems, shared->hyruleSeeds, shared->swords,
                          shared->shields, shared->ootOwnedEquipment, shared->ootUpgrades);
}

extern "C" void MergeShipShared_Apply(const MsSharedSave* shared, int flags) {
    if ((shared == nullptr) || (shared->lastWriter == 0)) {
        return;
    }
    // Majora's Mask's first cycle is still its own script: its ocarina, the Song of Time, the Song of Healing and the
    // transformation masks wait for it (MergeShipSharedSave.h, MergeShipShared_ApplyHeldBack).
    const bool hold = (flags & MERGESHIP_SHARED_HOLD_FIRST_CYCLE) != 0;
    auto& player = gSaveContext.save.saveInfo.playerData;
    auto& inventory = gSaveContext.save.saveInfo.inventory;

    // Life, magic, money: set.
    s16 capacity = (s16)shared->healthCapacity;
    if (capacity > MS_SHARED_MAX_HEALTH_CAPACITY) {
        capacity = MS_SHARED_MAX_HEALTH_CAPACITY;
    }
    if (capacity >= 0x30) {
        player.healthCapacity = capacity;
    }
    player.health = (shared->health > 0) ? shared->health : 0x30;
    if (player.health > player.healthCapacity) {
        player.health = player.healthCapacity;
    }
    inventory.questItems = (inventory.questItems & 0x0FFFFFFF) | ((u32)(shared->heartPieces & 3) << QUEST_HEART_PIECE_COUNT);
    if (shared->doubleDefense) {
        player.doubleDefense = true;
        inventory.defenseHearts = 20;
    }
    const s32 magicLevel = player.isMagicAcquired ? (player.isDoubleMagicAcquired ? 2 : 1) : 0;
    if (shared->magicLevel > magicLevel) {
        player.isMagicAcquired = true;
        player.isDoubleMagicAcquired = (shared->magicLevel >= 2);
        player.magicLevel = 0; // the HUD sizes the meter again, as on loading a file
    }
    // The magic in it only fills a meter Link owns (Hyrule's, carried above, or MM's own Great Fairy's): Termina never
    // gets a meter by itself (a new MM file has none, MergeShipHost.cpp).
    player.magic = player.isMagicAcquired ? (s8)shared->magicAmount : 0;
    RaiseUpgrade(UPG_WALLET, (shared->walletLevel > 2) ? 2 : shared->walletLevel);
    player.rupees = (s16)((shared->rupees > CUR_CAPACITY(UPG_WALLET)) ? CUR_CAPACITY(UPG_WALLET) : shared->rupees);
    if (shared->bankValid) {
        HS_SET_BANK_RUPEES((u32)shared->bankRupees[0] | ((u32)shared->bankRupees[1] << 8));
    }

    // Masks: added.
    for (s32 form = 0; (form < 4) && !hold; form++) {
        if (shared->transformationMasks & Bit(form)) {
            INV_CONTENT(kTransformationMasks[form]) = kTransformationMasks[form];
        }
    }
    for (s32 mask = 1; mask <= 20; mask++) {
        if (shared->mmMasks & Bit(mask)) {
            INV_CONTENT(RegularMaskItem(mask)) = RegularMaskItem(mask);
        }
    }

    // Items: added; capacities raised; counts set. What both games have and is one item (MsItems.def: the bow and
    // arrows, bombs, bombchus, sticks, nuts, beans, the Lens, the ocarina below) goes to MM's own slot, the only one
    // Termina has for it. MM's Hookshot comes only from MM's Hookshot (Hyrule's go to Items (Hyrule), below).
    // The bow, the magic arrows and the Lens: MM's own bits (MsShared_VersionBit); with the unified seed's "Combine
    // Shared Items" (itemsCombined) Hyrule's give MM's too.
    const bool splitModel = shared->itemsModel >= MS_SHARED_ITEMS_MODEL_SPLIT;
    const u32 items = MsShared_CombineVersions(shared->items, shared->itemsCombined);
    // The user's rule (2026-09-24, "all items except masks"): what both games have -- the ocarina, bombs, bombchus,
    // sticks, nuts, beans, their bags and quivers, the ammo and the bottles -- is each game's own, as the bow, the magic
    // arrows and the Lens are. Hyrule's never fill Termina's slots; only the seed's "Combine Shared Items" does.
    const bool combined = shared->itemsCombined != 0;
    std::vector<u8> newItems;
    for (const ItemMap& map : kItems) {
        if (hold && (map.item == ITEM_OCARINA_OF_TIME)) {
            continue;
        }
        if (!combined && MergeShipShared_BothGamesBit(map.bit)) {
            continue;
        }
        if ((map.bit == MS_ITEM_HOOKSHOT) && !splitModel) {
            continue; // the old model's bit named either game's Hookshot
        }
        if ((items & Bit(map.bit)) && (INV_CONTENT(map.item) == ITEM_NONE)) {
            INV_CONTENT(map.item) = map.item;
            newItems.push_back(map.item); // BUG-0105: arrives full, below
        }
    }
    // MergeShip2 M3: any of Hyrule's ocarinas lets Termina play (MM's one ocarina).
    if (!hold && combined && (shared->items & Bit(MS_ITEM_OCARINA_FAIRY)) &&
        (INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_NONE)) {
        INV_CONTENT(ITEM_OCARINA_OF_TIME) = ITEM_OCARINA_OF_TIME;
    }
    // A carried ocarina shows on C-Left, where a new MM file keeps the ocarina's button (sSaveDefaultItemEquips).
    if ((INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_OCARINA_OF_TIME) &&
        (C_SLOT_EQUIP(0, EQUIP_SLOT_C_LEFT) == SLOT_OCARINA) && (BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT) == ITEM_NONE)) {
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT) = ITEM_OCARINA_OF_TIME;
    }
    if (shared->items & Bit(MS_ITEM_BOMBERS_NOTEBOOK)) {
        SET_QUEST_ITEM(QUEST_BOMBERS_NOTEBOOK);
    }
    // Hyrule's own items (Slingshot, Boomerang, Hammer, the spells): Termina's Items (Hyrule) view.
    for (const ItemMap& map : kOoTItems) {
        if ((map.bit != MS_ITEM_LONGSHOT) && (shared->items & Bit(map.bit)) && !MergeShip_OoTItems_Owns(map.item)) {
            MergeShip_OoTItems_Give(gPlayState, map.item);
        }
    }
    // Hyrule's Hookshot / Longshot, magic arrows and Lens of Truth: the Items (Hyrule) cells, Hyrule's own version,
    // usable here (never MM's Hookshot, arrows or Lens, unless the seed combines them: above).
    if ((items & Bit(MS_ITEM_LONGSHOT)) && (MergeShip_OoTItems_HookshotLevel() < 2)) {
        MergeShip_OoTItems_Give(gPlayState, ITEM_LONGSHOT);
    }
    if ((items & Bit(MS_ITEM_OOT_HOOKSHOT)) && (MergeShip_OoTItems_HookshotLevel() < 1)) {
        OoTItems().owned |= Bit(MS_OOT_OWN_HOOKSHOT);
    }
    OoTItems().owned |= ((shared->items & Bit(MS_ITEM_ARROW_FIRE)) ? Bit(MS_OOT_OWN_ARROW_FIRE) : 0) |
                        ((shared->items & Bit(MS_ITEM_ARROW_ICE)) ? Bit(MS_OOT_OWN_ARROW_ICE) : 0) |
                        ((shared->items & Bit(MS_ITEM_ARROW_LIGHT)) ? Bit(MS_OOT_OWN_ARROW_LIGHT) : 0) |
                        ((shared->items & Bit(MS_ITEM_LENS_OF_TRUTH)) ? Bit(MS_OOT_OWN_LENS) : 0);
    if (combined) {
        RaiseUpgrade(UPG_QUIVER, shared->quiverLevel);
        RaiseUpgrade(UPG_BOMB_BAG, shared->bombBagLevel);
    }
    // Bombs need a bomb bag (z_parameter.c Item_Give): bombs an older build gave without one are taken back.
    if ((CUR_UPG_VALUE(UPG_BOMB_BAG) == 0) && (INV_CONTENT(ITEM_BOMB) == ITEM_BOMB)) {
        INV_CONTENT(ITEM_BOMB) = ITEM_NONE;
        AMMO(ITEM_BOMB) = 0;
    }
    if (combined) {
        RaiseUpgrade(UPG_DEKU_STICKS, shared->stickLevel);
        RaiseUpgrade(UPG_DEKU_NUTS, shared->nutLevel);
    }
    // Termina's counts are its own, combined or not (BUG-0105, 2026-10-12: "no shared ammo period"): an item this
    // apply gave Termina for the first time arrives full, as a found one does.
    for (u8 item : newItems) {
        switch (item) {
            case ITEM_BOW:
                RaiseUpgrade(UPG_QUIVER, 1);
                AMMO(ITEM_BOW) = (s8)CUR_CAPACITY(UPG_QUIVER);
                break;
            case ITEM_BOMB:
                AMMO(ITEM_BOMB) = (s8)CUR_CAPACITY(UPG_BOMB_BAG);
                break;
            case ITEM_BOMBCHU:
                AMMO(ITEM_BOMBCHU) = (s8)MergeShip_Own_ChuCapacity();
                break;
            case ITEM_DEKU_STICK:
                RaiseUpgrade(UPG_DEKU_STICKS, 1);
                AMMO(ITEM_DEKU_STICK) = (s8)CUR_CAPACITY(UPG_DEKU_STICKS);
                break;
            case ITEM_DEKU_NUT:
                RaiseUpgrade(UPG_DEKU_NUTS, 1);
                AMMO(ITEM_DEKU_NUT) = (s8)CUR_CAPACITY(UPG_DEKU_NUTS);
                break;
            case ITEM_MAGIC_BEANS:
                AMMO(ITEM_MAGIC_BEANS) = 10;
                break;
            case ITEM_POWDER_KEG:
                AMMO(ITEM_POWDER_KEG) = 1;
                break;
            default:
                break;
        }
    }

    // Bottles: a bottle is never taken away; what is in it is set. Majora's Mask's six are `bottles` (Hyrule may have
    // found or used one, MergeShipOwnership.h); Ocarina of Time's are `ootBottles`, read live on the Items (Hyrule)
    // page. Combined, `bottles` is the one set both games hold.
    for (s32 i = 0; i < MS_SHARED_BOTTLE_SLOTS; i++) {
        const u8 item = ItemFromBottle(shared->bottles[i]);
        if ((item != ITEM_NONE) && (inventory.items[SLOT_BOTTLE_1 + i] != item)) {
            inventory.items[SLOT_BOTTLE_1 + i] = item;
            RefreshButtons(SLOT_BOTTLE_1 + i, item);
        }
    }
    // Never a second Deku Princess (or Seahorse) in Termina, whichever way the bottles above brought it.
    KeepOnePerWorld();

    // Equipment: the best sword of the chain (the Kokiri Sword is the one both games share; Razor and Gilded are MM's),
    // never a downgrade, and MM's own shields. Hyrule's Hylian and Mirror Shields are Hyrule's (Equipment (Hyrule),
    // ApplyHyrule): they never give MM's Hero's or Mirror Shield (the user's rule).
    s32 sword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    s32 bestSword = (shared->swords & Bit(MS_SWORD_GILDED))   ? EQUIP_VALUE_SWORD_GILDED
                    : (shared->swords & Bit(MS_SWORD_RAZOR))  ? EQUIP_VALUE_SWORD_RAZOR
                    // MM's own Kokiri Sword (Hyrule's is Equipment (Hyrule)'s), or either with "Combine Shared Items"
                    : (MsShared_CombineSwords(shared->swords, combined ? 1 : 0) & Bit(MS_SWORD_MM_KOKIRI))
                        ? EQUIP_VALUE_SWORD_KOKIRI
                                                              : EQUIP_VALUE_SWORD_NONE;
    // (Not with "Don't Auto-Equip New Swords and Shields": they are Termina's own, not in use, MergeShipTerminaEquip.c.)
    const bool autoEquip = !CVarGetInteger("gEnhancements.MergeShip.NoAutoEquipGear", 1);
    if (autoEquip && (sword < EQUIP_VALUE_SWORD_DIETY) && (bestSword > sword)) {
        static const u8 kSwordItems[] = { ITEM_NONE, ITEM_SWORD_KOKIRI, ITEM_SWORD_RAZOR, ITEM_SWORD_GILDED };
        SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, bestSword);
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = kSwordItems[bestSword];
        if (bestSword == EQUIP_VALUE_SWORD_RAZOR) {
            player.swordHealth = (shared->razorSwordHealth != 0) ? shared->razorSwordHealth : 100;
        }
    }
    const s32 shield = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    // (Combined, Hyrule's Hylian / Mirror Shield gives MM's Hero's / Mirror Shield.)
    const u8 shields = MsShared_CombineShields(shared->shields, combined ? 1 : 0);
    if (autoEquip && splitModel && (shields & Bit(MS_SHIELD_MM_MIRROR)) && (shield < EQUIP_VALUE_SHIELD_MIRROR)) {
        SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_MIRROR);
    } else if (autoEquip && splitModel && (shields & Bit(MS_SHIELD_HERO)) && (shield == EQUIP_VALUE_SHIELD_NONE)) {
        SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_HERO);
    }
    // Termina Equipment: the swords and shields Termina holds, and the sword and shield it had in use -- unless the
    // lines above just equipped one Hyrule added since (MergeShipTerminaEquip.h).
    MergeShip_TerminaEquip_ApplyShared(shared);

    // Songs, remains, owls: added. Stray fairies and trade slots: set.
    for (const SongMap& map : kSongs) {
        if (hold && ((map.quest == QUEST_SONG_TIME) || (map.quest == QUEST_SONG_HEALING))) {
            continue;
        }
        if (shared->songs & Bit(map.song)) {
            SET_QUEST_ITEM(map.quest);
        }
    }
    for (s32 remains = 0; remains < 4; remains++) {
        if (shared->remains & Bit(remains)) {
            SET_QUEST_ITEM(QUEST_REMAINS_ODOLWA + remains);
        }
    }
    player.owlActivationFlags |= shared->owlActivationFlags;
    if (shared->strayFairies[0]) {
        SET_WEEKEVENTREG(WEEKEVENTREG_08_80);
    }
    for (s32 area = 0; area < 4; area++) {
        inventory.strayFairies[area] = (s8)shared->strayFairies[1 + area];
    }
    inventory.items[SLOT_TRADE_DEED] = shared->mmTradeDeed;
    inventory.items[SLOT_TRADE_KEY_MAMA] = shared->mmTradeKeyMama;
    inventory.items[SLOT_TRADE_COUPLE] = shared->mmTradeCouple;
    // Every trade item owned in either world: a randomizer file's obtained flags, which its trade slots cycle through
    // (Rando/MiscBehavior/KaleidoItemPage.cpp).
    if (IS_RANDO) {
        for (s32 i = 0; i <= ITEM_PENDANT_OF_MEMORIES - ITEM_MOONS_TEAR; i++) {
            if (shared->mmTradeOwned & (1u << i)) {
                Flags_SetRandoInf(RANDO_INF_OBTAINED_MOONS_TEAR + i);
            }
        }
    }

    // Hyrule's songs, quest status, items and equipment: Termina's Hyrule views.
    ApplyHyrule(shared);
    // BUG-0120: the sword Hyrule left on B, last (once Hyrule's swords are Termina's to wield).
    MergeShip_TerminaEquip_ApplyBSword(shared);

    MergeShipHostTest_Log("SHARED apply gen=%u from=%d hp=%d/%d rupees=%d masks=0x%X/0x%08X items=0x%08X songs=0x%08X%s",
                          shared->generation, shared->lastWriter, player.health, player.healthCapacity, player.rupees,
                          shared->transformationMasks, shared->mmMasks, shared->items, shared->songs,
                          hold ? " (first cycle: MM's ocarina, Song of Time/Healing and transformation masks held back)"
                               : "");
    MergeShipHostTest_Log("SHARED apply hyrule quest=0x%06X oot items=0x%08X seeds=%d bag=%d oot equip=0x%08X "
                          "strength=%d scale=%d",
                          MergeShip_Hyrule_GetQuestItems(), OoTItems().owned, OoTItems().seeds, OoTItems().bulletBag,
                          OoTEquip().owned, OoTEquip().strength, OoTEquip().scale);
    MergeShipHostTest_Log("SHARED apply gear model=%d mmSword=%d mmShield=%d B=0x%02X ocarina=0x%02X bow=0x%02X "
                          "mmHookshot=0x%02X ootHookshot=%d ootSword=%d ootShield=%d magic=%d/%d amount=%d",
                          shared->itemsModel, GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD),
                          GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD), BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B),
                          INV_CONTENT(ITEM_OCARINA_OF_TIME), INV_CONTENT(ITEM_BOW), INV_CONTENT(ITEM_HOOKSHOT),
                          MergeShip_OoTItems_HookshotLevel(), OoTEquip().sword, OoTEquip().shield,
                          player.isMagicAcquired ? 1 : 0, player.isDoubleMagicAcquired ? 1 : 0, player.magic);
    // Each game's counts: the slots take the copies in use's from the shared save (MergeShipOwnership.h).
    MergeShip_Own_Unbind();
}

extern "C" int MergeShipShared_ApplyHeldBack(const MsSharedSave* shared) {
    if ((shared == nullptr) || (shared->lastWriter == 0)) {
        return 0;
    }
    int given = 0;
    // The ocarina is each game's own (the user's rule) unless the seed combines them.
    if (shared->itemsCombined && (shared->items & (Bit(MS_ITEM_OCARINA_OF_TIME) | Bit(MS_ITEM_OCARINA_FAIRY))) &&
        (INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_NONE)) {
        INV_CONTENT(ITEM_OCARINA_OF_TIME) = ITEM_OCARINA_OF_TIME;
        given++;
    }
    for (s32 form = 0; form < 4; form++) {
        if ((shared->transformationMasks & Bit(form)) &&
            (INV_CONTENT(kTransformationMasks[form]) != kTransformationMasks[form])) {
            INV_CONTENT(kTransformationMasks[form]) = kTransformationMasks[form];
            given++;
        }
    }
    if ((shared->songs & Bit(MS_SONG_TIME)) && !CHECK_QUEST_ITEM(QUEST_SONG_TIME)) {
        SET_QUEST_ITEM(QUEST_SONG_TIME);
        given++;
    }
    if ((shared->songs & Bit(MS_SONG_HEALING)) && !CHECK_QUEST_ITEM(QUEST_SONG_HEALING)) {
        SET_QUEST_ITEM(QUEST_SONG_HEALING);
        given++;
    }
    MergeShipHostTest_Log("SHARED held back given: %d (first cycle over) masks=0x%X songs=0x%08X", given,
                          shared->transformationMasks, shared->songs);
    return given;
}

// MM's first cycle with a song Hyrule carried: the Happy Mask Salesman heals the cursed Deku Link only while he does not
// know the Song of Healing (En_Osn: the ocarina and !QUEST_SONG_HEALING), and that cutscene is the only giver of the
// Deku Mask outside a randomizer. Hyrule can carry the Song of Healing (SoH knows MM's songs), so a Termina file of a
// Hyrule file keeps him willing while Link is a Deku without the Deku Mask: he teaches it again (a song already known
// stays known) and gives the mask, as in MM. Nothing is given that MM's script does not give.
// A Hyrule magic arrow a button got before the Items (Hyrule) page put it on the bow (2026-09-30: the bare arrow, with
// the arrow cell's slot, and MM's bare-arrow icon): the bow with that arrow and the bow cell's slot, as the page equips
// it now. Run on every load of the button's icon, so a file saved before is right from its first frame.
static void MigrateOoTArrowButton(u8* item, u8* slot) {
    static const u8 kArrowCells[3] = { MS_OOT_SLOT_ARROW_FIRE, MS_OOT_SLOT_ARROW_ICE, MS_OOT_SLOT_ARROW_LIGHT };

    for (s32 i = 0; i < 3; i++) {
        if ((*item == (u8)(ITEM_ARROW_FIRE + i)) && (*slot == (u8)MS_OOT_BUTTON_SLOT(kArrowCells[i]))) {
            *item = (u8)(ITEM_BOW_FIRE + i);
            *slot = (u8)MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOW);
            MergeShipHostTest_Log("ARROWBUTTON Hyrule's arrow %d moved onto its bow", i);
        }
    }
}

void RegisterMergeShipFirstCycleHooks() {
    // The door's Termina file: Link is never Majora's Mask's cursed Deku (the user, 2026-10-10: "they turned into deku
    // link. make sure that doesnt happen"). He came from Hyrule as himself; a Deku Link without the Deku Mask can only
    // be the curse, which is lifted before the player is made (an Anchor teammate's dummy, spawned once the player
    // exists, keeps its own form).
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorInit>(
        ACTOR_PLAYER, [](Actor* actor, bool* should) {
            if ((gPlayState == nullptr) || (gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first != actor) ||
                (MergeShipHost_SharedSave() == nullptr) || (gSaveContext.save.playerForm != PLAYER_FORM_DEKU) ||
                (INV_CONTENT(ITEM_MASK_DEKU) == ITEM_MASK_DEKU)) {
                return;
            }
            gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
            MergeShipHostTest_Log("FORM no Deku Mask: Link is himself, not the cursed Deku");
        });
    REGISTER_VB_SHOULD(VB_OSN_CONSIDER_ELIGIBLE_FOR_SONG_OF_HEALING, {
        if (!*should && !IS_RANDO && (MergeShipHost_SharedSave() != nullptr) &&
            (gSaveContext.save.saveInfo.inventory.items[SLOT_OCARINA] != ITEM_NONE) &&
            (INV_CONTENT(ITEM_MASK_DEKU) != ITEM_MASK_DEKU) && (gSaveContext.save.playerForm == PLAYER_FORM_DEKU)) {
            *should = true;
            MergeShipHostTest_Log("OSN the Salesman heals the Deku Link who knows the Song of Healing already");
        }
    });
    // Hyrule's Hookshot (the Items (Hyrule) hookshot cell at level 1, only ever carried by the door) on a C button /
    // the D-pad shows Ocarina of Time's Hookshot icon, as the pause page does (gItemIcons[ITEM_LONGSHOT] is the
    // Longshot's).
    REGISTER_VB_SHOULD(VB_INTERFACE_LOAD_ITEM_ICON, {
        const u8 btn = (u8)va_arg(args, int);
        if ((gPlayState != nullptr) && (GET_CUR_FORM_BTN_ITEM(btn) == ITEM_LONGSHOT) &&
            (MergeShip_OoTItems_HookshotLevel() == 1)) {
            gPlayState->interfaceCtx.iconItemSegment[btn] = (char*)MergeShip_OoTItems_IconTexture(ITEM_LONGSHOT);
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_INTERFACE_LOAD_DPAD_ITEM_ICON, {
        const u8 btn = (u8)va_arg(args, int);
        if ((gPlayState != nullptr) && (DPAD_GET_CUR_FORM_BTN_ITEM(btn) == ITEM_LONGSHOT) &&
            (MergeShip_OoTItems_HookshotLevel() == 1)) {
            gPlayState->interfaceCtx.iconItemSegment[DPAD_BUTTON(btn) + EQUIP_SLOT_MAX] =
                (char*)MergeShip_OoTItems_IconTexture(ITEM_LONGSHOT);
            *should = false;
        }
    });
    // The shared bow slot (ITEM_BOW, MergeShipOwnership.h) on a C button / the D-pad shows Ocarina of Time's Fairy
    // Bow icon, as the Items (Hyrule) page does, when that button holds the Fairy Bow -- its own slot says which bow it
    // is (MergeShip_Own_ButtonBowIsOoT), not whether Termina also owns MM's Hero's Bow (the reported bug, 2026-09-30:
    // "Fairy bow icon still switches to Hero's bow").
    REGISTER_VB_SHOULD(VB_INTERFACE_LOAD_ITEM_ICON, {
        const u8 btn = (u8)va_arg(args, int);
        if ((gPlayState != nullptr) && (GET_CUR_FORM_BTN_ITEM(btn) == ITEM_BOW) &&
            MergeShip_Own_ButtonBowIsOoT(GET_CUR_FORM_BTN_SLOT(btn))) {
            gPlayState->interfaceCtx.iconItemSegment[btn] = (char*)MergeShip_OoTItems_IconTexture(ITEM_BOW);
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_INTERFACE_LOAD_DPAD_ITEM_ICON, {
        const u8 btn = (u8)va_arg(args, int);
        if ((gPlayState != nullptr) && (DPAD_GET_CUR_FORM_BTN_ITEM(btn) == ITEM_BOW) &&
            MergeShip_Own_ButtonBowIsOoT(DPAD_GET_CUR_FORM_BTN_SLOT(btn))) {
            gPlayState->interfaceCtx.iconItemSegment[DPAD_BUTTON(btn) + EQUIP_SLOT_MAX] =
                (char*)MergeShip_OoTItems_IconTexture(ITEM_BOW);
            *should = false;
        }
    });
    // A Hyrule magic arrow equipped onto Hyrule's Fairy Bow (the Items (Hyrule) page, as Ocarina of Time equips it:
    // MM's ITEM_BOW_FIRE .. with the bow cell's slot) shows Ocarina of Time's bow-with-arrow icon on its C button / the
    // D-pad (OoT gItemIconBowFireTex ..), as every load of the button's icon does -- the pause equip, a scene or file
    // load, the door -- not MM's (the reported bug, 2026-09-30: "OOT fire arrows dont equip onto the bow normally").
    REGISTER_VB_SHOULD(VB_INTERFACE_LOAD_ITEM_ICON, {
        const u8 btn = (u8)va_arg(args, int);
        if (btn != EQUIP_SLOT_B) {
            MigrateOoTArrowButton(&BUTTON_ITEM_EQUIP(0, btn), &C_SLOT_EQUIP(0, btn));
        }
        const char* icon = (gPlayState != nullptr) ? MergeShip_OoTItems_BowArrowButtonIcon(
                                                         GET_CUR_FORM_BTN_ITEM(btn), GET_CUR_FORM_BTN_SLOT(btn))
                                                   : nullptr;
        if (icon != nullptr) {
            gPlayState->interfaceCtx.iconItemSegment[btn] = (char*)icon;
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_INTERFACE_LOAD_DPAD_ITEM_ICON, {
        const u8 btn = (u8)va_arg(args, int);
        MigrateOoTArrowButton(&DPAD_GET_CUR_FORM_BTN_ITEM(btn), &DPAD_GET_CUR_FORM_BTN_SLOT(btn));
        const char* icon = (gPlayState != nullptr)
                               ? MergeShip_OoTItems_BowArrowButtonIcon(DPAD_GET_CUR_FORM_BTN_ITEM(btn),
                                                                       DPAD_GET_CUR_FORM_BTN_SLOT(btn))
                               : nullptr;
        if (icon != nullptr) {
            gPlayState->interfaceCtx.iconItemSegment[DPAD_BUTTON(btn) + EQUIP_SLOT_MAX] = (char*)icon;
            *should = false;
        }
    });
}
static RegisterShipInitFunc sMergeShipFirstCycleInit(RegisterMergeShipFirstCycleHooks, {});

extern "C" u8 MergeShipShared_ItemFromBottle(u8 bottle) {
    return ItemFromBottle(bottle);
}

extern "C" u8 MergeShipShared_BottleFromItem(u8 item) {
    return BottleFromItem(item);
}

#else // 2ship.exe: no shared save (MergeShipOwnership.h's callers check MergeShipHost_IsHosted first)

#include <stdint.h>

extern "C" uint8_t MergeShipShared_ItemFromBottle(uint8_t) {
    return 0xFF; // ITEM_NONE
}

extern "C" uint8_t MergeShipShared_BottleFromItem(uint8_t) {
    return 0; // MS_BOTTLE_NONE
}

#endif // MERGESHIP_HOSTED

// BUG-0101: Termina takes the buttons Hyrule left from MergeShip_Own_Sync, once its slots hold the items (the apply runs
// as the file boots, before Hyrule's bombs .. bow are in MM's slots).
extern "C" void MergeShipShared_ApplyButtons(void) {
    const MsSharedSave* shared = static_cast<const MsSharedSave*>(MergeShipHost_SharedSave());

    if ((shared != nullptr) && (shared->buttonsWriter == MS_ENGINE_SOH) && (gPlayState != nullptr)) {
        ApplyButtons(shared);
        for (s32 btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
            Interface_LoadItemIconImpl(gPlayState, btn);
        }
        // The D-pad's icons load through their own path (z_parameter.c): without it they stayed blank.
        for (s32 btn = EQUIP_SLOT_D_RIGHT; btn <= EQUIP_SLOT_D_UP; btn++) {
            if (DPAD_BUTTON_ITEM_EQUIP(0, btn) != ITEM_NONE) {
                Interface_Dpad_LoadItemIconImpl(gPlayState, (u8)btn);
            }
        }
    }
}
