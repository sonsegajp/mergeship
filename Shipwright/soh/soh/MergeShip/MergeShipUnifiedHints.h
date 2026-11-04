#pragma once

// MergeShip2 -- hints over both worlds (the unified randomizer, M6; UNIFIED_RANDO_DESIGN.md "Hints and trackers").
//
// SoH's hint system (3drando/hints.cpp, hint.cpp) places the seed's hints on Hyrule's Gossip Stones. In a unified seed
// with RSK_MS_TERMINA_HINTS (default on) it covers Termina too:
//  - Termina's regions get hint areas of their own (RA_MM_*: Clock Town, Woodfall Temple, the Moon ...), by the 2Ship
//    scene of each region (grottos and fairy fountains by the region they belong to); Termina's starting items are
//    Termina's "Link's Pocket". With areas, Termina's locations are hintable, and Way of the Hero (fill.cpp
//    CalculateWotH) and foolish (CalculateBarren) consider both worlds' areas alike.
//  - A Termina location has no hint text of its own: an item hint about one reads "They say that #<check># in Termina
//    holds #<item>#." (MergeShip_Hints_TerminaLocationText). Termina's dungeons count as dungeons for the hint
//    distribution ("hoards", the dungeon limit).
//  - The Termina items SoH has no hint text for (the RG_MM_* tools/port_mm_rando.py generated) are named by their item
//    name, which carries " (MM)" (Item::GetHint). MM's masks keep their hand-written texts (" (MM)" but for Deku and
//    Fierce Deity).
//  - Majora's Mask's Mask of Truth's extra Gossip Stone hint (MergeShipMaskMessages.cpp) can be about Termina: half of
//    the stones pick one of Termina's major items and name its area (CVar MergeShip.TruthTerminaHint, default on).
//
// With RSK_MS_TERMINA_HINTS off, or in any seed without Termina, nothing here changes a thing.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "soh/Enhancements/randomizer/randomizerTypes.h"

class CustomMessage;
class HintText;

// Termina's checks and areas take part in hints: a unified seed with RSK_MS_TERMINA_HINTS on.
bool MergeShip_Hints_TerminaHintable();
// RSK_MS_TERMINA_HINTS's name and description (option.cpp, through MergeShip_Termina_SettingText); nullptr for any
// other setting.
const std::string* MergeShip_Hints_SettingText(RandomizerSettingKey rsk, bool description);

// StaticData::HintTable_Init, last: the names of Termina's areas (RHT_MM_AREA_*) and their StaticData::areaNames rows.
void MergeShip_Hints_InitTextTable();
// RegionTable_Init, after Termina's regions: each Termina region's hint area (only when Termina is hintable).
void MergeShip_Hints_AssignTerminaAreas();

// The hint area of a Termina region / check, from 2Ship's data (no seed needed): RA_NONE when unknown, RA_LINKS_POCKET
// for Termina's starting items.
RandomizerArea MergeShip_Hints_TerminaRegionArea(RandomizerRegion rr);
RandomizerArea MergeShip_Hints_TerminaCheckArea(RandomizerCheck rc);
bool MergeShip_Hints_IsTerminaArea(RandomizerArea area);
bool MergeShip_Hints_IsTerminaDungeonArea(RandomizerArea area);
// The English name of a Termina area ("Clock Town"); "" for any other area.
const char* MergeShip_Hints_TerminaAreaName(RandomizerArea area);
// Every Termina area, in the order trackers list them.
const std::vector<RandomizerArea>& MergeShip_Hints_TerminaAreas();

// hints.cpp / hint.cpp: whether a location is a dungeon's for hints (Hyrule's own rule, or a Termina dungeon area).
bool MergeShip_Hints_IsDungeonLocation(RandomizerCheck rc);
// hint.cpp: the text of an item hint about a Termina location.
HintText MergeShip_Hints_TerminaLocationText(RandomizerCheck rc);
// item.cpp Item::GetHint: a generated Termina item's hint text (its name); nullptr for every other item.
const HintText* MergeShip_Hints_ItemNameText(RandomizerGet rg);

// Majora's Mask's Mask of Truth (MergeShipMaskMessages.cpp): how many of Termina's major items its extra hint can name
// (0 outside a unified seed, or with MergeShip.TruthTerminaHint off), and the hint about the pick-th of them.
size_t MergeShip_Hints_TruthTerminaCount();
CustomMessage MergeShip_Hints_TruthTerminaHint(uint32_t pick);

// Test harness (MergeShipTest.cpp): hints, hintarea, truthhint. False for any other command.
bool MergeShip_Hints_TestCommand(const std::string& op, const std::vector<std::string>& args);
