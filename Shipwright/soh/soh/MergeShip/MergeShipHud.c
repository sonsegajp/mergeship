#include "global.h"
#include "soh/MergeShip/MergeShipHud.h"

// MergeShip2 -- MM's do-action labels. See the header.

#define MM_DO_ACTION_TEX(name) "__OTR__mm/do_action_static/gDoAction" name "ENGTex"

// MM doActionTbl (2S2H z_parameter.c), in MM DoAction order. MM_DO_ACTION_NONE's slot holds Navi's
// label in do_action_static but is never drawn.
static const char* sMMDoActionTextures[MM_DO_ACTION_MAX] = {
    MM_DO_ACTION_TEX("Attack"), MM_DO_ACTION_TEX("Check"),   MM_DO_ACTION_TEX("Enter"),   MM_DO_ACTION_TEX("Return"),
    MM_DO_ACTION_TEX("Open"),   MM_DO_ACTION_TEX("Jump"),    MM_DO_ACTION_TEX("Decide"),  MM_DO_ACTION_TEX("Dive"),
    MM_DO_ACTION_TEX("Faster"), MM_DO_ACTION_TEX("Throw"),   MM_DO_ACTION_TEX("Navi"),    MM_DO_ACTION_TEX("Climb"),
    MM_DO_ACTION_TEX("Drop"),   MM_DO_ACTION_TEX("Down"),    MM_DO_ACTION_TEX("Quit"),    MM_DO_ACTION_TEX("Speak"),
    MM_DO_ACTION_TEX("Next"),   MM_DO_ACTION_TEX("Grab"),    MM_DO_ACTION_TEX("Stop"),    MM_DO_ACTION_TEX("PutAway"),
    MM_DO_ACTION_TEX("Reel"),   MM_DO_ACTION_TEX("Info"),    MM_DO_ACTION_TEX("Warp"),    MM_DO_ACTION_TEX("Snap"),
    MM_DO_ACTION_TEX("Explode"), MM_DO_ACTION_TEX("Dance"),  MM_DO_ACTION_TEX("March"),   MM_DO_ACTION_TEX("Num1"),
    MM_DO_ACTION_TEX("Num2"),   MM_DO_ACTION_TEX("Num3"),    MM_DO_ACTION_TEX("Num4"),    MM_DO_ACTION_TEX("Num5"),
    MM_DO_ACTION_TEX("Num6"),   MM_DO_ACTION_TEX("Num7"),    MM_DO_ACTION_TEX("Num8"),    MM_DO_ACTION_TEX("Curl"),
    MM_DO_ACTION_TEX("Surface"), MM_DO_ACTION_TEX("Swim"),   MM_DO_ACTION_TEX("Punch"),   MM_DO_ACTION_TEX("Pound"),
    MM_DO_ACTION_TEX("Hook"),   MM_DO_ACTION_TEX("Shoot"),
};

static const char* sMMBButtonLabel = NULL;

const char* MergeShip_GetMMDoActionTexture(s32 mmDoAction) {
    if ((mmDoAction < 0) || (mmDoAction >= MM_DO_ACTION_MAX) || (mmDoAction == MM_DO_ACTION_NONE)) {
        return NULL;
    }
    return sMMDoActionTextures[mmDoAction];
}

void MergeShip_SetMMAButtonDoAction(PlayState* play, s32 mmDoAction) {
    // OoT's Interface_SetDoAction runs MM's same A-button state machine; the label loader resolves
    // values from MERGESHIP_MM_DO_ACTION_BASE up to MM textures.
    Interface_SetDoAction(play, (mmDoAction == MM_DO_ACTION_NONE) ? DO_ACTION_NONE : MERGESHIP_MM_DO_ACTION(mmDoAction));
}

void MergeShip_SetMMBButtonDoAction(PlayState* play, s32 mmDoAction) {
    sMMBButtonLabel = MergeShip_GetMMDoActionTexture(mmDoAction);
}

void MergeShip_ClearMMBButtonDoAction(PlayState* play) {
    sMMBButtonLabel = NULL;
}

const char* MergeShip_GetMMBButtonLabel(void) {
    return sMMBButtonLabel;
}
