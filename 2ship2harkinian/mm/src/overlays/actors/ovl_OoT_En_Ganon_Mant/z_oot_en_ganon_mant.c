/*
 * File: z_oot_en_ganon_mant.c
 * Overlay: ovl_OoT_En_Ganon_Mant
 * Description: MergeShip2 -- Ocarina of Time's Ganondorf's Cape (OoT En_Ganon_Mant), ported to Majora's Mask for
 *              Ganondorf mode in Termina (2s2h/MergeShip/MergeShipGanondorf.c spawns it as the player's child and
 *              feeds it the points on his arms each frame, as BossGanon_Draw does).
 *
 * Source reference: Shipwright/soh/src/overlays/actors/ovl_En_Ganon_Mant/z_en_ganon_mant.c (OoT), with the player's
 * joint length SoH's Ganondorf mode uses. Its assets are Ocarina of Time's (the archive mounted under oot/).
 */

#include "z_oot_en_ganon_mant.h"
#include "2s2h/MergeShip/MergeShipGanondorf.h" // MergeShip2: Ganondorf mode (his cape's colour)
#include "2s2h/MergeShip/MergeShipAssets.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

#define MANT_PI 3.14159265358979323846f

void OoT_En_Ganon_Mant_Init(Actor* thisx, PlayState* play);
void OoT_En_Ganon_Mant_Destroy(Actor* thisx, PlayState* play);
void OoT_En_Ganon_Mant_Update(Actor* thisx, PlayState* play);
void OoT_En_Ganon_Mant_Draw(Actor* thisx, PlayState* play);

ActorProfile OoT_En_Ganon_Mant_Profile = {
    /**/ ACTOR_OOT_EN_GANON_MANT,
    /**/ ACTORCAT_MISC,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(OoTEnGanonMant),
    /**/ OoT_En_Ganon_Mant_Init,
    /**/ OoT_En_Ganon_Mant_Destroy,
    /**/ OoT_En_Ganon_Mant_Update,
    /**/ OoT_En_Ganon_Mant_Draw,
};

static const ALIGN_ASSET(2) char sOoTMantTex[] = OOT_ASSET("overlays/ovl_En_Ganon_Mant/gMantTex");
static const ALIGN_ASSET(2) char sOoTMant1Vtx[] = OOT_ASSET("overlays/ovl_En_Ganon_Mant/gMant1Vtx");
static const ALIGN_ASSET(2) char sOoTMant2Vtx[] = OOT_ASSET("overlays/ovl_En_Ganon_Mant/gMant2Vtx");
static const ALIGN_ASSET(2) char sOoTMantMaterialDL[] = OOT_ASSET("overlays/ovl_En_Ganon_Mant/gMantMaterialDL");
static const ALIGN_ASSET(2) char sOoTMantDL[] = OOT_ASSET("overlays/ovl_En_Ganon_Mant/gMantDL");

// (2s2h/BenPort.h: the resources' own vertices, and the cloth's tear mask over its texture)
Vtx* ResourceMgr_LoadVtxByName(char* path);
void Gfx_RegisterBlendedTexture(const char* name, u8* mask, u8* replacement);

// 2s2h/MergeShip/MergeShipGanondorfDL.cpp: a list with the cape's red recoloured (null: as it is)
Gfx* MergeShip_Ganondorf_ColorDL(const char* path, u32 armor, u32 cape);

static s16 sTearSizesMedium[] = {
    0, 0, 0, 1, 1, 1, 1, 1, 0, 0,
};

static s16 sTearSizesLarge[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 2, 1, 1, 0, 0, 0, 0,
};

static s16 sTearSizesSmall[] = {
    0, 0, 0, 0, 0, 0, 0,
};

typedef struct {
    s16* tearAreaSizes;
    s16 count;
} TearShape;

static TearShape sTearShapes[] = {
    { sTearSizesMedium, ARRAY_COUNT(sTearSizesMedium) },
    { sTearSizesMedium, ARRAY_COUNT(sTearSizesMedium) },
    { sTearSizesLarge, ARRAY_COUNT(sTearSizesLarge) },
    { sTearSizesSmall, ARRAY_COUNT(sTearSizesSmall) },
};

// How much each joint is affected by backwards/forwards swaying motion
static f32 sBackSwayCoefficients[GANON_MANT_NUM_JOINTS] = {
    0.0f, 1.0f, 0.5f, 0.25f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
};

// How much each joint is affected by sideways swaying motion, tends to 0
static f32 sSideSwayCoefficients[GANON_MANT_NUM_JOINTS] = {
    0.0f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, 0.0f,
};

static f32 sDistMultipliers[GANON_MANT_NUM_JOINTS] = {
    0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f,
};

#define MAP_STRAND_TO_VTX(n)                                                                               \
    (n) + GANON_MANT_NUM_JOINTS * 0, (n) + GANON_MANT_NUM_JOINTS * 1, (n) + GANON_MANT_NUM_JOINTS * 2,     \
        (n) + GANON_MANT_NUM_JOINTS * 3, (n) + GANON_MANT_NUM_JOINTS * 4, (n) + GANON_MANT_NUM_JOINTS * 5, \
        (n) + GANON_MANT_NUM_JOINTS * 6, (n) + GANON_MANT_NUM_JOINTS * 7, (n) + GANON_MANT_NUM_JOINTS * 8, \
        (n) + GANON_MANT_NUM_JOINTS * 9, (n) + GANON_MANT_NUM_JOINTS * 10, (n) + GANON_MANT_NUM_JOINTS * 11

static u16 sVerticesMap[GANON_MANT_NUM_STRANDS * GANON_MANT_NUM_JOINTS] = {
    MAP_STRAND_TO_VTX(11), MAP_STRAND_TO_VTX(10), MAP_STRAND_TO_VTX(9), MAP_STRAND_TO_VTX(8),
    MAP_STRAND_TO_VTX(7),  MAP_STRAND_TO_VTX(6),  MAP_STRAND_TO_VTX(5), MAP_STRAND_TO_VTX(4),
    MAP_STRAND_TO_VTX(3),  MAP_STRAND_TO_VTX(2),  MAP_STRAND_TO_VTX(1), MAP_STRAND_TO_VTX(0),
};

#define MANT_TEX_WIDTH 32
#define MANT_TEX_HEIGHT 64

static u8 sMaskTex[MANT_TEX_WIDTH * MANT_TEX_HEIGHT] = { 0 };

void OoT_En_Ganon_Mant_Init(Actor* thisx, PlayState* play) {
    OoTEnGanonMant* this = (OoTEnGanonMant*)thisx;
    s32 i;

    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    for (i = 0; i < ARRAY_COUNT(sMaskTex); i++) {
        sMaskTex[i] = 0;
    }
    Gfx_RegisterBlendedTexture(sOoTMantTex, sMaskTex, NULL);
}

void OoT_En_Ganon_Mant_Destroy(Actor* thisx, PlayState* play) {
}

// Randomly zeros portions of the cloak texture
static void OoTEnGanonMant_Tear(OoTEnGanonMant* this) {
    s16 i;
    s16 areaX;
    s16 areaY;
    s16 texIdx;
    f32 tx = Rand_ZeroFloat(MANT_TEX_WIDTH);
    f32 ty = Rand_ZeroFloat(MANT_TEX_HEIGHT);
    f32 tearAngle = Rand_ZeroFloat(2 * MANT_PI);
    f32 tearDirX = sinf(tearAngle);
    f32 tearDirY = cosf(tearAngle);
    TearShape* shape = &sTearShapes[(s16)Rand_ZeroFloat(ARRAY_COUNT(sTearShapes) - 0.01f)];
    s16 count = shape->count;
    s16* tearAreaSizes = shape->tearAreaSizes;

    for (i = 0; i < count; i++) {
        if ((0 <= tx && tx < MANT_TEX_WIDTH) && (0 <= ty && ty < MANT_TEX_HEIGHT)) {
            for (areaX = 0; areaX <= tearAreaSizes[i]; areaX++) {
                for (areaY = 0; areaY <= tearAreaSizes[i]; areaY++) {
                    texIdx = (s16)((s16)tx + ((s16)ty * MANT_TEX_WIDTH)) + ((s16)areaX + ((s16)areaY * MANT_TEX_WIDTH));
                    if (texIdx < MANT_TEX_WIDTH * MANT_TEX_HEIGHT) {
                        sMaskTex[texIdx] = 1;
                    }
                }
            }
        }
        tx += tearDirX;
        ty += tearDirY;
    }

    for (i = 0; i < 4; i++) {
        this->strands[(s16)Rand_ZeroFloat(GANON_MANT_NUM_STRANDS - 0.1f)]
            .torn[(s16)Rand_ZeroFloat(GANON_MANT_NUM_JOINTS - 0.1f)] = true;
    }
}

// Updates the dynamic strands that control the shape and motion of the cloak
static void OoTEnGanonMant_UpdateStrand(PlayState* play, OoTEnGanonMant* this, Vec3f* root, Vec3f* pos,
                                        Vec3f* nextPos, Vec3f* rot, Vec3f* vel, s16 strandNum) {
    f32 xDiff;
    f32 zDiff;
    f32 gravity;
    f32 yaw;
    s16 i;
    f32 x;
    f32 y;
    f32 z;
    // (SoH's Ganondorf mode: he is about two thirds of BossGanon's height, so the cloak hangs to his ankles)
    f32 jointLength = 5.8f;
    Vec3f delta;
    Vec3f posStep;
    Vec3f backSwayOffset;
    Vec3f sideSwayOffset;

    delta.y = 0;
    for (i = 0; i < GANON_MANT_NUM_JOINTS; i++, pos++, vel++, rot++, nextPos++) {
        if (i == 0) {
            // Constraint: first position is always root
            pos->x = root->x;
            pos->y = root->y;
            pos->z = root->z;
        } else {
            // Decelerate
            Math_ApproachZeroF(&vel->x, 1.0f, 0.1f);
            Math_ApproachZeroF(&vel->y, 1.0f, 0.1f);
            Math_ApproachZeroF(&vel->z, 1.0f, 0.1f);

            // Push the cloak away from attached actor, plus oscillations
            delta.x = 0;
            delta.z = (this->backPush + (sinf((strandNum * (2 * MANT_PI)) / 2.1f) * this->backSwayMagnitude)) *
                      sBackSwayCoefficients[i];
            Matrix_RotateYF(this->baseYaw, MTXMODE_NEW);
            Matrix_MultVec3f(&delta, &backSwayOffset);

            // Push the cloak out to either side, in a swaying manner
            delta.x = cosf((strandNum * MANT_PI) / (GANON_MANT_NUM_STRANDS - 1.0f)) * this->sideSwayMagnitude *
                      sSideSwayCoefficients[i];
            delta.z = 0;
            Matrix_MultVec3f(&delta, &sideSwayOffset);

            // Calculate position difference
            gravity = this->gravity;
            x = ((pos->x + vel->x) - (pos - 1)->x) + (backSwayOffset.x + sideSwayOffset.x);
            y = ((pos->y + vel->y) - (pos - 1)->y) + gravity;
            z = ((pos->z + vel->z) - (pos - 1)->z) + (backSwayOffset.z + sideSwayOffset.z);

            // Calculate rotations in the direction of the position difference
            yaw = Math_Atan2F(x, z);
            x = -Math_Atan2F(y, sqrtf(SQ(x) + SQ(z)));
            (rot - 1)->x = x;

            // Calculate real position difference of correct length in the correct direction
            delta.x = 0;
            delta.z = jointLength;
            Matrix_RotateYF(yaw, MTXMODE_NEW);
            Matrix_RotateXFApply(x);
            Matrix_MultVec3f(&delta, &posStep);

            // Save position
            x = pos->x;
            y = pos->y;
            z = pos->z;

            // Calculate next position
            pos->x = (pos - 1)->x + posStep.x;
            pos->y = (pos - 1)->y + posStep.y;
            pos->z = (pos - 1)->z + posStep.z;

            // Pushes the cloak away from the actor if it is too close
            xDiff = pos->x - this->actor.world.pos.x;
            zDiff = pos->z - this->actor.world.pos.z;
            if (sqrtf(SQ(xDiff) + SQ(zDiff)) < (sDistMultipliers[i] * this->minDist)) {
                yaw = Math_Atan2F(xDiff, zDiff);
                delta.z = this->minDist * sDistMultipliers[i];
                Matrix_RotateYF(yaw, MTXMODE_NEW);
                Matrix_MultVec3f(&delta, &posStep);
                pos->x = this->actor.world.pos.x + posStep.x;
                pos->z = this->actor.world.pos.z + posStep.z;
            }

            // Enforces minY constraint
            if (pos->y < this->minY) {
                pos->y = this->minY;
            }

            // Calculate next velocity
            vel->x = (pos->x - x) * 80.0f / 100.0f;
            vel->y = (pos->y - y) * 80.0f / 100.0f;
            vel->z = (pos->z - z) * 80.0f / 100.0f;

            // Clamp elements of vel into [-5.0, 5.0]
            vel->x = CLAMP(vel->x, -5.0f, 5.0f);
            vel->y = CLAMP(vel->y, -5.0f, 5.0f);
            vel->z = CLAMP(vel->z, -5.0f, 5.0f);

            // update angle
            xDiff = pos->x - nextPos->x;
            zDiff = pos->z - nextPos->z;
            (rot - 1)->y = Math_Atan2F(xDiff, zDiff);
        }
    }
    rot[11].y = rot[10].y;
    rot[11].x = rot[10].x;
}

// Update the cloak vertices using the current state of the strands
static void OoTEnGanonMant_UpdateVertices(OoTEnGanonMant* this) {
    s16 i;
    Vtx* vtx;
    Vtx* vertices;
    OoTMantStrand* strand;
    s16 j;
    s16 k;
    Vec3f up;
    Vec3f normal;

    vertices = ResourceMgr_LoadVtxByName((char*)((this->frameTimer % 2 != 0) ? sOoTMant1Vtx : sOoTMant2Vtx));
    if (vertices == NULL) {
        return;
    }

    up.x = 0.0f;
    up.y = 30.0f;
    up.z = 0.0f;

    strand = &this->strands[0];
    for (i = 0; i < GANON_MANT_NUM_STRANDS; i++, strand++) {
        for (j = 0, k = 0; j < GANON_MANT_NUM_JOINTS; j++, k += GANON_MANT_NUM_JOINTS) {
            vtx = &vertices[sVerticesMap[i + k]];
            vtx->n.ob[0] = strand->joints[j].x;
            vtx->n.ob[1] = strand->joints[j].y;
            vtx->n.ob[2] = strand->joints[j].z;
            Matrix_RotateYF(strand->rotations[j].y, MTXMODE_NEW);
            Matrix_RotateXFApply(strand->rotations[j].x);
            Matrix_MultVec3f(&up, &normal);
            vtx->n.n[0] = normal.x;
            vtx->n.n[1] = normal.y;
            vtx->n.n[2] = normal.z;
        }
    }
}

void OoT_En_Ganon_Mant_Update(Actor* thisx, PlayState* play) {
    OoTEnGanonMant* this = (OoTEnGanonMant*)thisx;

    this->updateHasRun = true;
    this->frameTimer++;

    if (this->attachLeftArmTimer != 0.0f) {
        this->attachLeftArmTimer -= 1.0f;
    }
    if (this->attachRightArmTimer != 0.0f) {
        this->attachRightArmTimer -= 1.0f;
    }
    if (this->attachShouldersTimer != 0.0f) {
        this->attachShouldersTimer -= 1.0f;
    }

    if (this->actor.parent != NULL) {
        this->actor.shape.rot.y = this->actor.parent->shape.rot.y;
    }

    if (this->tearTimer != 0) {
        this->tearTimer--;
        OoTEnGanonMant_Tear(this);
    }
}

static void OoTEnGanonMant_DrawCloak(PlayState* play, OoTEnGanonMant* this) {
    Gfx* colored = NULL;
    Vtx* vertices;

    OPEN_DISPS(play->state.gfxCtx);

    // Invalidate cape texture mask as it may have been torn
    gSPInvalidateTexCache(POLY_OPA_DISP++, (uintptr_t)sMaskTex);

    Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
    gSPMatrix(POLY_OPA_DISP++, Matrix_Finalize(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    // set texture (MergeShip2: in the player's cape colour)
    // (the player's: his colour from the menu, as SoH's EnGanonMant_Draw; the user saw it red in Termina, 2026-10-17)
    if ((this->actor.parent != NULL) && (this->actor.parent->id == ACTOR_PLAYER)) {
        this->capeColor = MergeShip_Ganondorf_CapeColor();
    }
    if (this->capeColor != 0) {
        colored = MergeShip_Ganondorf_ColorDL(sOoTMantMaterialDL, 0, this->capeColor);
    }
    gSPDisplayList(POLY_OPA_DISP++, (colored != NULL) ? colored : (Gfx*)sOoTMantMaterialDL);

    // set vertices, vertices are double buffered to prevent modification of vertices as they are being drawn
    vertices = ResourceMgr_LoadVtxByName((char*)((this->frameTimer % 2 != 0) ? sOoTMant1Vtx : sOoTMant2Vtx));
    if (vertices != NULL) {
        gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)vertices);
        // draw cloak
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sOoTMantDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void OoT_En_Ganon_Mant_Draw(Actor* thisx, PlayState* play) {
    OoTEnGanonMant* this = (OoTEnGanonMant*)thisx;
    f32 xDiff;
    f32 pitch;
    Vec3f strandOffset;
    Vec3f strandDivPos;
    f32 zDiff;
    f32 diffHalfDist;
    f32 yDiff;
    f32 yaw;
    Vec3f* rightPos;
    Vec3f* leftPos;
    s16 strandIdx;
    Vec3f midpoint;
    s16 nextStrandIdx;

    if (this->updateHasRun) {
        // Only run this if update has run since last draw

        // Choose endpoints
        if (this->attachRightArmTimer != 0.0f) {
            rightPos = &this->rightForearmPos;
            leftPos = &this->leftShoulderPos;
            this->gravity = -13.0f;
        } else if (this->attachLeftArmTimer != 0.0f) {
            rightPos = &this->rightShoulderPos;
            leftPos = &this->leftForearmPos;
            this->gravity = -13.0f;
        } else if (this->attachShouldersTimer != 0.0f) {
            rightPos = &this->rightShoulderPos;
            leftPos = &this->leftShoulderPos;
            this->gravity = -3.0f;
        } else {
            rightPos = &this->rightForearmPos;
            leftPos = &this->leftForearmPos;
        }

        xDiff = leftPos->x - rightPos->x;
        yDiff = leftPos->y - rightPos->y;
        zDiff = leftPos->z - rightPos->z;

        midpoint.x = rightPos->x + xDiff * 0.5f;
        midpoint.y = rightPos->y + yDiff * 0.5f;
        midpoint.z = rightPos->z + zDiff * 0.5f;

        // Calculate base orientation for chosen endpoints
        yaw = Math_Atan2F(xDiff, zDiff);
        pitch = -Math_Atan2F(yDiff, sqrtf(SQ(xDiff) + SQ(zDiff)));
        diffHalfDist = sqrtf(SQ(xDiff) + SQ(yDiff) + SQ(zDiff)) * 0.5f;

        Matrix_RotateYF(yaw, MTXMODE_NEW);
        Matrix_RotateXFApply(pitch);
        this->baseYaw = yaw - MANT_PI / 2.0f;

        for (strandIdx = 0; strandIdx < GANON_MANT_NUM_STRANDS; strandIdx++) {
            Matrix_Push();

            // Calculate root positions for chosen endpoints
            strandOffset.x = sinf((strandIdx * MANT_PI) / (GANON_MANT_NUM_STRANDS - 1)) * diffHalfDist;
            strandOffset.y = 0;
            strandOffset.z = -cosf((strandIdx * MANT_PI) / (GANON_MANT_NUM_STRANDS - 1)) * diffHalfDist;
            Matrix_MultVec3f(&strandOffset, &strandDivPos);
            this->strands[strandIdx].root.x = midpoint.x + strandDivPos.x;
            this->strands[strandIdx].root.y = midpoint.y + strandDivPos.y;
            this->strands[strandIdx].root.z = midpoint.z + strandDivPos.z;

            nextStrandIdx = strandIdx + 1;
            if (nextStrandIdx >= GANON_MANT_NUM_STRANDS) {
                nextStrandIdx = strandIdx - 1;
            }

            // Update the strand joints
            OoTEnGanonMant_UpdateStrand(play, this, &this->strands[strandIdx].root, this->strands[strandIdx].joints,
                                        this->strands[nextStrandIdx].joints, this->strands[strandIdx].rotations,
                                        this->strands[strandIdx].velocities, strandIdx);
            Matrix_Pop();
        }
        OoTEnGanonMant_UpdateVertices(this);
        this->updateHasRun = false;
    }

    OoTEnGanonMant_DrawCloak(play, this);
}
