#ifndef RANDO_MERGESHIP_UNIFIED_GOAL_H
#define RANDO_MERGESHIP_UNIFIED_GOAL_H

// MergeShip2 -- the unified randomizer's goal on 2Ship's side (SoH: soh/MergeShip/MergeShipGoal.h, the settings and
// generation; Shipwright/mergeship2/docs/UNIFIED_RANDO_DESIGN.md "Win condition").
//
// SoH writes the goal into the host's shared save (MsSharedSave.goalType ... goalSeed, include/mergeship/SharedSave.h);
// this file reads it live whenever the Majora's Mask file plays that seed (a randomizer file whose finalSeed is
// goalSeed). Without one (2ship.exe, a vanilla or standalone randomizer file, an off seed) nothing here does anything.
//
//   Majora's defeat  Majora's Wrath's death sends Link to MM's ending (Termina Field, cutscene 0xFFF7). Termina's
//                    part (MS_GOAL_FLAG_MAJORA) is written and committed at once. If that completes the goal (Majora,
//                    or Ganon and Majora with Hyrule's part done) the ending plays as in Majora's Mask; otherwise a
//                    message says what is left and the Song of Time takes Link back to the dawn of the first day.
//   Moon child       Majora requires MsSharedSave.majoraFormMasks transformation masks on top of 2Ship's own
//                    RO_ACCESS_MAJORA_* counts (text 0x21FC refuses, as 2Ship's own check does).
//   Triforce Hunt    pieces found in either world (the shared save's ledger, or 2Ship's own count, whichever is
//                    larger) reaching MsSharedSave.triforceRequired play MM's ending, as 2Ship's own hunt does. The
//                    seed sets 2Ship's RO_TRIFORCE_PIECES_REQUIRED out of reach so 2Ship's hunt never ends the game.
//   Clock Tower      arriving on the rooftop shows what the goal takes.

#include <string>
#include <vector>

namespace Rando {

namespace MergeShip {

// The Majora's Mask file plays the unified seed whose goal is in the shared save (or the harness forces one).
bool GoalActive();
// What the goal takes, in plain English.
std::string GoalDescription();

// Test harness (MergeShipHostTest.cpp / MergeShipTest.cpp): "goal <op> ..."; false when `op` is not "goal".
//   goal state                          log the goal and Termina's part
//   goal fake <type> [required] [forms] a goal of this type in a local shared save (no host needed; MS_GOAL_*)
//   goal usehost [type]                 the host's shared save, without the randomizer-file checks (type: set goalType)
//   goal hyrule <0|1>                   Hyrule's part, as SoH writes it
//   goal majora                         Majora's Wrath's death: the ending transition MM starts (Majora's lair only
//                                       in game; the harness plays it wherever Link is)
//   goal pieces <n>                     2Ship's own Triforce piece count
//   goal warp <entrance>                to an MM entrance (e.g. 0x2C00, the Clock Tower rooftop)
bool GoalTestCommand(const std::string& op, const std::vector<std::string>& args);

} // namespace MergeShip

} // namespace Rando

#endif
