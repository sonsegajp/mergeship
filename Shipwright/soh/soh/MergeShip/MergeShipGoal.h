#pragma once

// MergeShip2 -- the unified randomizer's goal: what beating a seed over Hyrule and Termina takes
// (Shipwright/mergeship2/docs/UNIFIED_RANDO_DESIGN.md, "Win condition"). Off unless RSK_MERGESHIP_TERMINA is on; with
// it off nothing here changes a seed, a save or the game.
//
// Settings (settings_mergeship_goal.inc, the randomizer menu's "Mergeship" sidebar):
//   RSK_MS_GOAL                      Ganon | Majora | Ganon and Majora (default) | Triforce Hunt
//                                    "Ganon" is Hyrule's own win condition (RSK_WINCON: Ganon, or one of SoH's
//                                    triggers; "Anywhere" counts as Ganon here). Majora is 2Ship's Majora, behind
//                                    2Ship's own moon and Majora requirements (RSK_MM_ACCESS_*).
//   RSK_MS_TRIFORCE_PIECES_TOTAL     Triforce Hunt: pieces in the pool, placed anywhere in either world (1-100)
//   RSK_MS_TRIFORCE_PIECES_REQUIRED  Triforce Hunt: pieces needed, found in either world (1-100)
//   RSK_MS_REMAINS_AS_MEDALLIONS     MM's boss remains count as medallions wherever Hyrule counts medallions or
//                                    dungeon rewards (Rainbow Bridge, Ganon's Boss Key, Ganon's Soul, win condition)
//   RSK_MS_MAJORA_FORM_MASKS         transformation masks Majora requires on top of 2Ship's 20-mask count (0-4), so
//                                    "all masks" can mean all 24
// 2Ship's own RSK_MM_ACCESS_MOON_REMAINS_COUNT / _MOON_MASKS_COUNT / _TRIALS / _MAJORA_REMAINS_COUNT /
// _MAJORA_MASKS_COUNT are shown in the same sidebar; 2Ship enforces them as it always does.
//
// Generation. Unless the goal is "Ganon", the Triforce (RG_TRIFORCE) sits at RC_WINCON and Ganon gives junk (as SoH
// does for its own non-Ganon win conditions); RC_WINCON is reachable when MergeShip_Goal_LogicMet says so:
//   Hyrule's part  LOGIC_MS_GANON_DEFEATED (Ganon's arena, unified seeds only) or SoH's own trigger
//   Majora's part  LOGIC_MM_MAJORA_DEFEATED, which the Termina logic port sets in RR_MM_MOON_MAJORAS_LAIR; while that
//                  region is still empty (milestone M1, Termina without logic) Majora counts once Termina is reached
//                  and the remains and masks 2Ship's options ask for are found
//   Triforce       GetTriforcePieceCount(), which counts pieces found in both worlds
// Remains and masks the goal needs are advancement (Item::IsAdvancement).
//
// In game. The goal is written to the host's shared save (MsSharedSave.goalType, SharedSave.h), which 2Ship reads
// (2s2h/Rando/MergeShip/UnifiedGoal.h). Ganon's defeat when it is not the whole goal leaves Ganon's castle with a
// message instead of the credits; SoH rolls its credits (the Triforce at RC_WINCON) once CheckTriggers finds the goal
// met, which includes coming back from Termina with Majora beaten. Triforce pieces count from both worlds (the
// shared save's ledger).

#include <stdint.h>

#ifdef __cplusplus

// RSK_MS_GOAL
enum MsGoalOption : uint8_t {
    RO_MS_GOAL_GANON,
    RO_MS_GOAL_MAJORA,
    RO_MS_GOAL_BOTH,
    RO_MS_GOAL_TRIFORCE,
};

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "soh/Enhancements/randomizer/randomizerTypes.h"

class CustomMessage;
struct MsSharedSave;
namespace Rando {
class Logic;
}

// The unified randomizer is on and has a goal of its own (not Hyrule's alone): RC_WINCON holds the Triforce.
bool MergeShip_Goal_OwnsWincon();
// MsSharedSave.goalType for the seed being played or generated (MS_GOAL_*; MS_GOAL_NONE without the unified mode).
uint8_t MergeShip_Goal_SharedType();
// Triforce pieces the goal needs (clamped to the pool's).
uint16_t MergeShip_Goal_TriforceRequired();
// The unified seed's "Combine Shared Items" (RSK_MS_COMBINE_SHARED_ITEMS): either game's version of an item both games
// have (SharedSave.h MsShared_Combine*, MsDelivery.h MsDelivery_OwnWorld / _Counterpart) gives both. Off (the default):
// each game's version is its own game's alone. For the seed being generated, or (InGame) the randomizer file being
// played.
bool MergeShip_Goal_CombineItems();
bool MergeShip_Goal_CombineItemsInGame();

// ---- generation ----------------------------------------------------------------------------------------------------
// The setting names and descriptions (option.cpp through MergeShip_Termina_SettingText), nullptr for other keys.
const std::string* MergeShip_Goal_SettingText(RandomizerSettingKey rsk, bool description);
// Logic::CanTriggerWincon while MergeShip_Goal_OwnsWincon.
bool MergeShip_Goal_LogicMet(Rando::Logic* logic);
// Logic::MedallionCount: MM's remains that count as medallions (RSK_MS_REMAINS_AS_MEDALLIONS).
uint8_t MergeShip_Goal_LogicExtraMedallions(Rando::Logic* logic);
// Item::IsAdvancement for an item that is not advancement by itself: the remains and masks the goal needs.
bool MergeShip_Goal_ItemIsAdvancement(RandomizerGet rg);
// MergeShip_Termina_AddItemsToPool: the Triforce Hunt's pieces (up to RSK_MS_TRIFORCE_PIECES_TOTAL in the pool).
void MergeShip_Goal_AddItemsToPool(std::vector<RandomizerGet>& itemPool);
// MERGESHIP_SEED: shared.winCondition, shared.goal, and the goal's 2Ship options in the "mm" section.
std::string MergeShip_Goal_Name();
nlohmann::ordered_json MergeShip_Goal_SeedSection();
void MergeShip_Goal_PatchMMOptions(nlohmann::ordered_json& mmOptions);

// ---- in game -------------------------------------------------------------------------------------------------------
// hook_handlers.cpp MeetsWinconRequirements while MergeShip_Goal_OwnsWincon: the whole goal is met.
bool MergeShip_Goal_RuntimeMet();
// hook_handlers.cpp VB_SLAY_GANON while MergeShip_Goal_OwnsWincon: Ganon fell (RAND_INF_DUNGEONS_DONE_GANONS_TOWER).
void MergeShip_Goal_OnGanonDefeated();
// Triforce pieces found: Hyrule's counter, or the pieces found in either world in a unified seed.
uint8_t MergeShip_Goal_TriforcePieces();
// The adult altar's win-condition line (Hint::GetWinconText while MergeShip_Goal_OwnsWincon).
CustomMessage MergeShip_Goal_AltarText();
// What the goal takes, in plain English (notifications, logs).
std::string MergeShip_Goal_Describe();
// MergeShipShared_Capture: the goal's configuration and Hyrule's part of it.
void MergeShip_Goal_Capture(MsSharedSave* shared);

// Test harness (MergeShipTest.cpp): "randogoal" after randogen, and "goal <state|ganon|majora|clear|pieces n>".
std::vector<std::string> MergeShip_Goal_Summary();
std::vector<std::string> MergeShip_Goal_TestCommand(const std::vector<std::string>& args);

extern "C" {
#endif

// z_play.c CheckMedallionCount: MM's remains owned, when they count as medallions in a unified seed (0 otherwise).
uint8_t MergeShip_Goal_ExtraMedallions(void);

#ifdef __cplusplus
}
#endif
