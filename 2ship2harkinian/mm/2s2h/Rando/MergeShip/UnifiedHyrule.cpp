// MergeShip2 -- Hyrule's checks on 2Ship's side (UnifiedHyrule.h, the unified randomizer M6/M7).

#include "UnifiedHyrule.h"
#include "UnifiedDelivery.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <mergeship/EngineApi.h>
#include <mergeship/MsItems.h>
#include <mergeship/MsWorldCatalog.h>
#include <mergeship/SharedSave.h>
#include <spdlog/spdlog.h>

#include "2s2h/MergeShip/MergeShipHost.h"
#include "Rando/Rando.h"
#include "Rando/StaticData/StaticData.h"

extern "C" void MergeShipTest_Log(const char* fmt, ...);

#define CVAR_HINT_HYRULE "gEnhancements.MergeShip.HintHyrule"
#define CVAR_TRACK_HYRULE "gEnhancements.MergeShip.TrackHyrule"

namespace {

// ---- the canonical table (include/mergeship/MsItems.def): each MsItemKey's RandoItemId ------------------------------

// clang-format off
constexpr RandoItemId kRandoItemOfKey[] = {
#define MS_ITEM(key, rg, ri, kind, persist, oot, mm, maxLevel, name) ri,
#define MS_ITEM_OOT(key, rg, kind, persist, oot, mm, maxLevel, name) RI_OOT_##key,
#include <mergeship/MsItems.def>
#undef MS_ITEM
#undef MS_ITEM_OOT
};
constexpr const char* kKeyNames[] = {
#define MS_ITEM(key, rg, ri, kind, persist, oot, mm, maxLevel, name) #key,
#define MS_ITEM_OOT(key, rg, kind, persist, oot, mm, maxLevel, name) #key,
#include <mergeship/MsItems.def>
#undef MS_ITEM
#undef MS_ITEM_OOT
};
// clang-format on
static_assert(sizeof(kRandoItemOfKey) / sizeof(kRandoItemOfKey[0]) == MSI_MAX, "MsItems.def: one row per MsItemKey");

RandoItemId RandoItemOfKey(uint16_t key) {
    return (key < MSI_MAX) ? kRandoItemOfKey[key] : RI_UNKNOWN;
}

const char* KeyName(uint16_t key) {
    return (key < MSI_MAX) ? kKeyNames[key] : "NONE";
}

// ---- the catalog, as the last trip from Hyrule brought it -----------------------------------------------------------

struct HyruleCheck {
    uint16_t check; // SoH RandomizerCheck, as the ledger numbers Hyrule's checks
    uint16_t item;  // MsItemKey
    uint16_t area;  // into Catalog::areas
    uint8_t flags;  // MS_CATALOG_*
    std::string name;
};

struct Catalog {
    bool valid = false;
    uint32_t seed = 0;
    std::vector<std::string> areas;
    std::vector<HyruleCheck> checks; // SoH's tracker order: area by area
} sCatalog;

// ---- what the ledger says was found in Hyrule -----------------------------------------------------------------------

struct FoundView {
    uint32_t seed = 0;
    uint32_t count = 0;
    std::unordered_map<uint16_t, uint16_t> item; // Hyrule check -> MsItemKey found there
} sFound;

const MsSharedSave* Shared() {
    return static_cast<const MsSharedSave*>(MergeShipHost_SharedSave());
}

void RefreshFound() {
    const MsSharedSave* shared = Shared();
    const uint32_t count =
        (shared == nullptr) ? 0u : std::min<uint32_t>(shared->ledgerCount, (uint32_t)MS_SHARED_LEDGER_MAX);
    const uint32_t seed = (shared == nullptr) ? 0u : shared->ledgerSeed;
    if ((seed != sFound.seed) || (count < sFound.count)) {
        sFound.item.clear();
        sFound.count = 0;
        sFound.seed = seed;
    }
    for (uint32_t i = sFound.count; i < count; i++) {
        const MsLedgerEntry& e = shared->ledger[i];
        if (e.world == MS_WORLD_HYRULE) {
            sFound.item[e.check] = e.item;
        }
    }
    sFound.count = count;
}

bool IsFound(const HyruleCheck& check) {
    return sFound.item.find(check.check) != sFound.item.end();
}

bool HintsOn() {
    return Rando::MergeShip::HyruleKnown() && CVarGetInteger(CVAR_HINT_HYRULE, 1);
}

// The Hyrule check holding `randoItemId` (the first, in the catalog's order); -1 when Hyrule holds none.
int FindHyrulePlacement(RandoItemId randoItemId) {
    for (size_t i = 0; i < sCatalog.checks.size(); i++) {
        if (RandoItemOfKey(sCatalog.checks[i].item) == randoItemId) {
            return (int)i;
        }
    }
    return -1;
}

std::string HyruleLocation(const HyruleCheck& check, bool exact) {
    if (exact) {
        return "at " + check.name + " in Hyrule";
    }
    return "in Hyrule's " + sCatalog.areas[check.area];
}

// ---- harness --------------------------------------------------------------------------------------------------------

void Log(const char* fmt, ...) {
    char line[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    SPDLOG_INFO("MergeShip Hyrule: {}", line);
    MergeShipHostTest_Log("%s", line); // trace_2ship.log (host harness); no-op without a script
    MergeShipTest_Log("%s", line);     // 2Ship's own harness; no-op without a script
}

RandoItemId ParseRandoItem(const std::string& s) {
    for (auto& [ri, item] : Rando::StaticData::Items) {
        if ((item.spoilerName != nullptr) && (s == item.spoilerName)) {
            return ri;
        }
    }
    // An MsItems.def key ("MM_DEKU_MASK", "KOKIRI_SWORD")
    for (int key = 0; key < MSI_MAX; key++) {
        if (s == kKeyNames[key]) {
            return kRandoItemOfKey[key];
        }
    }
    return (RandoItemId)atoi(s.c_str());
}

} // namespace

namespace Rando {

namespace MergeShip {

void OnWorldCatalog(const MsTransfer* arrival) {
    if ((arrival == nullptr) || !(arrival->flags & MS_TRANSFER_FLAG_WORLD_CATALOG) ||
        !MS_TRANSFER_HAS(arrival, worldCatalog) || (arrival->worldCatalog == nullptr)) {
        sCatalog = Catalog{}; // a trip without one (not a unified seed): the last one belonged to another file
        return;
    }
    const MsWorldCatalog* catalog = arrival->worldCatalog;
    if ((catalog->size < sizeof(MsWorldCatalog)) || (catalog->version != MS_CATALOG_VERSION) ||
        (catalog->world != MS_WORLD_HYRULE)) {
        SPDLOG_WARN("MergeShip Hyrule: ignored a world catalog of version {} (this build reads {})", catalog->version,
                    MS_CATALOG_VERSION);
        return;
    }
    sCatalog = Catalog{};
    sCatalog.seed = catalog->seed;
    for (uint32_t i = 0; i < catalog->areaCount; i++) {
        sCatalog.areas.push_back((catalog->areaNames[i] != nullptr) ? catalog->areaNames[i] : "?");
    }
    for (uint32_t i = 0; i < catalog->checkCount; i++) {
        const MsCatalogCheck& c = catalog->checks[i];
        if (c.area >= sCatalog.areas.size()) {
            continue;
        }
        sCatalog.checks.push_back({ c.check, c.item, c.area, c.flags, (c.name != nullptr) ? c.name : "?" });
    }
    sCatalog.valid = true;
    const MsSharedSave* shared = Shared();
    Log("CATALOG received seed=%u areas=%zu checks=%zu ledgerSeed=%u", sCatalog.seed, sCatalog.areas.size(),
        sCatalog.checks.size(), (shared != nullptr) ? shared->ledgerSeed : 0u);
}

bool HyruleKnown() {
    return sCatalog.valid && UnifiedActive();
}

std::string ItemHintLocation(RandoItemId randoItemId, RandoCheckId randoCheckId, bool exact) {
    if ((randoCheckId == RC_UNKNOWN) && HintsOn()) {
        const int index = FindHyrulePlacement(randoItemId);
        if (index >= 0) {
            return HyruleLocation(sCatalog.checks[index], exact);
        }
    }
    return Rando::StaticData::GetLocationNameForHint(randoCheckId, exact);
}

std::vector<HyruleHintCandidate> HyruleHintCandidates(bool onlyUnfound) {
    std::vector<HyruleHintCandidate> out;
    if (!HintsOn()) {
        return out;
    }
    RefreshFound();
    for (size_t i = 0; i < sCatalog.checks.size(); i++) {
        const HyruleCheck& check = sCatalog.checks[i];
        const RandoItemId randoItemId = RandoItemOfKey(check.item);
        if ((randoItemId == RI_UNKNOWN) || !Rando::StaticData::Items.contains(randoItemId) ||
            (Rando::StaticData::Items[randoItemId].randoItemType == RITYPE_JUNK) || (onlyUnfound && IsFound(check))) {
            continue;
        }
        out.push_back({ (uint32_t)i, randoItemId });
    }
    return out;
}

std::string HyruleCandidateLocation(uint32_t index, bool exact) {
    if (index >= sCatalog.checks.size()) {
        return Rando::StaticData::GetLocationNameForHint(RC_UNKNOWN, exact);
    }
    return HyruleLocation(sCatalog.checks[index], exact);
}

bool HyruleTrackerActive() {
    return HyruleKnown() && CVarGetInteger(CVAR_TRACK_HYRULE, 1);
}

std::string HyruleTrackerTotal() {
    if (!HyruleTrackerActive()) {
        return "";
    }
    RefreshFound();
    uint32_t found = 0;
    uint32_t total = 0;
    for (const HyruleCheck& check : sCatalog.checks) {
        if (check.flags & MS_CATALOG_TRACKED) {
            total++;
            found += IsFound(check) ? 1 : 0;
        }
    }
    return " + Hyrule " + std::to_string(found) + "/" + std::to_string(total);
}

void DrawHyruleTracker(const ImGuiTextFilter& filter, bool hideCollected, bool colorCollected,
                       const ImVec4& collectedColor) {
    if (!HyruleTrackerActive()) {
        return;
    }
    RefreshFound();
    ImGui::PushID("MergeShipHyrule");
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "Hyrule");
    // The catalog is area by area: one header per run of an area.
    size_t start = 0;
    while (start < sCatalog.checks.size()) {
        const uint16_t area = sCatalog.checks[start].area;
        size_t end = start;
        while ((end < sCatalog.checks.size()) && (sCatalog.checks[end].area == area)) {
            end++;
        }
        const std::string& areaName = sCatalog.areas[area];
        const bool areaMatches = filter.PassFilter(areaName.c_str());
        uint32_t found = 0;
        uint32_t total = 0;
        std::vector<const HyruleCheck*> shown;
        for (size_t i = start; i < end; i++) {
            const HyruleCheck& check = sCatalog.checks[i];
            if (!(check.flags & MS_CATALOG_TRACKED)) {
                continue;
            }
            total++;
            const bool isFound = IsFound(check);
            found += isFound ? 1 : 0;
            if ((isFound && hideCollected) || (!areaMatches && !filter.PassFilter(check.name.c_str()))) {
                continue;
            }
            shown.push_back(&check);
        }
        start = end;
        if (shown.empty()) {
            continue;
        }
        ImGui::PushID(area);
        const std::string header = areaName + " (" + std::to_string(found) + "/" + std::to_string(total) + ")";
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, (found == total) ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1, 1, 1, 1));
        if (ImGui::CollapsingHeader(header.c_str())) {
            ImGui::PopStyleColor();
            ImGui::Indent(20.0f);
            for (const HyruleCheck* check : shown) {
                auto it = sFound.item.find(check->check);
                if (it != sFound.item.end()) {
                    const RandoItemId randoItemId = RandoItemOfKey(it->second);
                    const char* itemName = Rando::StaticData::Items.contains(randoItemId)
                                               ? Rando::StaticData::Items[randoItemId].name
                                               : KeyName(it->second);
                    ImGui::TextColored(colorCollected ? collectedColor : ImVec4(1, 1, 1, 1), "%s (%s)",
                                       check->name.c_str(), itemName);
                } else {
                    ImGui::Text("%s", check->name.c_str());
                }
            }
            ImGui::Unindent(20.0f);
        } else {
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::PopID();
}

bool HyruleTestCommand(const std::string& op, const std::vector<std::string>& args) {
    auto arg = [&](size_t i) -> std::string { return (i < args.size()) ? args[i] : std::string(); };
    if (op == "hyrulecatalog") {
        // hyrulecatalog: the catalog the last trip brought, and what Hyrule's hints would draw from.
        size_t tracked = 0;
        size_t major = 0;
        for (const HyruleCheck& check : sCatalog.checks) {
            tracked += (check.flags & MS_CATALOG_TRACKED) ? 1 : 0;
            major += (check.flags & MS_CATALOG_MAJOR) ? 1 : 0;
        }
        Log("HYRULECATALOG valid=%d known=%d seed=%u areas=%zu checks=%zu tracked=%zu major=%zu hintCandidates=%zu",
            sCatalog.valid ? 1 : 0, HyruleKnown() ? 1 : 0, sCatalog.seed, sCatalog.areas.size(),
            sCatalog.checks.size(), tracked, major, HyruleHintCandidates(false).size());
        return true;
    }
    if (op == "hyrulehint") {
        // hyrulehint <item...>: where each item's hint says it is (RandoItemId name or MsItems.def key).
        // hyrulehint stones [n]: the first n (default 5) Hyrule hints a Gossip Stone could give, as it words them.
        if (arg(0) == "stones") {
            const std::vector<HyruleHintCandidate> candidates = HyruleHintCandidates(false);
            const size_t n = arg(1).empty() ? 5 : (size_t)atoi(arg(1).c_str());
            for (size_t i = 0; i < candidates.size() && i < n; i++) {
                const HyruleHintCandidate& c = candidates[i];
                Log("HYRULEHINT stone check=%u item=%s \"They say %s is hidden %s.\"", sCatalog.checks[c.index].check,
                    KeyName(sCatalog.checks[c.index].item),
                    Rando::StaticData::GetItemName(c.randoItemId, true).c_str(),
                    HyruleCandidateLocation(c.index, false).c_str());
            }
            Log("HYRULEHINT stones candidates=%zu", candidates.size());
            return true;
        }
        for (const std::string& name : args) {
            const RandoItemId randoItemId = ParseRandoItem(name);
            const RandoCheckId termina = Rando::FindItemPlacement(randoItemId);
            const int hyrule = FindHyrulePlacement(randoItemId);
            Log("HYRULEHINT %s ri=%d termina=%s hyrule=%s text=\"%s\" exact=\"%s\"", name.c_str(), (int)randoItemId,
                (termina != RC_UNKNOWN) ? Rando::StaticData::CheckNames[termina].c_str() : "none",
                (hyrule >= 0) ? sCatalog.checks[hyrule].name.c_str() : "none",
                ItemHintLocation(randoItemId, termina, false).c_str(),
                ItemHintLocation(randoItemId, termina, true).c_str());
        }
        return true;
    }
    if (op == "hyruletrack") {
        // hyruletrack: the check tracker's Hyrule section as it would draw now (from the ledger).
        RefreshFound();
        Log("HYRULETRACK active=%d%s ledger=%u", HyruleTrackerActive() ? 1 : 0, HyruleTrackerTotal().c_str(),
            sFound.count);
        size_t start = 0;
        while (start < sCatalog.checks.size()) {
            const uint16_t area = sCatalog.checks[start].area;
            uint32_t found = 0;
            uint32_t total = 0;
            for (; (start < sCatalog.checks.size()) && (sCatalog.checks[start].area == area); start++) {
                const HyruleCheck& check = sCatalog.checks[start];
                if (!(check.flags & MS_CATALOG_TRACKED)) {
                    continue;
                }
                total++;
                auto it = sFound.item.find(check.check);
                if (it != sFound.item.end()) {
                    found++;
                    Log("HYRULETRACK FOUND check=%u (%s) area=\"%s\" item=%s", check.check, check.name.c_str(),
                        sCatalog.areas[area].c_str(), KeyName(it->second));
                }
            }
            if (total > 0) {
                Log("HYRULETRACK AREA \"%s\" %u/%u", sCatalog.areas[area].c_str(), found, total);
            }
        }
        return true;
    }
    return false;
}

} // namespace MergeShip

} // namespace Rando
