// MergeShip2 -- Majora's Mask's stray fairies in Hyrule: counts, giving, containers, the get-item model and the Masks
// page's panel (MergeShipStrayFairy.h). The fairy itself is src/overlays/actors/ovl_MM_En_Elforg; the SoH side (actor
// registration, hooks, messages) is MergeShipStrayFairy.cpp.

#include "global.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/MergeShip/MergeShipStrayFairy.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "soh/MergeShip/MergeShipMMMasks.h"
#include "soh/MergeShip/MergeShipTransformFx.h" // MergeShip_PlayMMSfx*, MM_NA_SE_*
#include "soh/MergeShip/MergeShipAnimatedMat.h"
#include "soh/MergeShip/MergeShipPause.h"
#include "src/overlays/actors/ovl_MM_En_Elforg/z_mm_en_elforg.h"
#include "src/overlays/actors/ovl_En_Box/z_en_box.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "mm/interface/parameter_static/parameter_static.h"
#include "mm/interface/icon_item_dungeon_static/icon_item_dungeon_static.h"

extern PlayState* gPlayState;

// z_player.c (not static; SoH's randomizer uses them the same way for its chest items).
extern s32 Player_SetupWaitForPutAway(PlayState* play, Player* player, AfterPutAwayFunc func);
extern void Player_SetupActionPreserveAnimMovement(PlayState* play, Player* player, PlayerActionFunc actionFunc,
                                                   s32 flags);
extern void func_8084DFAC(PlayState* play, Player* player);

s16 gMergeShipActorId_MM_En_Elforg = -1;

// ---- state ------------------------------------------------------------------------------------------------------

s32 MergeShip_StrayFairy_IsEnabled(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.StrayFairies"), 1) && MergeShip_IsMajorasMaskMounted();
}

static s32 MergeShip_StrayFairy_ValidArea(s32 area) {
    return (area >= MM_STRAY_FAIRY_AREA_CLOCK_TOWN) && (area < MM_STRAY_FAIRY_AREA_MAX);
}

s32 MergeShip_StrayFairy_GetCount(s32 area) {
    return MergeShip_StrayFairy_ValidArea(area) ? gMergeShipSave.strayFairies[area] : 0;
}

s32 MergeShip_StrayFairy_GetMax(s32 area) {
    return (area == MM_STRAY_FAIRY_AREA_CLOCK_TOWN) ? 1 : MM_STRAY_FAIRY_SCATTERED_TOTAL;
}

// ---- spawning ----------------------------------------------------------------------------------------------------

Actor* MergeShip_StrayFairy_SpawnEx(PlayState* play, Vec3f* pos, s32 area, s32 type, s32 flag, Actor* parent,
                                    u32 userData) {
    Actor* fairy;
    s16 params;

    if ((play == NULL) || (pos == NULL) || (gMergeShipActorId_MM_En_Elforg < 0) || !MergeShip_StrayFairy_IsEnabled()) {
        return NULL;
    }
    if (!MergeShip_StrayFairy_ValidArea(area)) {
        area = MM_STRAY_FAIRY_AREA_CLOCK_TOWN;
    }
    params = MM_STRAY_FAIRY_PARAMS(flag, area, type);
    if (parent != NULL) {
        fairy = Actor_SpawnAsChild(&play->actorCtx, parent, play, gMergeShipActorId_MM_En_Elforg, pos->x, pos->y,
                                   pos->z, parent->world.rot.x, parent->world.rot.y, parent->world.rot.z, params);
    } else {
        fairy = Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_MM_En_Elforg, pos->x, pos->y, pos->z, 0, 0, 0,
                            params);
    }
    // MMEnElforg_Init kills a fairy whose flag is already set.
    if ((fairy == NULL) || (fairy->update == NULL)) {
        return NULL;
    }
    ((MMEnElforg*)fairy)->userData = userData;
    return fairy;
}

Actor* MergeShip_StrayFairy_Spawn(PlayState* play, Vec3f* pos, s32 area, s32 flag) {
    return MergeShip_StrayFairy_SpawnEx(play, pos, area, MM_STRAY_FAIRY_TYPE_FREE_FLOATING, flag, NULL, 0);
}

Actor* MergeShip_StrayFairy_AttachToEnemy(PlayState* play, Actor* enemy, s32 area, s32 flag, u32 userData) {
    if (enemy == NULL) {
        return NULL;
    }
    // MM EnElforg_GetHoldingEnemy finds the enemy whose home is the fairy's (Actor_Spawn makes home the spawn point).
    return MergeShip_StrayFairy_SpawnEx(play, &enemy->home.pos, area, MM_STRAY_FAIRY_TYPE_ENEMY, flag, NULL,
                                        userData);
}

// ---- containers --------------------------------------------------------------------------------------------------

typedef struct MergeShipStrayFairyContainer {
    Actor* actor;
    s16 area;
    s16 flag;
    u32 userData;
    u8 updated; // its update has run: an Actor_Kill before that is a failed init, not a break
} MergeShipStrayFairyContainer;

#define MERGESHIP_STRAY_FAIRY_MAX_CONTAINERS 32
static MergeShipStrayFairyContainer sContainers[MERGESHIP_STRAY_FAIRY_MAX_CONTAINERS];

static MergeShipStrayFairyContainer* MergeShip_StrayFairy_FindContainer(Actor* actor) {
    s32 i;

    if (actor == NULL) {
        return NULL;
    }
    for (i = 0; i < MERGESHIP_STRAY_FAIRY_MAX_CONTAINERS; i++) {
        if (sContainers[i].actor == actor) {
            return &sContainers[i];
        }
    }
    return NULL;
}

static s32 MergeShip_StrayFairy_ChestFlag(Actor* chest) {
    return chest->params & 0x1F;
}

s32 MergeShip_StrayFairy_AttachToContainer(Actor* container, s32 area, s32 flag, u32 userData) {
    MergeShipStrayFairyContainer* entry;
    s32 i;

    if ((container == NULL) || (gPlayState == NULL) || !MergeShip_StrayFairy_IsEnabled()) {
        return false;
    }
    if (container->id == ACTOR_EN_BOX) {
        // MM EnBox_Init: only an unopened chest holds its fairy.
        if (Flags_GetTreasure(gPlayState, MergeShip_StrayFairy_ChestFlag(container))) {
            return false;
        }
    } else if ((flag != MERGESHIP_STRAY_FAIRY_FLAG_NONE) && Flags_GetCollectible(gPlayState, flag)) {
        // MM Item_CanDropBigFairy
        return false;
    }

    entry = MergeShip_StrayFairy_FindContainer(container);
    for (i = 0; (entry == NULL) && (i < MERGESHIP_STRAY_FAIRY_MAX_CONTAINERS); i++) {
        if (sContainers[i].actor == NULL) {
            entry = &sContainers[i];
            entry->updated = false;
        }
    }
    if (entry == NULL) {
        return false;
    }
    entry->actor = container;
    entry->area = MergeShip_StrayFairy_ValidArea(area) ? area : MM_STRAY_FAIRY_AREA_CLOCK_TOWN;
    entry->flag = flag & 0x7F;
    entry->userData = userData;
    // MM EnBox_Init / ObjKibako2_Init / ObjTsubo_Update / ObjComb_Update: the holder is never culled.
    container->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    return true;
}

void MergeShip_StrayFairy_DetachFromContainer(Actor* container) {
    MergeShipStrayFairyContainer* entry = MergeShip_StrayFairy_FindContainer(container);

    if (entry != NULL) {
        entry->actor = NULL;
    }
}

s32 MergeShip_StrayFairy_ContainerHoldsFairy(Actor* container) {
    return MergeShip_StrayFairy_IsEnabled() && (MergeShip_StrayFairy_FindContainer(container) != NULL);
}

void MergeShip_StrayFairy_OnActorUpdate(Actor* actor) {
    MergeShipStrayFairyContainer* entry = MergeShip_StrayFairy_FindContainer(actor);
    PlayState* play = gPlayState;

    if ((entry == NULL) || (play == NULL)) {
        return;
    }
    entry->updated = true;
    if (!MergeShip_StrayFairy_IsEnabled()) {
        return;
    }

    if (actor->id == ACTOR_EN_BOX) {
        EnBox* chest = (EnBox*)actor;

        // MM EnBox_Update: ACTORCTX_FLAG_3 while the chest's flag is clear.
        if (!Flags_GetTreasure(play, MergeShip_StrayFairy_ChestFlag(actor))) {
            gMergeShipStrayFairyNearby = true;
        }
        // MM EnBox_Open: when the lid's animation ends, the fairy flies out (Actor_SpawnAsChild, the chest's position
        // and rotation, STRAY_FAIRY_TYPE_CHEST saved in the chest's flag). unk_1F4 is set by the player opening it.
        if ((chest->unk_1F4 != 0) && (chest->skelanime.curFrame >= chest->skelanime.endFrame)) {
            s32 area = entry->area;
            u32 userData = entry->userData;

            entry->actor = NULL;
            MergeShip_StrayFairy_SpawnEx(play, &actor->world.pos, area, MM_STRAY_FAIRY_TYPE_CHEST,
                                         MergeShip_StrayFairy_ChestFlag(actor), actor, userData);
        }
        return;
    }

    // MM ObjTsubo_Update / ObjTaru_Update / ObjKibako2_Update / ObjComb_Update: ACTORCTX_FLAG_3 while it holds one.
    gMergeShipStrayFairyNearby = true;
}

void MergeShip_StrayFairy_OnActorKill(Actor* actor) {
    MergeShipStrayFairyContainer* entry = MergeShip_StrayFairy_FindContainer(actor);
    PlayState* play = gPlayState;
    Actor* fairy;
    Vec3f pos;

    if (entry == NULL) {
        return;
    }
    entry->actor = NULL;
    // A chest releases its fairy only by opening; a failed init, an unloaded room or a missing object is no break.
    if ((play == NULL) || !entry->updated || (actor->id == ACTOR_EN_BOX) || !MergeShip_StrayFairy_IsEnabled() ||
        !Object_IsLoaded(&play->objectCtx, actor->objBankIndex) ||
        ((actor->room >= 0) && (actor->room != play->roomCtx.curRoom.num) &&
         (actor->room != play->roomCtx.prevRoom.num))) {
        return;
    }

    // MM Item_DropCollectible(ITEM00_BIG_FAIRY): an En_Elforg 40 units up, STRAY_FAIRY_TYPE_COLLECTIBLE, and
    // NA_SE_EV_BUTTERFRY_TO_FAIRY if it was not yet collected.
    pos = actor->world.pos;
    pos.y += 40.0f;
    fairy = MergeShip_StrayFairy_SpawnEx(play, &pos, entry->area, MM_STRAY_FAIRY_TYPE_COLLECTIBLE, entry->flag, NULL,
                                         entry->userData);
    if (fairy != NULL) {
        // MM SoundSource_PlaySfxAtFixedWorldPos(play, spawnPos, 40, NA_SE_EV_BUTTERFRY_TO_FAIRY), from the fairy.
        MergeShip_PlayMMSfxActor(fairy, MM_NA_SE_EV_BUTTERFRY_TO_FAIRY);
    }
}

void MergeShip_StrayFairy_OnActorDestroy(Actor* actor) {
    MergeShip_StrayFairy_DetachFromContainer(actor);
}

// The rest of MM Player_Action_65 for GI_STRAY_FAIRY: once the kick has played, back to standing (func_80839E74).
static void MergeShip_StrayFairy_ChestKickAction(Player* player, PlayState* play) {
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        func_8084DFAC(play, player);
    }
}

static void MergeShip_StrayFairy_AfterChestPutAway(PlayState* play, Player* player) {
    Player_SetupActionPreserveAnimMovement(play, player, MergeShip_StrayFairy_ChestKickAction, 0);
    player->stateFlags1 |= PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_CUTSCENE;
}

void MergeShip_StrayFairy_KickOpenChest(PlayState* play) {
    if (play != NULL) {
        Player_SetupWaitForPutAway(play, GET_PLAYER(play), MergeShip_StrayFairy_AfterChestPutAway);
    }
}

// ---- giving ------------------------------------------------------------------------------------------------------

static s32 sGiveAreaPending = -1;
static s32 sLastGivenArea = MM_STRAY_FAIRY_AREA_WOODFALL;
static s32 sMessageArea = MM_STRAY_FAIRY_AREA_WOODFALL;
static s32 sMessageCount = 0;
static s32 sMessageFromActor = false;
static MergeShipStrayFairyCollectFunc sCollectOverride = NULL;

void MergeShip_StrayFairy_SetCollectOverride(MergeShipStrayFairyCollectFunc func) {
    sCollectOverride = func;
}

void MergeShip_StrayFairy_SetGiveArea(s32 area) {
    sGiveAreaPending = MergeShip_StrayFairy_ValidArea(area) ? area : -1;
}

// The area the next give counts in, without using it up.
static s32 MergeShip_StrayFairy_PeekGiveArea(void) {
    if (sGiveAreaPending >= 0) {
        return sGiveAreaPending;
    }
    if (gPlayState != NULL) {
        Player* player = GET_PLAYER(gPlayState);

        if ((player != NULL) && (player->getItemEntry.modIndex == MOD_NONE) &&
            MERGESHIP_GI_IS_MM_STRAY_FAIRY(player->getItemEntry.getItemId)) {
            return player->getItemEntry.getItemId - MERGESHIP_GI_MM_STRAY_FAIRY_BASE;
        }
    }
    return sLastGivenArea;
}

u8 MergeShip_StrayFairy_GiveItem(PlayState* play) {
    s32 area = MergeShip_StrayFairy_PeekGiveArea();

    (void)play;
    sGiveAreaPending = -1;
    // MM: gSaveContext.save.saveInfo.inventory.strayFairies[dungeonSceneSharedIndex]++ (EnElforg_FreeFloating), or
    // SET_WEEKEVENTREG(WEEKEVENTREG_08_80) for Clock Town's.
    if (area == MM_STRAY_FAIRY_AREA_CLOCK_TOWN) {
        gMergeShipSave.strayFairies[area] = 1;
    } else if (gMergeShipSave.strayFairies[area] < 0xFF) {
        gMergeShipSave.strayFairies[area]++;
    }
    sLastGivenArea = area;
    sMessageArea = area;
    sMessageCount = gMergeShipSave.strayFairies[area];

    // As Item_Give's Return_Item: the item-receive hooks run.
    GameInteractor_ExecuteOnItemReceiveHooks(MergeShip_StrayFairy_GetItemEntry(area));
    return ITEM_NONE;
}

void MergeShip_StrayFairy_Give(PlayState* play, s32 area) {
    MergeShip_StrayFairy_SetGiveArea(area);
    Item_Give(play, ITEM_MM_STRAY_FAIRY);
}

s32 MergeShip_StrayFairy_OnCollected(PlayState* play, Actor* fairy, s32 area, s32 type, u32 userData) {
    s32 count;

    if ((sCollectOverride != NULL) && sCollectOverride(play, fairy, area, type, userData)) {
        return false;
    }

    if ((type == MM_STRAY_FAIRY_TYPE_CLOCK_TOWN) || (area == MM_STRAY_FAIRY_AREA_CLOCK_TOWN)) {
        // MM: "Bring me back to North Clock Town!" (0x579); counted when the textbox closes.
        Message_StartTextbox(play, MERGESHIP_TEXT_STRAY_FAIRY_CLOCK_TOWN, NULL);
        return true;
    }

    MergeShip_StrayFairy_Give(play, area);
    count = MergeShip_StrayFairy_GetCount(area);
    sMessageFromActor = true;
    // MM: "You found a Stray Fairy!" (0x11), which Message_StartTextbox turns into 0xF3 on the fifteenth.
    Message_StartTextbox(play,
                         (count == MM_STRAY_FAIRY_SCATTERED_TOTAL) ? MERGESHIP_TEXT_STRAY_FAIRY_FOUND_ALL
                                                                   : MERGESHIP_TEXT_STRAY_FAIRY_FOUND,
                         NULL);
    if (count >= MM_STRAY_FAIRY_SCATTERED_TOTAL) {
        // MM Audio_PlayFanfare(NA_BGM_GET_ITEM): the same item-get fanfare as Ocarina of Time's NA_BGM_ITEM_GET.
        Audio_PlayFanfare(NA_BGM_ITEM_GET);
    }
    return false;
}

void MergeShip_StrayFairy_GetMessageArea(s32* area, s32* count) {
    if (sMessageFromActor) {
        sMessageFromActor = false;
        *area = sMessageArea;
        *count = sMessageCount;
        return;
    }
    // A get-item: Ocarina of Time opens its text before Item_Give counts the fairy (func_8084DFF4).
    *area = MergeShip_StrayFairy_PeekGiveArea();
    *count = MergeShip_StrayFairy_GetCount(*area) + 1;
    if ((*area == MM_STRAY_FAIRY_AREA_CLOCK_TOWN) && (*count > 1)) {
        *count = 1;
    }
}

void MergeShip_StrayFairy_GetIconColors(s32 area, Color_RGB8* prim, Color_RGB8* env) {
    // MM z_message.c sStrayFairyIconPrimColors / sStrayFairyIconEnvColors, by temple. MM has no Clock Town entry (its
    // fairy never shows this icon); it gets the Clock Town fairy's own sparkle colours (EnElforg_SpawnSparkles).
    static const Color_RGB8 sPrim[MM_STRAY_FAIRY_AREA_MAX] = {
        { 255, 235, 220 }, { 255, 110, 160 }, { 90, 255, 100 }, { 120, 255, 255 }, { 245, 245, 90 },
    };
    static const Color_RGB8 sEnv[MM_STRAY_FAIRY_AREA_MAX] = {
        { 255, 150, 0 }, { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 }, { 225, 170, 0 },
    };

    if (!MergeShip_StrayFairy_ValidArea(area)) {
        area = MM_STRAY_FAIRY_AREA_CLOCK_TOWN;
    }
    *prim = sPrim[area];
    *env = sEnv[area];
}

// MM Message_DrawItemIcon, ITEM_STRAY_FAIRIES: the glowing circle in the area's colours (drawn here at x, y, 32 x 24),
// then the area's fairy loaded for the caller's own 32 x 24 rectangle over it. MM has no Clock Town fairy icon: the
// Stone Tower's (yellow) over Clock Town's orange glow.
void MergeShip_StrayFairy_DrawTextIcon(Gfx** gfxP, s32 area, s32 x, s32 y, u8 alpha) {
    static const char* sFairyIcons[MM_STRAY_FAIRY_AREA_MAX] = {
        mm_gStrayFairyStoneTowerIconTex, mm_gStrayFairyWoodfallIconTex,   mm_gStrayFairySnowheadIconTex,
        mm_gStrayFairyGreatBayIconTex,   mm_gStrayFairyStoneTowerIconTex,
    };
    Gfx* gfx = *gfxP;
    Color_RGB8 prim;
    Color_RGB8 env;

    if (!MergeShip_StrayFairy_ValidArea(area)) {
        area = MM_STRAY_FAIRY_AREA_CLOCK_TOWN;
    }
    MergeShip_StrayFairy_GetIconColors(area, &prim, &env);
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(gfx++, 0, 0, prim.r, prim.g, prim.b, alpha);
    gDPSetEnvColor(gfx++, env.r, env.g, env.b, 0);
    gDPLoadTextureBlock_4b(gfx++, mm_gStrayFairyGlowingCircleIconTex, G_IM_FMT_I, 32, 24, 0,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);
    gSPTextureRectangle(gfx++, x << 2, y << 2, (x + 32) << 2, (y + 24) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    // The fairy: MM's G_CC_MODULATEIA_PRIM at white, as Message_DrawItemIcon's caller set it.
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
    gDPLoadTextureBlock(gfx++, sFairyIcons[area], G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 24, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    *gfxP = gfx;
}

// ---- the get-item model ------------------------------------------------------------------------------------------

static const char* sStrayFairyTexAnims[MM_STRAY_FAIRY_AREA_MAX] = {
    mm_gStrayFairyClockTownTexAnim, mm_gStrayFairyWoodfallTexAnim,   mm_gStrayFairySnowheadTexAnim,
    mm_gStrayFairyGreatBayTexAnim,  mm_gStrayFairyStoneTowerTexAnim,
};

// 2S2H StrayFairyOverrideLimbDraw: always the left-facing head.
static s32 MergeShip_StrayFairy_GiOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos,
                                                   Vec3s* rot, void* arg, Gfx** gfx) {
    if (limbIndex == STRAY_FAIRY_LIMB_RIGHT_FACING_HEAD) {
        *dList = NULL;
    }
    return false;
}

// 2S2H DrawStrayFairy (mm/2s2h/Rando/DrawItem.cpp): one shared skeleton, advanced once per frame.
void MergeShip_StrayFairy_DrawModel(PlayState* play, s32 area) {
    static SkelAnime sSkelAnime;
    static Vec3s sJointTable[STRAY_FAIRY_LIMB_MAX];
    static PlayState* sInitPlay = NULL;
    static u32 sLastUpdate = 0;

    if ((play == NULL) || !MergeShip_IsMajorasMaskMounted()) {
        return;
    }
    if (!MergeShip_StrayFairy_ValidArea(area)) {
        area = MM_STRAY_FAIRY_AREA_CLOCK_TOWN;
    }
    if (sInitPlay != play) {
        sInitPlay = play;
        SkelAnime_InitFlex(play, &sSkelAnime, (FlexSkeletonHeader*)mm_gStrayFairySkel,
                           (AnimationHeader*)mm_gStrayFairyFlyingAnim, sJointTable, sJointTable,
                           STRAY_FAIRY_LIMB_MAX);
    }
    if (sLastUpdate != play->state.frames) {
        sLastUpdate = play->state.frames;
        SkelAnime_Update(&sSkelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    MergeShip_AnimatedMat_Draw(play, (void*)sStrayFairyTexAnims[area]);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
    POLY_XLU_DISP = SkelAnime_DrawFlex(play, sSkelAnime.skeleton, sSkelAnime.jointTable, sSkelAnime.dListCount,
                                       MergeShip_StrayFairy_GiOverrideLimbDraw, NULL, NULL, POLY_XLU_DISP);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void MergeShip_StrayFairy_DrawGi(PlayState* play, GetItemEntry* getItemEntry) {
    s32 area = MERGESHIP_GI_IS_MM_STRAY_FAIRY(getItemEntry->getItemId)
                   ? (getItemEntry->getItemId - MERGESHIP_GI_MM_STRAY_FAIRY_BASE)
                   : MM_STRAY_FAIRY_AREA_CLOCK_TOWN;

    if (!MergeShip_IsMajorasMaskMounted()) {
        GetItem_Draw(play, GID_BOTTLE);
        return;
    }
    MergeShip_StrayFairy_DrawModel(play, area);
}

// MM sGetItemTable[GI_STRAY_FAIRY - 1]: GET_ITEM(ITEM_NONE, OBJECT_UNSET_0, GID_NONE, 0x11, 0, 0). Hyrule's get-item
// needs an item (ITEM_MM_STRAY_FAIRY, counted in `area`) and something to hold up (the fairy, drawFunc). OBJECT_GI_BOTTLE
// and GID_BOTTLE are only what is loaded/drawn without mm.o2r.
GetItemEntry MergeShip_StrayFairy_GetItemEntry(s32 area) {
    s32 gi = MERGESHIP_GI_MM_STRAY_FAIRY(MergeShip_StrayFairy_ValidArea(area) ? area : MM_STRAY_FAIRY_AREA_CLOCK_TOWN);
    GetItemEntry entry = GET_ITEM(ITEM_MM_STRAY_FAIRY, OBJECT_GI_BOTTLE, GID_BOTTLE, MERGESHIP_TEXT_STRAY_FAIRY_FOUND,
                                  0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_SKULLTULA_TOKEN, MOD_NONE, gi);

    entry.drawFunc = MergeShip_StrayFairy_DrawGi;
    return entry;
}

// ---- the Masks page's panel --------------------------------------------------------------------------------------

// One textured quad in the page's model space (y up), `w` x `h` units, the texture's `texW` x `texH` texels across it.
static void MergeShip_StrayFairy_PanelQuad(PlayState* play, s16 x, s16 y, s16 w, s16 h, s16 texW, s16 texH,
                                           s32 mirror) {
    Vtx* vtx = Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
    s32 k;

    vtx[0].v.ob[0] = vtx[2].v.ob[0] = x;
    vtx[1].v.ob[0] = vtx[3].v.ob[0] = x + w;
    vtx[0].v.ob[1] = vtx[1].v.ob[1] = y;
    vtx[2].v.ob[1] = vtx[3].v.ob[1] = y - h;
    for (k = 0; k < 4; k++) {
        vtx[k].v.ob[2] = 0;
        vtx[k].v.flag = 0;
        vtx[k].v.cn[0] = vtx[k].v.cn[1] = vtx[k].v.cn[2] = vtx[k].v.cn[3] = 255;
    }
    // MM mirrors the first icon frame (sStrayFairyIconRectS, G_TX_MIRROR).
    vtx[0].v.tc[0] = vtx[2].v.tc[0] = mirror ? (texW << 5) : 0;
    vtx[1].v.tc[0] = vtx[3].v.tc[0] = mirror ? 0 : (texW << 5);
    vtx[0].v.tc[1] = vtx[1].v.tc[1] = 0;
    vtx[2].v.tc[1] = vtx[3].v.tc[1] = texH << 5;

    OPEN_DISPS(play->state.gfxCtx);
    gSPVertex(POLY_OPA_DISP++, (uintptr_t)vtx, 4, 0);
    gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM KaleidoScope_DrawDungeonStrayFairyCount's digit: a black shadow one unit down-right, then white.
static void MergeShip_StrayFairy_PanelChar(PlayState* play, const char* tex, s16 x, s16 y, u8 alpha) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPLoadTextureBlock(POLY_OPA_DISP++, tex, G_IM_FMT_I, G_IM_SIZ_8b, 8, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 0, alpha);
    CLOSE_DISPS(play->state.gfxCtx);
    MergeShip_StrayFairy_PanelQuad(play, x + 1, y - 1, 6, 12, 8, 16, false);
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, alpha);
    CLOSE_DISPS(play->state.gfxCtx);
    MergeShip_StrayFairy_PanelQuad(play, x, y, 6, 12, 8, 16, false);
}

void MergeShip_StrayFairy_DrawMaskPagePanel(PlayState* play) {
    // MM KaleidoScope_DrawDungeonMap's icon animation.
    static s16 sStrayFairyIconTimer = 30;
    static s16 sStrayFairyIconIndex = 0;
    static s16 sStrayFairyIconAlphaScaleTimer = 15;
    static s16 sStrayFairyIconAlphaScaleState = 0;
    static s16 sStrayFairyIconAlpha = 255;
    // MM sStrayFairyIconTextures, by temple (MMStrayFairyArea - 1).
    static const char* sStrayFairyIconTextures[4][4] = {
        { mm_gStrayFairyWoodfallIconTex, mm_gDungeonStrayFairyWoodfallIconTex, mm_gStrayFairyWoodfallIconTex,
          mm_gDungeonStrayFairyWoodfallIconTex },
        { mm_gStrayFairySnowheadIconTex, mm_gDungeonStrayFairySnowheadIconTex, mm_gStrayFairySnowheadIconTex,
          mm_gDungeonStrayFairySnowheadIconTex },
        { mm_gStrayFairyGreatBayIconTex, mm_gDungeonStrayFairyGreatBayIconTex, mm_gStrayFairyGreatBayIconTex,
          mm_gDungeonStrayFairyGreatBayIconTex },
        { mm_gStrayFairyStoneTowerIconTex, mm_gDungeonStrayFairyStoneTowerIconTex, mm_gStrayFairyStoneTowerIconTex,
          mm_gDungeonStrayFairyStoneTowerIconTex },
    };
    // MM sStrayFairyIconRectS: the first frame is mirrored.
    static const u8 sStrayFairyIconMirror[4] = { true, false, false, false };
    static const char* sCounterTextures[10] = {
        mm_gCounterDigit0Tex, mm_gCounterDigit1Tex, mm_gCounterDigit2Tex, mm_gCounterDigit3Tex, mm_gCounterDigit4Tex,
        mm_gCounterDigit5Tex, mm_gCounterDigit6Tex, mm_gCounterDigit7Tex, mm_gCounterDigit8Tex, mm_gCounterDigit9Tex,
    };
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 i;
    u8 iconAlpha;

    // MM draws it only on the page in view, while the pause menu is idle (PAUSE_STATE_MAIN, mainState IDLE).
    if (!MergeShip_StrayFairy_IsEnabled() || (pauseCtx->pageIndex != MERGESHIP_PAUSE_MASK) ||
        (pauseCtx->state != 6) || (pauseCtx->unk_1E4 != 0)) {
        return;
    }

    if (!sStrayFairyIconAlphaScaleState) {
        sStrayFairyIconAlpha -= 6;
    } else {
        sStrayFairyIconAlpha += 6;
    }
    sStrayFairyIconAlphaScaleTimer--;
    if (sStrayFairyIconAlphaScaleTimer == 0) {
        sStrayFairyIconAlphaScaleState ^= 1;
        sStrayFairyIconAlphaScaleTimer = 15;
    }
    sStrayFairyIconTimer--;
    if (sStrayFairyIconTimer == 0) {
        sStrayFairyIconIndex = (sStrayFairyIconIndex + 1) & 3;
        sStrayFairyIconTimer = 34;
    }
    iconAlpha = (u8)((CLAMP(sStrayFairyIconAlpha, 0, 255) * pauseCtx->alpha) / 255);

    Gfx_SetupDL_42Opa(play->state.gfxCtx);

    // Under the name panel (Ocarina of Time's info panel spans y -76 .. -100, z_kaleido_scope_PAL.c), one entry per
    // temple, 48 units each across the grid's width: the glowing circle and the fairy icon (MM's 32 x 24 at half size),
    // then "NN/15". (Directly below the grid it sat on the name panel's top edge.)
    for (i = 0; i < 4; i++) {
        s32 area = MM_STRAY_FAIRY_AREA_WOODFALL + i;
        s32 count = CLAMP(MergeShip_StrayFairy_GetCount(area), 0, 99);
        s16 x = -94 + i * 48;
        s16 y = -103 + pauseCtx->offsetY;
        Color_RGB8 prim;
        Color_RGB8 env;

        MergeShip_StrayFairy_GetIconColors(area, &prim, &env);

        OPEN_DISPS(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                          PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, prim.r, prim.g, prim.b, iconAlpha);
        gDPSetEnvColor(POLY_OPA_DISP++, env.r, env.g, env.b, 0);
        gDPLoadTextureBlock_4b(POLY_OPA_DISP++, mm_gStrayFairyGlowingCircleIconTex, G_IM_FMT_I, 32, 24, 0,
                               G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                               G_TX_NOLOD, G_TX_NOLOD);
        CLOSE_DISPS(play->state.gfxCtx);
        MergeShip_StrayFairy_PanelQuad(play, x, y, 16, 12, 32, 24, false);

        OPEN_DISPS(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, iconAlpha);
        gDPLoadTextureBlock(POLY_OPA_DISP++, sStrayFairyIconTextures[i][sStrayFairyIconIndex], G_IM_FMT_RGBA,
                            G_IM_SIZ_32b, 32, 24, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        CLOSE_DISPS(play->state.gfxCtx);
        MergeShip_StrayFairy_PanelQuad(play, x, y, 16, 12, 32, 24, sStrayFairyIconMirror[sStrayFairyIconIndex]);

        // MM KaleidoScope_DrawDungeonStrayFairyCount: the count, the slash and STRAY_FAIRY_SCATTERED_TOTAL.
        MergeShip_StrayFairy_PanelChar(play, sCounterTextures[count / 10], x + 17, y, pauseCtx->alpha);
        MergeShip_StrayFairy_PanelChar(play, sCounterTextures[count % 10], x + 23, y, pauseCtx->alpha);
        MergeShip_StrayFairy_PanelChar(play, mm_gStrayFairyMapCounterSlashTex, x + 29, y, pauseCtx->alpha);
        MergeShip_StrayFairy_PanelChar(play, sCounterTextures[MM_STRAY_FAIRY_SCATTERED_TOTAL / 10], x + 35, y,
                                       pauseCtx->alpha);
        MergeShip_StrayFairy_PanelChar(play, sCounterTextures[MM_STRAY_FAIRY_SCATTERED_TOTAL % 10], x + 41, y,
                                       pauseCtx->alpha);
    }

    // Leave the page's own state as the mask icons expect it.
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    CLOSE_DISPS(play->state.gfxCtx);
}
