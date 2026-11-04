#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/Rando/Rando.h"

extern "C" {
#include "variables.h"
}

#define CVAR_NAME "gEnhancements.Cutscenes.SkipEntranceCutscenes"
// MergeShip2 (BUG-0052/0053): always on a randomizer file, as Ocarina of Time's randomizer skips its own story
// scenes.
#define CVAR (CVarGetInteger(CVAR_NAME, 0) || IS_RANDO)

void RegisterSkipEntranceCutscenes() {
    COND_VB_SHOULD(VB_PLAY_ENTRANCE_CS, CVAR, { *should = false; });

    COND_VB_SHOULD(VB_START_CUTSCENE, CVAR, {
        s16* csId = va_arg(args, s16*);
        Actor* actor = va_arg(args, Actor*);

        if (actor == NULL || actor->id != ACTOR_OBJ_DEMO) {
            return;
        }

        // Skip the entrance cutscene in the castle
        if (gPlayState->sceneId == SCENE_CASTLE && (*csId == 0 || *csId == 1)) {
            *should = false;
        }

        // Snowhead entrance cutscene
        if (gPlayState->sceneId == SCENE_HAKUGIN && (*csId == 24 || *csId == 25)) {
            *should = false;
        }

        // Pirate Fortress entrance cutscene
        if (gPlayState->sceneId == SCENE_TORIDE && *csId == 12) {
            *should = false;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipEntranceCutscenes, { CVAR_NAME, "IS_RANDO" });
