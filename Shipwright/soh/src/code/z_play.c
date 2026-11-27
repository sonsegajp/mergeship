#include "soh/MergeShip/MergeShipTransformFx.h"
#include "soh/MergeShip/MergeShipHost.h"
#include "soh/MergeShip/MergeShipSohFd.h"
#include "soh/MergeShip/MergeShipAdultMask.h" // MergeShip2: the timeline an Adult Mask age keeps
#include "soh/MergeShip/MergeShipPort3ds.h"
#include "global.h"
#include "vt.h"

#include <string.h>

#include "soh/Enhancements/gameconsole.h"
#include "soh/frame_interpolation.h"
#include <overlays/actors/ovl_En_Niw/z_en_niw.h>
#include <overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h>
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/SaveManager.h"
#include "soh/framebuffer_effects.h"

#include <time.h>
#include <assert.h>

TransitionUnk sTrnsnUnk;
s32 gTrnsnUnkState;
VisMono gPlayVisMono;
Color_RGBA8_u32 gVisMonoColor;

FaultClient D_801614B8;

s16 sTransitionFillTimer;

void* gDebugCutsceneScript = NULL;
UNK_TYPE D_8012D1F4 = 0; // unused

Input* D_8012D1F8 = NULL;

PlayState* gPlayState;
s16 firstInit = 0;

void Play_SpawnScene(PlayState* play, s32 sceneId, s32 spawn);

// This macro prints the number "1" with a file and line number if R_ENABLE_PLAY_LOGS is enabled.
// For example, it can be used to trace the play state execution at a high level.
// SOHTODO: Revert log statements everywhere back to authentic, and deal with dynamic line/file names via macro
#define PLAY_LOG(line)                                  \
    do {                                                \
        if (1 & HREG(63)) {                             \
            LOG_NUM("1", 1 /*, "../z_play.c", line */); \
        }                                               \
    } while (0)

void enableBetaQuest();
void disableBetaQuest();

void OTRPlay_SpawnScene(PlayState* play, s32 sceneId, s32 spawn);

void Play_RequestViewpointBgCam(PlayState* play) {
    Camera_ChangeDataIdx(GET_ACTIVE_CAM(play), play->unk_1242B - 1);
}

void Play_SetViewpoint(PlayState* play, s16 viewpoint) {
    assert(viewpoint == 1 || viewpoint == 2);

    play->unk_1242B = viewpoint;

    if ((YREG(15) != 0x10) && (gSaveContext.cutsceneIndex < 0xFFF0)) {
        Audio_PlaySfxGeneral((viewpoint == 1) ? NA_SE_SY_CAMERA_ZOOM_DOWN : NA_SE_SY_CAMERA_ZOOM_UP, &gSfxDefaultPos, 4,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }

    Play_RequestViewpointBgCam(play);
}

/**
 * @return true if the currently set viewpoint is the same as the one provided in the argument
 */
s32 Play_CheckViewpoint(PlayState* play, s16 viewpoint) {
    return (viewpoint == play->unk_1242B);
}

/**
 * If the scene is a shop, set the viewpoint that will set the bgCamIndex
 * to toggle the camera into a "browsing item selection" setting.
 */
void Play_SetShopBrowsingViewpoint(PlayState* play) {
    osSyncPrintf("Game_play_shop_pr_vr_switch_set()\n");

    if (YREG(15) == 0x10) {
        play->unk_1242B = 2;
    }
}

void Gameplay_SetupTransition(PlayState* play, s32 transitionType) {
    TransitionContext* transitionCtx = &play->transitionCtx;

    memset(transitionCtx, 0, sizeof(TransitionContext));

    transitionCtx->transitionType = transitionType;

    // Circle Transition Types
    if ((transitionCtx->transitionType >> 5) == 1) {
        transitionCtx->init = TransitionCircle_Init;
        transitionCtx->destroy = TransitionCircle_Destroy;
        transitionCtx->start = TransitionCircle_Start;
        transitionCtx->isDone = TransitionCircle_IsDone;
        transitionCtx->draw = TransitionCircle_Draw;
        transitionCtx->update = TransitionCircle_Update;
        transitionCtx->setType = TransitionCircle_SetType;
        transitionCtx->setColor = TransitionCircle_SetColor;
        transitionCtx->setEnvColor = TransitionCircle_SetEnvColor;
    } else {
        switch (transitionCtx->transitionType) {
            case TRANS_TYPE_TRIFORCE:
                transitionCtx->init = TransitionTriforce_Init;
                transitionCtx->destroy = TransitionTriforce_Destroy;
                transitionCtx->start = TransitionTriforce_Start;
                transitionCtx->isDone = TransitionTriforce_IsDone;
                transitionCtx->draw = TransitionTriforce_Draw;
                transitionCtx->update = TransitionTriforce_Update;
                transitionCtx->setType = TransitionTriforce_SetType;
                transitionCtx->setColor = TransitionTriforce_SetColor;
                transitionCtx->setEnvColor = NULL;
                break;

            case TRANS_TYPE_WIPE:
            case TRANS_TYPE_WIPE_FAST:
                transitionCtx->init = TransitionWipe_Init;
                transitionCtx->destroy = TransitionWipe_Destroy;
                transitionCtx->start = TransitionWipe_Start;
                transitionCtx->isDone = TransitionWipe_IsDone;
                transitionCtx->draw = TransitionWipe_Draw;
                transitionCtx->update = TransitionWipe_Update;
                transitionCtx->setType = TransitionWipe_SetType;
                transitionCtx->setColor = TransitionWipe_SetColor;
                transitionCtx->setEnvColor = NULL;
                break;

            case TRANS_TYPE_FADE_BLACK:
            case TRANS_TYPE_FADE_WHITE:
            case TRANS_TYPE_FADE_BLACK_FAST:
            case TRANS_TYPE_FADE_WHITE_FAST:
            case TRANS_TYPE_FADE_BLACK_SLOW:
            case TRANS_TYPE_FADE_WHITE_SLOW:
            case TRANS_TYPE_FADE_WHITE_CS_DELAYED:
            case TRANS_TYPE_FADE_WHITE_INSTANT:
            case TRANS_TYPE_FADE_GREEN:
            case TRANS_TYPE_FADE_BLUE:
                transitionCtx->init = TransitionFade_Init;
                transitionCtx->destroy = TransitionFade_Destroy;
                transitionCtx->start = TransitionFade_Start;
                transitionCtx->isDone = TransitionFade_IsDone;
                transitionCtx->draw = TransitionFade_Draw;
                transitionCtx->update = TransitionFade_Update;
                transitionCtx->setType = TransitionFade_SetType;
                transitionCtx->setColor = TransitionFade_SetColor;
                transitionCtx->setEnvColor = NULL;
                break;

            case TRANS_TYPE_FILL_WHITE2:
            case TRANS_TYPE_FILL_WHITE:
                play->transitionMode = TRANS_MODE_FILL_WHITE_INIT;
                break;

            case TRANS_TYPE_INSTANT:
                play->transitionMode = TRANS_MODE_INSTANT;
                break;

            case TRANS_TYPE_FILL_BROWN:
                play->transitionMode = TRANS_MODE_FILL_BROWN_INIT;
                break;

            case TRANS_TYPE_SANDSTORM_PERSIST:
                play->transitionMode = TRANS_MODE_SANDSTORM_INIT;
                break;

            case TRANS_TYPE_SANDSTORM_END:
                play->transitionMode = TRANS_MODE_SANDSTORM_END_INIT;
                break;

            case TRANS_TYPE_CS_BLACK_FILL:
                play->transitionMode = TRANS_MODE_CS_BLACK_FILL_INIT;
                break;

            default:
                Fault_AddHungupAndCrash(__FILE__, __LINE__);
                break;
        }
    }
}

void func_800BC88C(PlayState* play) {
    play->transitionCtx.transitionType = -1;
}

Gfx* Play_SetFog(PlayState* play, Gfx* gfx) {
    return Gfx_SetFog2(gfx, play->lightCtx.fogColor[0], play->lightCtx.fogColor[1], play->lightCtx.fogColor[2], 0,
                       play->lightCtx.fogNear, 1000);
}

void Play_Destroy(GameState* thisx) {
    PlayState* play = (PlayState*)thisx;
    Player* player = GET_PLAYER(play);

    GameInteractor_ExecuteOnPlayDestroy();

    play->state.gfxCtx->callback = NULL;
    play->state.gfxCtx->callbackParam = 0;

    SREG(91) = 0;
    R_PAUSE_MENU_MODE = 0;

    PreRender_Destroy(&play->pauseBgPreRender);
    Effect_DeleteAll(play);
    EffectSs_ClearAll(play);
    CollisionCheck_DestroyContext(play, &play->colChkCtx);

    if (gTrnsnUnkState == 3) {
        TransitionUnk_Destroy(&sTrnsnUnk);
        gTrnsnUnkState = 0;
    }

    if (play->transitionMode == TRANS_MODE_INSTANCE_RUNNING) {
        play->transitionCtx.destroy(&play->transitionCtx.data);
        func_800BC88C(play);
        play->transitionMode = TRANS_MODE_OFF;
    }

    ShrinkWindow_Destroy();
    TransitionFade_Destroy(&play->transitionFade);
    VisMono_Destroy(&gPlayVisMono);

    if (gSaveContext.linkAge != play->linkAgeOnLoad) {
        Inventory_SwapAgeEquipment();
        Player_SetEquipmentData(play, player);
    }

    func_80031C3C(&play->actorCtx, play);
    func_80110990(play);
    KaleidoScopeCall_Destroy(play);
    KaleidoManager_Destroy();
    ZeldaArena_Cleanup();

    Fault_RemoveClient(&D_801614B8);

    disableBetaQuest();

    gPlayState = NULL;
}

u8 CheckStoneCount() {
    u8 stoneCount = 0;

    if (CHECK_QUEST_ITEM(QUEST_KOKIRI_EMERALD)) {
        stoneCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_GORON_RUBY)) {
        stoneCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_ZORA_SAPPHIRE)) {
        stoneCount++;
    }

    return stoneCount;
}

u8 MergeShip_Goal_ExtraMedallions(void); // MergeShip2: soh/MergeShip/MergeShipGoal.cpp
s32 MergeShipTest_ActorsFrozen(s32 category); // MergeShip2: test harness (soh/MergeShip/MergeShipTest.cpp)
s32 MergeShipTest_FilmView(Vec3f* eye, Vec3f* at); // ... 1 with the film camera's eye and aim while it is the view
s32 MergeShipTest_Clean(void);                      // ... 1 while a script wants nothing but the scene in the picture

u8 CheckMedallionCount() {
    u8 medallionCount = 0;

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_FOREST)) {
        medallionCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_FIRE)) {
        medallionCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_WATER)) {
        medallionCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_SHADOW)) {
        medallionCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_SPIRIT)) {
        medallionCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_LIGHT)) {
        medallionCount++;
    }

    // MergeShip2: Majora's Mask's remains count as medallions in a unified seed that says so (MergeShipGoal.h)
    medallionCount += MergeShip_Goal_ExtraMedallions();

    return medallionCount;
}

u8 CheckDungeonCount() {
    u8 dungeonCount = 0;

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_DEKU_TREE_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_DODONGOS_CAVERN_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_JABU_JABUS_BELLY_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_FOREST_TEMPLE_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_FIRE_TEMPLE_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetRandomizerInf(RAND_INF_DUNGEONS_DONE_SPIRIT_TEMPLE)) {
        dungeonCount++;
    }

    if (Flags_GetRandomizerInf(RAND_INF_DUNGEONS_DONE_SHADOW_TEMPLE)) {
        dungeonCount++;
    }

    if (Flags_GetRandomizerInf(RAND_INF_DUNGEONS_DONE_GANONS_TOWER)) {
        dungeonCount++;
    }

    return dungeonCount;
}

u8 CheckBridgeRewardCount() {
    u8 bridgeRewardCount = 0;

    switch (Randomizer_GetSettingValue(RSK_BRIDGE_OPTIONS)) {
        case RO_BRIDGE_WILDCARD_REWARD:
            if (Flags_GetRandomizerInf(RAND_INF_GREG_FOUND)) {
                bridgeRewardCount += 1;
            }
            break;
        case RO_BRIDGE_GREG_REWARD:
            if (Flags_GetRandomizerInf(RAND_INF_GREG_FOUND)) {
                bridgeRewardCount += 1;
            }
            break;
    }
    return bridgeRewardCount;
}

void Play_Init(GameState* thisx) {
    PlayState* play = (PlayState*)thisx;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    uintptr_t zAlloc;
    uintptr_t zAllocAligned;
    size_t zAllocSize;
    Player* player;
    s32 playerStartBgCamIndex;
    s32 i;
    u8 baseSceneLayer;
    s32 pad[2];

    enableBetaQuest();

    // Properly initialize the frame counter so it doesn't use garbage data
    if (!firstInit) {
        play->gameplayFrames = 0;
        firstInit = 1;
    }

    // Invalid entrance, so immediately exit the game to opening title
    if (gSaveContext.entranceIndex == ENTR_LOAD_OPENING) {
        gSaveContext.entranceIndex = 0;
        play->state.running = false;
        SET_NEXT_GAMESTATE(&play->state, Opening_Init, OpeningContext);
        GameInteractor_ExecuteOnExitGame(gSaveContext.fileNum);
        return;
    }

    gPlayState = play;

    SystemArena_Display();

    // OTRTODO allocate double the normal amount of memory
    // This is to avoid some parts of the game, like loading actors, causing OoM
    // This is potionally unavoidable due to struct size differences, but is x2 the right amount?
    GameState_Realloc(&play->state, 0x1D4790 * 2);
    KaleidoManager_Init(play);
    View_Init(&play->view, gfxCtx);
    Audio_SetExtraFilter(0);
    Quake_Init();

    for (i = 0; i < ARRAY_COUNT(play->cameraPtrs); i++) {
        play->cameraPtrs[i] = NULL;
    }

    Camera_Init(&play->mainCamera, &play->view, &play->colCtx, play);
    Camera_ChangeStatus(&play->mainCamera, CAM_STAT_ACTIVE);

    for (i = 0; i < 3; i++) {
        Camera_Init(&play->subCameras[i], &play->view, &play->colCtx, play);
        Camera_ChangeStatus(&play->subCameras[i], CAM_STAT_UNK100);
    }

    play->cameraPtrs[CAM_ID_MAIN] = &play->mainCamera;
    play->cameraPtrs[CAM_ID_MAIN]->uid = 0;
    play->activeCamera = CAM_ID_MAIN;
    func_8005AC48(&play->mainCamera, 0xFF);
    // Sram_Init(this, &this->sramCtx);
    Regs_InitData(play);
    Message_Init(play);
    GameOver_Init(play);
    SoundSource_InitAll(play);
    Effect_InitContext(play);
    EffectSs_InitInfo(play, 0x55);
    CollisionCheck_InitContext(play, &play->colChkCtx);
    AnimationContext_Reset(&play->animationCtx);
    func_8006450C(play, &play->csCtx);

    if (gSaveContext.nextCutsceneIndex != 0xFFEF) {
        gSaveContext.cutsceneIndex = gSaveContext.nextCutsceneIndex;
        gSaveContext.nextCutsceneIndex = 0xFFEF;
    }

    if (gSaveContext.cutsceneIndex == 0xFFFD) {
        gSaveContext.cutsceneIndex = 0;
    }

    if (gSaveContext.nextDayTime != 0xFFFF) {
        gSaveContext.dayTime = gSaveContext.nextDayTime;
        gSaveContext.skyboxTime = gSaveContext.nextDayTime;
    }

    if (gSaveContext.dayTime > 0xC000 || gSaveContext.dayTime < 0x4555) {
        gSaveContext.nightFlag = 1;
    } else {
        gSaveContext.nightFlag = 0;
    }

    Cutscene_HandleConditionalTriggers(play);

    // MergeShip2 (soh_fd): a Fierce Deity leaving his areas by a scene load arrives as Link.
    MergeShip_SohFd_OnSceneLoad(gEntranceTable[((void)0, gSaveContext.entranceIndex)].scene);

    if (gSaveContext.gameMode != GAMEMODE_NORMAL || gSaveContext.cutsceneIndex >= 0xFFF0) {
        gSaveContext.nayrusLoveTimer = 0;
        Magic_Reset(play);
        gSaveContext.sceneLayer = SCENE_LAYER_CUTSCENE_FIRST + (gSaveContext.cutsceneIndex & 0xF);
    } else if (!MergeShip_AdultMask_TimelineIsAdult() && IS_DAY) {
        // MergeShip2: the era is the timeline's -- an Adult Mask age keeps the world in the timeline it was in
        // (MergeShipAdultMask.h, MergeShip_AdultMask_TimelineIsAdult; LINK_IS_ADULT otherwise).
        gSaveContext.sceneLayer = SCENE_LAYER_CHILD_DAY;
    } else if (!MergeShip_AdultMask_TimelineIsAdult() && !IS_DAY) {
        gSaveContext.sceneLayer = SCENE_LAYER_CHILD_NIGHT;
    } else if (MergeShip_AdultMask_TimelineIsAdult() && IS_DAY) {
        gSaveContext.sceneLayer = SCENE_LAYER_ADULT_DAY;
    } else {
        gSaveContext.sceneLayer = SCENE_LAYER_ADULT_NIGHT;
    }

    // save the base scene layer (before accounting for the special cases below) to use later for the transition type
    baseSceneLayer = gSaveContext.sceneLayer;

    if ((gEntranceTable[((void)0, gSaveContext.entranceIndex)].scene == SCENE_HYRULE_FIELD) &&
        !MergeShip_AdultMask_TimelineIsAdult() && !IS_CUTSCENE_LAYER) {
        if (CHECK_QUEST_ITEM(QUEST_KOKIRI_EMERALD) && CHECK_QUEST_ITEM(QUEST_GORON_RUBY) &&
            CHECK_QUEST_ITEM(QUEST_ZORA_SAPPHIRE)) {
            gSaveContext.sceneLayer = 1;
        } else {
            gSaveContext.sceneLayer = 0;
        }
    } else if ((gEntranceTable[((void)0, gSaveContext.entranceIndex)].scene == SCENE_KOKIRI_FOREST) &&
               MergeShip_AdultMask_TimelineIsAdult() && !IS_CUTSCENE_LAYER) {
        gSaveContext.sceneLayer = (Flags_GetEventChkInf(EVENTCHKINF_USED_FOREST_TEMPLE_BLUE_WARP)) ? 3 : 2;
    }

    Play_SpawnScene(play,
                    gEntranceTable[((void)0, gSaveContext.entranceIndex) + ((void)0, gSaveContext.sceneLayer)].scene,
                    gEntranceTable[((void)0, gSaveContext.sceneLayer) + ((void)0, gSaveContext.entranceIndex)].spawn);

    osSyncPrintf("\nSCENE_NO=%d COUNTER=%d\n", ((void)0, gSaveContext.entranceIndex), gSaveContext.sceneLayer);

#if 0
    // When entering Gerudo Valley in the credits, trigger the GC emulator to play the ending movie.
    // The emulator constantly checks whether PC is 0x81000000, so this works even though it's not a valid address.
    if ((gEntranceTable[((void)0, gSaveContext.save.entranceIndex)].sceneId == SCENE_GERUDO_VALLEY) &&
        gSaveContext.sceneLayer == 6) {
        PRINTF("エンディングはじまるよー\n"); // "The ending starts"
        ((void (*)(void))0x81000000)();
        PRINTF("出戻り？\n"); // "Return?"
    }
#endif

    Cutscene_HandleEntranceTriggers(play);
    KaleidoScopeCall_Init(play);
    func_801109B0(play);

    if (gSaveContext.nextDayTime != 0xFFFF) {
        if (gSaveContext.nextDayTime == 0x8001) {
            gSaveContext.totalDays++;
            gSaveContext.bgsDayCount++;
            gSaveContext.dogIsLost = true;

            if (Inventory_HatchWeirdEgg(play) || Inventory_HatchPocketCucco(play)) {
                Message_StartTextbox(play, 0x3066, NULL);
            }

            gSaveContext.nextDayTime = 0xFFFE;
        } else {
            gSaveContext.nextDayTime = 0xFFFD;
        }
    }

    SREG(91) = -1;
    R_PAUSE_MENU_MODE = 0;
    PreRender_Init(&play->pauseBgPreRender);
    PreRender_SetValuesSave(&play->pauseBgPreRender, SCREEN_WIDTH, SCREEN_HEIGHT, NULL, NULL, NULL);
    PreRender_SetValues(&play->pauseBgPreRender, SCREEN_WIDTH, SCREEN_HEIGHT, NULL, NULL);
    gTrnsnUnkState = 0;
    play->transitionMode = TRANS_MODE_OFF;
    FrameAdvance_Init(&play->frameAdvCtx);
    Rand_Seed((u32)osGetTime());
    Matrix_Init(&play->state);
    play->state.main = Play_Main;
    play->state.destroy = Play_Destroy;
    play->transitionTrigger = TRANS_TRIGGER_END;
    play->unk_11E16 = 0xFF;
    play->bgCoverAlpha = 0;
    play->haltAllActors = false;

    if (gSaveContext.gameMode != GAMEMODE_TITLE_SCREEN) {
        if (gSaveContext.nextTransitionType == TRANS_NEXT_TYPE_DEFAULT) {
            play->transitionType = ENTRANCE_INFO_END_TRANS_TYPE(
                gEntranceTable[((void)0, gSaveContext.entranceIndex) + baseSceneLayer].field); // Fade In
        } else {
            play->transitionType = gSaveContext.nextTransitionType;
            gSaveContext.nextTransitionType = TRANS_NEXT_TYPE_DEFAULT;
        }
    } else {
        play->transitionType = TRANS_TYPE_FADE_BLACK_SLOW;
    }

    ShrinkWindow_Init();
    TransitionFade_Init(&play->transitionFade);
    TransitionFade_SetType(&play->transitionFade, 3);
    TransitionFade_SetColor(&play->transitionFade, RGBA8(160, 160, 160, 255));
    TransitionFade_Start(&play->transitionFade);
    VisMono_Init(&gPlayVisMono);
    gVisMonoColor.a = 0;
    Flags_UnsetAllEnv(play);

    osSyncPrintf("ZELDA ALLOC SIZE=%x\n", THA_GetSize(&play->state.tha));
    zAllocSize = THA_GetSize(&play->state.tha);
    zAlloc = (uintptr_t)GAMESTATE_ALLOC_MC(&play->state, zAllocSize);
    zAllocAligned = (zAlloc + 8) & ~0xF;
    ZeldaArena_Init((void*)zAllocAligned, zAllocSize - (zAllocAligned - zAlloc));
    // "Zelda Heap"
    osSyncPrintf("ゼルダヒープ %08x-%08x\n", zAllocAligned,
                 (u8*)zAllocAligned + zAllocSize - (s32)(zAllocAligned - zAlloc));

    Fault_AddClient(&D_801614B8, ZeldaArena_Display, NULL, NULL);

    // Handle Rocs Feather requirement
    gItemAgeReqs[ITEM_ROCS_FEATHER] = AGE_REQ_NONE;
    // MergeShip2: MM's transformation masks are usable at either age.
    gItemAgeReqs[ITEM_MM_MASK_FIERCE_DEITY] = AGE_REQ_NONE;
    gItemAgeReqs[ITEM_MM_MASK_GORON] = AGE_REQ_NONE;
    gItemAgeReqs[ITEM_MM_MASK_ZORA] = AGE_REQ_NONE;
    gItemAgeReqs[ITEM_MM_MASK_DEKU] = AGE_REQ_NONE;
    gItemAgeReqs[ITEM_MM_SWORD_FIERCE_DEITY] = AGE_REQ_NONE;
    gItemAgeReqs[ITEM_MM_FD] = AGE_REQ_NONE;
    // MergeShip2: so are MM's other masks (they fit adult Link's head too).
    for (s32 mmMaskItem = ITEM_MM_MASK_TRUTH; mmMaskItem <= ITEM_MM_MASK_GIANT; mmMaskItem++) {
        gItemAgeReqs[mmMaskItem] = AGE_REQ_NONE;
    }
    // MergeShip2: MM's Chateau Romani is a bottle, drunk at either age.
    gItemAgeReqs[ITEM_MM_CHATEAU] = AGE_REQ_NONE;
    // MergeShip2: every other MM item too -- young and adult Link can both use MM's gear.
    for (s32 mmItem = ITEM_MM_SWORD_RAZOR; mmItem <= ITEM_MM_LAST; mmItem++) {
        gItemAgeReqs[mmItem] = AGE_REQ_NONE;
    }
    gSlotAgeReqs[SLOT_NAYRUS_LOVE] = AGE_REQ_NONE;

    Actor_InitContext(play, &play->actorCtx, play->linkActorEntry);

    while (!func_800973FC(play, &play->roomCtx)) {
        ; // Empty Loop
    }

    player = GET_PLAYER(play);
    Camera_InitPlayerSettings(&play->mainCamera, player);
    Camera_RequestMode(&play->mainCamera, CAM_MODE_NORMAL);

    // OTRTODO: Bounds check cameraDataList to guard against scenes spawning the player with
    // an out of bounds background camera index. This requires adding an extra field to the
    // CollisionHeader struct to save the length of cameraDataList.
    // Fixes Dodongo's Cavern blue warp crash.
    {
        CollisionHeader* colHeader = BgCheck_GetCollisionHeader(&play->colCtx, BGCHECK_SCENE);

        u8 camId = player->actor.params & 0xFF;
        // If the player's start cam is out of bounds, set it to 0xFF so it isn't used.
        if (colHeader != NULL && (camId != 0xFF) && (camId >= colHeader->cameraDataListLen)) {
            player->actor.params |= 0xFF;
        }
    }

    playerStartBgCamIndex = player->actor.params & 0xFF;
    if (playerStartBgCamIndex != 0xFF) {
        osSyncPrintf("player has start camera ID (" VT_FGCOL(BLUE) "%d" VT_RST ")\n", playerStartBgCamIndex);
        Camera_ChangeDataIdx(&play->mainCamera, playerStartBgCamIndex);
    }

    if (YREG(15) == 32) {
        play->unk_1242B = 2;
    } else if (YREG(15) == 16) {
        play->unk_1242B = 1;
    } else {
        play->unk_1242B = 0;
    }
    Interface_SetSceneRestrictions(play);
    Environment_PlaySceneSequence(play);
    gSaveContext.seqId = play->sequenceCtx.seqId;
    gSaveContext.natureAmbienceId = play->sequenceCtx.natureAmbienceId;
    Actor_InitPlayerHorse(play, GET_PLAYER(play));
    AnimationContext_Update(play, &play->animationCtx);
    gSaveContext.respawnFlag = 0;

    // #region SOH [Stats]
    if (gSaveContext.ship.stats.sceneNum != gPlayState->sceneNum) {
        u16 idx = gSaveContext.ship.stats.tsIdx;
        gSaveContext.ship.stats.sceneTimestamps[idx].sceneTime = gSaveContext.ship.stats.sceneTimer / 2;
        gSaveContext.ship.stats.sceneTimestamps[idx].roomTime = gSaveContext.ship.stats.roomTimer / 2;
        gSaveContext.ship.stats.sceneTimestamps[idx].scene = gSaveContext.ship.stats.sceneNum;
        gSaveContext.ship.stats.sceneTimestamps[idx].room = gSaveContext.ship.stats.roomNum;
        gSaveContext.ship.stats.sceneTimestamps[idx].isRoom =
            gPlayState->sceneNum == gSaveContext.ship.stats.sceneTimestamps[idx].scene &&
            gPlayState->roomCtx.curRoom.num != gSaveContext.ship.stats.sceneTimestamps[idx].room;
        gSaveContext.ship.stats.tsIdx++;
        gSaveContext.ship.stats.sceneTimer = 0;
        gSaveContext.ship.stats.roomTimer = 0;
    } else if (gSaveContext.ship.stats.roomNum != gPlayState->roomCtx.curRoom.num) {
        u16 idx = gSaveContext.ship.stats.tsIdx;
        gSaveContext.ship.stats.sceneTimestamps[idx].roomTime = gSaveContext.ship.stats.roomTimer / 2;
        gSaveContext.ship.stats.sceneTimestamps[idx].scene = gSaveContext.ship.stats.sceneNum;
        gSaveContext.ship.stats.sceneTimestamps[idx].room = gSaveContext.ship.stats.roomNum;
        gSaveContext.ship.stats.sceneTimestamps[idx].isRoom =
            gPlayState->sceneNum == gSaveContext.ship.stats.sceneTimestamps[idx].scene &&
            gPlayState->roomCtx.curRoom.num != gSaveContext.ship.stats.sceneTimestamps[idx].room;
        gSaveContext.ship.stats.tsIdx++;
        gSaveContext.ship.stats.roomTimer = 0;
    }

    gSaveContext.ship.stats.sceneNum = gPlayState->sceneNum;
    gSaveContext.ship.stats.roomNum = gPlayState->roomCtx.curRoom.num;
    // #endregion

#if 0
    if (R_USE_DEBUG_CUTSCENE) {
        static u64 sDebugCutsceneScriptBuf[0xA00];

        gDebugCutsceneScript = sDebugCutsceneScriptBuf;
        PRINTF("\nkawauso_data=[%x]", gDebugCutsceneScript);

        // This hardcoded ROM address extends past the end of the ROM file.
        // Presumably the ROM was larger at a previous point in development when this debug feature was used.
        DmaMgr_DmaRomToRam(0x03FEB000, gDebugCutsceneScript, sizeof(sDebugCutsceneScriptBuf));
    }
#endif

    // nextEntranceIndex was not initialized, so the previous value was carried over during soft resets.
    gPlayState->nextEntranceIndex = gSaveContext.entranceIndex;
}

void Play_Update(PlayState* play) {
    Input* input = play->state.input;
    s32 isPaused;
    s32 pad1;

    if ((SREG(1) < 0) || (DREG(0) != 0)) {
        SREG(1) = 0;
        ZeldaArena_Display();
    }

    if ((HREG(80) == 18) && (HREG(81) < 0)) {
        u32 i;
        s32 pad2;

        HREG(81) = 0;
        osSyncPrintf("object_exchange_rom_address %u\n", gObjectTableSize);
        osSyncPrintf("RomStart RomEnd   Size\n");

        for (i = 0; i < gObjectTableSize; i++) {
            ptrdiff_t size = gObjectTable[i].vromEnd - gObjectTable[i].vromStart;

            osSyncPrintf("%08x-%08x %08x(%8.3fKB)\n", gObjectTable[i].vromStart, gObjectTable[i].vromEnd, size,
                         size / 1024.0f);
        }

        osSyncPrintf("\n");
    }

    if ((HREG(81) == 18) && (HREG(82) < 0)) {
        HREG(82) = 0;
        // ActorOverlayTable_LogPrint();
    }

    if (CVarGetInteger(CVAR_SETTING("FreeLook.Enabled"), 0) && Player_InCsMode(play)) {
        play->manualCamera = false;
    }

    gSegments[4] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[play->objectCtx.mainKeepIndex].segment);
    gSegments[5] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[play->objectCtx.subKeepIndex].segment);
    gSegments[2] = VIRTUAL_TO_PHYSICAL(play->sceneSegment);

    if (FrameAdvance_Update(&play->frameAdvCtx, &input[1])) {
        if ((play->transitionMode == TRANS_MODE_OFF) && (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
            play->transitionMode = TRANS_MODE_SETUP;
        }

        // #region SOH [Stats] Gameplay stats: Count button presses
        if (!gSaveContext.ship.stats.gameComplete) {
            if (CHECK_BTN_ALL(input[0].press.button, BTN_A)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_A]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_B)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_B]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_CUP)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_CUP]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_CRIGHT)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_CRIGHT]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_CLEFT)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_CLEFT]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_CDOWN)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_CDOWN]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_DUP)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_DUP]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_DRIGHT)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_DRIGHT]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_DDOWN)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_DDOWN]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_DLEFT)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_DLEFT]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_L)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_L]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_R)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_R]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_Z)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_Z]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_START)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_START]++;
            }

            // Start RTA timing on first non-c-up input after intro cutscene
            if (!gSaveContext.ship.stats.firstInput && !Player_InCsMode(play) &&
                ((input[0].press.button && input[0].press.button != 0x8) || input[0].rel.stick_x != 0 ||
                 input[0].rel.stick_y != 0)) {
                gSaveContext.ship.stats.firstInput = GetUnixTimestamp();
            }
        }
        // #endregion

        if (gTrnsnUnkState != 0) {
            switch (gTrnsnUnkState) {
                case 2:
                    if (TransitionUnk_Init(&sTrnsnUnk, 10, 7) == NULL) {
                        osSyncPrintf("fbdemo_init呼出し失敗！\n"); // "fbdemo_init call failed!"
                        gTrnsnUnkState = 0;
                    } else {
                        sTrnsnUnk.zBuffer = (u16*)gZBuffer;
                        gTrnsnUnkState = 3;
                        R_UPDATE_RATE = 1;
                    }
                    break;
                case 3:
                    func_800B23E8(&sTrnsnUnk);
                    break;
            }
        }

        if ((u32)play->transitionMode != TRANS_MODE_OFF) {
            switch (play->transitionMode) {
                case TRANS_MODE_SETUP:
                    if (play->transitionTrigger != TRANS_TRIGGER_END) {
                        s16 sceneLayer = 0;
                        Interface_ChangeHudVisibilityMode(1);

                        if (gSaveContext.cutsceneIndex >= 0xFFF0) {
                            sceneLayer = SCENE_LAYER_CUTSCENE_FIRST + (gSaveContext.cutsceneIndex & 0xF);
                        }

                        // fade out bgm if "continue bgm" flag is not set
                        if (!(gEntranceTable[play->nextEntranceIndex + sceneLayer].field &
                              ENTRANCE_INFO_CONTINUE_BGM_FLAG)) {
                            // "Sound initalized. 111"
                            osSyncPrintf("\n\n\nサウンドイニシャル来ました。111");
                            if ((play->transitionType < TRANS_TYPE_MAX) && !Environment_IsForcedSequenceDisabled()) {
                                // "Sound initalized. 222"
                                osSyncPrintf("\n\n\nサウンドイニシャル来ました。222");
                                func_800F6964(0x14);
                                gSaveContext.seqId = (u8)NA_BGM_DISABLED;
                                gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
                            }
                        }
                    }

                    if (!R_TRANS_DBG_ENABLED) {
                        Gameplay_SetupTransition(play, play->transitionType);
                    } else {
                        Gameplay_SetupTransition(play, R_TRANS_DBG_TYPE);
                    }

                    if (play->transitionMode >= TRANS_MODE_FILL_WHITE_INIT) {
                        // non-instance modes break out of this switch
                        break;
                    }
                    FALLTHROUGH;
                case TRANS_MODE_INSTANCE_INIT:
                    play->transitionCtx.init(&play->transitionCtx.data);

                    // Circle Transition Types
                    if ((play->transitionCtx.transitionType >> 5) == 1) {
                        play->transitionCtx.setType(&play->transitionCtx.data,
                                                    play->transitionCtx.transitionType | TC_SET_PARAMS);
                    }

                    gSaveContext.transWipeSpeed = 14;

                    if ((play->transitionCtx.transitionType == TRANS_TYPE_WIPE_FAST) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FILL_WHITE2)) {
                        //! @bug TRANS_TYPE_FILL_WHITE2 will never reach this code.
                        //! It is a non-instance type transition which doesn't run this case.
                        gSaveContext.transWipeSpeed = 28;
                    }

                    gSaveContext.transFadeDuration = 60;

                    if ((play->transitionCtx.transitionType == TRANS_TYPE_FADE_BLACK_FAST) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_FAST)) {
                        gSaveContext.transFadeDuration = 20;
                    } else if ((play->transitionCtx.transitionType == TRANS_TYPE_FADE_BLACK_SLOW) ||
                               (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_SLOW)) {
                        gSaveContext.transFadeDuration = 150;
                    } else if (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_INSTANT) {
                        gSaveContext.transFadeDuration = 2;
                    }

                    if ((play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_FAST) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_SLOW) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_CS_DELAYED) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_INSTANT)) {
                        play->transitionCtx.setColor(&play->transitionCtx.data, RGBA8(160, 160, 160, 255));

                        if (play->transitionCtx.setEnvColor != NULL) {
                            play->transitionCtx.setEnvColor(&play->transitionCtx.data, RGBA8(160, 160, 160, 255));
                        }
                    } else if (play->transitionCtx.transitionType == TRANS_TYPE_FADE_GREEN) {
                        play->transitionCtx.setColor(&play->transitionCtx.data, RGBA8(140, 140, 100, 255));

                        if (play->transitionCtx.setEnvColor != NULL) {
                            play->transitionCtx.setEnvColor(&play->transitionCtx.data, RGBA8(140, 140, 100, 255));
                        }
                    } else if (play->transitionCtx.transitionType == TRANS_TYPE_FADE_BLUE) {
                        play->transitionCtx.setColor(&play->transitionCtx.data, RGBA8(70, 100, 110, 255));

                        if (play->transitionCtx.setEnvColor != NULL) {
                            play->transitionCtx.setEnvColor(&play->transitionCtx.data, RGBA8(70, 100, 110, 255));
                        }
                    } else {
                        play->transitionCtx.setColor(&play->transitionCtx.data, RGBA8(0, 0, 0, 0));

                        if (play->transitionCtx.setEnvColor != NULL) {
                            play->transitionCtx.setEnvColor(&play->transitionCtx.data, RGBA8(0, 0, 0, 0));
                        }
                    }

                    if (play->transitionTrigger == TRANS_TRIGGER_END) {
                        play->transitionCtx.setType(&play->transitionCtx.data, 1);
                    } else {
                        play->transitionCtx.setType(&play->transitionCtx.data, 2);
                    }

                    play->transitionCtx.start(&play->transitionCtx);

                    if (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_CS_DELAYED) {
                        play->transitionMode = TRANS_MODE_INSTANCE_WAIT;
                    } else {
                        play->transitionMode = TRANS_MODE_INSTANCE_RUNNING;
                    }
                    break;

                case TRANS_MODE_INSTANCE_RUNNING:
                    if (play->transitionCtx.isDone(&play->transitionCtx.data)) {
                        if (play->transitionCtx.transitionType >= TRANS_TYPE_MAX) {
                            if (play->transitionTrigger == TRANS_TRIGGER_END) {
                                play->transitionCtx.destroy(&play->transitionCtx.data);
                                func_800BC88C(play);
                                play->transitionMode = TRANS_MODE_OFF;
                            }
                        } else if (play->transitionTrigger != TRANS_TRIGGER_END) {
                            // MergeShip2: the door to Termina holds the faded-out frame while the player is in the
                            // other engine, and commits the arrival when they come back (MergeShipHost.h).
                            if (MergeShipHost_HoldTransition(play)) {
                                break;
                            }
                            play->state.running = false;

                            if (gSaveContext.gameMode != GAMEMODE_FILE_SELECT) {
                                SET_NEXT_GAMESTATE(&play->state, Play_Init, PlayState);
                                gSaveContext.entranceIndex = play->nextEntranceIndex;

                                if (gSaveContext.minigameState == 1) {
                                    gSaveContext.minigameState = 3;
                                }
                            } else {
                                SET_NEXT_GAMESTATE(&play->state, FileChoose_Init, FileChooseContext);
                            }
                        } else {
                            play->transitionCtx.destroy(&play->transitionCtx.data);
                            func_800BC88C(play);
                            play->transitionMode = TRANS_MODE_OFF;

                            if (gTrnsnUnkState == 3) {
                                TransitionUnk_Destroy(&sTrnsnUnk);
                                gTrnsnUnkState = 0;
                                R_UPDATE_RATE = 3;
                            }

                            // Transition end for standard transitions
                            GameInteractor_ExecuteOnTransitionEndHooks(play->sceneNum);
                        }

                        play->transitionTrigger = TRANS_TRIGGER_OFF;
                    } else {
                        play->transitionCtx.update(&play->transitionCtx.data, R_UPDATE_RATE);
                    }
                    break;
            }

            // update non-instance transitions
            switch (play->transitionMode) {
                case TRANS_MODE_FILL_WHITE_INIT:
                    sTransitionFillTimer = 0;
                    play->envCtx.fillScreen = true;
                    play->envCtx.screenFillColor[0] = 160;
                    play->envCtx.screenFillColor[1] = 160;
                    play->envCtx.screenFillColor[2] = 160;

                    if (play->transitionTrigger != TRANS_TRIGGER_END) {
                        play->envCtx.screenFillColor[3] = 0;
                        play->transitionMode = TRANS_MODE_FILL_IN;
                    } else {
                        play->envCtx.screenFillColor[3] = 255;
                        play->transitionMode = TRANS_MODE_FILL_OUT;
                    }
                    break;

                case TRANS_MODE_FILL_IN:
                    play->envCtx.screenFillColor[3] = (sTransitionFillTimer / 20.0f) * 255.0f;

                    if (sTransitionFillTimer >= 20) {
                        if (MergeShipHost_HoldTransition(play)) { // MergeShip2: the door to Termina
                            break;
                        }
                        play->state.running = false;
                        SET_NEXT_GAMESTATE(&play->state, Play_Init, PlayState);
                        gSaveContext.entranceIndex = play->nextEntranceIndex;
                        play->transitionTrigger = TRANS_TRIGGER_OFF;
                        play->transitionMode = TRANS_MODE_OFF;
                    } else {
                        sTransitionFillTimer++;
                    }
                    break;

                case TRANS_MODE_FILL_OUT:
                    play->envCtx.screenFillColor[3] = (1 - sTransitionFillTimer / 20.0f) * 255.0f;

                    if (sTransitionFillTimer >= 20) {
                        gTrnsnUnkState = 0;
                        R_UPDATE_RATE = 3;
                        play->transitionTrigger = TRANS_TRIGGER_OFF;
                        play->transitionMode = TRANS_MODE_OFF;
                        play->envCtx.fillScreen = false;
                    } else {
                        sTransitionFillTimer++;
                    }
                    break;

                case TRANS_MODE_FILL_BROWN_INIT:
                    sTransitionFillTimer = 0;
                    play->envCtx.fillScreen = true;
                    play->envCtx.screenFillColor[0] = 170;
                    play->envCtx.screenFillColor[1] = 160;
                    play->envCtx.screenFillColor[2] = 150;

                    if (play->transitionTrigger != TRANS_TRIGGER_END) {
                        play->envCtx.screenFillColor[3] = 0;
                        play->transitionMode = TRANS_MODE_FILL_IN;
                    } else {
                        play->envCtx.screenFillColor[3] = 255;
                        play->transitionMode = TRANS_MODE_FILL_OUT;
                    }
                    break;

                case TRANS_MODE_INSTANT:
                    if (play->transitionTrigger != TRANS_TRIGGER_END) {
                        if (MergeShipHost_HoldTransition(play)) { // MergeShip2: the door to Termina
                            break;
                        }
                        play->state.running = 0;
                        SET_NEXT_GAMESTATE(&play->state, Play_Init, PlayState);
                        gSaveContext.entranceIndex = play->nextEntranceIndex;
                        play->transitionTrigger = TRANS_TRIGGER_OFF;
                        play->transitionMode = TRANS_MODE_OFF;
                    } else {
                        gTrnsnUnkState = 0;
                        R_UPDATE_RATE = 3;
                        play->transitionTrigger = TRANS_TRIGGER_OFF;
                        play->transitionMode = TRANS_MODE_OFF;
                    }
                    break;

                case TRANS_MODE_INSTANCE_WAIT:
                    if (gSaveContext.cutsceneTransitionControl != 0) {
                        play->transitionMode = TRANS_MODE_INSTANCE_RUNNING;
                    }
                    break;

                case TRANS_MODE_SANDSTORM_INIT:
                    if (play->transitionTrigger != TRANS_TRIGGER_END) {
                        play->envCtx.sandstormState = SANDSTORM_FILL;
                        play->transitionMode = TRANS_MODE_SANDSTORM;
                    } else {
                        play->envCtx.sandstormState = SANDSTORM_UNFILL;
                        play->envCtx.sandstormPrimA = 255;
                        play->envCtx.sandstormEnvA = 255;
                        play->transitionMode = TRANS_MODE_SANDSTORM;
                    }
                    break;

                case TRANS_MODE_SANDSTORM:
                    Audio_PlaySfxGeneral(NA_SE_EV_SAND_STORM - SFX_FLAG, &gSfxDefaultPos, 4,
                                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

                    if (play->transitionTrigger == TRANS_TRIGGER_END) {
                        if (play->envCtx.sandstormPrimA < 110) {
                            gTrnsnUnkState = 0;
                            R_UPDATE_RATE = 3;
                            play->transitionTrigger = TRANS_TRIGGER_OFF;
                            play->transitionMode = TRANS_MODE_OFF;

                            // Transition end for sandstorm effect (delayed until effect is finished)
                            GameInteractor_ExecuteOnTransitionEndHooks(play->sceneNum);
                        }
                    } else {
                        if (play->envCtx.sandstormEnvA == 255) {
                            if (MergeShipHost_HoldTransition(play)) { // MergeShip2: the door to Termina
                                break;
                            }
                            play->state.running = false;
                            SET_NEXT_GAMESTATE(&play->state, Play_Init, PlayState);
                            gSaveContext.entranceIndex = play->nextEntranceIndex;
                            play->transitionTrigger = TRANS_TRIGGER_OFF;
                            play->transitionMode = TRANS_MODE_OFF;
                        }
                    }
                    break;

                case TRANS_MODE_SANDSTORM_END_INIT:
                    if (play->transitionTrigger == TRANS_TRIGGER_END) {
                        play->envCtx.sandstormState = SANDSTORM_DISSIPATE;
                        play->envCtx.sandstormPrimA = 255;
                        play->envCtx.sandstormEnvA = 255;
                        // "It's here!!!!!!!!!"
                        LOG_STRING("来た!!!!!!!!!!!!!!!!!!!!!");
                        play->transitionMode = TRANS_MODE_SANDSTORM_END;
                    } else {
                        play->transitionMode = TRANS_MODE_SANDSTORM_INIT;
                    }
                    break;

                case TRANS_MODE_SANDSTORM_END:
                    Audio_PlaySfxGeneral(NA_SE_EV_SAND_STORM - SFX_FLAG, &gSfxDefaultPos, 4,
                                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

                    if (play->transitionTrigger == TRANS_TRIGGER_END) {
                        if (play->envCtx.sandstormPrimA <= 0) {
                            gTrnsnUnkState = 0;
                            R_UPDATE_RATE = 3;
                            play->transitionTrigger = TRANS_TRIGGER_OFF;
                            play->transitionMode = TRANS_MODE_OFF;

                            // Transition end for sandstorm effect (delayed until effect is finished)
                            GameInteractor_ExecuteOnTransitionEndHooks(play->sceneNum);
                        }
                    }
                    break;

                case TRANS_MODE_CS_BLACK_FILL_INIT:
                    sTransitionFillTimer = 0;
                    play->envCtx.fillScreen = true;
                    play->envCtx.screenFillColor[0] = 0;
                    play->envCtx.screenFillColor[1] = 0;
                    play->envCtx.screenFillColor[2] = 0;
                    play->envCtx.screenFillColor[3] = 255;
                    play->transitionMode = TRANS_MODE_CS_BLACK_FILL;
                    break;

                case TRANS_MODE_CS_BLACK_FILL:
                    if (gSaveContext.cutsceneTransitionControl != 0) {
                        play->envCtx.screenFillColor[3] = gSaveContext.cutsceneTransitionControl;

                        if (gSaveContext.cutsceneTransitionControl <= 100) {
                            gTrnsnUnkState = 0;
                            R_UPDATE_RATE = 3;
                            play->transitionTrigger = TRANS_TRIGGER_OFF;
                            play->transitionMode = TRANS_MODE_OFF;
                        }
                    }
                    break;
            }
        }

        PLAY_LOG(3533);

        if (1 && (gTrnsnUnkState != 3)) {
            PLAY_LOG(3542);

            if ((gSaveContext.gameMode == GAMEMODE_NORMAL) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
                (play->gameOverCtx.state == GAMEOVER_INACTIVE)) {
                KaleidoSetup_Update(play);
            }

            PLAY_LOG(3551);
            isPaused = (play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0);

            PLAY_LOG(3555);
            AnimationContext_Reset(&play->animationCtx);

            PLAY_LOG(3561);
            Object_UpdateBank(&play->objectCtx);

            PLAY_LOG(3577);

            if (!isPaused && (IREG(72) == 0)) {
                PLAY_LOG(3580);

                play->gameplayFrames++;
                func_800AA178(true);

                // Gameplay stat tracking
                if (!gSaveContext.ship.stats.gameComplete &&
                    (!IS_BOSS_RUSH || !gSaveContext.ship.quest.data.bossRush.isPaused)) {
                    gSaveContext.ship.stats.playTimer++;
                    gSaveContext.ship.stats.sceneTimer++;
                    gSaveContext.ship.stats.roomTimer++;
                }

                if (play->actorCtx.freezeFlashTimer && (play->actorCtx.freezeFlashTimer-- < 5)) {
                    if (GameInteractor_Should(VB_FLASH_SCREEN_FOR_FINISHING_BLOW, true)) {
                        osSyncPrintf("FINISH=%d\n", play->actorCtx.freezeFlashTimer);

                        if ((play->actorCtx.freezeFlashTimer > 0) && ((play->actorCtx.freezeFlashTimer % 2) != 0)) {
                            play->envCtx.fillScreen = true;
                            play->envCtx.screenFillColor[0] = play->envCtx.screenFillColor[1] =
                                play->envCtx.screenFillColor[2] = 150;
                            play->envCtx.screenFillColor[3] = 80;
                        } else {
                            play->envCtx.fillScreen = false;
                        }
                    }
                } else {
                    PLAY_LOG(3606);
                    func_800973FC(play, &play->roomCtx);

                    PLAY_LOG(3612);
                    CollisionCheck_AT(play, &play->colChkCtx);

                    PLAY_LOG(3618);
                    CollisionCheck_OC(play, &play->colChkCtx);

                    PLAY_LOG(3624);
                    CollisionCheck_Damage(play, &play->colChkCtx);

                    PLAY_LOG(3631);
                    CollisionCheck_ClearContext(play, &play->colChkCtx);

                    PLAY_LOG(3637);

                    if (!play->haltAllActors) {
                        Actor_UpdateAll(play, &play->actorCtx);
                    }

                    PLAY_LOG(3643);
                    func_80064558(play, &play->csCtx);

                    PLAY_LOG(3648);
                    func_800645A0(play, &play->csCtx);

                    PLAY_LOG(3651);
                    if (!MergeShipTest_ActorsFrozen(-1)) { // MergeShip2: the test harness holds the effects still too
                        Effect_UpdateAll(play);

                        PLAY_LOG(3657);
                        EffectSs_UpdateAll(play);
                    }

                    PLAY_LOG(3662);
                }
            } else {
                func_800AA178(false);
            }

            PLAY_LOG(3672);
            func_80095AA0(play, &play->roomCtx.curRoom, &input[1], 0);

            PLAY_LOG(3675);
            func_80095AA0(play, &play->roomCtx.prevRoom, &input[1], 1);

            PLAY_LOG(3677);

            if (play->unk_1242B != 0) {
                if (CHECK_BTN_ALL(input[0].press.button, BTN_CUP)) {
                    if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0)) {
                        // "Changing viewpoint is prohibited due to the kaleidoscope"
                        osSyncPrintf(VT_FGCOL(CYAN) "カレイドスコープ中につき視点変更を禁止しております\n" VT_RST);
                    } else if (Player_InCsMode(play)) {
                        // "Changing viewpoint is prohibited during the cutscene"
                        osSyncPrintf(VT_FGCOL(CYAN) "デモ中につき視点変更を禁止しております\n" VT_RST);
                    } else if (YREG(15) == 0x10) {
                        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    } else {
                        // C-Up toggle for houses, move between pivot camera and fixed camera
                        // Toggle viewpoint between VIEWPOINT_LOCKED and VIEWPOINT_PIVOT
                        Play_SetViewpoint(play, play->unk_1242B ^ 3);
                    }
                }

                Play_RequestViewpointBgCam(play);
            }

            PLAY_LOG(3708);
            SkyboxDraw_Update(&play->skyboxCtx);

            PLAY_LOG(3716);

            if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0)) {
                PLAY_LOG(3721);
                KaleidoScopeCall_Update(play);
            } else if (play->gameOverCtx.state != GAMEOVER_INACTIVE) {
                PLAY_LOG(3727);
                GameOver_Update(play);
            } else {
                PLAY_LOG(3733);
                Message_Update(play);
            }

            PLAY_LOG(3737);

            PLAY_LOG(3742);
            Interface_Update(play);

            PLAY_LOG(3765);
            AnimationContext_Update(play, &play->animationCtx);

            PLAY_LOG(3771);
            SoundSource_UpdateAll(play);

            PLAY_LOG(3777);
            ShrinkWindow_Update(R_UPDATE_RATE);

            PLAY_LOG(3783);
            TransitionFade_Update(&play->transitionFade, R_UPDATE_RATE);
        } else {
            goto skip;
        }
    }

    PLAY_LOG(3799);

skip:
    PLAY_LOG(3801);

    GameInteractor_ExecuteOnCameraState(play);

    if (!isPaused || gDbgCamEnabled) {
        s32 i;

        play->nextCamera = play->activeCamera;

        PLAY_LOG(3806);

        for (i = 0; i < NUM_CAMS; i++) {
            if ((i != play->nextCamera) && (play->cameraPtrs[i] != NULL)) {
                PLAY_LOG(3809);
                Camera_Update(play->cameraPtrs[i]);
            }
        }

        Camera_Update(play->cameraPtrs[play->nextCamera]);

        PLAY_LOG(3814);
    }

    // MergeShip2: MM Play_UpdateMain runs Distortion_Update right after the cameras.
    if (!isPaused) {
        MergeShip_Distortion_Update(play);
    }

    PLAY_LOG(3816);
    Environment_Update(play, &play->envCtx, &play->lightCtx, &play->pauseCtx, &play->msgCtx, &play->gameOverCtx,
                       play->state.gfxCtx);
}

// MergeShip2 real-time shadows (the "Real-Time Shadows" option; libultraship fast/interpreter.cpp, "MergeShip2
// real-time shadows"). The renderer draws the frame once more as the light sees it and darkens what lies in shadow;
// the game says where the light is and where in the frame the darkening goes: behind the world's opaque surfaces
// and before everything else, which is the head of the translucent buffer (its view matrices are in already).
// Outdoors the light is the sun, or the moon while the sun is down (weaker); the two cross at the horizon, where
// the shadows fade out and in again. Indoors it is a light from above, a little aslant, and only cast shadows are
// drawn (a wall is not darkened for facing away from a lamp that is not there). That light comes down from a level
// plane under the ceiling over Link: what is above the plane (window sills, beams, the next floor up) is not there
// for it, and casts nothing into the room.
typedef struct {
    u8 on;
    u8 bothSides; // what faces away from the light covers too
    u8 fromPlane; // the light comes down from a level plane at `top`
    u8 casts;     // shadows are cast (else the pass over the screen is only for the ambient shading and the clouds)
    f32 dir[3];   // the way to the light
    f32 focus[3]; // the middle of what the shadows cover
    f32 radius;
    f32 shade; // how bright a surface in shadow stays
    f32 away;  // the same for a surface turned away from the light
    f32 top;   // (fromPlane) the plane's height
    f32 deep;  // (fromPlane) how far under it the light reaches
    f32 reach; // (fromPlane) how high the plane is over the ground: a shadow fades out towards that far from what
               // casts it (a wall the plane cuts off leaves no band behind it)
    // The rest of what the same pass over the screen does (the "Ambient Shading" and "Cloud Shadows" options):
    f32 ao;      // how dark the ambient shading goes: where other surfaces stand close over a place (a corner, the
                 // foot of a wall, under a bush) less light arrives (0: none)
    f32 aoReach; // how far it looks, in the world's units
    f32 clouds;  // how dark a cloud's shadow is: they drift over open country with the wind (0: none)
    f32 tint[3]; // what a shadow leaves of red, green and blue against its plain shade (by day a shadow is lit by
                 // the sky: bluer than what the sun lights)
    f32 sun[3];  // how much brighter what the light reaches is drawn (the "Warm Sunlight" option; 1: as it is)
    // The game's lamps (the "Real Light Sources" option): its point lights -- torches, fires, fairies, explosions,
    // spells -- light the surfaces about them, with the game's own colour, reach and flicker. The game itself
    // lights only characters by them, and no wall or floor.
    s32 lampCount;     // at most 12, the nearest to what the camera looks at
    f32 lampStrength;  // how strongly their light is drawn
    f32 lamps[12][8];  // each: its place (x, y, z), how far it reaches, its colour (0 to 1), one unused
} MergeShipShadowLight;

static MergeShipShadowLight sMergeShipShadowLight;
static f32 sMergeShipShadowTop;
static s32 sMergeShipShadowTopScene = -1;

// A lamp's colour as the screen pass takes it: the game redraws a torch's brightness from nothing every frame
// (anywhere between a half and all of it), which on a character is a flicker and on a whole wall would be a strobe.
// So each lamp's colour follows the game's a part of the way a frame.
typedef struct {
    LightInfo* info;
    f32 color[3];
    u32 frame;
} MergeShipLampEase;

static MergeShipLampEase sMergeShipLampEase[32];
static u32 sMergeShipLampFrame;
static s32 sMergeShipLamps;

static void Play_MergeShipLampColor(LightInfo* info, f32* color) {
    MergeShipLampEase* found = NULL;
    MergeShipLampEase* spare = NULL;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sMergeShipLampEase); i++) {
        MergeShipLampEase* ease = &sMergeShipLampEase[i];
        s32 fresh = ease->info != NULL && (sMergeShipLampFrame - ease->frame) <= 2;

        if (fresh && ease->info == info) {
            found = ease;
            break;
        }
        if (!fresh && spare == NULL) {
            spare = ease;
        }
    }
    if (found != NULL) {
        for (i = 0; i < 3; i++) {
            found->color[i] += (color[i] - found->color[i]) * 0.22f;
            color[i] = found->color[i];
        }
        found->frame = sMergeShipLampFrame;
    } else if (spare != NULL) {
        // (a lamp first seen: at the middle of a flicker)
        spare->info = info;
        for (i = 0; i < 3; i++) {
            spare->color[i] = color[i];
        }
        spare->frame = sMergeShipLampFrame;
    }
}

// Whether the screen pass draws the lamps' light this frame: it lights the characters as it does the walls, so the
// game's own light from them on a character is left out (z_lights.c), or he would be lit twice.
s32 Play_MergeShipLampsOn(void) {
    return sMergeShipLamps && sMergeShipShadowLight.on;
}

// Whether this frame's world is drawn with them (the actors' own round shadows are then left out: z_actor.c). Not
// where the room is a painted picture with no floor drawn (the Market, a shop seen from its fixed camera): nothing
// is there for a shadow to fall on, so the round shadows stay.
s32 Play_MergeShipShadowsOn(void) {
    if (!sMergeShipShadowLight.on || !sMergeShipShadowLight.casts) {
        return false;
    }
    if (gPlayState != NULL && gPlayState->roomCtx.curRoom.meshHeader != NULL &&
        gPlayState->roomCtx.curRoom.meshHeader->base.type == 1 && !MergeShipPort3ds_RoomDrawn()) {
        return false;
    }
    return true;
}

static void Play_MergeShipShadows(PlayState* play) {
    MergeShipShadowLight* light = &sMergeShipShadowLight;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    f32 strength = CVarGetInteger("gMergeShip.ShadowStrength", 45) / 100.0f;
    f32 length;
    f32 high;
    Vec3f forward;
    Gfx* gfx;
    s32 shadows = CVarGetInteger("gMergeShip.Shadows", 0);
    s32 shading = CVarGetInteger("gMergeShip.AmbientShading", 0);
    s32 clouds = CVarGetInteger("gMergeShip.CloudShadows", 0);
    s32 lamps = CVarGetInteger("gMergeShip.RealLights", 0);
    s32 sunUp = true;
    f32 daylight = 1.0f;

    light->on = false;
    light->casts = false;
    light->lampCount = 0;
    sMergeShipLamps = false;
    if ((!shadows && !shading && !clouds && !lamps) || !MergeShipPort3ds_ShadowsSupported()) {
        return;
    }
    sMergeShipLamps = lamps != 0;
    sMergeShipLampFrame++;
    if (lamps) {
        // the nearest twelve of the game's point lights to what the camera looks at
        LightNode* node;
        f32 far2[12];
        s32 n;

        light->lampStrength = CLAMP(CVarGetInteger("gMergeShip.RealLightsStrength", 100), 20, 200) / 100.0f;
        for (node = play->lightCtx.listHead; node != NULL; node = node->next) {
            LightPoint* point;
            f32 color[3];
            f32 dx;
            f32 dy;
            f32 dz;
            f32 d2;
            s32 slot;

            if (node->info == NULL || node->info->type == LIGHT_DIRECTIONAL) {
                continue;
            }
            point = &node->info->params.point;
            if (point->radius <= 0 || (point->color[0] | point->color[1] | point->color[2]) == 0) {
                continue;
            }
            dx = point->x - play->view.lookAt.x;
            dy = point->y - play->view.lookAt.y;
            dz = point->z - play->view.lookAt.z;
            d2 = SQ(dx) + SQ(dy) + SQ(dz);
            // (the pass draws a lamp's light up to 900 units from it)
            if (d2 > SQ(1500.0f + 900.0f)) {
                continue;
            }
            color[0] = point->color[0] / 255.0f;
            color[1] = point->color[1] / 255.0f;
            color[2] = point->color[2] / 255.0f;
            Play_MergeShipLampColor(node->info, color);
            // (kept in order of distance: a nearer lamp pushes the farthest out)
            slot = light->lampCount;
            if (slot == 12) {
                if (d2 >= far2[11]) {
                    continue;
                }
                slot = 11;
            } else {
                light->lampCount++;
            }
            while (slot > 0 && far2[slot - 1] > d2) {
                far2[slot] = far2[slot - 1];
                for (n = 0; n < 8; n++) {
                    light->lamps[slot][n] = light->lamps[slot - 1][n];
                }
                slot--;
            }
            far2[slot] = d2;
            light->lamps[slot][0] = point->x;
            light->lamps[slot][1] = point->y;
            light->lamps[slot][2] = point->z;
            light->lamps[slot][3] = point->radius;
            light->lamps[slot][4] = color[0];
            light->lamps[slot][5] = color[1];
            light->lamps[slot][6] = color[2];
            light->lamps[slot][7] = 0.0f;
        }
    }
    light->casts = shadows != 0;
    light->ao = shading ? CLAMP(CVarGetInteger("gMergeShip.AmbientShadingStrength", 60), 0, 100) / 100.0f : 0.0f;
    light->aoReach = 34.0f;
    light->clouds = 0.0f;
    light->tint[0] = light->tint[1] = light->tint[2] = 1.0f;
    light->sun[0] = light->sun[1] = light->sun[2] = 1.0f;
    strength = CLAMP(strength, 0.0f, 0.9f);
    if (play->envCtx.indoors || play->envCtx.sunMoonDisabled) {
        Player* player = GET_PLAYER(play);
        Vec3f under = player->actor.world.pos;
        f32 ceilingY;
        f32 top = player->actor.world.pos.y + 160.0f;

        // (leant far enough that a shadow shows beside its thing: the ceiling and the walls do not stand in its way)
        light->dir[0] = 0.40f;
        light->dir[1] = 0.87f;
        light->dir[2] = 0.28f;
        light->shade = 1.0f - strength * 0.75f;
        light->away = 1.0f;
        // (a room's ceiling and the walls on the light's side are drawn from inside alone: the light comes
        // through them)
        light->bothSides = false;
        // the light's plane: over anyone's head, and under the ceiling over Link where that is lower (what is on
        // the floor above would cast into this room)
        under.y += 30.0f;
        if (BgCheck_AnyCheckCeiling(&play->colCtx, &ceilingY, &under, 170.0f)) {
            top = MIN(top, ceilingY + 170.0f - 10.0f);
        }
        top = MAX(top, player->actor.world.pos.y + 60.0f);
        // (eased: a doorway's lintel is overhead for a few frames only)
        if (sMergeShipShadowTopScene != play->sceneNum || fabsf(top - sMergeShipShadowTop) > 700.0f) {
            sMergeShipShadowTop = top;
            sMergeShipShadowTopScene = play->sceneNum;
        } else {
            Math_ApproachF(&sMergeShipShadowTop, top, 0.3f, 30.0f);
        }
        light->fromPlane = true;
        light->top = sMergeShipShadowTop;
        light->deep = 1600.0f;
        light->reach = MAX(sMergeShipShadowTop - player->actor.world.pos.y, 40.0f);
    } else {
        Vec3f sun = play->envCtx.sunPos;

        length = sqrtf(SQ(sun.x) + SQ(sun.y) + SQ(sun.z));
        if (length < 1.0f) {
            // (no sun to go by: nothing casts; what else is asked for stays)
            sun.x = 0.0f;
            sun.y = 1.0f;
            sun.z = 0.0f;
            length = 1.0f;
            light->casts = false;
        }
        high = sun.y / length;
        // (the moon is across the sky from the sun)
        if (high < 0.0f) {
            sun.x = -sun.x;
            sun.y = -sun.y;
            sun.z = -sun.z;
            strength *= 0.55f;
            sunUp = false;
        }
        high = fabsf(high);
        // at the horizon no shadow; and none longer than a light a fifth up the sky throws
        daylight = CLAMP((high - 0.04f) / 0.18f, 0.0f, 1.0f);
        strength *= daylight;
        if (strength <= 0.01f) {
            // (the light is at the horizon: nothing casts; the ambient shading stays)
            light->casts = false;
        }
        // the clouds' shadows, fainter under the moon; and the colour of shade: what the sun does not reach is lit
        // by the sky, and is bluer
        if (clouds) {
            light->clouds = (sunUp ? 0.55f : 0.25f) * daylight;
        }
        light->tint[0] = sunUp ? 0.88f : 0.94f;
        light->tint[1] = sunUp ? 0.94f : 0.97f;
        light->tint[2] = 1.0f;
        // (and what the sun does reach is a little brighter and warmer for it)
        if (sunUp && CVarGetInteger("gMergeShip.SunWarmth", 0)) {
            light->sun[0] = 1.0f + 0.12f * daylight;
            light->sun[1] = 1.0f + 0.08f * daylight;
            light->sun[2] = 1.0f + 0.01f * daylight;
        }
        light->dir[0] = sun.x / length;
        light->dir[1] = MAX(sun.y / length, 0.3f);
        light->dir[2] = sun.z / length;
        // Nor a shadow shorter than half of what throws it: a light straight overhead puts every shadow under its
        // own thing, where nobody sees it, and the sun and the moon pass close to that. So the light is leant out
        // the way it leans already, until it stands 28 degrees off overhead.
        {
            f32 flat = sqrtf(SQ(light->dir[0]) + SQ(light->dir[2]));
            f32 least = light->dir[1] * 0.53f;

            if (flat < least) {
                if (flat > 0.001f) {
                    light->dir[0] *= least / flat;
                    light->dir[2] *= least / flat;
                } else {
                    light->dir[2] = least;
                }
            }
        }
        light->shade = 1.0f - strength;
        light->away = 1.0f - strength * 0.4f;
        // (a cliff drawn from one side only still stands between the sun and the ground)
        light->bothSides = true;
        light->fromPlane = false;
    }
    // about what the camera looks at, and somewhat on from it
    forward.x = play->view.lookAt.x - play->view.eye.x;
    forward.y = 0.0f;
    forward.z = play->view.lookAt.z - play->view.eye.z;
    length = sqrtf(SQ(forward.x) + SQ(forward.z));
    if (length > 1.0f) {
        forward.x /= length;
        forward.z /= length;
    } else {
        forward.x = forward.z = 0.0f;
    }
    light->radius = 1400.0f;
    light->focus[0] = play->view.lookAt.x + forward.x * 500.0f;
    light->focus[1] = play->view.lookAt.y;
    light->focus[2] = play->view.lookAt.z + forward.z * 500.0f;
    light->on = true;

    OPEN_DISPS(gfxCtx);
    gfx = POLY_XLU_DISP;
    gfx->words.w0 = (0x4Fu << 24) | (9u << 16); // G_REGS_EX 9: the light
    gfx->words.w1 = (uintptr_t)light;
    gfx++;
    gfx->words.w0 = (0x4Fu << 24) | (8u << 16); // G_REGS_EX 8: darken here
    gfx->words.w1 = 0;
    gfx++;
    POLY_XLU_DISP = gfx;
    CLOSE_DISPS(gfxCtx);
}

// The finished picture's glow and look (the "Glow", "Film Look", "Darker Corners" and "Background Blur" options):
// one pass over the world as it has been drawn, before the game's fades, its HUD and its text
// (GfxRenderingAPI::ApplyPost; Direct3D 11). `slot` is the place kept for it in the overlay's list, where the world
// is finished; the world's camera is kept for it here, at the head of what is drawn of the world.
static f32 sMergeShipPost[48];
static f32 sMergeShipFocus;     // how far the background's blur has come in (0 to 1)
static f32 sMergeShipFocusAt;   // the distance in focus
static f32 sMergeShipFocusWide; // how far either side of it stays sharp

static void Play_MergeShipPost(PlayState* play, Gfx* slot) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 glow = CVarGetInteger("gMergeShip.Glow", 0);
    s32 film = CVarGetInteger("gMergeShip.FilmLook", 0);
    s32 corners = CVarGetInteger("gMergeShip.Vignette", 0);
    s32 blur = CVarGetInteger("gMergeShip.FocusBlur", 0);
    f32* p = sMergeShipPost;
    Gfx* gfx;

    if (slot == NULL || (!glow && !film && !corners && !blur) || !MergeShipPort3ds_ShadowsSupported()) {
        sMergeShipFocus = 0.0f;
        return;
    }
    memset(sMergeShipPost, 0, sizeof(sMergeShipPost));
    if (glow) {
        p[0] = 0.70f;
        p[1] = CLAMP(CVarGetInteger("gMergeShip.GlowStrength", 100), 10, 200) / 100.0f;
    }
    p[5] = 1.0f;
    if (film) {
        f32 amount = CLAMP(CVarGetInteger("gMergeShip.FilmLookStrength", 50), 0, 100) / 100.0f;

        p[4] = 0.6f * amount;
        p[5] = 1.0f + 0.14f * amount;
        p[7] = 1.5f * amount;
    }
    if (corners) {
        p[6] = 0.24f;
    }
    if (blur) {
        // In a conversation and while Link is locked on to something, the two of them are sharp and what lies
        // well behind or before them is blurred, as a camera focused on them would have it.
        Player* player = GET_PLAYER(play);
        Actor* subject = player->focusActor;

        if (subject != NULL && subject != &player->actor) {
            f32 toLink = Math_Vec3f_DistXYZ(&play->view.eye, &player->actor.focus.pos);
            f32 toSubject = Math_Vec3f_DistXYZ(&play->view.eye, &subject->focus.pos);
            f32 at = (toLink + toSubject) * 0.5f;
            f32 wide = fabsf(toSubject - toLink) * 0.5f + 70.0f;

            if (sMergeShipFocus <= 0.0f) {
                sMergeShipFocusAt = at;
                sMergeShipFocusWide = wide;
            } else {
                Math_ApproachF(&sMergeShipFocusAt, at, 0.3f, 60.0f);
                Math_ApproachF(&sMergeShipFocusWide, wide, 0.3f, 40.0f);
            }
            Math_ApproachF(&sMergeShipFocus, 1.0f, 0.3f, 0.12f);
        } else {
            Math_ApproachF(&sMergeShipFocus, 0.0f, 0.3f, 0.12f);
        }
        p[8] = sMergeShipFocus * CLAMP(CVarGetInteger("gMergeShip.FocusBlurStrength", 70), 20, 100) / 100.0f;
        p[9] = sMergeShipFocusAt;
        p[10] = sMergeShipFocusWide;
        p[11] = 260.0f + sMergeShipFocusAt * 0.6f;
    } else {
        sMergeShipFocus = 0.0f;
    }
    slot->words.w1 = (uintptr_t)sMergeShipPost;

    OPEN_DISPS(gfxCtx);
    gfx = POLY_XLU_DISP;
    gfx->words.w0 = (0x4Fu << 24) | (13u << 16); // G_REGS_EX 13: the camera in effect is the world's
    gfx->words.w1 = 0;
    gfx++;
    POLY_XLU_DISP = gfx;
    CLOSE_DISPS(gfxCtx);
}

void Play_DrawOverlayElements(PlayState* play) {
    if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0)) {
        KaleidoScopeCall_Draw(play);
    }

    if (gSaveContext.gameMode == GAMEMODE_NORMAL && !MergeShipTest_Clean()) { // MergeShip2: test harness pictures: no HUD
        Interface_Draw(play);
    }

    Message_Draw(play);

    if (play->gameOverCtx.state != GAMEOVER_INACTIVE) {
        GameOver_FadeInLights(play);
    }
}

void Play_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Lights* sp228;
    Vec3f sp21C;
    Gfx* mergeShipPostSlot = NULL; // (MergeShip2: Play_MergeShipPost)

    // #region SOH [Port] Frame buffer effects for pause menu
    // Track render size when paused and that a copy was performed
    static u32 lastPauseWidth;
    static u32 lastPauseHeight;
    static bool lastAltAssets;
    static bool hasCapturedPauseBuffer;
    bool recapturePauseBuffer = false;

    // If the size has changed, alt assets toggled, or dropped frames leading to the buffer not being copied,
    // set the prerender state back to setup to copy a new frame.
    // This requires not rendering kaleido during this copy to avoid kaleido itself being copied too.
    if ((R_PAUSE_MENU_MODE == 2 || R_PAUSE_MENU_MODE == 3) &&
        (lastPauseWidth != OTRGetGameRenderWidth() || lastPauseHeight != OTRGetGameRenderHeight() ||
         lastAltAssets != ResourceMgr_IsAltAssetsEnabled() || !hasCapturedPauseBuffer)) {
        R_PAUSE_MENU_MODE = 1;
        recapturePauseBuffer = true;
    }
    // #endregion

    OPEN_DISPS(gfxCtx);

    gSegments[4] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[play->objectCtx.mainKeepIndex].segment);
    gSegments[5] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[play->objectCtx.subKeepIndex].segment);
    gSegments[2] = VIRTUAL_TO_PHYSICAL(play->sceneSegment);

    gSPSegment(POLY_OPA_DISP++, 0x00, NULL);
    gSPSegment(POLY_XLU_DISP++, 0x00, NULL);
    gSPSegment(OVERLAY_DISP++, 0x00, NULL);

    gSPSegment(POLY_OPA_DISP++, 0x04, play->objectCtx.status[play->objectCtx.mainKeepIndex].segment);
    gSPSegment(POLY_XLU_DISP++, 0x04, play->objectCtx.status[play->objectCtx.mainKeepIndex].segment);
    gSPSegment(OVERLAY_DISP++, 0x04, play->objectCtx.status[play->objectCtx.mainKeepIndex].segment);

    gSPSegment(POLY_OPA_DISP++, 0x05, play->objectCtx.status[play->objectCtx.subKeepIndex].segment);
    gSPSegment(POLY_XLU_DISP++, 0x05, play->objectCtx.status[play->objectCtx.subKeepIndex].segment);
    gSPSegment(OVERLAY_DISP++, 0x05, play->objectCtx.status[play->objectCtx.subKeepIndex].segment);

    gSPSegment(POLY_OPA_DISP++, 0x02, play->sceneSegment);
    gSPSegment(POLY_XLU_DISP++, 0x02, play->sceneSegment);
    gSPSegment(OVERLAY_DISP++, 0x02, play->sceneSegment);

    Gfx_SetupFrame(gfxCtx, 0, 0, 0);

    if ((HREG(80) != 10) || (HREG(82) != 0)) {
        GameInteractor_ExecuteOnPlayDrawBegin();
        // MergeShip2 (MergeShipPort3ds.h): the view distance of a scene that is real geometry in the 3DS rooms
        MergeShipPort3ds_SceneView(play);

        POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
        POLY_XLU_DISP = Play_SetFog(play, POLY_XLU_DISP);

        {
            // MergeShip2: the test harness's film camera is the view while a script keeps its pictures clean,
            // whatever setting the scene gives its cameras (a shop's fixed camera)
            Vec3f filmEye;
            Vec3f filmAt;
            if (MergeShipTest_FilmView(&filmEye, &filmAt)) {
                Vec3f filmUp = { 0.0f, 1.0f, 0.0f };
                func_800AA358(&play->view, &filmEye, &filmAt, &filmUp);
                play->view.fovy = 60.0f;
                View_InitDistortion(&play->view); // (no wobble: Jabu-Jabu's belly, a hot room)
            }
        }
        func_800AA460(&play->view, play->view.fovy, play->view.zNear, play->lightCtx.fogFar);
        func_800AAA50(&play->view, 15);

        // Flip the projections and invert culling for the OPA and XLU display buffers
        // These manage the world and effects when we are not drawing kaleido
        if (R_PAUSE_MENU_MODE <= 1 && CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0)) {
            gSPSetExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
            gSPSetExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
            gSPMatrix(POLY_OPA_DISP++, play->view.projectionFlippedPtr, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
            gSPMatrix(POLY_XLU_DISP++, play->view.projectionFlippedPtr, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
            gSPMatrix(POLY_OPA_DISP++, play->view.viewingPtr, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
            gSPMatrix(POLY_XLU_DISP++, play->view.viewingPtr, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
        }

        // The billboard matrix temporarily stores the viewing matrix
        Matrix_MtxToMtxF(&play->view.viewing, &play->billboardMtxF);
        Matrix_MtxToMtxF(&play->view.projection, &play->viewProjectionMtxF);
        Matrix_Mult(&play->viewProjectionMtxF, MTXMODE_NEW);
        // The billboard is still a viewing matrix at this stage
        Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
        Matrix_Get(&play->viewProjectionMtxF);
        play->billboardMtxF.mf[0][3] = play->billboardMtxF.mf[1][3] = play->billboardMtxF.mf[2][3] =
            play->billboardMtxF.mf[3][0] = play->billboardMtxF.mf[3][1] = play->billboardMtxF.mf[3][2] = 0.0f;
        // This transpose is where the viewing matrix is properly converted into a billboard matrix
        Matrix_Transpose(&play->billboardMtxF);
        play->billboardMtx =
            Matrix_MtxFToMtx(MATRIX_CHECKFLOATS(&play->billboardMtxF), Graph_Alloc(gfxCtx, sizeof(Mtx)));

        gSPSegment(POLY_OPA_DISP++, 0x01, play->billboardMtx);

        // MergeShip2, the finished picture's glow and look: a place for its command here, where the overlay's list
        // has nothing of the game's yet (its fades, the HUD and the text come after and stay as they are). It is
        // filled in where the world's drawing starts, and stays empty when the world is not drawn.
        mergeShipPostSlot = OVERLAY_DISP++;
        mergeShipPostSlot->words.w0 = (0x4Fu << 24) | (12u << 16); // G_REGS_EX 12
        mergeShipPostSlot->words.w1 = 0;

        if ((HREG(80) != 10) || (HREG(92) != 0)) {
            Gfx* gfxP;
            Gfx* sp1CC = POLY_OPA_DISP;

            gfxP = Graph_GfxPlusOne(sp1CC);
            gSPDisplayList(OVERLAY_DISP++, gfxP);
            gSPGrayscale(gfxP++, false);

            if ((play->transitionMode == TRANS_MODE_INSTANCE_RUNNING) ||
                (play->transitionMode == TRANS_MODE_INSTANCE_WAIT) ||
                (play->transitionCtx.transitionType >= TRANS_TYPE_MAX)) {
                View view;

                View_Init(&view, gfxCtx);
                view.flags = 2 | 8;

                SET_FULLSCREEN_VIEWPORT(&view);

                func_800AB9EC(&view, 15, &gfxP);
                play->transitionCtx.draw(&play->transitionCtx.data, &gfxP);
            }

            TransitionFade_Draw(&play->transitionFade, &gfxP);

            if (gVisMonoColor.a > 0) {
                gPlayVisMono.vis.primColor.rgba = gVisMonoColor.rgba;
                VisMono_Draw(&gPlayVisMono, &gfxP);
            }

            gSPEndDisplayList(gfxP++);
            Graph_BranchDlist(sp1CC, gfxP);
            POLY_OPA_DISP = gfxP;
        }

        if (gTrnsnUnkState == 3) {
            Gfx* sp88 = POLY_OPA_DISP;

            TransitionUnk_Draw(&sTrnsnUnk, &sp88);
            POLY_OPA_DISP = sp88;
            goto Play_Draw_DrawOverlayElements;
        }

        PreRender_SetValues(&play->pauseBgPreRender, SCREEN_WIDTH, SCREEN_HEIGHT, gfxCtx->curFrameBuffer, gZBuffer);

        if (R_PAUSE_MENU_MODE == 2) {
            // Wait for the previous frame's display list to be processed,
            // so that `pauseBgPreRender.fbufSave` and `pauseBgPreRender.cvgSave` are filled with the appropriate
            // content and can be used by `PreRender_ApplyFilters` below.
            MsgEvent_SendNullTask();

            PreRender_Calc(&play->pauseBgPreRender);

            R_PAUSE_MENU_MODE = 3;
        } else if (R_PAUSE_MENU_MODE >= 4) {
            R_PAUSE_MENU_MODE = 0;
        }

        if (R_PAUSE_MENU_MODE == 3) {
            Gfx* gfxP = POLY_OPA_DISP;

            // SOH [Port] Draw game framebuffer using our custom handling
            // func_800C24BC(&play->pauseBgPreRender, &gfxP);
            FB_DrawFromFramebuffer(&gfxP, gPauseFrameBuffer, 255);
            POLY_OPA_DISP = gfxP;

            goto Play_Draw_DrawOverlayElements;
        }

        // MergeShip2 real-time shadows: the world is drawn from here on
        Play_MergeShipShadows(play);
        Play_MergeShipPost(play, mergeShipPostSlot);

        if ((HREG(80) != 10) || (HREG(83) != 0)) {
            if (play->skyboxId && (play->skyboxId != SKYBOX_UNSET_1D) && !play->envCtx.skyboxDisabled) {
                if ((play->skyboxId == SKYBOX_NORMAL_SKY) || (play->skyboxId == SKYBOX_CUTSCENE_MAP)) {
                    Environment_UpdateSkybox(play, play->skyboxId, &play->envCtx, &play->skyboxCtx);
                    SkyboxDraw_Draw(&play->skyboxCtx, gfxCtx, play->skyboxId, play->envCtx.skyboxBlend,
                                    play->view.eye.x, play->view.eye.y, play->view.eye.z);
                } else if (play->skyboxCtx.unk_140 == 0) {
                    SkyboxDraw_Draw(&play->skyboxCtx, gfxCtx, play->skyboxId, 0, play->view.eye.x, play->view.eye.y,
                                    play->view.eye.z);
                }
            }
        }

        if ((HREG(80) != 10) || (HREG(90) & 2)) {
            if (!play->envCtx.sunMoonDisabled) {
                Environment_DrawSunAndMoon(play);
            }
        }

        if ((HREG(80) != 10) || (HREG(90) & 1)) {
            Environment_DrawSkyboxFilters(play);
        }

        if ((HREG(80) != 10) || (HREG(90) & 4)) {
            Environment_UpdateLightningStrike(play);
            Environment_DrawLightning(play, 0);
        }

        if ((HREG(80) != 10) || (HREG(90) & 8)) {
            sp228 = LightContext_NewLights(&play->lightCtx, gfxCtx);
            Lights_BindAll(sp228, play->lightCtx.listHead, NULL);
            Lights_Draw(sp228, gfxCtx);
        }

        if ((HREG(80) != 10) || (HREG(84) != 0)) {
            if (VREG(94) == 0) {
                s32 roomDrawFlags;

                if (HREG(80) != 10) {
                    roomDrawFlags = 3;
                } else {
                    roomDrawFlags = HREG(84);
                }
                Scene_Draw(play);
                Room_Draw(play, &play->roomCtx.curRoom, roomDrawFlags & 3);
                Room_Draw(play, &play->roomCtx.prevRoom, roomDrawFlags & 3);
                MergeShipPort3ds_DrawGrass(play); // (MergeShip2: more grass, on the floors that are grass)
            }
        }

        if ((HREG(80) != 10) || (HREG(83) != 0)) {
            // MergeShip2 (MergeShipPort3ds.h): the pre-rendered panorama of an interior would cover a room drawn
            // from the Ocarina of Time 3D pack, which is real geometry.
            if ((play->skyboxCtx.unk_140 != 0) && (GET_ACTIVE_CAM(play)->setting != CAM_SET_PREREND_FIXED) &&
                !MergeShipPort3ds_RoomDrawn()) {
                Vec3f quakeOffset;

                Camera_GetSkyboxOffset(&quakeOffset, GET_ACTIVE_CAM(play));
                SkyboxDraw_Draw(&play->skyboxCtx, gfxCtx, play->skyboxId, 0, play->view.eye.x + quakeOffset.x,
                                play->view.eye.y + quakeOffset.y, play->view.eye.z + quakeOffset.z);
            }
        }

        if (play->envCtx.unk_EE[1] != 0) {
            Environment_DrawRain(play, &play->view, gfxCtx);
        }

        if ((HREG(80) != 10) || (HREG(84) != 0)) {
            Environment_FillScreen(gfxCtx, 0, 0, 0, play->bgCoverAlpha, FILL_SCREEN_OPA);
        }

        if ((HREG(80) != 10) || (HREG(85) != 0)) {
            Actor_DrawAll(play, &play->actorCtx);
        }

        if ((HREG(80) != 10) || (HREG(86) != 0)) {
            if (!play->envCtx.sunMoonDisabled) {
                sp21C.x = play->view.eye.x + play->envCtx.sunPos.x;
                sp21C.y = play->view.eye.y + play->envCtx.sunPos.y;
                sp21C.z = play->view.eye.z + play->envCtx.sunPos.z;
                Environment_DrawSunLensFlare(play, &play->envCtx, &play->view, gfxCtx, sp21C, 0);
            }
            Environment_DrawCustomLensFlare(play);
        }

        if ((HREG(80) != 10) || (HREG(87) != 0)) {
            if (MREG(64) != 0) {
                Environment_FillScreen(gfxCtx, MREG(65), MREG(66), MREG(67), MREG(68),
                                       FILL_SCREEN_OPA | FILL_SCREEN_XLU);
            }

            switch (play->envCtx.fillScreen) {
                case 1:
                    Environment_FillScreen(gfxCtx, play->envCtx.screenFillColor[0], play->envCtx.screenFillColor[1],
                                           play->envCtx.screenFillColor[2], play->envCtx.screenFillColor[3],
                                           FILL_SCREEN_OPA | FILL_SCREEN_XLU);
                    break;
                default:
                    break;
            }
        }

        if ((HREG(80) != 10) || (HREG(88) != 0)) {
            if (play->envCtx.sandstormState != SANDSTORM_OFF) {
                // MergeShip2: the test harness's pictures: no sandstorm over them. The storm still runs (entering a
                // desert scene waits for it to settle): what it drew is taken back.
                Gfx* sandOpa = POLY_OPA_DISP;
                Gfx* sandXlu = POLY_XLU_DISP;
                Gfx* sandOverlay = OVERLAY_DISP;
                Environment_DrawSandstorm(play, play->envCtx.sandstormState);
                if (MergeShipTest_Clean()) {
                    POLY_OPA_DISP = sandOpa;
                    POLY_XLU_DISP = sandXlu;
                    OVERLAY_DISP = sandOverlay;
                }
            }
        }

        if ((HREG(80) != 10) || (HREG(93) != 0)) {
            DebugDisplay_DrawObjects(play);
        }

        // MergeShip2: MM Play_Draw draws its motion blur here.
        MergeShip_MotionBlur_Draw(play);

        if ((R_PAUSE_MENU_MODE == 1) || (gTrnsnUnkState == 1)) {
            Gfx* gfxP = OVERLAY_DISP;

            // Copy the frame buffer contents at this point in the display list to the zbuffer
            // The zbuffer must then stay untouched until unpausing
            play->pauseBgPreRender.fbuf = gfxCtx->curFrameBuffer;
            play->pauseBgPreRender.fbufSave = (u16*)gZBuffer;
            // SOH [Port] Use our custom copy method instead of the prerender system
            // func_800C1F20(&play->pauseBgPreRender, &gfxP);
            if (R_PAUSE_MENU_MODE == 1) {
                play->pauseBgPreRender.cvgSave = (u8*)gfxCtx->curFrameBuffer;
                // func_800C20B4(&play->pauseBgPreRender, &gfxP);
                R_PAUSE_MENU_MODE = 2;

                // #region SOH [Port] Custom handling for pause prerender background capture
                lastPauseWidth = OTRGetGameRenderWidth();
                lastPauseHeight = OTRGetGameRenderHeight();
                lastAltAssets = ResourceMgr_IsAltAssetsEnabled();
                hasCapturedPauseBuffer = false;

                FB_CopyToFramebuffer(&gfxP, 0, gPauseFrameBuffer, false, &hasCapturedPauseBuffer);

                // Set the state back to ready after the recapture is done
                if (recapturePauseBuffer) {
                    R_PAUSE_MENU_MODE = 3;
                }
                // #endregion
            } else {
                gTrnsnUnkState = 2;
            }
            OVERLAY_DISP = gfxP;
            play->unk_121C7 = 2;
            SREG(33) |= 1;

            // SOH [Port] Continue to render the post world for pausing to avoid flashing the HUD
            if (gTrnsnUnkState == 2) {
                goto Play_Draw_skip;
            }
        }

        // Draw Enhancements that need to be placed in the world. This happens before the PostWorldDraw
        // so that they aren't drawn when the pause menu is up (e.g. collision viewer, actor name tags)
        GameInteractor_ExecuteOnPlayDrawEnd();

    Play_Draw_DrawOverlayElements:
        if ((HREG(80) != 10) || (HREG(89) != 0)) {
            Play_DrawOverlayElements(play);
        }

        // Reset the inverted culling
        if (CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0)) {
            gSPClearExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
            gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
        }
    }

Play_Draw_skip:

    if (play->view.unk_124 != 0) {
        Camera_Update(GET_ACTIVE_CAM(play));
        func_800AB944(&play->view);
        play->view.unk_124 = 0;
        if (play->skyboxId && (play->skyboxId != SKYBOX_UNSET_1D) && !play->envCtx.skyboxDisabled) {
            SkyboxDraw_UpdateMatrix(&play->skyboxCtx, play->view.eye.x, play->view.eye.y, play->view.eye.z);
        }
    }

    Camera_Finish(GET_ACTIVE_CAM(play));

    CLOSE_DISPS(gfxCtx);

    Interface_DrawTotalGameplayTimer(play);
}

time_t Play_GetRealTime() {
    time_t t1, t2;
    struct tm* tms;
    time(&t1);
    tms = localtime(&t1);
    tms->tm_hour = 0;
    tms->tm_min = 0;
    tms->tm_sec = 0;
    t2 = mktime(tms);
    return t1 - t2;
}

void Play_Main(GameState* thisx) {
    PlayState* play = (PlayState*)thisx;

    if (play->envCtx.unk_EE[2] == 0 && CVarGetInteger(CVAR_GENERAL("LetItSnow"), 0)) {
        play->envCtx.unk_EE[3] = 64;
        Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_OBJECT_KANKYO, 0, 0, 0, 0, 0, 0, 3);
    }

    D_8012D1F8 = &play->state.input[0];

    DebugDisplay_Init();

    PLAY_LOG(4556);

    if ((HREG(80) == 10) && (HREG(94) != 10)) {
        HREG(81) = 1;
        HREG(82) = 1;
        HREG(83) = 1;
        HREG(84) = 3;
        HREG(85) = 1;
        HREG(86) = 1;
        HREG(87) = 1;
        HREG(88) = 1;
        HREG(89) = 1;
        HREG(90) = 15;
        HREG(91) = 1;
        HREG(92) = 1;
        HREG(93) = 1;
        HREG(94) = 10;
    }

    if ((HREG(80) != 10) || (HREG(81) != 0)) {
        Play_Update(play);
    }

    PLAY_LOG(4583);

    FrameInterpolation_StartRecord();
    Play_Draw(play);
    FrameInterpolation_StopRecord();

    // MergeShip2: MM runs CutsceneManager_Update after Play_Draw.
    MergeShip_TransformCam_EndOfFrame(play);

    PLAY_LOG(4587);

    if (CVarGetInteger(CVAR_CHEAT("TimeSync"), 0)) {
        const int maxRealDaySeconds = 86400;
        const int maxInGameDayTicks = 65536;

        int secs = (int)Play_GetRealTime();
        float percent = (float)secs / (float)maxRealDaySeconds;

        int newIngameTime = maxInGameDayTicks * percent;

        gSaveContext.dayTime = newIngameTime;
    }
}

u8 PlayerGrounded(Player* player) {
    return player->actor.bgCheckFlags & 1;
}

// original name: "Game_play_demo_mode_check"
s32 Play_InCsMode(PlayState* play) {
    return (play->csCtx.state != CS_STATE_IDLE) || Player_InCsMode(play);
}

f32 func_800BFCB8(PlayState* play, MtxF* mf, Vec3f* pos) {
    CollisionPoly poly;
    f32 temp1;
    f32 temp2;
    f32 temp3;
    f32 floorY = BgCheck_AnyRaycastFloor1(&play->colCtx, &poly, pos);

    if (floorY > BGCHECK_Y_MIN) {
        f32 nx = COLPOLY_GET_NORMAL(poly.normal.x);
        f32 ny = COLPOLY_GET_NORMAL(poly.normal.y);
        f32 nz = COLPOLY_GET_NORMAL(poly.normal.z);
        s32 pad[5];

        temp1 = sqrtf(1.0f - SQ(nx));

        if (temp1 != 0.0f) {
            temp2 = ny * temp1;
            temp3 = -nz * temp1;
        } else {
            temp3 = 0.0f;
            temp2 = 0.0f;
        }

        mf->xx = temp1;
        mf->yx = -nx * temp2;
        mf->zx = nx * temp3;
        mf->xy = nx;
        mf->yy = ny;
        mf->zy = nz;
        mf->yz = temp3;
        mf->zz = temp2;
        mf->wx = 0.0f;
        mf->wy = 0.0f;
        mf->xz = 0.0f;
        mf->wz = 0.0f;
        mf->xw = pos->x;
        mf->yw = floorY;
        mf->zw = pos->z;
        mf->ww = 1.0f;
    } else {
        mf->xy = 0.0f;
        mf->zx = 0.0f;
        mf->yx = 0.0f;
        mf->xx = 0.0f;
        mf->wz = 0.0f;
        mf->xz = 0.0f;
        mf->wy = 0.0f;
        mf->wx = 0.0f;
        mf->zz = 0.0f;
        mf->yz = 0.0f;
        mf->zy = 0.0f;
        mf->yy = 1.0f;
        mf->xw = pos->x;
        mf->yw = pos->y;
        mf->zw = pos->z;
        mf->ww = 1.0f;
    }

    return floorY;
}

void* Play_LoadFile(PlayState* play, RomFile* file) {
    size_t size;
    void* allocp;

    size = file->vromEnd - file->vromStart;
    allocp = GAMESTATE_ALLOC_MC(&play->state, size);
    DmaMgr_SendRequest1(allocp, file->vromStart, size, __FILE__, __LINE__);

    return allocp;
}

void Play_InitEnvironment(PlayState* play, s16 skyboxId) {
    Skybox_Init(&play->state, &play->skyboxCtx, skyboxId);
    Environment_Init(play, &play->envCtx, 0);
}

void Play_InitScene(PlayState* play, s32 spawn) {
    play->curSpawn = spawn;

    play->linkActorEntry = NULL;
    play->unk_11DFC = NULL;
    play->setupEntranceList = NULL;
    play->setupExitList = NULL;
    play->cUpElfMsgs = NULL;
    play->setupPathList = NULL;

    play->numSetupActors = 0;

    Object_InitBank(play, &play->objectCtx);
    LightContext_Init(play, &play->lightCtx);
    TransitionActor_InitContext(&play->state, &play->transiActorCtx);
    func_80096FD4(play, &play->roomCtx.curRoom);
    YREG(15) = 0;
    gSaveContext.worldMapArea = 0;
    Scene_ExecuteCommands(play, play->sceneSegment);
    Play_InitEnvironment(play, play->skyboxId);
}

void Play_SpawnScene(PlayState* play, s32 sceneId, s32 spawn) {
    uint8_t mqMode = CVarGetInteger(CVAR_GENERAL("BetterDebugWarpScreenMQMode"), WARP_MODE_OVERRIDE_OFF);
    int16_t mqModeScene = CVarGetInteger(CVAR_GENERAL("BetterDebugWarpScreenMQModeScene"), -1);
    if (mqMode != WARP_MODE_OVERRIDE_OFF && sceneId != mqModeScene) {
        CVarClear(CVAR_GENERAL("BetterDebugWarpScreenMQMode"));
        CVarClear(CVAR_GENERAL("BetterDebugWarpScreenMQModeScene"));
    }

    OTRPlay_SpawnScene(play, sceneId, spawn);
}

void func_800C016C(PlayState* play, Vec3f* src, Vec3f* dest) {
    f32 w;

    Matrix_Mult(&play->viewProjectionMtxF, MTXMODE_NEW);
    Matrix_MultVec3f(src, dest);

    w = play->viewProjectionMtxF.ww + (play->viewProjectionMtxF.wx * src->x + play->viewProjectionMtxF.wy * src->y +
                                       play->viewProjectionMtxF.wz * src->z);

    dest->x = (SCREEN_WIDTH / 2) + ((dest->x / w) * (SCREEN_WIDTH / 2));
    dest->y = (SCREEN_HEIGHT / 2) - ((dest->y / w) * (SCREEN_HEIGHT / 2));
}

s16 Play_CreateSubCamera(PlayState* play) {
    s16 i;

    for (i = CAM_ID_SUB_FIRST; i < NUM_CAMS; i++) {
        if (play->cameraPtrs[i] == NULL) {
            break;
        }
    }

    if (i == NUM_CAMS) {
        osSyncPrintf(VT_COL(RED, WHITE) "camera control: error: fulled sub camera system area\n" VT_RST);
        return SUBCAM_NONE;
    }

    osSyncPrintf("camera control: " VT_BGCOL(CYAN) " " VT_COL(WHITE, BLUE) " create new sub camera [%d] " VT_BGCOL(
                     CYAN) " " VT_RST "\n",
                 i);

    play->cameraPtrs[i] = &play->subCameras[i - CAM_ID_SUB_FIRST];
    Camera_Init(play->cameraPtrs[i], &play->view, &play->colCtx, play);
    play->cameraPtrs[i]->thisIdx = i;

    return i;
}

s16 Play_GetActiveCamId(PlayState* play) {
    return play->activeCamera;
}

s16 Play_ChangeCameraStatus(PlayState* play, s16 camId, s16 status) {
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;

    if (status == CAM_STAT_ACTIVE) {
        play->activeCamera = camIdx;
    }

    return Camera_ChangeStatus(play->cameraPtrs[camIdx], status);
}

void Play_ClearCamera(PlayState* play, s16 camId) {
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;

    if (camIdx == CAM_ID_MAIN) {
        osSyncPrintf(VT_COL(RED, WHITE) "camera control: error: never clear camera !!\n" VT_RST);
    }

    if (play->cameraPtrs[camIdx] != NULL) {
        Camera_ChangeStatus(play->cameraPtrs[camIdx], CAM_STAT_UNK100);
        play->cameraPtrs[camIdx] = NULL;
        osSyncPrintf("camera control: " VT_BGCOL(CYAN) " " VT_COL(WHITE, BLUE) " clear sub camera [%d] " VT_BGCOL(
                         CYAN) " " VT_RST "\n",
                     camIdx);
    } else {
        osSyncPrintf(VT_COL(RED, WHITE) "camera control: error: camera No.%d already cleared\n" VT_RST, camIdx);
    }
}

void Play_ClearAllSubCameras(PlayState* play) {
    s16 i;

    for (i = CAM_ID_SUB_FIRST; i < NUM_CAMS; i++) {
        if (play->cameraPtrs[i] != NULL) {
            Play_ClearCamera(play, i);
        }
    }

    play->activeCamera = CAM_ID_MAIN;
}

Camera* Play_GetCamera(PlayState* play, s16 camId) {
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;

    return play->cameraPtrs[camIdx];
}

s32 Play_CameraSetAtEye(PlayState* play, s16 camId, Vec3f* at, Vec3f* eye) {
    s32 ret = 0;
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;
    Camera* camera = play->cameraPtrs[camIdx];
    Player* player;

    ret |= Camera_SetParam(camera, 1, at);
    ret <<= 1;
    ret |= Camera_SetParam(camera, 2, eye);

    camera->dist = Math3D_Vec3f_DistXYZ(at, eye);

    player = camera->player;
    if (player != NULL) {
        camera->posOffset.x = at->x - player->actor.world.pos.x;
        camera->posOffset.y = at->y - player->actor.world.pos.y;
        camera->posOffset.z = at->z - player->actor.world.pos.z;
    } else {
        camera->posOffset.x = camera->posOffset.y = camera->posOffset.z = 0.0f;
    }

    camera->atLERPStepScale = 0.01f;

    return ret;
}

s32 Play_CameraSetAtEyeUp(PlayState* play, s16 camId, Vec3f* at, Vec3f* eye, Vec3f* up) {
    s32 ret = 0;
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;
    Camera* camera = play->cameraPtrs[camIdx];
    Player* player;

    ret |= Camera_SetParam(camera, 1, at);
    ret <<= 1;
    ret |= Camera_SetParam(camera, 2, eye);
    ret <<= 1;
    ret |= Camera_SetParam(camera, 4, up);

    camera->dist = Math3D_Vec3f_DistXYZ(at, eye);

    player = camera->player;
    if (player != NULL) {
        camera->posOffset.x = at->x - player->actor.world.pos.x;
        camera->posOffset.y = at->y - player->actor.world.pos.y;
        camera->posOffset.z = at->z - player->actor.world.pos.z;
    } else {
        camera->posOffset.x = camera->posOffset.y = camera->posOffset.z = 0.0f;
    }

    camera->atLERPStepScale = 0.01f;

    return ret;
}

s32 Play_CameraSetFov(PlayState* play, s16 camId, f32 fov) {
    s32 ret = Camera_SetParam(play->cameraPtrs[camId], 0x20, &fov) & 1;

    return ret;
}

s32 Play_SetCameraRoll(PlayState* play, s16 camId, s16 roll) {
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;
    Camera* camera = play->cameraPtrs[camIdx];

    camera->roll = roll;

    return 1;
}

void Play_CopyCamera(PlayState* play, s16 camId1, s16 camId2) {
    s16 camIdx2 = (camId2 == SUBCAM_ACTIVE) ? play->activeCamera : camId2;
    s16 camIdx1 = (camId1 == SUBCAM_ACTIVE) ? play->activeCamera : camId1;

    Camera_Copy(play->cameraPtrs[camIdx1], play->cameraPtrs[camIdx2]);
}

s32 func_800C0808(PlayState* play, s16 camId, Player* player, s16 setting) {
    Camera* camera;
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;

    camera = play->cameraPtrs[camIdx];
    Camera_InitPlayerSettings(camera, player);
    return Camera_RequestSetting(camera, setting);
}

s32 Play_CameraChangeSetting(PlayState* play, s16 camId, s16 setting) {
    return Camera_RequestSetting(Play_GetCamera(play, camId), setting);
}

void func_800C08AC(PlayState* play, s16 camId, s16 arg2) {
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;
    s16 i;

    Play_ClearCamera(play, camIdx);

    for (i = CAM_ID_SUB_FIRST; i < NUM_CAMS; i++) {
        if (play->cameraPtrs[i] != NULL) {
            osSyncPrintf(
                VT_COL(RED, WHITE) "camera control: error: return to main, other camera left. %d cleared!!\n" VT_RST,
                i);
            Play_ClearCamera(play, i);
        }
    }

    if (arg2 <= 0) {
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        play->cameraPtrs[CAM_ID_MAIN]->childCamIdx = play->cameraPtrs[CAM_ID_MAIN]->parentCamIdx = SUBCAM_FREE;
    } else {
        OnePointCutscene_Init(play, 1020, arg2, NULL, CAM_ID_MAIN);
    }
}

s16 Play_CameraGetUID(PlayState* play, s16 camId) {
    Camera* camera = play->cameraPtrs[camId];

    if (camera != NULL) {
        return camera->uid;
    } else {
        return -1;
    }
}

s16 func_800C09D8(PlayState* play, s16 camId, s16 arg2) {
    Camera* camera = play->cameraPtrs[camId];

    if (camera != NULL) {
        return 0;
    } else if (camera->uid != arg2) {
        return 0;
    } else if (camera->status != CAM_STAT_ACTIVE) {
        return 2;
    } else {
        return 1;
    }
}

void Play_SaveSceneFlags(PlayState* play) {
    SavedSceneFlags* savedSceneFlags = &gSaveContext.sceneFlags[play->sceneNum];

    savedSceneFlags->chest = play->actorCtx.flags.chest;
    savedSceneFlags->swch = play->actorCtx.flags.swch;
    savedSceneFlags->clear = play->actorCtx.flags.clear;
    savedSceneFlags->collect = play->actorCtx.flags.collect;
}

void Play_SetRespawnData(PlayState* play, s32 respawnMode, s16 entranceIndex, s32 roomIndex, s32 playerParams,
                         Vec3f* pos, s16 yaw) {
    RespawnData* respawnData = &gSaveContext.respawn[respawnMode];

    respawnData->entranceIndex = entranceIndex;
    respawnData->roomIndex = roomIndex;
    respawnData->pos = *pos;
    respawnData->yaw = yaw;
    respawnData->playerParams = playerParams;
    respawnData->tempSwchFlags = play->actorCtx.flags.tempSwch;
    respawnData->tempCollectFlags = play->actorCtx.flags.tempCollect;
}

void Play_SetupRespawnPoint(PlayState* play, s32 respawnMode, s32 playerParams) {
    Player* player = GET_PLAYER(play);
    s32 entranceIndex;
    s8 roomIndex;

    if ((play->sceneNum != SCENE_FAIRYS_FOUNTAIN) && (play->sceneNum != SCENE_GROTTOS)) {
        roomIndex = play->roomCtx.curRoom.num;
        entranceIndex = gSaveContext.entranceIndex;
        Play_SetRespawnData(play, respawnMode, entranceIndex, roomIndex, playerParams, &player->actor.world.pos,
                            player->actor.shape.rot.y);
    }
}

void Play_TriggerVoidOut(PlayState* play) {
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempSwchFlags = play->actorCtx.flags.tempSwch;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempCollectFlags = play->actorCtx.flags.tempCollect;
    gSaveContext.respawnFlag = 1;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->nextEntranceIndex = gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
}

void Play_LoadToLastEntrance(PlayState* play) {
    gSaveContext.respawnFlag = -1;
    play->transitionTrigger = TRANS_TRIGGER_START;

    if ((play->sceneNum == SCENE_GANONS_TOWER_COLLAPSE_INTERIOR) ||
        (play->sceneNum == SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR) ||
        (play->sceneNum == SCENE_INSIDE_GANONS_CASTLE_COLLAPSE) || (play->sceneNum == SCENE_GANON_BOSS)) {
        play->nextEntranceIndex = ENTR_GANONS_TOWER_COLLAPSE_EXTERIOR_0;
        Item_Give(play, ITEM_SWORD_MASTER);
    } else if ((gSaveContext.entranceIndex == ENTR_HYRULE_FIELD_11) ||
               (gSaveContext.entranceIndex == ENTR_HYRULE_FIELD_12) ||
               (gSaveContext.entranceIndex == ENTR_HYRULE_FIELD_13) ||
               (gSaveContext.entranceIndex == ENTR_HYRULE_FIELD_15)) {
        play->nextEntranceIndex = ENTR_HYRULE_FIELD_CENTER_EXIT;
    } else {
        play->nextEntranceIndex = gSaveContext.entranceIndex;
    }

    play->transitionType = TRANS_TYPE_FADE_BLACK;
}

void Play_TriggerRespawn(PlayState* play) {
    Play_SetupRespawnPoint(play, RESPAWN_MODE_DOWN, 0xDFF);
    Play_LoadToLastEntrance(play);
}

s32 Play_CamIsNotFixed(PlayState* play) {
    return (play->roomCtx.curRoom.meshHeader->base.type != 1) && (YREG(15) != 0x20) && (YREG(15) != 0x30) &&
           (YREG(15) != 0x40) && (play->sceneNum != SCENE_CASTLE_COURTYARD_GUARDS_DAY);
}

s32 FrameAdvance_IsEnabled(PlayState* play) {
    return !!play->frameAdvCtx.enabled;
}

s32 func_800C0D34(PlayState* play, Actor* actor, s16* yaw) {
    TransitionActorEntry* transitionActor;
    s32 frontRoom;

    if (actor->category != ACTORCAT_DOOR) {
        return 0;
    }

    transitionActor = &play->transiActorCtx.list[(u16)actor->params >> 10];
    frontRoom = transitionActor->sides[0].room;

    if (frontRoom == transitionActor->sides[1].room) {
        return 0;
    }

    if (frontRoom == actor->room) {
        *yaw = actor->shape.rot.y;
    } else {
        *yaw = actor->shape.rot.y + 0x8000;
    }

    return 1;
}

s32 func_800C0DB4(PlayState* play, Vec3f* pos) {
    WaterBox* waterBox;
    CollisionPoly* poly;
    Vec3f waterSurfacePos;
    s32 bgId;

    waterSurfacePos = *pos;

    if (WaterBox_GetSurface1(play, &play->colCtx, waterSurfacePos.x, waterSurfacePos.z, &waterSurfacePos.y,
                             &waterBox) == true &&
        pos->y < waterSurfacePos.y &&
        BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &waterSurfacePos) != BGCHECK_Y_MIN) {
        return true;
    } else {
        return false;
    }
}

void Play_PerformSave(PlayState* play) {
    if (play != NULL && gSaveContext.fileNum != 0xFF) {
        Play_SaveSceneFlags(play);
        gSaveContext.savedSceneNum = play->sceneNum;

        // Track values from temp B
        uint8_t prevB = gSaveContext.equips.buttonItems[0];
        uint8_t prevStatus = gSaveContext.buttonStatus[0];

        // Replicate the B button restore from minigames/epona that kaleido does
        if (gSaveContext.equips.buttonItems[0] == ITEM_SLINGSHOT || gSaveContext.equips.buttonItems[0] == ITEM_BOW ||
            gSaveContext.equips.buttonItems[0] == ITEM_BOMBCHU ||
            gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE ||
            (gSaveContext.equips.buttonItems[0] == ITEM_NONE && !Flags_GetInfTable(INFTABLE_SWORDLESS))) {

            gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];
            GameInteractor_Should(VB_TEMP_B_RESTORE_SWORDLESS, true);
        }

        Save_SaveFile();

        // Restore temp B values back
        gSaveContext.equips.buttonItems[0] = prevB;
        gSaveContext.buttonStatus[0] = prevStatus;
    }
}
