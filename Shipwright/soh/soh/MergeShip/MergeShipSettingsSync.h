#pragma once

// MergeShip2 -- Ship of Harkinian's side of settings sharing (include/mergeship/SettingsSync.h in the MergeShip2
// repository). 2 Ship 2 Harkinian copies the options set here into its own config every time it resumes, and writes
// back into SoH's CVars what the player changed in its menu (last writer wins). SoH is suspended then: on resume it
// re-runs what depends on a CVar that changed meanwhile (ShipInit, as the menu does; the window's MSAA and resolution;
// the volumes). The switch is the Mergeship menu's "Share Settings with Termina" (MS_CVAR_SHARE_SETTINGS, default on).
// mergeship.exe only (MERGESHIP_HOSTED): soh.exe gets no-ops.

#ifdef __cplusplus
// EngineExport.cpp: MsEngine_Suspend (before SoH's CVars are saved) and MsEngine_Resume (SoH's context active).
void MergeShipSettings_OnSuspend();
void MergeShipSettings_OnResume();
#endif
