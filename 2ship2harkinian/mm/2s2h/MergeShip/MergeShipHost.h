#pragma once

// MergeShip2 -- 2 Ship 2 Harkinian's side of the host (mergeship.exe, include/mergeship/EngineApi.h in the MergeShip2
// repository): Termina's end of the door to Hyrule (M3) and the shared save (M4). See
// Shipwright/mergeship2/docs/HOST_DESIGN.md.
//
// Arriving. The player comes from Hyrule's Lost Woods. Each Ocarina of Time file has its own Majora's Mask file (the
// same slot number, saves/fileN.json; SoH's unsaved debug file uses slot 3). If that file does not exist yet, it is
// created exactly as MM's file select creates one, named after the Hyrule file, and Majora's Mask begins as it always
// does: its opening in Termina's Lost Woods, the Skull Kid, the fall into the Clock Tower. Otherwise the file is
// loaded as MM loads it (its first-cycle save continues in South Clock Town out of the Clock Tower's door; after a
// Song of Time, MM's dawn of the first day). If the player is already in Termina in this session (they left through
// the door and came back), the cycle simply continues: they come out of the Clock Tower's door into South Clock Town.
//
// Leaving. The way back is the way Link came in: inside the Clock Tower, through the stone doors behind the Happy
// Mask Salesman's room and down the passage (the Clock Tower interior's exit 1, ENTRANCE(OPENING_DUNGEON, 1)). In
// MM the stone doors shut behind Link for good; with the door on, they stand open whenever Link enters the room from
// South Clock Town, and that exit takes him back to Hyrule's Lost Woods (the first clearing).
//
// Only in mergeship.exe, only for a player who came from Hyrule this session, and only with the CVar
// MS_CVAR_TERMINA_DOOR ("gEnhancements.MergeShip.TerminaDoor", the same name as SoH's option; menu "Mergeship").

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// Running inside mergeship.exe (2ship.dll), not 2ship.exe.
s32 MergeShipHost_IsHosted(void);
// Hosted, the door is on, and the player in Termina came from Hyrule.
s32 MergeShipHost_TerminaDoorActive(void);
// The shared save (MsSharedSave, include/mergeship/SharedSave.h) of the Hyrule file this Termina belongs to, or NULL
// (not hosted, or not a Termina that came from Hyrule). The unified randomizer's goal reads and writes its own fields
// there (2s2h/Rando/MergeShip/UnifiedGoal.h); CommitSharedSave writes it to disk.
void* MergeShipHost_SharedSave(void);
void MergeShipHost_CommitSharedSave(void);
// Termina takes part in the shared save now: Majora's Mask's prologue is over, or "Share items during Termina's first
// cycle" is on for this trip -- the rule the door's capture follows (Anchor's team state merges follow it too).
s32 MergeShipHost_SharingAllowed(void);
// Sram_ResetSaveFromMoonCrash (z_sram_NES.c): the file was read back from flash, without what the arrival applied from
// Hyrule (never saved there): apply the shared save again, as an arrival does.
void MergeShipHost_OnMoonCrashReset(void);

// func_808354A4 (z_player.c): the player took a scene exit (play->nextEntrance is set).
void MergeShipHost_OnSceneExit(PlayState* play);
// Play_UpdateTransition (z_play.c), where each transition kind commits play->nextEntrance. True: do not commit this
// frame (the door is waiting for the host). False: commit play->nextEntrance, which this may have replaced.
s32 MergeShipHost_HoldTransition(PlayState* play);
// BgCtowerRot_Init: the stone doors between the salesman's room and the passage stand open (the way back to Hyrule).
s32 MergeShipHost_StoneDoorsStayOpen(void);
// Play_Init (z_play.c), before the scene builds its sky from the day (Environment_Init): the Termina file's calendar is
// one Majora's Mask can play -- a raw day past 4 folded into the day the game reads, and day 4 only for the moon's crash
// (the dark "day 4" sky, MergeShipHost.cpp FoldCalendar).
void MergeShipHost_CheckCalendar(PlayState* play);
// A warp song to one of Ocarina of Time's warp pads (MergeShip_RequestHyruleWarp): the same trip back as the door,
// with the file slot, the shared save and Link's body. False when this Termina has no Hyrule file to go back to.
s32 MergeShipHost_RequestHyruleWarp(u16 ootEntrance);
// BUG-0036: Anchor's warp to a teammate in Hyrule -- Hyrule can be reached from this Termina, and the trip there,
// arriving at the teammate's place (Ocarina of Time entrance, room, position, yaw).
s32 MergeShipHost_CanReachHyrule(void);
s32 MergeShipHost_RequestHyruleAnchorWarp(s32 ootEntrance, s32 room, const f32 pos[3], s32 yaw);

// padmgr.c: the test script's controller (MERGESHIP_TEST_SCRIPT_2SHIP), a no-op otherwise.
void MergeShipHostTest_OverrideInput(OSContPad* pad);
// Test log (a no-op without a script).
void MergeShipHostTest_Log(const char* fmt, ...);
const char* MergeShipHost_DoorState(void);

#ifdef __cplusplus
}

struct MsHostServices;
struct MsTransfer;
// EngineExport.cpp
void MergeShipHost_SetServices(const MsHostServices* services);
// Before each frame: starts a pending arrival once the game has a game state to replace.
void MergeShipHost_BeforeFrame();
// True once after the frame in which a transfer was requested.
bool MergeShipHost_TakeTransferRequested();
void MergeShipHost_OnResume(const MsTransfer* arrival);
// MergeShipHostTest.cpp
void MergeShipHostTest_Init();
void MergeShipHostTest_Frame();
#endif
