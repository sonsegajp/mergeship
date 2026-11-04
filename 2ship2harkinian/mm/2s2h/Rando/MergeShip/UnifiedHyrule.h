#ifndef RANDO_MERGESHIP_UNIFIED_HYRULE_H
#define RANDO_MERGESHIP_UNIFIED_HYRULE_H

// MergeShip2 -- Hyrule's checks on 2Ship's side: hints and the check tracker over both worlds (the unified randomizer,
// M6/M7; UNIFIED_RANDO_DESIGN.md "Hints and trackers").
//
// 2Ship's seed ("mm" section) holds only Termina's checks. SoH describes Hyrule's with every trip from Hyrule
// (include/mergeship/MsWorldCatalog.h, MsTransfer.worldCatalog): each Hyrule check that holds an item of the seed, its
// name, its area as SoH's check tracker names it ("Kokiri Forest") and the item (MsItemKey). It is copied on arrival.
// What has been found in Hyrule is the shared save's ledger (SharedSave.h, world MS_WORLD_HYRULE), read live.
//
//  - Hints (CVar MergeShip.HintHyrule, default on): 2Ship's hints about where an item is (the Oath to Order, the Song
//    of Soaring, the Hookshot, the remains, the transformation masks, the moon's Gossip Stones, gold dust) name its
//    Hyrule check when Hyrule holds it: "in Hyrule's Kokiri Forest". Gossip Stones (and the purchasable hints) draw
//    from Hyrule's checks too, weighted as 2Ship weighs its own, and name Hyrule's items (Ocarina of Time's names).
//  - The check tracker (CVar MergeShip.TrackHyrule, default on) lists Hyrule's checks after Termina's, by Hyrule's
//    areas, each with the item it held once found in Hyrule.
//
// Only in unified mode (UnifiedDelivery.h UnifiedActive) with a catalog from this session's trip; nothing otherwise.

#include <cstdint>
#include <string>
#include <vector>

#include "Rando/Types.h"

struct MsTransfer;
struct ImGuiTextFilter;
struct ImVec4;

namespace Rando {

namespace MergeShip {

// MergeShipHost_OnResume: keeps a copy of the Hyrule catalog the trip carries (MS_TRANSFER_FLAG_WORLD_CATALOG).
void OnWorldCatalog(const MsTransfer* arrival);

// Unified mode and a Hyrule catalog: Hyrule's checks are known.
bool HyruleKnown();

// A hint about where `randoItemId` is: `randoCheckId` is where Termina holds it (Rando::FindItemPlacement), RC_UNKNOWN
// when it does not. Termina's check as 2Ship names it (StaticData::GetLocationNameForHint); else the Hyrule check that
// holds it ("in Hyrule's Kokiri Forest", `exact`: "at Mido's Top Left Chest in Hyrule"); else 2Ship's unknown.
std::string ItemHintLocation(RandoItemId randoItemId, RandoCheckId randoCheckId, bool exact);

// Gossip Stones: Hyrule's checks a hint may name (not junk; `onlyUnfound`: not found yet), each with the item as 2Ship
// knows it (native, or the proxy RI_OOT_*). Empty unless HyruleKnown and MergeShip.HintHyrule.
struct HyruleHintCandidate {
    uint32_t index; // into the catalog
    RandoItemId randoItemId;
};
std::vector<HyruleHintCandidate> HyruleHintCandidates(bool onlyUnfound);
// The candidate's check, as a Gossip Stone names it (see ItemHintLocation).
std::string HyruleCandidateLocation(uint32_t index, bool exact);

// The check tracker (Rando/CheckTracker/CheckTracker.cpp): Hyrule's section after Termina's, and its totals.
bool HyruleTrackerActive();
void DrawHyruleTracker(const ImGuiTextFilter& filter, bool hideCollected, bool colorCollected,
                       const ImVec4& collectedColor);
// " + Hyrule found/total" for the tracker's total line; "" when the section is not shown.
std::string HyruleTrackerTotal();

// Test harness (MergeShipHostTest.cpp): hyrulecatalog, hyrulehint, hyruletrack. False for any other command.
bool HyruleTestCommand(const std::string& op, const std::vector<std::string>& args);

} // namespace MergeShip

} // namespace Rando

#endif
