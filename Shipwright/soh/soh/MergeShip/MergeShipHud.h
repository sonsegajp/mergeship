#pragma once

// MergeShip2 -- Majora's Mask's button labels for its forms.
//
// MM's do-action labels ("Punch", "Curl", the Deku's hop count ...) are MM textures, read from
// mm.o2r's do_action_static. They share Ocarina of Time's A-button label machinery by using values
// from MERGESHIP_MM_DO_ACTION_BASE up, so OoT's own labels are untouched.

#include "z64.h"

// MM DoAction (z64interface.h), MM's values.
typedef enum MMDoAction {
    MM_DO_ACTION_UNDEFINED = -1,
    MM_DO_ACTION_ATTACK = 0,
    MM_DO_ACTION_CHECK,
    MM_DO_ACTION_ENTER,
    MM_DO_ACTION_RETURN,
    MM_DO_ACTION_OPEN,
    MM_DO_ACTION_JUMP,
    MM_DO_ACTION_DECIDE,
    MM_DO_ACTION_DIVE,
    MM_DO_ACTION_FASTER,
    MM_DO_ACTION_THROW,
    MM_DO_ACTION_NONE, // 0x0A
    MM_DO_ACTION_CLIMB,
    MM_DO_ACTION_DROP,
    MM_DO_ACTION_DOWN,
    MM_DO_ACTION_QUIT,
    MM_DO_ACTION_SPEAK,
    MM_DO_ACTION_NEXT,
    MM_DO_ACTION_GRAB,
    MM_DO_ACTION_STOP,
    MM_DO_ACTION_PUTAWAY,
    MM_DO_ACTION_REEL,
    MM_DO_ACTION_INFO,
    MM_DO_ACTION_WARP,
    MM_DO_ACTION_SNAP,
    MM_DO_ACTION_EXPLODE,
    MM_DO_ACTION_DANCE,
    MM_DO_ACTION_MARCH,
    MM_DO_ACTION_1,
    MM_DO_ACTION_2,
    MM_DO_ACTION_3,
    MM_DO_ACTION_4,
    MM_DO_ACTION_5,
    MM_DO_ACTION_6,
    MM_DO_ACTION_7,
    MM_DO_ACTION_8,
    MM_DO_ACTION_CURL,
    MM_DO_ACTION_SURFACE,
    MM_DO_ACTION_SWIM,
    MM_DO_ACTION_PUNCH,
    MM_DO_ACTION_POUND,
    MM_DO_ACTION_HOOK,
    MM_DO_ACTION_SHOOT,
    MM_DO_ACTION_MAX
} MMDoAction;

#define MERGESHIP_MM_DO_ACTION_BASE 0x100
#define MERGESHIP_MM_DO_ACTION(a) (MERGESHIP_MM_DO_ACTION_BASE + (a))

#ifdef __cplusplus
extern "C" {
#endif

// MM texture path for an MM do action, or NULL for MM_DO_ACTION_NONE.
const char* MergeShip_GetMMDoActionTexture(s32 mmDoAction);

// MM Interface_SetAButtonDoAction.
void MergeShip_SetMMAButtonDoAction(PlayState* play, s32 mmDoAction);
// MM Interface_SetBButtonPlayerDoAction / clearing bButtonPlayerDoActionActive.
void MergeShip_SetMMBButtonDoAction(PlayState* play, s32 mmDoAction);
void MergeShip_ClearMMBButtonDoAction(PlayState* play);
// The B label the HUD should draw instead of the B item, or NULL.
const char* MergeShip_GetMMBButtonLabel(void);

#ifdef __cplusplus
}
#endif
