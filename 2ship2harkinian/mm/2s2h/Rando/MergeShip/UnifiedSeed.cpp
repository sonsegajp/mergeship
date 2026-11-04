// MergeShip2 -- the unified randomizer on 2Ship's side (UnifiedSeed.h).

#include "UnifiedSeed.h"

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <mergeship/EngineApi.h>
#include <mergeship/MsItems.h>
#include <mergeship/SharedSave.h>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "BenPort.h"
#include "Rando/Rando.h"
#include "2s2h/BenGui/Notification.h"
#include "2s2h/MergeShip/MergeShipHost.h"

namespace {

// ---- static checks: MsItems.def and RandoItemId agree ----------------------------------------------------------------

// clang-format off
constexpr RandoItemId kNative[] = {
#define MS_ITEM(key, rg, ri, kind, persist, oot, mm, maxLevel, name) ri,
#define MS_SKIP_RI(ri, why) ri,
#include <mergeship/MsItems.def>
#undef MS_ITEM
#undef MS_SKIP_RI
};
constexpr RandoItemId kProxies[] = {
#define MS_ITEM_OOT(key, rg, kind, persist, oot, mm, maxLevel, name) RI_OOT_##key,
#include <mergeship/MsItems.def>
#undef MS_ITEM_OOT
};
// clang-format on

// Every native RandoItemId (RI_UNKNOWN .. RI_MAX_TRAP) is named exactly once by an item or a skipped row.
constexpr bool EveryNativeItemOnce() {
    std::array<uint8_t, RI_MAX_TRAP + 1> count{};
    for (RandoItemId ri : kNative) {
        if (ri < 0 || ri > RI_MAX_TRAP) {
            return false;
        }
        count[ri]++;
    }
    for (size_t i = 0; i <= RI_MAX_TRAP; i++) {
        if (count[i] != 1) {
            return false;
        }
    }
    return true;
}
static_assert(EveryNativeItemOnce(), "include/mergeship/MsItems.def must name every RandoItemId exactly once (run "
                                     "tools/port_mm_rando.py --check for the list)");

// The proxies are exactly the ids after RI_MAX_TRAP, in table order.
constexpr bool ProxiesContiguous() {
    for (size_t i = 0; i < sizeof(kProxies) / sizeof(kProxies[0]); i++) {
        if (kProxies[i] != (RandoItemId)(RI_MAX_TRAP + 1 + i)) {
            return false;
        }
    }
    return RI_MAX == RI_MAX_TRAP + 1 + sizeof(kProxies) / sizeof(kProxies[0]);
}
static_assert(ProxiesContiguous(), "RI_OOT_* proxies must follow RI_MAX_TRAP directly (Types.h)");

std::vector<RandoItemId> sPendingProxies;

} // namespace

namespace Rando {

namespace MergeShip {

bool IsProxy(RandoItemId randoItemId) {
    return randoItemId > RI_MAX_TRAP && randoItemId < RI_MAX;
}

void OnProxyObtained(RandoItemId randoItemId) {
    SPDLOG_INFO("MergeShip unified randomizer: found {} ({}), an item of Hyrule's side, kept for the shared inventory",
                Rando::StaticData::Items[randoItemId].spoilerName, Rando::StaticData::Items[randoItemId].name);
    sPendingProxies.push_back(randoItemId);
}

std::vector<RandoItemId> TakePendingProxies() {
    std::vector<RandoItemId> out;
    out.swap(sPendingProxies);
    return out;
}

// ---- the seed a trip from Hyrule names (OnSeedTransfer) -------------------------------------------------------------

static bool sHostSpoke = false;           // a trip from Hyrule said whether its file plays a unified seed
static bool sHostSeedNamed = false;       // ... and it does
static uint32_t sHostSeed = 0;            // SoH ctx->GetSeed()
static uint32_t sHostLedgerSeed = 0;      // the slot's shared save's ledgerSeed on that trip (0: not carried)
static std::string sHostSeedFile;         // SoH's seed file (UTF-8, "" none)
static std::filesystem::path sFoundSeedFile; // the file of sHostSeed found here (cached per seed)
static uint32_t sFoundSeedFor = 0;

static void SeedLog(const char* fmt, ...) {
    char line[768];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    SPDLOG_INFO("MergeShip unified randomizer: {}", line);
    MergeShipHostTest_Log("%s", line); // trace_2ship.log; no-op without a script
}

static std::string PathToUtf8(const std::filesystem::path& path) {
    const std::u8string u8 = path.u8string();
    return std::string(u8.begin(), u8.end());
}

// shared.finalSeed of a MERGESHIP_SEED file, 0 when it is not one. The big sections are dropped while parsing.
static uint32_t SeedOfFile(const std::filesystem::path& path) {
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
        if (!IsMergedSeed(file) || !file.contains("shared") || !file["shared"].is_object()) {
            return 0;
        }
        return file["shared"].value("finalSeed", 0u);
    } catch (const std::exception&) { return 0; }
}

// The file of the seed Hyrule named: SoH's path if it is that seed's, else a seed file of that seed in the randomizer
// folder (SoH's "Randomizer" folder is the same folder: both engines share the app directory).
static std::filesystem::path HostSeedPath() {
    if (sFoundSeedFor == sHostSeed && !sFoundSeedFile.empty()) {
        return sFoundSeedFile;
    }
    sFoundSeedFile.clear();
    sFoundSeedFor = sHostSeed;
    if (!sHostSeedFile.empty()) {
        const std::filesystem::path path(std::u8string(sHostSeedFile.begin(), sHostSeedFile.end()));
        if (SeedOfFile(path) == sHostSeed) {
            sFoundSeedFile = path;
            return sFoundSeedFile;
        }
    }
    const std::filesystem::path folder = Ship::Context::GetPathRelativeToAppDirectory("randomizer", appShortName);
    std::error_code error;
    for (std::filesystem::directory_iterator it(folder, error), end; !error && (it != end); it.increment(error)) {
        if ((it->path().extension() == ".json") && (SeedOfFile(it->path()) == sHostSeed)) {
            sFoundSeedFile = it->path();
            SeedLog("seed %u: Hyrule's seed file \"%s\" is not it; found %s", sHostSeed, sHostSeedFile.c_str(),
                    PathToUtf8(sFoundSeedFile).c_str());
            return sFoundSeedFile;
        }
    }
    SeedLog("seed %u: no seed file of it (Hyrule named \"%s\"; nothing in %s)", sHostSeed, sHostSeedFile.c_str(),
            PathToUtf8(folder).c_str());
    return {};
}

void OnSeedTransfer(const MsTransfer* arrival) {
    // Only a trip from Hyrule that goes ahead, from a SoH that knows the field (a refused trip bounces the request).
    if ((arrival == nullptr) || (arrival->from != MS_ENGINE_SOH) || (arrival->entrance < 0) ||
        !MS_TRANSFER_HAS(arrival, unifiedSeedFile)) {
        return;
    }
    const bool named = (arrival->flags & MS_TRANSFER_FLAG_UNIFIED_SEED) != 0;
    std::string file;
    if (named) {
        const size_t length = strnlen(arrival->unifiedSeedFile, sizeof(arrival->unifiedSeedFile));
        file.assign(arrival->unifiedSeedFile, length);
    }
    const uint32_t seed = named ? arrival->unifiedSeed : 0u;
    sHostLedgerSeed = 0;
    if ((arrival->flags & MS_TRANSFER_FLAG_SHARED_SAVE) && (arrival->sharedSave != nullptr) &&
        (arrival->sharedSaveSize >= sizeof(MsSharedSave))) {
        sHostLedgerSeed = static_cast<const MsSharedSave*>(arrival->sharedSave)->ledgerSeed;
    }
    const bool changed = !sHostSpoke || (named != sHostSeedNamed) || (seed != sHostSeed) || (file != sHostSeedFile);
    sHostSpoke = true;
    sHostSeedNamed = named;
    sHostSeed = seed;
    sHostSeedFile = file;
    if (changed) {
        sFoundSeedFile.clear();
        sFoundSeedFor = 0;
        if (named) {
            SeedLog("Hyrule's file plays unified seed %u (seed file %s, ledger seed %u)", seed,
                    file.empty() ? "not found by SoH" : file.c_str(), sHostLedgerSeed);
        } else {
            SeedLog("Hyrule's file plays no unified seed: Termina's files are not made from one");
        }
    }
    if (named && (sHostLedgerSeed != 0) && (sHostLedgerSeed != seed)) {
        SPDLOG_WARN("MergeShip unified randomizer: the shared save's ledger belongs to seed {}, the trip's seed is {}",
                    sHostLedgerSeed, seed);
    }
}

bool HostNamesSeed(uint32_t* seed) {
    if (seed != nullptr) {
        *seed = sHostSeed;
    }
    return sHostSpoke && sHostSeedNamed;
}

std::filesystem::path MergedSeedPath() {
    if (sHostSpoke) {
        return sHostSeedNamed ? HostSeedPath() : std::filesystem::path();
    }
    std::string name = CVarGetString(MERGESHIP_UNIFIED_SEED_CVAR, "");
    if (name.empty()) {
        return {};
    }
    std::filesystem::path path(name);
    if (path.is_absolute()) {
        return path;
    }
    return Ship::Context::GetPathRelativeToAppDirectory("randomizer/" + name, appShortName);
}

bool HasMergedSeed() {
    if (sHostSpoke) {
        return sHostSeedNamed;
    }
    const std::filesystem::path path = MergedSeedPath();
    std::error_code error;
    return !path.empty() && std::filesystem::is_regular_file(path, error);
}

bool LoadedFileNeedsSeed() {
    if (!HostNamesSeed() || (IS_RANDO && (gSaveContext.save.shipSaveInfo.rando.finalSeed == sHostSeed))) {
        return false;
    }
    if (HostSeedPath().empty()) {
        return false; // nothing to remake it from: carry on with the file (Boot would only reload its last save)
    }
    SeedLog("the Termina file in play is not seed %u's randomizer file: booting it again", sHostSeed);
    return true;
}

bool RetireStaleTerminaFile(int32_t fileNum, const void* saveData) {
    if (!HostNamesSeed() || (saveData == nullptr) || (fileNum < 0) || (fileNum > 2)) {
        return false;
    }
    const Save* save = static_cast<const Save*>(saveData);
    const bool rando = save->shipSaveInfo.saveType == SAVETYPE_RANDO;
    const uint32_t finalSeed = rando ? save->shipSaveInfo.rando.finalSeed : 0u;
    if (rando && (finalSeed == sHostSeed)) {
        return false; // the seed's own file
    }
    if (HostSeedPath().empty()) {
        // Nothing to make it from: keep the file (the notification says why Termina is not randomized).
        SeedLog("Termina file %d is %s, but seed %u's file is missing: kept as it is", fileNum + 1,
                rando ? "another seed's randomizer file" : "not a randomizer file", sHostSeed);
        Notification::Emit({
            .prefix = "Mergeship seed:",
            .prefixColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
            .message = "the seed file is missing from the Randomizer folder; Termina is not randomized",
        });
        return false;
    }
    const std::filesystem::path folder = Ship::Context::GetPathRelativeToAppDirectory("saves", appShortName);
    const std::string stamp = std::to_string((long long)std::time(nullptr));
    for (int backup = 0; backup < 2; backup++) {
        const std::string stem = "file" + std::to_string(fileNum + 1) + (backup ? "backup" : "");
        const std::filesystem::path from = folder / (stem + ".json");
        const std::filesystem::path to = folder / (stem + "_notunified_" + stamp + ".json");
        std::error_code error;
        if (std::filesystem::exists(from, error)) {
            std::filesystem::rename(from, to, error);
            if (error) {
                SPDLOG_ERROR("MergeShip unified randomizer: could not set {} aside: {}", PathToUtf8(from),
                             error.message());
                return false;
            }
        }
    }
    SeedLog("Termina file %d was %s (finalSeed %u), not seed %u's randomizer file: set aside as "
            "file%d_notunified_%s.json, made again from the seed",
            fileNum + 1, rando ? "another seed's randomizer file" : "a vanilla file", finalSeed, sHostSeed,
            fileNum + 1, stamp.c_str());
    Notification::Emit({
        .prefix = "Mergeship seed:",
        .message = "Termina's file was not made from this seed; it starts over as the seed's randomizer file",
    });
    return true;
}

bool IsMergedSeed(const nlohmann::json& file) {
    return file.is_object() && file.contains("type") && file["type"] == MS_SEED_TYPE;
}

nlohmann::json UnwrapSeed(const nlohmann::json& seed) {
    if (!IsMergedSeed(seed)) {
        throw std::runtime_error("Not a Mergeship unified seed");
    }
    if (seed.value("version", 0) != MS_SEED_VERSION) {
        throw std::runtime_error("Mergeship seed version " + std::to_string(seed.value("version", 0)) +
                                 ", this build reads " + std::to_string(MS_SEED_VERSION));
    }
    const nlohmann::json shared = seed.value("shared", nlohmann::json::object());
    if (shared.value("msItemsVersion", -1) != MS_ITEMS_VERSION || shared.value("msItemCount", -1) != (int)MSI_MAX) {
        throw std::runtime_error("Mergeship seed made from another item table (MsItems.def)");
    }
    if (!seed.contains("mm") || !seed["mm"].is_object() || !seed["mm"].contains("checks")) {
        throw std::runtime_error("Mergeship seed has no Termina section");
    }
    return seed["mm"];
}

nlohmann::json LoadMergedSeed() {
    const std::filesystem::path path = MergedSeedPath();
    if (path.empty()) {
        throw std::runtime_error("Mergeship seed " + std::to_string(sHostSeed) +
                                 ": its file is not in the randomizer folder");
    }
    std::ifstream stream(path);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open Mergeship seed " + PathToUtf8(path));
    }
    nlohmann::json seed;
    try {
        stream >> seed;
    } catch (nlohmann::json::exception& e) { throw std::runtime_error("Failed to parse Mergeship seed"); }
    nlohmann::json mm = UnwrapSeed(seed);
    const uint32_t finalSeed = mm.value("finalSeed", 0u);
    if (HostNamesSeed() && (finalSeed != sHostSeed)) {
        throw std::runtime_error("Mergeship seed file " + PathToUtf8(path) + " is seed " + std::to_string(finalSeed) +
                                 ", Hyrule plays " + std::to_string(sHostSeed));
    }
    SeedLog("applying seed %u (%s): %zu Termina checks placed, ledger seed %u%s", finalSeed, PathToUtf8(path).c_str(),
            mm["checks"].size(), sHostLedgerSeed,
            (sHostLedgerSeed == 0 || sHostLedgerSeed == finalSeed) ? "" : " (MISMATCH: the ledger is another seed's)");
    // The seed's "Combine Shared Items" (SoH RSK_MS_COMBINE_SHARED_ITEMS): the shared save's itemsCombined, which the
    // shared-save apply and the delivery read (SoH writes it too, with the goal). Absent in older seeds: off.
    const bool combined = mm.value("combineSharedItems", false);
    if (MsSharedSave* shared = static_cast<MsSharedSave*>(MergeShipHost_SharedSave())) {
        shared->itemsCombined = combined ? 1 : 0;
    }
    SeedLog("combine shared items: %s", combined ? "on (either game's version gives both)" : "off (each game's own)");
    return mm;
}

bool SeedTestCommand(const std::string& op, const std::vector<std::string>& args) {
    if (op != "unifiedseed") {
        return false;
    }
    const std::filesystem::path path = MergedSeedPath();
    const bool rando = gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO;
    SeedLog("UNIFIEDSEED named=%d hostSpoke=%d seed=%u ledgerSeed=%u file=%s | file %d saveType=%d finalSeed=%u",
            HostNamesSeed() ? 1 : 0, sHostSpoke ? 1 : 0, sHostSeed, sHostLedgerSeed,
            path.empty() ? "(none)" : PathToUtf8(path).c_str(), gSaveContext.fileNum + 1,
            (int)gSaveContext.save.shipSaveInfo.saveType, rando ? gSaveContext.save.shipSaveInfo.rando.finalSeed : 0u);
    if ((args.size() >= 2) && (args[0] == "check")) {
        const RandoCheckId rc = Rando::StaticData::GetCheckIdFromName(args[1].c_str());
        if ((rc == RC_UNKNOWN) || !rando) {
            SeedLog("UNIFIEDSEED check %s: %s", args[1].c_str(), rando ? "unknown check" : "not a randomizer file");
            return true;
        }
        const RandoItemId ri = RANDO_SAVE_CHECKS[rc].randoItemId;
        const auto item = Rando::StaticData::Items.find(ri);
        SeedLog("UNIFIEDSEED check %s shuffled=%d item=%s (%s)", args[1].c_str(),
                RANDO_SAVE_CHECKS[rc].shuffled ? 1 : 0,
                (item != Rando::StaticData::Items.end()) ? item->second.spoilerName : "?",
                (item != Rando::StaticData::Items.end()) ? item->second.name : "?");
    }
    return true;
}

} // namespace MergeShip

} // namespace Rando
