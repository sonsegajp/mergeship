#pragma once
// MergeShip2 -- singleplayer mode in Termina (Shipwright soh/MergeShip/MergeShipSingleplayer.h; docs/SINGLEPLAYER.md).
//
// Hyrule turns it on for a trip with MS_TRANSFER_FLAG_SINGLEPLAYER (never for a randomizer file; and never here on a
// Termina randomizer file). Termina is then Ocarina of Time's New Game Plus:
//   * Navi in Tatl's place: Ocarina of Time's Navi colours when she rests (white and blue), Navi's voice (Ocarina of
//     Time's own samples in place of Tatl's), the C-Up label "Navi", and her lines -- Tatl's name and the story lines
//     about Tatl and her brother told as Navi's (MergeShipSingleplayer.cpp, kStoryLines);
//   * the Fierce Deity's Mask only from the Happy Mask Salesman in Hyrule, unless the menu also lets the moon's child
//     give it (MS_TRANSFER_FLAG_SP_FD_FROM_MOON);
//   * Ocarina of Time's items and the Adult Mask are Termina's already (MergeShipOoTItems.h, MergeShipAdultMask.h).

#include "ultra64.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MsTransfer;

// Every trip from Hyrule: whether it is a singleplayer file's.
void MergeShip_Singleplayer_OnTransfer(const struct MsTransfer* arrival);
// Singleplayer, on a file that is not a Termina randomizer file.
s32 MergeShip_Singleplayer_Active(void);
// The moon's child keeps the Fierce Deity's Mask (singleplayer, and the menu does not let the moon give it).
s32 MergeShip_Singleplayer_MoonKeepsFierceDeity(void);
// The C-Up label: Ocarina of Time's "Navi" (32x8 IA4), or NULL for Majora's Mask's "Tatl" (32x12).
const char* MergeShip_Singleplayer_CUpLabel(void);
// Tests: turn it on or off without a trip.
void MergeShip_Singleplayer_DebugSet(s32 active, s32 fdFromMoon);

#ifdef __cplusplus
}
#endif
