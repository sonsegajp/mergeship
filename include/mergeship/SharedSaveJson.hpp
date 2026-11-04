// MergeShip2 -- the shared save's progress in the Anchor team state ("state"."mergeShipShared", AnchorNet.h
// MS_ANCHOR_KEY_SHARED_TEAM_STATE), and the merge a teammate's copy gets. Header-only C++ (nlohmann::json), used by
// both engines' Anchor.
//
// The team is one playthrough (the user, 2026-09-30: "when anyone joins, they should get EXACTLY what any player has.
// DOWN to a T"). The shared save holds what belongs to both worlds -- Majora's Mask's items, masks, songs and gear
// held in Hyrule, Ocarina of Time's held in Termina, which game owns the bombs .. the bow and their one ammo pool, the
// bottles, the team's Termina checks -- so without it a joiner got Hyrule's SaveContext only: no MM ocarina, no MM
// masks, and bombs hidden because nothing said they were owned.
//
// The merge only ever adds: bits are ORed, levels, capacities and counts raised, an empty slot (bottle, trade slot)
// takes the team's; nothing a player has is taken away, so applying any teammate's copy, in any order, any number of
// times, converges on the union. What both sides already have of a slot stays the receiver's own (its bottle's
// content, its trade item). Counters that go down as they are spent (rupees, magic) are taken only on `join` (the
// answer to the receiver's own request: it has nothing to lose yet). The ledger is not carried (each file's own, as
// the projectors' counts are), nor the goal (SoH writes it from the seed) or Hyrule's equipped-gear block (SoH's own
// view, written by its capture).
#pragma once

#include <mergeship/MsDelivery.h>
#include <mergeship/SharedSave.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace MsSharedJson {

template <typename T, size_t N> inline std::vector<int> Array(const T (&values)[N]) {
    std::vector<int> out;
    out.reserve(N);
    for (size_t i = 0; i < N; i++) {
        out.push_back((int)values[i]);
    }
    return out;
}

inline nlohmann::json Write(const MsSharedSave& s) {
    nlohmann::json j;
    j["itemsModel"] = s.itemsModel;
    j["healthCapacity"] = s.healthCapacity;
    j["heartPieces"] = std::min<int>(s.heartPieces, 3); // a 4th becomes a container on its own textbox
    j["doubleDefense"] = s.doubleDefense;
    j["magicLevel"] = s.magicLevel;
    j["magicAmount"] = s.magicAmount;
    j["rupees"] = s.rupees;
    j["walletLevel"] = s.walletLevel;
    j["transformationMasks"] = s.transformationMasks;
    j["mmTradeOwned"] = s.mmTradeOwned;
    j["mmMasks"] = s.mmMasks;
    j["items"] = s.items;
    j["levels"] = { s.quiverLevel, s.bombBagLevel, s.stickLevel, s.nutLevel };
    j["counts"] = { s.arrows, s.bombs, s.bombchus, s.sticks, s.nuts, s.magicBeans, s.powderKegs };
    j["bottles"] = Array(s.bottles);
    j["swords"] = s.swords;
    j["shields"] = s.shields;
    j["razorSwordHealth"] = s.razorSwordHealth;
    j["songs"] = s.songs;
    j["remains"] = s.remains;
    j["owlActivationFlags"] = s.owlActivationFlags;
    j["strayFairies"] = Array(s.strayFairies);
    j["mmTrade"] = { s.mmTradeDeed, s.mmTradeKeyMama, s.mmTradeCouple };
    j["hyruleBlock"] = s.hyruleBlock;
    j["hyruleQuest"] = s.hyruleQuest;
    j["hyruleItems"] = s.hyruleItems;
    j["gsTokens"] = s.gsTokens;
    j["hyruleSeeds"] = s.hyruleSeeds;
    j["hyruleTrade"] = { s.hyruleTradeChild, s.hyruleTradeAdult };
    j["terminaGearValid"] = s.terminaGearValid;
    j["terminaSwords"] = s.terminaSwords;
    j["terminaShields"] = s.terminaShields;
    j["terminaSword"] = s.terminaSword;
    j["terminaShield"] = s.terminaShield;
    j["hyruleOwn"] = s.hyruleOwn;
    j["terminaOwn"] = s.terminaOwn;
    j["hyruleBags"] = s.hyruleBags;
    j["terminaBags"] = s.terminaBags;
    j["chuBagLevel"] = MsShared_ChuBagLevel(s.chuBagLevel, s.ownValid);
    j["ammo"] = Array(s.ammo);
    j["ootBottles"] = Array(s.ootBottles);
    j["ledgerSeed"] = s.ledgerSeed;
    if ((s.ledgerSeed != 0) && (s.teamChecksSeed == s.ledgerSeed)) {
        j["teamTerminaChecks"] = Array(s.teamTerminaChecks);
    }
    return j;
}

namespace detail {

template <typename T> inline bool Or(T& field, uint64_t theirs) {
    const T merged = (T)(field | (T)theirs);
    if (merged == field) {
        return false;
    }
    field = merged;
    return true;
}

template <typename T> inline bool Max(T& field, int64_t theirs) {
    if ((int64_t)field >= theirs) {
        return false;
    }
    field = (T)theirs;
    return true;
}

template <typename T> inline bool FillEmpty(T& field, int64_t theirs, int64_t empty) {
    if (((int64_t)field != empty) || (theirs == empty)) {
        return false;
    }
    field = (T)theirs;
    return true;
}

inline uint64_t U(const nlohmann::json& j, const char* key) {
    return (j.contains(key) && j[key].is_number()) ? j[key].get<uint64_t>() : 0;
}

inline std::vector<int64_t> V(const nlohmann::json& j, const char* key) {
    std::vector<int64_t> out;
    if (j.contains(key) && j[key].is_array()) {
        for (const auto& v : j[key]) {
            out.push_back(v.is_number() ? v.get<int64_t>() : 0);
        }
    }
    return out;
}

// Each 2-bit field of a bags byte (MS_BAGS_*) raised to the larger.
inline bool MaxBags(uint8_t& bags, uint64_t theirs) {
    uint8_t merged = bags;
    for (int field = 0; field < 4; field++) {
        const uint8_t level = std::max(MS_BAGS_LEVEL(bags, field), MS_BAGS_LEVEL((uint8_t)theirs, field));
        merged = MS_BAGS_WITH(merged, field, level);
    }
    if (merged == bags) {
        return false;
    }
    bags = merged;
    return true;
}

// The bottle a slot takes from a teammate's `theirs`: an empty one instead when its content is one of a kind per world
// and the receiver's bottles hold it already.
inline int64_t BottleFor(const uint8_t* bottles, size_t count, int64_t theirs) {
    if ((theirs <= MS_BOTTLE_EMPTY) || !MsDelivery_BottleOnePerWorld((int)theirs)) {
        return theirs;
    }
    for (size_t i = 0; i < count; i++) {
        if (bottles[i] == theirs) {
            return MS_BOTTLE_EMPTY;
        }
    }
    return theirs;
}

} // namespace detail

// A teammate's copy from another three-day cycle in Termina (their "mergeShipMM" cycle differs from the receiver's, or
// either is unknown): what a Song of Time takes away is not theirs to give in this cycle -- stray fairies, Termina's
// trade items, bottle contents (the bottles themselves stay, empty). The caller decides; Merge then skips them.
inline void DropCycleBound(nlohmann::json& j) {
    if (!j.is_object()) {
        return;
    }
    j.erase("strayFairies");
    j.erase("mmTrade");
    j.erase("mmTradeOwned");
    if (j.contains("bottles") && j["bottles"].is_array()) {
        for (auto& bottle : j["bottles"]) {
            if (bottle.is_number() && (bottle.get<int64_t>() > MS_BOTTLE_EMPTY)) {
                bottle = (int)MS_BOTTLE_EMPTY;
            }
        }
    }
}

// Merges a teammate's copy `j` (Write) into `s`. True when anything changed. `s` must be the receiver's shared save as
// its engine just captured it (so its own live progress is in it): the caller applies `s` back to its engine after.
inline bool Merge(MsSharedSave& s, const nlohmann::json& j, bool join) {
    using namespace detail;
    bool changed = false;

    if (!j.is_object() || (U(j, "itemsModel") != s.itemsModel)) {
        return false; // another model names the shared items differently (an older build): nothing to merge safely
    }

    // Life and magic. Pieces of heart follow the containers: the larger capacity's count, or the larger count.
    const uint64_t capacity = std::min<uint64_t>(U(j, "healthCapacity"), MS_SHARED_MAX_HEALTH_CAPACITY);
    if (capacity > s.healthCapacity) {
        s.health = (int16_t)(s.health + (int16_t)(capacity - s.healthCapacity));
        s.healthCapacity = (uint16_t)capacity;
        s.heartPieces = (uint8_t)std::min<uint64_t>(U(j, "heartPieces"), 3);
        changed = true;
    } else if (capacity == s.healthCapacity) {
        changed |= Max(s.heartPieces, (int64_t)std::min<uint64_t>(U(j, "heartPieces"), 3));
    }
    changed |= Or(s.doubleDefense, U(j, "doubleDefense"));
    if (Max(s.magicLevel, (int64_t)U(j, "magicLevel"))) {
        changed = true;
        s.magicAmount = (uint8_t)std::max<uint64_t>(s.magicAmount, U(j, "magicAmount"));
    }
    if (join) {
        changed |= Max(s.magicAmount, (int64_t)U(j, "magicAmount"));
        changed |= Max(s.rupees, (int64_t)U(j, "rupees"));
    }
    changed |= Max(s.walletLevel, (int64_t)U(j, "walletLevel"));

    // Masks, items, gear, songs, quest status: only ever gained.
    changed |= Or(s.transformationMasks, U(j, "transformationMasks"));
    changed |= Or(s.mmTradeOwned, U(j, "mmTradeOwned"));
    changed |= Or(s.mmMasks, U(j, "mmMasks"));
    changed |= Or(s.items, U(j, "items"));
    const bool hadRazor = (s.swords & (1u << MS_SWORD_RAZOR)) != 0;
    changed |= Or(s.swords, U(j, "swords"));
    changed |= Or(s.shields, U(j, "shields"));
    if (!hadRazor && (s.swords & (1u << MS_SWORD_RAZOR))) {
        s.razorSwordHealth = (uint8_t)U(j, "razorSwordHealth");
    }
    changed |= Or(s.songs, U(j, "songs"));
    changed |= Or(s.remains, U(j, "remains"));
    changed |= Or(s.owlActivationFlags, U(j, "owlActivationFlags"));
    {
        const std::vector<int64_t> fairies = V(j, "strayFairies");
        for (size_t i = 0; i < fairies.size() && i < sizeof(s.strayFairies); i++) {
            changed |= Max(s.strayFairies[i], fairies[i]);
        }
    }

    // Bags, capacities and the one ammo pool (raised only, as MS_ANCHOR_AMMO_SYNC does: spending is each player's).
    {
        const std::vector<int64_t> levels = V(j, "levels");
        uint8_t* fields[] = { &s.quiverLevel, &s.bombBagLevel, &s.stickLevel, &s.nutLevel };
        for (size_t i = 0; i < levels.size() && i < 4; i++) {
            changed |= Max(*fields[i], levels[i]);
        }
        const std::vector<int64_t> counts = V(j, "counts");
        uint8_t* countFields[] = { &s.arrows, &s.bombs, &s.bombchus, &s.sticks, &s.nuts, &s.magicBeans, &s.powderKegs };
        for (size_t i = 0; i < counts.size() && i < 7; i++) {
            changed |= Max(*countFields[i], counts[i]);
        }
    }
    changed |= Or(s.hyruleOwn, U(j, "hyruleOwn"));
    changed |= Or(s.terminaOwn, U(j, "terminaOwn"));
    changed |= MaxBags(s.hyruleBags, U(j, "hyruleBags"));
    changed |= MaxBags(s.terminaBags, U(j, "terminaBags"));
    {
        const uint8_t chuBag = MsShared_ChuBagLevel(s.chuBagLevel, s.ownValid);
        const uint64_t theirs = U(j, "chuBagLevel");
        if (theirs > chuBag) {
            s.chuBagLevel = (uint8_t)std::min<uint64_t>(theirs, 3);
            s.ownValid |= MS_OWN_VALID_CHU_BAG;
            changed = true;
        }
        const std::vector<int64_t> ammo = V(j, "ammo");
        for (size_t i = 0; i < ammo.size() && i < MS_OWN_AMMO_COUNT; i++) {
            changed |= Max(s.ammo[i], ammo[i]);
        }
    }

    // Bottles and trade slots: an empty one takes the team's; one the receiver has keeps its own content. A content
    // there is one of per world (the Deku Princess, the Seahorse: MsDelivery_BottleOnePerWorld) comes as an empty
    // bottle when the receiver's world has it already -- each player's world keeps its own one.
    {
        const std::vector<int64_t> bottles = V(j, "bottles");
        for (size_t i = 0; i < bottles.size() && i < MS_SHARED_BOTTLE_SLOTS; i++) {
            changed |= FillEmpty(s.bottles[i], BottleFor(s.bottles, MS_SHARED_BOTTLE_SLOTS, bottles[i]), MS_BOTTLE_NONE);
        }
        const std::vector<int64_t> ootBottles = V(j, "ootBottles");
        for (size_t i = 0; i < ootBottles.size() && i < 4; i++) {
            changed |= FillEmpty(s.ootBottles[i], BottleFor(s.ootBottles, 4, ootBottles[i]), MS_BOTTLE_NONE);
        }
        const std::vector<int64_t> mmTrade = V(j, "mmTrade");
        uint8_t* tradeFields[] = { &s.mmTradeDeed, &s.mmTradeKeyMama, &s.mmTradeCouple };
        for (size_t i = 0; i < mmTrade.size() && i < 3; i++) {
            changed |= FillEmpty(*tradeFields[i], mmTrade[i], 0xFF);
        }
    }

    // Hyrule's block (quest status, masks and trade items Termina holds), once someone has written it.
    if (U(j, "hyruleBlock") != 0) {
        changed |= Or(s.hyruleQuest, U(j, "hyruleQuest") & MS_HYRULE_QUEST_BITS);
        const uint32_t theirItems = (uint32_t)U(j, "hyruleItems");
        changed |= Or(s.hyruleItems, theirItems & MS_HYRULE_MASK_BITS);
        if ((s.hyruleItems & (MS_HYRULE_CHILD_TRADE_BITS | MS_HYRULE_ADULT_TRADE_BITS)) == 0) {
            changed |= Or(s.hyruleItems, theirItems & (MS_HYRULE_CHILD_TRADE_BITS | MS_HYRULE_ADULT_TRADE_BITS));
        }
        changed |= Max(s.gsTokens, (int64_t)U(j, "gsTokens"));
        changed |= Max(s.hyruleSeeds, (int64_t)U(j, "hyruleSeeds"));
        const std::vector<int64_t> trade = V(j, "hyruleTrade");
        if (trade.size() >= 2) {
            changed |= FillEmpty(s.hyruleTradeChild, trade[0], MS_HYRULE_SLOT_EMPTY);
            changed |= FillEmpty(s.hyruleTradeAdult, trade[1], MS_HYRULE_SLOT_EMPTY);
        }
        changed |= Or(s.hyruleBlock, U(j, "hyruleBlock"));
    }

    // Termina's own swords and shields, once Termina has written them.
    if (U(j, "terminaGearValid") != 0) {
        changed |= Or(s.terminaSwords, U(j, "terminaSwords"));
        changed |= Or(s.terminaShields, U(j, "terminaShields"));
        changed |= FillEmpty(s.terminaSword, (int64_t)U(j, "terminaSword"), 0);
        changed |= FillEmpty(s.terminaShield, (int64_t)U(j, "terminaShield"), 0);
        changed |= Or(s.terminaGearValid, 1);
    }

    // The team's Termina checks, for the unified seed they belong to.
    if ((s.ledgerSeed != 0) && (U(j, "ledgerSeed") == s.ledgerSeed) && j.contains("teamTerminaChecks")) {
        if (s.teamChecksSeed != s.ledgerSeed) {
            memset(s.teamTerminaChecks, 0, sizeof(s.teamTerminaChecks));
            s.teamChecksSeed = s.ledgerSeed;
            changed = true;
        }
        const std::vector<int64_t> checks = V(j, "teamTerminaChecks");
        for (size_t i = 0; i < checks.size() && i < MS_TEAM_TERMINA_CHECK_BYTES; i++) {
            changed |= Or(s.teamTerminaChecks[i], (uint64_t)checks[i]);
        }
    }

    return changed;
}

} // namespace MsSharedJson
