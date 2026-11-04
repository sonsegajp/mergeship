#ifndef RANDO_MERGESHIP_UNIFIED_DELIVERY_H
#define RANDO_MERGESHIP_UNIFIED_DELIVERY_H

// MergeShip2 -- the unified randomizer's deliveries on 2Ship's side (M3, UNIFIED_RANDO_DESIGN.md "Delivery";
// include/mergeship/MsDelivery.h, SharedSave.h's ledger).
//
//  - A Termina check that holds an item of Hyrule's (a proxy RI_OOT_*) shows it with Ocarina of Time's get-item model
//    and icon, and "You found X for Hyrule!" when Termina cannot use it. Where 2Ship already has the item natively
//    (Ocarina of Time's C-button items, masks, trade items and equipment: MergeShipOoTItems.h, MergeShipOoTEquip.h;
//    medallions, Spiritual Stones and songs: MergeShipHyrule.h; bottles, capacities) it is given natively too.
//  - Every Termina check found while a unified seed is played is written to the shared save's ledger (the check and
//    the item, for trackers) and committed at once; what Hyrule has to give carries MS_LEDGER_FOR_HYRULE.
//  - Entries Hyrule found for Termina (MS_LEDGER_FOR_TERMINA: Termina's keys, souls, owls, frogs, clocks, Ocarina of
//    Time's equipment ...) are given natively when the player is in Termina, past MM's prologue -- silently: the find
//    was already announced (the finder's own textbox, or Anchor's live "<player> found ..."), so catching the ledger
//    up here is bookkeeping, not a second delivery (2026-09-30, no notification burst on crossing). The count already
//    given is ShipSaveInfo::mergeShipLedgerApplied, so it follows the Majora's Mask save.
//
// Unified mode: a randomizer file whose seed is the one the shared save's ledger belongs to (ledgerSeed, set by SoH
// when it loads the Hyrule file). Everything else here is off: an off seed, 2ship.exe, or a vanilla file.

#include <cstdint>
#include <string>
#include <vector>

#include "Rando/Types.h"

struct Actor;

namespace Rando {

namespace MergeShip {

// The file plays the unified seed of its shared save (or the harness forces it).
bool UnifiedActive();

// Rando::GiveItem for a proxy (RI_OOT_*): the native Majora's Mask item where 2Ship has one. True when given.
bool GiveProxy(RandoItemId proxy);
// Termina can hold this proxy natively (GiveProxy would give something).
bool ProxyHeldInTermina(RandoItemId proxy);

// Rando::ConvertItem: an item whose Termina level is full but whose unified level is not (the second copy of the
// progressive ocarina: MM holds one) becomes the proxy of the level Hyrule still lacks. False: not handled.
bool ConvertItem(RandoItemId randoItemId, RandoCheckId randoCheckId, RandoItemId* out);

// A Termina check was obtained: `placed` is the seed's item, `converted` what Rando::ConvertItem made of it (RI_JUNK
// when it became junk). Writes the ledger entry (unified mode only).
void OnCheckObtained(RandoCheckId randoCheckId, RandoItemId placed, RandoItemId converted);

// The concrete item OnCheckObtained's `converted` resolved `randoCheckId` to (RI_UNKNOWN before it is obtained, or an
// invalid id). For a progressive check (RI_PROGRESSIVE_SWORD and so on) this is the actual step given -- unlike
// `placed`, which every teammate would otherwise convert on his own state instead of matching what was really given
// (2s2h/Network/Anchor/Packets/GiveItem.cpp's "msItem", for a teammate in the other world, who has no sword/bomb
// bag/etc of his own to convert it against).
RandoItemId ResolvedItemOf(RandoCheckId randoCheckId);

// A Termina check an Anchor teammate obtained (MM_SET_CHECK_STATUS): written to the shared save at once
// (MsSharedSave.teamTerminaChecks), so the team's find outlives this file's next save and a restart.
void NoteTeamCheck(RandoCheckId randoCheckId);
// Marks the team's Termina checks obtained in this file (on load and on every return to Termina): an open check the
// team already found gave its item again (the user's second Deku Princess, 2026-09-30).
void ApplyTeamChecks();

// Get-item presentation for a proxy: the message (" for Hyrule" when Termina cannot use it), the textbox icon (MM
// header icon byte, 0xFE none; arms MergeShip_OoTItems_SetTextIconPending) and the notification icon.
// " (OOT)" / " (MM)": the game an item is from, for Termina's texts and notifications (empty outside mergeship.exe).
std::string OriginTag(RandoItemId randoItemId);
std::string DecorateGetItemMessage(RandoItemId randoItemId, const std::string& message);
bool GetItemTextIcon(RandoItemId randoItemId, uint8_t* icon);
const char* GetItemIconTexture(RandoItemId randoItemId);
// Draws a proxy's get-item model at the current matrix (Ocarina of Time's where 2Ship has it, else Majora's Mask's
// closest). False: nothing drawn (the caller draws its fallback).
bool DrawProxy(RandoItemId proxy);

// Test harness (MergeShipHostTest.cpp / MergeShipTest.cpp): the M3 commands; false when `op` is not one of them.
bool TestCommand(const std::string& op, const std::vector<std::string>& args);

} // namespace MergeShip

} // namespace Rando

#endif
