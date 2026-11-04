#pragma once

// MergeShip2 -- Termina from Hyrule: MM's Song of Soaring owl-warp map in Ocarina of Time's pause menu, and the hook
// the host integration will turn into the engine transfer (Shipwright/mergeship2/docs/HOST_DESIGN.md).
//
// MM (z_message.c MSGMODE_TEXT_CLOSING, z_kaleido_scope_NES.c PAUSE_STATE_OWL_WARP_*): after the Song of Soaring the
// pause menu opens on the map page with Termina's world map, an owl face on every owl statue the player has
// activated, and a cursor that steps between them; A asks "Soar to <place>?", Yes closes the menu and MM's En_Test7
// flies Link to the statue. Here the map is the same (MM's gWorldMapImageTex / TLUT, gWorldMapOwlFaceTex at MM's
// map-page coordinates, MM's spinning cursor and colours, MM's owl names), inside Ocarina of Time's map face; Yes
// closes the menu and calls MergeShip_RequestTerminaWarp. Termina's scenes are the other engine's, so until the
// host implements that transfer the request logs and shows MM's "can't warp" answer.

#include "z64.h"

// MM OWL_WARP_* (z64scene.h): the owl statues, in MM's order and bits of owlActivationFlags.
typedef enum MergeShipOwlWarp {
    /* 0 */ MERGESHIP_OWL_GREAT_BAY_COAST,
    /* 1 */ MERGESHIP_OWL_ZORA_CAPE,
    /* 2 */ MERGESHIP_OWL_SNOWHEAD,
    /* 3 */ MERGESHIP_OWL_MOUNTAIN_VILLAGE,
    /* 4 */ MERGESHIP_OWL_CLOCK_TOWN,
    /* 5 */ MERGESHIP_OWL_MILK_ROAD,
    /* 6 */ MERGESHIP_OWL_WOODFALL,
    /* 7 */ MERGESHIP_OWL_SOUTHERN_SWAMP,
    /* 8 */ MERGESHIP_OWL_IKANA_CANYON,
    /* 9 */ MERGESHIP_OWL_STONE_TOWER,
    /* 10 */ MERGESHIP_OWL_MAX
} MergeShipOwlWarp;

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct GraphicsContext;

// ---- For the host integration ----------------------------------------------------------------------------------
// The player confirmed "Soar to <owl>?" (MM PAUSE_STATE_OWL_WARP_CONFIRM, sCursorPointsToOcarinaModes[owl]). Called
// once the pause menu has closed. MM would now play En_Test7's soaring cutscene and load the statue's entrance
// (MM sOwlWarpEntrances). Stub: logs, and shows MM's "Your notes echoed far... but nothing happened." (MM 0x1B95, the
// answer MM gives wherever the song cannot take Link).
void MergeShip_RequestTerminaWarp(u8 owlIndex);
// MM saveInfo.playerData.owlActivationFlags as Hyrule sees it: the MergeShip save's copy (gMergeShipSave), or every
// statue with CVAR_ENHANCEMENT("MergeShip.MMSongs.AllOwls") (on by default: no statue can be struck in Hyrule).
u16 MergeShip_Termina_GetOwlActivationFlags(void);
s32 MergeShip_Termina_IsOwlActive(u8 owlIndex);
// Marks a statue activated in the MergeShip save (MM ObjWarpstone: owlActivationFlags |= 1 << owl), for the host
// (a shared save) or a test.
void MergeShip_Termina_ActivateOwl(u8 owlIndex);
// MM sOwlWarpTextENG.
const char* MergeShip_Termina_OwlName(u8 owlIndex);

// ---- The owl-warp map (MergeShipTermina.c / z_kaleido_mergeship.c) ----------------------------------------------
// MM: pauseCtx->state = PAUSE_STATE_OWL_WARP_0 with the map page. Opens Ocarina of Time's pause menu on its map face
// in owl-warp mode.
void MergeShip_OwlWarp_Open(struct PlayState* play);
// True from MergeShip_OwlWarp_Open until the pause menu has closed again.
s32 MergeShip_OwlWarp_IsActive(void);
// The owl under the cursor.
u8 MergeShip_OwlWarp_GetCursorOwl(void);
// Once per frame: finishes the mode when the menu has closed, and makes the pending warp request.
void MergeShip_OwlWarp_FrameUpdate(struct PlayState* play);

// Kaleido hooks (z_kaleido_mergeship.c).
// KaleidoScope_Update, pauseCtx->state 6 (MM PAUSE_STATE_OWL_WARP_SELECT / _CONFIRM): replaces the main state's input.
void MergeShip_OwlWarp_UpdateMain(struct PlayState* play);
// KaleidoScope_DrawWorldMap: MM KaleidoScope_DrawWorldMap in owl-warp mode (map image, owl faces, cursor).
void MergeShip_OwlWarp_DrawWorldMap(struct PlayState* play, struct GraphicsContext* gfxCtx);
// KaleidoScope_UpdateNamePanel: MM KaleidoScope_UpdateOwlWarpNamePanel (map_name_static's owl names).
void MergeShip_OwlWarp_UpdateNamePanel(struct PlayState* play);
// BUG-0017: C-Up on the pause screen's world map shows Termina's (the owl-warp map, to look at).
s32 MergeShip_TerminaMap_IsShown(struct PlayState* play);

// Internal: pause-state plumbing shared by the two files.
void MergeShip_OwlWarp_SetCursorOwl(u8 owlIndex);
void MergeShip_OwlWarp_Confirmed(u8 owlIndex); // Yes: the request is made once the menu has closed
void MergeShip_OwlWarp_ClosePause(struct PlayState* play);

#ifdef __cplusplus
}
#endif
