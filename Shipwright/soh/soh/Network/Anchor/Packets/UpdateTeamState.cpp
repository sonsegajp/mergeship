#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include <nlohmann/json.hpp>
#include "soh/OTRGlobals.h"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include "soh/Network/Anchor/MergeShipAnchor.h"
#include "soh/MergeShip/MergeShipHost.h"         // MergeShip2: the shared save of this file
#include "soh/MergeShip/MergeShipMMEquipment.h"  // MergeShip2: MMEQUIP_SLOT_* (Majora's Mask's gear in Hyrule's slots)
#include "soh/MergeShip/MergeShipSharedSave.h"   // MergeShip2: MergeShipShared_Capture / _Apply
#include "soh/MergeShip/MergeShipMMItems.h"      // MergeShip2: gMergeShipMMItemsSave (keg, Pictograph Box, trades)
#include "soh/MergeShip/MergeShipChateau.h"      // MergeShip2: MERGESHIP_ITEM_IS_BOTTLE
#include <mergeship/AnchorHost.hpp>              // MergeShip2: MsAnchor_SeedMatches
#include <mergeship/AnchorReplay.hpp>            // MergeShip2: MsAnchor_ReplaySeq
#include <mergeship/SharedSaveJson.hpp>          // MergeShip2: "mergeShipShared" (AnchorNet.h)

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"                    // Interface_LoadItemIcon1
#include "soh/MergeShip/MergeShipForms.h" // gMergeShipSave
#include "soh/MergeShip/MergeShipOwnership.h" // MergeShip_Own_ChuCapacity (BUG-0105)
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...); // MergeShip2: the test harness's trace
}

/**
 * UPDATE_TEAM_STATE
 *
 * Pushes the current save state to the server for other teammates to use.
 *
 * Fires when the server passes on a REQUEST_TEAM_STATE packet, or when this client saves the game
 *
 * When sending this packet we will assume that the team queue has been emptied for this client, so the queue
 * stored in the server will be cleared.
 *
 * When receiving this packet, if there is items in the team queue, we will play them back in order.
 *
 * MergeShip2: the team is one playthrough, and every player has all of it (the user, 2026-09-30: "when anyone joins,
 * they should get EXACTLY what any player has. DOWN to a T"). Stock Anchor let the last state received overwrite the
 * receiver's (the player with the most progress was meant to load first): a joiner lost what only they had, and a
 * teammate's save could take items back. Here a received state is a union merge: flags are ORed, capacities, levels and
 * counts raised, an empty slot takes the team's item; nothing the receiver has is ever lost, so any teammate's state,
 * in any order, any number of times, converges on everything the team has. The state also carries what
 * Ocarina of Time's SaveContext does not: MergeShip's own progress in Hyrule ("mergeShip") and the shared save
 * ("mergeShipShared", mergeship.exe), and Termina's part ("mergeShipMM", applied by 2Ship).
 */

namespace {

// A number from a teammate's state, 0 when it is missing or not a number: an older or malformed state must never
// throw half-way through the merge (whatever was merged before the throw would stay, the rest would be lost).
int64_t Num(const json& j) {
    if (j.is_number_unsigned()) {
        return (int64_t)j.get<uint64_t>();
    }
    if (j.is_number_integer()) {
        return j.get<int64_t>();
    }
    if (j.is_number_float()) {
        return (int64_t)j.get<double>();
    }
    if (j.is_boolean()) {
        return j.get<bool>() ? 1 : 0;
    }
    return 0;
}

int64_t Num(const json& j, const char* key) {
    return (j.is_object() && j.contains(key)) ? Num(j[key]) : 0;
}

// j[key] when it is an array / an object, else an empty one.
const json& ArrayOf(const json& j, const char* key) {
    static const json kEmpty = json::array();
    return (j.is_object() && j.contains(key) && j[key].is_array()) ? j[key] : kEmpty;
}

const json& ObjectOf(const json& j, const char* key) {
    static const json kEmpty = json::object();
    return (j.is_object() && j.contains(key) && j[key].is_object()) ? j[key] : kEmpty;
}

template <typename T> bool OrInto(T& field, int64_t theirs) {
    const T merged = (T)(field | (T)theirs);
    if (merged == field) {
        return false;
    }
    field = merged;
    return true;
}

template <typename T> bool MaxInto(T& field, int64_t theirs) {
    if ((int64_t)field >= theirs) {
        return false;
    }
    field = (T)theirs;
    return true;
}

template <typename T, size_t N> bool OrArray(T (&fields)[N], const json& theirs, size_t count = N) {
    bool changed = false;
    for (size_t i = 0; (i < count) && (i < N) && (i < theirs.size()); i++) {
        changed |= OrInto(fields[i], Num(theirs[i]));
    }
    return changed;
}

// ---- Majora's Mask's gear held in Ocarina of Time's slots (MergeShipMMEquipment.h) ---------------------------------
//
// An MM sword, shield, hookshot or bow owned without its Ocarina of Time partner holds the partner's slot: the slot's
// equipment bit or Items-page item is set, and gMergeShipSave.mmEquipSlotOnlyMM says the partner itself is not owned
// (MergeShipMMEquipment.c FillSlot). The team state's inventory is Ocarina of Time's, which has no such bit: sent as
// is, a teammate got a real Hylian Shield for a Hero's Shield (the user's joiner "got a Hylian Shield I don't have",
// 2026-09-30), a Kokiri Sword for the Razor Sword, a Fairy Bow for the Hero's Bow. So the sender leaves those out; the
// MM item itself goes in "mergeShip" (mmEquipOwned), and the receiver's own Sanitize holds the slot for it again.

// The equipment bit a slot holds (0 for the Items-page slots).
u16 SlotEquipFlag(s32 slot) {
    switch (slot) {
        case MMEQUIP_SLOT_KOKIRI_SWORD:
            return OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);
        case MMEQUIP_SLOT_BIGGORON:
            return OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
        case MMEQUIP_SLOT_HYLIAN:
            return OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_HYLIAN);
        case MMEQUIP_SLOT_MIRROR:
            return OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR);
        default:
            return 0;
    }
}

// The Items-page slot a slot holds (-1 for the equipment slots).
s32 SlotInvSlot(s32 slot) {
    switch (slot) {
        case MMEQUIP_SLOT_HOOKSHOT:
            return SLOT_HOOKSHOT;
        case MMEQUIP_SLOT_BOW:
            return SLOT_BOW;
        default:
            return -1;
    }
}

// `item` in that Items-page slot is the slot's Ocarina of Time item.
bool SlotHoldsOoTItem(s32 slot, int64_t item) {
    return (slot == MMEQUIP_SLOT_BOW) ? (item == ITEM_BOW) : ((item == ITEM_HOOKSHOT) || (item == ITEM_LONGSHOT));
}

// Sender: takes the slots held only for MM gear out of the team state's inventory.
void StripMMPlaceholders(json& inventory) {
    if (!inventory.is_object()) {
        return;
    }
    for (s32 slot = 0; slot < MMEQUIP_SLOT_MAX; slot++) {
        if (!(gMergeShipSave.mmEquipSlotOnlyMM & (1 << slot))) {
            continue;
        }
        const u16 flag = SlotEquipFlag(slot);
        if ((flag != 0) && inventory.contains("equipment")) {
            inventory["equipment"] = (u16)(Num(inventory["equipment"]) & ~flag);
        }
        const s32 invSlot = SlotInvSlot(slot);
        if ((invSlot >= 0) && inventory.contains("items") && inventory["items"].is_array() &&
            ((size_t)invSlot < inventory["items"].size())) {
            inventory["items"][invSlot] = (u8)ITEM_NONE;
        }
    }
}

// Receiver: a slot this player holds only for MM gear, which the teammate owns the real Ocarina of Time item of (the
// sender left out its own placeholders): the merge just gave this player that item, so the slot is no longer MM's
// alone.
bool TakeRealOoTGear(int64_t theirEquipment, const json& theirItems) {
    bool changed = false;
    for (s32 slot = 0; slot < MMEQUIP_SLOT_MAX; slot++) {
        if (!(gMergeShipSave.mmEquipSlotOnlyMM & (1 << slot))) {
            continue;
        }
        const u16 flag = SlotEquipFlag(slot);
        const s32 invSlot = SlotInvSlot(slot);
        const bool real = (flag != 0) ? ((theirEquipment & flag) != 0)
                                      : ((invSlot >= 0) && ((size_t)invSlot < theirItems.size()) &&
                                         SlotHoldsOoTItem(slot, Num(theirItems[invSlot])));
        if (real) {
            gMergeShipSave.mmEquipSlotOnlyMM &= ~(1 << slot);
            changed = true;
        }
    }
    return changed;
}

#ifdef MERGESHIP_HOSTED
// The host's shared save when it is this file's (MsOwn_Shared's test, MergeShipMMEquipment.c): an earlier
// playthrough's is never sent or merged into.
MsSharedSave* TeamSharedSave() {
    if (!MergeShipHost_IsHosted() || (gSaveContext.fileNum < 0) || (gSaveContext.fileNum > 2)) {
        return nullptr;
    }
    MsSharedSave* shared = static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
    if ((shared == nullptr) || (shared->hyruleFileId != MergeShip_Save_SharedFileId())) {
        return nullptr;
    }
    return shared;
}
#endif

// ---- bottles ------------------------------------------------------------------------------------------------------

// A content Majora's Mask keeps one of per world (MsDelivery_BottleOnePerWorld: the Deku Princess, the Seahorse) as the
// shared save's bottle, or MS_BOTTLE_NONE for any other item.
u8 OnePerWorldBottle(u8 item) {
    switch (item) {
        case ITEM_MM_DEKU_PRINCESS:
            return MS_BOTTLE_DEKU_PRINCESS;
        case ITEM_MM_SEAHORSE:
            return MS_BOTTLE_SEAHORSE;
        default:
            return MS_BOTTLE_NONE;
    }
}

// This player's world has its one `item` already: in one of Hyrule's four bottles (including those this merge filled)
// or, in mergeship.exe, one of Majora's Mask's six, which Hyrule holds in the shared save -- the same test
// MergeShip_Own_GiveMMBottle makes for a find.
bool WorldHasOnePerWorld(u8 item) {
    const u8 bottle = OnePerWorldBottle(item);
    if (bottle == MS_BOTTLE_NONE) {
        return false;
    }
    for (s32 i = 0; i < 4; i++) {
        if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == item) {
            return true;
        }
    }
#ifdef MERGESHIP_HOSTED
    if (const MsSharedSave* shared = TeamSharedSave()) {
        for (s32 i = 0; i < MS_SHARED_BOTTLE_SLOTS; i++) {
            if (shared->bottles[i] == bottle) {
                return true;
            }
        }
    }
#endif
    return false;
}

// A Hyrule bottle slot (SLOT_BOTTLE_1..4) and a teammate's bottle `theirs`. An empty slot takes it; so does Ruto's
// Letter once King Zora has moved (a teammate delivered it, which made their letter an empty bottle and moved him for
// the team, so this copy can never be delivered: kept, it was a bottle lost for good). Any other bottle keeps its own
// content. A content there is one of per world comes as an empty bottle when this player's world has its one already
// (the user, 2026-09-30: never a second Deku Princess). True when the slot changed.
bool MergeBottleSlot(u8& ours, u8 theirs) {
    if (!MERGESHIP_ITEM_IS_BOTTLE(theirs) || (theirs == ours)) {
        return false;
    }
    const bool undeliverableLetter = (ours == ITEM_LETTER_RUTO) && (theirs != ITEM_LETTER_RUTO) &&
                                     GET_EVENTCHKINF(EVENTCHKINF_KING_ZORA_MOVED);
    if ((ours != ITEM_NONE) && !undeliverableLetter) {
        return false;
    }
    const u8 taken = WorldHasOnePerWorld(theirs) ? (u8)ITEM_BOTTLE : theirs;
    if (taken == ours) {
        return false;
    }
    ours = taken;
    return true;
}

// ---- "mergeShip": MergeShip's own progress in Hyrule (gMergeShipSave, MS_ANCHOR_KEY_MERGESHIP_TEAM_STATE) ----------
//
// Majora's Mask's masks, gear, songs, remains, owls and stray fairies held in Hyrule, and its items that have no Ocarina
// of Time slot (gMergeShipMMItemsSave: the Powder Keg and its ammo, the Pictograph Box, the trade items): soh.exe has no
// shared save to carry them. The per-player fields (the form the player is in, the B items, the variant in use, the
// transformation cutscenes seen, the Adult Mask's timeline) are not the team's.

// Every MM trade item's bit in gMergeShipMMItemsSave.tradeOwned (Moon's Tear .. Pendant of Memories).
constexpr u16 kMMTradeOwnedBits = (u16)((1u << (ITEM_MM_PENDANT_OF_MEMORIES - ITEM_MM_MOONS_TEAR + 1)) - 1);

json MergeShipPart() {
    json j;
    j["ownedMasks"] = gMergeShipSave.ownedMasks;
    j["ownedMMMasks"] = gMergeShipSave.ownedMMMasks;
    j["mmEquipOwned"] = gMergeShipSave.mmEquipOwned;
    j["mmSongs"] = gMergeShipSave.mmSongs;
    j["mmQuestItems"] = gMergeShipSave.mmQuestItems;
    j["owlActivationFlags"] = gMergeShipSave.owlActivationFlags;
    j["strayFairies"] =
        std::vector<int>(std::begin(gMergeShipSave.strayFairies), std::end(gMergeShipSave.strayFairies));
    j["mmRazorHealth"] = gMergeShipSave.mmRazorHealth;
    j["powderKeg"] = gMergeShipMMItemsSave.powderKeg;
    j["powderKegAmmo"] = gMergeShipMMItemsSave.powderKegAmmo;
    j["pictographBox"] = gMergeShipMMItemsSave.pictographBox;
    j["tradeDeed"] = gMergeShipMMItemsSave.tradeDeed;
    j["tradeKeyMama"] = gMergeShipMMItemsSave.tradeKeyMama;
    j["tradeCouple"] = gMergeShipMMItemsSave.tradeCouple;
    j["tradeOwned"] = gMergeShipMMItemsSave.tradeOwned;
    return j;
}

// An empty `slot` takes the teammate's trade item when it is one of the slot's own (first .. last), which is then owned.
bool FillTradeSlot(u8& slot, int64_t theirs, u8 first, u8 last) {
    if ((slot != ITEM_NONE) || (theirs < first) || (theirs > last)) {
        return false;
    }
    slot = (u8)theirs;
    gMergeShipMMItemsSave.tradeOwned |= (u16)MERGESHIP_MM_TRADE_BIT(slot);
    return true;
}

// takeCycleBound: the teammate's Termina is in this player's three-day cycle, or there is no Termina (soh.exe). In
// mergeship.exe what Hyrule holds of Majora's Mask's stray fairies and trade items is Termina's own (the shared save),
// which a Song of Time takes away: another cycle's are not this player's to have (MsSharedJson::DropCycleBound).
bool MergeMergeShipPart(const json& j, bool takeCycleBound) {
    if (!j.is_object()) {
        return false;
    }
    bool changed = false;
    const bool hadRazor = (gMergeShipSave.mmEquipOwned & (1 << MMEQUIP_RAZOR)) != 0;
    changed |= OrInto(gMergeShipSave.ownedMasks, Num(j, "ownedMasks"));
    changed |= OrInto(gMergeShipSave.ownedMMMasks, Num(j, "ownedMMMasks"));
    changed |= OrInto(gMergeShipSave.mmEquipOwned, Num(j, "mmEquipOwned"));
    changed |= OrInto(gMergeShipSave.mmSongs, Num(j, "mmSongs"));
    changed |= OrInto(gMergeShipSave.mmQuestItems, Num(j, "mmQuestItems"));
    changed |= OrInto(gMergeShipSave.owlActivationFlags, Num(j, "owlActivationFlags"));
    const json& fairies = ArrayOf(j, "strayFairies");
    for (size_t i = 0; takeCycleBound && (i < ARRAY_COUNT(gMergeShipSave.strayFairies)) && (i < fairies.size()); i++) {
        changed |= MaxInto(gMergeShipSave.strayFairies[i], Num(fairies[i]));
    }
    // The Razor Sword's hits left come with the sword: a player who had it keeps their own count.
    if (!hadRazor && (gMergeShipSave.mmEquipOwned & (1 << MMEQUIP_RAZOR)) && j.contains("mmRazorHealth")) {
        gMergeShipSave.mmRazorHealth = (u8)Num(j, "mmRazorHealth");
    }

    // Majora's Mask's items without an Ocarina of Time slot: the keg and the box gained, the keg's ammo raised (as the
    // shared save's counts are), an empty trade slot takes the team's, every trade item owned is ORed.
    // The keg's count is this player's own (BUG-0105): a keg new here arrives as one.
    if ((gMergeShipMMItemsSave.powderKeg == ITEM_NONE) && (Num(j, "powderKeg") == ITEM_MM_POWDER_KEG)) {
        gMergeShipMMItemsSave.powderKeg = ITEM_MM_POWDER_KEG;
        gMergeShipMMItemsSave.powderKegAmmo = MERGESHIP_POWDER_KEG_MAX_AMMO;
        changed = true;
    }
    if ((gMergeShipMMItemsSave.pictographBox == ITEM_NONE) && (Num(j, "pictographBox") == ITEM_MM_PICTOGRAPH_BOX)) {
        gMergeShipMMItemsSave.pictographBox = ITEM_MM_PICTOGRAPH_BOX;
        changed = true;
    }
    if (takeCycleBound) {
        changed |= OrInto(gMergeShipMMItemsSave.tradeOwned, Num(j, "tradeOwned") & kMMTradeOwnedBits);
        changed |= FillTradeSlot(gMergeShipMMItemsSave.tradeDeed, Num(j, "tradeDeed"), ITEM_MM_MOONS_TEAR,
                                 ITEM_MM_DEED_OCEAN);
        changed |= FillTradeSlot(gMergeShipMMItemsSave.tradeKeyMama, Num(j, "tradeKeyMama"), ITEM_MM_ROOM_KEY,
                                 ITEM_MM_LETTER_TO_MAMA);
        changed |= FillTradeSlot(gMergeShipMMItemsSave.tradeCouple, Num(j, "tradeCouple"), ITEM_MM_LETTER_TO_KAFEI,
                                 ITEM_MM_PENDANT_OF_MEMORIES);
    }
    return changed;
}

// ---- Ocarina of Time's SaveContext --------------------------------------------------------------------------------

// Switch flags that stay the receiver's: where this player's own copy of the room is (Water Temple's water level,
// Forest Temple's elevator, the collapse timer), not progress.
u32 KeepLocalSwitchFlags(s32 scene) {
    switch (scene) {
        case SCENE_WATER_TEMPLE:
            return (1u << 0x1C) | (1u << 0x1D) | (1u << 0x1E);
        case SCENE_FOREST_TEMPLE:
            return 1u << 0x1B;
        case SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR:
            return 1u << 0x17;
        default:
            return 0;
    }
}

// The item a slot keeps when both players have one: the Ocarina of Time over the Fairy Ocarina, the Longshot over the
// Hookshot. Any other slot (the trade items; the bottles have their own rule, MergeBottleSlot) stays the receiver's
// own.
bool BetterInSlot(size_t slot, u8 ours, u8 theirs) {
    if (slot == SLOT_OCARINA) {
        return (ours == ITEM_OCARINA_FAIRY) && (theirs == ITEM_OCARINA_TIME);
    }
    if (slot == SLOT_HOOKSHOT) {
        return (ours == ITEM_HOOKSHOT) && (theirs == ITEM_LONGSHOT);
    }
    return false;
}

// Keeps the earlier of two times (0: not set): the team's file is as old as its oldest copy, and vanilla effects
// seeded by it (EnemyRandomizer, MirroredWorld, cosmetics) are then the same for everyone.
void KeepEarliest(uint64_t& field, int64_t theirs) {
    if ((theirs > 0) && ((field == 0) || ((uint64_t)theirs < field))) {
        field = (uint64_t)theirs;
    }
}

// Merges a teammate's SaveContext part into gSaveContext (and the live scene flags). A file is loaded. True when
// anything changed.
bool MergeOoTState(const json& state, bool join, bool relayed) {
    bool changed = false;
    const json& inventory = ObjectOf(state, "inventory");

    // Life and magic: raised, never lowered. A teammate's 4th Piece of Heart not yet made a container (its textbox was
    // open when the state was taken: z_message_PAL.c converts it as the textbox closes) counts as that container: 4
    // pieces taken as they are would never become one here.
    const s16 capacityBefore = gSaveContext.healthCapacity;
    const u32 theirQuest = (u32)Num(inventory, "questItems");
    const u32 theirPieceCount = (theirQuest >> 28) % 4;
    const int64_t theirCapacity = std::min<int64_t>(
        Num(state, "healthCapacity") + 0x10 * (int64_t)((theirQuest >> 28) / 4), MS_SHARED_MAX_HEALTH_CAPACITY);
    if (theirCapacity > gSaveContext.healthCapacity) {
        gSaveContext.health = (s16)(gSaveContext.health + (theirCapacity - gSaveContext.healthCapacity));
        gSaveContext.healthCapacity = (s16)theirCapacity;
        changed = true;
    }
    changed |= MaxInto(gSaveContext.magicLevel, Num(state, "magicLevel"));
    if (MaxInto(gSaveContext.magicCapacity, Num(state, "magicCapacity"))) {
        gSaveContext.magic = (s8)gSaveContext.magicCapacity;
        changed = true;
    }
    changed |= OrInto(gSaveContext.isMagicAcquired, Num(state, "isMagicAcquired"));
    changed |= OrInto(gSaveContext.isDoubleMagicAcquired, Num(state, "isDoubleMagicAcquired"));
    changed |= OrInto(gSaveContext.isDoubleDefenseAcquired, Num(state, "isDoubleDefenseAcquired"));
    changed |= OrInto(gSaveContext.bgsFlag, Num(state, "bgsFlag"));
    changed |= MaxInto(gSaveContext.swordHealth, Num(state, "swordHealth"));

    // The randomizer's team counters. The quest itself and the silver rupee counters (which the state does not carry)
    // are this file's own.
    if (IS_RANDO) {
        const json& rando = ObjectOf(ObjectOf(ObjectOf(ObjectOf(state, "ship"), "quest"), "data"), "randomizer");
        changed |= MaxInto(gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected,
                           Num(rando, "triforcePiecesCollected"));
        changed |= MaxInto(gSaveContext.ship.quest.data.randomizer.bombchuUpgradeLevel,
                           Num(rando, "bombchuUpgradeLevel"));
    }

    // Scene flags: chests, switches, clears and collectibles ORed, for the saved scenes and the live one. The map data
    // (unk, rooms, floors) is each player's own.
    const json& sceneFlags = ArrayOf(state, "sceneFlags");
    for (s32 i = 0; (i < (s32)ARRAY_COUNT(gSaveContext.sceneFlags)) && ((size_t)(i * 4 + 3) < sceneFlags.size());
         i++) {
        const u32 chest = (u32)Num(sceneFlags[i * 4]);
        const u32 swch = (u32)Num(sceneFlags[i * 4 + 1]) & ~KeepLocalSwitchFlags(i);
        const u32 clear = (u32)Num(sceneFlags[i * 4 + 2]);
        const u32 collect = (u32)Num(sceneFlags[i * 4 + 3]);
        changed |= OrInto(gSaveContext.sceneFlags[i].chest, chest);
        changed |= OrInto(gSaveContext.sceneFlags[i].swch, swch);
        changed |= OrInto(gSaveContext.sceneFlags[i].clear, clear);
        changed |= OrInto(gSaveContext.sceneFlags[i].collect, collect);
        if (gPlayState->sceneNum == i) {
            changed |= OrInto(gPlayState->actorCtx.flags.chest, chest);
            changed |= OrInto(gPlayState->actorCtx.flags.swch, swch);
            changed |= OrInto(gPlayState->actorCtx.flags.clear, clear);
            changed |= OrInto(gPlayState->actorCtx.flags.collect, collect);
        }
    }

    changed |= OrArray(gSaveContext.eventChkInf, ArrayOf(state, "eventChkInf"));
    changed |= OrArray(gSaveContext.itemGetInf, ArrayOf(state, "itemGetInf"));
    // Skip last row of infTable, don't want to sync swordless flag
    changed |= OrArray(gSaveContext.infTable, ArrayOf(state, "infTable"), 29);
    changed |= OrArray(gSaveContext.gsFlags, ArrayOf(state, "gsFlags"));

    const json& ship = ObjectOf(state, "ship");
    changed |= OrArray(gSaveContext.ship.randomizerInf, ArrayOf(ship, "randomizerInf"));
    const json& stats = ObjectOf(ship, "stats");
    changed |= OrArray(gSaveContext.ship.stats.entrancesDiscovered, ArrayOf(stats, "entrancesDiscovered"));
    KeepEarliest(gSaveContext.ship.stats.firstInput, Num(stats, "firstInput"));
    KeepEarliest(gSaveContext.ship.stats.fileCreatedAt, Num(stats, "fileCreatedAt"));

    // Ensure ganon barrier state matches trials
    if (gSaveContext.eventChkInf[10] & 0x2000 && gSaveContext.eventChkInf[11] & 0xFC00) {
        gSaveContext.eventChkInf[12] |= 0x8;
    }

    // Inventory.
    std::vector<size_t> newSlots;
    const json& items = ArrayOf(inventory, "items");
    for (size_t slot = 0; (slot < ARRAY_COUNT(gSaveContext.inventory.items)) && (slot < items.size()); slot++) {
        const int64_t theirs = Num(items[slot]);
        u8& ours = gSaveContext.inventory.items[slot];
        if ((theirs == ITEM_NONE) || (theirs == ours) || (theirs < 0) || (theirs > 0xFF)) {
            continue;
        }
        if ((slot >= SLOT_BOTTLE_1) && (slot <= SLOT_BOTTLE_4)) {
            changed |= MergeBottleSlot(ours, (u8)theirs);
            continue;
        }
        if ((ours == ITEM_NONE) || BetterInSlot(slot, ours, (u8)theirs)) {
            if (ours == ITEM_NONE) {
                newSlots.push_back(slot); // BUG-0105: a counted item arrives full (below)
            }
            ours = (u8)theirs;
            changed = true;
        }
    }

    // Upgrades: each level raised (before the ammo, which they hold).
    const u32 theirUpgrades = (u32)Num(inventory, "upgrades");
    for (s32 upgrade = 0; upgrade < (s32)ARRAY_COUNT(gUpgradeMasks); upgrade++) {
        const u32 ours = (gSaveContext.inventory.upgrades & gUpgradeMasks[upgrade]) >> gUpgradeShifts[upgrade];
        const u32 theirs = (theirUpgrades & gUpgradeMasks[upgrade]) >> gUpgradeShifts[upgrade];
        if (theirs > ours) {
            gSaveContext.inventory.upgrades =
                (gSaveContext.inventory.upgrades & ~gUpgradeMasks[upgrade]) | (theirs << gUpgradeShifts[upgrade]);
            changed = true;
        }
    }

    // Ammo is each player's own (BUG-0105, 2026-10-12: "no shared ammo period"): no teammate's count is taken. A counted
    // item the team's state just gave arrives full, as a found one does.
    for (size_t slot : newSlots) {
        switch (gSaveContext.inventory.items[slot]) {
            case ITEM_BOW:
                gSaveContext.inventory.ammo[slot] = (s8)CUR_CAPACITY(UPG_QUIVER);
                break;
            case ITEM_BOMB:
                gSaveContext.inventory.ammo[slot] = (s8)CUR_CAPACITY(UPG_BOMB_BAG);
                break;
            case ITEM_BOMBCHU:
                gSaveContext.inventory.ammo[slot] = (s8)MergeShip_Own_ChuCapacity();
                break;
            case ITEM_STICK:
                gSaveContext.inventory.ammo[slot] = (s8)CUR_CAPACITY(UPG_STICKS);
                break;
            case ITEM_NUT:
                gSaveContext.inventory.ammo[slot] = (s8)CUR_CAPACITY(UPG_NUTS);
                break;
            case ITEM_SLINGSHOT:
                gSaveContext.inventory.ammo[slot] = (s8)CUR_CAPACITY(UPG_BULLET_BAG);
                break;
            case ITEM_BEAN:
                gSaveContext.inventory.ammo[slot] = 10;
                break;
            default:
                break;
        }
    }

    // Swords, shields, tunics and boots owned (the sender left out MM gear's placeholders). The broken Giant's Knife
    // bit is not something owned but the state of this player's own Giant's Knife (a break toggles it, z_parameter.c;
    // Medigoron's repair clears it): a teammate's broken blade must not break this player's working one, nor a repair
    // here be undone. It comes only with the teammate's knife to a player with no Giant's Knife at all (a slot held
    // only for MM's Gilded Sword is none), who gets it as the teammate has it.
    const u16 giantsKnife = (u16)OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
    const u16 brokenKnife = (u16)OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);
    const bool hadKnife = ((gSaveContext.inventory.equipment & (giantsKnife | brokenKnife)) != 0) &&
                          !(gMergeShipSave.mmEquipSlotOnlyMM & (1 << MMEQUIP_SLOT_BIGGORON));
    const int64_t theirEquipment = Num(inventory, "equipment");
    changed |= OrInto(gSaveContext.inventory.equipment, hadKnife ? (theirEquipment & ~(int64_t)brokenKnife)
                                                                 : theirEquipment);
    // The Biggoron Sword (bgsFlag, raised above) never breaks: a broken knife it replaces is gone, as when
    // Item_Give(ITEM_SWORD_BGS) repairs it.
    if (gSaveContext.bgsFlag && (gSaveContext.inventory.equipment & brokenKnife)) {
        gSaveContext.inventory.equipment &= (u16)~brokenKnife;
        if (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KNIFE) {
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
            Interface_LoadItemIcon1(gPlayState, 0);
        }
        changed = true;
    }

    // Quest status ORed; the pieces of heart follow the containers: the larger capacity's count, or the larger count
    // (theirPieceCount: a 4th piece is in theirCapacity, above).
    changed |= OrInto(gSaveContext.inventory.questItems, theirQuest & 0x0FFFFFFF);
    const u32 ourPieces = gSaveContext.inventory.questItems & 0xF0000000;
    const u32 theirPieces = theirPieceCount << 28;
    if ((theirPieces != ourPieces) && ((theirCapacity > capacityBefore) ||
                                       ((theirCapacity == capacityBefore) && (theirPieces > ourPieces)))) {
        gSaveContext.inventory.questItems = (gSaveContext.inventory.questItems & 0x0FFFFFFF) | theirPieces;
        changed = true;
    }

    changed |= OrArray(gSaveContext.inventory.dungeonItems, ArrayOf(inventory, "dungeonItems"));
    // Small keys go up and down as doors open (UPDATE_DUNGEON_ITEMS keeps them in step while playing): a joiner takes
    // the team's, anyone else keeps their own. Raised only, never lowered: the answer can be the server's stored state,
    // which may be this player's own from before the keys found since (a returning player), or a stale relayed copy.
    // A relayed part (Termina's engine sending Hyrule's last snapshot, MS_ANCHOR_KEY_OOT_RELAYED) still counts keys
    // spent since: it only gives keys for a dungeon the joiner has none of.
    if (join) {
        const json& keys = ArrayOf(inventory, "dungeonKeys");
        for (size_t i = 0; (i < ARRAY_COUNT(gSaveContext.inventory.dungeonKeys)) && (i < keys.size()); i++) {
            if (relayed && (gSaveContext.inventory.dungeonKeys[i] > 0)) {
                continue;
            }
            changed |= MaxInto(gSaveContext.inventory.dungeonKeys[i], (int64_t)(s8)Num(keys[i]));
        }
    }
    changed |= MaxInto(gSaveContext.inventory.defenseHearts, Num(inventory, "defenseHearts"));
    changed |= MaxInto(gSaveContext.inventory.gsTokens, Num(inventory, "gsTokens"));

    changed |= TakeRealOoTGear(theirEquipment, items);

    // Randomizer checks: each status raised in RandomizerCheckStatus's order (unchecked .. collected, saved), skips
    // ORed. Only what changes is set, so the hooks (MergeShipUnified's ledger) see only real changes.
    const json& locations = ArrayOf(ObjectOf(state, "rando"), "itemLocations");
    if (IS_RANDO && !locations.empty()) {
        auto randoContext = Rando::Context::GetInstance();
        bool checksChanged = false;
        for (size_t i = 0; (i < (size_t)RC_MAX) && (i < locations.size()); i++) {
            const json& location = locations[i];
            if (!location.is_array() || (location.size() < 2)) {
                continue;
            }
            Rando::ItemLocation* itemLocation = randoContext->GetItemLocation(i);
            const int64_t status = Num(location[0]);
            if ((status > (int64_t)itemLocation->GetCheckStatus()) && (status <= (int64_t)RCSHOW_SAVED)) {
                itemLocation->SetCheckStatus((RandomizerCheckStatus)status);
                checksChanged = true;
            }
            if ((Num(location[1]) != 0) && !itemLocation->GetIsSkipped()) {
                itemLocation->SetIsSkipped(true);
                checksChanged = true;
            }
        }
        if (checksChanged) {
            // As SET_CHECK_STATUS does (Packets/SetCheckStatus.cpp), once for the whole state.
            CheckTracker::RecalculateAllAreaTotals();
            CheckTracker::RecalculateAvailableChecks();
            changed = true;
        }
    }

    return changed;
}

} // namespace

// MergeShip2: the "state" of UPDATE_TEAM_STATE, apart from sending it: in mergeship.exe the engine of the other world
// sends the team state while the player is there, with this world's latest part (MergeShipAnchorHost.cpp).
json Anchor::PrepTeamState() {
    json state = gSaveContext;
    // MergeShip2: the server keeps only "state": the playthrough goes inside it (AnchorNet.h MS_ANCHOR_KEY_SEED).
    state[MS_ANCHOR_KEY_SEED] = MergeShipAnchor_LocalSeed();
    // Whose state it is (AnchorNet.h MS_ANCHOR_KEY_OWNER): the server's stored copy may come back to its own player.
    state[MS_ANCHOR_KEY_OWNER] = MergeShip_Save_SharedFileId();
    // manually update current scene flags
    state["sceneFlags"][gPlayState->sceneNum * 4] = gPlayState->actorCtx.flags.chest;
    state["sceneFlags"][gPlayState->sceneNum * 4 + 1] = gPlayState->actorCtx.flags.swch;
    state["sceneFlags"][gPlayState->sceneNum * 4 + 2] = gPlayState->actorCtx.flags.clear;
    state["sceneFlags"][gPlayState->sceneNum * 4 + 3] = gPlayState->actorCtx.flags.collect;

    // MergeShip2: Ocarina of Time's items only (a slot held for MM gear is not one), and MergeShip's own progress.
    StripMMPlaceholders(state["inventory"]);
    state[MS_ANCHOR_KEY_MERGESHIP_TEAM_STATE] = MergeShipPart();

    // MergeShip2: a 4th Piece of Heart whose textbox is still open (z_message_PAL.c makes it a container as the textbox
    // closes) goes out as the container it is becoming: a teammate never holds 4 pieces that nothing converts.
    const u32 pieces = gSaveContext.inventory.questItems >> 28;
    if (pieces >= 4) {
        state["healthCapacity"] = (s16)std::min<s32>(gSaveContext.healthCapacity + 0x10 * (s32)(pieces / 4),
                                                     MS_SHARED_MAX_HEALTH_CAPACITY);
        state["inventory"]["questItems"] = (u32)((gSaveContext.inventory.questItems & 0x0FFFFFFF) | ((pieces % 4) << 28));
    }

    // The commented out code below is an attempt at sending the entire randomizer seed over, in hopes that a player
    // doesn't have to generate the seed themselves Currently it doesn't work :)
    if (IS_RANDO) {
        auto randoContext = Rando::Context::GetInstance();

        state["rando"] = json::object();
        state["rando"]["itemLocations"] = json::array();
        for (int i = 0; i < RC_MAX; i++) {
            state["rando"]["itemLocations"][i] = json::array();
            // state["rando"]["itemLocations"][i]["rgID"] =
            // randoContext->GetItemLocation(i)->GetPlacedRandomizerGet();
            state["rando"]["itemLocations"][i][0] = randoContext->GetItemLocation(i)->GetCheckStatus();
            state["rando"]["itemLocations"][i][1] = (u8)randoContext->GetItemLocation(i)->GetIsSkipped();

            // if (randoContext->GetItemLocation(i)->GetPlacedRandomizerGet() == RG_ICE_TRAP) {
            //     state["rando"]["itemLocations"][i]["fakeRgID"] =
            //     randoContext->GetItemOverride(i).LooksLike();
            //     state["rando"]["itemLocations"][i]["trickName"] = json::object();
            //     state["rando"]["itemLocations"][i]["trickName"]["english"] =
            //     randoContext->GetItemOverride(i).GetTrickName().GetEnglish();
            //     state["rando"]["itemLocations"][i]["trickName"]["french"] =
            //     randoContext->GetItemOverride(i).GetTrickName().GetFrench();
            // }
            // if (randoContext->GetItemLocation(i)->HasCustomPrice()) {
            //     state["rando"]["itemLocations"][i]["price"] =
            //     randoContext->GetItemLocation(i)->GetPrice();
            // }
        }

        // auto entranceCtx = randoContext->GetEntranceShuffler();
        // for (int i = 0; i < ENTRANCE_OVERRIDES_MAX_COUNT; i++) {
        //     state["rando"]["entrances"][i] = json::object();
        //     state["rando"]["entrances"][i]["type"] = entranceCtx->entranceOverrides[i].type;
        //     state["rando"]["entrances"][i]["index"] = entranceCtx->entranceOverrides[i].index;
        //     state["rando"]["entrances"][i]["destination"] = entranceCtx->entranceOverrides[i].destination;
        //     state["rando"]["entrances"][i]["override"] = entranceCtx->entranceOverrides[i].override;
        //     state["rando"]["entrances"][i]["overrideDestination"] =
        //     entranceCtx->entranceOverrides[i].overrideDestination;
        // }

        // state["rando"]["seed"] = json::array();
        // for (int i = 0; i < randoContext->hashIconIndexes.size(); i++) {
        //     state["rando"]["seed"][i] = randoContext->hashIconIndexes[i];
        // }
        // state["rando"]["inputSeed"] = randoContext->GetSeedString();
        // state["rando"]["finalSeed"] = randoContext->GetSeed();

        // state["rando"]["randoSettings"] = json::array();
        // for (int i = 0; i < RSK_MAX; i++) {
        //     state["rando"]["randoSettings"][i] =
        //     randoContext->GetOption((RandomizerSettingKey(i))).GetSelectedOptionIndex();
        // }

        // state["rando"]["masterQuestDungeonCount"] = randoContext->GetDungeons()->CountMQ();
        // state["rando"]["masterQuestDungeons"] = json::array();
        // for (int i = 0; i < randoContext->GetDungeons()->GetDungeonListSize(); i++) {
        //     state["rando"]["masterQuestDungeons"][i] = randoContext->GetDungeon(i)->IsMQ();
        // }
        // for (int i = 0; i < randoContext->GetTrials()->GetTrialListSize(); i++) {
        //     state["rando"]["requiredTrials"][i] = randoContext->GetTrial(i)->IsRequired();
        // }
    }

    return state;
}

void Anchor::SendPacket_UpdateTeamState(bool captureShared) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    json payload;
    payload["type"] = UPDATE_TEAM_STATE;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");

    // Assume the team queue has been emptied, so clear it
    payload["queue"] = json::array();

    payload["state"] = PrepTeamState();
    // MergeShip2: one team state on the server holds both worlds: Termina's part goes along with Hyrule's.
    json mm = MergeShipAnchor_MMTeamState(this);
    if (mm.is_object() && !mm.empty()) {
        // Never live here: Termina's engine's last snapshot (AnchorNet.h MS_ANCHOR_KEY_RELAYED).
        mm[MS_ANCHOR_KEY_RELAYED] = true;
        payload["state"][MS_ANCHOR_KEY_MM_TEAM_STATE] = mm;
    }
#ifdef MERGESHIP_HOSTED
    // MergeShip2: and the shared save's progress (both worlds' items held in the other, ownership and the ammo pool,
    // bottles, the team's Termina checks), captured first (captureShared) so the copy is this moment's. Every send runs
    // on the game thread, the only one that touches the shared save (a save's send waits for the next frame).
    if (MsSharedSave* shared = TeamSharedSave()) {
        if (captureShared) {
            MergeShipShared_Capture(shared);
            MergeShipHost_CommitSharedSave();
        }
        payload["state"][MS_ANCHOR_KEY_SHARED_TEAM_STATE] = MsSharedJson::Write(*shared);
    }
#else
    (void)captureShared;
#endif

    SendJsonToRemote(payload);
}

void Anchor::SendPacket_ClearTeamState(std::string teamId) {
    json payload;
    payload["type"] = UPDATE_TEAM_STATE;
    payload["targetTeamId"] = teamId;
    payload["queue"] = json::array();
    payload["state"] = json::object();
    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_UpdateTeamState(nlohmann::json payload) {
    if (!roomState.syncItemsAndFlags) {
        return;
    }

    isHandlingUpdateTeamState = true;
    // MergeShip2: cleared however this returns (a malformed state throws, and the flag used to stay set for good).
    struct ClearFlag {
        bool& flag;
        ~ClearFlag() {
            flag = false;
        }
    } clearFlag{ isHandlingUpdateTeamState };

    // MergeShip2: an answer to this player's own request (arrived within kMergeShipJoinWindowMs after it; every teammate
    // online answers): a joiner, who takes the team's small keys and, from the shared save, its rupees and magic --
    // raised only, never lowered.
    const bool join = MergeShipIsJoinAnswer(payload);
    const json& state = ObjectOf(payload, "state");

    // MergeShip2: a state of another playthrough is never merged (AnchorNet.h MS_ANCHOR_KEY_SEED: the server keeps a
    // room's last team state for as long as the room lives). Its queue is still replayed below: the server keeps it
    // apart from the state, and every queued packet carries its own playthrough, checked as it is handled
    // (MergeShipAnchor_SeedAccepts).
    const uint32_t localSeed = MergeShipAnchor_LocalSeed();
    const bool otherSeed = !state.empty() && !MsAnchor_SeedMatches(state, localSeed);
    // The answer to our own request was another playthrough's (the server's stored state of an earlier seed, or a
    // teammate still on one): ours becomes the team's, so whoever loads this seed next gets this seed's. Once per
    // request: the window stays open for the answers of teammates on this seed.
    bool pushOwnState = false;
    if (otherSeed) {
        const uint32_t theirs = MsAnchor_PacketSeed(state);
        SPDLOG_INFO("[Anchor] MergeShip2: UPDATE_TEAM_STATE state of seed {} ignored (this file's seed is {})", theirs,
                    localSeed);
        MergeShipTest_Log("ANCHOR UPDATE_TEAM_STATE state of seed %u ignored (local seed %u), its queue replayed", theirs,
                          localSeed);
        pushOwnState = join && !mergeShipPushedOwnTeamState.exchange(true);
    }

    // MergeShip2: merged only into a loaded file. A team state that arrives at the title screen, file select or a scene
    // load waits for one (Anchor::ProcessIncomingPacketQueue), so this is the normal case; written at file select, the
    // file's load used to overwrite it, and the joiner lost the team's state. Our own state echoed back (the other
    // world's engine answering for this player, on the same client id) is already ours.
    if (IsSaveLoaded() && !state.empty() && !otherSeed && !MergeShipAnchor_IsOwnEcho(payload)) {
        bool changed = false;
        // In mergeship.exe what Hyrule holds of Termina's three-day state (stray fairies, trade items, bottle contents)
        // is Termina's, which a Song of Time takes away: a teammate's from another cycle is not this player's to have
        // (MsSharedJson::DropCycleBound). soh.exe has no Termina cycle.
        const bool takeCycleBound =
            !MergeShipHost_IsHosted() || MergeShipAnchor_SameTerminaCycle(ObjectOf(state, MS_ANCHOR_KEY_MM_TEAM_STATE));

        // Termina's part is Termina's engine's: kept (with whose it is) to send along with ours.
        if (state.contains(MS_ANCHOR_KEY_MM_TEAM_STATE) && state[MS_ANCHOR_KEY_MM_TEAM_STATE].is_object()) {
            const uint32_t fileId = MergeShip_Save_SharedFileId();
            const uint32_t seed = MergeShipAnchor_LocalSeed();
            std::lock_guard<std::mutex> lock(mergeShipMMTeamStateMutex);
            mergeShipMMTeamState = state[MS_ANCHOR_KEY_MM_TEAM_STATE];
            mergeShipMMTeamStateFileId = fileId;
            mergeShipMMTeamStateSeed = seed;
            // BUG-0035: the owl statues a teammate struck in Termina are Hyrule's to soar to as well (the Song of Soaring
            // map, MergeShipTermina.h) -- persistent, as in Termina (2Ship UpdateTeamState: owlActivationFlags |=).
            const json& mm = state[MS_ANCHOR_KEY_MM_TEAM_STATE];
            if (mm.contains("owlActivationFlags") && mm["owlActivationFlags"].is_number()) {
                const uint16_t owls = mm["owlActivationFlags"].get<uint16_t>();
                if ((gMergeShipSave.owlActivationFlags | owls) != gMergeShipSave.owlActivationFlags) {
                    gMergeShipSave.owlActivationFlags |= owls;
                    changed = true;
                }
            }
        }

        // Hyrule's part may be missing (a team state only Termina has written yet).
        // This player's own older state (the server's stored copy, AnchorNet.h MS_ANCHOR_KEY_OWNER) gives no
        // spent-and-refilled counters: its own are newer.
        const uint32_t ownFileId = MergeShip_Save_SharedFileId();
        const bool ownState = (ownFileId != 0) && (Num(state, MS_ANCHOR_KEY_OWNER) == (int64_t)ownFileId);
        const bool takeCounters = join && !ownState;
        if (state.contains("inventory")) {
            changed |= MergeOoTState(state, takeCounters, state.value(MS_ANCHOR_KEY_OOT_RELAYED, false));
        }
        changed |= MergeMergeShipPart(ObjectOf(state, MS_ANCHOR_KEY_MERGESHIP_TEAM_STATE), takeCycleBound);

#ifdef MERGESHIP_HOSTED
        // The shared save is one per process: merged once (by the engine that handles the packet first, the one the
        // player is in), into this moment's copy (captured after the merges above), and applied back to Hyrule.
        json sharedPart = ObjectOf(state, MS_ANCHOR_KEY_SHARED_TEAM_STATE);
        if (!takeCycleBound) {
            MsSharedJson::DropCycleBound(sharedPart);
        }
        MsSharedSave* shared = TeamSharedSave();
        if (!sharedPart.empty() && (shared != nullptr) && MergeShipAnchor_ClaimShared(payload)) {
            MergeShipShared_Capture(shared);
            if (MsSharedJson::Merge(*shared, sharedPart, takeCounters)) {
                MergeShipShared_Apply(shared);
                changed = true;
            }
            MergeShipHost_CommitSharedSave();
        }
#endif

        // MergeShip2: told once, when joining brought something new (a teammate's save merging in is silent: what it
        // carries came live already).
        if (join && changed) {
            Notification::Emit({
                .message = "Save updated from team",
            });
        }
    }

    // MergeShip2: the queue is replayed into this engine's own (handled from the next frame, in order). Each packet is
    // numbered from its own text (MsAnchor_ReplaySeq, AnchorReplay.hpp), the same in Termina's engine, which replays
    // the same queue: the server hands it to every request until someone's team state clears it (SoH's and 2Ship's
    // own requests, a reconnect's), so the same queued packet is applied once per process however many replies carry
    // it (Claim for items; Anchor::mergeShipReplayed for the rest). Numbered from the delivery as before, each reply
    // gave it a new number and its item was given again.
    size_t replayedOfThisSeed = 0;
    if (payload.contains("queue") && payload["queue"].is_array()) {
        std::unordered_map<std::string, uint32_t> occurrences; // identical earlier entries: two finds of one item
        std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
        for (const auto& item : payload["queue"]) {
            if (!item.is_string()) {
                continue;
            }
            const std::string& text = item.get_ref<const std::string&>();
            const uint32_t occurrence = occurrences[text]++;
            nlohmann::json itemPayload = nlohmann::json::parse(text, nullptr, false);
            if (itemPayload.is_discarded() || !itemPayload.is_object() || !itemPayload.contains("type") ||
                !itemPayload["type"].is_string()) {
                continue; // MergeShip2: a malformed entry is skipped, not the rest of the queue
            }
            itemPayload[MS_ANCHOR_KEY_SEQ] = MsAnchor_ReplaySeq(text, occurrence);
            if (MsAnchor_SeedMatches(itemPayload, localSeed)) {
                replayedOfThisSeed++;
            }
            incomingPacketQueue.push(std::move(itemPayload));
        }
        // After them, our team state, holding them by then, goes out (Anchor::ProcessIncomingPacketQueue): the server
        // clears the queue. Only for a queue with this playthrough's packets: clearing another seed's would take them
        // from the offline players of that seed.
        if (replayedOfThisSeed > 0) {
            incomingPacketQueue.push(json{ { "type", kMergeShipReplayDone } });
        }
    }
    if (pushOwnState && (replayedOfThisSeed == 0)) {
        SendPacket_UpdateTeamState(true); // (with a replay, the state sent after it is ours already)
    }
}
