#pragma once

// MergeShip2 -- Ship of Harkinian's side of the host (mergeship.exe, include/mergeship/EngineApi.h): the door to
// Termina and the shared save (Shipwright/mergeship2/docs/HOST_DESIGN.md, M3 and M4).
//
// The door. Getting lost in Hyrule's Lost Woods leads to Termina, the way Majora's Mask begins: Link lost in the
// woods. In the Lost Woods every wrong turn is scene exit 1 (ENTR_KOKIRI_FOREST_UPPER_EXIT), which Ocarina of Time
// uses to throw the player back out to Kokiri Forest. Taken anywhere but at the opening the woods' Kokiri Forest
// entrance leads in through (where the same exit 1 is the real way back to Kokiri Forest), it becomes the reserved
// entrance ENTR_MERGESHIP_TERMINA. When the fade-out commits that entrance, instead of loading a scene the game captures the
// shared save, asks the host to hand the player to 2 Ship 2 Harkinian (Majora's Mask), and holds the black frame.
// When the player comes back through Termina's door, the host resumes this engine with an arrival; the held
// transition then commits the arrival entrance (ENTR_LOST_WOODS_SOUTH_EXIT, the woods' first clearing) instead.
//
// Only in mergeship.exe and only with CVAR_ENHANCEMENT("MergeShip.TerminaDoor") (Mergeship menu, "Termina",
// default on). soh.exe, or the option off, keeps Ocarina of Time's exit.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// Running inside mergeship.exe (soh.dll), not soh.exe.
s32 MergeShipHost_IsHosted(void);
// Hosted, and the Termina door is on.
s32 MergeShipHost_TerminaDoorEnabled(void);
// The host's shared save of this file slot (MsSharedSave, include/mergeship/SharedSave.h), or NULL outside
// mergeship.exe. The unified randomizer's goal reads and writes its own fields there (MergeShipGoal.h).
void* MergeShipHost_SharedSave(void);
// Writes this file slot's shared save to disk (the unified randomizer's ledger, MergeShipUnified.h). No-op outside
// mergeship.exe and for the unsaved debug file.
void MergeShipHost_CommitSharedSave(void);

// Player_HandleExitsAndVoids (z_player.c): a scene exit was taken; returns the entrance to use instead, which is
// ENTR_MERGESHIP_TERMINA for the door and `nextEntranceIndex` otherwise.
s16 MergeShipHost_OnSceneExit(PlayState* play, s16 nextEntranceIndex);

// Play_Update (z_play.c), where each transition kind commits play->nextEntranceIndex. True: do not commit this frame
// (the door is waiting for the host; the screen stays as it is, faded out). False: commit play->nextEntranceIndex,
// which this may have replaced with the arrival from Termina.
s32 MergeShipHost_HoldTransition(PlayState* play);

// MergeShipTermina.c MergeShip_RequestTerminaWarp: the Song of Soaring from Hyrule to Termina's owl statue `owl`
// (MergeShipOwlWarp). Fades out to white and hands the player to Majora's Mask at that statue (MS_TRANSFER_FLAG_OWL_WARP),
// the way the door does. False when the door is off (soh.exe, or the option off): nothing happens.
s32 MergeShipHost_RequestOwlWarp(PlayState* play, u8 owl);
// BUG-0036: Anchor's warp to a teammate in Termina: a fade-out, then the trip to that Majora's Mask entrance, arriving
// at the teammate's place (room, position, yaw in Termina's scene). False when Termina cannot be reached.
s32 MergeShipHost_RequestAnchorWarp(PlayState* play, s32 mmEntrance, s32 room, const f32 pos[3], s32 yaw);

// OnLoadGame (both this module's and the unified randomizer's hook, whichever runs first): a new file or a new seed in
// the slot starts the slot anew -- the shared save is wiped before anything reads it, and Termina's file of the slot is
// made again on the next trip (MsSharedSave.terminaFileStale). The shared save then belongs to this file. No-op when it
// already does, and outside mergeship.exe.
void MergeShipHost_ClaimSharedSave(s32 fileNum);
// MergeShipSave.cpp: this file's id in its slot's shared save (MsSharedSave.hyruleFileId), 0 for an older file.
u32 MergeShip_Save_SharedFileId(void);

// Test harness: the last door event, for logs.
const char* MergeShipHost_DoorState(void);

#ifdef __cplusplus
}

struct MsHostServices;
struct MsTransfer;
// EngineExport.cpp
void MergeShipHost_SetServices(const MsHostServices* services);
// After the game has started: the shared save's load/save hooks (M4).
void MergeShipHost_RegisterHooks();
// True once after the frame in which a transfer was requested (MsEngine_RunFrame returns MS_FRAME_TRANSFER).
bool MergeShipHost_TakeTransferRequested();
// MsEngine_Resume: `arrival` is the transfer that brought the player here, or nullptr.
void MergeShipHost_OnResume(const MsTransfer* arrival);
// First run (OTRGlobals::RunExtract): another loaded engine's ROM extractor, through the host (MsHostServices.ExtractRom),
// usable before that engine has started. CanExtractRom: hosted, and the engine (MsEngineId) has one. ExtractRom builds
// its archive from romPath into destDir, blocking; progress (may be null) is called from this thread.
#include <cstdint>
#include <string>
// A line in the host's log (mergeship-host.log): level 0 info, 1 warning, 2 error. No-op outside mergeship.exe.
void MergeShipHost_Log(int32_t level, const std::string& message);
bool MergeShipHost_CanExtractRom(int32_t engineId);
bool MergeShipHost_ExtractRom(int32_t engineId, const std::string& romPath, const std::string& destDir,
                              void (*progress)(void* user, uint64_t done, uint64_t total), void* user);
#endif
