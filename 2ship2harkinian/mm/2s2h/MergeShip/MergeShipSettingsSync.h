#pragma once

// MergeShip2 -- 2 Ship 2 Harkinian's side of settings sharing (include/mergeship/SettingsSync.h in the MergeShip2
// repository): the options the player sets in Ship of Harkinian (frame rate, V-Sync, volumes, D-pad items and boots,
// camera, time savers, cheats, colours...) are copied into 2Ship's CVars at its start and on every resume, and what the
// player changes in 2Ship's own menu goes back to SoH on suspend. mergeship.exe only (MERGESHIP_HOSTED); SoH's
// Mergeship menu "Share Settings with Termina" turns it off.

#ifdef __cplusplus
struct MsHostServices;

// EngineExport.cpp, in MsEngine_Init before the game starts.
void MergeShipSettings_SetServices(const MsHostServices* services);
// With 2Ship's Ship::Context active: SoH's values -> 2Ship's CVars, then re-run what depends on the changed ones.
// `why` names the occasion in the log ("init", "resume"). Returns how many 2Ship CVars changed.
int MergeShipSettings_Pull(const char* why);
// With 2Ship's Ship::Context still active, before it is suspended: values the player changed in 2Ship -> SoH's CVars.
// Returns how many SoH CVars were written.
int MergeShipSettings_PushBack();
// SoH shares its settings with this 2Ship (hosted, SoH loaded, its option on).
bool MergeShipSettings_Active();
// Test log: every row's SoH and 2Ship values (MergeShipHostTest.cpp "settings").
void MergeShipSettings_LogAll();
#endif
