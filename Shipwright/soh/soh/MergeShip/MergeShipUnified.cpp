// MergeShip2 -- the unified randomizer's deliveries on SoH's side (MergeShipUnified.h, M3).

#include "soh/MergeShip/MergeShipUnified.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <mergeship/MsDelivery.h>
#include <mergeship/MsItems.h>
#include <mergeship/SharedSave.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "soh/cvar_prefixes.h"

#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/randomizerEnumStrings.h"
#include "soh/Network/Anchor/Anchor.h"
#include "soh/MergeShip/MergeShipMsItems.h"
#include "soh/MergeShip/MergeShipTerminaRando.h"
#include "soh/MergeShip/MergeShipHost.h"
#include "soh/MergeShip/MergeShipGoal.h"
#include "soh/MergeShip/MergeShipTermina.h"
#include "soh/MergeShip/MergeShipOwnership.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipMMEquipment.h"
#include "soh/MergeShip/MergeShipSongs.h"
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...);
}

namespace {

// ---- the items SoH cannot hold: the RG_MM_* tools/port_mm_rando.py generated ------------------------------------

#pragma push_macro("RANDO_ENUM_ITEM")
#undef RANDO_ENUM_ITEM
#define RANDO_ENUM_ITEM(x) x,
constexpr RandomizerGet kTerminaOnly[] = {
#include "soh/Enhancements/randomizer/randomizerEnums/RandomizerGetTermina.inc"
};
#undef RANDO_ENUM_ITEM
#pragma pop_macro("RANDO_ENUM_ITEM")

bool IsTerminaOnly(RandomizerGet rg) {
    for (RandomizerGet t : kTerminaOnly) {
        if (t == rg) {
            return true;
        }
    }
    return false;
}

// ---- logs ------------------------------------------------------------------------------------------------------

void Log(const char* fmt, ...) {
    char line[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    SPDLOG_INFO("MergeShip unified: {}", line);
    MergeShipTest_Log("%s", line); // trace.log; no-op without a script
}

const char* WorldName(uint8_t world) {
    return (world == MS_WORLD_HYRULE) ? "hyrule" : (world == MS_WORLD_TERMINA) ? "termina" : "none";
}

const char* KeyName(uint16_t key) {
    const MsItemInfo* info = MsItems_Get((MsItemKey)key);
    return (info != nullptr) ? info->keyName : "?";
}

// ---- state -----------------------------------------------------------------------------------------------------

bool sTestUnified = false;          // harness: "unified test"
uint32_t sLedgerApplied = 0;        // entries projected (the file's "mergeShipUnified" section)
u32 sBottleSnapshot = 0;            // Hyrule's bottle slots before the item being received (BottleSlots)
std::vector<bool> sRecorded;        // Hyrule checks already in the ledger, by RandomizerCheck
uint32_t sRecordedCount = 0;        // ledger entries sRecorded was built from
uint32_t sRecordedSeed = 0;

MsSharedSave* Shared() {
    return static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
}

// The seed's "Combine Shared Items" (MergeShipGoal.h): an item both games have gives both games' versions.
int CombinedItems() {
    return MergeShip_Goal_CombineItemsInGame() ? 1 : 0;
}

// Majora's Mask's owl statue item `key` (2Ship's owl statue shuffle): its statue (MergeShipOwlWarp), or -1.
s32 OwlOfKey(MsItemKey key) {
    switch (key) {
        case MSI_MM_OWL_GREAT_BAY_COAST: return MERGESHIP_OWL_GREAT_BAY_COAST;
        case MSI_MM_OWL_ZORA_CAPE: return MERGESHIP_OWL_ZORA_CAPE;
        case MSI_MM_OWL_SNOWHEAD: return MERGESHIP_OWL_SNOWHEAD;
        case MSI_MM_OWL_MOUNTAIN_VILLAGE: return MERGESHIP_OWL_MOUNTAIN_VILLAGE;
        case MSI_MM_OWL_CLOCK_TOWN_SOUTH: return MERGESHIP_OWL_CLOCK_TOWN;
        case MSI_MM_OWL_MILK_ROAD: return MERGESHIP_OWL_MILK_ROAD;
        case MSI_MM_OWL_WOODFALL: return MERGESHIP_OWL_WOODFALL;
        case MSI_MM_OWL_SOUTHERN_SWAMP: return MERGESHIP_OWL_SOUTHERN_SWAMP;
        case MSI_MM_OWL_IKANA_CANYON: return MERGESHIP_OWL_IKANA_CANYON;
        case MSI_MM_OWL_STONE_TOWER: return MERGESHIP_OWL_STONE_TOWER;
        default: return -1;
    }
}

// An owl statue item found for Termina activates its statue in Hyrule's copy right away (the user, 2026-09-24: "if you
// get owl statue drops, you should definitely get that statue automatically activated"), so the Song of Soaring can
// soar to it from Hyrule (MergeShipTermina.h) before Termina's file has it (the ledger gives Termina the item).
void ActivateOwlItem(MsItemKey key) {
    const s32 owl = OwlOfKey(key);
    if (owl >= 0) {
        MergeShip_Termina_ActivateOwl((u8)owl);
        Log("OWL %s: statue %d activated", KeyName((uint16_t)key), owl);
    }
}

// A bottle of Majora's Mask's given here: one of Majora's Mask's six (the shared save's, on its MM item menu and usable
// here, MergeShipOwnership.h). False when it has six (the ledger gives it to Termina anyway).
bool GiveMMBottleHere(u8 content) {
    return MergeShip_Own_GiveMMBottle(content);
}

// One of Majora's Mask's own items bits in the shared save (its magic arrows, Lens, ocarina: the Items (Termina) page).
void SetTerminaItemBit(u32 bit) {
    if (MsSharedSave* shared = Shared()) {
        shared->items |= 1u << bit;
    }
}

// Majora's Mask's copy of an item both games have, found here (the RG_MM_* SoH generated for it): given here on the MM
// item menu and usable at once (the user, 2026-09-24: "Everything you get should be given ON THE FLY and you can use ANY
// mm item and OOT item when you get them"; MergeShipOwnership.h), never as Ocarina of Time's. The ledger gives it to
// Termina too (RecordHyruleCheck). False for any other item, and for an MM bottle while Majora's Mask has six.
bool GiveMMCopyHere(PlayState* play, RandomizerGet rg) {
    switch (rg) {
        case RG_MM_FIRE_ARROWS:
            SetTerminaItemBit(MS_ITEM_MM_ARROW_FIRE);
            return true;
        case RG_MM_ICE_ARROWS:
            SetTerminaItemBit(MS_ITEM_MM_ARROW_ICE);
            return true;
        case RG_MM_LIGHT_ARROWS:
            SetTerminaItemBit(MS_ITEM_MM_ARROW_LIGHT);
            return true;
        case RG_MM_LENS_OF_TRUTH:
            SetTerminaItemBit(MS_ITEM_MM_LENS_OF_TRUTH);
            return true;
        case RG_MM_OCARINA:
            SetTerminaItemBit(MS_ITEM_MM_OCARINA);
            return true;
        case RG_MM_BOMB_BAG:
        case RG_MM_BIG_BOMB_BAG:
        case RG_MM_BIGGEST_BOMB_BAG:
        case RG_MM_PROGRESSIVE_BOMB_BAG:
            MergeShip_Own_RaiseMMUpgrade(UPG_BOMB_BAG, (rg == RG_MM_PROGRESSIVE_BOMB_BAG)
                                                           ? MergeShip_Own_MMUpgrade(UPG_BOMB_BAG) + 1
                                                           : (rg == RG_MM_BOMB_BAG)     ? 1
                                                             : (rg == RG_MM_BIG_BOMB_BAG) ? 2
                                                                                          : 3);
            MergeShip_Own_FillMM(MS_OWN_BOMB); // MM Item_Give: a new bag comes full (Majora's Mask's count)
            return true;
        case RG_MM_PROGRESSIVE_BOW:
        case RG_MM_BIG_QUIVER:
        case RG_MM_BIGGEST_QUIVER:
            if ((rg == RG_MM_PROGRESSIVE_BOW) && !MergeShip_MMEquip_Owns(MMEQUIP_BOW)) {
                MergeShip_MMEquip_Give(play, ITEM_MM_BOW_HERO); // with MM's first quiver
            } else {
                MergeShip_Own_RaiseMMUpgrade(UPG_QUIVER, (rg == RG_MM_PROGRESSIVE_BOW)
                                                             ? MergeShip_Own_MMUpgrade(UPG_QUIVER) + 1
                                                             : (rg == RG_MM_BIG_QUIVER) ? 2
                                                                                        : 3);
            }
            MergeShip_Own_FillMM(MS_OWN_BOW);
            return true;
        case RG_MM_KOKIRI_SWORD:
            if (!MergeShip_MMEquip_Owns(MMEQUIP_KOKIRI)) {
                MergeShip_MMEquip_Give(play, ITEM_MM_SWORD_KOKIRI);
            }
            return true;
        // A teammate's exact sword step (Termina's own RI_SWORD_RAZOR / RI_SWORD_GILDED, not SoH's own
        // RG_PROGRESSIVE_MM_SWORD pool item): give that step directly, as GiveMMSwords' per-copy re-grant does, so a
        // Razor Sword found in Termina never shows here as a Kokiri Sword and a Gilded Sword never re-selects Razor.
        case RG_MM_RAZOR_SWORD:
            if (!MergeShip_MMEquip_Owns(MMEQUIP_RAZOR) && !MergeShip_MMEquip_Owns(MMEQUIP_GILDED)) {
                MergeShip_MMEquip_Give(play, ITEM_MM_SWORD_RAZOR);
            }
            return true;
        case RG_MM_GILDED_SWORD:
            if (!MergeShip_MMEquip_Owns(MMEQUIP_GILDED)) {
                MergeShip_MMEquip_Give(play, ITEM_MM_SWORD_GILDED);
            }
            return true;
        // A teammate's exact Goron Lullaby step (Termina's RI_SONG_LULLABY_INTRO / RI_SONG_LULLABY, not SoH's own
        // RG_PROGRESSIVE_GORON_LULLABY): that step's found flag and song, as GiveLullabies gives them per flag. The
        // full Lullaby counts the Intro as found too (SyncProgressiveFlags' rule), so SoH's next progressive copy is
        // not the Intro.
        case RG_MM_GORON_LULLABY_INTRO:
        case RG_MM_GORONS_LULLABY: {
            const bool full = (rg == RG_MM_GORONS_LULLABY);
            if (!Flags_GetRandomizerInf(RAND_INF_OBTAINED_MM_GORON_LULLABY_INTRO)) {
                Flags_SetRandomizerInf(RAND_INF_OBTAINED_MM_GORON_LULLABY_INTRO);
            }
            if (full && !Flags_GetRandomizerInf(RAND_INF_OBTAINED_MM_GORON_LULLABY)) {
                Flags_SetRandomizerInf(RAND_INF_OBTAINED_MM_GORON_LULLABY);
            }
            const u8 song = full ? ITEM_MM_SONG_GORON_LULLABY : ITEM_MM_SONG_GORON_LULLABY_INTRO;
            if (!MergeShip_Songs_HasItem(song)) {
                MergeShip_Songs_GiveItem(song);
            }
            return true;
        }
        // Held only when it went into one of Majora's Mask's six (the shared save's bottles).
        case RG_MM_EMPTY_BOTTLE:
            return GiveMMBottleHere(ITEM_BOTTLE);
        case RG_MM_BOTTLE_WITH_MILK:
            return GiveMMBottleHere(ITEM_MILK_BOTTLE);
        case RG_MM_BOTTLE_WITH_RED_POTION:
            return GiveMMBottleHere(ITEM_POTION_RED);
        default:
            return false;
    }
}

// Termina's item, found here: nothing of it is Hyrule's but its consumables' Ocarina of Time equivalent.
// (Majora's Mask's items Hyrule can hold -- masks, songs, MM's gear, bottles -- are given here on the MM menu and usable,
// the user's rule, 2026-09-24: "you can use ANY mm item and OOT item when you get them"; the ledger takes them to
// Termina too, MsDelivery_OwnWorld.)
bool TerminasHere(RandomizerGet rg) {
    return IsTerminaOnly(rg);
}

// MergeShipUnified_ItemGive's gives (defined with it, below): true when Hyrule now holds some of Termina's item `rg`.
bool GiveTerminaItemHere(PlayState* play, RandomizerGet rg);

// ---- the seed file, for Termina ------------------------------------------------------------------------------------
// SoH keeps a randomizer file's seed in the save itself; 2Ship needs the MERGESHIP_SEED file's "mm" section to make
// Termina's file of the same seed. Which file that is is recorded with the Hyrule file (the "mergeShipUnified"
// section's seedFile) when it loads: CVAR_GENERAL("SpoilerLog") names whatever seed was generated or loaded last, not
// necessarily this file's. Each trip hands it over (MsTransfer.unifiedSeedFile, MergeShipHost.cpp).

std::string sSeedFile;          // this file's MERGESHIP_SEED, absolute, UTF-8 ("" unknown)
uint32_t sSeedFileSeed = 0;     // the seed sSeedFile was checked against (0: not checked since the file loaded)
bool sTestSeed = false;         // harness: "unifiedseed use" (the debug file hands the generated seed over)

std::string PathToUtf8(const std::filesystem::path& path) {
    const std::u8string u8 = path.u8string();
    return std::string(u8.begin(), u8.end());
}

std::filesystem::path PathFromUtf8(const std::string& text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

// The seed (shared.finalSeed) of a MERGESHIP_SEED file; 0 when it is not one or cannot be read. The big sections (the
// "oot" spoiler, "mm", the playthrough, hints, entrances, placements) are dropped while parsing.
uint32_t SeedOfFile(const std::filesystem::path& path) {
    std::error_code error;
    if (path.empty() || !std::filesystem::is_regular_file(path, error)) {
        return 0;
    }
    try {
        std::ifstream stream(path);
        if (!stream.is_open()) {
            return 0;
        }
        const nlohmann::json file = nlohmann::json::parse(
            stream, [](int depth, nlohmann::json::parse_event_t event, nlohmann::json& parsed) {
                (void)depth;
                if (event == nlohmann::json::parse_event_t::key) {
                    return (parsed != "oot") && (parsed != "mm") && (parsed != "playthrough") && (parsed != "hints") &&
                           (parsed != "entrances") && (parsed != "placements");
                }
                return true;
            });
        if (!file.is_object() || (file.value("type", std::string()) != MS_SEED_TYPE) || !file.contains("shared") ||
            !file["shared"].is_object()) {
            return 0;
        }
        return file["shared"].value("finalSeed", 0u);
    } catch (const std::exception& e) {
        SPDLOG_WARN("MergeShip unified: {} is not a readable seed file ({})", PathToUtf8(path), e.what());
        return 0;
    }
}

std::filesystem::path Absolute(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path out = std::filesystem::absolute(path, error);
    return error ? path : out.lexically_normal();
}

// This file's MERGESHIP_SEED: the recorded one, the spoiler SoH loaded last, <app>/Randomizer/<hash icons>.json (the
// name SoH's spoiler log gives it), or any seed file of the Randomizer folder, whichever is a seed file of `seed`.
std::string FindSeedFile(uint32_t seed, const std::string& recorded) {
    std::vector<std::filesystem::path> candidates;
    if (!recorded.empty()) {
        candidates.push_back(PathFromUtf8(recorded));
    }
    const char* spoiler = CVarGetString(CVAR_GENERAL("SpoilerLog"), "");
    if ((spoiler != nullptr) && (spoiler[0] != '\0')) {
        candidates.push_back(Absolute(std::filesystem::path(spoiler)));
    }
    const std::filesystem::path folder = Ship::Context::GetPathRelativeToAppDirectory("Randomizer");
    auto ctx = Rando::Context::GetInstance();
    char hashName[32];
    const auto& icons = ctx->hashIconIndexes;
    snprintf(hashName, sizeof(hashName), "%02u-%02u-%02u-%02u-%02u.json", icons[0], icons[1], icons[2], icons[3],
             icons[4]);
    candidates.push_back(folder / hashName);
    for (const std::filesystem::path& candidate : candidates) {
        if (SeedOfFile(candidate) == seed) {
            return PathToUtf8(Absolute(candidate));
        }
    }
    std::error_code error;
    for (std::filesystem::directory_iterator it(folder, error), end; !error && (it != end); it.increment(error)) {
        const std::filesystem::path& path = it->path();
        if ((path.extension() != ".json") ||
            (std::find(candidates.begin(), candidates.end(), path) != candidates.end())) {
            continue;
        }
        if (SeedOfFile(path) == seed) {
            return PathToUtf8(Absolute(path));
        }
    }
    return std::string();
}

// The loaded file plays a unified seed (or the harness says so).
bool FilePlaysUnifiedSeed() {
    return sTestSeed || (IS_RANDO && MergeShip_Termina_Enabled());
}

// Checks (once per load) that sSeedFile is this file's seed, finding it otherwise.
void ResolveSeedFile(bool notify) {
    const uint32_t seed = Rando::Context::GetInstance()->GetSeed();
    if ((sSeedFileSeed == seed) && (seed != 0)) {
        return;
    }
    sSeedFileSeed = seed;
    const std::string before = sSeedFile;
    sSeedFile = FindSeedFile(seed, sSeedFile);
    if (sSeedFile.empty()) {
        Log("SEEDFILE none for seed %u (recorded \"%s\"): Termina cannot be randomized until the seed file is back in "
            "the Randomizer folder",
            seed, before.c_str());
        if (notify) {
            Notification::Emit({
                .prefix = "Mergeship seed:",
                .prefixColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                .message = "this file's seed file is missing from the Randomizer folder; Termina cannot use it",
            });
        }
        return;
    }
    Log("SEEDFILE seed %u: %s%s", seed, sSeedFile.c_str(), (before == sSeedFile) ? "" : " (recorded now)");
}

// Hyrule's four bottle slots, a byte each: a bottle item Hyrule held changed them -- a new bottle, or its content
// poured into an empty one Hyrule had (MergeShipRandoItems.cpp GiveMMBottle keeps the free slots for Ocarina of Time's
// bottles still to come). With "Combine Shared Items" these four are the shared save's first four, which carry the
// item to Termina (MsDelivery_Flags: no ledger entry then). Counting the bottles missed the pour, and the ledger gave
// Termina a second bottle of the same content: a second Deku Princess (the user, 2026-09-30). Unchanged: Hyrule's four
// were full, the bottle is Termina's.
u32 BottleSlots() {
    u32 slots = 0;
    for (s32 i = 0; i < 4; i++) {
        slots |= (u32)gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] << (8 * i);
    }
    return slots;
}

// The Hyrule checks the ledger holds, kept in step with it (another engine only ever appends Termina's).
void RefreshRecorded(const MsSharedSave* shared) {
    const uint32_t count = (shared->ledgerCount < MS_SHARED_LEDGER_MAX) ? shared->ledgerCount : MS_SHARED_LEDGER_MAX;
    if ((sRecorded.size() != RC_MAX) || (count < sRecordedCount) || (shared->ledgerSeed != sRecordedSeed)) {
        sRecorded.assign(RC_MAX, false);
        sRecordedCount = 0;
        sRecordedSeed = shared->ledgerSeed;
    }
    for (uint32_t i = sRecordedCount; i < count; i++) {
        const MsLedgerEntry& e = shared->ledger[i];
        if ((e.world == MS_WORLD_HYRULE) && (e.check < RC_MAX)) {
            sRecorded[e.check] = true;
        }
    }
    sRecordedCount = count;
}

void Record(MsSharedSave* shared, RandomizerCheck rc, MsItemKey key, uint8_t flags, bool commit) {
    RefreshRecorded(shared);
    if ((rc > RC_UNKNOWN_CHECK) && (rc < RC_MAX) && sRecorded[rc]) {
        return;
    }
    const uint32_t count = sRecordedCount;
    if (count >= MS_SHARED_LEDGER_MAX) {
        SPDLOG_ERROR("MergeShip unified: the shared save's ledger is full; check {} not recorded", (int)rc);
        return;
    }
    MsLedgerEntry& e = shared->ledger[count];
    e.item = (uint16_t)key;
    e.check = (uint16_t)rc;
    e.world = MS_WORLD_HYRULE;
    e.flags = flags;
    e.seq = (uint16_t)(count + 1);
    shared->ledgerCount = count + 1;
    sRecordedCount = count + 1;
    if ((rc > RC_UNKNOWN_CHECK) && (rc < RC_MAX)) {
        sRecorded[rc] = true;
    }
    if (commit) {
        MergeShipHost_CommitSharedSave();
    }
    Log("LEDGER +#%u world=hyrule check=%d (%s) item=%s flags=0x%02X", count + 1, (int)rc,
        std::string(EnumToString(rc)).c_str(), KeyName((uint16_t)key), flags);
}

// The step a progressive Majora's Mask item found here gave (GiveMMCopyHere / the MM sword gives ran first: the check
// is marked found once the item is received), read off Hyrule's MM items; MSI_MAX for any other item. The ledger names
// that step, never the pool item: the shared save carries the step to Termina already (the swords, the bow, the bags),
// so Termina resolving "Progressive Sword (MM)" again from its own state gave the next step on top -- one Progressive
// Sword (MM) became two, and an MM bow found here gave Termina nothing (2026-11-03).
MsItemKey MmProgressiveStep(RandomizerGet rg) {
    switch (rg) {
        case RG_PROGRESSIVE_MM_SWORD:
            return MergeShip_MMEquip_Owns(MMEQUIP_GILDED)   ? MSI_MM_GILDED_SWORD
                   : MergeShip_MMEquip_Owns(MMEQUIP_RAZOR)  ? MSI_MM_RAZOR_SWORD
                   : MergeShip_MMEquip_Owns(MMEQUIP_KOKIRI) ? MSI_MM_KOKIRI_SWORD
                                                            : MSI_MAX;
        case RG_MM_PROGRESSIVE_BOW: {
            const s32 level = MergeShip_Own_MMUpgrade(UPG_QUIVER);
            return (level >= 3)                             ? MSI_MM_BIGGEST_QUIVER
                   : (level == 2)                           ? MSI_MM_BIG_QUIVER
                   : MergeShip_MMEquip_Owns(MMEQUIP_BOW)    ? MSI_MM_HEROS_BOW
                                                            : MSI_MAX;
        }
        case RG_MM_PROGRESSIVE_BOMB_BAG: {
            const s32 level = MergeShip_Own_MMUpgrade(UPG_BOMB_BAG);
            return (level >= 3) ? MSI_MM_BIGGEST_BOMB_BAG
                   : (level == 2) ? MSI_MM_BIG_BOMB_BAG
                   : (level == 1) ? MSI_MM_BOMB_BAG
                                  : MSI_MAX;
        }
        default:
            return MSI_MAX;
    }
}

// A Hyrule check holding `rg` was found; `held`: SoH gave it (not Termina's alone, not a fifth bottle). `stepKey`: the
// step a progressive item gave (MmProgressiveStep), MSI_MAX to record the placed item.
void RecordHyruleCheck(RandomizerCheck rc, RandomizerGet rg, bool held, bool commit, MsItemKey stepKey = MSI_MAX) {
    MsSharedSave* shared = Shared();
    const MsItemKey key = (stepKey < MSI_MAX) ? stepKey : MsItems_KeyOfRG(rg);
    const MsItemInfo* info = MsItems_Get(key);
    if ((shared == nullptr) || (info == nullptr)) {
        return;
    }
    const uint8_t flags =
        MsDelivery_Flags(key, MS_WORLD_HYRULE, held ? 1 : 0, (info->persist == MS_CYCLE) ? 1 : 0, CombinedItems());
    Record(shared, rc, key, flags, commit);
}

bool HeldInHyrule(RandomizerGet rg, bool bottlesKnown) {
    if (IsTerminaOnly(rg)) {
        const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(rg));
        return (info != nullptr) && (info->persist == MS_CYCLE); // SoH gives the Ocarina of Time equivalent
    }
    const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(rg));
    if (bottlesKnown && (info != nullptr) && MsDelivery_IsBottle(info->kind)) {
        return BottleSlots() != sBottleSnapshot; // unchanged: Hyrule's four were full, Termina's fifth or sixth
    }
    return true;
}

// An entry Termina found for Hyrule: given natively here, as Anchor gives a remote player's item. `notify`: every
// caller now passes false (2026-09-30, "no notification burst on crossing") -- kept for a caller that one day needs
// its own announced delivery; the default stays off so a new call site does not reintroduce the burst by omission.
bool GiveHyrule(MsItemKey key, bool notify = false) {
    const MsItemInfo* info = MsItems_Get(key);
    if ((info == nullptr) || (gPlayState == nullptr) || TerminasHere(info->rg)) {
        return false;
    }
    Rando::Item& item = Rando::StaticData::RetrieveItem(info->rg);
    GetItemEntry entry = item.GetGIEntry_Copy();
    if (entry.modIndex == MOD_NONE) {
        if (entry.getItemId == GI_SWORD_BGS) {
            gSaveContext.bgsFlag = true;
        }
        Item_Give(gPlayState, (u8)entry.itemId);
    } else if (entry.modIndex == MOD_RANDOMIZER) {
        if (entry.getItemId == RG_ICE_TRAP) {
            return false;
        }
        Randomizer_Item_Give(gPlayState, entry);
    } else {
        return false;
    }
    if (!notify) {
        return true;
    }
    Notification::Emit({
        .prefix = "From Termina:",
        .message = "",
        .suffix = item.GetName().GetEnglish(),
    });
    return true;
}

// Anchor's item and flag packets are held back while the ledger is projected (RoomState.syncItemsAndFlags, which
// SendPacket_GiveItem and the flag packets check, Anchor.h): every entry already reached the team when it was found --
// the finder's MM_GIVE_ITEM, or for an MS_LEDGER_REMOTE entry the teammate's own packet -- and the gives' OnItemReceive
// would send each one to the team again as a GIVE_ITEM. Anchor's isProcessingIncomingPacket does the same for a
// received item, but is private to it.
class AnchorSyncPause {
  public:
    AnchorSyncPause() : mAnchor(Anchor::Instance) {
        if (mAnchor != nullptr) {
            mSaved = mAnchor->roomState.syncItemsAndFlags;
            mAnchor->roomState.syncItemsAndFlags = 0;
        }
    }
    ~AnchorSyncPause() {
        if (mAnchor != nullptr) {
            mAnchor->roomState.syncItemsAndFlags = mSaved;
        }
    }
    AnchorSyncPause(const AnchorSyncPause&) = delete;
    AnchorSyncPause& operator=(const AnchorSyncPause&) = delete;

  private:
    Anchor* mAnchor;
    u8 mSaved = 0;
};

// Gives what Termina found for Hyrule since this file last did.
void Project(bool force) {
    MsSharedSave* shared = Shared();
    if ((shared == nullptr) || !MergeShipUnified_Active() || (gPlayState == nullptr)) {
        return;
    }
    const uint32_t count = (shared->ledgerCount < MS_SHARED_LEDGER_MAX) ? shared->ledgerCount : MS_SHARED_LEDGER_MAX;
    if (sLedgerApplied > count) {
        sLedgerApplied = count; // the ledger was cleared (another seed claimed the slot): nothing to redo
    }
    if (!force && (sLedgerApplied == count)) {
        return;
    }
    const AnchorSyncPause anchorPause; // the team has these already (AnchorSyncPause)
    for (uint32_t i = sLedgerApplied; i < count; i++) {
        const MsLedgerEntry& e = shared->ledger[i];
        if ((e.world != MS_WORLD_TERMINA) || !(e.flags & MS_LEDGER_FOR_HYRULE)) {
            continue;
        }
        // Silent: the item already belongs to this file the moment Termina found it (the ledger write is
        // synchronous); catching it up here is bookkeeping, not a delivery to announce (the user, 2026-09-30: no
        // notification burst on crossing). The finder's own textbox and Anchor's live "<player> found ..." are
        // the only notifications for this item.
        const bool given = GiveHyrule((MsItemKey)e.item, false);
        Log("PROJECT #%u from termina check=%d item=%s -> %s", i + 1, (int)e.check, KeyName(e.item),
            given ? "given" : "nothing in Hyrule");
    }
    sLedgerApplied = count;
}

// Termina's Razor / Gilded Sword and Goron Lullaby reach Hyrule with the shared save's state; SoH's progressive
// items pick their next copy from the randomizer's own found flags, which that state never set.
void SyncProgressiveFlags() {
    if (MergeShip_MMEquip_Owns(MMEQUIP_RAZOR) && !Flags_GetRandomizerInf(RAND_INF_OBTAINED_MM_RAZOR_SWORD)) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_MM_RAZOR_SWORD);
    }
    if (MergeShip_MMEquip_Owns(MMEQUIP_GILDED) && !Flags_GetRandomizerInf(RAND_INF_OBTAINED_MM_GILDED_SWORD)) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_MM_GILDED_SWORD);
    }
    const u16 intro = (u16)(1u << (MERGESHIP_SONG_GORON_LULLABY_INTRO - MERGESHIP_SONG_FIRST));
    const u16 full = (u16)(1u << (MERGESHIP_SONG_GORON_LULLABY - MERGESHIP_SONG_FIRST));
    if ((gMergeShipSave.mmSongs & (intro | full)) &&
        !Flags_GetRandomizerInf(RAND_INF_OBTAINED_MM_GORON_LULLABY_INTRO)) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_MM_GORON_LULLABY_INTRO);
    }
    if ((gMergeShipSave.mmSongs & full) && !Flags_GetRandomizerInf(RAND_INF_OBTAINED_MM_GORON_LULLABY)) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_MM_GORON_LULLABY);
    }
}

// Loading a unified file: claim the slot's ledger for this seed, and write the checks this file found that the ledger
// lacks (a file played before the ledger existed, or saved while the shared save was not).
void OnLoadGame(int32_t fileNum) {
    (void)fileNum;
    sRecorded.clear();
    sRecordedCount = 0;
    sSeedFileSeed = 0;
    sTestSeed = false;
    // A new file or seed starts the slot anew before its ledger is read (MergeShipHost.h).
    MergeShipHost_ClaimSharedSave(fileNum);
    MsSharedSave* shared = Shared();
    if ((shared == nullptr) || !IS_RANDO || !MergeShip_Termina_Enabled()) {
        return;
    }
    // The seed file Termina's file is made from (each trip hands it over): recorded with this file now, while the
    // seed SoH last loaded may still be this one.
    ResolveSeedFile(true);
    const uint32_t seed = Rando::Context::GetInstance()->GetSeed();
    if (shared->ledgerSeed != seed) {
        Log("LEDGER slot claimed for seed %u (was %u, %u entries dropped)", seed, shared->ledgerSeed,
            shared->ledgerCount);
        shared->ledgerSeed = seed;
        shared->ledgerCount = 0;
        sLedgerApplied = 0;
    }
    uint32_t added = 0;
    const uint32_t before = shared->ledgerCount;
    auto ctx = Rando::Context::GetInstance();
    for (int rc = RC_UNKNOWN_CHECK + 1; rc < RC_MAX; rc++) {
        if (MergeShip_Termina_IsTerminaCheck((RandomizerCheck)rc)) {
            continue;
        }
        Rando::ItemLocation* loc = ctx->GetItemLocation((RandomizerCheck)rc);
        if ((loc == nullptr) || !loc->HasObtained()) {
            continue;
        }
        RecordHyruleCheck((RandomizerCheck)rc, loc->GetPlacedRandomizerGet(),
                          HeldInHyrule(loc->GetPlacedRandomizerGet(), false), false);
    }
    added = shared->ledgerCount - before;
    MergeShipHost_CommitSharedSave();
    Log("LEDGER loaded: seed=%u entries=%u (%u from this file's found checks) applied=%u", seed,
        shared->ledgerCount, added, sLedgerApplied);
}

void OnCheckStatus(RandomizerCheck rc, RandomizerCheckStatus status) {
    if ((status != RCSHOW_COLLECTED) || !GameInteractor::IsSaveLoaded(true) || !MergeShipUnified_Active() ||
        MergeShip_Termina_IsTerminaCheck(rc)) {
        return;
    }
    Rando::ItemLocation* loc = Rando::Context::GetInstance()->GetItemLocation(rc);
    if (loc == nullptr) {
        return;
    }
    const RandomizerGet rg = loc->GetPlacedRandomizerGet();
    RecordHyruleCheck(rc, rg, HeldInHyrule(rg, true), true, MmProgressiveStep(rg));
}

void OnFrame() {
    if ((gPlayState == nullptr) || !MergeShipUnified_Active()) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    SyncProgressiveFlags();
    const bool busy = (player->stateFlags1 & (PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_ITEM_CS)) ||
                      (gPlayState->msgCtx.msgMode != MSGMODE_NONE) || (gSaveContext.gameMode != GAMEMODE_NORMAL);
    if (busy) {
        return;
    }
    sBottleSnapshot = BottleSlots();
    Project(false);
}

// ---- the file's section ------------------------------------------------------------------------------------------

void InitSection(bool isDebug) {
    (void)isDebug;
    sLedgerApplied = 0;
    sSeedFile.clear();
    sSeedFileSeed = 0;
}

void LoadSection() {
    SaveManager::Instance->LoadData("ledgerApplied", sLedgerApplied, (uint32_t)0);
    SaveManager::Instance->LoadData("seedFile", sSeedFile, std::string());
    sSeedFileSeed = 0;
}

void SaveSection(SaveContext* saveContext, int sectionID, bool fullSave) {
    (void)saveContext;
    (void)sectionID;
    (void)fullSave;
    SaveManager::Instance->SaveData("ledgerApplied", sLedgerApplied);
    SaveManager::Instance->SaveData("seedFile", sSeedFile);
}

void RegisterUnified() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    SaveManager::Instance->AddInitFunction(InitSection);
    SaveManager::Instance->AddLoadFunction("mergeShipUnified", 1, LoadSection);
    SaveManager::Instance->AddSaveFunction("mergeShipUnified", 1, SaveSection, true, SECTION_PARENT_NONE);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>(OnLoadGame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnRandoSetCheckStatus>(OnCheckStatus);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(OnFrame);
}

static RegisterShipInitFunc sInitFunc(RegisterUnified, {});

// ---- harness -----------------------------------------------------------------------------------------------------

bool SameName(const std::string& a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (toupper((unsigned char)a[i]) != toupper((unsigned char)b[i])) {
            return false;
        }
    }
    return true;
}

// An MsItems.def key ("MASTER_SWORD"), a RandomizerGet name ("RG_MASTER_SWORD") or number.
RandomizerGet ParseRG(const std::string& s) {
    for (int k = 0; k < MSI_MAX; k++) {
        const MsItemInfo* info = MsItems_Get((MsItemKey)k);
        if ((info != nullptr) && SameName(s, info->keyName)) {
            return info->rg;
        }
    }
    for (int rg = 0; rg < RG_MAX; rg++) {
        if (SameName(s, EnumToString((RandomizerGet)rg))) {
            return (RandomizerGet)rg;
        }
    }
    return (RandomizerGet)strtol(s.c_str(), nullptr, 0);
}

RandomizerCheck ParseRC(const std::string& s) {
    if (s.empty()) {
        return RC_UNKNOWN_CHECK;
    }
    if ((s[0] >= '0') && (s[0] <= '9')) {
        return (RandomizerCheck)strtol(s.c_str(), nullptr, 0);
    }
    for (int rc = 0; rc < RC_MAX; rc++) {
        if (SameName(s, EnumToString((RandomizerCheck)rc))) {
            return (RandomizerCheck)rc;
        }
    }
    return RC_UNKNOWN_CHECK;
}

void LogState() {
    MsSharedSave* shared = Shared();
    Log("HYRULE ocarina=0x%02X hookshot=0x%02X slingshot=0x%02X hammer=0x%02X equipment=0x%04X upgrades=0x%08X "
        "quest=0x%08X forestKeys=%d bgs=%d mmEquip=0x%02X mmSongs=0x%04X mmQuest=0x%02X masks=0x%02X/0x%08X "
        "aButton=%d swim=%d ledger=%u applied=%u unified=%d",
        INV_CONTENT(ITEM_OCARINA_FAIRY), INV_CONTENT(ITEM_HOOKSHOT), INV_CONTENT(ITEM_SLINGSHOT),
        INV_CONTENT(ITEM_HAMMER), gSaveContext.inventory.equipment, gSaveContext.inventory.upgrades,
        gSaveContext.inventory.questItems, gSaveContext.inventory.dungeonKeys[SCENE_FOREST_TEMPLE],
        gSaveContext.bgsFlag, gMergeShipSave.mmEquipOwned, gMergeShipSave.mmSongs, gMergeShipSave.mmQuestItems,
        gMergeShipSave.ownedMasks, gMergeShipSave.ownedMMMasks,
        Flags_GetRandomizerInf(RAND_INF_HAS_OCARINA_A) ? 1 : 0, Flags_GetRandomizerInf(RAND_INF_CAN_SWIM) ? 1 : 0,
        (shared != nullptr) ? shared->ledgerCount : 0u, sLedgerApplied, MergeShipUnified_Active() ? 1 : 0);
}

} // namespace

bool MergeShipUnified_Active() {
    MsSharedSave* shared = Shared();
    if (shared == nullptr) {
        return false;
    }
    if (sTestUnified) {
        return true;
    }
    return IS_RANDO && MergeShip_Termina_Enabled() && (shared->ledgerSeed != 0) &&
           (shared->ledgerSeed == Rando::Context::GetInstance()->GetSeed());
}

bool MergeShipUnified_SeedHandoff(std::string& path, uint32_t& seed) {
    path.clear();
    seed = 0;
    if (!FilePlaysUnifiedSeed()) {
        return false;
    }
    ResolveSeedFile(false);
    seed = Rando::Context::GetInstance()->GetSeed();
    path = sSeedFile;
    return true;
}

// Anchor (AnchorNet.h "Cross-world items", soh/Network/Anchor/MergeShipAnchorHost.cpp): a teammate found this item in
// Termina and the player is in Hyrule. Delivered as an item found here (MsDelivery.h), without a check of this file:
// given natively where Hyrule holds it; Termina gets it from the shared save's state on the next crossing, or from a
// ledger entry (world hyrule, check 0, MS_LEDGER_REMOTE) when the state does not carry it or Hyrule cannot hold it.
// Anchor claims the packet only after a 1, so this runs once per item per process; an item already owned is not
// given again (Hyrule's gives of owned items are no-ops, Termina's projection converts an owned item to nothing).
extern "C" int32_t MergeShipShared_ReceiveRemoteItem(int32_t msItemKey) {
    const MsItemInfo* info = MsItems_Get((MsItemKey)msItemKey);
    if ((info == nullptr) || (gPlayState == nullptr)) {
        return 0; // not an item, or no game to give it to: Anchor keeps the packet
    }
    const MsItemKey key = (MsItemKey)msItemKey;
    const bool unified = MergeShipUnified_Active();
    const bool consumable = (info->persist == MS_CYCLE);
    MsSharedSave* shared = Shared();
    // Termina's alone (a generated RG_MM_*, oot=0 in MsItems.def) outside a unified seed, in mergeship.exe: no ledger
    // takes Hyrule's copy of it to Termina, so it is left to Termina's engine, which gives it natively when the player
    // is there (its capture then brings Majora's Mask's copy to Hyrule's MM menu). Its consumables are Hyrule's to
    // take. soh.exe has no Termina: Hyrule gives what it can hold of it, below.
    if (!unified && (shared != nullptr) && IsTerminaOnly(info->rg) && !consumable) {
        return 0;
    }
    if (unified) {
        ActivateOwlItem(key);
    }
    bool held;
    if (key == MSI_ICE_TRAP) {
        gSaveContext.ship.pendingIceTrapCount++;
        held = true;
    } else if (IsTerminaOnly(info->rg)) {
        // Majora's Mask's copy on Hyrule's MM menu (an MM bottle goes into Majora's Mask's six, which Hyrule's four
        // below never count), its combined counterpart or a consumable's equivalent; false: nothing of it is Hyrule's.
        held = GiveTerminaItemHere(gPlayState, info->rg);
    } else {
        const u32 bottlesBefore = BottleSlots();
        held = GiveHyrule(key, false); // Anchor shows its own "<player> found ..." notification
        if (held && MsDelivery_IsBottle(info->kind)) {
            held = BottleSlots() != bottlesBefore; // Hyrule's four bottles were full: the bottle is Termina's
        }
    }
    // A 4th piece of heart: Item_Give (z_parameter.c) only counts it, and normally z_message.c turns 4 into a
    // container while showing "You got a Piece of Heart!" -- a remote piece shows no such textbox (Anchor's own
    // notification instead), so without this a piece from a teammate in Termina could leave the count stuck at 4/4
    // forever, never raising max health (mirrors soh/Network/Anchor/Packets/GiveItem.cpp's same-world handling).
    if (held && (key == MSI_PIECE_OF_HEART)) {
        s32 heartPieces = (s32)(gSaveContext.inventory.questItems & 0xF0000000) >> (QUEST_HEART_PIECE + 4);
        if (heartPieces >= 4) {
            gSaveContext.inventory.questItems &= ~0xF0000000;
            gSaveContext.inventory.questItems += (heartPieces % 4) << (QUEST_HEART_PIECE + 4);
            gSaveContext.healthCapacity += 0x10 * (heartPieces / 4);
            gSaveContext.health += 0x10 * (heartPieces / 4);
        }
    }
    if (unified && (shared != nullptr)) {
        const uint8_t flags =
            MsDelivery_Flags(key, MS_WORLD_HYRULE, held ? 1 : 0, consumable ? 1 : 0, CombinedItems());
        Record(shared, RC_UNKNOWN_CHECK, key, (uint8_t)(flags | MS_LEDGER_REMOTE), true);
    }
    Log("REMOTE %s from an Anchor teammate: %s%s", info->keyName, held ? "given in Hyrule" : "not held in Hyrule",
        unified ? ", recorded" : "");
    return (held || unified) ? 1 : 0;
}

bool MergeShipUnified_ForTermina(RandomizerGet rg) {
    if (!TerminasHere(rg)) {
        return false;
    }
    switch (rg) {
        // Majora's Mask's copies Hyrule gives on its MM menu (GiveMMCopyHere): found here, usable here.
        case RG_MM_FIRE_ARROWS:
        case RG_MM_ICE_ARROWS:
        case RG_MM_LIGHT_ARROWS:
        case RG_MM_LENS_OF_TRUTH:
        case RG_MM_OCARINA:
        case RG_MM_BOMB_BAG:
        case RG_MM_BIG_BOMB_BAG:
        case RG_MM_BIGGEST_BOMB_BAG:
        case RG_MM_PROGRESSIVE_BOMB_BAG:
        case RG_MM_PROGRESSIVE_BOW:
        case RG_MM_BIG_QUIVER:
        case RG_MM_BIGGEST_QUIVER:
        case RG_MM_KOKIRI_SWORD:
        case RG_MM_RAZOR_SWORD:
        case RG_MM_GILDED_SWORD:
        case RG_MM_GORON_LULLABY_INTRO:
        case RG_MM_GORONS_LULLABY:
        case RG_MM_EMPTY_BOTTLE:
        case RG_MM_BOTTLE_WITH_MILK:
        case RG_MM_BOTTLE_WITH_RED_POTION:
            return false;
        default:
            break;
    }
    const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(rg));
    return (info != nullptr) && (info->persist == MS_PERM);
}

namespace {

// What Hyrule gives of Termina's item `rg`. True when Hyrule takes it: Majora's Mask's copy on the MM menu, Ocarina of
// Time's counterpart ("Combine Shared Items"), or a consumable (its Ocarina of Time equivalent where Hyrule has room
// for one). False when nothing of it is Hyrule's (the ledger or Termina's engine gives it):
// MergeShipShared_ReceiveRemoteItem's `held`.
bool GiveTerminaItemHere(PlayState* play, RandomizerGet rg) {
    // Majora's Mask's copy of an item both games have: given here, on the MM menu.
    const bool mmCopy = GiveMMCopyHere(play, rg);
    // "Combine Shared Items": it is Ocarina of Time's too (the ledger takes Majora's Mask's to Termina,
    // RecordHyruleCheck).
    const MsItemKey counterpart = CombinedItems() ? MsDelivery_Counterpart(MsItems_KeyOfRG(rg)) : MSI_MAX;
    if (counterpart < MSI_MAX) {
        const u32 bottlesBefore = BottleSlots();
        bool given = GiveHyrule(counterpart, false);
        if (given && MsDelivery_IsBottleKey(counterpart)) {
            given = BottleSlots() != bottlesBefore; // Hyrule's four bottles were full
        }
        Log("GIVE %s: Ocarina of Time's %s %s", std::string(EnumToString(rg)).c_str(), KeyName((uint16_t)counterpart),
            given ? "given" : "not held here");
        return mmCopy || given;
    }
    if (mmCopy) {
        Log("GIVE %s: Majora's Mask's, on the MM menu here (and Termina's by the ledger)",
            std::string(EnumToString(rg)).c_str());
        return true;
    }
    // Termina's consumables: the engine whose check gave them keeps them (MS_CYCLE), as Ocarina of Time's nearest.
    switch (rg) {
        case RG_MM_ARROWS_50:
            if (INV_CONTENT(ITEM_BOW) == ITEM_BOW) {
                Inventory_ChangeAmmo(ITEM_BOW, 50);
            }
            break;
        // Majora's Mask's bombchus, nuts and sticks: Majora's Mask's (its MM menu), in Ocarina of Time's slot and ammo.
        case RG_MM_BOMBCHU_1:
            // task 3: no chus before the seed's Bombchu Bag, in either game (MergeShip_Own_ChusLocked). Owning Majora's
            // Mask's chus would also end the lock for good, so a pre-bag chu gives nothing at all.
            if (MergeShip_Own_ChusLocked()) {
                Log("GIVE %s: no bombchus before the Bombchu Bag", std::string(EnumToString(rg)).c_str());
                break;
            }
            MergeShip_Own_GiveMM(MS_OWN_BOMBCHU);
            Inventory_ChangeAmmo(ITEM_BOMBCHU, 1);
            break;
        case RG_MM_DEKU_NUT_1:
            MergeShip_Own_GiveMM(MS_OWN_NUT);
            Inventory_ChangeAmmo(ITEM_NUT, 1);
            break;
        case RG_MM_DEKU_STICKS_5:
            MergeShip_Own_GiveMM(MS_OWN_STICK);
            Inventory_ChangeAmmo(ITEM_STICK, 5);
            break;
        case RG_MM_MAGIC_JAR_SMALL:
            Item_Give(play, ITEM_MAGIC_SMALL);
            break;
        case RG_MM_MAGIC_JAR_BIG:
            Item_Give(play, ITEM_MAGIC_LARGE);
            break;
        case RG_MM_SILVER_RUPEE:
            Rupees_ChangeBy(100);
            break;
        case RG_MM_FAIRY_REFILL:
            Item_Give(play, ITEM_FAIRY);
            break;
        case RG_MM_CHATEAU_ROMANI_REFILL:
        case RG_MM_GOLD_DUST_REFILL:
        case RG_MM_MUSHROOM:
            for (s32 i = 0; i < 4; i++) {
                if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_BOTTLE) {
                    gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] =
                        (rg == RG_MM_CHATEAU_ROMANI_REFILL) ? ITEM_MM_CHATEAU
                        : (rg == RG_MM_GOLD_DUST_REFILL)    ? ITEM_MM_GOLD_DUST
                                                            : ITEM_MM_MUSHROOM;
                    break;
                }
            }
            break;
        default:
            // Termina's alone: recorded for 2Ship when the check is marked found (the ledger).
            Log("GIVE %s is Termina's: nothing in Hyrule", std::string(EnumToString(rg)).c_str());
            return false;
    }
    return true;
}

} // namespace

bool MergeShipUnified_ItemGive(PlayState* play, RandomizerGet rg) {
    if (!TerminasHere(rg)) {
        return false;
    }
    ActivateOwlItem(MsItems_KeyOfRG(rg));
    GiveTerminaItemHere(play, rg);
    return true; // handled, whatever Hyrule got of it: Randomizer_Item_Give returns as for a given item
}

bool MergeShipUnified_TestCommand(const std::string& op, const std::vector<std::string>& args) {
    auto arg = [&](size_t i) -> std::string { return (i < args.size()) ? args[i] : std::string(); };

    if (op == "unified") {
        // unified test [seed] | unified off: the unified mode for the harness's debug file (not a unified seed's).
        sTestUnified = (arg(0) == "test");
        MsSharedSave* shared = Shared();
        if (sTestUnified) {
            // SoH's progressive items read the logic's save context (Item::GetGIEntry); a seed's file sets it.
            Rando::Context::GetInstance()->GetLogic()->SetSaveContext(&gSaveContext);
            if ((shared != nullptr) && !arg(1).empty()) {
                const uint32_t seed = (uint32_t)strtoul(arg(1).c_str(), nullptr, 0);
                if (shared->ledgerSeed != seed) {
                    shared->ledgerSeed = seed;
                    shared->ledgerCount = 0;
                    sLedgerApplied = 0;
                }
            }
        }
        Log("UNIFIED test=%d active=%d hosted=%d seed=%u ledger=%u applied=%u", sTestUnified ? 1 : 0,
            MergeShipUnified_Active() ? 1 : 0, (shared != nullptr) ? 1 : 0,
            (shared != nullptr) ? shared->ledgerSeed : 0u, (shared != nullptr) ? shared->ledgerCount : 0u,
            sLedgerApplied);
        return true;
    }
    if (op == "ledger") {
        MsSharedSave* shared = Shared();
        if (shared == nullptr) {
            Log("LEDGER no shared save (not hosted)");
            return true;
        }
        const uint32_t count = (shared->ledgerCount < MS_SHARED_LEDGER_MAX) ? shared->ledgerCount : 0u;
        Log("LEDGER seed=%u count=%u applied=%u", shared->ledgerSeed, count, sLedgerApplied);
        for (uint32_t i = 0; i < count; i++) {
            const MsLedgerEntry& e = shared->ledger[i];
            Log("LEDGER #%u world=%s check=%u item=%s flags=0x%02X", i + 1, WorldName(e.world), e.check,
                KeyName(e.item), e.flags);
        }
        return true;
    }
    if (op == "ledgeradd") {
        // ledgeradd <hyrule|termina> <item> [check] [flags]: an entry as the other engine would write it.
        MsSharedSave* shared = Shared();
        const MsItemKey key = MsItems_KeyOfRG(ParseRG(arg(1)));
        const MsItemInfo* info = MsItems_Get(key);
        if ((shared == nullptr) || (info == nullptr) || (shared->ledgerCount >= MS_SHARED_LEDGER_MAX)) {
            Log("LEDGERADD failed (%s)", arg(1).c_str());
            return true;
        }
        const uint8_t world = (arg(0) == "hyrule") ? MS_WORLD_HYRULE : MS_WORLD_TERMINA;
        const uint8_t flags = arg(3).empty()
                                  ? MsDelivery_Flags(key, world, 1, (info->persist == MS_CYCLE) ? 1 : 0,
                                                     CombinedItems())
                                  : (uint8_t)strtoul(arg(3).c_str(), nullptr, 0);
        MsLedgerEntry& e = shared->ledger[shared->ledgerCount];
        e.item = (uint16_t)key;
        e.check = (uint16_t)strtoul(arg(2).c_str(), nullptr, 0);
        e.world = world;
        e.flags = flags;
        e.seq = (uint16_t)(shared->ledgerCount + 1);
        shared->ledgerCount++;
        Log("LEDGERADD #%u world=%s item=%s flags=0x%02X", shared->ledgerCount, WorldName(world), info->keyName,
            flags);
        return true;
    }
    if (op == "hyrulefind") {
        // hyrulefind <item> <check>: the check holds <item> and is found: SoH gives it as the randomizer does
        // (Randomizer_Item_Give / Item_Give), then marks the check found, which writes the ledger.
        const RandomizerGet rg = ParseRG(arg(0));
        const RandomizerCheck rc = ParseRC(arg(1));
        auto ctx = Rando::Context::GetInstance();
        Rando::ItemLocation* loc =
            ((rc > RC_UNKNOWN_CHECK) && (rc < RC_MAX)) ? ctx->GetItemLocation(rc) : nullptr;
        if ((loc == nullptr) || (gPlayState == nullptr) || (rg <= RG_NONE) || (rg >= RG_MAX)) {
            Log("HYRULEFIND failed (%s at %s)", arg(0).c_str(), arg(1).c_str());
            return true;
        }
        loc->SetPlacedItem(rg);
        loc->SetCheckStatus(RCSHOW_UNCHECKED);
        sBottleSnapshot = BottleSlots();
        Rando::Item& item = Rando::StaticData::RetrieveItem(rg);
        GetItemEntry entry = item.GetGIEntry_Copy();
        if (entry.modIndex == MOD_NONE) {
            Item_Give(gPlayState, (u8)entry.itemId);
        } else {
            Randomizer_Item_Give(gPlayState, entry);
        }
        loc->SetCheckStatus(RCSHOW_COLLECTED);
        const bool forTermina = MergeShipUnified_ForTermina(rg);
        Log("HYRULEFIND %s at %s: \"You found %s%s!\"", std::string(EnumToString(rg)).c_str(),
            std::string(EnumToString(rc)).c_str(), item.GetName().GetEnglish().c_str(),
            forTermina ? " for Termina" : "");
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
    if (op == "unifiedseed") {
        // unifiedseed use: the harness's (debug) file hands the seed generated last (randogen) to Termina as a unified
        // file's would, and claims the slot's ledger for it. unifiedseed [check <2Ship RandoCheckId>]: what the next
        // trip hands over, and what the seed file's "mm" section places at that Termina check.
        if (arg(0) == "use") {
            sTestSeed = true;
            sSeedFileSeed = 0;
            const uint32_t seed = Rando::Context::GetInstance()->GetSeed();
            MsSharedSave* shared = Shared();
            if ((shared != nullptr) && (shared->ledgerSeed != seed)) {
                shared->ledgerSeed = seed;
                shared->ledgerCount = 0;
                sLedgerApplied = 0;
            }
        }
        std::string path;
        uint32_t seed = 0;
        const bool unified = MergeShipUnified_SeedHandoff(path, seed);
        Log("UNIFIEDSEED unified=%d rando=%d termina=%d seed=%u file=%s", unified ? 1 : 0, IS_RANDO ? 1 : 0,
            MergeShip_Termina_Enabled() ? 1 : 0, seed, path.empty() ? "(none)" : path.c_str());
        if ((arg(0) == "check") && !arg(1).empty() && !path.empty()) {
            try {
                std::ifstream stream(PathFromUtf8(path));
                const nlohmann::json file = nlohmann::json::parse(stream);
                const nlohmann::json& checks = file.at("mm").at("checks");
                std::string item = "(not shuffled: vanilla)";
                if (checks.contains(arg(1))) {
                    const nlohmann::json& entry = checks[arg(1)];
                    item = entry.is_string() ? entry.get<std::string>() : entry.value("randoItemId", std::string("?"));
                }
                Log("UNIFIEDSEED check %s mmFinalSeed=%u item=%s", arg(1).c_str(),
                    file.at("mm").value("finalSeed", 0u), item.c_str());
            } catch (const std::exception& e) { Log("UNIFIEDSEED check %s failed: %s", arg(1).c_str(), e.what()); }
        }
        return true;
    }
    return false;
}
