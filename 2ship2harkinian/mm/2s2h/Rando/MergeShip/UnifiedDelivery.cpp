// MergeShip2 -- the unified randomizer's deliveries on 2Ship's side (UnifiedDelivery.h, M3).

#include "2s2h/MergeShip/MergeShipOwnership.h" // MergeShip2: each game's own items
#include "UnifiedDelivery.h"
#include "UnifiedSeed.h"
#include "OoTGiDraw.h"

#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <mergeship/MsItems.h>
#include <mergeship/MsDelivery.h>
#include <mergeship/SharedSave.h>
#include <spdlog/spdlog.h>

#include "2s2h/BenGui/Notification.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/MergeShip/MergeShipHost.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h" // MergeShipAnchor_NoteCheckStatus
#include "Rando/Rando.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "2s2h/MergeShip/MergeShipOoTItems.h"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipTerminaEquip.h"
#include "2s2h/MergeShip/MergeShipHyrule.h"
extern PlayState* gPlayState;
}

namespace {

// ---- the canonical table (include/mergeship/MsItems.def), 2Ship's view ---------------------------------------------

struct Row {
    RandoItemId ri; // native, or the proxy RI_OOT_<key>
    MsPersist persist;
    MsKind kind;
    const char* keyName;
};

// clang-format off
constexpr Row kRows[] = {
#define MS_ITEM(key, rg, ri, kind, persist, oot, mm, maxLevel, name) { ri, persist, kind, #key },
#define MS_ITEM_OOT(key, rg, kind, persist, oot, mm, maxLevel, name) { RI_OOT_##key, persist, kind, #key },
#include <mergeship/MsItems.def>
#undef MS_ITEM
#undef MS_ITEM_OOT
};
// clang-format on
static_assert(sizeof(kRows) / sizeof(kRows[0]) == MSI_MAX, "MsItems.def: one row per MsItemKey");

constexpr std::array<uint16_t, RI_MAX> BuildKeyOfRI() {
    std::array<uint16_t, RI_MAX> keys{};
    for (size_t i = 0; i < keys.size(); i++) {
        keys[i] = (uint16_t)MSI_MAX;
    }
    for (size_t i = 0; i < sizeof(kRows) / sizeof(kRows[0]); i++) {
        keys[kRows[i].ri] = (uint16_t)i;
    }
    return keys;
}
constexpr std::array<uint16_t, RI_MAX> kKeyOfRI = BuildKeyOfRI();

MsItemKey KeyOfRI(RandoItemId ri) {
    return ((ri >= 0) && (ri < RI_MAX)) ? (MsItemKey)kKeyOfRI[ri] : MSI_MAX;
}

// ---- logs -----------------------------------------------------------------------------------------------------------

void Log(const char* fmt, ...) {
    char line[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    SPDLOG_INFO("MergeShip unified: {}", line);
    MergeShipHostTest_Log("%s", line); // trace_2ship.log (host harness); no-op without a script
    MergeShipTest_Log("%s", line);     // 2Ship's own harness; no-op without a script
}

const char* WorldName(uint8_t world) {
    return (world == MS_WORLD_HYRULE) ? "hyrule" : (world == MS_WORLD_TERMINA) ? "termina" : "none";
}

// ---- what Termina holds natively for an item of Hyrule's -----------------------------------------------------------

enum class Native : u8 {
    None,       // nothing in Termina: Hyrule's alone
    ItemGive,   // Item_Give(arg): Ocarina of Time's songs, Stone of Agony, Gerudo Card (MergeShipHyrule), ammo, refills
    OoTItem,    // MergeShip_OoTItems_Give(arg): C-button items, masks, trade items, Ruto's Letter (MergeShipOoTItems)
    OoTEquip,   // MergeShip_OoTEquip_Give(arg): swords, shields, tunics, boots, strength, scales (MergeShipOoTEquip)
    Quest,      // MergeShip_Hyrule_GiveQuestItem(arg): medallions and Spiritual Stones
    NewBottle,  // a new bottle holding MM item `arg` (the first empty bottle slot of MM's six)
    OoTBottle,  // a new bottle of Hyrule's holding MM item `arg` (its four, the Items (Hyrule) page; combined: MM's)
    ChuBag,     // the seed's Bombchu Bag: Majora's Mask's bombchus (MergeShip_Own_UnlockMMChus)
    Ocarina,    // MM's Ocarina of Time (MM's one ocarina)
    StickUpg,   // UPG_DEKU_STICKS to level arg (0: the next)
    NutUpg,     // UPG_DEKU_NUTS to level arg (0: the next)
    Strength,   // the next of the Goron's Bracelet, Silver and Golden Gauntlets
    Scale,      // the next of the Silver and Golden Scales
    Slingshot,  // the Slingshot, then the next bullet bag
    BulletBag,  // bullet bag to level arg
    Arrows,     // arg arrows (with the bow)
    Seeds,      // arg Deku Seeds (with a bullet bag)
    Hookshot,   // Hyrule's next hookshot level in the Items (Hyrule) cell (Hookshot, then Longshot)
    OoTOwnBit,  // Items (Hyrule) ownership bit arg (Hyrule's magic arrows, Lens of Truth)
    OoTOcarina, // Hyrule's ocarina (arg 0 Fairy Ocarina, 1 Ocarina of Time, 2 the progressive one's next)
    OoTBag,     // Hyrule's bomb bag at level arg (0: its next)
    OoTBow,     // Hyrule's bow (arg 1: the progressive bow -- the bow, then its next quiver)
    OoTQuiver,  // Hyrule's quiver at level arg
};

struct Action {
    Native kind;
    u16 arg;
};

int CombinedItems(); // below
MsSharedSave* Shared(); // below

Action NativeAction(MsItemKey key) {
    switch (key) {
        // Ocarina of Time's equipment
        case MSI_MASTER_SWORD: return { Native::OoTEquip, ITEM_OOT_SWORD_MASTER };
        case MSI_GIANTS_KNIFE: return { Native::OoTEquip, ITEM_OOT_SWORD_GIANTS_KNIFE };
        case MSI_BIGGORON_SWORD: return { Native::OoTEquip, ITEM_OOT_SWORD_BIGGORON };
        case MSI_DEKU_SHIELD: return { Native::OoTEquip, ITEM_OOT_SHIELD_DEKU };
        case MSI_GORON_TUNIC: return { Native::OoTEquip, ITEM_OOT_TUNIC_GORON };
        case MSI_ZORA_TUNIC: return { Native::OoTEquip, ITEM_OOT_TUNIC_ZORA };
        case MSI_IRON_BOOTS: return { Native::OoTEquip, ITEM_OOT_BOOTS_IRON };
        case MSI_HOVER_BOOTS: return { Native::OoTEquip, ITEM_OOT_BOOTS_HOVER };
        case MSI_GORONS_BRACELET: return { Native::OoTEquip, ITEM_OOT_BRACELET };
        case MSI_SILVER_GAUNTLETS: return { Native::OoTEquip, ITEM_OOT_GAUNTLETS_SILVER };
        case MSI_GOLDEN_GAUNTLETS: return { Native::OoTEquip, ITEM_OOT_GAUNTLETS_GOLD };
        case MSI_PROGRESSIVE_STRENGTH: return { Native::Strength, 0 };
        case MSI_SILVER_SCALE: return { Native::OoTEquip, ITEM_OOT_SCALE_SILVER };
        case MSI_GOLDEN_SCALE: return { Native::OoTEquip, ITEM_OOT_SCALE_GOLDEN };
        case MSI_PROGRESSIVE_SCALE: return { Native::Scale, 0 };
        case MSI_BIG_BULLET_BAG: return { Native::BulletBag, 2 };
        case MSI_BIGGEST_BULLET_BAG: return { Native::BulletBag, 3 };
        // Ocarina of Time's C-button items, masks and trade items
        case MSI_FAIRY_SLINGSHOT: return { Native::OoTItem, ITEM_SLINGSHOT };
        case MSI_PROGRESSIVE_SLINGSHOT: return { Native::Slingshot, 0 };
        case MSI_LONGSHOT: return { Native::OoTItem, ITEM_LONGSHOT };
        // Hyrule's own versions of items both games have, on its own menu here (Items / Equipment (Hyrule)), never MM's
        // (the user, 2026-09-24)
        case MSI_PROGRESSIVE_HOOKSHOT:
        case MSI_HOOKSHOT: return { Native::Hookshot, 0 };
        case MSI_FIRE_ARROWS: return { Native::OoTOwnBit, MS_OOT_OWN_ARROW_FIRE };
        case MSI_ICE_ARROWS: return { Native::OoTOwnBit, MS_OOT_OWN_ARROW_ICE };
        case MSI_LIGHT_ARROWS: return { Native::OoTOwnBit, MS_OOT_OWN_ARROW_LIGHT };
        case MSI_LENS_OF_TRUTH: return { Native::OoTOwnBit, MS_OOT_OWN_LENS };
        case MSI_HYLIAN_SHIELD: return { Native::OoTEquip, ITEM_OOT_SHIELD_HYLIAN };
        case MSI_MIRROR_SHIELD: return { Native::OoTEquip, ITEM_OOT_SHIELD_MIRROR };
        case MSI_KOKIRI_SWORD: return { Native::OoTEquip, ITEM_OOT_SWORD_KOKIRI };
        case MSI_BOOMERANG: return { Native::OoTItem, ITEM_OOT_BOOMERANG };
        case MSI_MEGATON_HAMMER: return { Native::OoTItem, ITEM_OOT_HAMMER };
        case MSI_DINS_FIRE: return { Native::OoTItem, ITEM_OOT_DINS_FIRE };
        case MSI_FARORES_WIND: return { Native::OoTItem, ITEM_OOT_FARORES_WIND };
        case MSI_NAYRUS_LOVE: return { Native::OoTItem, ITEM_OOT_NAYRUS_LOVE };
        case MSI_SKULL_MASK: return { Native::OoTItem, ITEM_OOT_MASK_SKULL };
        case MSI_SPOOKY_MASK: return { Native::OoTItem, ITEM_OOT_MASK_SPOOKY };
        case MSI_GERUDO_MASK: return { Native::OoTItem, ITEM_OOT_MASK_GERUDO };
        case MSI_KEATON_MASK: return { Native::OoTItem, ITEM_OOT_MASK_KEATON }; // BUG-0066
        case MSI_BUNNY_HOOD: return { Native::OoTItem, ITEM_OOT_MASK_BUNNY };
        case MSI_GORON_MASK: return { Native::OoTItem, ITEM_OOT_MASK_GORON };
        case MSI_ZORA_MASK: return { Native::OoTItem, ITEM_OOT_MASK_ZORA };
        case MSI_MASK_OF_TRUTH: return { Native::OoTItem, ITEM_OOT_MASK_TRUTH };
        case MSI_WEIRD_EGG: return { Native::OoTItem, ITEM_OOT_WEIRD_EGG };
        case MSI_ZELDAS_LETTER: return { Native::OoTItem, ITEM_OOT_LETTER_ZELDA };
        case MSI_POCKET_EGG: return { Native::OoTItem, ITEM_OOT_POCKET_EGG };
        case MSI_COJIRO: return { Native::OoTItem, ITEM_OOT_COJIRO };
        case MSI_ODD_MUSHROOM: return { Native::OoTItem, ITEM_OOT_ODD_MUSHROOM };
        case MSI_ODD_POTION: return { Native::OoTItem, ITEM_OOT_ODD_POTION };
        case MSI_POACHERS_SAW: return { Native::OoTItem, ITEM_OOT_SAW };
        case MSI_PRESCRIPTION: return { Native::OoTItem, ITEM_OOT_PRESCRIPTION };
        case MSI_EYEBALL_FROG: return { Native::OoTItem, ITEM_OOT_FROG };
        case MSI_EYEDROPS: return { Native::OoTItem, ITEM_OOT_EYEDROPS };
        case MSI_CLAIM_CHECK: return { Native::OoTItem, ITEM_OOT_CLAIM_CHECK };
        case MSI_RUTOS_LETTER: return { Native::OoTItem, ITEM_OOT_LETTER_RUTO };
        // Ocarina of Time's quest status (MergeShipHyrule.h)
        case MSI_STONE_OF_AGONY: return { Native::ItemGive, ITEM_OOT_STONE_OF_AGONY };
        case MSI_GERUDO_MEMBERSHIP_CARD: return { Native::ItemGive, ITEM_OOT_GERUDO_CARD };
        case MSI_ZELDAS_LULLABY: return { Native::ItemGive, ITEM_OOT_SONG_LULLABY };
        case MSI_MINUET_OF_FOREST: return { Native::ItemGive, ITEM_OOT_SONG_MINUET };
        case MSI_BOLERO_OF_FIRE: return { Native::ItemGive, ITEM_OOT_SONG_BOLERO };
        case MSI_SERENADE_OF_WATER: return { Native::ItemGive, ITEM_OOT_SONG_SERENADE };
        case MSI_REQUIEM_OF_SPIRIT: return { Native::ItemGive, ITEM_OOT_SONG_REQUIEM };
        case MSI_NOCTURNE_OF_SHADOW: return { Native::ItemGive, ITEM_OOT_SONG_NOCTURNE };
        case MSI_PRELUDE_OF_LIGHT: return { Native::ItemGive, ITEM_OOT_SONG_PRELUDE };
        case MSI_FOREST_MEDALLION: return { Native::Quest, MERGESHIP_OOT_QUEST_MEDALLION_FOREST };
        case MSI_FIRE_MEDALLION: return { Native::Quest, MERGESHIP_OOT_QUEST_MEDALLION_FIRE };
        case MSI_WATER_MEDALLION: return { Native::Quest, MERGESHIP_OOT_QUEST_MEDALLION_WATER };
        case MSI_SPIRIT_MEDALLION: return { Native::Quest, MERGESHIP_OOT_QUEST_MEDALLION_SPIRIT };
        case MSI_SHADOW_MEDALLION: return { Native::Quest, MERGESHIP_OOT_QUEST_MEDALLION_SHADOW };
        case MSI_LIGHT_MEDALLION: return { Native::Quest, MERGESHIP_OOT_QUEST_MEDALLION_LIGHT };
        case MSI_KOKIRI_EMERALD: return { Native::Quest, MERGESHIP_OOT_QUEST_KOKIRI_EMERALD };
        case MSI_GORON_RUBY: return { Native::Quest, MERGESHIP_OOT_QUEST_GORON_RUBY };
        case MSI_ZORA_SAPPHIRE: return { Native::Quest, MERGESHIP_OOT_QUEST_ZORA_SAPPHIRE };
        // the ocarina and the stick / nut capacities: Majora's Mask's own items
        // Hyrule's Fairy Ocarina and Ocarina of Time are Hyrule's: never MM's ocarina in Termina (the user, 2026-09-24:
        // "When getting the Fairy Ocarina, don't put it in MM's inventory"; each game's ocarina is its own).
        // Hyrule's ocarina: its own, on the Items (Hyrule) page (with "Combine Shared Items" MM's too, GiveProxy)
        case MSI_FAIRY_OCARINA: return { Native::OoTOcarina, 0 };
        case MSI_OCARINA_OF_TIME: return { Native::OoTOcarina, 1 };
        case MSI_PROGRESSIVE_OCARINA: return { Native::OoTOcarina, 2 };
        // Hyrule's bow, quivers and bomb bags: its own, on the Items (Hyrule) page, held in Majora's Mask's slots with
        // the larger bag (MergeShipOwnership.h)
        case MSI_BOMB_BAG: return { Native::OoTBag, 1 };
        case MSI_BIG_BOMB_BAG: return { Native::OoTBag, 2 };
        case MSI_BIGGEST_BOMB_BAG: return { Native::OoTBag, 3 };
        case MSI_PROGRESSIVE_BOMB_BAG: return { Native::OoTBag, 0 };
        case MSI_FAIRY_BOW: return { Native::OoTBow, 0 };
        case MSI_PROGRESSIVE_BOW: return { Native::OoTBow, 1 };
        case MSI_BIG_QUIVER: return { Native::OoTQuiver, 2 };
        case MSI_BIGGEST_QUIVER: return { Native::OoTQuiver, 3 };
        // Hyrule's bottles (Majora's Mask's bottle slots hold them)
        case MSI_EMPTY_BOTTLE: return { Native::OoTBottle, ITEM_BOTTLE };
        case MSI_BOTTLE_WITH_MILK: return { Native::OoTBottle, ITEM_MILK_BOTTLE };
        case MSI_BOTTLE_WITH_RED_POTION: return { Native::OoTBottle, ITEM_POTION_RED };
        case MSI_PROGRESSIVE_STICK_UPGRADE: return { Native::StickUpg, 0 };
        case MSI_DEKU_STICK_BAG: return { Native::StickUpg, 1 };
        case MSI_DEKU_STICK_CAPACITY_20: return { Native::StickUpg, 2 };
        case MSI_DEKU_STICK_CAPACITY_30: return { Native::StickUpg, 3 };
        case MSI_PROGRESSIVE_NUT_UPGRADE: return { Native::NutUpg, 0 };
        case MSI_DEKU_NUT_BAG: return { Native::NutUpg, 1 };
        case MSI_DEKU_NUT_CAPACITY_30: return { Native::NutUpg, 2 };
        case MSI_DEKU_NUT_CAPACITY_40: return { Native::NutUpg, 3 };
        // new bottles (MM holds six; Hyrule sees the first four)
        case MSI_BOTTLE_WITH_GREEN_POTION: return { Native::OoTBottle, ITEM_POTION_GREEN };
        case MSI_BOTTLE_WITH_BLUE_POTION: return { Native::OoTBottle, ITEM_POTION_BLUE };
        case MSI_BOTTLE_WITH_FAIRY: return { Native::OoTBottle, ITEM_FAIRY };
        case MSI_BOTTLE_WITH_FISH: return { Native::OoTBottle, ITEM_FISH };
        case MSI_BOTTLE_WITH_BLUE_FIRE: return { Native::OoTBottle, ITEM_BLUE_FIRE };
        case MSI_BOTTLE_WITH_BUGS: return { Native::OoTBottle, ITEM_BUG };
        case MSI_BOTTLE_WITH_POE: return { Native::OoTBottle, ITEM_POE };
        case MSI_BOTTLE_WITH_BIG_POE: return { Native::OoTBottle, ITEM_BIG_POE };
        case MSI_MM_BOTTLE_WITH_DEKU_PRINCESS: return { Native::NewBottle, ITEM_DEKU_PRINCESS };
        case MSI_MM_BOTTLE_WITH_ZORA_EGG: return { Native::NewBottle, ITEM_ZORA_EGG };
        case MSI_MM_BOTTLE_WITH_MUSHROOM: return { Native::NewBottle, ITEM_MUSHROOM };
        case MSI_MM_BOTTLE_WITH_SEAHORSE: return { Native::NewBottle, ITEM_SEAHORSE };
        case MSI_MM_BOTTLE_WITH_HYLIAN_LOACH: return { Native::NewBottle, ITEM_HYLIAN_LOACH };
        case MSI_MM_BOTTLE_WITH_SPRING_WATER: return { Native::NewBottle, ITEM_SPRING_WATER };
        case MSI_MM_BOTTLE_WITH_HOT_SPRING_WATER: return { Native::NewBottle, ITEM_HOT_SPRING_WATER };
        // consumables: they stay with the engine whose check gave them (MS_CYCLE)
        case MSI_FISH: return { Native::ItemGive, ITEM_FISH };               // fills an empty bottle
        case MSI_BUY_BLUE_FIRE: return { Native::ItemGive, ITEM_BLUE_FIRE }; // fills an empty bottle
        case MSI_BUY_BOTTLE_BUG: return { Native::ItemGive, ITEM_BUG };      // fills an empty bottle
        case MSI_BUY_POE: return { Native::ItemGive, ITEM_POE };             // fills an empty bottle
        case MSI_BOMBS_20: return { Native::ItemGive, ITEM_BOMBS_20 };
        case MSI_BUY_BOMBS_30: return { Native::ItemGive, ITEM_BOMBS_30 };
        case MSI_BOMBCHU_20: return { Native::ItemGive, ITEM_BOMBCHUS_20 };
        case MSI_PROGRESSIVE_BOMBCHU_BAG: return { Native::ChuBag, ITEM_BOMBCHU };
        case MSI_ARROWS_5: return { Native::Arrows, 5 };
        case MSI_DEKU_SEEDS_30: return { Native::Seeds, 30 };
        default: return { Native::None, 0 };
    }
}

MergeShipOoTItemsSave& OoTItems() {
    return gSaveContext.save.shipSaveInfo.mergeShipOoTItems;
}
MergeShipOoTEquipSave& OoTEquip() {
    return gSaveContext.save.shipSaveInfo.mergeShipOoTEquip;
}

bool OwnsSlingshot() {
    return (OoTItems().owned & (1u << MS_OOT_OWN_SLINGSHOT)) != 0;
}

// The Ocarina of Time item a progressive action gives next (ITEM_NONE at its top).
u8 NextStrength() {
    static const u8 kItems[] = { ITEM_OOT_BRACELET, ITEM_OOT_GAUNTLETS_SILVER, ITEM_OOT_GAUNTLETS_GOLD };
    return (OoTEquip().strength < 3) ? kItems[OoTEquip().strength] : (u8)ITEM_NONE;
}
u8 NextScale() {
    return (OoTEquip().scale == 0) ? (u8)ITEM_OOT_SCALE_SILVER
                                   : (OoTEquip().scale == 1) ? (u8)ITEM_OOT_SCALE_GOLDEN : (u8)ITEM_NONE;
}

bool RaiseUpgradeTo(s32 upgrade, u16 level, u8 item) {
    const u16 current = (u16)CUR_UPG_VALUE(upgrade);
    const u16 target = (level == 0) ? (u16)(current + 1) : level;
    if (target > 3) {
        return false;
    }
    if (current < target) {
        Inventory_ChangeUpgrade(upgrade, target);
    }
    INV_CONTENT(item) = item;
    AMMO(item) = (s8)CUR_CAPACITY(upgrade);
    return true;
}

// A bottle content Termina has already and keeps only one of (MsDelivery.h MsDelivery_BottleOnePerWorld: the Deku
// Princess, the Seahorse): in one of its six bottles or Hyrule's four on the Items (Hyrule) page (MM
// Inventory_HasItemInBottle), or -- the Deku Princess -- back in the Deku King's chamber this cycle (MM EnDnp_Init,
// WEEKEVENTREG_23_20).
bool TerminaHasOnePerWorld(u8 item) {
    if (!MsDelivery_BottleOnePerWorld(MergeShipShared_BottleFromItem(item))) {
        return false;
    }
    if (Inventory_HasItemInBottle(item)) {
        return true;
    }
    for (s32 i = 0; i < 4; i++) {
        if (MergeShip_Own_OoTBottle(i) == item) {
            return true;
        }
    }
    return (item == ITEM_DEKU_PRINCESS) && CHECK_WEEKEVENTREG(WEEKEVENTREG_23_20);
}

bool GiveNative(const Action& action) {
    PlayState* play = gPlayState;
    if (play == nullptr) {
        return false;
    }
    switch (action.kind) {
        case Native::None:
            return false;
        case Native::ItemGive:
            // A pack of chus before the seed's Bombchu Bag: nothing (MergeShip_Own_ChusLocked).
            if ((((u8)action.arg >= ITEM_BOMBCHUS_20) && ((u8)action.arg <= ITEM_BOMBCHUS_5)) &&
                MergeShip_Own_ChusLocked()) {
                return false;
            }
            return Item_Give(play, (u8)action.arg) == ITEM_NONE;
        case Native::ChuBag:
            return MergeShip_Own_UnlockMMChus() != 0;
        case Native::OoTItem:
            // Directly: MM's Item_Give keeps ITEM_LONGSHOT as its "new bottle of Red Potion" (MergeShipOoTGetItemText.cpp).
            return MergeShip_OoTItems_Give(play, (u8)action.arg) == ITEM_NONE;
        case Native::OoTEquip:
            return MergeShip_OoTEquip_Give(play, (u8)action.arg) == ITEM_NONE;
        case Native::Quest:
            return MergeShip_Hyrule_GiveQuestItem((u8)action.arg) != 0;
        case Native::OoTBottle:
            // Hyrule's bottle: one of its own four (the shared save's, the Items (Hyrule) page; MergeShipOwnership.h).
            if (MergeShip_Own_OoTBottlesHere()) {
                return MergeShip_Own_GiveOoTBottle((u8)action.arg);
            }
            [[fallthrough]];
        case Native::NewBottle: {
            // One of a kind in its world and Termina has it already (a "Bottle with Deku Princess" found, from a
            // teammate or from Hyrule while Woodfall's is caught or home): the bottle comes empty, never a second one
            // (the user, 2026-09-30).
            u8 content = (u8)action.arg;
            if (TerminaHasOnePerWorld(content)) {
                Log("BOTTLE 0x%02X: Termina has its one already, the bottle comes empty", content);
                content = ITEM_BOTTLE;
            }
            for (s32 slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; slot++) {
                if (gSaveContext.save.saveInfo.inventory.items[slot] == ITEM_NONE) {
                    gSaveContext.save.saveInfo.inventory.items[slot] = content;
                    return true;
                }
            }
            return false;
        }
        case Native::Ocarina:
            if (INV_CONTENT(ITEM_OCARINA_OF_TIME) != ITEM_OCARINA_OF_TIME) {
                INV_CONTENT(ITEM_OCARINA_OF_TIME) = ITEM_OCARINA_OF_TIME;
            }
            return true;
        case Native::StickUpg:
            MergeShip_Own_GiveOoT(MS_OWN_STICK); // Hyrule's sticks (the Items (Hyrule) page)
            return RaiseUpgradeTo(UPG_DEKU_STICKS, action.arg, ITEM_DEKU_STICK);
        case Native::NutUpg:
            MergeShip_Own_GiveOoT(MS_OWN_NUT);
            return RaiseUpgradeTo(UPG_DEKU_NUTS, action.arg, ITEM_DEKU_NUT);
        case Native::OoTOcarina: {
            MsSharedSave* shared = Shared();
            const bool ofTime = (action.arg == 1) ||
                                ((action.arg == 2) && MergeShip_Own_OoTOwns(ITEM_OCARINA_OF_TIME));
            MergeShip_Own_GiveOoT(MS_OWN_OCARINA);
            if (ofTime && (shared != nullptr)) {
                shared->items |= 1u << MS_ITEM_OCARINA_OF_TIME; // the Items (Hyrule) cell shows Hyrule's level
            }
            return true;
        }
        case Native::OoTBag: {
            const s32 level = (action.arg != 0) ? action.arg : (MergeShip_Own_OoTUpgrade(UPG_BOMB_BAG) + 1);
            if (level > 3) {
                return false;
            }
            MergeShip_Own_RaiseOoTUpgrade(UPG_BOMB_BAG, level);
            MergeShip_Own_FillOoT(MS_OWN_BOMB); // a new bag comes full (Ocarina of Time's count)
            return true;
        }
        case Native::OoTBow:
            if (!MergeShip_Own_OoTOwns(ITEM_BOW)) {
                MergeShip_Own_GiveOoT(MS_OWN_BOW);
                MergeShip_Own_RaiseOoTUpgrade(UPG_QUIVER, 1);
            } else if (action.arg != 0) {
                if (MergeShip_Own_OoTUpgrade(UPG_QUIVER) >= 3) {
                    return false;
                }
                MergeShip_Own_RaiseOoTUpgrade(UPG_QUIVER, MergeShip_Own_OoTUpgrade(UPG_QUIVER) + 1);
            }
            MergeShip_Own_FillOoT(MS_OWN_BOW);
            return true;
        case Native::OoTQuiver:
            MergeShip_Own_RaiseOoTUpgrade(UPG_QUIVER, action.arg);
            MergeShip_Own_FillOoT(MS_OWN_BOW);
            return true;
        case Native::Strength: {
            const u8 item = NextStrength();
            return (item != ITEM_NONE) && (MergeShip_OoTEquip_Give(play, item) == ITEM_NONE);
        }
        case Native::Scale: {
            const u8 item = NextScale();
            return (item != ITEM_NONE) && (MergeShip_OoTEquip_Give(play, item) == ITEM_NONE);
        }
        case Native::Slingshot:
            if (!OwnsSlingshot()) {
                return MergeShip_OoTItems_Give(play, ITEM_SLINGSHOT) == ITEM_NONE;
            }
            return (OoTItems().bulletBag < 3) && (MergeShip_OoTEquip_Give(play, ITEM_OOT_BULLET_BAG) == ITEM_NONE);
        case Native::BulletBag:
            if (OoTItems().bulletBag >= action.arg) {
                return true;
            }
            while (OoTItems().bulletBag < action.arg) {
                MergeShip_OoTEquip_Give(play, ITEM_OOT_BULLET_BAG);
            }
            return true;
        case Native::Arrows:
            if (INV_CONTENT(ITEM_BOW) != ITEM_BOW) {
                return false;
            }
            Inventory_ChangeAmmo(ITEM_BOW, (s16)action.arg);
            return true;
        case Native::Hookshot:
            if (MergeShip_OoTItems_HookshotLevel() == 0) {
                OoTItems().owned |= (1u << MS_OOT_OWN_HOOKSHOT);
                return true;
            }
            return (MergeShip_OoTItems_HookshotLevel() < 2) &&
                   (MergeShip_OoTItems_Give(play, ITEM_LONGSHOT) == ITEM_NONE);
        case Native::OoTOwnBit:
            OoTItems().owned |= (1u << action.arg);
            return true;
        case Native::Seeds: {
            if (OoTItems().bulletBag == 0) {
                return false;
            }
            const s32 seeds = OoTItems().seeds + action.arg;
            const s32 capacity = MergeShip_OoTItems_SeedCapacity();
            OoTItems().seeds = (u8)((seeds > capacity) ? capacity : seeds);
            return true;
        }
    }
    return false;
}

// The Ocarina of Time / Majora's Mask item id standing for the action (for icons and models), ITEM_NONE if none.
u8 ActionItem(const Action& action) {
    switch (action.kind) {
        case Native::ItemGive:
        case Native::OoTItem:
        case Native::OoTEquip:
        case Native::NewBottle:
        case Native::OoTBottle:
        case Native::ChuBag:
            return (u8)action.arg;
        case Native::Strength:
            return (NextStrength() != ITEM_NONE) ? NextStrength() : (u8)ITEM_OOT_GAUNTLETS_GOLD;
        case Native::Scale:
            return (NextScale() != ITEM_NONE) ? NextScale() : (u8)ITEM_OOT_SCALE_GOLDEN;
        case Native::Slingshot:
            return OwnsSlingshot() ? (u8)ITEM_OOT_BULLET_BAG : (u8)ITEM_SLINGSHOT;
        case Native::BulletBag:
            return ITEM_OOT_BULLET_BAG;
        case Native::Seeds:
            return ITEM_OOT_DEKU_SEEDS;
        default:
            return ITEM_NONE;
    }
}

// ---- the ledger ---------------------------------------------------------------------------------------------------

bool sTestUnified = false; // harness: "unified test"
RandoCheckId sTestCheck = RC_UNKNOWN;
bool sLastProxyHeld = false;
// Termina's six bottles as they were before this frame's gives (OnGameStateUpdate): a bottle item is held only when
// the give changed one (all six full: MM's Item_Give adds nothing).
std::array<u8, 6> sBottleSnapshot{};

std::array<u8, 6> BottleSlots() {
    std::array<u8, 6> slots{};
    for (s32 i = 0; i < 6; i++) {
        slots[i] = gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1 + i];
    }
    return slots;
}

// The concrete item each check actually gave (OnCheckObtained's `converted`), by RandoCheckId: RI_UNKNOWN (0, the
// array's zero value) until the check is obtained. Rando::MergeShip::ResolvedItemOf reads it back for Anchor's
// cross-world "msItem" (Packets/GiveItem.cpp), which the placed pool item alone cannot give for a progressive check.
std::array<RandoItemId, RC_MAX> sResolvedItem{};

MsSharedSave* Shared() {
    return static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
}

// The unified seed's "Combine Shared Items" (SoH writes it to the shared save with the goal, SharedSave.h
// itemsCombined): an item both games have gives both games' versions. Off: each game's version is its own.
int CombinedItems() {
    const MsSharedSave* shared = Shared();
    return ((shared != nullptr) && shared->itemsCombined) ? 1 : 0;
}

void Record(MsSharedSave* shared, RandoCheckId randoCheckId, MsItemKey key, uint8_t flags) {
    const uint32_t count = (shared->ledgerCount < MS_SHARED_LEDGER_MAX) ? shared->ledgerCount : MS_SHARED_LEDGER_MAX;
    if (randoCheckId != RC_UNKNOWN) {
        for (uint32_t i = 0; i < count; i++) {
            const MsLedgerEntry& e = shared->ledger[i];
            if ((e.world == MS_WORLD_TERMINA) && (e.check == (uint16_t)randoCheckId)) {
                Log("LEDGER termina check %d already recorded (#%u)", (int)randoCheckId, i + 1);
                return;
            }
        }
    }
    if (count >= MS_SHARED_LEDGER_MAX) {
        SPDLOG_ERROR("MergeShip unified: the shared save's ledger is full; check {} not recorded", (int)randoCheckId);
        return;
    }
    MsLedgerEntry& e = shared->ledger[count];
    e.item = (uint16_t)key;
    e.check = (uint16_t)randoCheckId;
    e.world = MS_WORLD_TERMINA;
    e.flags = flags;
    e.seq = (uint16_t)(count + 1);
    shared->ledgerCount = count + 1;
    MergeShipHost_CommitSharedSave();
    Log("LEDGER +#%u world=termina check=%d item=%s flags=0x%02X", count + 1, (int)randoCheckId, kRows[key].keyName,
        flags);
}

// An entry Hyrule found for Termina: given natively here. `notify`: every caller now passes false (2026-09-30, "no
// notification burst on crossing") -- kept for a caller that one day needs its own announced delivery; the default
// stays off so a new call site does not reintroduce the burst by omission.
bool GiveTermina(MsItemKey key, bool notify = false) {
    if (key >= MSI_MAX) {
        return false;
    }
    const RandoItemId ri = kRows[key].ri;
    bool given = false;
    if (Rando::MergeShip::IsProxy(ri)) {
        given = Rando::MergeShip::GiveProxy(ri);
    } else if ((ri == RI_SWORD_KOKIRI) && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_NONE)) {
        given = false; // never a downgrade of MM's sword (MM's Item_Give equips it)
    } else if ((ri == RI_SHIELD_HERO) && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) != EQUIP_VALUE_SHIELD_NONE)) {
        given = false; // never a downgrade of the Mirror Shield
    } else {
        const RandoItemId converted = Rando::ConvertItem(ri);
        if ((converted != RI_JUNK) && (converted != RI_NONE) && (converted != RI_UNKNOWN)) {
            Rando::GiveItem(converted);
            given = true;
        }
    }
    if (given && notify) {
        Notification::Emit({
            .itemIcon = Rando::MergeShip::GetItemIconTexture(ri),
            .prefix = "From Hyrule:",
            .message = "",
            .suffix = std::string(Rando::StaticData::Items[ri].name) + Rando::MergeShip::OriginTag(ri),
        });
    }
    return given;
}

// Gives what Hyrule found for Termina since this file last did (ShipSaveInfo::mergeShipLedgerApplied).
void Project(bool force) {
    MsSharedSave* shared = Shared();
    if ((shared == nullptr) || !Rando::MergeShip::UnifiedActive() || (gPlayState == nullptr)) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    if (!force) {
        // Majora's Mask's prologue is its own (as the shared save's apply, MergeShipHost.cpp), and nothing is given
        // while an item is being received or a textbox is up.
        if ((gSaveContext.save.saveInfo.playerData.threeDayResetCount == 0) ||
            (player->stateFlags1 & (PLAYER_STATE1_400 | PLAYER_STATE1_TALKING)) ||
            (gPlayState->msgCtx.msgMode != MSGMODE_NONE)) {
            return;
        }
    }
    u32& applied = gSaveContext.save.shipSaveInfo.mergeShipLedgerApplied;
    const u32 count = (shared->ledgerCount < MS_SHARED_LEDGER_MAX) ? shared->ledgerCount : MS_SHARED_LEDGER_MAX;
    if (applied > count) {
        applied = count; // the shared save was reset (a new seed on this slot): nothing to redo
    }
    for (u32 i = applied; i < count; i++) {
        const MsLedgerEntry& e = shared->ledger[i];
        if ((e.world != MS_WORLD_HYRULE) || !(e.flags & MS_LEDGER_FOR_TERMINA)) {
            continue;
        }
        // Silent: the item already belongs to this file the moment Hyrule found it (the ledger write is
        // synchronous); catching it up here is bookkeeping, not a delivery to announce (the user, 2026-09-30: no
        // notification burst on crossing). The finder's own textbox and Anchor's live "<player> found ..." are
        // the only notifications for this item.
        const bool given = GiveTermina((MsItemKey)e.item, false);
        Log("PROJECT #%u from hyrule check=%d item=%s -> %s", i + 1, (int)e.check,
            (e.item < MSI_MAX) ? kRows[e.item].keyName : "?", given ? "given" : "nothing in Termina");
    }
    applied = count;
}

// The doubled Kokiri Sword (the user's seed, 2026-11-03): RI_PROGRESSIVE_SWORD resolved from the sword in use alone, so
// with Hyrule's sword on B (MM's put away on Termina's equipment page) a second progressive sword gave MM's Kokiri Sword
// again. A file that has it -- MM's Kokiri Sword recorded from two or more of Termina's progressive-sword checks --
// gets each step it missed (the Razor Sword, then the Gilded Sword) unless Termina holds it already, and the record is
// corrected to that step, which also keeps this from running again. Every frame, as Project; a scan only when the
// ledger has grown.
void RepairDoubledSwords() {
    static u32 sScanned = UINT32_MAX;
    MsSharedSave* shared = Shared();
    if ((shared == nullptr) || !Rando::MergeShip::UnifiedActive() || (gPlayState == nullptr)) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if ((player == nullptr) || (player->stateFlags1 & (PLAYER_STATE1_400 | PLAYER_STATE1_TALKING)) ||
        (gPlayState->msgCtx.msgMode != MSGMODE_NONE)) {
        return;
    }
    const u32 count = (shared->ledgerCount < MS_SHARED_LEDGER_MAX) ? shared->ledgerCount : MS_SHARED_LEDGER_MAX;
    if (count == sScanned) {
        return;
    }
    sScanned = count;
    s32 progressive = 0;
    for (u32 i = 0; i < count; i++) {
        MsLedgerEntry& e = shared->ledger[i];
        if ((e.world != MS_WORLD_TERMINA) || (e.flags & MS_LEDGER_REMOTE) || (e.check <= RC_UNKNOWN) ||
            (e.check >= RC_MAX) || (RANDO_SAVE_CHECKS[e.check].randoItemId != RI_PROGRESSIVE_SWORD)) {
            continue;
        }
        progressive++;
        if ((e.item != MSI_MM_KOKIRI_SWORD) || (progressive < 2)) {
            continue;
        }
        const MsItemKey want = (progressive == 2) ? MSI_MM_RAZOR_SWORD : MSI_MM_GILDED_SWORD;
        const u8 wantItem = (progressive == 2) ? ITEM_SWORD_RAZOR : ITEM_SWORD_GILDED;
        const bool held = MergeShip_TerminaEquip_Owns(wantItem) ||
                          ((progressive == 2) && MergeShip_TerminaEquip_Owns(ITEM_SWORD_GILDED)) ||
                          (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) >= ((progressive == 2) ? EQUIP_VALUE_SWORD_RAZOR
                                                                                          : EQUIP_VALUE_SWORD_GILDED));
        const bool given = !held && GiveTermina(want, true);
        e.item = (uint16_t)want;
        Log("REPAIR #%u check=%d: a second Kokiri Sword from a progressive sword -> %s (%s)", i + 1, (int)e.check,
            kRows[want].keyName, given ? "given" : (held ? "already held" : "not given"));
        MergeShipHost_CommitSharedSave();
    }
}

void RegisterHooks() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
        Project(false);
        RepairDoubledSwords();
        // The team's Termina checks (MsSharedSave.teamTerminaChecks), every frame the file plays the unified seed: a
        // file just made at the door, loaded, or resumed is not unified yet when it loads or when the shared save is
        // applied to it (its seed comes after), so those moments alone missed them. Only bit tests once all are marked.
        Rando::MergeShip::ApplyTeamChecks();
        sBottleSnapshot = BottleSlots();
    });
}

static RegisterShipInitFunc sInitFunc(RegisterHooks, {});

// ---- harness --------------------------------------------------------------------------------------------------------

bool SameName(const std::string& a, const char* b) {
    size_t i = 0;
    for (; (i < a.size()) && (b[i] != '\0'); i++) {
        if (toupper((unsigned char)a[i]) != toupper((unsigned char)b[i])) {
            return false;
        }
    }
    return (i == a.size()) && (b[i] == '\0');
}

MsItemKey ParseKey(const std::string& s) {
    for (size_t i = 0; i < MSI_MAX; i++) {
        if (SameName(s, kRows[i].keyName)) {
            return (MsItemKey)i;
        }
    }
    const RandoItemId ri = Rando::StaticData::GetItemIdFromName(s.c_str());
    if (ri != RI_UNKNOWN) {
        return KeyOfRI(ri);
    }
    const long n = strtol(s.c_str(), nullptr, 0);
    return ((n >= 0) && (n < MSI_MAX)) ? (MsItemKey)n : MSI_MAX;
}

RandoCheckId ParseCheck(const std::string& s) {
    if (s.empty()) {
        return RC_UNKNOWN;
    }
    if ((s[0] >= '0') && (s[0] <= '9')) {
        return (RandoCheckId)strtol(s.c_str(), nullptr, 0);
    }
    return Rando::StaticData::GetCheckIdFromName(s.c_str());
}

void LogState() {
    MsSharedSave* shared = Shared();
    Log("TERMINA ootItems=0x%08X ootEquip=0x%08X strength=%d scale=%d bulletBag=%d seeds=%d quest=0x%06X "
        "ocarina=0x%02X sword=%d shield=%d woodfallKeys=%d swim=%d ledger=%u applied=%u unified=%d",
        OoTItems().owned, OoTEquip().owned, OoTEquip().strength, OoTEquip().scale, OoTItems().bulletBag,
        OoTItems().seeds, MergeShip_Hyrule_GetQuestItems(), INV_CONTENT(ITEM_OCARINA_OF_TIME),
        GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD), GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD),
        DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE), Flags_GetRandoInf(RANDO_INF_OBTAINED_SWIM) ? 1 : 0,
        (shared != nullptr) ? shared->ledgerCount : 0u, gSaveContext.save.shipSaveInfo.mergeShipLedgerApplied,
        Rando::MergeShip::UnifiedActive() ? 1 : 0);
}

// The get-item a Termina check gives (Rando/MiscBehavior/CheckQueue.cpp's presentation), for `terminafind`.
void QueueTestFind(RandoItemId placed) {
    GameInteractor::Instance->events.emplace_back(GIEventGiveItem{
        .showGetItemCutscene = true,
        .param = (int16_t)placed,
        .giveItem =
            [](Actor* actor, PlayState* play) {
                const RandoItemId placed = (RandoItemId)CUSTOM_ITEM_PARAM;
                const RandoItemId converted = Rando::ConvertItem(placed, RC_UNKNOWN);
                RandoItemId given = (converted == RI_JUNK) ? Rando::CurrentJunkItem(RC_UNKNOWN) : converted;
                std::string message = Rando::MergeShip::DecorateGetItemMessage(
                    given, Rando::StaticData::GetItemName(given, true, RC_UNKNOWN));
                CustomMessage::Entry entry = {
                    .textboxType = 2,
                    .icon = Rando::StaticData::GetIconForZMessage(given),
                    .msg = "You found " + message + "!",
                };
                uint8_t icon;
                if (Rando::MergeShip::GetItemTextIcon(given, &icon)) {
                    entry.icon = icon;
                }
                if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
                    CustomMessage::SetActiveCustomMessage(entry.msg, entry);
                } else {
                    CustomMessage::StartTextbox(entry.msg + "\x1C\x02\x10", entry);
                }
                Rando::GiveItem(given);
                Rando::MergeShip::OnCheckObtained(sTestCheck, placed, converted);
                Log("TERMINAFIND %s -> %s \"%s\"", Rando::StaticData::Items[placed].spoilerName,
                    Rando::StaticData::Items[given].spoilerName, entry.msg.c_str());
            },
        .drawItem =
            [](Actor* actor, PlayState* play) {
                const RandoItemId placed = (RandoItemId)CUSTOM_ITEM_PARAM;
                Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
                Rando::DrawItem(Rando::ConvertItem(placed, RC_UNKNOWN), RC_UNKNOWN, actor);
            } });
}

} // namespace

namespace Rando {

namespace MergeShip {

bool UnifiedActive() {
    MsSharedSave* shared = Shared();
    if (shared == nullptr) {
        return false;
    }
    if (sTestUnified) {
        return true;
    }
    return IS_RANDO && (shared->ledgerSeed != 0) &&
           (shared->ledgerSeed == gSaveContext.save.shipSaveInfo.rando.finalSeed);
}

// An Ocarina of Time item found here is given here on its own menu (its native action) and to Hyrule by the ledger
// (the user, 2026-09-24: "Everything you get should be given ON THE FLY"). With "Combine Shared Items" Termina also
// gets Majora's Mask's copy of it (MsDelivery_Counterpart).
MsItemKey ProxyCounterpart(MsItemKey key) {
    const MsItemKey counterpart = CombinedItems() ? MsDelivery_Counterpart(key) : MSI_MAX;
    return ((counterpart < MSI_MAX) && !IsProxy(kRows[counterpart].ri)) ? counterpart : MSI_MAX;
}

bool ProxyHeldInTermina(RandoItemId proxy) {
    if (!IsProxy(proxy)) {
        return false;
    }
    return (ProxyCounterpart(KeyOfRI(proxy)) < MSI_MAX) || (NativeAction(KeyOfRI(proxy)).kind != Native::None);
}

bool GiveProxy(RandoItemId proxy) {
    const MsItemKey key = KeyOfRI(proxy);
    const MsItemKey counterpart = ProxyCounterpart(key);
    sLastProxyHeld = GiveNative(NativeAction(key));
    if (counterpart < MSI_MAX) {
        sLastProxyHeld = GiveTermina(counterpart, false) || sLastProxyHeld;
    }
    Log("PROXY %s: %s%s", (key < MSI_MAX) ? kRows[key].keyName : "?",
        sLastProxyHeld ? "given in Termina (its own menu)" : "Hyrule's (nothing in Termina)",
        (counterpart < MSI_MAX) ? ", with Majora's Mask's copy (combined)" : "");
    return sLastProxyHeld;
}

bool ConvertItem(RandoItemId randoItemId, RandoCheckId randoCheckId, RandoItemId* out) {
    // The progressive ocarina: Majora's Mask holds one ocarina, which is level 1 (the shared save's
    // MS_ITEM_OCARINA_FAIRY). A copy found in Termina with that one in hand is Hyrule's Ocarina of Time (level 2),
    // unless Hyrule has it already.
    if ((randoItemId != RI_OCARINA) || !UnifiedActive()) {
        return false;
    }
    // Each game's ocarina is its own (2026-09-24): Termina's copy never becomes Hyrule's, unless the seed combines them.
    if (!CombinedItems()) {
        return false;
    }
    if ((randoCheckId != RC_UNKNOWN) && RANDO_SAVE_CHECKS[randoCheckId].obtained) {
        return false;
    }
    if (INV_CONTENT(ITEM_OCARINA_OF_TIME) != ITEM_OCARINA_OF_TIME) {
        return false;
    }
    const MsSharedSave* shared = Shared();
    if ((shared != nullptr) && (shared->items & (1u << MS_ITEM_OCARINA_OF_TIME))) {
        return false;
    }
    *out = RI_OOT_OCARINA_OF_TIME;
    return true;
}

void OnCheckObtained(RandoCheckId randoCheckId, RandoItemId placed, RandoItemId converted) {
    // The concrete item this check actually gave, kept regardless of whether a unified seed is active: Anchor's
    // cross-world "msItem" (ResolvedItemOf, below) needs it for plain co-op too.
    if ((randoCheckId >= 0) && (randoCheckId < RC_MAX)) {
        sResolvedItem[randoCheckId] = converted;
    }
    MsSharedSave* shared = Shared();
    if ((shared == nullptr) || !UnifiedActive()) {
        return;
    }
    // The item delivered: a proxy the conversion made (the ocarina's second level); for a progressive check
    // (RI_PROGRESSIVE_SWORD and so on) the concrete step it resolved to -- what was actually given, and its own
    // MsItemKey (Kokiri/Razor/Gilded Sword...), never the abstract pool item's: Hyrule's own progressive chain does
    // not start at the same step (RG_PROGRESSIVE_MM_SWORD skips the Kokiri Sword, MsItems.def), so recording the
    // placed item there gave a teammate the wrong step (a Kokiri Sword recorded, and delivered, as a Razor Sword).
    // The seed's own placed item otherwise: a plain item, or a progressive already at its cap that resolved to junk
    // (nothing recorded as held).
    RandoItemId item = placed;
    if (IsProxy(converted)) {
        item = converted;
    } else if (KeyOfRI(converted) < MSI_MAX) {
        item = converted;
    }
    const MsItemKey key = KeyOfRI(item);
    if (key >= MSI_MAX) {
        return; // not an item of the table (RI_JUNK, RI_NONE ...)
    }
    bool held = IsProxy(item) ? sLastProxyHeld : (converted != RI_JUNK);
    // A bottle item is held when a bottle slot took it: with all six full MM gives nothing, and the ledger must then
    // take it to the other game (MsDelivery_Flags leaves a held bottle to the shared bottles alone).
    if (!IsProxy(item) && MsDelivery_IsBottleKey(key)) {
        held = (BottleSlots() != sBottleSnapshot);
    }
    const uint8_t flags =
        MsDelivery_Flags(key, MS_WORLD_TERMINA, held ? 1 : 0, (kRows[key].persist == MS_CYCLE) ? 1 : 0,
                         CombinedItems());
    Record(shared, randoCheckId, key, flags);
}

void NoteTeamCheck(RandoCheckId randoCheckId) {
    MsSharedSave* shared = Shared();
    if ((shared == nullptr) || !UnifiedActive() || (randoCheckId <= RC_UNKNOWN) || (randoCheckId >= RC_MAX)) {
        return;
    }
    if (MsShared_NoteTeamTerminaCheck(shared, (uint32_t)randoCheckId)) {
        MergeShipHost_CommitSharedSave();
        Log("TEAMCHECK +%d (a teammate's)", (int)randoCheckId);
    }
}

void ApplyTeamChecks() {
    static_assert(RC_MAX <= MS_TEAM_TERMINA_CHECK_BYTES * 8, "MsSharedSave.teamTerminaChecks: one bit per check");
    const MsSharedSave* shared = Shared();
    if ((shared == nullptr) || !UnifiedActive()) {
        return;
    }
    s32 applied = 0;
    for (s32 rc = RC_UNKNOWN + 1; rc < RC_MAX; rc++) {
        RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
        if (check.obtained || !MsShared_TeamTerminaCheck(shared, (uint32_t)rc)) {
            continue;
        }
        // A dungeon's keys, map and compass live only in this file: if it was not saved after one arrived, keeping its
        // check open is the way back to it (a lost key can lock a dungeon; an extra one cannot).
        const RandoItemId ri = check.randoItemId;
        if ((ri > RI_UNKNOWN) && (ri < RI_MAX)) {
            const auto& item = Rando::StaticData::Items[ri];
            if ((item.randoItemType == RITYPE_SMALL_KEY) || (item.randoItemType == RITYPE_BOSS_KEY) ||
                (item.itemId == ITEM_DUNGEON_MAP) || (item.itemId == ITEM_COMPASS)) {
                continue;
            }
        }
        // As HandlePacket_MMSetCheckStatus marks a teammate's find (Packets/SetCheckStatus.cpp), without the cycle:
        // this file missed it, so it counts as found in an earlier cycle -- found again, a check gives its junk
        // (Rando::IsItemObtainable), a shuffled grass / pot / crate its plain drop.
        check.obtained = true;
        check.eligible = false;
        MergeShipAnchor_NoteCheckStatus(rc); // Anchor must not take it for this player's own find and send it
        applied++;
    }
    if (applied > 0) {
        Log("TEAMCHECK %d of the team's Termina checks marked obtained in this file", applied);
    }
}

RandoItemId ResolvedItemOf(RandoCheckId randoCheckId) {
    return ((randoCheckId >= 0) && (randoCheckId < RC_MAX)) ? sResolvedItem[randoCheckId] : RI_UNKNOWN;
}

// The game an item is from, in Termina's texts and notifications (the user's rule: every item says which game's it
// is): " (OOT)" for Hyrule's (the RI_OOT_* proxies), " (MM)" for Majora's Mask's own. The Deku Mask and the Fierce
// Deity's Mask keep their names, as they do in Hyrule (SoH's naming rule).
std::string OriginTag(RandoItemId randoItemId) {
    if (!MergeShipHost_IsHosted() || (randoItemId == RI_NONE) || (randoItemId == RI_UNKNOWN) ||
        (randoItemId == RI_JUNK) || (randoItemId == RI_TRAP)) {
        return "";
    }
    // Masks carry no tag (the user's rule), either game's.
    const MsItemKey key = KeyOfRI(randoItemId);
    if ((key < MSI_MAX) && (kRows[key].kind == MS_KIND_MASK)) {
        return "";
    }
    // The item's own game (MsItems.def's columns): Majora's Mask's items delivered as proxies (its special bottles)
    // are Majora's Mask's.
    if (key < MSI_MAX) {
        if (MsDelivery_OwnWorld(key) == MS_WORLD_HYRULE) {
            return " (OOT)";
        }
        if (MsDelivery_OwnWorld(key) == MS_WORLD_TERMINA) {
            return " (MM)";
        }
        // A song both games have (Song of Time, Epona's Song, Song of Storms) is neither's: no tag.
        if (kRows[key].kind == MS_KIND_SONG) {
            return "";
        }
    }
    return IsProxy(randoItemId) ? " (OOT)" : " (MM)";
}

std::string DecorateGetItemMessage(RandoItemId randoItemId, const std::string& message) {
    const std::string tagged = message + OriginTag(randoItemId);
    if (!IsProxy(randoItemId)) {
        return tagged;
    }
    const MsItemKey key = KeyOfRI(randoItemId);
    if ((key >= MSI_MAX) || (kRows[key].persist == MS_CYCLE) || (NativeAction(key).kind != Native::None)) {
        return tagged;
    }
    return tagged + " for Hyrule";
}

bool GetItemTextIcon(RandoItemId randoItemId, uint8_t* icon) {
    if (!IsProxy(randoItemId)) {
        return false;
    }
    // BUG-0033: Greg the Green Rupee's message shows the Goron Mask, as Hyrule's does (SoH item_list.cpp
    // RG_GREG_RUPEE .CustomIcon(gItemIconMaskGoronTex)). The header's icon is a get-item id (BUG-0074: ITEM_MASK_GORON's
    // value is GI_SHIELD_MIRROR's).
    if (KeyOfRI(randoItemId) == MSI_GREG_RUPEE) {
        *icon = GI_MASK_GORON;
        return true;
    }
    const u8 item = ActionItem(NativeAction(KeyOfRI(randoItemId)));
    // Ocarina of Time's own icons (z_message.c takes the header icon as the item id, MergeShipOoTItems.h).
    if (!ITEM_IS_OOT_ITEM(item) && !ITEM_IS_OOT_EQUIP(item)) {
        return false;
    }
    *icon = item;
    MergeShip_OoTItems_SetTextIconPending(item);
    return true;
}

const char* GetItemIconTexture(RandoItemId randoItemId) {
    if (!IsProxy(randoItemId)) {
        return Rando::StaticData::GetIconTexturePath(randoItemId);
    }
    if (KeyOfRI(randoItemId) == MSI_GREG_RUPEE) { // BUG-0074: the notification's icon too
        return Rando::StaticData::GetIconTexturePath(RI_MASK_GORON);
    }
    const Action action = NativeAction(KeyOfRI(randoItemId));
    if (action.kind == Native::Quest) {
        return MergeShip_Hyrule_QuestIconTexture((u8)action.arg);
    }
    const u8 item = ActionItem(action);
    if (ITEM_IS_OOT_EQUIP(item)) {
        return MergeShip_OoTEquip_IconTexture(item);
    }
    if (ITEM_IS_OOT_ITEM(item) || (item == ITEM_SLINGSHOT) || (item == ITEM_LONGSHOT)) {
        return MergeShip_OoTItems_IconTexture(item);
    }
    if (item == ITEM_OOT_STONE_OF_AGONY) {
        return MergeShip_Hyrule_QuestIconTexture(MERGESHIP_OOT_QUEST_STONE_OF_AGONY);
    }
    if (item == ITEM_OOT_GERUDO_CARD) {
        return MergeShip_Hyrule_QuestIconTexture(MERGESHIP_OOT_QUEST_GERUDO_CARD);
    }
    return nullptr;
}

bool DrawProxy(RandoItemId proxy) {
    PlayState* play = gPlayState;
    const MsItemKey key = KeyOfRI(proxy);
    if ((play == nullptr) || (key >= MSI_MAX)) {
        return false;
    }
    const Action action = NativeAction(key);
    const u8 item = ActionItem(action);
    switch (action.kind) {
        case Native::OoTItem:
        case Native::Slingshot:
        case Native::Seeds:
            if (item == ITEM_OOT_BULLET_BAG) {
                MergeShip_OoTEquip_DrawGetItem(play, item);
            } else {
                MergeShip_OoTItems_DrawGetItem(play, item);
            }
            return true;
        case Native::OoTEquip:
        case Native::Strength:
        case Native::Scale:
        case Native::BulletBag:
            MergeShip_OoTEquip_DrawGetItem(play, item);
            return true;
        case Native::Quest:
            MergeShip_Hyrule_DrawGetItemModel(play, (u16)(0x100 + action.arg));
            return true;
        case Native::ItemGive:
            if (MergeShip_Hyrule_IsHyruleItem(item)) {
                MergeShip_Hyrule_DrawGetItemModel(play, item);
                return true;
            }
            break;
        default:
            break;
    }
    // Every other Hyrule item: Ocarina of Time's own model, as SoH draws it (OoTGiDraw.h, tools/gen_cross_gi.py).
    if (MergeShip_OoTGi_DrawKey(play, key)) {
        return true;
    }
    // Without oot.o2r: Majora's Mask's closest model.
    switch (action.kind) {
        case Native::Ocarina:
            GetItem_Draw(play, GID_OCARINA);
            return true;
        case Native::StickUpg:
            GetItem_Draw(play, GID_DEKU_STICK);
            return true;
        case Native::NutUpg:
            GetItem_Draw(play, GID_DEKU_NUTS);
            return true;
        case Native::Arrows:
            GetItem_Draw(play, GID_ARROWS_SMALL);
            return true;
        default:
            break;
    }
    switch (item) {
        case ITEM_POTION_GREEN: GetItem_Draw(play, GID_POTION_GREEN); return true;
        case ITEM_POTION_BLUE: GetItem_Draw(play, GID_POTION_BLUE); return true;
        case ITEM_FAIRY: GetItem_Draw(play, GID_FAIRY); return true;
        case ITEM_FISH: GetItem_Draw(play, GID_FISH); return true;
        case ITEM_BUG: GetItem_Draw(play, GID_BUG); return true;
        case ITEM_POE: GetItem_Draw(play, GID_POE); return true;
        case ITEM_BIG_POE: GetItem_Draw(play, GID_BIG_POE); return true;
        case ITEM_BOMBS_20:
        case ITEM_BOMBS_30: GetItem_Draw(play, GID_BOMB); return true;
        case ITEM_BOMBCHUS_20: GetItem_Draw(play, GID_BOMBCHU); return true;
        default: break;
    }
    if ((action.kind == Native::NewBottle) || (action.kind == Native::OoTBottle)) {
        GetItem_Draw(play, GID_BOTTLE);
        return true;
    }
    const char* name = kRows[key].keyName;
    switch (kRows[key].kind) {
        case MS_KIND_SMALL_KEY:
            GetItem_Draw(play, (strstr(name, "SILVER") != nullptr) ? GID_RUPEE_HUGE : GID_KEY_SMALL);
            return true;
        case MS_KIND_BOSS_KEY:
            GetItem_Draw(play, GID_KEY_BOSS);
            return true;
        case MS_KIND_DUNGEON_MAP:
            GetItem_Draw(play, (strstr(name, "COMPASS") != nullptr) ? GID_COMPASS : GID_DUNGEON_MAP);
            return true;
        case MS_KIND_TOKEN:
            GetItem_Draw(play, GID_SKULL_TOKEN);
            return true;
        default:
            break;
    }
    switch (key) {
        case MSI_KEATON_MASK: GetItem_Draw(play, GID_MASK_KEATON); return true;
        case MSI_BUNNY_HOOD: GetItem_Draw(play, GID_MASK_BUNNY); return true;
        case MSI_MASK_OF_TRUTH: GetItem_Draw(play, GID_MASK_TRUTH); return true;
        case MSI_HOOKSHOT:
        case MSI_PROGRESSIVE_HOOKSHOT: GetItem_Draw(play, GID_HOOKSHOT); return true;
        case MSI_MAGIC_BEAN_PACK:
        case MSI_PROGRESSIVE_DEKU_BEANS: GetItem_Draw(play, GID_MAGIC_BEANS); return true;
        case MSI_GREG_RUPEE: GetItem_Draw(play, GID_RUPEE_GREEN); return true;
        default: return false;
    }
}

bool TestCommand(const std::string& op, const std::vector<std::string>& args) {
    auto arg = [&](size_t i) -> std::string { return (i < args.size()) ? args[i] : std::string(); };

    if (op == "unified") {
        // unified test [seed] | unified off: force the unified mode for a file that is not a unified seed's.
        sTestUnified = (arg(0) == "test");
        MsSharedSave* shared = Shared();
        if (sTestUnified && (shared != nullptr) && !arg(1).empty()) {
            shared->ledgerSeed = (uint32_t)strtoul(arg(1).c_str(), nullptr, 0);
        }
        Log("UNIFIED test=%d active=%d linked=%d seed=%u ledger=%u applied=%u", sTestUnified ? 1 : 0,
            UnifiedActive() ? 1 : 0, (shared != nullptr) ? 1 : 0, (shared != nullptr) ? shared->ledgerSeed : 0u,
            (shared != nullptr) ? shared->ledgerCount : 0u, gSaveContext.save.shipSaveInfo.mergeShipLedgerApplied);
        return true;
    }
    if (op == "ledger") {
        MsSharedSave* shared = Shared();
        if (shared == nullptr) {
            Log("LEDGER no shared save (not linked to a Hyrule file)");
            return true;
        }
        const uint32_t count = (shared->ledgerCount < MS_SHARED_LEDGER_MAX) ? shared->ledgerCount : 0u;
        Log("LEDGER seed=%u count=%u applied=%u", shared->ledgerSeed, count,
            gSaveContext.save.shipSaveInfo.mergeShipLedgerApplied);
        for (uint32_t i = 0; i < count; i++) {
            const MsLedgerEntry& e = shared->ledger[i];
            Log("LEDGER #%u world=%s check=%u item=%s flags=0x%02X", i + 1, WorldName(e.world), e.check,
                (e.item < MSI_MAX) ? kRows[e.item].keyName : "?", e.flags);
        }
        return true;
    }
    if (op == "teamcheck") {
        // teamcheck <check> [note|apply|open]: a check's team bit (MsSharedSave.teamTerminaChecks) and its status in
        // this file; "note" sets the bit as a teammate's MM_SET_CHECK_STATUS does, "apply" runs ApplyTeamChecks,
        // "open" marks the check not obtained in this file (as a file saved before the find).
        const RandoCheckId rc = ParseCheck(arg(0));
        if ((rc <= RC_UNKNOWN) || (rc >= RC_MAX)) {
            Log("TEAMCHECK unknown check %s", arg(0).c_str());
            return true;
        }
        if (arg(1) == "note") {
            NoteTeamCheck(rc);
        } else if (arg(1) == "apply") {
            ApplyTeamChecks();
        } else if (arg(1) == "open") {
            RANDO_SAVE_CHECKS[rc].obtained = false;
            RANDO_SAVE_CHECKS[rc].cycleObtained = false;
            RANDO_SAVE_CHECKS[rc].eligible = true;
            MergeShipAnchor_NoteCheckStatus(rc);
        }
        const RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
        Log("TEAMCHECK %d team=%d obtained=%d cycleObtained=%d item=%d", (int)rc,
            MsShared_TeamTerminaCheck(Shared(), (uint32_t)rc), check.obtained ? 1 : 0, check.cycleObtained ? 1 : 0,
            (int)check.randoItemId);
        return true;
    }
    if (op == "ledgeradd") {
        // ledgeradd <hyrule|termina> <item> [check] [flags]: an entry as the other engine would write it.
        MsSharedSave* shared = Shared();
        const MsItemKey key = ParseKey(arg(1));
        if ((shared == nullptr) || (key >= MSI_MAX) || (shared->ledgerCount >= MS_SHARED_LEDGER_MAX)) {
            Log("LEDGERADD failed (%s)", arg(1).c_str());
            return true;
        }
        const uint8_t world = (arg(0) == "termina") ? MS_WORLD_TERMINA : MS_WORLD_HYRULE;
        const uint8_t flags =
            arg(3).empty() ? MsDelivery_Flags(key, world, 1, (kRows[key].persist == MS_CYCLE) ? 1 : 0,
                                              CombinedItems())
                           : (uint8_t)strtoul(arg(3).c_str(), nullptr, 0);
        MsLedgerEntry& e = shared->ledger[shared->ledgerCount];
        e.item = (uint16_t)key;
        e.check = (uint16_t)strtoul(arg(2).c_str(), nullptr, 0);
        e.world = world;
        e.flags = flags;
        e.seq = (uint16_t)(shared->ledgerCount + 1);
        shared->ledgerCount++;
        Log("LEDGERADD #%u world=%s item=%s flags=0x%02X", shared->ledgerCount, WorldName(world),
            kRows[key].keyName, flags);
        return true;
    }
    if (op == "terminafind") {
        // terminafind <item> [check]: a Termina check holding <item> is found (the randomizer's get-item).
        const MsItemKey key = ParseKey(arg(0));
        if (key >= MSI_MAX) {
            Log("TERMINAFIND unknown item %s", arg(0).c_str());
            return true;
        }
        sTestCheck = ParseCheck(arg(1));
        QueueTestFind(kRows[key].ri);
        Log("TERMINAFIND queued %s check=%d", kRows[key].keyName, (int)sTestCheck);
        return true;
    }
    if (op == "project") {
        Project(true);
        LogState();
        return true;
    }
    if (op == "unifiedstate") {
        LogState();
        return true;
    }
    return false;
}

} // namespace MergeShip

} // namespace Rando

// Anchor (AnchorNet.h "Cross-world items", 2s2h/Network/Anchor/MergeShipAnchorHost.cpp): a teammate found this item in
// Hyrule and the player is in Termina. Delivered as an item found here (MsDelivery.h), without a check of this file:
// given natively where Termina holds it (a proxy through GiveProxy, a native item through Rando::ConvertItem, which
// makes an owned item nothing); Hyrule gets it from the shared save's state on the next crossing, or from a ledger
// entry (world termina, check 0, MS_LEDGER_REMOTE) when the state does not carry it or Termina cannot hold it. Anchor
// claims the packet only after a 1, so this runs once per item per process.
extern "C" int32_t MergeShipShared_ReceiveRemoteItem(int32_t msItemKey) {
    if ((msItemKey < 0) || (msItemKey >= MSI_MAX) || (gPlayState == nullptr) || (msItemKey == MSI_ICE_TRAP)) {
        return 0; // not an item, no game to give it to, or a trap (Anchor's own): Anchor keeps the packet
    }
    const MsItemKey key = (MsItemKey)msItemKey;
    const RandoItemId ri = kRows[key].ri;
    const bool unified = Rando::MergeShip::UnifiedActive();
    const bool consumable = (kRows[key].persist == MS_CYCLE);
    const bool canHold = !Rando::MergeShip::IsProxy(ri) || Rando::MergeShip::ProxyHeldInTermina(ri);
    if (!canHold && !unified) {
        return 0; // outside a unified seed nothing takes it to Hyrule: Hyrule's engine applies it itself
    }
    const bool held = canHold && GiveTermina(key, false); // Anchor shows its own "<player> found ..." notification
    MsSharedSave* shared = Shared();
    if (unified && (shared != nullptr)) {
        const uint8_t flags =
            MsDelivery_Flags(key, MS_WORLD_TERMINA, held ? 1 : 0, consumable ? 1 : 0, CombinedItems());
        Record(shared, RC_UNKNOWN, key, (uint8_t)(flags | MS_LEDGER_REMOTE));
    }
    Log("REMOTE %s from an Anchor teammate: %s%s", kRows[key].keyName, held ? "given in Termina" : "not held in Termina",
        unified ? ", recorded" : "");
    return (held || unified) ? 1 : 0;
}
