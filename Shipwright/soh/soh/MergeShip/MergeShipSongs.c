// MergeShip2 -- Majora's Mask's songs in Hyrule (MergeShipSongs.h): what is learned, what each song does after it is
// played, and the Inverted Song of Time / Song of Double Time mapped onto Ocarina of Time's day. The ocarina itself is
// code_800EC960.c, the message steps z_message_PAL.c, the messages and get-items MergeShipSongs.cpp, the pause pages
// z_kaleido_mergeship.c, the song effects ovl_MM_Oceff_Wipe567.
//
// What each song does in Hyrule (MM z_message.c MSGMODE_18 / MSGMODE_TEXT_CLOSING, and MM's actors):
//   Sonata of Awakening, Goron Lullaby (and its intro), New Wave Bossa Nova, Oath to Order, Song of Healing:
//     MM plays the song, shows "You played ..." and sets OCARINA_MODE_EVENT for whatever actor is listening (the
//     Deku Palace door, Goron Elder's son / sleeping Gorons, the Zora eggs, the Clock Tower roof, masks' spirits).
//     None of those actors exist in Hyrule, so, as anywhere in Termina with no listener, nothing further happens.
//     Ocarina of Time's Gorons are not put to sleep: MM's precedent (En_Go, En_Gk) is those actors' own behaviour
//     and animations, which Ocarina of Time's Gorons do not have.
//   Elegy of Emptiness: MM works only in Ikana's scenes and answers "Your notes echoed far... but nothing
//     happened." everywhere else (MM 0x1B95) -- every Hyrule scene is "elsewhere". BUG-0144: with "Elegy of
//     Emptiness Anywhere" (Mergeship menu; Termina's 2 Ship cheat of the same name follows it) it works in Hyrule as
//     MM's does in Ikana (MM Player_Action_88): Link holds still, on the tenth frame the shell of his form appears in
//     its beam of light where he stands (MM func_80848640: MM's En_Torch2 and Eff_Change, ovl_MM_En_Torch2), and
//     ninety frames on the ocarina ends. A form keeps one shell: played again, the Elegy moves it.
//   Song of Soaring: with a statue activated, Termina's owl-warp map (MergeShipTermina.h), and the statue chosen takes
//     Link to Termina; without: MM 0xFB. It works wherever Ocarina of Time's own warp songs do (BUG-0128, the user:
//     "warp to termina and use terminas map screen in hyrule"), dungeons too: MM's "Soar to Entrance?" there is
//     Termina's own dungeons' warp. Where Ocarina of Time forbids warp songs (restrictions.warpSongs == 3,
//     disableWarpSongs), MM's restricted answer 0x1B95.
//   Inverted Song of Time: MM slows time to a third (R_TIME_SPEED 3 + timeSpeedOffset -2) or restores it,
//     after "Slow the current flow of time?" / "Return the flow of time to normal?". Here the offset scales Ocarina
//     of Time's day clock the same way (MergeShip_Songs_TimeStep). In a scene where time stands still (Ocarina of
//     Time's timeIncrement 0, MM's R_TIME_SPEED 0): "But nothing happened..." as MM.
//   Song of Double Time: MM skips to the next 6:00 or 18:00 after "Proceed to the ... Day?". Hyrule has no three
//     days, so MM's day number is Ocarina of Time's day count modulo three (day 1..3), which picks MM's prompt; on
//     "the Night of the Final Day" MM refuses ("You cannot proceed any further through time..."). Where Ocarina of
//     Time's clock runs, the skip is Ocarina of Time's own fast-forward (the Sun's Song's gTimeSpeed 400) up to
//     6:00 / 18:00; where it stands still the clock is set, as MM sets it. MM's En_Test6 clock cutscene is not
//     ported. Where Ocarina of Time forbids the Sun's Song (restrictions.sunsSong == 3) both time songs give MM's
//     restricted answer 0x1B95.

#include "global.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
#include "soh/MergeShip/MergeShipSongs.h"
#include "soh/MergeShip/MergeShipTermina.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipMMAudio.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "src/overlays/actors/ovl_MM_Oceff_Wipe567/z_mm_oceff_wipe567.h"
#include "src/overlays/actors/ovl_MM_En_Torch2/z_mm_en_torch2.h" // MergeShip2 (BUG-0144): the Elegy's shells
#include "soh/MergeShip/MergeShipTransformFx.h"                    // MergeShip_PlayMMSfxActor
#include "soh/MergeShip/MergeShipMMSfx.h"                          // MM_NA_SE_PL_TRANSFORM
#include "mm/objects/object_bsmask/object_bsmask.h"
#include "mm/objects/object_gi_schedule/object_gi_schedule.h"

void MergeShipTest_Log(const char* fmt, ...);

static void MergeShip_Songs_Answer(PlayState* play, u16 textId);

// The song whose MSGMODE_TEXT_CLOSING step is still to come (MM checks sLastPlayedSong there).
static u8 sPendingSong = 0xFF;
// BUG-0144: frames into the Elegy's hold (MM Player_Action_88's av2.actionVar2); -1: none.
static s32 sElegyFrames = -1;
#define MERGESHIP_ELEGY_ANYWHERE_CVAR CVAR_ENHANCEMENT("MergeShip.ElegyAnywhere")

// The Song of Double Time's fast-forward.
static s32 sDoubleTimeActive = false;
static s32 sDoubleTimeToDawn = false;
static u16 sDoubleTimePrevSpeed = 0;
static u32 sTimeStepRemainder = 0;

s32 MergeShip_Songs_Enabled(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMSongs"), 1);
}

s32 MergeShip_Songs_IsLearned(u8 song) {
    if (!MERGESHIP_SONG_IS_MM(song)) {
        return false;
    }
    // MM Message_DisplayOcarinaStaffImpl: CHECK_QUEST_ITEM(QUEST_SONG_TIME) -> ocarinaAvailableSongs |= 0x3000.
    if ((song == MERGESHIP_SONG_INVERTED_TIME) || (song == MERGESHIP_SONG_DOUBLE_TIME)) {
        return CHECK_QUEST_ITEM(QUEST_SONG_TIME);
    }
    return (gMergeShipSave.mmSongs >> (song - MERGESHIP_SONG_FIRST)) & 1;
}

u32 MergeShip_Songs_GetAvailableFlags(void) {
    u32 flags = 0;
    u8 song;

    if (!MergeShip_Songs_Enabled()) {
        return 0;
    }
    for (song = MERGESHIP_SONG_FIRST; song < MERGESHIP_SONG_END; song++) {
        if (MergeShip_Songs_IsLearned(song)) {
            flags |= MERGESHIP_SONG_FLAG(song);
        }
    }
    // MM: the intro is a prefix of the full lullaby, so it is not offered once the lullaby is learned.
    if (MergeShip_Songs_IsLearned(MERGESHIP_SONG_GORON_LULLABY)) {
        flags &= ~MERGESHIP_SONG_FLAG(MERGESHIP_SONG_GORON_LULLABY_INTRO);
    }
    return flags;
}

u8 MergeShip_Songs_ItemToSong(u8 item) {
    switch (item) {
        case ITEM_MM_SONG_SONATA:
            return MERGESHIP_SONG_SONATA;
        case ITEM_MM_SONG_GORON_LULLABY:
            return MERGESHIP_SONG_GORON_LULLABY;
        case ITEM_MM_SONG_NEW_WAVE:
            return MERGESHIP_SONG_NEW_WAVE;
        case ITEM_MM_SONG_ELEGY:
            return MERGESHIP_SONG_ELEGY;
        case ITEM_MM_SONG_OATH:
            return MERGESHIP_SONG_OATH;
        case ITEM_MM_SONG_HEALING:
            return MERGESHIP_SONG_HEALING;
        case ITEM_MM_SONG_SOARING:
            return MERGESHIP_SONG_SOARING;
        case ITEM_MM_SONG_GORON_LULLABY_INTRO:
            return MERGESHIP_SONG_GORON_LULLABY_INTRO;
        default:
            return 0xFF;
    }
}

// MM Item_Give: ITEM_SONG_* -> SET_QUEST_ITEM, ITEM_REMAINS_* -> SET_QUEST_ITEM, ITEM_BOMBERS_NOTEBOOK ->
// SET_QUEST_ITEM(QUEST_BOMBERS_NOTEBOOK); all return ITEM_NONE.
s32 MergeShip_Songs_GiveItem(u8 item) {
    u8 song = MergeShip_Songs_ItemToSong(item);

    if (song != 0xFF) {
        gMergeShipSave.mmSongs |= (1 << (song - MERGESHIP_SONG_FIRST));
        MergeShipTest_Log("MMSONG learned song=%d item=0x%02X", song, item);
        return true;
    }
    if ((item >= ITEM_MM_REMAINS_ODOLWA) && (item <= ITEM_MM_REMAINS_TWINMOLD)) {
        gMergeShipSave.mmQuestItems |= (MERGESHIP_QUEST_REMAINS_ODOLWA << (item - ITEM_MM_REMAINS_ODOLWA));
        return true;
    }
    if (item == ITEM_MM_BOMBERS_NOTEBOOK) {
        gMergeShipSave.mmQuestItems |= MERGESHIP_QUEST_BOMBERS_NOTEBOOK;
        return true;
    }
    return false;
}

s32 MergeShip_Songs_HasItem(u8 item) {
    u8 song = MergeShip_Songs_ItemToSong(item);

    if (song != 0xFF) {
        return MergeShip_Songs_IsLearned(song);
    }
    if ((item >= ITEM_MM_REMAINS_ODOLWA) && (item <= ITEM_MM_REMAINS_TWINMOLD)) {
        return (gMergeShipSave.mmQuestItems >> (item - ITEM_MM_REMAINS_ODOLWA)) & 1;
    }
    if (item == ITEM_MM_BOMBERS_NOTEBOOK) {
        return (gMergeShipSave.mmQuestItems & MERGESHIP_QUEST_BOMBERS_NOTEBOOK) != 0;
    }
    return false;
}

// ---- Get-items ---------------------------------------------------------------------------------------------------

// MM GetItem_DrawRemains (z_draw.c, OBJECT_BSMASK): the boss's mask at 0.02 scale, then the scene's fog again.
static void MergeShip_Songs_DrawGiRemains(PlayState* play, GetItemEntry* getItemEntry) {
    static const char* sRemainsDLs[4] = { mm_gRemainsOdolwaDL, mm_gRemainsGohtDL, mm_gRemainsGyorgDL,
                                          mm_gRemainsTwinmoldDL };
    s32 index = getItemEntry->itemId - ITEM_MM_REMAINS_ODOLWA;

    if (!MergeShip_IsMajorasMaskMounted() || (index < 0) || (index > 3)) {
        GetItem_Draw(play, GID_MEDALLION_FOREST);
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRemainsDLs[index]);
    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM GetItem_Draw(GID_BOMBERS_NOTEBOOK): GetItem_DrawOpa0Xlu1 { gGiBombersNotebookEmptyDL, gGiBombersNotebookDL }.
static void MergeShip_Songs_DrawGiNotebook(PlayState* play, GetItemEntry* getItemEntry) {
    (void)getItemEntry;
    if (!MergeShip_IsMajorasMaskMounted()) {
        GetItem_Draw(play, GID_MEDALLION_FOREST);
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_gGiBombersNotebookEmptyDL);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gGiBombersNotebookDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM sGetItemTable: GI_REMAINS_* GET_ITEM(ITEM_REMAINS_*, OBJECT_UNSET_0, GID_REMAINS_*, 0x55..0x58,
// GIFIELD(GIFIELD_NO_COLLECTIBLE, 0), CHEST_ANIM_LONG) and GI_BOMBERS_NOTEBOOK GET_ITEM(ITEM_BOMBERS_NOTEBOOK,
// OBJECT_GI_SCHEDULE, GID_BOMBERS_NOTEBOOK, 0x50, ...). The models are drawn from mm.o2r by drawFunc; the medallion
// object / draw id are only Ocarina of Time's stand-ins when mm.o2r is missing. MM's songs have no get-item model
// (they are taught): their entry only carries the item to Item_Give's receive hooks.
GetItemEntry MergeShip_Songs_GetItemEntry(u8 item) {
    GetItemEntry entry = GET_ITEM_NONE;
    u8 song = MergeShip_Songs_ItemToSong(item);

    if ((item >= ITEM_MM_REMAINS_ODOLWA) && (item <= ITEM_MM_BOMBERS_NOTEBOOK)) {
        GetItemEntry gi = GET_ITEM(item, OBJECT_GI_MEDAL, GID_MEDALLION_FOREST,
                                   MERGESHIP_TEXT_GET_BASE + (item - ITEM_MM_REMAINS_ODOLWA), 0x80, CHEST_ANIM_LONG,
                                   ITEM_CATEGORY_MAJOR, MOD_NONE,
                                   MERGESHIP_GI_MM_REMAINS_ODOLWA + (item - ITEM_MM_REMAINS_ODOLWA));

        gi.drawFunc =
            (item == ITEM_MM_BOMBERS_NOTEBOOK) ? MergeShip_Songs_DrawGiNotebook : MergeShip_Songs_DrawGiRemains;
        entry = gi;
    } else if (song != 0xFF) {
        GetItemEntry gi = GET_ITEM(item, OBJECT_GI_MELODY, GID_SONG_GENERIC, 0, 0x80, CHEST_ANIM_LONG,
                                   ITEM_CATEGORY_MAJOR, MOD_NONE,
                                   MERGESHIP_GI_MM_SONG_FIRST + (song - MERGESHIP_SONG_FIRST));
        entry = gi;
    }
    return entry;
}

// ---- The song's steps (z_message_PAL.c) ------------------------------------------------------------------------

// MM Message_SpawnSongEffect: sOcarinaEffectActorIds / sOcarinaEffectActorParams, with the effect active flag.
void MergeShip_Songs_SpawnEffect(PlayState* play, u8 song) {
    Player* player = GET_PLAYER(play);
    s16 actorId = -1;
    s16 params = 0;

    switch (song) {
        case MERGESHIP_SONG_SONATA:
        case MERGESHIP_SONG_GORON_LULLABY:
        case MERGESHIP_SONG_NEW_WAVE:
        case MERGESHIP_SONG_ELEGY:
        case MERGESHIP_SONG_OATH:
            actorId = gMergeShipActorId_Oceff_Wipe5; // MM ACTOR_OCEFF_WIPE5, params 0..4
            params = song - MERGESHIP_SONG_SONATA;
            break;
        case MERGESHIP_SONG_HEALING:
            actorId = gMergeShipActorId_Oceff_Wipe7; // MM ACTOR_OCEFF_WIPE7
            break;
        case MERGESHIP_SONG_SOARING:
            actorId = gMergeShipActorId_Oceff_Wipe6; // MM ACTOR_OCEFF_WIPE6
            break;
        case MERGESHIP_SONG_INVERTED_TIME:
        case MERGESHIP_SONG_DOUBLE_TIME:
            // MM ACTOR_OCEFF_WIPE params 1: the Song of Time's effect, which is Ocarina of Time's Oceff_Wipe too.
            Actor_Spawn(&play->actorCtx, play, ACTOR_OCEFF_WIPE, player->actor.world.pos.x,
                        player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, 1);
            return;
        default:
            // MM: none for the Goron Lullaby Intro.
            return;
    }
    if ((actorId >= 0) && (Actor_Spawn(&play->actorCtx, play, actorId, player->actor.world.pos.x,
                                       player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0,
                                       params) != NULL)) {
        gMergeShipSongEffectActive = true;
    }
}

void MergeShip_Songs_PlayFanfare(u8 song) {
    if (MERGESHIP_SONG_IS_MM(song)) {
        MergeShip_MMAudio_PlayMMSongFanfare(song - MERGESHIP_SONG_FIRST);
    }
}

u16 MergeShip_Songs_PlayedTextId(u8 song) {
    return MERGESHIP_TEXT_SONG_PLAYED_BASE + (song - MERGESHIP_SONG_FIRST);
}

// MM MSGMODE_18 for FREE_PLAY_DONE (Ocarina of Time has already closed the box and set OCARINA_MODE_01, its
// OCARINA_MODE_EVENT, which MSGMODE_TEXT_CLOSING turns into OCARINA_MODE_04 when nothing claims it).
void MergeShip_Songs_OnSongPlayedAct(PlayState* play, u8 song) {
    if (!MERGESHIP_SONG_IS_MM(song)) {
        return;
    }
    MergeShipTest_Log("MMSONG played song=%d", song);
    switch (song) {
        case MERGESHIP_SONG_ELEGY:
            // MM: only Ikana's scenes (SCENE_F40 .. SCENE_TOUGITES) answer the Elegy -- in Hyrule, only with
            // "Elegy of Emptiness Anywhere" (2Ship's cheat VB_ELEGY_CHECK_SCENE; BUG-0144).
            if (CVarGetInteger(MERGESHIP_ELEGY_ANYWHERE_CVAR, 0) && (gMergeShipActorId_En_Torch2 >= 0)) {
                sPendingSong = song;
            } else {
                MergeShip_Songs_Answer(play, MERGESHIP_TEXT_NOTHING_HAPPENED);
            }
            break;
        case MERGESHIP_SONG_SOARING:
        case MERGESHIP_SONG_INVERTED_TIME:
        case MERGESHIP_SONG_DOUBLE_TIME:
            sPendingSong = song;
            break;
        default:
            break;
    }
}

// MM's answers that end the song (MM OCARINA_MODE_PROCESS_RESTRICTED_SONG -> OCARINA_MODE_END). Ocarina of Time's
// Message_StartTextbox resets the ocarina mode, so the player is released at once, as after Ocarina of Time's own
// "You can't warp here!" (0x88C, OCARINA_MODE_04).
static void MergeShip_Songs_Answer(PlayState* play, u16 textId) {
    Message_StartTextbox(play, textId, NULL);
    play->msgCtx.ocarinaMode = OCARINA_MODE_04;
}

// MM CURRENT_DAY for Hyrule (see the top of the file).
static s32 MergeShip_Songs_CurrentDay(void) {
    return (gSaveContext.totalDays % 3) + 1;
}

s32 MergeShip_Songs_OnTextClosing(PlayState* play) {
    MessageContext* msgCtx = &play->msgCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    u8 song = sPendingSong;

    if (song == 0xFF) {
        return false;
    }
    sPendingSong = 0xFF;

    switch (song) {
        case MERGESHIP_SONG_ELEGY:
            // MM OCARINA_MODE_EVENT with the Elegy: the box closes and Link holds still (Player_Action_88, which
            // MergeShip_Songs_Update plays out under MERGESHIP_OCARINA_MODE_ELEGY).
            msgCtx->msgLength = 0;
            msgCtx->msgMode = MSGMODE_NONE;
            msgCtx->textId = 0;
            msgCtx->stateTimer = 0;
            interfaceCtx->unk_1FA = interfaceCtx->unk_1FC = 0;
            msgCtx->textboxEndType = TEXTBOX_ENDTYPE_DEFAULT;
            play->msgCtx.ocarinaMode = MERGESHIP_OCARINA_MODE_ELEGY;
            sElegyFrames = 0;
            return true;

        case MERGESHIP_SONG_SOARING:
            if (msgCtx->disableWarpSongs || ((interfaceCtx->restrictions.warpSongs == 3) && !IS_RANDO)) {
                MergeShip_Songs_Answer(play, MERGESHIP_TEXT_NOTHING_HAPPENED);
                return true;
            }
            if (gMergeShipSongEffectActive) {
                // MM: wait for the song's effect to finish (msgCtx->stateTimer = 1, next frame again).
                sPendingSong = song;
                msgCtx->stateTimer = 1;
                return true;
            }
            if (MergeShip_Termina_GetOwlActivationFlags() != 0) {
                // MM: the box closes, the ocarina ends, and the pause menu opens on the owl-warp map.
                msgCtx->msgLength = 0;
                msgCtx->msgMode = MSGMODE_NONE;
                msgCtx->textId = 0;
                msgCtx->stateTimer = 0;
                interfaceCtx->unk_1FA = interfaceCtx->unk_1FC = 0;
                msgCtx->textboxEndType = TEXTBOX_ENDTYPE_DEFAULT;
                play->msgCtx.ocarinaMode = OCARINA_MODE_04;
                MergeShip_OwlWarp_Open(play);
                return true;
            }
            MergeShip_Songs_Answer(play, MERGESHIP_TEXT_NO_OWLS);
            return true;

        case MERGESHIP_SONG_INVERTED_TIME:
            if (interfaceCtx->restrictions.sunsSong == 3) {
                MergeShip_Songs_Answer(play, MERGESHIP_TEXT_NOTHING_HAPPENED);
            } else if (play->envCtx.timeIncrement != 0) {
                Message_StartTextbox(play,
                                     (gMergeShipSave.timeSpeedOffset == 0) ? MERGESHIP_TEXT_TIME_SLOW
                                                                           : MERGESHIP_TEXT_TIME_NORMAL,
                                     NULL);
                play->msgCtx.ocarinaMode = MERGESHIP_OCARINA_MODE_PROCESS_INVERTED_TIME;
            } else {
                MergeShip_Songs_Answer(play, MERGESHIP_TEXT_TIME_NOTHING);
            }
            return true;

        case MERGESHIP_SONG_DOUBLE_TIME: {
            // MM D_801D045C (day -> night of that day) and D_801D0464 (night -> dawn of the next).
            static const u16 sToNight[3] = { 0x1B91, 0x1B90, 0x1B8F };
            static const u16 sToDawn[2] = { 0x1B92, 0x1B8E };
            s32 day = MergeShip_Songs_CurrentDay();

            if (interfaceCtx->restrictions.sunsSong == 3) {
                MergeShip_Songs_Answer(play, MERGESHIP_TEXT_NOTHING_HAPPENED);
            } else if ((day == 3) && IS_NIGHT) {
                MergeShip_Songs_Answer(play, MERGESHIP_TEXT_TIME_NO_FURTHER);
            } else {
                u16 mmText = IS_NIGHT ? sToDawn[day - 1] : sToNight[day - 1];

                Message_StartTextbox(play, MERGESHIP_TEXT_DOUBLE_TIME_BASE + (mmText - 0x1B8E), NULL);
                play->msgCtx.ocarinaMode = MERGESHIP_OCARINA_MODE_PROCESS_DOUBLE_TIME;
            }
            return true;
        }

        default:
            return false;
    }
}

// MM OCARINA_MODE_APPLY_DOUBLE_SOT: 6:00 after a night, 18:00 after a day.
static void MergeShip_Songs_StartDoubleTime(PlayState* play) {
    sDoubleTimeToDawn = IS_NIGHT;
    if (play->envCtx.timeIncrement != 0) {
        // Ocarina of Time's own fast-forward (z_parameter.c, the Sun's Song: gTimeSpeed 400) until the hour.
        sDoubleTimePrevSpeed = gTimeSpeed;
        gTimeSpeed = 400;
        sDoubleTimeActive = true;
        play->msgCtx.ocarinaMode = MERGESHIP_OCARINA_MODE_PROCESS_DOUBLE_TIME; // the player waits (Update ends it)
    } else {
        // The clock stands still here: set it, as MM sets gSaveContext.save.time. Dawn is a new day (Ocarina of
        // Time counts days at dawn, z_kankyo.c / z_play.c).
        if (sDoubleTimeToDawn) {
            gSaveContext.totalDays++;
            gSaveContext.bgsDayCount++;
            gSaveContext.dayTime = CLOCK_TIME(6, 0);
        } else {
            gSaveContext.dayTime = CLOCK_TIME(18, 0);
        }
        gSaveContext.skyboxTime = gSaveContext.dayTime;
        play->msgCtx.ocarinaMode = OCARINA_MODE_04;
    }
    MergeShipTest_Log("MMSONG double time toDawn=%d flowing=%d", sDoubleTimeToDawn, play->envCtx.timeIncrement != 0);
}

// MM Message_Update MSGMODE_TEXT_DONE, TEXTBOX_ENDTYPE_TWO_CHOICE with OCARINA_MODE_PROCESS_INVERTED_TIME /
// PROCESS_DOUBLE_TIME.
s32 MergeShip_Songs_OnChoice(PlayState* play) {
    MessageContext* msgCtx = &play->msgCtx;

    if ((play->msgCtx.ocarinaMode != MERGESHIP_OCARINA_MODE_PROCESS_INVERTED_TIME) &&
        (play->msgCtx.ocarinaMode != MERGESHIP_OCARINA_MODE_PROCESS_DOUBLE_TIME)) {
        return false;
    }
    if (!Message_ShouldAdvanceSilent(play)) {
        return true;
    }

    if (msgCtx->choiceIndex == 0) {
        Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        if (play->msgCtx.ocarinaMode == MERGESHIP_OCARINA_MODE_PROCESS_INVERTED_TIME) {
            // MM OCARINA_MODE_APPLY_INV_SOT_SLOW / _FAST.
            gMergeShipSave.timeSpeedOffset = (gMergeShipSave.timeSpeedOffset == 0) ? -2 : 0;
            sTimeStepRemainder = 0;
            play->msgCtx.ocarinaMode = OCARINA_MODE_04;
            MergeShipTest_Log("MMSONG inverted time offset=%d", gMergeShipSave.timeSpeedOffset);
        } else {
            MergeShip_Songs_StartDoubleTime(play);
        }
    } else {
        Audio_PlaySfxGeneral(NA_SE_SY_MESSAGE_PASS, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        play->msgCtx.ocarinaMode = OCARINA_MODE_04; // MM OCARINA_MODE_END
    }
    Message_CloseTextbox(play);
    return true;
}

// MM Environment's time step: (R_TIME_SPEED + timeSpeedOffset) with R_TIME_SPEED 3, i.e. a third of the speed after
// the Inverted Song of Time. Ocarina of Time's own fast-forwards (the Sun's Song, the Song of Double Time: 400) are
// not slowed, as MM's are not.
u16 MergeShip_Songs_TimeStep(u16 timeSpeed) {
    u32 scaled;

    if ((gMergeShipSave.timeSpeedOffset == 0) || (timeSpeed >= 0x190) || !MergeShip_Songs_Enabled()) {
        return timeSpeed;
    }
    scaled = (u32)timeSpeed * (u32)(3 + gMergeShipSave.timeSpeedOffset) + sTimeStepRemainder;
    sTimeStepRemainder = scaled % 3;
    return (u16)(scaled / 3);
}

// MM func_80848640: the shell of Link's form where he stands (the one already in the scene is moved), and its beam of
// light with the transformation's sound.
static void MergeShip_Songs_ElegyShell(PlayState* play) {
    // MM's player->transformation for each of Hyrule's forms (the human of either age is MM's human; the Fierce
    // Deity leaves the human's shell, as in MM).
    static const s16 kShellOfForm[PLAYER_FORM_MAX] = {
        MM_TORCH2_PARAM_HUMAN,        // PLAYER_FORM_ADULT
        MM_TORCH2_PARAM_HUMAN,        // PLAYER_FORM_CHILD
        MM_TORCH2_PARAM_DEKU,         // PLAYER_FORM_DEKU
        MM_TORCH2_PARAM_GORON,        // PLAYER_FORM_GORON
        MM_TORCH2_PARAM_ZORA,         // PLAYER_FORM_ZORA
        MM_TORCH2_PARAM_FIERCE_DEITY, // PLAYER_FORM_FIERCE_DEITY
    };
    Player* player = GET_PLAYER(play);
    s16 params = (player->mergeShipForm < PLAYER_FORM_MAX) ? kShellOfForm[player->mergeShipForm]
                                                           : MM_TORCH2_PARAM_HUMAN;
    MMEnTorch2* shell = gMergeShipElegyShells[params];

    if (shell != NULL) {
        Math_Vec3f_Copy(&shell->actor.home.pos, &player->actor.world.pos);
        shell->actor.home.rot.y = player->actor.shape.rot.y;
        shell->state = MM_TORCH2_STATE_INITIALIZED;
        shell->framesUntilNextState = 20;
    } else {
        shell = (MMEnTorch2*)Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_En_Torch2, player->actor.world.pos.x,
                                         player->actor.world.pos.y, player->actor.world.pos.z, 0,
                                         player->actor.shape.rot.y, 0, params);
    }
    if (shell != NULL) {
        gMergeShipElegyShells[params] = shell;
    }
    if (gMergeShipActorId_Eff_Change >= 0) {
        Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_Eff_Change, player->actor.world.pos.x,
                    player->actor.world.pos.y, player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0,
                    params); // MM (GET_PLAYER_FORM << 3) | transformation: its colours are the low three bits
    }
    MergeShip_PlayMMSfxActor(&player->actor, MM_NA_SE_PL_TRANSFORM);
    MergeShipTest_Log("MMSONG elegy shell=%d form=%d at (%.0f,%.0f,%.0f)", params, player->mergeShipForm,
                      player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z);
}

void MergeShip_Songs_Update(PlayState* play) {
    if (play == NULL) {
        sDoubleTimeActive = false;
        sElegyFrames = -1;
        return;
    }

    // BUG-0144: MM Player_Action_88 -- the shell on the tenth frame, the ocarina's end after ninety.
    if (sElegyFrames >= 0) {
        if (play->msgCtx.ocarinaMode != MERGESHIP_OCARINA_MODE_ELEGY) {
            sElegyFrames = -1;
        } else if (sElegyFrames++ > 90) {
            play->msgCtx.ocarinaMode = OCARINA_MODE_04;
            sElegyFrames = -1;
        } else if (sElegyFrames == 10) {
            MergeShip_Songs_ElegyShell(play);
        }
    }

    MergeShip_OwlWarp_FrameUpdate(play);
    if (play->pauseCtx.state == 0) {
        MergeShip_Kaleido_ResetWorldViews(play); // MergeShip2: the Quest, Equipment and Item faces' C-Down views
    }

    if (sDoubleTimeActive) {
        u16 time = gSaveContext.dayTime;
        s32 reached = sDoubleTimeToDawn ? ((time >= CLOCK_TIME(6, 0)) && (time < CLOCK_TIME(12, 0)))
                                        : (time >= CLOCK_TIME(18, 0));

        if (play->envCtx.timeIncrement == 0) {
            // The scene changed under the fast-forward: finish at once.
            gSaveContext.dayTime = sDoubleTimeToDawn ? CLOCK_TIME(6, 0) : CLOCK_TIME(18, 0);
            reached = true;
        }
        if (reached) {
            gTimeSpeed = sDoubleTimePrevSpeed;
            sDoubleTimeActive = false;
            if (play->msgCtx.ocarinaMode == MERGESHIP_OCARINA_MODE_PROCESS_DOUBLE_TIME) {
                play->msgCtx.ocarinaMode = OCARINA_MODE_04;
            }
            MergeShipTest_Log("MMSONG double time done time=0x%04X", gSaveContext.dayTime);
        }
    }
}
