// MergeShip2 -- Termina from Hyrule (MergeShipTermina.h): the owl statues, the owl-warp pause mode's state, and the
// warp request the host integration will implement. The owl-warp map's drawing and input are kaleido code
// (src/overlays/misc/ovl_kaleido_scope/z_kaleido_mergeship.c).

#include "global.h"
#include "soh/MergeShip/MergeShipTermina.h"
#include "soh/MergeShip/MergeShipSongs.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipPause.h"
#include "soh/MergeShip/MergeShipHost.h"

void MergeShipTest_Log(const char* fmt, ...);

// MM sOwlWarpTextENG (z_message_nes.c).
static const char* sOwlNames[MERGESHIP_OWL_MAX] = {
    "Great Bay Coast", "Zora Cape", "Snowhead",       "Mountain Village", "Clock Town",
    "Milk Road",       "Woodfall",  "Southern Swamp", "Ikana Canyon",     "Stone Tower",
};

static s32 sOwlWarpActive = false;
static u8 sOwlWarpCursor = MERGESHIP_OWL_CLOCK_TOWN;
static u8 sPendingWarpOwl = 0xFF;
static u8 sSavedPageIndex = PAUSE_ITEM;
static s16 sSavedWorldMapCursor = 0;

// ---- The owl statues ------------------------------------------------------------------------------------------

u16 MergeShip_Termina_GetOwlActivationFlags(void) {
    // Off by default: the statues Link has struck in Termina or whose owl statue item he has found (the user,
    // 2026-09-24), shared with Termina's file through the shared save.
    if (CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMSongs.AllOwls"), 0)) {
        return (1 << MERGESHIP_OWL_MAX) - 1;
    }
    return gMergeShipSave.owlActivationFlags;
}

s32 MergeShip_Termina_IsOwlActive(u8 owlIndex) {
    return (owlIndex < MERGESHIP_OWL_MAX) && ((MergeShip_Termina_GetOwlActivationFlags() >> owlIndex) & 1);
}

void MergeShip_Termina_ActivateOwl(u8 owlIndex) {
    if (owlIndex < MERGESHIP_OWL_MAX) {
        gMergeShipSave.owlActivationFlags |= (1 << owlIndex);
    }
}

const char* MergeShip_Termina_OwlName(u8 owlIndex) {
    return (owlIndex < MERGESHIP_OWL_MAX) ? sOwlNames[owlIndex] : "";
}

// ---- The host's hook ------------------------------------------------------------------------------------------

void MergeShip_RequestTerminaWarp(u8 owlIndex) {
    PlayState* play = gPlayState;

    // In mergeship.exe the host hands the player to Majora's Mask at the statue (MS_TRANSFER_FLAG_OWL_WARP).
    const s32 warped = MergeShipHost_RequestOwlWarp(play, owlIndex);
    MergeShipTest_Log("TERMINAWARP owl=%d (%s) %s", owlIndex, MergeShip_Termina_OwlName(owlIndex),
                      warped ? "soaring to Termina" : "Termina is not reachable (soh.exe, or the door is off)");
    if (warped) {
        return;
    }

    // MM's answer where the song cannot take Link (MM 0x1B95, OCARINA_MODE_PROCESS_RESTRICTED_SONG).
    if ((play != NULL) && (play->msgCtx.msgMode == MSGMODE_NONE)) {
        Message_StartTextbox(play, MERGESHIP_TEXT_NOTHING_HAPPENED, NULL);
    }
}

// ---- The owl-warp pause mode ----------------------------------------------------------------------------------

s32 MergeShip_OwlWarp_IsActive(void) {
    return sOwlWarpActive;
}

u8 MergeShip_OwlWarp_GetCursorOwl(void) {
    return sOwlWarpCursor;
}

void MergeShip_OwlWarp_SetCursorOwl(u8 owlIndex) {
    if (owlIndex < MERGESHIP_OWL_MAX) {
        sOwlWarpCursor = owlIndex;
    }
}

void MergeShip_OwlWarp_Confirmed(u8 owlIndex) {
    sPendingWarpOwl = owlIndex;
}

// OoT's START in the pause menu's main state (z_kaleido_scope_PAL.c state 6), which is also how MM's owl-warp
// states leave (PAUSE_STATE_OWL_WARP_6 -> PAUSE_STATE_UNPAUSE_CLOSE).
void MergeShip_OwlWarp_ClosePause(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    Interface_SetDoAction(play, DO_ACTION_NONE);
    pauseCtx->state = 0x12;
    WREG(2) = -6240;
    func_800F64E0(0);
}

// MM z_message.c MSGMODE_TEXT_CLOSING (Song of Soaring, owls activated): pauseCtx->state = PAUSE_STATE_OWL_WARP_0 on
// the map page, after func_800F4A10 put the cursor on the first activated statue -- Clock Town's when it is one.
// Ocarina of Time's menu opens the way KaleidoSetup_Update opens it on START, landing on its map face.
void MergeShip_OwlWarp_Open(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 owl;

    if (pauseCtx->state != 0) {
        return;
    }

    sSavedPageIndex = pauseCtx->pageIndex;
    sSavedWorldMapCursor = pauseCtx->cursorPoint[PAUSE_WORLD_MAP];
    sPendingWarpOwl = 0xFF;

    for (owl = MERGESHIP_OWL_STONE_TOWER; owl >= MERGESHIP_OWL_GREAT_BAY_COAST; owl--) {
        if (MergeShip_Termina_IsOwlActive(owl)) {
            sOwlWarpCursor = owl;
        }
    }
    if (MergeShip_Termina_IsOwlActive(MERGESHIP_OWL_CLOCK_TOWN)) {
        sOwlWarpCursor = MERGESHIP_OWL_CLOCK_TOWN;
    }
    sOwlWarpActive = true;

    // KaleidoSetup_Update, START (with the last page set to the map face).
    gSaveContext.prevHudVisibilityMode = gSaveContext.hudVisibilityMode;
    WREG(16) = -175;
    WREG(17) = 155;
    pauseCtx->unk_1EA = 0;
    pauseCtx->unk_1E4 = 1;
    pauseCtx->pageIndex = PAUSE_MAP;
    if (ZREG(48) == 0) {
        pauseCtx->pageIndex = MergeShip_Pause_NextPage(MergeShip_Pause_NextPage(pauseCtx->pageIndex, true), true);
    } else {
        pauseCtx->pageIndex = MergeShip_Pause_NextPage(pauseCtx->pageIndex, true);
    }
    MergeShip_Pause_GetEye(pauseCtx->pageIndex, &pauseCtx->eye.x, &pauseCtx->eye.z);
    pauseCtx->mode = (u16)(pauseCtx->pageIndex * 2) + 1;
    pauseCtx->state = 1;

    WREG(2) = -6240;
    R_UPDATE_RATE = 2;
    if (ShrinkWindow_GetVal()) {
        Letterbox_SetSizeTarget(0);
    }
    func_800F64E0(1);

    MergeShipTest_Log("OWLWARP open cursor=%d owls=0x%03X", sOwlWarpCursor, MergeShip_Termina_GetOwlActivationFlags());
}

// MM PAUSE_STATE_OWL_WARP_6 -> unpaused: MM restores the page and world-map cursor it saved (unk_2C8 / unk_2CA).
void MergeShip_OwlWarp_FrameUpdate(PlayState* play) {
    u8 owl;

    if (!sOwlWarpActive || (play == NULL) || (play->pauseCtx.state != 0)) {
        return;
    }
    sOwlWarpActive = false;
    play->pauseCtx.pageIndex = sSavedPageIndex;
    play->pauseCtx.cursorPoint[PAUSE_WORLD_MAP] = sSavedWorldMapCursor;

    owl = sPendingWarpOwl;
    sPendingWarpOwl = 0xFF;
    if (owl != 0xFF) {
        MergeShip_RequestTerminaWarp(owl);
    }
}
