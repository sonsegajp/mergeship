// MergeShip2 -- Ship of Harkinian's projector for the host's shared save (MergeShipSharedSave.h, SharedSave.h).

// Only in soh.dll (MERGESHIP_HOSTED): soh.exe has no shared save.
#ifdef MERGESHIP_HOSTED

#include "soh/MergeShip/MergeShipSharedSave.h"
#include <libultraship/bridge/consolevariablebridge.h> // CVarGetInteger (BUG-0101)
#include <vector>

#include <mergeship/EngineApi.h>
#include <mergeship/SharedSave.h>
#include "soh/MergeShip/MergeShipGoal.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipMMEquipment.h"
#include "soh/MergeShip/MergeShipMMItems.h"
#include "soh/MergeShip/MergeShipSongs.h"
#include "soh/MergeShip/MergeShipOwnership.h"
#include "soh/MergeShip/MergeShipDekuBeans.h" // the beans Termina shares (BUG-0014)
#include "soh/MergeShip/MergeShipChateau.h"   // Chateau Romani's endless magic (BUG-0153)
void MergeShipTest_Log(const char* fmt, ...);
extern u8 gEquipAgeReqs[][4]; // z_kaleido_scope_PAL.c (BUG-0120)
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
        case ITEM_FISH: return MS_BOTTLE_FISH;
        case ITEM_MILK_BOTTLE: return MS_BOTTLE_MILK;
        case ITEM_MILK_HALF: return MS_BOTTLE_MILK_HALF;
        case ITEM_LETTER_RUTO: return MS_BOTTLE_RUTOS_LETTER;
        case ITEM_BLUE_FIRE: return MS_BOTTLE_BLUE_FIRE;
        case ITEM_BUG: return MS_BOTTLE_BUG;
        case ITEM_BIG_POE: return MS_BOTTLE_BIG_POE;
        case ITEM_POE: return MS_BOTTLE_POE;
        case ITEM_MM_CHATEAU: return MS_BOTTLE_CHATEAU_ROMANI;
        case ITEM_MM_DEKU_PRINCESS: return MS_BOTTLE_DEKU_PRINCESS;
        case ITEM_MM_ZORA_EGG: return MS_BOTTLE_ZORA_EGG;
        case ITEM_MM_GOLD_DUST: return MS_BOTTLE_GOLD_DUST;
        case ITEM_MM_MUSHROOM: return MS_BOTTLE_MUSHROOM;
        case ITEM_MM_SEAHORSE: return MS_BOTTLE_SEAHORSE;
        case ITEM_MM_HYLIAN_LOACH: return MS_BOTTLE_HYLIAN_LOACH;
        case ITEM_MM_SPRING_WATER: return MS_BOTTLE_SPRING_WATER;
        case ITEM_MM_HOT_SPRING_WATER: return MS_BOTTLE_HOT_SPRING_WATER;
        default: return MS_BOTTLE_NONE;
    }
}

// ITEM_NONE: not something a Hyrule bottle can hold (Termina's Mystery Milk): the slot is left alone.
u8 ItemFromBottle(u8 bottle) {
    switch (bottle) {
        case MS_BOTTLE_EMPTY: return ITEM_BOTTLE;
        case MS_BOTTLE_RED_POTION: return ITEM_POTION_RED;
        case MS_BOTTLE_GREEN_POTION: return ITEM_POTION_GREEN;
        case MS_BOTTLE_BLUE_POTION: return ITEM_POTION_BLUE;
        case MS_BOTTLE_FAIRY: return ITEM_FAIRY;
        case MS_BOTTLE_FISH: return ITEM_FISH;
        case MS_BOTTLE_MILK: return ITEM_MILK_BOTTLE;
        case MS_BOTTLE_MILK_HALF: return ITEM_MILK_HALF;
        case MS_BOTTLE_RUTOS_LETTER: return ITEM_LETTER_RUTO;
        case MS_BOTTLE_BLUE_FIRE: return ITEM_BLUE_FIRE;
        case MS_BOTTLE_BUG: return ITEM_BUG;
        case MS_BOTTLE_BIG_POE: return ITEM_BIG_POE;
        case MS_BOTTLE_POE: return ITEM_POE;
        case MS_BOTTLE_CHATEAU_ROMANI: return ITEM_MM_CHATEAU;
        case MS_BOTTLE_DEKU_PRINCESS: return ITEM_MM_DEKU_PRINCESS;
        case MS_BOTTLE_ZORA_EGG: return ITEM_MM_ZORA_EGG;
        case MS_BOTTLE_GOLD_DUST: return ITEM_MM_GOLD_DUST;
        case MS_BOTTLE_MUSHROOM: return ITEM_MM_MUSHROOM;
        case MS_BOTTLE_SEAHORSE: return ITEM_MM_SEAHORSE;
        case MS_BOTTLE_HYLIAN_LOACH: return ITEM_MM_HYLIAN_LOACH;
        case MS_BOTTLE_SPRING_WATER: return ITEM_MM_SPRING_WATER;
        case MS_BOTTLE_HOT_SPRING_WATER: return ITEM_MM_HOT_SPRING_WATER;
        default: return ITEM_NONE;
    }
}

// A slot's item changed under the C buttons that point at it (bottles): show the new one.
void RefreshButtons(s32 slot, u8 item) {
    for (s32 button = 1; button < ARRAY_COUNT(gSaveContext.equips.buttonItems); button++) {
        if (gSaveContext.equips.cButtonSlots[button - 1] == slot) {
            gSaveContext.equips.buttonItems[button] = item;
        }
    }
}

// ---- songs ----------------------------------------------------------------------------------------------------

struct SongMap {
    u8 song;   // MsSong
    s8 quest;  // Ocarina of Time QUEST_SONG_*, or -1
    s8 mmSong; // MERGESHIP_SONG_* - MERGESHIP_SONG_FIRST (bit of gMergeShipSave.mmSongs), or -1
};
const SongMap kSongs[] = {
    { MS_SONG_TIME, QUEST_SONG_TIME, -1 },
    { MS_SONG_EPONA, QUEST_SONG_EPONA, -1 },
    { MS_SONG_STORMS, QUEST_SONG_STORMS, -1 },
    { MS_SONG_MINUET, QUEST_SONG_MINUET, -1 },
    { MS_SONG_BOLERO, QUEST_SONG_BOLERO, -1 },
    { MS_SONG_SERENADE, QUEST_SONG_SERENADE, -1 },
    { MS_SONG_REQUIEM, QUEST_SONG_REQUIEM, -1 },
    { MS_SONG_NOCTURNE, QUEST_SONG_NOCTURNE, -1 },
    { MS_SONG_PRELUDE, QUEST_SONG_PRELUDE, -1 },
    { MS_SONG_ZELDAS_LULLABY, QUEST_SONG_LULLABY, -1 },
    { MS_SONG_SARIA, QUEST_SONG_SARIA, -1 },
    { MS_SONG_SUN, QUEST_SONG_SUN, -1 },
    { MS_SONG_SONATA, -1, MERGESHIP_SONG_SONATA - MERGESHIP_SONG_FIRST },
    { MS_SONG_GORON_LULLABY, -1, MERGESHIP_SONG_GORON_LULLABY - MERGESHIP_SONG_FIRST },
    { MS_SONG_GORON_LULLABY_INTRO, -1, MERGESHIP_SONG_GORON_LULLABY_INTRO - MERGESHIP_SONG_FIRST },
    { MS_SONG_NEW_WAVE, -1, MERGESHIP_SONG_NEW_WAVE - MERGESHIP_SONG_FIRST },
    { MS_SONG_ELEGY, -1, MERGESHIP_SONG_ELEGY - MERGESHIP_SONG_FIRST },
    { MS_SONG_OATH, -1, MERGESHIP_SONG_OATH - MERGESHIP_SONG_FIRST },
    { MS_SONG_HEALING, -1, MERGESHIP_SONG_HEALING - MERGESHIP_SONG_FIRST },
    { MS_SONG_SOARING, -1, MERGESHIP_SONG_SOARING - MERGESHIP_SONG_FIRST },
    { MS_SONG_INVERTED_TIME, -1, MERGESHIP_SONG_INVERTED_TIME - MERGESHIP_SONG_FIRST },
    { MS_SONG_DOUBLE_TIME, -1, MERGESHIP_SONG_DOUBLE_TIME - MERGESHIP_SONG_FIRST },
};

// ---- small helpers --------------------------------------------------------------------------------------------

bool Has(u8 item) {
    return INV_CONTENT(item) == item;
}

// BUG-0105 (2026-10-12, "no shared ammo period"): counts are each game's own. The counted items this apply gives Hyrule
// for the first time arrive full, as a found one does (FillNewSlotItems, once their bags are raised).
std::vector<u8> sNewSlotItems;
void RaiseUpgrade(s32 upgrade, u8 level);

void GiveSlotItem(u8 item) {
    if (INV_CONTENT(item) == ITEM_NONE) {
        INV_CONTENT(item) = item;
        sNewSlotItems.push_back(item);
    }
}

void FillNewSlotItems() {
    for (u8 item : sNewSlotItems) {
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
            case ITEM_STICK:
                RaiseUpgrade(UPG_STICKS, 1);
                AMMO(ITEM_STICK) = (s8)CUR_CAPACITY(UPG_STICKS);
                break;
            case ITEM_NUT:
                RaiseUpgrade(UPG_NUTS, 1);
                AMMO(ITEM_NUT) = (s8)CUR_CAPACITY(UPG_NUTS);
                break;
            case ITEM_BEAN:
                AMMO(ITEM_BEAN) = 10;
                break;
            case ITEM_SLINGSHOT:
                RaiseUpgrade(UPG_BULLET_BAG, 1);
                AMMO(ITEM_SLINGSHOT) = (s8)CUR_CAPACITY(UPG_BULLET_BAG);
                break;
            default:
                break;
        }
    }
    sNewSlotItems.clear();
}

u8 ClampAmmo(u8 count, s32 upgrade) {
    const s32 capacity = CUR_CAPACITY(upgrade);
    return (count > capacity) ? (u8)capacity : count;
}

void RaiseUpgrade(s32 upgrade, u8 level) {
    if (CUR_UPG_VALUE(upgrade) < level) {
        Inventory_ChangeUpgrade(upgrade, level);
    }
}

// Termina's trade items: SoH's ITEM_MM_MOONS_TEAR .. ITEM_MM_PENDANT_OF_MEMORIES are MM's ITEM_MOONS_TEAR (0x28) ..
// ITEM_PENDANT_OF_MEMORIES (0x30) in the same order; the shared save holds MM's ids.
constexpr u8 kMMItemMoonsTear = 0x28;
u8 TradeToShared(u8 item) {
    return MERGESHIP_ITEM_IS_MM_TRADE(item) ? (u8)(kMMItemMoonsTear + (item - ITEM_MM_MOONS_TEAR)) : (u8)0xFF;
}
u8 TradeFromShared(u8 mmItem) {
    return ((mmItem >= kMMItemMoonsTear) && (mmItem <= kMMItemMoonsTear + (ITEM_MM_PENDANT_OF_MEMORIES - ITEM_MM_MOONS_TEAR)))
               ? (u8)(ITEM_MM_MOONS_TEAR + (mmItem - kMMItemMoonsTear))
               : (u8)ITEM_NONE;
}

bool OwnsEquip(s32 type, s32 inv) {
    return CHECK_OWNED_EQUIP(type, inv) != 0;
}

// ---- Hyrule's masks and trade items (MsHyruleItem: Ocarina of Time's ItemId order without SOLD OUT) ----------------

constexpr s32 kHyruleAdultFirst = MS_HYRULE_POCKET_EGG;

// The MsHyruleItem of an item in a trade slot, or -1.
s32 HyruleItemOf(u8 item) {
    if ((item >= ITEM_WEIRD_EGG) && (item <= ITEM_MASK_TRUTH)) {
        return MS_HYRULE_WEIRD_EGG + (item - ITEM_WEIRD_EGG);
    }
    if ((item >= ITEM_POCKET_EGG) && (item <= ITEM_CLAIM_CHECK)) {
        return kHyruleAdultFirst + (item - ITEM_POCKET_EGG);
    }
    return -1;
}

u8 ItemOfHyrule(s32 hyruleItem) {
    return (hyruleItem < kHyruleAdultFirst) ? (u8)(ITEM_WEIRD_EGG + hyruleItem)
                                            : (u8)(ITEM_POCKET_EGG + (hyruleItem - kHyruleAdultFirst));
}

// The randomizer's "has" flag of a trade item (ShuffleTradeItems.c; the same order as MsHyruleItem).
RandomizerInf HyruleItemRandoInf(s32 hyruleItem) {
    return (hyruleItem < kHyruleAdultFirst)
               ? (RandomizerInf)(RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG + hyruleItem)
               : (RandomizerInf)(RAND_INF_ADULT_TRADES_HAS_POCKET_EGG + (hyruleItem - kHyruleAdultFirst));
}

u8 TradeSlotToShared(u8 item) {
    const s32 hyruleItem = HyruleItemOf(item);
    if (hyruleItem >= 0) {
        return (u8)hyruleItem;
    }
    return (item == ITEM_SOLD_OUT) ? (u8)MS_HYRULE_SLOT_SOLD_OUT : (u8)MS_HYRULE_SLOT_EMPTY;
}

// What Hyrule owns of its masks and trade items: what the trade slots hold, the randomizer's trade flags, and the
// masks the Happy Mask Shop lends (OoT: each sold mask can be borrowed again; after the Bunny Hood, all of them).
u32 CaptureHyruleItems() {
    u32 owned = 0;
    for (s32 i = 0; i < MS_HYRULE_ITEM_COUNT; i++) {
        if (Flags_GetRandomizerInf(HyruleItemRandoInf(i))) {
            owned |= Bit(i);
        }
    }
    const s32 child = HyruleItemOf(INV_CONTENT(ITEM_TRADE_CHILD));
    const s32 adult = HyruleItemOf(INV_CONTENT(ITEM_TRADE_ADULT));
    owned |= (child >= 0) ? Bit(child) : 0;
    owned |= (adult >= 0) ? Bit(adult) : 0;
    owned |= Flags_GetItemGetInf(ITEMGETINF_38) ? Bit(MS_HYRULE_MASK_KEATON) : 0;
    owned |= Flags_GetItemGetInf(ITEMGETINF_39) ? Bit(MS_HYRULE_MASK_SKULL) : 0;
    owned |= Flags_GetItemGetInf(ITEMGETINF_3A) ? Bit(MS_HYRULE_MASK_SPOOKY) : 0;
    if (Flags_GetItemGetInf(ITEMGETINF_3B)) {
        owned |= Bit(MS_HYRULE_MASK_BUNNY) | Bit(MS_HYRULE_MASK_GORON) | Bit(MS_HYRULE_MASK_ZORA) |
                 Bit(MS_HYRULE_MASK_GERUDO) | Bit(MS_HYRULE_MASK_TRUTH);
    }
    return owned;
}

// Hyrule's Ocarina of Time items Termina can also hold (MsItem, 2Ship MergeShipOoTItems.h): added back.
struct SlotItemMap {
    u8 bit;  // MsItem
    u8 item; // SoH ItemId (its own slot)
};
const SlotItemMap kOoTSlotItems[] = {
    { MS_ITEM_SLINGSHOT, ITEM_SLINGSHOT },       { MS_ITEM_BOOMERANG, ITEM_BOOMERANG },
    { MS_ITEM_MEGATON_HAMMER, ITEM_HAMMER },     { MS_ITEM_DINS_FIRE, ITEM_DINS_FIRE },
    { MS_ITEM_FARORES_WIND, ITEM_FARORES_WIND }, { MS_ITEM_NAYRUS_LOVE, ITEM_NAYRUS_LOVE },
};

// The tunics and boots Termina's Equipment view can hold (the shared save's ootOwnedEquipment bits).
struct EquipInvMap {
    s32 type;
    s32 inv;
};
const EquipInvMap kTunicsBoots[] = {
    { EQUIP_TYPE_TUNIC, EQUIP_INV_TUNIC_GORON },
    { EQUIP_TYPE_TUNIC, EQUIP_INV_TUNIC_ZORA },
    { EQUIP_TYPE_BOOTS, EQUIP_INV_BOOTS_IRON },
    { EQUIP_TYPE_BOOTS, EQUIP_INV_BOOTS_HOVER },
};

// A level of Ocarina of Time's inventory.upgrades word (gUpgradeShifts / gUpgradeMasks).
u8 UpgradeIn(u32 upgrades, s32 upgrade) {
    return (u8)((upgrades & gUpgradeMasks[upgrade]) >> gUpgradeShifts[upgrade]);
}

} // namespace

// ---- BUG-0101: the C and D-pad buttons carried between the worlds ---------------------------------------------------

// Hyrule's equips.buttonItems index of each MsSharedSave.buttonItems entry (C-left .. C-right, D-pad up, down, left,
// right -- the DpadEquips order).
const u8 kButtonIndex[MS_SHARED_BUTTONS] = { 1, 2, 3, 4, 5, 6, 7 };

u8 ButtonItemOf(u8 item) {
    switch (item) {
        case ITEM_STICK:
            return MS_BTN_STICK;
        case ITEM_NUT:
            return MS_BTN_NUT;
        case ITEM_BOMB:
            return MS_BTN_BOMB;
        case ITEM_BOW:
        case ITEM_MM_BOW_HERO:
            return MS_BTN_BOW;
        case ITEM_ARROW_FIRE:
        case ITEM_BOW_ARROW_FIRE:
            return MS_BTN_ARROW_FIRE;
        case ITEM_ARROW_ICE:
        case ITEM_BOW_ARROW_ICE:
            return MS_BTN_ARROW_ICE;
        case ITEM_ARROW_LIGHT:
        case ITEM_BOW_ARROW_LIGHT:
            return MS_BTN_ARROW_LIGHT;
        case ITEM_SLINGSHOT:
            return MS_BTN_SLINGSHOT;
        case ITEM_OCARINA_FAIRY:
        case ITEM_OCARINA_TIME:
            return MS_BTN_OCARINA;
        case ITEM_BOMBCHU:
            return MS_BTN_BOMBCHU;
        case ITEM_HOOKSHOT:
        case ITEM_LONGSHOT:
        case ITEM_MM_HOOKSHOT:
            return MS_BTN_HOOKSHOT;
        case ITEM_LENS:
            return MS_BTN_LENS;
        case ITEM_BEAN:
            return MS_BTN_BEAN;
        case ITEM_HAMMER:
            return MS_BTN_HAMMER;
        case ITEM_BOOMERANG:
            return MS_BTN_BOOMERANG;
        case ITEM_DINS_FIRE:
            return MS_BTN_DINS_FIRE;
        case ITEM_FARORES_WIND:
            return MS_BTN_FARORES_WIND;
        case ITEM_NAYRUS_LOVE:
            return MS_BTN_NAYRUS_LOVE;
        case ITEM_MM_MASK_DEKU:
            return MS_BTN_MM_MASK_FIRST + 0;
        case ITEM_MM_MASK_GORON:
            return MS_BTN_MM_MASK_FIRST + 1;
        case ITEM_MM_MASK_ZORA:
            return MS_BTN_MM_MASK_FIRST + 2;
        case ITEM_MM_MASK_FIERCE_DEITY:
            return MS_BTN_MM_MASK_FIRST + 3;
        default:
            // MM's Mask of Truth .. Giant's Mask are MM's ItemId order from MM's fifth mask on.
            if ((item >= ITEM_MM_MASK_TRUTH) && (item <= ITEM_MM_MASK_GIANT)) {
                return (u8)(MS_BTN_MM_MASK_FIRST + 4 + (item - ITEM_MM_MASK_TRUTH));
            }
            return MS_BTN_NONE;
    }
}

// Hyrule's item and button slot for a carried button item, or false when Hyrule has none of it.
bool ButtonItemHere(u8 btnItem, u8* item, u8* slot) {
    u8 base = ITEM_NONE;

    switch (btnItem) {
        case MS_BTN_STICK: base = ITEM_STICK; break;
        case MS_BTN_NUT: base = ITEM_NUT; break;
        case MS_BTN_BOMB: base = ITEM_BOMB; break;
        case MS_BTN_BOW: base = ITEM_BOW; break;
        case MS_BTN_SLINGSHOT: base = ITEM_SLINGSHOT; break;
        case MS_BTN_OCARINA: base = ITEM_OCARINA_FAIRY; break;
        case MS_BTN_BOMBCHU: base = ITEM_BOMBCHU; break;
        case MS_BTN_HOOKSHOT: base = ITEM_HOOKSHOT; break;
        case MS_BTN_LENS: base = ITEM_LENS; break;
        case MS_BTN_BEAN: base = ITEM_BEAN; break;
        case MS_BTN_HAMMER: base = ITEM_HAMMER; break;
        case MS_BTN_BOOMERANG: base = ITEM_BOOMERANG; break;
        case MS_BTN_DINS_FIRE: base = ITEM_DINS_FIRE; break;
        case MS_BTN_FARORES_WIND: base = ITEM_FARORES_WIND; break;
        case MS_BTN_NAYRUS_LOVE: base = ITEM_NAYRUS_LOVE; break;
        case MS_BTN_ARROW_FIRE:
        case MS_BTN_ARROW_ICE:
        case MS_BTN_ARROW_LIGHT: {
            // The pause screen puts the bow and the arrow on the button (ITEM_BOW_ARROW_*), from the arrow's slot.
            static const u8 kArrows[3] = { ITEM_ARROW_FIRE, ITEM_ARROW_ICE, ITEM_ARROW_LIGHT };
            const u8 arrow = kArrows[btnItem - MS_BTN_ARROW_FIRE];
            if ((INV_CONTENT(arrow) != arrow) || (INV_CONTENT(ITEM_BOW) != ITEM_BOW)) {
                return false;
            }
            *item = (u8)(ITEM_BOW_ARROW_FIRE + (btnItem - MS_BTN_ARROW_FIRE));
            *slot = SLOT(arrow);
            return true;
        }
        default:
            break;
    }
    if ((btnItem == MS_BTN_BOW) && MergeShip_MMEquip_IsSelected(MMEQUIP_BOW)) {
        // BUG-0122: Termina has one bow; Hyrule's in use is MM's Hero's Bow (what was used last, or the only bow
        // owned): the button gets it back, not the Fairy Bow its slot holds -- as the Items (Termina) page equips it
        *item = ITEM_MM_BOW_HERO;
        *slot = (u8)MergeShip_MMItems_PageSlotOoTSlot(MERGESHIP_MM_SLOT_BOW);
        return true;
    }
    if ((btnItem == MS_BTN_HOOKSHOT) && MergeShip_MMEquip_IsSelected(MMEQUIP_HOOKSHOT)) {
        // BUG-0146: the same for the hookshot -- Hyrule's in use is MM's Hookshot (Items (Termina) page)
        *item = ITEM_MM_HOOKSHOT;
        *slot = (u8)MergeShip_MMItems_PageSlotOoTSlot(MERGESHIP_MM_SLOT_HOOKSHOT);
        return true;
    }
    if (base != ITEM_NONE) {
        const u8 held = INV_CONTENT(base);
        if ((held == ITEM_NONE) || (held >= ITEM_MM_MASK_FIERCE_DEITY && held <= ITEM_MM_MASK_ADULT)) {
            return false;
        }
        *item = held; // the Longshot for the hookshot, the Ocarina of Time for the ocarina, ...
        *slot = SLOT(base);
        return true;
    }
    if ((btnItem >= MS_BTN_MM_MASK_FIRST) && (btnItem <= MS_BTN_MM_MASK_LAST)) {
        static const u8 kFirstMasks[4] = { ITEM_MM_MASK_DEKU, ITEM_MM_MASK_GORON, ITEM_MM_MASK_ZORA,
                                           ITEM_MM_MASK_FIERCE_DEITY };
        const s32 index = btnItem - MS_BTN_MM_MASK_FIRST;
        const u8 mask = (index < 4) ? kFirstMasks[index] : (u8)(ITEM_MM_MASK_TRUTH + (index - 4));
        if (!MergeShip_HasMMMaskItem(mask)) {
            return false;
        }
        *item = mask;
        *slot = SLOT_NONE; // as the Masks page equips one (z_kaleido_mask.c)
        return true;
    }
    return false;
}

// ---- BUG-0120: the sword on B carried between the worlds ----------------------------------------------------------

u8 BSwordOf(u8 item) {
    switch (item) {
        case ITEM_SWORD_KOKIRI:
            return MS_BSWORD_OOT_KOKIRI;
        case ITEM_SWORD_MASTER:
            return MS_BSWORD_OOT_MASTER;
        case ITEM_SWORD_BGS:
        case ITEM_SWORD_KNIFE:
            return MS_BSWORD_OOT_BIGGORON;
        case ITEM_MM_SWORD_KOKIRI:
            return MS_BSWORD_MM_KOKIRI;
        case ITEM_MM_SWORD_RAZOR:
            return MS_BSWORD_MM_RAZOR;
        case ITEM_MM_SWORD_GILDED:
            return MS_BSWORD_MM_GILDED;
        case ITEM_MM_SWORD_GREAT_FAIRY:
            return MS_BSWORD_MM_GREAT_FAIRY;
        default:
            return MS_BSWORD_NONE;
    }
}

void CaptureBSword(MsSharedSave* shared) {
    // (an MM form's B is its own: the human's waits in gMergeShipSave, MergeShip_SwapFormEquips)
    const u8 b = MERGESHIP_FORM_IS_MM(MergeShip_GetPlayerForm()) ? gMergeShipSave.bButtonItem[MM_FORM_HUMAN]
                                                                 : gSaveContext.equips.buttonItems[0];

    shared->bSword = BSwordOf(b);
    shared->bWriter = MS_ENGINE_SOH;
}

// Termina's sword on B, if Hyrule holds it and Link can wield it at his age: as the Equipment page's A equips it (its
// Termina view for MM's swords).
void ApplyBSword(const MsSharedSave* shared) {
    if (shared->bWriter != MS_ENGINE_2SHIP) {
        return;
    }
    const_cast<MsSharedSave*>(shared)->bWriter = 0; // once
    if (MERGESHIP_FORM_IS_MM(MergeShip_GetPlayerForm())) {
        return; // (an MM form's B is its own)
    }
    static const u8 kOoT[3][2] = { { EQUIP_VALUE_SWORD_KOKIRI, ITEM_SWORD_KOKIRI },
                                   { EQUIP_VALUE_SWORD_MASTER, ITEM_SWORD_MASTER },
                                   { EQUIP_VALUE_SWORD_BIGGORON, ITEM_SWORD_BGS } };
    static const u8 kMM[4] = { ITEM_MM_SWORD_KOKIRI, ITEM_MM_SWORD_RAZOR, ITEM_MM_SWORD_GILDED,
                               ITEM_MM_SWORD_GREAT_FAIRY };
    const u8 code = shared->bSword;

    if ((code >= MS_BSWORD_OOT_KOKIRI) && (code <= MS_BSWORD_OOT_BIGGORON)) {
        const u8 value = kOoT[code - MS_BSWORD_OOT_KOKIRI][0];
        u8 item = kOoT[code - MS_BSWORD_OOT_KOKIRI][1];
        const u8 req = gEquipAgeReqs[EQUIP_TYPE_SWORD][value];

        if (!CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, value - 1) ||
            ((req != 9) && (req != gSaveContext.linkAge) && !CVarGetInteger(CVAR_CHEAT("TimelessEquipment"), 0))) {
            return;
        }
        if ((value == EQUIP_VALUE_SWORD_BIGGORON) && !gSaveContext.bgsFlag) {
            item = ITEM_SWORD_KNIFE;
        }
        MergeShip_MMEquip_PauseSelectOoT(EQUIP_TYPE_SWORD, value); // (its slot back from an MM variant)
        Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, value);
        gSaveContext.equips.buttonItems[0] = item;
        Flags_UnsetInfTable(INFTABLE_SWORDLESS);
    } else if ((code >= MS_BSWORD_MM_KOKIRI) && (code <= MS_BSWORD_MM_GREAT_FAIRY)) {
        MergeShip_MMEquip_TerminaEquip(nullptr, kMM[code - MS_BSWORD_MM_KOKIRI]);
    }
    MergeShipTest_Log("BSWORD from Termina: code %d -> B %02X", code, gSaveContext.equips.buttonItems[0]);
}

void CaptureButtons(MsSharedSave* shared) {
    for (s32 i = 0; i < MS_SHARED_BUTTONS; i++) {
        const u8 index = kButtonIndex[i];
        const u8 slot = gSaveContext.equips.cButtonSlots[index - 1];
        // A bottle's content is the bottle's, not an item the other world has.
        const bool bottle = (slot >= SLOT_BOTTLE_1) && (slot <= SLOT_BOTTLE_4);
        shared->buttonItems[i] = bottle ? (u8)MS_BTN_NONE : ButtonItemOf(gSaveContext.equips.buttonItems[index]);
    }
    shared->buttonsWriter = MS_ENGINE_SOH;
}

void ApplyButtons(const MsSharedSave* shared) {
    if (shared->buttonsWriter != MS_ENGINE_2SHIP) {
        return;
    }
    const s32 count = CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) ? MS_SHARED_BUTTONS : 3;
    // BUG-0146: what each of Hyrule's buttons held before (an item of a kind Termina names on a button keeps Hyrule's
    // own of that kind: MM's Hookshot, not Ocarina of Time's in its slot; the Hero's Bow; the Longshot).
    u8 hyruleItems[8];
    u8 hyruleSlots[8];
    for (s32 other = 1; other <= 7; other++) {
        hyruleItems[other] = gSaveContext.equips.buttonItems[other];
        hyruleSlots[other] = gSaveContext.equips.cButtonSlots[other - 1];
    }
    for (s32 i = 0; i < count; i++) {
        u8 item;
        u8 slot;
        if (shared->buttonItems[i] == MS_BTN_NONE) {
            continue; // nothing carried: the button keeps what Hyrule had there
        }
        bool hyrules = false;
        for (s32 other = 1; other <= 7; other++) {
            if ((hyruleItems[other] != ITEM_NONE) && (ButtonItemOf(hyruleItems[other]) == shared->buttonItems[i])) {
                item = hyruleItems[other];
                slot = hyruleSlots[other];
                hyrules = true;
                break;
            }
        }
        if (!hyrules && !ButtonItemHere(shared->buttonItems[i], &item, &slot)) {
            continue; // Hyrule has none of it: the button keeps what Hyrule had there
        }
        const u8 index = kButtonIndex[i];
        // The item leaves any other button of Hyrule's (one item, one button).
        for (s32 other = 1; other <= 7; other++) {
            if ((other != index) && (ButtonItemOf(gSaveContext.equips.buttonItems[other]) == shared->buttonItems[i])) {
                gSaveContext.equips.buttonItems[other] = ITEM_NONE;
                gSaveContext.equips.cButtonSlots[other - 1] = SLOT_NONE;
            }
        }
        gSaveContext.equips.buttonItems[index] = item;
        gSaveContext.equips.cButtonSlots[index - 1] = slot;
    }
    const_cast<MsSharedSave*>(shared)->buttonsWriter = 0; // once (the shared save the host holds is the player's)
    MergeShipTest_Log("BUTTONS from Termina: C %02X %02X %02X D %02X %02X %02X %02X", gSaveContext.equips.buttonItems[1],
                      gSaveContext.equips.buttonItems[2], gSaveContext.equips.buttonItems[3],
                      gSaveContext.equips.buttonItems[4], gSaveContext.equips.buttonItems[5],
                      gSaveContext.equips.buttonItems[6], gSaveContext.equips.buttonItems[7]);
}

extern "C" void MergeShipShared_Capture(MsSharedSave* shared) {
    if (shared == nullptr) {
        return;
    }
    // Life, magic, money.
    shared->healthCapacity = (u16)gSaveContext.healthCapacity;
    shared->health = gSaveContext.health;
    shared->heartPieces = (u8)((gSaveContext.inventory.questItems & 0xF0000000) >> 28);
    shared->doubleDefense = gSaveContext.isDoubleDefenseAcquired ? 1 : 0;
    shared->magicLevel = gSaveContext.isMagicAcquired ? (gSaveContext.isDoubleMagicAcquired ? 2 : 1) : 0;
    shared->magicAmount = (u8)gSaveContext.magic;
    // BUG-0153: Chateau Romani's endless magic, one status for both games
    shared->chateauMagic = MS_CHATEAU_VALID | (MergeShip_Chateau_Drunk() ? MS_CHATEAU_ACTIVE : 0);
    shared->rupees = (u16)gSaveContext.rupees;
    // The bigger wallet either game has found (as 2Ship's capture keeps it): Hyrule's own smaller one must not shrink a
    // wallet Termina found, or the rupees it holds are cut to the smaller one's size (BUG-0030).
    if ((u8)CUR_UPG_VALUE(UPG_WALLET) > shared->walletLevel) {
        shared->walletLevel = (u8)CUR_UPG_VALUE(UPG_WALLET);
    }

    // Majora's Mask's masks.
    shared->transformationMasks = gMergeShipSave.ownedMasks & 0x0F;
    shared->mmMasks = gMergeShipSave.ownedMMMasks & 0x001FFFFE;

    // Items.
    u32 items = 0;
    const u8 ocarina = INV_CONTENT(ITEM_OCARINA_FAIRY);
    items |= (ocarina == ITEM_OCARINA_FAIRY) ? Bit(MS_ITEM_OCARINA_FAIRY) : 0;
    items |= (ocarina == ITEM_OCARINA_TIME) ? Bit(MS_ITEM_OCARINA_OF_TIME) : 0;
    // Each game's version of an item both games have is its own (SharedSave.h itemsModel; the user's rule): the Fairy
    // Bow and MM's Hero's Bow, Hyrule's Hookshot / Longshot and MM's Hookshot (a slot held only for MM's variant is
    // not Hyrule's item, MergeShipMMEquipment.h).
    items |= MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_BOW) ? Bit(MS_ITEM_BOW) : 0;
    items |= MergeShip_MMEquip_Owns(MMEQUIP_BOW) ? Bit(MS_ITEM_MM_BOW) : 0;
    items |= Has(ITEM_ARROW_FIRE) ? Bit(MS_ITEM_ARROW_FIRE) : 0;
    items |= Has(ITEM_ARROW_ICE) ? Bit(MS_ITEM_ARROW_ICE) : 0;
    items |= Has(ITEM_ARROW_LIGHT) ? Bit(MS_ITEM_ARROW_LIGHT) : 0;
    const bool ootHookshot = MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_HOOKSHOT);
    items |= ootHookshot ? Bit(MS_ITEM_OOT_HOOKSHOT) : 0;
    items |= (ootHookshot && (INV_CONTENT(ITEM_HOOKSHOT) == ITEM_LONGSHOT)) ? Bit(MS_ITEM_LONGSHOT) : 0;
    items |= MergeShip_MMEquip_Owns(MMEQUIP_HOOKSHOT) ? Bit(MS_ITEM_HOOKSHOT) : 0;
    items |= Has(ITEM_BOMB) ? Bit(MS_ITEM_BOMB_BAG) : 0;
    items |= Has(ITEM_BOMBCHU) ? Bit(MS_ITEM_BOMBCHU) : 0;
    items |= Has(ITEM_STICK) ? Bit(MS_ITEM_DEKU_STICK) : 0;
    items |= Has(ITEM_NUT) ? Bit(MS_ITEM_DEKU_NUT) : 0;
    items |= Has(ITEM_BEAN) ? Bit(MS_ITEM_MAGIC_BEANS) : 0;
    items |= Has(ITEM_LENS) ? Bit(MS_ITEM_LENS_OF_TRUTH) : 0;
    items |= (gMergeShipMMItemsSave.powderKeg != ITEM_NONE) ? Bit(MS_ITEM_POWDER_KEG) : 0;
    items |= (gMergeShipMMItemsSave.pictographBox != ITEM_NONE) ? Bit(MS_ITEM_PICTOGRAPH_BOX) : 0;
    items |= MergeShip_MMEquip_Owns(MMEQUIP_GREAT_FAIRY) ? Bit(MS_ITEM_GREAT_FAIRYS_SWORD) : 0;
    items |= (gMergeShipSave.mmQuestItems & MERGESHIP_QUEST_BOMBERS_NOTEBOOK) ? Bit(MS_ITEM_BOMBERS_NOTEBOOK) : 0;
    items |= Has(ITEM_SLINGSHOT) ? Bit(MS_ITEM_SLINGSHOT) : 0;
    items |= Has(ITEM_BOOMERANG) ? Bit(MS_ITEM_BOOMERANG) : 0;
    items |= Has(ITEM_DINS_FIRE) ? Bit(MS_ITEM_DINS_FIRE) : 0;
    items |= Has(ITEM_FARORES_WIND) ? Bit(MS_ITEM_FARORES_WIND) : 0;
    items |= Has(ITEM_NAYRUS_LOVE) ? Bit(MS_ITEM_NAYRUS_LOVE) : 0;
    items |= Has(ITEM_HAMMER) ? Bit(MS_ITEM_MEGATON_HAMMER) : 0;
    // MM's own arrows and Lens of Truth (itemsModel 2): Hyrule cannot hold them; they stay Termina's.
    shared->items = items | (shared->items & MS_ITEMS_TERMINA_VERSION_BITS);

    shared->quiverLevel = (u8)CUR_UPG_VALUE(UPG_QUIVER);
    shared->bombBagLevel = (u8)CUR_UPG_VALUE(UPG_BOMB_BAG);
    shared->stickLevel = (u8)CUR_UPG_VALUE(UPG_STICKS);
    shared->nutLevel = (u8)CUR_UPG_VALUE(UPG_NUTS);
    shared->arrows = Has(ITEM_BOW) ? (u8)AMMO(ITEM_BOW) : 0;
    shared->bombs = Has(ITEM_BOMB) ? (u8)AMMO(ITEM_BOMB) : 0;
    shared->bombchus = Has(ITEM_BOMBCHU) ? (u8)AMMO(ITEM_BOMBCHU) : 0;
    shared->sticks = Has(ITEM_STICK) ? (u8)AMMO(ITEM_STICK) : 0;
    shared->nuts = Has(ITEM_NUT) ? (u8)AMMO(ITEM_NUT) : 0;
    shared->magicBeans = Has(ITEM_BEAN) ? (u8)AMMO(ITEM_BEAN) : 0;
    // BUG-0129: Hyrule's beans, for Termina to plant (its slot counts these while it has none of its own)
    shared->hyruleBeans = Has(ITEM_BEAN) ? (u8)AMMO(ITEM_BEAN) : 0;
    shared->hyruleBeansWriter = MS_ENGINE_SOH;
    shared->powderKegs = gMergeShipMMItemsSave.powderKegAmmo;

    // Each game's bottles are its own (MergeShipOwnership.h): Hyrule's four are Ocarina of Time's (ootBottles);
    // Majora's Mask's six are the shared save's own `bottles`, which Hyrule holds there.
    if (!MergeShip_Goal_CombineItemsInGame()) {
        for (s32 i = 0; i < 4; i++) {
            shared->ootBottles[i] = BottleFromItem(gSaveContext.inventory.items[SLOT_BOTTLE_1 + i]);
        }
    }
    // Combined: Hyrule has four bottles, the first four shared ones. Termina's fifth and sixth are left as they are,
    // and so is anything a Hyrule bottle cannot hold (it was never given to one).
    for (s32 i = 0; (i < 4) && MergeShip_Goal_CombineItemsInGame(); i++) {
        const u8 current = shared->bottles[i];
        if ((current != MS_BOTTLE_NONE) && (ItemFromBottle(current) == ITEM_NONE)) {
            continue;
        }
        shared->bottles[i] = BottleFromItem(gSaveContext.inventory.items[SLOT_BOTTLE_1 + i]);
    }

    // Swords and shields (an Ocarina of Time slot held only for its Majora's Mask variant is not the item itself).
    u8 swords = 0;
    swords |= (OwnsEquip(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI) &&
               MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_KOKIRI_SWORD))
                  ? Bit(MS_SWORD_KOKIRI)
                  : 0;
    swords |= MergeShip_MMEquip_Owns(MMEQUIP_KOKIRI) ? Bit(MS_SWORD_MM_KOKIRI) : 0; // MM's own Kokiri Sword
    swords |= MergeShip_MMEquip_Owns(MMEQUIP_RAZOR) ? Bit(MS_SWORD_RAZOR) : 0;
    swords |= MergeShip_MMEquip_Owns(MMEQUIP_GILDED) ? Bit(MS_SWORD_GILDED) : 0;
    swords |= OwnsEquip(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER) ? Bit(MS_SWORD_MASTER) : 0;
    const bool bgsSlot = OwnsEquip(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON) &&
                         MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_BIGGORON);
    swords |= (bgsSlot && gSaveContext.bgsFlag) ? Bit(MS_SWORD_BIGGORON) : 0;
    swords |= ((bgsSlot && !gSaveContext.bgsFlag) || OwnsEquip(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE))
                  ? Bit(MS_SWORD_GIANTS_KNIFE)
                  : 0;
    shared->swords = swords;
    // Shields: each game's is its own (Hyrule's Mirror Shield never gives Termina MM's, nor MM's Hyrule's).
    u8 shields = 0;
    shields |= OwnsEquip(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_DEKU) ? Bit(MS_SHIELD_DEKU) : 0;
    shields |= MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_HYLIAN) ? Bit(MS_SHIELD_HYLIAN) : 0;
    shields |= MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_MIRROR) ? Bit(MS_SHIELD_MIRROR) : 0;
    shields |= MergeShip_MMEquip_Owns(MMEQUIP_HERO_SHIELD) ? Bit(MS_SHIELD_HERO) : 0;
    shields |= MergeShip_MMEquip_Owns(MMEQUIP_MIRROR_SHIELD) ? Bit(MS_SHIELD_MM_MIRROR) : 0;
    shared->shields = shields;
    shared->razorSwordHealth = gMergeShipSave.mmRazorHealth;

    // Songs, remains, owls, stray fairies, Termina's trade slots.
    u32 songs = 0;
    for (const SongMap& map : kSongs) {
        const bool known = (map.quest >= 0) ? (CHECK_QUEST_ITEM(map.quest) != 0)
                                            : ((gMergeShipSave.mmSongs & (1u << map.mmSong)) != 0);
        songs |= known ? Bit(map.song) : 0;
    }
    shared->songs = songs;
    shared->remains = gMergeShipSave.mmQuestItems & 0x0F;
    shared->owlActivationFlags = gMergeShipSave.owlActivationFlags;
    for (s32 area = 0; area < 5; area++) {
        shared->strayFairies[area] = gMergeShipSave.strayFairies[area];
    }
    shared->mmTradeDeed = TradeToShared(gMergeShipMMItemsSave.tradeDeed);
    shared->mmTradeKeyMama = TradeToShared(gMergeShipMMItemsSave.tradeKeyMama);
    shared->mmTradeCouple = TradeToShared(gMergeShipMMItemsSave.tradeCouple);
    // Every MM trade item Hyrule holds (the slots cycle through them, 2026-09-24): set.
    shared->mmTradeOwned = gMergeShipMMItemsSave.tradeOwned;

    // Hyrule's equipped gear, carried for whoever plays Link in Termina.
    shared->ootAge = (u8)gSaveContext.linkAge;
    shared->ootGearWriter = MS_ENGINE_SOH;
    for (s32 i = 0; i < 8; i++) {
        shared->ootButtonItems[i] = gSaveContext.equips.buttonItems[i];
    }
    for (s32 i = 0; i < 7; i++) {
        shared->ootCButtonSlots[i] = gSaveContext.equips.cButtonSlots[i];
    }
    shared->ootEquipment = gSaveContext.equips.equipment;
    shared->ootOwnedEquipment = gSaveContext.inventory.equipment;
    shared->ootUpgrades = gSaveContext.inventory.upgrades;

    // Hyrule's quest status, masks, trade items and Deku Seeds (the Hyrule block; songs are above).
    shared->hyruleQuest = gSaveContext.inventory.questItems & MS_HYRULE_QUEST_BITS;
    shared->gsTokens = gSaveContext.inventory.gsTokens;
    shared->hyruleItems = (shared->hyruleItems & MS_HYRULE_MASK_BITS) | CaptureHyruleItems();
    shared->hyruleTradeChild = TradeSlotToShared(INV_CONTENT(ITEM_TRADE_CHILD));
    shared->hyruleTradeAdult = TradeSlotToShared(INV_CONTENT(ITEM_TRADE_ADULT));
    shared->hyruleBlock |= MS_HYRULE_BLOCK_SOH;
    // Hyrule's Roc's Feather and Deku Beans, which Termina uses too, and the beans' counts (BUG-0014).
    shared->hyruleExtras =
        MS_HYRULE_EXTRA_VALID |
        ((IS_RANDO && Flags_GetRandomizerInf(RAND_INF_OBTAINED_ROCS_FEATHER)) ? MS_HYRULE_EXTRA_ROCS_FEATHER : 0) |
        (MergeShip_DekuBeans_CanSelect() ? MS_HYRULE_EXTRA_DEKU_BEANS : 0) |
        (MergeShip_DekuBeans_GoldOwned() ? MS_HYRULE_EXTRA_GOLD_BEANS : 0) |
        (MergeShip_DekuBeans_AmmoMode() ? MS_HYRULE_EXTRA_BEAN_AMMO : 0);

    // The unified randomizer's goal: its configuration and Hyrule's part (MergeShipGoal.h)
    MergeShip_Goal_Capture(shared);

    shared->itemsModel = MS_SHARED_ITEMS_MODEL_SPLIT_ALL;
    CaptureButtons(shared); // BUG-0101
    CaptureBSword(shared);  // BUG-0120
    shared->generation++;
    shared->lastWriter = MS_ENGINE_SOH;
    MergeShipTest_Log("SHARED capture gen=%u hp=%d/%d rupees=%d masks=0x%X/0x%08X items=0x%08X songs=0x%08X",
                      shared->generation, shared->health, shared->healthCapacity, shared->rupees,
                      shared->transformationMasks, shared->mmMasks, shared->items, shared->songs);
    MergeShipTest_Log("SHARED capture hyrule quest=0x%06X items=0x%06X trade=%d/%d seeds=%d tokens=%d swords=0x%02X "
                      "shields=0x%02X equipment=0x%04X upgrades=0x%06X",
                      shared->hyruleQuest, shared->hyruleItems, shared->hyruleTradeChild, shared->hyruleTradeAdult,
                      shared->hyruleSeeds, shared->gsTokens, shared->swords, shared->shields,
                      shared->ootOwnedEquipment, shared->ootUpgrades);
}

extern "C" void MergeShipShared_Apply(const MsSharedSave* shared) {
    if ((shared == nullptr) || (shared->lastWriter == 0)) {
        return; // nothing captured yet: this file's own values stand
    }
    sNewSlotItems.clear();

    // Life, magic, money: set. Every 4 pieces of heart are a container, as the 4th piece's textbox makes them
    // (z_message_PAL.c; Anchor's GIVE_ITEM does the same): a 4th captured before that textbox closed (a team state
    // merged meanwhile, Anchor) became 0 pieces and no container when masked to 0..3 -- a heart lost for good, its
    // piece collected.
    const s32 containers = shared->heartPieces / 4;
    s16 capacity = (s16)shared->healthCapacity;
    if (capacity < 0x30) {
        capacity = gSaveContext.healthCapacity; // not captured yet: this file's own stands
    }
    capacity = (s16)(capacity + 0x10 * containers);
    if (capacity > MS_SHARED_MAX_HEALTH_CAPACITY) {
        capacity = MS_SHARED_MAX_HEALTH_CAPACITY;
    }
    if (capacity >= 0x30) {
        gSaveContext.healthCapacity = capacity;
    }
    gSaveContext.health = (s16)(((shared->health > 0) ? shared->health : 0x30) + 0x10 * containers);
    if (gSaveContext.health > gSaveContext.healthCapacity) {
        gSaveContext.health = gSaveContext.healthCapacity;
    }
    gSaveContext.inventory.questItems =
        (gSaveContext.inventory.questItems & 0x0FFFFFFF) | ((u32)(shared->heartPieces % 4) << 28);
    if (shared->doubleDefense) {
        gSaveContext.isDoubleDefenseAcquired = true;
        gSaveContext.inventory.defenseHearts = 20;
    }
    const s32 magicLevel = gSaveContext.isMagicAcquired ? (gSaveContext.isDoubleMagicAcquired ? 2 : 1) : 0;
    if (shared->magicLevel > magicLevel) {
        gSaveContext.isMagicAcquired = true;
        gSaveContext.isDoubleMagicAcquired = (shared->magicLevel >= 2);
        gSaveContext.magicLevel = 0; // the HUD steps the meter up to the new size, as when a file is loaded
    }
    // The amount only fills a meter Link owns: it never makes one.
    gSaveContext.magic = gSaveContext.isMagicAcquired ? (s8)shared->magicAmount : 0;
    if (shared->chateauMagic & MS_CHATEAU_VALID) { // BUG-0153
        MergeShip_Chateau_SetDrunk((shared->chateauMagic & MS_CHATEAU_ACTIVE) != 0);
    }
    RaiseUpgrade(UPG_WALLET, shared->walletLevel);
    gSaveContext.rupees = (s16)((shared->rupees > CUR_CAPACITY(UPG_WALLET)) ? CUR_CAPACITY(UPG_WALLET) : shared->rupees);

    // Masks: added.
    gMergeShipSave.ownedMasks |= shared->transformationMasks & 0x0F;
    gMergeShipSave.ownedMMMasks |= shared->mmMasks & 0x001FFFFE;

    // Items: gained ones added, counts set. Each game's version of an item both games have is its own (SharedSave.h
    // itemsModel): an MM item gives Hyrule MM's (MergeShipMMEquipment.h), never its Ocarina of Time counterpart. A
    // shared save of the old model named either game's with one bit: those bits are left alone until it is rewritten.
    // The arrows, the Lens, the bows, the hookshots, the shields and the Kokiri Swords (SharedSave.h MsShared_Combine*):
    // Hyrule's own bits give Hyrule's; MM's give Hyrule nothing (the user, 2026-09-24: "when its off, all oot and all mm
    // items NEED to be separate"), unless the unified seed combines them (RSK_MS_COMBINE_SHARED_ITEMS): then either
    // game's gives both.
    const bool combineSetting = MergeShip_Goal_CombineItemsInGame();
    u32 items = MsShared_CombineVersions(shared->items, combineSetting ? 1 : 0);
    const u8 shields = MsShared_CombineShields(shared->shields, combineSetting ? 1 : 0);
    const u8 swords = MsShared_CombineSwords(shared->swords, combineSetting ? 1 : 0);
    // MM's own items Hyrule can hold are MM's here, on the MM menu and usable (the user, 2026-09-24: "you can use ANY mm
    // item and OOT item when you get them"), never their Ocarina of Time counterpart unless combined: MM's Hero's Bow
    // shoots in Hyrule as itself (MMEQUIP_BOW).
    const bool splitModel = shared->itemsModel >= MS_SHARED_ITEMS_MODEL_SPLIT;
    // The user's rule (2026-09-24, "all items except masks"): what both games have -- the ocarina, bombs, bombchus,
    // sticks, nuts, beans, their bags and quivers, the ammo and the bottles -- is each game's own, as the bows, the
    // magic arrows and the Lens are: Termina's never fill Hyrule's slots, unless the seed combines them.
    const bool bothGames = combineSetting;
    if (bothGames && (items & Bit(MS_ITEM_OCARINA_OF_TIME))) {
        INV_CONTENT(ITEM_OCARINA_FAIRY) = ITEM_OCARINA_TIME;
    } else if (bothGames && (items & Bit(MS_ITEM_OCARINA_FAIRY))) {
        GiveSlotItem(ITEM_OCARINA_FAIRY);
    }
    if (splitModel && (items & Bit(MS_ITEM_BOW)) && !MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_BOW)) {
        GiveSlotItem(ITEM_BOW); // the Fairy Bow (the slot may be held for MM's Hero's Bow: it is Hyrule's now too)
        MergeShip_MMEquip_OnOoTItemGiven(ITEM_BOW);
    }
    if (splitModel && (items & Bit(MS_ITEM_MM_BOW)) && !MergeShip_MMEquip_Owns(MMEQUIP_BOW)) {
        MergeShip_MMEquip_Give(nullptr, ITEM_MM_BOW_HERO); // MM's Hero's Bow, found in Termina
    }
    if (items & Bit(MS_ITEM_ARROW_FIRE)) {
        GiveSlotItem(ITEM_ARROW_FIRE);
    }
    if (items & Bit(MS_ITEM_ARROW_ICE)) {
        GiveSlotItem(ITEM_ARROW_ICE);
    }
    if (items & Bit(MS_ITEM_ARROW_LIGHT)) {
        GiveSlotItem(ITEM_ARROW_LIGHT);
    }
    // Hyrule's Hookshot / Longshot, and MM's Hookshot as MM's (combined, MM's gives Hyrule's Hookshot too).
    if (items & Bit(MS_ITEM_LONGSHOT)) {
        INV_CONTENT(ITEM_HOOKSHOT) = ITEM_LONGSHOT;
        MergeShip_MMEquip_OnOoTItemGiven(ITEM_LONGSHOT);
    } else if (splitModel && (items & Bit(MS_ITEM_OOT_HOOKSHOT)) &&
               !MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_HOOKSHOT)) {
        GiveSlotItem(ITEM_HOOKSHOT);
        MergeShip_MMEquip_OnOoTItemGiven(ITEM_HOOKSHOT);
    }
    if (splitModel && (items & Bit(MS_ITEM_HOOKSHOT)) && !MergeShip_MMEquip_Owns(MMEQUIP_HOOKSHOT)) {
        MergeShip_MMEquip_Give(nullptr, ITEM_MM_HOOKSHOT);
    }
    // Bombs need a bomb bag: a shared save written while Termina held bombs without one (an older build) does not
    // give them here.
    if (bothGames && (items & Bit(MS_ITEM_BOMB_BAG)) &&
        ((shared->bombBagLevel > 0) || (CUR_UPG_VALUE(UPG_BOMB_BAG) > 0))) {
        GiveSlotItem(ITEM_BOMB);
    }
    if (bothGames && (items & Bit(MS_ITEM_BOMBCHU))) {
        GiveSlotItem(ITEM_BOMBCHU);
    }
    if (bothGames && (items & Bit(MS_ITEM_DEKU_STICK))) {
        GiveSlotItem(ITEM_STICK);
    }
    if (bothGames && (items & Bit(MS_ITEM_DEKU_NUT))) {
        GiveSlotItem(ITEM_NUT);
    }
    if (bothGames && (items & Bit(MS_ITEM_MAGIC_BEANS))) {
        GiveSlotItem(ITEM_BEAN);
    }
    if (items & Bit(MS_ITEM_LENS_OF_TRUTH)) {
        GiveSlotItem(ITEM_LENS);
    }
    // Hyrule's own items, which Termina can hold too (found there in a unified seed): added.
    for (const SlotItemMap& map : kOoTSlotItems) {
        if (items & Bit(map.bit)) {
            GiveSlotItem(map.item);
        }
    }
    if (Has(ITEM_SLINGSHOT) && (CUR_UPG_VALUE(UPG_BULLET_BAG) == 0)) {
        Inventory_ChangeUpgrade(UPG_BULLET_BAG, 1);
    }
    if ((items & Bit(MS_ITEM_GREAT_FAIRYS_SWORD)) && !MergeShip_MMEquip_Owns(MMEQUIP_GREAT_FAIRY)) {
        MergeShip_MMEquip_Give(nullptr, ITEM_MM_SWORD_GREAT_FAIRY);
    }
    if (items & Bit(MS_ITEM_BOMBERS_NOTEBOOK)) {
        gMergeShipSave.mmQuestItems |= MERGESHIP_QUEST_BOMBERS_NOTEBOOK;
    }
    // Termina's Powder Keg and Pictograph Box: set (the keg comes and goes with Termina's days).
    // Its count is Hyrule's own (BUG-0105): a keg new here arrives as one.
    const bool hadKeg = gMergeShipMMItemsSave.powderKeg != ITEM_NONE;
    gMergeShipMMItemsSave.powderKeg = (items & Bit(MS_ITEM_POWDER_KEG)) ? ITEM_MM_POWDER_KEG : ITEM_NONE;
    gMergeShipMMItemsSave.powderKegAmmo =
        !(items & Bit(MS_ITEM_POWDER_KEG)) ? 0 : hadKeg ? gMergeShipMMItemsSave.powderKegAmmo : 1;
    if (items & Bit(MS_ITEM_PICTOGRAPH_BOX)) {
        gMergeShipMMItemsSave.pictographBox = ITEM_MM_PICTOGRAPH_BOX;
    }

    // Hyrule's own bags and quivers stay Hyrule's (its file holds them); combined, the last capture's are both games'.
    // The counts are Hyrule's own either way (BUG-0105, 2026-10-12: "no shared ammo period"); an item first given here
    // arrives full.
    if (bothGames) {
        RaiseUpgrade(UPG_QUIVER, shared->quiverLevel);
        RaiseUpgrade(UPG_BOMB_BAG, shared->bombBagLevel);
        RaiseUpgrade(UPG_STICKS, shared->stickLevel);
        RaiseUpgrade(UPG_NUTS, shared->nutLevel);
    }
    RaiseUpgrade(UPG_BULLET_BAG, UpgradeIn(shared->ootUpgrades, UPG_BULLET_BAG));
    FillNewSlotItems();

    // Bottles: a bottle is never taken away; what is in it is set. Each game's bottles are its own unless combined:
    // Ocarina of Time's four (ootBottles, Termina may have found or used one) are Hyrule's slots; Majora's Mask's are
    // the shared save's, read live (MergeShipOwnership.h).
    for (s32 i = 0; (i < 4) && !bothGames; i++) {
        const u8 item = ItemFromBottle(shared->ootBottles[i]);
        if ((item != ITEM_NONE) && (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] != item)) {
            gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = item;
            RefreshButtons(SLOT_BOTTLE_1 + i, item);
        }
    }
    for (s32 i = 0; (i < 4) && bothGames; i++) {
        const u8 item = ItemFromBottle(shared->bottles[i]);
        if ((item != ITEM_NONE) && (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] != item)) {
            gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = item;
            RefreshButtons(SLOT_BOTTLE_1 + i, item);
        }
    }

    // Swords and shields: added (Termina's as Majora's Mask's equipment, on the MM menu and usable by either age).
    if ((swords & Bit(MS_SWORD_KOKIRI)) && !OwnsEquip(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);
        MergeShip_MMEquip_OnOoTItemGiven(ITEM_SWORD_KOKIRI);
    }
    // MM's own Kokiri Sword: MM's equipment (its model), never Ocarina of Time's Kokiri Sword (2026-09-24).
    if ((swords & Bit(MS_SWORD_MM_KOKIRI)) && !MergeShip_MMEquip_Owns(MMEQUIP_KOKIRI)) {
        MergeShip_MMEquip_Give(nullptr, ITEM_MM_SWORD_KOKIRI);
    }
    if ((shared->swords & Bit(MS_SWORD_RAZOR)) && !MergeShip_MMEquip_Owns(MMEQUIP_RAZOR)) {
        MergeShip_MMEquip_Give(nullptr, ITEM_MM_SWORD_RAZOR);
    }
    if ((shared->swords & Bit(MS_SWORD_GILDED)) && !MergeShip_MMEquip_Owns(MMEQUIP_GILDED)) {
        MergeShip_MMEquip_Give(nullptr, ITEM_MM_SWORD_GILDED);
    }
    if (MergeShip_MMEquip_Owns(MMEQUIP_RAZOR)) {
        gMergeShipSave.mmRazorHealth = shared->razorSwordHealth;
    }
    // MM's shields (MS_SHIELD_HERO / _MM_MIRROR): MM's here too, never Hyrule's Hylian / Mirror Shield unless combined
    // (MsShared_CombineShields).
    if (splitModel && (shields & Bit(MS_SHIELD_MM_MIRROR)) &&
        !MergeShip_MMEquip_Owns(MMEQUIP_MIRROR_SHIELD)) {
        MergeShip_MMEquip_Give(nullptr, ITEM_MM_SHIELD_MIRROR);
    }
    if (splitModel && (shields & Bit(MS_SHIELD_HERO)) &&
        !MergeShip_MMEquip_Owns(MMEQUIP_HERO_SHIELD)) {
        MergeShip_MMEquip_Give(nullptr, ITEM_MM_SHIELD_HERO);
    }
    // Hyrule's own equipment Termina holds too (its Equipment view; found there in a unified seed): added.
    if ((shared->swords & Bit(MS_SWORD_MASTER)) && !OwnsEquip(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);
    }
    const bool bgsOwned =
        OwnsEquip(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON) && MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_BIGGORON);
    if ((shared->swords & (Bit(MS_SWORD_BIGGORON) | Bit(MS_SWORD_GIANTS_KNIFE))) && !bgsOwned &&
        !OwnsEquip(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
        gSaveContext.bgsFlag = (shared->swords & Bit(MS_SWORD_BIGGORON)) ? 1 : 0;
        gSaveContext.swordHealth = 8;
        MergeShip_MMEquip_OnOoTItemGiven(ITEM_SWORD_BGS);
    } else if ((shared->swords & Bit(MS_SWORD_BIGGORON)) && bgsOwned && !gSaveContext.bgsFlag) {
        gSaveContext.bgsFlag = 1; // the knife became Biggoron's Sword in Termina
        gSaveContext.swordHealth = 8;
    }
    // Hyrule's Hylian and Mirror Shields, which Termina holds too (its Equipment (Hyrule)): added. (The old model's
    // Hylian bit could be MM's Hero's Shield: then only into an empty slot, as before.)
    if ((shields & Bit(MS_SHIELD_HYLIAN)) &&
        (splitModel ? !MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_HYLIAN)
                    : (!OwnsEquip(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_HYLIAN) &&
                       !MergeShip_MMEquip_Owns(MMEQUIP_HERO_SHIELD)))) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_HYLIAN);
        MergeShip_MMEquip_OnOoTItemGiven(ITEM_SHIELD_HYLIAN);
    }
    if (splitModel && (shields & Bit(MS_SHIELD_MIRROR)) && !MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_MIRROR)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR);
        MergeShip_MMEquip_OnOoTItemGiven(ITEM_SHIELD_MIRROR);
    }
    // The Deku Shield burns: set both ways once Termina has written it (2Ship's capture sets the bit).
    if (shared->shields & Bit(MS_SHIELD_DEKU)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_DEKU);
    } else if ((shared->lastWriter == MS_ENGINE_2SHIP) && (shared->hyruleBlock & MS_HYRULE_BLOCK_2SHIP) &&
               OwnsEquip(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_DEKU)) {
        gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_DEKU);
        if (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_DEKU) {
            Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
        }
    }
    for (const EquipInvMap& map : kTunicsBoots) {
        const u16 flag = (u16)OWNED_EQUIP_FLAG(map.type, map.inv);
        if (shared->ootOwnedEquipment & flag) {
            gSaveContext.inventory.equipment |= flag;
        }
    }
    RaiseUpgrade(UPG_STRENGTH, UpgradeIn(shared->ootUpgrades, UPG_STRENGTH));
    RaiseUpgrade(UPG_SCALE, UpgradeIn(shared->ootUpgrades, UPG_SCALE));

    // Hyrule's equipped gear, if Termina changed it (a 2Ship player using Hyrule's gear: adult Link, M5).
    if (shared->ootGearWriter == MS_ENGINE_2SHIP) {
        for (s32 i = 0; i < 8; i++) {
            gSaveContext.equips.buttonItems[i] = shared->ootButtonItems[i];
        }
        for (s32 i = 0; i < 7; i++) {
            gSaveContext.equips.cButtonSlots[i] = shared->ootCButtonSlots[i];
        }
        gSaveContext.equips.equipment = shared->ootEquipment;
    }

    // Songs, remains, owls: added. Stray fairies and Termina's trade slots: set.
    for (const SongMap& map : kSongs) {
        if (!(shared->songs & Bit(map.song))) {
            continue;
        }
        if (map.quest >= 0) {
            gSaveContext.inventory.questItems |= gBitFlags[map.quest];
        } else {
            gMergeShipSave.mmSongs |= (u16)(1u << map.mmSong);
        }
    }
    gMergeShipSave.mmQuestItems |= shared->remains & 0x0F;
    gMergeShipSave.owlActivationFlags |= shared->owlActivationFlags;
    for (s32 area = 0; area < 5; area++) {
        gMergeShipSave.strayFairies[area] = shared->strayFairies[area];
    }
    gMergeShipMMItemsSave.tradeDeed = TradeFromShared(shared->mmTradeDeed);
    gMergeShipMMItemsSave.tradeKeyMama = TradeFromShared(shared->mmTradeKeyMama);
    gMergeShipMMItemsSave.tradeCouple = TradeFromShared(shared->mmTradeCouple);
    // Every MM trade item owned: set from the last capture (a file from before it: what the slots show).
    gMergeShipMMItemsSave.tradeOwned = shared->mmTradeOwned;
    for (u8 shown : { gMergeShipMMItemsSave.tradeDeed, gMergeShipMMItemsSave.tradeKeyMama,
                      gMergeShipMMItemsSave.tradeCouple }) {
        if (MERGESHIP_ITEM_IS_MM_TRADE(shown)) {
            gMergeShipMMItemsSave.tradeOwned |= MERGESHIP_MM_TRADE_BIT(shown);
        }
    }

    // Hyrule's quest status: added (medallions, Spiritual Stones, Stone of Agony, Gerudo Card).
    gSaveContext.inventory.questItems |= shared->hyruleQuest & MS_HYRULE_QUEST_BITS;
    // The Deku Beans' counts are Hyrule's own (BUG-0105): Termina plants from its own (2Ship MergeShipOoTItemsSave).
    // Masks and trade items Termina holds: an empty trade slot takes one (Hyrule's own trades stay as they are:
    // Termina cannot trade Hyrule's items). A randomizer file also gets the trade flags its slots cycle through.
    if (shared->hyruleBlock & MS_HYRULE_BLOCK_2SHIP) {
        const u32 owned = shared->hyruleItems;
        if (IS_RANDO) {
            for (s32 i = 0; i < MS_HYRULE_ITEM_COUNT; i++) {
                if (owned & Bit(i)) {
                    Flags_SetRandomizerInf(HyruleItemRandoInf(i));
                }
            }
        }
        for (s32 adult = 0; adult < 2; adult++) {
            const u8 slotItem = adult ? ITEM_TRADE_ADULT : ITEM_TRADE_CHILD;
            if (INV_CONTENT(slotItem) != ITEM_NONE) {
                continue;
            }
            const s32 first = adult ? kHyruleAdultFirst : (s32)MS_HYRULE_WEIRD_EGG;
            const s32 last = adult ? (s32)(MS_HYRULE_ITEM_COUNT - 1) : (s32)MS_HYRULE_MASK_TRUTH;
            const s32 current = adult ? shared->hyruleTradeAdult : shared->hyruleTradeChild;
            s32 pick = ((current >= first) && (current <= last) && (owned & Bit(current))) ? current : -1;
            for (s32 i = last; (pick < 0) && (i >= first); i--) {
                pick = (owned & Bit(i)) ? i : -1;
            }
            if (pick >= 0) {
                INV_CONTENT(slotItem) = ItemOfHyrule(pick);
            }
        }
    }

    MergeShipTest_Log("SHARED apply gen=%u from=%d hp=%d/%d rupees=%d masks=0x%X/0x%08X items=0x%08X songs=0x%08X",
                      shared->generation, shared->lastWriter, gSaveContext.health, gSaveContext.healthCapacity,
                      gSaveContext.rupees, gMergeShipSave.ownedMasks, gMergeShipSave.ownedMMMasks, shared->items,
                      shared->songs);
    MergeShipTest_Log("SHARED apply hyrule quest=0x%08X equipment=0x%04X upgrades=0x%06X trade=0x%02X/0x%02X",
                      gSaveContext.inventory.questItems, gSaveContext.inventory.equipment,
                      gSaveContext.inventory.upgrades, INV_CONTENT(ITEM_TRADE_CHILD), INV_CONTENT(ITEM_TRADE_ADULT));
    // Each game's counts: the slots take the copies in use's from the shared save (MergeShipOwnership.h).
    MergeShip_Own_Unbind();
    ApplyButtons(shared); // BUG-0101: last, once the items are given
    ApplyBSword(shared);  // BUG-0120
    // BUG-0129: the beans Termina planted of Hyrule's are gone from Hyrule's count, once
    if (shared->hyruleBeansWriter == MS_ENGINE_2SHIP) {
        if (Has(ITEM_BEAN)) {
            AMMO(ITEM_BEAN) = (s8)shared->hyruleBeans;
        }
        const_cast<MsSharedSave*>(shared)->hyruleBeansWriter = MS_ENGINE_SOH;
        MergeShipTest_Log("BEANS from Termina: Hyrule has %d", shared->hyruleBeans);
    }
}

extern "C" u8 MergeShipShared_ItemFromBottle(u8 bottle) {
    return ItemFromBottle(bottle);
}

extern "C" u8 MergeShipShared_BottleFromItem(u8 item) {
    return BottleFromItem(item);
}

extern "C" s32 MergeShipShared_CombinedInGame(void) {
    return MergeShip_Goal_CombineItemsInGame() ? 1 : 0;
}

#else // soh.exe: no shared save (MergeShipOwnership.h's callers check MergeShipHost_IsHosted first)

#include <stdint.h>

extern "C" uint8_t MergeShipShared_ItemFromBottle(uint8_t) {
    return 0xFF; // ITEM_NONE
}

extern "C" uint8_t MergeShipShared_BottleFromItem(uint8_t) {
    return 0; // MS_BOTTLE_NONE
}

extern "C" int32_t MergeShipShared_CombinedInGame(void) {
    return 0;
}

#endif // MERGESHIP_HOSTED
