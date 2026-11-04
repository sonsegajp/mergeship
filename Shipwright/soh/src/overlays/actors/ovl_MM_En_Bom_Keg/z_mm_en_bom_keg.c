/*
 * File: z_mm_en_bom_keg.c
 * Overlay: ovl_MM_En_Bom_Keg
 * Description: Majora's Mask's Powder Keg
 *
 * MergeShip2 -- the powder keg branch of MM En_Bom (2ship2harkinian/mm/src/overlays/actors/ovl_En_Bom/z_en_bom.c):
 * every `isPowderKeg` branch, with isPowderKeg always true. MM's timers (gSaveContext.powderKegTimer, here
 * gMergeShipPowderKegTimer), scales, collider sizes, bounces, rolling, fuse (its segment physics and model), sparks,
 * sounds, quake and rumble, and the explosion drawn by MM En_Clear_Tag's large explosion (ovl_MM_En_Clear_Tag). The
 * keg's model, fuse and explosion come from mm.o2r (ovl_En_Bom, ovl_En_Clear_Tag, gameplay_keep); none of their display
 * lists calls segment 0x0C. Sounds are MM's, through MergeShip's MM sound engine.
 *
 * Carrying: the actor follows Ocarina of Time's carry protocol the way Ocarina of Time's En_Bom does (and MM's, which
 * is the same): while `parent` is set it waits (WaitForRelease), the player positions it, and the player's own
 * throw / put-down code clears `parent` and gives it speedXZ / velocity; on the ground it offers itself to be picked
 * up again (Actor_OfferCarry). When it explodes in the player's hands it releases the player's carry state as MM's
 * func_80123590 (= Ocarina of Time's EnBom_Explode) does.
 *
 * Host-game differences:
 * - Init's parent test: MM links `parent` before Init runs (Actor_SpawnAsChildAndCutscene); Ocarina of Time's
 *   Actor_Spawn runs Init first and Actor_SpawnAsChild links the parent after it returns. MM Init's
 *   `if (Actor_HasParent) ... else ...` tail is therefore run at the start of the first update (parentChecked).
 * - WaitForRelease writes gMergeShipPowderKegTimer only while the player holds the keg. MM's keg only ever has the
 *   player as parent; Ocarina of Time's Dodongos parent any explosive they eat (En_Dodongo), and that must not leave a
 *   timer behind for the player to re-light a keg with after a loading zone.
 * - Damage: the explosion's toucher carries Ocarina of Time's DMG_EXPLOSIVE (MM 0x80000008 = DMG_EXPLOSIVES |
 *   DMG_POWDER_KEG). MM's DMG_POWDER_KEG bit is the one MM's keg-only boulders (Obj_Hugebombiwa, Obj_Bigicicle ...)
 *   look for; Hyrule has no such boulders, and in Ocarina of Time that bit (DMG_UNKNOWN_2) would make every enemy read
 *   damage-table entry 31 (none) instead of the explosive entry. With DMG_EXPLOSIVE the keg breaks everything a bomb
 *   breaks: walls that read Actor_GetCollidedExplosive or DMG_EXPLOSIVE (Bg_Breakwall, Bg_Spot16_Bombstone -- Dodongo's
 *   Cavern's entrance -- Bg_Ddan_Kd, Bg_Dodoago's eyes, Bg_Hidan_Kowarerukabe ...). Damage value 8: MM's toucher says
 *   4, but MM's CollisionCheck_GetElementATDamage makes any En_Bom explosion deal 8 to the player, and Ocarina of
 *   Time's bomb (the reference for actors without a damage table) deals 8.
 * - Walls that break on a nearby explosion (func_80033684: Obj_Bombiwa, Obj_Kibako2, Bg_Spot17_Bakudankabe ...)
 *   measure `shape.rot.z * 10 + 80` to an explosive whose params is 1. MM's keg does not use shape.rot.z once it
 *   explodes (only its body is drawn), so the explosion sets it to 12: their reach becomes MM's 200 explosion radius.
 * - AC bumper: MM 0x00013828 (DMG_EXPLOSIVES | arrows | DMG_DEKU_BUBBLE) in Ocarina of Time's bits; MM's Deku bubble is
 *   carried as DMG_SLINGSHOT in MergeShip (ovl_MM_En_Arrow).
 * - ACTOR_FLAG_FREEZE_EXCEPTION does not exist in Ocarina of Time (its bit 20 is unassigned); the held keg keeps
 *   updating through Ocarina of Time's own rule (the player's held actor / child is never frozen). The explosion frames
 *   are not exempted from a freeze.
 * - ACTORCTX_FLAG_0 ("a keg exists", read by MM's Goron keg seller) is gMergeShipPowderKegActorExists.
 * - PLAYER_STATE1_2 (MM: transforming) is the player's mergeShipTransforming; Player_GetMask == PLAYER_MASK_GIANT is
 *   MergeShip_GiantsMask_IsWorn, as in Ocarina of Time's own En_Bom.
 * - The fuse's hiss: MM plays NA_SE_IT_BIG_BOMB_IGNIT with Audio_PlaySfx_AtPosWithChannelIO, whose io port 6
 *   (0/1/2 as flashSpeedScale drops at 300 and 60 frames) quickens the sound. MergeShip's MM sound engine does not
 *   export AudioSfx_SetChannelIO yet, so the hiss plays without the io value (see MMEnBomKeg_PlayIgniteSfx).
 * - SoundSource_PlaySfxAtFixedWorldPos (the "fizzle" when water puts the fuse out) plays at the keg's position
 *   projected once, not re-projected over its 30 frames: Ocarina of Time's sound sources play OoT ids only.
 * - The fuse texture: MM's 2x2 gPowderKegFuseTex is too small for Fast3D; 2S2H substitutes an 8x8 texture
 *   (gPowderKegFuseTexLarger) from its own archive, which is not part of mm.o2r. The same 8x8 texels are embedded here.
 * - MM's EnBom_Draw bomb branch (GameInteractor bomb billboarding) belongs to bombs only.
 */

#include "z_mm_en_bom_keg.h"
#include <stddef.h>
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Bombf/z_en_bombf.h"
#include "overlays/actors/ovl_MM_En_Clear_Tag/z_mm_en_clear_tag.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "mm/overlays/ovl_En_Bom/ovl_En_Bom.h"
#include "soh/MergeShip/MergeShipMMItems.h"
#include "soh/MergeShip/MergeShipTransformFx.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

#define MMKEG_SFX_FLAG 0x800
#define MMKEG_PIf 3.14159265358979323846f
// MM's binang helpers (Ocarina of Time's BINANG_SUB / BINANG_ROT180 differ in parenthesisation / value).
#define MMKEG_BINANG_SUB(a, b) ((s16)((a) - (b)))
#define MMKEG_BINANG_ROT180(angle) ((s16)((angle) + 0x8000))
#define MMKEG_ABS_ALT(x) ((x) < 0 ? -(x) : (x))

// MM FloorType / FloorEffect values used here (SurfaceType_GetFloorType / GetFloorEffect read the same bits in both
// games).
#define MMKEG_FLOOR_TYPE_4 4   // sand
#define MMKEG_FLOOR_TYPE_5 5   // ice
#define MMKEG_FLOOR_TYPE_14 14 // snow
#define MMKEG_FLOOR_TYPE_15 15 // deep snow
#define MMKEG_FLOOR_EFFECT_1 1 // slippery slope

// Ocarina of Time's actors cast any explosive to EnBom / EnBombf; the keg's timer must sit where theirs does.
_Static_assert(offsetof(MMEnBomKeg, timer) == offsetof(EnBom, timer), "MMEnBomKeg.timer must line up with EnBom");
_Static_assert(offsetof(MMEnBomKeg, timer) == offsetof(EnBombf, timer), "MMEnBomKeg.timer must line up with EnBombf");
_Static_assert(offsetof(MMEnBomKeg, collider2) == offsetof(EnBom, explosionCollider),
               "MMEnBomKeg's explosion collider must line up with EnBom's");

s16 gMergeShipActorId_MM_En_Bom_Keg = -1;
u8 gMergeShipPowderKegActorExists = 0;

// MergeShip2: harness log (no-op outside a test script).
void MergeShipTest_Log(const char* fmt, ...);
// MergeShip2: MM Player_GetMask(play) == PLAYER_MASK_GIANT (MergeShipGiantsMask.inc, z_player.c).
s32 MergeShip_GiantsMask_IsWorn(PlayState* play);

void MMEnBomKeg_Move(MMEnBomKeg* this, PlayState* play);
void MMEnBomKeg_WaitForRelease(MMEnBomKeg* this, PlayState* play);
void MMEnBomKeg_Explode(MMEnBomKeg* this, PlayState* play);

void MMEnBomKeg_InitFuse(PlayState* play, Vec3f* arg1);
void MMEnBomKeg_UpdateFuse(PlayState* play, Vec3f* arg1, Vec3f* arg2, Vec3f* arg3, s32 arg4);
void MMEnBomKeg_DrawFuse(PlayState* play, s32 timer);

// MM PowderKegFuseSegment
typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ Vec3f velocity;
    /* 0x18 */ s16 rotY;
    /* 0x1A */ s16 rotX;
} MMPowderKegFuseSegment; // size = 0x1C

static MMPowderKegFuseSegment sPowderKegFuseSegments[16];

// MM enBomScales[] = { 0.01f (bomb), 0.03f (keg) }
static f32 enBomScales[] = { 0.01f, 0.03f };

// MM gZeroVec3f
static Vec3f sMMKegZeroVec = { 0.0f, 0.0f, 0.0f };

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_HIT0,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER | AC_TYPE_OTHER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000000, 0x00, 0x00 },
        // MM 0x00013828: DMG_EXPLOSIVES | DMG_NORMAL_ARROW | DMG_FIRE_ARROW | DMG_ICE_ARROW | DMG_LIGHT_ARROW |
        // DMG_DEKU_BUBBLE (MergeShip's Deku bubble is DMG_SLINGSHOT).
        { DMG_EXPLOSIVE | DMG_ARROW_NORMAL | DMG_ARROW_FIRE | DMG_ARROW_ICE | DMG_ARROW_LIGHT | DMG_SLINGSHOT, 0x00,
          0x00 },
        TOUCH_NONE | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON,
    },
    { 6, 11, 14, { 0, 0, 0 } },
};

// MM sJntSphElementsInit2 (the keg's; sJntSphElementsInit1 is the bomb's).
static ColliderJntSphElementInit sJntSphElementsInit2[1] = {
    {
        {
            ELEMTYPE_UNK0,
            // MM { 0x80000008, 0x00, 0x04 }: see the top of the file for the flag and the damage.
            { DMG_EXPLOSIVE, 0x00, 0x08 },
            { 0x00000000, 0x00, 0x00 },
            TOUCH_ON | TOUCH_SFX_NONE,
            BUMP_NONE,
            OCELEM_NONE,
        },
        { 0, { { 0, 0, 0 }, 0 }, 100 },
    },
};

static ColliderJntSphInit sJntSphInit2 = {
    {
        COLTYPE_HIT0,
        AT_ON | AT_TYPE_ALL,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_JNTSPH,
    },
    ARRAY_COUNT(sJntSphElementsInit2),
    sJntSphElementsInit2,
};

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F(scale, 0, ICHAIN_CONTINUE),
    ICHAIN_F32(targetArrowOffset, 2000, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, -4000, ICHAIN_STOP),
};

// MM Math_Vec3f_ToVec3s
static void MMEnBomKeg_Vec3fToVec3s(Vec3s* dest, Vec3f* src) {
    dest->x = (s16)src->x;
    dest->y = (s16)src->y;
    dest->z = (s16)src->z;
}

// MM Actor_GetSlopeDirection
static void MMEnBomKeg_GetSlopeDirection(CollisionPoly* floorPoly, Vec3f* slopeNormal, s16* downwardSlopeYaw) {
    slopeNormal->x = COLPOLY_GET_NORMAL(floorPoly->normal.x);
    slopeNormal->y = COLPOLY_GET_NORMAL(floorPoly->normal.y);
    slopeNormal->z = COLPOLY_GET_NORMAL(floorPoly->normal.z);
    // MM Math_Atan2S_XY(x, y) is Ocarina of Time's Math_Atan2S(x, y).
    *downwardSlopeYaw = Math_Atan2S(slopeNormal->z, slopeNormal->x);
}

// MM func_80123590: the explosive the player carries goes away; the player's carry state is released.
static s32 MMEnBomKeg_ReleaseFromPlayer(PlayState* play, Actor* actor) {
    Player* player = GET_PLAYER(play);

    if ((player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) && (player->heldActor == actor)) {
        player->interactRangeActor = NULL;
        player->heldActor = NULL;
        player->actor.child = NULL;
        player->stateFlags1 &= ~PLAYER_STATE1_CARRYING_ACTOR;
        return true;
    }

    return false;
}

// MM Audio_PlaySfx_AtPosWithChannelIO(&thisx->projectedPos, NA_SE_IT_BIG_BOMB_IGNIT - SFX_FLAG, ioData). The io value
// (port 6) is not applied: see the top of the file. When MergeShipMMSfx.c exports MM's AudioSfx_SetChannelIO, call it
// here with (&this->actor.projectedPos, id, ioData) before playing.
static void MMEnBomKeg_PlayIgniteSfx(MMEnBomKeg* this, u8 ioData) {
    (void)ioData;
    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_BIG_BOMB_IGNIT - MMKEG_SFX_FLAG);
}

// MM SoundSource_PlaySfxAtFixedWorldPos(play, worldPos, 30, sfxId): the sound stays where the keg was, after the
// keg is gone, so it cannot use the actor's projectedPos.
static Vec3f sFixedSfxProjectedPos;
static void MMEnBomKeg_PlaySfxAtFixedWorldPos(PlayState* play, Vec3f* worldPos, u16 mmSfxId) {
    SkinMatrix_Vec3fMtxFMultXYZ(&play->viewProjectionMtxF, worldPos, &sFixedSfxProjectedPos);
    MergeShip_PlayMMSfxAtPos(&sFixedSfxProjectedPos, mmSfxId);
}

// MM EnBom_Init, with isPowderKeg = true.
void MMEnBomKeg_Init(Actor* thisx, PlayState* play) {
    MMEnBomKeg* this = (MMEnBomKeg*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    ActorShape_Init(&this->actor.shape, 700.0f, ActorShadow_DrawCircle, 16.0f);
    this->actor.colChkInfo.mass = 200;
    this->actor.colChkInfo.cylRadius = 5;
    this->actor.colChkInfo.cylHeight = 10;

    this->flashSpeedScale = 7;
    this->isPowderKeg = true;
    // MM `play->actorCtx.flags |= ACTORCTX_FLAG_0`
    gMergeShipPowderKegActorExists = true;
    this->timer = (s16)gMergeShipPowderKegTimer;

    Collider_InitCylinder(play, &this->collider1);
    Collider_InitJntSph(play, &this->collider2);
    Collider_SetCylinder(play, &this->collider1, &this->actor, &sCylinderInit);

    Collider_SetJntSph(play, &this->collider2, &this->actor, &sJntSphInit2, this->collider2Elements);
    this->collider1.dim.radius = 20;
    this->collider1.dim.height = 36;
    MMEnBomKeg_InitFuse(play, &this->actor.world.pos);

    this->collider2Elements[0].info.toucher.damage += (u8)((thisx->shape.rot.z & 0xFF00) >> 8);
    this->actor.shape.rot.z &= 0xFF;
    if (this->actor.shape.rot.z & 0x80) {
        this->actor.shape.rot.z |= 0xFF00;
    }

    this->collider2.elements[0].dim.worldSphere.center.x = (s16)this->actor.world.pos.x;
    this->collider2.elements[0].dim.worldSphere.center.y = (s16)this->actor.world.pos.y;
    this->collider2.elements[0].dim.worldSphere.center.z = (s16)this->actor.world.pos.z;

    // MM: this->actor.flags |= ACTOR_FLAG_FREEZE_EXCEPTION (no Ocarina of Time counterpart, see the top of the file).

    // MM's `if (Actor_HasParent) ... else ...` runs at the first update (MMEnBomKeg_CheckParent), once
    // Actor_SpawnAsChild has linked the parent.
    this->parentChecked = false;
    this->actionFunc = MMEnBomKeg_Move;

    MergeShipTest_Log("K keg init timer=%d id=%d", this->timer, this->actor.id);
}

// The tail of MM EnBom_Init (see the top of the file).
static void MMEnBomKeg_CheckParent(MMEnBomKeg* this, PlayState* play) {
    this->parentChecked = true;

    if (Actor_HasParent(&this->actor, play)) {
        this->actionFunc = MMEnBomKeg_WaitForRelease;
        this->actor.room = -1;
        Actor_SetScale(&this->actor, enBomScales[this->isPowderKeg]);
    } else {
        this->actionFunc = MMEnBomKeg_Move;
        gMergeShipPowderKegTimer = 0;
    }
}

// MM EnBom_Destroy
void MMEnBomKeg_Destroy(Actor* thisx, PlayState* play) {
    MMEnBomKeg* this = (MMEnBomKeg*)thisx;

    Collider_DestroyJntSph(play, &this->collider2);
    Collider_DestroyCylinder(play, &this->collider1);
    if (this->isPowderKeg) {
        // MM `play->actorCtx.flags &= ~ACTORCTX_FLAG_0`
        gMergeShipPowderKegActorExists = false;
    }
}

// MM EnBom_Move
void MMEnBomKeg_Move(MMEnBomKeg* this, PlayState* play) {
    static Vec3f D_80872E68[] = {
        { 2.0f, -6.0f, -0.3f },
        { 1.5f, -5.0f, -0.6f },
        { 0.2f, -6.0f, -0.1f },
    };

    // if bomb has a parent actor, the bomb hasnt been released yet
    if (Actor_HasParent(&this->actor, play)) {
        this->actionFunc = MMEnBomKeg_WaitForRelease;
        this->actor.room = -1;
        return;
    }

    if ((this->actor.velocity.y > 0.0f) && (this->actor.bgCheckFlags & BGCHECKFLAG_CEILING)) {
        this->actor.velocity.y = -this->actor.velocity.y;
    }

    // rebound bomb off the wall it hits
    if ((this->actor.speedXZ != 0.0f) && (this->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
        s16 yDiff = MMKEG_BINANG_SUB(this->actor.wallYaw, this->actor.world.rot.y);

        if (MMKEG_ABS_ALT(yDiff) > 0x4000) {
            this->actor.world.rot.y =
                MMKEG_BINANG_SUB(this->actor.wallYaw - this->actor.world.rot.y + this->actor.wallYaw, 0x8000);
        }

        MergeShip_PlayMMSfxActor(&this->actor, this->isPowderKeg ? MM_NA_SE_EV_PUT_DOWN_WOODBOX : MM_NA_SE_EV_BOMB_BOUND);
        Actor_MoveXZGravity(&this->actor);
        this->actor.speedXZ *= 0.7f;
        this->actor.bgCheckFlags &= ~BGCHECKFLAG_WALL;
    }

    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        Math_StepToF(&this->actor.speedXZ, 0.0f, 0.08f);
    } else {
        Vec3f* sp58;
        u32 floorType = SurfaceType_GetFloorType(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId);
        Vec3f slopeNormal;
        s16 downwardSlopeYaw;
        f32 sp40;
        f32 sp3C;
        f32 sp38;

        sp58 = &D_80872E68[this->isPowderKeg];

        if (floorType == MMKEG_FLOOR_TYPE_5) {
            sp58 = &D_80872E68[2];
        }

        if ((floorType == MMKEG_FLOOR_TYPE_4) || (floorType == MMKEG_FLOOR_TYPE_14) ||
            (floorType == MMKEG_FLOOR_TYPE_15)) {
            s16 sp36;

            Math_ApproachF(&this->actor.shape.yOffset, 0.0f, 0.1f, 50.0f);
            sp36 = this->actor.shape.shadowAlpha;
            Math_ApproachS(&sp36, 0, 1, 20);
            this->actor.shape.shadowAlpha = (u8)sp36;
        } else {
            Math_ApproachF(&this->actor.shape.yOffset, 700.0f, 1.0f, 700.0f);
        }

        sp40 = Math_SinS(this->actor.world.rot.y) * this->actor.speedXZ;
        sp3C = Math_CosS(this->actor.world.rot.y) * this->actor.speedXZ;
        MMEnBomKeg_GetSlopeDirection(this->actor.floorPoly, &slopeNormal, &downwardSlopeYaw);

        sp40 += 3.0f * slopeNormal.x;
        sp3C += 3.0f * slopeNormal.z;
        sp38 = sqrtf(SQ(sp40) + SQ(sp3C));

        if ((sp38 < this->actor.speedXZ) ||
            (SurfaceType_GetFloorEffect(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId) ==
             MMKEG_FLOOR_EFFECT_1)) {
            if (sp38 > 16.0f) {
                this->actor.speedXZ = 16.0f;
            } else {
                this->actor.speedXZ = sp38;
            }
            this->actor.world.rot.y = Math_Atan2S(sp3C, sp40);
        }

        if (!Math_StepToF(&this->actor.speedXZ, 0.0f, sp58->x)) {
            s16 temp = this->actor.world.rot.y;
            s32 pad;

            if (MMKEG_ABS_ALT(MMKEG_BINANG_SUB(this->actor.world.rot.y, this->actor.shape.rot.y)) > 0x4000) {
                temp = MMKEG_BINANG_ROT180(temp);
            }
            Math_ScaledStepToS(&this->actor.shape.rot.y, temp, (s16)(this->actor.speedXZ * 100.0f));
            this->unk_1FA += (s16)(s32)(this->actor.speedXZ * 800.0f);
        }

        if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
            MergeShip_PlayMMSfxActor(&this->actor,
                                     this->isPowderKeg ? MM_NA_SE_EV_TRE_BOX_BOUND : MM_NA_SE_EV_BOMB_BOUND);
            if (this->actor.velocity.y < sp58->y) {
                if ((floorType == MMKEG_FLOOR_TYPE_4) || (floorType == MMKEG_FLOOR_TYPE_14) ||
                    (floorType == MMKEG_FLOOR_TYPE_15)) {
                    this->actor.velocity.y = 0.0f;
                } else {
                    this->actor.velocity.y *= sp58->z;
                }
                this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
            }
        } else if ((this->timer >= 4) && (GET_PLAYER(play)->mergeShipForm == PLAYER_FORM_GORON)) {
            // MergeShip2: MM Player_ActionHandler_2 lets only the Goron lift a powder keg (the keg is offered to it
            // alone here, as Hyrule's player has no such check).
            Actor_OfferCarry(&this->actor, play);
        }
    }

    Actor_MoveXZGravity(&this->actor);
}

// MM EnBom_WaitForRelease
void MMEnBomKeg_WaitForRelease(MMEnBomKeg* this, PlayState* play) {
    // if parent pointer is NULL, the bomb has been released
    if (Actor_HasNoParent(&this->actor, play)) {
        this->actionFunc = MMEnBomKeg_Move;
        this->actor.room = play->roomCtx.curRoom.num;
        // MM: this->actor.flags &= ~ACTOR_FLAG_FREEZE_EXCEPTION
        this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        Math_Vec3s_ToVec3f(&this->actor.prevPos, &this->actor.home.rot);
        if (this->isPowderKeg) {
            gMergeShipPowderKegTimer = 0;
        }
        MergeShipTest_Log("K keg released timer=%d speed=%.2f vy=%.2f", this->timer, this->actor.speedXZ,
                          this->actor.velocity.y);
        MMEnBomKeg_Move(this, play);
    } else {
        MMEnBomKeg_Vec3fToVec3s(&this->actor.home.rot, &this->actor.parent->world.pos);
        // MergeShip2: only while the player holds it (see the top of the file).
        if (this->isPowderKeg && (this->actor.parent == &GET_PLAYER(play)->actor)) {
            gMergeShipPowderKegTimer = (u16)this->timer;
        }
    }
    Math_ScaledStepToS(&this->unk_1FA, 0, 0x7D0);
}

// MM EnBom_Explode
void MMEnBomKeg_Explode(MMEnBomKeg* this, PlayState* play) {
    static s16 D_80872E8C[] = { 100, 200 };
    static Color_RGBA8 D_80872E90 = { 185, 140, 70, 255 };
    static Color_RGBA8 D_80872E94 = { 255, 255, 255, 255 };
    s32 i;
    u32 floorType;
    f32 temp_f20;
    s32 pad;
    f32 spCC;
    Vec3f spC0;
    Vec3f spB4;
    CollisionPoly* spB0;
    s32 bgId;
    Vec3f spA0;
    Vec3f sp94;
    Vec3f sp88;
    Color_RGBA8 sp84;
    Color_RGBA8 sp80;

    if (this->collider2.elements[0].dim.modelSphere.radius == 0) {
        this->actor.flags |= ACTOR_FLAG_DRAW_CULLING_DISABLED;
        Rumble_Request(this->actor.xzDistToPlayer, 255, 20, 150);
    }

    this->collider2.elements[0].dim.worldSphere.radius = D_80872E8C[this->isPowderKeg];
    if (this->timer == 7) {
        this->collider2.base.atFlags &= ~AT_TYPE_ENEMY;
    }

    if (this->actor.params == MMENBOMKEG_EXPLOSION) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider2.base);
    }

    if (play->envCtx.adjLight1Color[0] != 0) {
        play->envCtx.adjLight1Color[0] -= 25;
    }

    if (play->envCtx.adjLight1Color[1] != 0) {
        play->envCtx.adjLight1Color[1] -= 25;
    }

    if (play->envCtx.adjLight1Color[2] != 0) {
        play->envCtx.adjLight1Color[2] -= 25;
    }

    if (play->envCtx.adjAmbientColor[0] != 0) {
        play->envCtx.adjAmbientColor[0] -= 25;
    }

    if (play->envCtx.adjAmbientColor[1] != 0) {
        play->envCtx.adjAmbientColor[1] -= 25;
    }

    if (play->envCtx.adjAmbientColor[2] != 0) {
        play->envCtx.adjAmbientColor[2] -= 25;
    }

    if (this->timer == 0) {
        MMEnBomKeg_ReleaseFromPlayer(play, &this->actor);
        Actor_Kill(&this->actor);
    }

    if ((this->timer & 1) == 0) {
        spCC = Rand_ZeroFloat(MMKEG_PIf);

        for (i = 0; i < 15; i++) {
            Matrix_RotateY(((2.0f * (i * MMKEG_PIf)) / 15.0f) + spCC, MTXMODE_NEW);
            // MM Matrix_MultVecZ
            {
                Vec3f z = { 0.0f, 0.0f, 0.0f };

                z.z = (10 - this->timer) * 300.0f * 0.1f;
                Matrix_MultVec3f(&z, &spC0);
            }
            spB4.x = this->actor.world.pos.x + spC0.x;
            spB4.y = this->actor.world.pos.y + 500.0f;
            spB4.z = this->actor.world.pos.z + spC0.z;
            if (BgCheck_EntityRaycastFloor3(&play->colCtx, &spB0, &bgId, &spB4) != BGCHECK_Y_MIN) {
                floorType = SurfaceType_GetFloorType(&play->colCtx, spB0, bgId);
                temp_f20 = BgCheck_EntityRaycastFloor1(&play->colCtx, &spB0, &spB4);

                if ((floorType == MMKEG_FLOOR_TYPE_4) || (floorType == MMKEG_FLOOR_TYPE_15) ||
                    (floorType == MMKEG_FLOOR_TYPE_14)) {
                    if (floorType == MMKEG_FLOOR_TYPE_4) {
                        sp84 = D_80872E90;
                        sp80 = D_80872E90;
                    } else {
                        sp84 = D_80872E94;
                        sp80 = D_80872E94;
                    }
                    {
                        Vec3f z = { 0.0f, 0.0f, 5.0f };

                        Matrix_MultVec3f(&z, &sp94);
                    }
                    sp88.x = sp88.z = 0.0f;
                    sp94.y = 2.0f;
                    sp88.y = 0.2f;

                    spA0.x = spB4.x;
                    spA0.y = temp_f20;
                    spA0.z = spB4.z;

                    temp_f20 = Rand_ZeroFloat(250.0f);
                    // MM func_800B0F18 (EffectSsDust_Spawn with DUST_DRAWFLAG1 and a life)
                    EffectSsDust_Spawn(play, 1, &spA0, &sp94, &sp88, &sp84, &sp80, (s16)(temp_f20 + 950.0f), 10,
                                       (s16)(Rand_ZeroFloat(5.0f) + 14.0f), 0);
                }
            }
        }
    }
}

static s16 sQuakeY[] = { 3, 5 };
static s16 sQuakeDurations[] = { 10, 15 };

// MM EnBom_Update, with isPowderKeg = true.
void MMEnBomKeg_Update(Actor* thisx, PlayState* play) {
    Vec3f effVelocity = { 0.0f, 0.0f, 0.0f };
    Vec3f bomb2Accel = { 0.0f, 0.1f, 0.0f }; // unused
    Vec3f effAccel = { 0.0f, 0.0f, 0.0f };
    Vec3f effPos;
    Vec3f dustAccel = { 0.0f, 0.6f, 0.0f };
    Color_RGBA8 dustColor = { 255, 255, 255, 255 };
    MMEnBomKeg* this = (MMEnBomKeg*)thisx;
    s32 pad;
    Player* player = GET_PLAYER(play);

    (void)bomb2Accel;

    if (!this->parentChecked) {
        MMEnBomKeg_CheckParent(this, play);
    }

    // MM `player->stateFlags1 & PLAYER_STATE1_2` (transforming)
    if (player->mergeShipTransforming) {
        return;
    }

    // MM `Player_GetMask(play) == PLAYER_MASK_GIANT`
    if (MergeShip_GiantsMask_IsWorn(play)) {
        Actor_Kill(thisx);
        return;
    }

    if (this->unk_1FC != 0) {
        this->unk_1FC--;
        Math_ApproachZeroF(&thisx->speedXZ, 1.0f, 1.0f);
        Actor_MoveXZGravity(thisx);
        Actor_UpdateBgCheckInfo(play, thisx, 35.0f, 10.0f, 36.0f, 4 /* MM UPDBGCHECKINFO_FLAG_4 */);
        if (this->unk_1FC == 0) {
            if (this->isPowderKeg) {
                gMergeShipPowderKegTimer = 0;
            }
            Actor_Kill(thisx);
        }
    } else {
        thisx->gravity = -1.2f;
        if (this->timer != 0) {
            if (!this->isPowderKeg || (MMEnBomKeg_Explode == this->actionFunc) || !Play_InCsMode(play)) {
                this->timer--;
            }
        }

        if ((!this->isPowderKeg && (this->timer == 67)) || (this->isPowderKeg && (this->timer <= 2400))) {
            MergeShip_PlayMMSfxActor(thisx, MM_NA_SE_PL_TAKE_OUT_SHIELD);
            Actor_SetScale(thisx, enBomScales[this->isPowderKeg]);
        }

        if ((thisx->xzDistToPlayer >= 20.0f) || (fabsf(thisx->yDistToPlayer) >= 80.0f)) {
            this->unk_1F8 = 1;
        }

        this->actionFunc(this, play);

        // MM UPDBGCHECKINFO_FLAG_1 | 2 | 4 | 8 | 0x10 (the same bits in Ocarina of Time)
        Actor_UpdateBgCheckInfo(play, thisx, 35.0f, 10.0f, 36.0f, 0x1F);
        if (thisx->params == MMENBOMKEG_BODY) {
            static Vec3us D_80872ED4[] = {
                { 40, 20, 100 },
                { 300, 60, 600 },
            };
            Vec3us* sp60 = &D_80872ED4[this->isPowderKeg];

            // spawn spark effect on even frames
            dustAccel.y = 0.2f;
            Math_Vec3f_Copy(&effPos, &thisx->home.pos);
            if ((play->gameplayFrames % 2) == 0) {
                EffectSsGSpk_SpawnFuse(play, thisx, &effPos, &effVelocity, &effAccel);
            }
            MMEnBomKeg_PlayIgniteSfx(this, (this->flashSpeedScale == 7)   ? 0
                                           : (this->flashSpeedScale == 3) ? 1
                                                                          : 2);

            effPos.y += 3.0f;
            // MM func_800B0DE0
            func_8002829C(play, &effPos, &effVelocity, &dustAccel, &dustColor, &dustColor, 50, 5);

            if ((this->collider1.base.acFlags & AC_HIT) ||
                ((this->collider1.base.ocFlags1 & OC1_HIT) && ((this->collider1.base.oc->category == ACTORCAT_ENEMY) ||
                                                               (this->collider1.base.oc->category == ACTORCAT_BOSS)))) {
                this->timer = 0;
                thisx->shape.rot.z = 0;
            } else {
                // if a lit stick touches the bomb, set timer to 100
                if ((this->timer > 100) && Player_IsBurningStickInRange(play, &thisx->world.pos, 30.0f, 50.0f)) {
                    this->timer = 100;
                }
            }

            dustAccel.y = 0.2f;
            effPos = thisx->world.pos;
            effPos.y += 10.0f;

            // double bomb flash speed and adjust red color at certain times during the countdown
            if ((this->timer == sp60->x) || (this->timer == sp60->y) || (this->timer == 3)) {
                thisx->shape.rot.z = 0;
                this->flashSpeedScale >>= 1;
            }

            if ((this->timer < sp60->z) && (this->timer & (this->flashSpeedScale + 1))) {
                Math_ApproachF(&this->unk_1F4, 140.0f, 1.0f, 140.0f / this->flashSpeedScale);
            } else {
                Math_ApproachZeroF(&this->unk_1F4, 1.0f, 140.0f / this->flashSpeedScale);
            }

            if (this->timer < 3) {
                Actor_SetScale(thisx, thisx->scale.x + 0.002f);
            }

            if (this->timer == 0) {
                effPos = thisx->world.pos;
                effPos.y += 10.0f;
                if (Actor_HasParent(thisx, play)) {
                    effPos.y += 30.0f;
                }
                // MM: Actor_Spawn(ACTOR_EN_CLEAR_TAG, ..., CLEAR_TAG_PARAMS(this->isPowderKeg)) -- the large explosion.
                if (gMergeShipActorId_MM_En_Clear_Tag >= 0) {
                    Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_MM_En_Clear_Tag, effPos.x, effPos.y - 10.0f,
                                effPos.z, 0, 0, 0, MMCLEARTAG_PARAMS(this->isPowderKeg));
                }
                Actor_RequestQuakeAndRumble(thisx, play, sQuakeY[this->isPowderKeg],
                                            sQuakeDurations[this->isPowderKeg]);
                play->envCtx.adjLight1Color[0] = play->envCtx.adjLight1Color[1] = play->envCtx.adjLight1Color[2] =
                    250;
                play->envCtx.adjAmbientColor[0] = play->envCtx.adjAmbientColor[1] =
                    play->envCtx.adjAmbientColor[2] = 250;
                Camera_AddQuake(&play->mainCamera, 2, 11, 8);
                thisx->params = MMENBOMKEG_EXPLOSION;
                this->timer = 10;
                // MM also sets ACTOR_FLAG_FREEZE_EXCEPTION (none in Ocarina of Time).
                thisx->flags |= ACTOR_FLAG_DRAW_CULLING_DISABLED;
                this->actionFunc = MMEnBomKeg_Explode;
                // MergeShip2: Ocarina of Time's "explosion nearby" test (func_80033684) reaches rot.z * 10 + 80:
                // 200, MM's keg explosion radius (see the top of the file).
                thisx->shape.rot.z = 12;
                if (this->isPowderKeg) {
                    gMergeShipPowderKegTimer = 0;
                    MergeShip_PlayMMSfxActor(thisx, MM_NA_SE_IT_BIG_BOMB_EXPLOSION);
                } else {
                    MergeShip_PlayMMSfxActor(thisx, MM_NA_SE_IT_BOMB_EXPLOSION);
                }
                MergeShipTest_Log("K keg explode pos=(%.0f,%.0f,%.0f) held=%d", thisx->world.pos.x,
                                  thisx->world.pos.y, thisx->world.pos.z, Actor_HasParent(thisx, play) ? 1 : 0);
            }
        }

        Actor_SetFocus(thisx, 20.0f);

        if (thisx->params <= MMENBOMKEG_BODY) {
            Collider_UpdateCylinder(thisx, &this->collider1);

            // if link is not holding the bomb anymore and bump conditions are met, subscribe to OC
            if (!Actor_HasParent(thisx, play) && (this->unk_1F8 != 0)) {
                CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider1.base);
            }
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider1.base);
        }

        if ((enBomScales[this->isPowderKeg] <= thisx->scale.x) && (thisx->params != MMENBOMKEG_EXPLOSION)) {
            // MM depthInWater is Ocarina of Time's yDistToWater (positive below the surface in both).
            if (thisx->yDistToWater >= 20.0f) {
                Vec3f effPos2;

                effPos2.x = thisx->world.pos.x;
                effPos2.y = thisx->world.pos.y + thisx->yDistToWater;
                effPos2.z = thisx->world.pos.z;
                EffectSsGRipple_Spawn(play, &effPos2, 70, 500, 0);
                EffectSsGRipple_Spawn(play, &effPos2, 70, 500, 10);
                effPos2.y += 10.0f;
                EffectSsGSplash_Spawn(play, &effPos2, NULL, NULL, 1, 500);
                if (gMergeShipActorId_MM_En_Clear_Tag >= 0) {
                    Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_MM_En_Clear_Tag, effPos2.x, effPos2.y,
                                effPos2.z, 0, 0, 1, MMCLEARTAG_PARAMS(MMCLEARTAG_SMOKE));
                }
                MMEnBomKeg_PlaySfxAtFixedWorldPos(play, &thisx->world.pos, MM_NA_SE_IT_BOMB_UNEXPLOSION);
                this->unk_1F4 = 0.0f;
                thisx->velocity.y = (KREG(83) * 0.1f) + -2.0f;
                thisx->gravity = (KREG(84) * 0.1f) + -0.5f;
                this->unk_1FC = (u8)(KREG(81) + 10);
                MergeShipTest_Log("K keg put out by water");
            } else if (thisx->bgCheckFlags & BGCHECKFLAG_WATER_TOUCH) {
                thisx->bgCheckFlags &= ~BGCHECKFLAG_WATER_TOUCH;
                MergeShip_PlayMMSfxActor(thisx, MM_NA_SE_EV_BOMB_DROP_WATER);
            }
        }
    }
}

static Vec3f D_80872EE0 = { 0.0f, 800.0f, 0.0f }; // the bomb's fuse (unused by the keg)
static Vec3f D_80872EEC = { -750.0f, 0.0f, 0.0f };
static Vec3f D_80872EF8 = { -800.0f, 0.0f, 0.0f };
static Vec3f D_80872F04 = { 0.0f, 0.0f, 0.0f };

// MM EnBom_Draw, the powder keg branch.
void MMEnBomKeg_Draw(Actor* thisx, PlayState* play) {
    s32 pad;
    MMEnBomKeg* this = (MMEnBomKeg*)thisx;

    (void)D_80872EE0;

    OPEN_DISPS(play->state.gfxCtx);

    if (this->actor.params == MMENBOMKEG_BODY) {
        Vec3f sp58;
        Vec3f sp4C;

        Gfx_SetupDL_25Opa(play->state.gfxCtx);

        Collider_UpdateSpheres(0, &this->collider2);

        if (this->unk_1FA != 0) {
            s16 sp4A = this->actor.world.rot.y - this->actor.shape.rot.y;
            f32 sp44 =
                (1000.0f / Math_CosS((s16)(MMKEG_ABS_ALT((s16)(this->unk_1FA % 10922)) - 0x1555))) + -1000.0f;

            // MM Matrix_RotateYS / Matrix_RotateXS (binang rotations).
            Matrix_RotateZYX(0, sp4A, 0, MTXMODE_APPLY);
            Matrix_Translate(0.0f, sp44, 0.0f, MTXMODE_APPLY);
            Matrix_RotateZYX(this->unk_1FA, 0, 0, MTXMODE_APPLY);
            Matrix_RotateZYX(0, (s16)-sp4A, 0, MTXMODE_APPLY);
        }

        Matrix_MultVec3f(&D_80872EEC, &this->actor.home.pos);
        Matrix_MultVec3f(&D_80872EF8, &sp58);
        Matrix_MultVec3f(&D_80872F04, &sp4C);

        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, mm_gPowderKegBarrelDL);
        gSPDisplayList(POLY_OPA_DISP++, mm_gPowderKegGoronSkullDL);

        MMEnBomKeg_UpdateFuse(play, &this->actor.home.pos, &sp58, &sp4C, this->timer);
        MMEnBomKeg_DrawFuse(play, this->timer);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM func_80872648
void MMEnBomKeg_InitFuse(PlayState* play, Vec3f* arg1) {
    MMPowderKegFuseSegment* fuseSegmentPtr = &sPowderKegFuseSegments[0];
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sPowderKegFuseSegments); i++, fuseSegmentPtr++) {
        Math_Vec3f_Copy(&fuseSegmentPtr->pos, arg1);
        Math_Vec3f_Copy(&fuseSegmentPtr->velocity, &sMMKegZeroVec);
        fuseSegmentPtr->rotY = 0;
        fuseSegmentPtr->rotX = 0x4000;
    }
}

// MM func_808726DC: the fuse's segments hang from the keg, 8 units each, one per 240 frames of fuse left.
void MMEnBomKeg_UpdateFuse(PlayState* play, Vec3f* arg1, Vec3f* arg2, Vec3f* arg3, s32 arg4) {
    s32 i;
    f32 temp_f20;
    Vec3f spCC;
    Vec3f spC0;
    MMPowderKegFuseSegment* fuseSegmentPtr = &sPowderKegFuseSegments[0];
    MMPowderKegFuseSegment* fuseSegmentPtr2 = &sPowderKegFuseSegments[1];
    f32 temp_f26 = Math_Vec3f_DistXYZ(arg3, arg1);
    s32 spB0;
    f32 temp_f2;
    f32 distXZ;

    Math_Vec3f_Copy(&fuseSegmentPtr->pos, arg1);
    Math_Vec3f_Diff(arg2, arg1, &spCC);

    fuseSegmentPtr->rotY = Math_Atan2S(spCC.z, spCC.x);
    distXZ = sqrtf(SQXZ(spCC));
    fuseSegmentPtr->rotX = Math_Atan2S(distXZ, spCC.y);

    spB0 = (arg4 / 240) + 1;

    for (i = 0; i < spB0; i++, fuseSegmentPtr++, fuseSegmentPtr2++, arg4 -= 240) {
        f32 phi_f22;
        CollisionPoly* spA0;
        s32 bgId;
        Vec3f sp90;

        if (arg4 >= 240) {
            phi_f22 = 8.0f;
        } else {
            phi_f22 = (arg4 % 240) * (1.0f / 240) * 8.0f;
        }

        Math_Vec3f_Sum(&fuseSegmentPtr2->pos, &fuseSegmentPtr2->velocity, &fuseSegmentPtr2->pos);
        temp_f20 = Math_Vec3f_DistXYZAndStoreDiff(arg3, &fuseSegmentPtr2->pos, &spCC);
        if (temp_f20 < temp_f26) {
            if (temp_f20 == 0.0f) {
                spCC.x = 0.0f;
                spCC.y = temp_f26;
                spCC.z = 0.0f;
            } else {
                temp_f20 = temp_f26 / temp_f20;
                spCC.x *= temp_f20;
                spCC.y *= temp_f20;
                spCC.z *= temp_f20;
            }
            Math_Vec3f_Sum(arg3, &spCC, &fuseSegmentPtr2->pos);
        }

        if (Math_Vec3f_DistXYZAndStoreDiff(&fuseSegmentPtr->pos, &fuseSegmentPtr2->pos, &spCC) == 0.0f) {
            spCC.x = 0.0f;
            spCC.y = phi_f22;
            spCC.z = 0.0f;
        }

        fuseSegmentPtr2->rotY = Math_Atan2S(spCC.z, spCC.x);
        distXZ = sqrtf(SQXZ(spCC));
        fuseSegmentPtr2->rotX = Math_Atan2S(distXZ, spCC.y);

        fuseSegmentPtr2->rotY =
            (s16)((s16)CLAMP(MMKEG_BINANG_SUB(fuseSegmentPtr2->rotY, fuseSegmentPtr->rotY), -8000, 8000) +
                  fuseSegmentPtr->rotY);
        fuseSegmentPtr2->rotX =
            (s16)((s16)CLAMP(MMKEG_BINANG_SUB(fuseSegmentPtr2->rotX, fuseSegmentPtr->rotX), -8000, 8000) +
                  fuseSegmentPtr->rotX);

        temp_f20 = Math_CosS(fuseSegmentPtr2->rotX) * phi_f22;
        spC0.x = Math_SinS(fuseSegmentPtr2->rotY) * temp_f20;
        spC0.z = Math_CosS(fuseSegmentPtr2->rotY) * temp_f20;
        spC0.y = Math_SinS(fuseSegmentPtr2->rotX) * phi_f22;

        Math_Vec3f_Sum(&fuseSegmentPtr->pos, &spC0, &fuseSegmentPtr2->pos);
        Math_Vec3f_Copy(&sp90, &fuseSegmentPtr2->pos);

        sp90.y += 50.0f;

        temp_f2 = BgCheck_EntityRaycastFloor3(&play->colCtx, &spA0, &bgId, &sp90) - fuseSegmentPtr2->pos.y;
        if (temp_f2 >= 0.0f) {
            spC0.y += temp_f2;
            if (phi_f22 < spC0.y) {
                spC0.y = phi_f22;
                temp_f2 = 0.0f;
            } else {
                temp_f2 = sqrtf(SQ(phi_f22) - SQ(spC0.y));
            }

            if (temp_f20 == 0.0f) {
                spC0.x = temp_f2;
            } else {
                temp_f2 /= temp_f20;
                spC0.x *= temp_f2;
                spC0.z *= temp_f2;
            }

            Math_Vec3f_Sum(&fuseSegmentPtr->pos, &spC0, &fuseSegmentPtr2->pos);
            Math_Vec3f_Copy(&fuseSegmentPtr2->velocity, &sMMKegZeroVec);
        }

        fuseSegmentPtr2->velocity.y += -1.0f;
        if (fuseSegmentPtr2->velocity.y < -10.0f) {
            fuseSegmentPtr2->velocity.y = -10.0f;
        }
    }

    Math_Vec3f_Copy(arg1, &fuseSegmentPtr->pos);
}

// 2S2H's gPowderKegFuseTexLarger (2ship.o2r, overlays/ovl_En_Bom): MM's 2x2 I8 gPowderKegFuseTex (a white row over a
// black row) repeated to 8x8, which Fast3D can sample. Not in mm.o2r, so its texels are embedded.
static ALIGN_ASSET(8) u8 sPowderKegFuseTexLarger[8 * 8] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// MM EnBom_DrawKeg (the fuse), as 2S2H draws it.
void MMEnBomKeg_DrawFuse(PlayState* play, s32 timer) {
    s32 temp_s5;
    s32 i;
    MMPowderKegFuseSegment* fuseSegmentPtr = &sPowderKegFuseSegments[0];
    MMPowderKegFuseSegment* fuseSegmentPtr2;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Translate(fuseSegmentPtr->pos.x, fuseSegmentPtr->pos.y, fuseSegmentPtr->pos.z, MTXMODE_NEW);
    Matrix_RotateZYX(fuseSegmentPtr->rotX, fuseSegmentPtr->rotY, 0, MTXMODE_APPLY);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    // 2S2H [Port]: gPowderKegFuseMaterialDL with the 8x8 texture (see sPowderKegFuseTexLarger).
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BOTH);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_LIGHTING);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIDECALA, G_CC_MODULATEI_PRIM2);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(POLY_OPA_DISP++, sPowderKegFuseTexLarger, G_IM_FMT_I, G_IM_SIZ_8b, 8, 8, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 1, 1, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 200, 200, 0, 255);
    gSPVertex(POLY_OPA_DISP++, (uintptr_t)mm_gPowderKegFuseVtx, 4, 0);

    temp_s5 = (timer / 240) + 1;
    fuseSegmentPtr2 = &sPowderKegFuseSegments[1];
    for (i = 1; i < temp_s5; i++, fuseSegmentPtr2++) {
        Matrix_Translate(fuseSegmentPtr2->pos.x, fuseSegmentPtr2->pos.y, fuseSegmentPtr2->pos.z, MTXMODE_NEW);
        Matrix_RotateZYX(fuseSegmentPtr2->rotX, fuseSegmentPtr2->rotY, 0, MTXMODE_APPLY);
        Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

        if ((i % 2) == 0) {
            gSPDisplayList(POLY_OPA_DISP++, mm_gPowderKegFuseModel1DL);
        } else {
            gSPDisplayList(POLY_OPA_DISP++, mm_gPowderKegFuseModel2DL);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
