#pragma once

// MergeShip2 -- trackers over both worlds (the unified randomizer, M7; UNIFIED_RANDO_DESIGN.md "Hints and trackers").
//
// Every check found in either world is in the shared save's ledger (SharedSave.h, MsLedgerEntry world + check + item),
// written by each engine as it finds it. The trackers read it live, so what was found in Termina shows as found while
// the player is in Hyrule, and the other way round:
//  - SoH's check tracker (randomizer_check_tracker.cpp) lists Termina's checks after Hyrule's areas, grouped by
//    Termina's hint areas (MergeShipUnifiedHints.h: Clock Town, Woodfall Temple, ...), each with the item it held once
//    found. The checks are the seed's Termina locations that 2Ship's randomizer handles (MergeShip_Termina_IsTrackedCheck).
//    CVar MergeShip.TrackTermina (default on).
//  - SoH's item tracker (randomizer_item_tracker.cpp) gets a "Termina Items Tracker" window: remains, masks, songs,
//    each temple's keys, maps and stray fairies, owls, frogs, tokens, souls, Termina's quest items; found in either
//    world (ledger) or held by the shared save. CVar MergeShip.ItemTrackerTermina (default on).
//  - 2Ship's check tracker lists Hyrule's checks the same way (2s2h/Rando/MergeShip/UnifiedHyrule.h). 2Ship does not
//    know Hyrule's checks: SoH describes them (MsWorldCatalog.h) and hands the catalog over with every trip.
//
// All of it only in a unified seed inside mergeship.exe (MergeShipUnified_Active); ImGui, so it draws in whichever
// engine is running.

#include <cstdint>
#include <string>
#include <vector>

#include "soh/Enhancements/randomizer/randomizerTypes.h"

struct ImGuiTextFilter;
struct ImVec4;
struct MsWorldCatalog;

// The check tracker's Termina section is shown (a unified seed, MergeShip.TrackTermina on).
bool MergeShip_CheckTracker_TerminaActive();
// Draws Termina's areas and checks. `search` filters by area or check name; `hideCollected` hides found checks;
// `collectedColor` colours them otherwise.
void MergeShip_CheckTracker_DrawTermina(const ImGuiTextFilter& search, bool hideCollected, const ImVec4& collectedColor);
// Termina's checks found / tracked; false when the section is not shown.
bool MergeShip_CheckTracker_TerminaTotals(uint16_t* gotten, uint16_t* total);

// The item tracker's Termina window is shown (a unified seed, MergeShip.ItemTrackerTermina on).
bool MergeShip_ItemTracker_ShowTermina();
void MergeShip_ItemTracker_DrawTermina();

// BUG-0131: "Both Worlds' Items" (MergeShip.ItemTrackerBothWorlds, on; mm.o2r mounted) -- the item tracker's main window
// is two columns: Hyrule's items as Ship of Harkinian draws them and, beside them, Termina's (Majora's Mask's equipment,
// items, remains, songs and every mask; what the file owns, as the shared save and Hyrule's own MM items say), drawn
// from `originX` at the tracker's icon size and spacing. The transformation masks are then Termina's column's, not
// Hyrule's.
bool MergeShip_ItemTracker_BothWorlds();
void MergeShip_ItemTracker_DrawTerminaColumn(float originX, float originY);

// Hyrule's checks for 2Ship's hints and check tracker (MsWorldCatalog.h), built from the loaded seed; nullptr outside a
// unified seed. Sent with every trip to Termina (MergeShipHost.cpp).
const MsWorldCatalog* MergeShip_Catalog_Hyrule();

// Test harness (MergeShipTest.cpp): trackterm, trackitems, catalog. False for any other command.
bool MergeShip_Tracker_TestCommand(const std::string& op, const std::vector<std::string>& args);
