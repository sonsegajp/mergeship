#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/MergeShip/MergeShipHost.h"

extern "C" {
#include "variables.h"
void Flags_SetWeekEventReg(s32 flag);
void MergeShipHostTest_Log(const char* fmt, ...);
}

#define INTRO_CVAR_NAME "gEnhancements.Cutscenes.SkipIntroSequence"
#define INTRO_CVAR CVarGetInteger(INTRO_CVAR_NAME, 0)
#define FIRST_CYCLE_CVAR_NAME "gEnhancements.Cutscenes.SkipFirstCycle"
#define FIRST_CYCLE_CVAR CVarGetInteger(FIRST_CYCLE_CVAR_NAME, 0)

// MergeShip2 (BUG-0166): a Termina file of the door that starts somewhere else than its intro -- the Song of Soaring
// from Hyrule to an owl statue, a shuffled entrance of Hyrule's, Anchor's warp to a teammate. The intro skip below
// only answers the intro's own entrance, so such a file arrived as it was made: Clock Town not entered yet, no Tatl,
// and South Clock Town played its first-arrival scene on a Link who was never released (the softlock). The same state
// the skip leaves on the door's file (nothing given: the user's rule), whatever the Skip Intro Sequence setting says,
// as the intro itself is not what this arrival plays. Link's form is the arrival's (MergeShipHost.cpp).
// Call it before the arrival's entrance is written. Returns true when the file was prepared.
extern "C" s32 MergeShip_PrepareFileForArrival(void) {
    // (isFirstCycle alone tells: "Link has entered Clock Town", true forever once set. A file just made still carries
    // Sram_InitSave's cutscene index 0xFFF0, so the skip's own test of the intro's entrance does not fit here.)
    if (gSaveContext.save.isFirstCycle) {
        return false;
    }
    gSaveContext.save.hasTatl = true;
    gSaveContext.cycleSceneFlags[SCENE_INSIDETOWER].switch0 |= (1 << 0);
    gSaveContext.cycleSceneFlags[SCENE_OPENINGDAN].switch0 |= (1 << 2);
    gSaveContext.cycleSceneFlags[SCENE_OPENINGDAN].switch0 |= (1 << 0);
    gSaveContext.cycleSceneFlags[SCENE_OPENINGDAN].chest |= (1 << 0);
    gSaveContext.save.isFirstCycle = true;
    MergeShipHostTest_Log("FORM intro skipped for an arrival past it: form=%d", gSaveContext.save.playerForm);
    return true;
}

void RegisterSkipIntroSequence() {
    COND_VB_SHOULD(VB_PLAY_TRANSITION_CS, INTRO_CVAR, {
        // Intro cutscene
        if (!(gSaveContext.save.entrance == ENTRANCE(CUTSCENE, 0) && gSaveContext.save.cutsceneIndex == 0) ||
            gSaveContext.save.isFirstCycle) {
            return;
        }

        gSaveContext.save.entrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
        gSaveContext.save.hasTatl = true;
        gSaveContext.cycleSceneFlags[SCENE_INSIDETOWER].switch0 |= (1 << 0);
        gSaveContext.cycleSceneFlags[SCENE_OPENINGDAN].switch0 |= (1 << 2);
        gSaveContext.cycleSceneFlags[SCENE_OPENINGDAN].switch0 |= (1 << 0);
        gSaveContext.save.playerForm = PLAYER_FORM_DEKU;

        // Marks when Link enters clock town for the first time.
        // This should be true forever, it is not "just" for "first cycle". Don't unset it in the first cycle skip.
        gSaveContext.save.isFirstCycle = true;

        // MergeShip2: a Termina file of a Hyrule file (mergeship.exe's door) is given nothing (the user's rule): Skip
        // First Cycle would give the ocarina, the Deku Mask, the Songs of Time and Healing and the magic meter, so there
        // Majora's Mask's first cycle is played (Deku Link from South Clock Town, as the rest of this skip leaves him).
        // What Hyrule carries comes with the shared save (MergeShipSharedSave.h).
        const bool mergeShipTermina = MergeShipHost_SharedSave() != nullptr;
        // ...but Link stays himself, never the cursed Deku (the user, 2026-10-10: "they turned into deku link. make
        // sure that doesnt happen"): he came through the door as Hyrule's Link.
        if (mergeShipTermina) {
            gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
        }
        MergeShipHostTest_Log("FORM intro skipped: door's file=%d form=%d", mergeShipTermina ? 1 : 0,
                          gSaveContext.save.playerForm);

        // Mark chest as opened and give the player the Deku Nuts
        // MergeShip2: not on the door's Termina file (the skipped intro's chest is no randomizer check either)
        gSaveContext.cycleSceneFlags[SCENE_OPENINGDAN].chest |= (1 << 0);
        if (!mergeShipTermina) {
            Item_Give(gPlayState, ITEM_DEKU_NUTS_10);
        }
        if (FIRST_CYCLE_CVAR && !mergeShipTermina) {
            gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
            gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
            Item_Give(gPlayState, ITEM_OCARINA_OF_TIME);
            Item_Give(gPlayState, ITEM_MASK_DEKU);
            gSaveContext.save.saveInfo.inventory.questItems |= (1 << QUEST_SONG_TIME) | (1 << QUEST_SONG_HEALING);
            gSaveContext.save.saveInfo.playerData.threeDayResetCount = 1;

            // Lose nuts
            AMMO(ITEM_DEKU_NUT) = 0;

            // Tatl's text at seeing the broken great fairy
            gSaveContext.cycleSceneFlags[SCENE_YOUSEI_IZUMI].switch0 |= (1 << 10);

            // Tatl's text when first entering South Clock Town
            SET_WEEKEVENTREG(WEEKEVENTREG_59_04);

            // Set Tatl second cycle (?)
            SET_WEEKEVENTREG(WEEKEVENTREG_31_04);

            // Set Entrance Cutscene flags for Clock Town
            SET_WEEKEVENTREG(WEEKEVENTREG_ENTERED_EAST_CLOCK_TOWN);
            SET_WEEKEVENTREG(WEEKEVENTREG_ENTERED_WEST_CLOCK_TOWN);
            SET_WEEKEVENTREG(WEEKEVENTREG_ENTERED_NORTH_CLOCK_TOWN);
        }
        // Persist this modified state so moon crashes without saving keep it
        Sram_SaveSpecialEnterClockTown(gPlayState);
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipIntroSequence, { INTRO_CVAR_NAME, FIRST_CYCLE_CVAR_NAME });
