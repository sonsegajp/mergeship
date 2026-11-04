#include "soh/MergeShip/MergeShipEpona.h"
#include "global.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "soh/MergeShip/MergeShipGanondorf.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

s32 MergeShip_YoungEpona_Enabled(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.YoungEpona"), 1) && MergeShip_IsMajorasMaskMounted();
}

s32 MergeShip_Epona_IsYoung(void) {
    return LINK_IS_CHILD && MergeShip_YoungEpona_Enabled();
}

s32 MergeShip_Epona_Owned(void) {
    if (LINK_IS_ADULT) {
        return Flags_GetEventChkInf(EVENTCHKINF_EPONA_OBTAINED);
    }
    // A randomizer seed asks what it asks of the adult's Epona (z_horse.c Horse_SetupInGameplay): the song, an
    // ocarina and the song's notes.
    return MergeShip_YoungEpona_Enabled() && CHECK_QUEST_ITEM(QUEST_SONG_EPONA) &&
           (!IS_RANDO || (GameInteractor_Should(VB_HAVE_OCARINA_NOTE_D5, true) &&
                          GameInteractor_Should(VB_HAVE_OCARINA_NOTE_B4, true) &&
                          GameInteractor_Should(VB_HAVE_OCARINA_NOTE_A4, true) &&
                          (INV_CONTENT(ITEM_OCARINA_FAIRY) != ITEM_NONE)));
}

s32 Horse_CanSpawn(s32 scene); // z_horse.c

void MergeShip_Epona_OnSongPlayed(PlayState* play) {
    Player* player;
    Actor* actor;
    Actor* horse;

    // MergeShip2: Ganondorf mode -- his horse is his own: it answers his song at either age, Epona or not
    s32 ganon = MergeShip_Ganondorf_Enabled();

    if ((play == NULL) || !(ganon || (MergeShip_Epona_IsYoung() && MergeShip_Epona_Owned())) ||
        !Horse_CanSpawn(play->sceneNum) || (play->sceneNum == SCENE_LON_LON_RANCH)) {
        return;
    }
    for (actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_EN_HORSE) {
            return; // she is here: she answers the song herself (EnHorse_Inactive / the idle actions)
        }
    }
    // As Horse_SetupInGameplay places her to wait for the song (params 2, EnHorse_InitInactive): she then comes from the
    // scene's nearest horse spawn out of sight (EnHorse_Spawn), as the adult's does.
    player = GET_PLAYER(play);
    horse = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_HORSE, player->actor.world.pos.x, player->actor.world.pos.y,
                        player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, 2);
    if (horse != NULL) {
        if (play->sceneNum == SCENE_GERUDOS_FORTRESS) {
            horse->room = -1;
        }
        gSaveContext.horseData.scene = play->sceneNum;
        DREG(53) = 1; // EnHorse_Init cleared it: the song still calls her
    }
}
