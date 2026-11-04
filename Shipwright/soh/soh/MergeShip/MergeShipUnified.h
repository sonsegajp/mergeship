#pragma once

// MergeShip2 -- the unified randomizer's deliveries on SoH's side (M3, UNIFIED_RANDO_DESIGN.md "Delivery";
// include/mergeship/MsDelivery.h, SharedSave.h's ledger).
//
//  - A Hyrule check that holds an item of Termina's SoH has no use for (the RG_MM_* items tools/port_mm_rando.py
//    generated: Termina's keys, maps, souls, owls, frogs, clocks, Skulltula tokens ...) gives nothing here, shows
//    "You found X for Termina!", and is written to the shared save's ledger for 2Ship (MS_LEDGER_FOR_TERMINA). The
//    MM items SoH implements (masks, songs, equipment, remains ...) are given here and reach Termina with the shared
//    save's state (MsDelivery_StateCarried); the rest of Hyrule's items that Termina holds natively (Ocarina of Time's
//    equipment, C-button items, quest status) go to 2Ship through the ledger.
//  - Every Hyrule check found while a unified seed is played is written to the ledger (its RandomizerCheck and item,
//    for trackers) and committed at once.
//  - Entries Termina found for Hyrule (MS_LEDGER_FOR_HYRULE) are given natively when the player is in Hyrule --
//    silently: the find was already announced (the finder's own textbox, or Anchor's live "<player> found ..."), so
//    catching the ledger up here is bookkeeping, not a second delivery (2026-09-30, no notification burst on
//    crossing). The count already given is saved with the file ("mergeShipUnified" section, ledgerApplied).
//
// Unified mode: a randomizer file whose seed has RSK_MERGESHIP_TERMINA on, inside mergeship.exe. Loading such a file
// claims its slot's ledger for its seed (MsSharedSave.ledgerSeed; a ledger of another seed is cleared). Off (an off
// seed, soh.exe, a vanilla file) nothing here runs, except that a Termina-only item never asserts in
// Randomizer_Item_Give.

#include <cstdint>
#include <string>
#include <vector>

#include "soh/Enhancements/randomizer/randomizerTypes.h"

struct PlayState;

// The file plays a unified seed inside mergeship.exe (or the harness forces it).
bool MergeShipUnified_Active();

// For the trip to Termina (MsTransfer.unifiedSeed / unifiedSeedFile): true when the loaded file plays a unified seed
// (a randomizer file of a seed with RSK_MERGESHIP_TERMINA, or the harness's "unifiedseed use"); `seed` is its
// ctx->GetSeed() and `path` its MERGESHIP_SEED file (absolute, UTF-8), "" when none was found. The file is recorded in
// the Hyrule save's "mergeShipUnified" section (seedFile) when the file loads; the spoiler SoH loaded last may be another
// seed's.
bool MergeShipUnified_SeedHandoff(std::string& path, uint32_t& seed);

// Randomizer_Item_Give, first: an item SoH cannot hold (Termina's alone). True: handled (nothing given here, or the
// Ocarina of Time equivalent of a Termina consumable); the caller returns as for a given item.
bool MergeShipUnified_ItemGive(PlayState* play, RandomizerGet rg);

// The item is Termina's alone: its get-item text says "for Termina" (Messages/ItemMessages.cpp).
bool MergeShipUnified_ForTermina(RandomizerGet rg);

// Test harness (MergeShipTest.cpp): the M3 commands; false when `op` is not one of them.
bool MergeShipUnified_TestCommand(const std::string& op, const std::vector<std::string>& args);
