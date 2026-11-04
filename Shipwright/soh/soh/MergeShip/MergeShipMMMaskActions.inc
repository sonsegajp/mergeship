// MergeShip2 -- Majora's Mask's masks that change what Link does (MM z_player.c), included at the end of
// z_player.c:
//
// * the B button's do-action while the Blast Mask, Bremen Mask or Kamaro's Mask is worn (Player_UpdateInterface's
//   human branch, Interface_SetBButtonPlayerDoAction, Player_GetItemOnButton's ITEM_F0 / ITEM_F1 / ITEM_F2);
// * the Blast Mask's explosion (Player_ProcessItemButtons, ITEM_F0);
// * the Bremen Mask's march (func_80839978, Player_Action_11) and Kamaro's dance (func_80839A10, Player_Action_12);
// * the Mask of Scents' sniffing idle (Player_GetIdleAnim).
//
// MM marks the dance with PLAYER_STATE2_2000000, which is Ocarina of Time's PLAYER_STATE2_PLAY_FOR_ACTOR (actors
// would take it for the ocarina); the dance keeps its own flag here instead. The march's PLAYER_STATE3_20000000
// is free in Ocarina of Time.

#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "seqcmd.h"

static u8 sMergeShipKamaroDancing = false;
static f32 sMergeShipMarchSpeed; // MM Player::unk_B48

s32 MergeShip_MMMasks_IsDancing(void) {
    return sMergeShipKamaroDancing;
}

// MM sequences for the two masks' fanfares (MergeShipAudio.cpp).
s32 MergeShip_MMAudio_PlayMMFanfare(s32 which);
#define MERGESHIP_MM_FANFARE_BREMEN_MARCH 0 // MM NA_BGM_BREMEN_MARCH
#define MERGESHIP_MM_FANFARE_KAMARO_DANCE 1 // MM NA_BGM_KAMARO_DANCE

// The MM do-action B shows for the mask being worn, or MM_DO_ACTION_UNDEFINED.
static s32 MergeShip_MMMasks_BDoAction(Player* this) {
    if (MERGESHIP_FORM_IS_MM(this->mergeShipForm) || !CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMMasks"), 1)) {
        return MM_DO_ACTION_UNDEFINED;
    }
    if (MERGESHIP_WEARS_MM_MASK(this, MM_MASK_BLAST)) {
        return MM_DO_ACTION_EXPLODE;
    }
    if (MERGESHIP_WEARS_MM_MASK(this, MM_MASK_BREMEN)) {
        return MM_DO_ACTION_MARCH;
    }
    if (MERGESHIP_WEARS_MM_MASK(this, MM_MASK_KAMARO)) {
        return MM_DO_ACTION_DANCE;
    }
    return MM_DO_ACTION_UNDEFINED;
}

// MM Player_UpdateInterface's PLAYER_FORM_HUMAN branch for B, through Interface_SetBButtonPlayerDoAction (only
// over a sword, nothing, or Deku Nuts on B -- MergeShip_SetMMBButtonDoActionForForm applies that rule).
void MergeShip_MMMasks_UpdateBDoAction(PlayState* play, Player* this) {
    s32 doActionB = MergeShip_MMMasks_BDoAction(this);

    if (doActionB > MM_DO_ACTION_UNDEFINED) {
        MergeShip_SetMMBButtonDoActionForForm(play, this, doActionB);
    }
}

// MM func_80839978: the Bremen Mask's march.
static void MergeShip_MMMasks_StartMarch(PlayState* play, Player* this);
// MM func_80839A10: Kamaro's dance.
static void MergeShip_MMMasks_StartDance(PlayState* play, Player* this);

// MM Player_ProcessItemButtons: B pressed while its label is the mask's (Player_GetItemOnButton returned
// ITEM_F0 / ITEM_F1 / ITEM_F2). Returns true when B was the mask's.
s32 MergeShip_MMMasks_UseBAction(PlayState* play, Player* this) {
    s32 doActionB;

    if (MergeShip_GetMMBButtonLabel() == NULL) {
        return false;
    }
    doActionB = MergeShip_MMMasks_BDoAction(this);
    if (doActionB == MM_DO_ACTION_EXPLODE) {
        // ITEM_F0
        if (gMergeShipBlastMaskTimer == 0) {
            EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, this->actor.focus.pos.x,
                                              this->actor.focus.pos.y, this->actor.focus.pos.z, 0, 0, 0, BOMB_BODY);

            if (bomb != NULL) {
                // OoT's EnBom places its blast sphere only while its body is drawn (EnBom_Draw's
                // Collider_UpdateSpheres); this bomb explodes on its first update, before any draw, so the sphere is
                // centred on the bomb here -- or the blast never reaches bomb walls, rocks or enemies.
                Sphere16* blast = &bomb->explosionCollider.elements[0].dim.worldSphere;

                blast->center.x = (s16)bomb->actor.world.pos.x;
                blast->center.y = (s16)bomb->actor.world.pos.y;
                blast->center.z = (s16)bomb->actor.world.pos.z;
                bomb->timer = 0;
                gMergeShipBlastMaskTimer = 310;
            }
        }
        return true;
    }
    if (doActionB == MM_DO_ACTION_MARCH) {
        MergeShip_MMMasks_StartMarch(play, this); // ITEM_F1
        return true;
    }
    if (doActionB == MM_DO_ACTION_DANCE) {
        MergeShip_MMMasks_StartDance(play, this); // ITEM_F2
        return true;
    }
    return false;
}

// MM Player_ProcessItemButtons' early returns: no item is used while marching or dancing.
s32 MergeShip_MMMasks_BlocksItems(Player* this) {
    return sMergeShipKamaroDancing || (this->stateFlags3 & PLAYER_STATE3_20000000);
}

// MM Player_Action_11: marching to the Bremen March for as long as B is held.
void MergeShip_MMMasks_Action_March(Player* this, PlayState* play) {
    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET; // MM PLAYER_STATE2_20

    if (this->linearVelocity < 1.0f) {
        this->skelAnime.animation = (LinkAnimationHeader*)mm_gPlayerAnim_clink_normal_okarina_walk;
    } else {
        this->skelAnime.animation = (LinkAnimationHeader*)mm_gPlayerAnim_clink_normal_okarina_walkB;
    }
    LinkAnimation_Update(play, &this->skelAnime);

    // (MM func_80847880 is its bow minigames' B ammo, which Ocarina of Time's galleries do not use.)
    if (!Player_TryActionHandlerList(play, this, sActionHandlerListIdle, true) ||
        (MergeShip_MMMasks_Action_March == this->actionFunc)) {
        f32 speedTarget;
        f32 phase;
        f32 halfLength;
        s16 yawTarget;
        s16 inputYaw;

        if (!CHECK_BTN_ALL(sControlInput->cur.button, BTN_B)) {
            func_8083C0E8(this, play); // MM func_80839E74 (MergeShip_MMMasks_SyncActionFlags then stops the march)
            return;
        }

        this->linearVelocity = sMergeShipMarchSpeed;
        Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_CURVED, play);
        inputYaw = yawTarget;

        if (!MergeShip_Goron_func_8083A4A4(this, &speedTarget, &yawTarget, REG(43) / 100.0f)) { // MM R_DECELERATE_RATE
            // MM func_8083CB04(this, speedTarget, yawTarget, REG(19) / 100.0f, 1.5f, 0x3E8)
            Math_AsymStepToF(&this->linearVelocity, speedTarget, REG(19) / 100.0f, 1.5f);
            Math_ScaledStepToS(&this->yaw, yawTarget, 0x3E8);
            func_8083DDC8(this, play); // MM func_8083C8E8
            if ((this->linearVelocity == 0.0f) && (speedTarget == 0.0f)) {
                this->yaw = inputYaw;
                this->actor.shape.rot.y = this->yaw;
            }
        }

        sMergeShipMarchSpeed = this->linearVelocity;
        phase = this->skelAnime.curFrame + 5.0f;
        halfLength = this->skelAnime.animLength / 2.0f;

        // effectively an fmodf
        phase -= halfLength * (s32)(phase / halfLength);
        this->linearVelocity *= Math_CosS(phase * 1000.0f) * 0.4f;
    }
}

// MM Player_Action_12: dancing Kamaro's dance for as long as B is held.
static void MergeShip_MMMasks_NpcDanceTick(void);

void MergeShip_MMMasks_Action_Dance(Player* this, PlayState* play) {
    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET; // MM PLAYER_STATE2_20
    MergeShip_MMMasks_NpcDanceTick();
    LinkAnimation_Update(play, &this->skelAnime);

    Player_DecelerateToZero(this);

    if (!Player_TryActionHandlerList(play, this, sActionHandlerListIdle, false) ||
        (MergeShip_MMMasks_Action_Dance == this->actionFunc)) {
        if (!CHECK_BTN_ALL(sControlInput->cur.button, BTN_B)) {
            func_8083C0E8(this, play); // MM func_80839E74 (MergeShip_MMMasks_SyncActionFlags then stops the dance)
        }
    }
}

static void MergeShip_MMMasks_StartMarch(PlayState* play, Player* this) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->itemAction = PLAYER_IA_OCARINA_FAIRY; // MM PLAYER_IA_OCARINA
        Player_SetupActionPreserveItemAction(play, this, MergeShip_MMMasks_Action_March, 0);
        Player_AnimPlayLoopAdjusted(play, this, (LinkAnimationHeader*)mm_gPlayerAnim_clink_normal_okarina_walk);
        Player_StartAnimMovement(play, this, 0x204); // MM Player_AnimReplace_Setup(ANIM_FLAG_4 | ANIM_FLAG_200)
        this->stateFlags3 |= PLAYER_STATE3_20000000;
        sMergeShipMarchSpeed = this->linearVelocity;
        MergeShip_MMAudio_PlayMMFanfare(MERGESHIP_MM_FANFARE_BREMEN_MARCH);
    }
}

// The NPC Link had Z-targeted when he started Kamaro's dance dances with him (Actor_Draw, MergeShip_MMMasks_NpcDance).
static Actor* sMergeShipKamaroPartner = NULL;
static s16 sMergeShipKamaroPartnerFrames = 0;
// Link's pose when the dance began (his skelAnime joint table), which his dance moves are measured from.
static Vec3s sMergeShipKamaroRestPose[PLAYER_LIMB_MAX];
// Set when a dance starts: the skeletons read for the last one may since have been unloaded.
static s32 sMergeShipKamaroNewDance = false;

static s32 MergeShip_MMMasks_ActorIsLive(PlayState* play, Actor* actor) {
    Actor* a;

    for (a = play->actorCtx.actorLists[ACTORCAT_NPC].head; a != NULL; a = a->next) {
        if (a == actor) {
            return true;
        }
    }
    return false;
}

static void MergeShip_MMMasks_StartDance(PlayState* play, Player* this) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        sMergeShipKamaroPartner =
            ((this->focusActor != NULL) && (this->focusActor->category == ACTORCAT_NPC)) ? this->focusActor : NULL;
        sMergeShipKamaroPartnerFrames = 0;
        memcpy(sMergeShipKamaroRestPose, this->skelAnime.jointTable, sizeof(sMergeShipKamaroRestPose));
        sMergeShipKamaroNewDance = true;
        this->itemAction = PLAYER_IA_NONE;
        Player_SetupActionPreserveItemAction(play, this, MergeShip_MMMasks_Action_Dance, 0);
        Player_AnimPlayLoopAdjusted(play, this, (LinkAnimationHeader*)mm_gPlayerAnim_alink_dance_loop);
        sMergeShipKamaroDancing = true; // MM PLAYER_STATE2_2000000
        MergeShip_MMAudio_PlayMMFanfare(MERGESHIP_MM_FANFARE_KAMARO_DANCE);
    }
}

// MM Player_GetIdleAnim: with the Mask of Scents on, Link sniffs (gPlayerAnim_cl_msbowait) instead of standing.
LinkAnimationHeader* MergeShip_MMMasks_IdleAnim(Player* this) {
    if (MERGESHIP_WEARS_MM_MASK(this, MM_MASK_SCENTS) && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMMasks"), 1)) {
        return (LinkAnimationHeader*)mm_gPlayerAnim_cl_msbowait;
    }
    return NULL;
}

// MM func_80831454, which MM's Player_SetAction runs on every change of action: leaving the march or the dance
// (letting go of B, a hit, a cutscene, a fall) stops its fanfare. The flags belong to the two actions, so they are
// cleared here too.
void MergeShip_MMMasks_SyncActionFlags(Player* this) {
    s32 wasPlaying = (this->stateFlags3 & PLAYER_STATE3_20000000) || sMergeShipKamaroDancing;

    if (this->actionFunc != MergeShip_MMMasks_Action_March) {
        this->stateFlags3 &= ~PLAYER_STATE3_20000000;
    }
    if (this->actionFunc != MergeShip_MMMasks_Action_Dance) {
        sMergeShipKamaroDancing = false;
        sMergeShipKamaroPartner = NULL;
    }
    if (wasPlaying && !(this->stateFlags3 & PLAYER_STATE3_20000000) && !sMergeShipKamaroDancing) {
        SEQCMD_STOP_SEQUENCE(SEQ_PLAYER_FANFARE, 0);
    }
}

// The partner's skeleton is being drawn (MergeShip_MMMasks_NpcDance .. MergeShip_MMMasks_NpcDanceEnd).
static s32 sMergeShipKamaroLimbs = false;
// This frame's dance, per Player limb: how far each of Link's joints has turned from sMergeShipKamaroRestPose.
static Vec3s sMergeShipKamaroMoves[PLAYER_LIMB_MAX];
static Player* sMergeShipKamaroDancer = NULL;
// The partner's last skeleton drawn had a body Link's dance maps onto; without one (a Deku Scrub, a cow) it dances
// the generic way.
static s32 sMergeShipKamaroPartnerHumanoid = false;

// Link's dance, retargeted: each skeleton the partner draws is read once for its legs, arms, head and the two
// joints they hang from, which then turn as Link's matching limbs turn (PLAYER_LIMB_*).
#define MS_DANCE_MAX_LIMBS 64
#define MS_DANCE_MAPS 8

typedef struct {
    void** skeleton;
    u8 playerLimb[MS_DANCE_MAX_LIMBS + 1]; // by the draw's limb index (the joint table's, from 1): a PLAYER_LIMB_*
    u8 humanoid;
} MsDanceMap;

typedef struct {
    u8 count;
    u8 child[MS_DANCE_MAX_LIMBS];
    u8 sibling[MS_DANCE_MAX_LIMBS];
    u8 parent[MS_DANCE_MAX_LIMBS];
    u8 subtree[MS_DANCE_MAX_LIMBS];
    Vec3s pos[MS_DANCE_MAX_LIMBS];
} MsDanceSkeleton;

typedef struct {
    u8 hub;
    u8 len;
    u8 a[3];
    u8 b[3];
} MsDancePair;

static MsDanceMap sMsDanceMaps[MS_DANCE_MAPS];
static s32 sMsDanceMapNext = 0;

static s32 MsDance_Read(void** skeleton, MsDanceSkeleton* skel) {
    u8 stack[MS_DANCE_MAX_LIMBS];
    s32 depth = 0;
    s32 i;

    memset(skel, 0, sizeof(*skel));
    memset(skel->parent, LIMB_DONE, sizeof(skel->parent));
    // Every limb a draw can reach, from limb 0 through child / sibling.
    stack[depth++] = 0;
    while (depth > 0) {
        u8 n = stack[--depth];
        StandardLimb* limb;

        if ((n >= MS_DANCE_MAX_LIMBS) || (skeleton[n] == NULL)) {
            return false;
        }
        limb = (StandardLimb*)SEGMENTED_TO_VIRTUAL(skeleton[n]);
        skel->child[n] = limb->child;
        skel->sibling[n] = limb->sibling;
        skel->pos[n] = limb->jointPos;
        if (n + 1 > skel->count) {
            skel->count = n + 1;
        }
        if (limb->sibling != LIMB_DONE) {
            if ((limb->sibling >= MS_DANCE_MAX_LIMBS) || (depth >= MS_DANCE_MAX_LIMBS)) {
                return false;
            }
            skel->parent[limb->sibling] = skel->parent[n];
            stack[depth++] = limb->sibling;
        }
        if (limb->child != LIMB_DONE) {
            if ((limb->child >= MS_DANCE_MAX_LIMBS) || (depth >= MS_DANCE_MAX_LIMBS)) {
                return false;
            }
            skel->parent[limb->child] = n;
            stack[depth++] = limb->child;
        }
    }
    for (i = 0; i < skel->count; i++) {
        u8 n = (u8)i;

        // Each limb counts once in its own subtree and in every ancestor's.
        while (n != LIMB_DONE) {
            skel->subtree[n]++;
            n = skel->parent[n];
        }
    }
    return true;
}

static s32 MsDance_ChildCount(MsDanceSkeleton* skel, u8 n) {
    s32 count = 0;
    u8 c;

    for (c = skel->child[n]; c != LIMB_DONE; c = skel->sibling[c]) {
        count++;
    }
    return count;
}

// A limb and the limbs hanging below it one by one (thigh, shin, foot; shoulder, forearm, hand), up to three.
static s32 MsDance_Chain(MsDanceSkeleton* skel, u8 start, u8* out) {
    s32 len = 0;
    u8 n = start;

    while ((n != LIMB_DONE) && (len < 3)) {
        out[len++] = n;
        if (MsDance_ChildCount(skel, n) != 1) {
            break;
        }
        n = skel->child[n];
    }
    return len;
}

static f32 MsDance_Length(Vec3s* v) {
    return sqrtf(((f32)v->x * v->x) + ((f32)v->y * v->y) + ((f32)v->z * v->z));
}

// The two limbs of a hub that mirror each other: chains of the same length whose joints sit the same distance from
// the hub, on its two sides.
static s32 MsDance_BestPair(MsDanceSkeleton* skel, u8 hub, MsDancePair* pair) {
    u8 starts[MS_DANCE_MAX_LIMBS];
    u8 lens[MS_DANCE_MAX_LIMBS];
    u8 chains[MS_DANCE_MAX_LIMBS][3];
    s32 num = 0;
    f32 best = 1e30f;
    s32 found = false;
    s32 i;
    s32 j;
    u8 c;

    for (c = skel->child[hub]; (c != LIMB_DONE) && (num < MS_DANCE_MAX_LIMBS); c = skel->sibling[c]) {
        s32 len = MsDance_Chain(skel, c, chains[num]);

        if (len >= 2) {
            starts[num] = c;
            lens[num] = (u8)len;
            num++;
        }
    }
    for (i = 0; i < num; i++) {
        for (j = i + 1; j < num; j++) {
            f32 score;

            if ((skel->pos[starts[i]].x == skel->pos[starts[j]].x) &&
                (skel->pos[starts[i]].y == skel->pos[starts[j]].y) &&
                (skel->pos[starts[i]].z == skel->pos[starts[j]].z)) {
                continue;
            }
            score = (ABS(lens[i] - lens[j]) * 100000.0f) - (MIN(lens[i], lens[j]) * 10000.0f) +
                    fabsf(MsDance_Length(&skel->pos[starts[i]]) - MsDance_Length(&skel->pos[starts[j]]));
            if (score < best) {
                best = score;
                pair->hub = hub;
                pair->len = MIN(lens[i], lens[j]);
                memcpy(pair->a, chains[i], 3);
                memcpy(pair->b, chains[j], 3);
                found = true;
            }
        }
    }
    return found;
}

// Which of a pair is on the positive side of the axis the two sit furthest apart on (the same test for Link's
// skeleton and the partner's, so their left and right line up).
static s32 MsDance_AIsPositive(Vec3s* a, Vec3s* b) {
    s32 dx = a->x - b->x;
    s32 dy = a->y - b->y;
    s32 dz = a->z - b->z;

    if ((ABS(dx) >= ABS(dy)) && (ABS(dx) >= ABS(dz))) {
        return dx > 0;
    }
    if (ABS(dy) >= ABS(dz)) {
        return dy > 0;
    }
    return dz > 0;
}

static void MsDance_MapChain(MsDanceMap* map, u8* chain, s32 len, u8 limb0) {
    s32 i;

    for (i = 0; i < len; i++) {
        map->playerLimb[chain[i] + 1] = (u8)(limb0 + i);
    }
}

// A pair onto Link's pair (PLAYER_LIMB_R_THIGH / L_THIGH, or L_SHOULDER / R_SHOULDER), matched side to side.
static void MsDance_MapPair(MsDanceMap* map, MsDanceSkeleton* skel, MsDancePair* pair, u8 linkA, u8 linkB) {
    StandardLimb* limbA = (StandardLimb*)SEGMENTED_TO_VIRTUAL(sMergeShipKamaroDancer->skelAnime.skeleton[linkA - 1]);
    StandardLimb* limbB = (StandardLimb*)SEGMENTED_TO_VIRTUAL(sMergeShipKamaroDancer->skelAnime.skeleton[linkB - 1]);
    s32 linkAPositive = MsDance_AIsPositive(&limbA->jointPos, &limbB->jointPos);
    s32 npcAPositive = MsDance_AIsPositive(&skel->pos[pair->a[0]], &skel->pos[pair->b[0]]);

    MsDance_MapChain(map, pair->a, pair->len, (linkAPositive == npcAPositive) ? linkA : linkB);
    MsDance_MapChain(map, pair->b, pair->len, (linkAPositive == npcAPositive) ? linkB : linkA);
}

static void MsDance_BuildMap(void** skeleton, MsDanceMap* map) {
    static MsDanceSkeleton skel;
    MsDancePair pairs[MS_DANCE_MAX_LIMBS];
    MsDancePair* legs;
    MsDancePair* arms;
    s32 numPairs = 0;
    s32 i;
    u8 c;
    u8 head = LIMB_DONE;

    memset(map, 0, sizeof(*map));
    map->skeleton = skeleton;
    if (!MsDance_Read(skeleton, &skel)) {
        return;
    }
    for (i = 0; i < skel.count; i++) {
        if (MsDance_BestPair(&skel, (u8)i, &pairs[numPairs])) {
            numPairs++;
        }
    }
    if (numPairs < 2) {
        return;
    }
    // The two biggest pairs are the legs and the arms (smaller ones are fingers, skirts, ears).
    for (i = 0; i < 2; i++) {
        s32 j;
        s32 best = i;

        for (j = i + 1; j < numPairs; j++) {
            if ((pairs[j].len > pairs[best].len) ||
                ((pairs[j].len == pairs[best].len) && (skel.subtree[pairs[j].hub] > skel.subtree[pairs[best].hub]))) {
                best = j;
            }
        }
        if (best != i) {
            MsDancePair swap = pairs[i];

            pairs[i] = pairs[best];
            pairs[best] = swap;
        }
    }
    legs = &pairs[0];
    arms = &pairs[1];
    {
        // The legs hang from the hub nearer the root (the arms' hub, the chest, hangs below it), or, side by side
        // like Link's LOWER and UPPER, from the hub with fewer limbs (the chest carries the head too).
        s32 armsBelowLegs = false;
        s32 legsBelowArms = false;
        u8 n;

        for (n = skel.parent[arms->hub]; n != LIMB_DONE; n = skel.parent[n]) {
            armsBelowLegs |= (n == legs->hub);
        }
        for (n = skel.parent[legs->hub]; n != LIMB_DONE; n = skel.parent[n]) {
            legsBelowArms |= (n == arms->hub);
        }
        if (legsBelowArms ||
            (!armsBelowLegs && (MsDance_ChildCount(&skel, legs->hub) > MsDance_ChildCount(&skel, arms->hub)))) {
            MsDancePair* swap = legs;

            legs = arms;
            arms = swap;
        }
    }
    // The head: the chest's biggest other limb.
    for (c = skel.child[arms->hub]; c != LIMB_DONE; c = skel.sibling[c]) {
        if ((c != arms->a[0]) && (c != arms->b[0]) && ((head == LIMB_DONE) || (skel.subtree[c] > skel.subtree[head]))) {
            head = c;
        }
    }
    MsDance_MapPair(map, &skel, legs, PLAYER_LIMB_R_THIGH, PLAYER_LIMB_L_THIGH);
    MsDance_MapPair(map, &skel, arms, PLAYER_LIMB_L_SHOULDER, PLAYER_LIMB_R_SHOULDER);
    if (head != LIMB_DONE) {
        map->playerLimb[head + 1] = PLAYER_LIMB_HEAD;
    }
    map->playerLimb[arms->hub + 1] = PLAYER_LIMB_UPPER;
    if (legs->hub != arms->hub) {
        map->playerLimb[legs->hub + 1] = PLAYER_LIMB_LOWER;
    }
    map->humanoid = true;
}

static MsDanceMap* MsDance_GetMap(void** skeleton) {
    s32 i;

    for (i = 0; i < MS_DANCE_MAPS; i++) {
        if (sMsDanceMaps[i].skeleton == skeleton) {
            return &sMsDanceMaps[i];
        }
    }
    MsDance_BuildMap(skeleton, &sMsDanceMaps[sMsDanceMapNext]);
    i = sMsDanceMapNext;
    sMsDanceMapNext = (sMsDanceMapNext + 1) % MS_DANCE_MAPS;
    return &sMsDanceMaps[i];
}

// Actor_Draw, between the actor's position/rotation and its scale: while Link dances Kamaro's dance, the NPC he had
// Z-targeted dances it with him on its own body -- its legs, arms, head and body turn as Link's do
// (MergeShip_MMMasks_DanceLimb) and it bobs as he bobs. Its own interpolation child, so it never shifts the actor's.
void MergeShip_MMMasks_NpcDance(PlayState* play, Actor* actor) {
    Player* player = GET_PLAYER(play);
    f32 t;
    s32 i;

    if ((actor == NULL) || (actor != sMergeShipKamaroPartner) || !sMergeShipKamaroDancing) {
        return;
    }
    if (!MergeShip_MMMasks_ActorIsLive(play, actor)) {
        sMergeShipKamaroPartner = NULL;
        return;
    }
    if (sMergeShipKamaroNewDance || (sMergeShipKamaroDancer != player)) {
        sMergeShipKamaroNewDance = false;
        sMergeShipKamaroDancer = player;
        sMergeShipKamaroPartnerHumanoid = false;
        memset(sMsDanceMaps, 0, sizeof(sMsDanceMaps));
    }
    for (i = 0; i < PLAYER_LIMB_MAX; i++) {
        sMergeShipKamaroMoves[i].x = player->skelAnime.jointTable[i].x - sMergeShipKamaroRestPose[i].x;
        sMergeShipKamaroMoves[i].y = player->skelAnime.jointTable[i].y - sMergeShipKamaroRestPose[i].y;
        sMergeShipKamaroMoves[i].z = player->skelAnime.jointTable[i].z - sMergeShipKamaroRestPose[i].z;
    }
    t = (f32)sMergeShipKamaroPartnerFrames;
    FrameInterpolation_RecordOpenChild(&sMergeShipKamaroPartner, 0);
    if (sMergeShipKamaroPartnerHumanoid) {
        // Link's root rises and sinks with the dance's steps (joint table 0 is the root's position).
        Matrix_Translate(0.0f, sMergeShipKamaroMoves[0].y * player->actor.scale.y, 0.0f, MTXMODE_APPLY);
    } else {
        Matrix_Translate(0.0f, 6.0f * fabsf(Math_SinF(t * 0.33f)), 0.0f, MTXMODE_APPLY);
        Matrix_RotateY(0.5f * Math_SinF(t * 0.165f), MTXMODE_APPLY);
        Matrix_RotateZ(0.15f * Math_SinF(t * 0.165f), MTXMODE_APPLY);
    }
    FrameInterpolation_RecordCloseChild();
    sMergeShipKamaroLimbs = true;
}

// Actor_Draw, after the actor's draw.
void MergeShip_MMMasks_NpcDanceEnd(void) {
    sMergeShipKamaroLimbs = false;
}

// z_skelanime.c, each limb below the root of a skeleton being drawn (limbIndex as the joint table counts it, from
// 1): the dance partner's limb turns as Link's matching limb turns. A skeleton with no legs and arms to match
// swings its limbs to the beat instead, neighbouring limbs the opposite way.
void MergeShip_MMMasks_DanceLimb(void** skeleton, s32 limbIndex, Vec3s* rot) {
    MsDanceMap* map;
    f32 t;

    if (!sMergeShipKamaroLimbs || (sMergeShipKamaroDancer == NULL)) {
        return;
    }
    map = MsDance_GetMap(skeleton);
    sMergeShipKamaroPartnerHumanoid = map->humanoid;
    if (map->humanoid) {
        if ((limbIndex >= 1) && (limbIndex <= MS_DANCE_MAX_LIMBS) && (map->playerLimb[limbIndex] != 0)) {
            Vec3s* move = &sMergeShipKamaroMoves[map->playerLimb[limbIndex]];

            rot->x += move->x;
            rot->y += move->y;
            rot->z += move->z;
        }
        return;
    }
    t = (f32)sMergeShipKamaroPartnerFrames;
    rot->z += (s16)(0x1000 * Math_SinF((t * 0.165f) + ((limbIndex & 1) ? 3.14159f : 0.0f)));
    rot->x += (s16)(0x0800 * Math_SinF((t * 0.33f) + (limbIndex * 0.9f)));
    rot->y += (s16)(0x0A00 * Math_SinF((t * 0.165f) + (limbIndex * 0.5f)));
}

// Once per frame from the dance action: the partner's beat.
static void MergeShip_MMMasks_NpcDanceTick(void) {
    if (sMergeShipKamaroPartner != NULL) {
        sMergeShipKamaroPartnerFrames++;
    }
}
