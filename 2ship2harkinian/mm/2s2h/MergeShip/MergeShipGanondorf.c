// MergeShip2 -- Ganondorf mode in Termina (MergeShipGanondorf.h): the 2Ship port of SoH's
// soh/MergeShip/MergeShipGanondorf.c -- Ganondorf drawn on the player's skeleton (each of his limb display lists rides a
// Link limb with a constant matrix; his skinned lists read their limbs' matrices from segment 0x0D), the bosses'
// animations retargeted onto it, his cape, Beast Ganon's own skeleton, his masks, his magic's look, his organ.

#include "MergeShipGanondorf.h"
#include "global.h"
#include "MergeShipAdultLink.h"
#include "MergeShipOoTEquip.h"
#include "MergeShipGanondorfAssets.h"
#include "MergeShipOoTSfx.h"
#include "overlays/actors/ovl_OoT_En_Ganon_Mant/z_oot_en_ganon_mant.h"
#include "overlays/actors/ovl_MS_Ganon_Magic/z_ms_ganon_magic.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

void MergeShipTest_Log(const char* fmt, ...);
s32 MergeShip_AdultMask_ArtMounted(void); // MergeShipAdultMask.cpp
void SkelAnime_GetFrameData(AnimationHeader* animation, s32 frame, s32 limbCount, Vec3s* frameTable);

// One of Ganondorf's limb display lists, the Link limb it rides (PLAYER_LIMB_*) and its constant matrix: Ganondorf's
// limb space -> that Link limb's space at the rig's bind pose (scaled to the drawn size).
typedef struct {
    const char* dList;
    u8 ganonLimb; // his own limb (gGanondorfSkel / gYoungGanondorfSkel, from 0)
    u8 linkLimb;
    MtxF c;
} MsGanondorfPart;

// The boss animations on the rig (tools/ganon_retarget.py): the Link limb carrying one of his limbs, and the inverse
// of the rig's rotation pairing them (C_g^-1, row-major).
typedef struct {
    u8 linkLimb; // PLAYER_LIMB_*
    u8 bossLimb; // from 0
    f32 invC[9];
} MsGanondorfCarrier;

#include "../../../../Shipwright/soh/soh/MergeShip/MergeShipGanondorfRig.inc" // (SoH's generated rig, shared)
// the Iron Knuckle's animations on the tower's rig (his axe; tools/ik_retarget.py), shared
#include "../../../../Shipwright/soh/soh/MergeShip/MergeShipGanondorfIkRig.inc"

#define MS_GANONDORF_MAX_PARTS 32
#define MS_GANONDORF_PATH_MAX 128

static const MsGanondorfPart* sParts;
static char (*sPartPaths)[MS_GANONDORF_PATH_MAX]; // sParts' lists, under oot/
static s32 sPartCount;
static const Vec3f* sJoints;
static f32 sRootScale;
static Mtx* sMtx;
static u8 sFilled[MS_GANONDORF_MAX_PARTS];
static OverrideLimbDrawFlex sLinkOverride;
static s32 sDrawingFirstPerson; // Player_OverrideLimbDrawGameplayFirstPerson's draw (the hands' lists are its own)
static PostLimbDrawFlex sLinkPost;
static s32 sDrawingTower;
static s32 sDrawingPhantom;
static Vec3f sZeroPos = { 0.0f, 0.0f, 0.0f };
static Gfx* sHandItem[2]; // the item each hand holds this frame (Link's hand stripped), left and right

// Link's hand types that hold an item (not a sword, a shield or nothing): Ganondorf holds those items.
static s32 MergeShip_Ganondorf_HandHoldsItem(Player* player, s32 limbIndex) {
    if (limbIndex == PLAYER_LIMB_LEFT_HAND) {
        return player->leftHandType == PLAYER_MODELTYPE_LH_BOTTLE;
    }
    return (player->rightHandType == PLAYER_MODELTYPE_RH_BOW) ||
           (player->rightHandType == PLAYER_MODELTYPE_RH_INSTRUMENT) ||
           (player->rightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT);
}

// His fist is bigger than Link's: the items (and what Link's own post-limb draw puts in or on a hand, and the masks on
// his head) are drawn larger, a little further out along the hand.
#define MS_GANONDORF_ITEM_SCALE 1.35f
#define MS_GANONDORF_ITEM_OUT 250.0f
#define MS_GANONDORF_MASK_SCALE 1.45f // (his own masks in his hand: MergeShip_Ganondorf_DrawMask)
// What Link wears on his head, on Ganondorf's (the user, 2026-10-16: "all the masks sized and fit ganon's head"): his
// head on Link's head limb is about Link's own size, his face lower and turned down a little -- Link's head limb space
// (+X out of the face, -Y up) moved, tilted (about Z) and scaled onto his, the same for every mask but the young
// Ganondorf's two small ones, which his jutting face would swallow (tools: the previews of 2026-10-16, approved).
typedef struct {
    f32 scale;
    f32 x;
    f32 y;
    f32 pitchDeg;
} MsGanondorfHeadFit;

static const MsGanondorfHeadFit sGanondorfHeadFit = { 1.1f, -200.0f, 100.0f, 12.0f }; // (the young; fitted in game)
static const MsGanondorfHeadFit sGanondorfYoungSmallMaskFit = { 1.2f, 90.0f, 0.0f, 12.0f };

// The tower Ganondorf's (his masks are MM's adult fits, MsMaskFit_Adult, on top of this one).
static const MsGanondorfHeadFit sGanondorfTowerHeadFit = { 1.1f, -400.0f, 250.0f, 12.0f }; // (fitted in game)

// Each value can be set by a CVar, "gMergeShip.GanondorfMaskFit.<Tower|Young>.<Scale|X|Y|Pitch>" (the fitting).
static const MsGanondorfHeadFit* MergeShip_Ganondorf_HeadFit(Player* player) {
    static MsGanondorfHeadFit sFit;
    s32 adult = MergeShip_AdultLink_TablesAdult();
    const MsGanondorfHeadFit* base = adult ? &sGanondorfTowerHeadFit : &sGanondorfHeadFit;
    const char* era = adult ? "Tower" : "Young";
    char name[64];

    if (!adult && ((player->currentMask == PLAYER_MASK_ALL_NIGHT) || (player->currentMask == PLAYER_MASK_BLAST))) {
        return &sGanondorfYoungSmallMaskFit;
    }
    snprintf(name, sizeof(name), "gMergeShip.GanondorfMaskFit.%s.Scale", era);
    sFit.scale = CVarGetFloat(name, base->scale);
    snprintf(name, sizeof(name), "gMergeShip.GanondorfMaskFit.%s.X", era);
    sFit.x = CVarGetFloat(name, base->x);
    snprintf(name, sizeof(name), "gMergeShip.GanondorfMaskFit.%s.Y", era);
    sFit.y = CVarGetFloat(name, base->y);
    snprintf(name, sizeof(name), "gMergeShip.GanondorfMaskFit.%s.Pitch", era);
    sFit.pitchDeg = CVarGetFloat(name, base->pitchDeg);
    return &sFit;
}

// The tower Ganondorf's cape: Ocarina of Time's own cloth (OoTEnGanonMant), as BossGanon wears it -- a child actor of the
// player, fed the points BossGanon_PostLimbDraw finds on his limbs (the backs of the upper arms hold it; his hands for
// the attachments BossGanon times, unused here).
#define GANON_LIMB_LEFT_UPPER_ARM 3
#define GANON_LIMB_LEFT_HAND 5
#define GANON_LIMB_RIGHT_UPPER_ARM 7
#define GANON_LIMB_RIGHT_HAND 9
#define GANON_LIMB_JEWEL 10
static OoTEnGanonMant* sCape;
static Vec3f sCapeLeftArm;
static Vec3f sCapeRightArm;
static Vec3f sCapeLeftHand;
static Vec3f sCapeRightHand;
static s32 sCapePointsFrame = -1;

// The rig's lists are SoH's resource names ("__OTR__objects/..."): here Ocarina of Time's archive is under oot/.
static char sYoungPaths[ARRAY_COUNT(sGanondorfYoungParts)][MS_GANONDORF_PATH_MAX];
static char sTowerPaths[ARRAY_COUNT(sGanondorfTowerParts)][MS_GANONDORF_PATH_MAX];
static char sPhantomPaths[ARRAY_COUNT(sGanondorfPhantomParts)][MS_GANONDORF_PATH_MAX];

static const char* MergeShip_Ganondorf_PartDL(s32 i) {
    char* out = sPartPaths[i];

    if (out[0] == '\0') {
        const char* path = sParts[i].dList;
        const char* rest = (strncmp(path, "__OTR__", 7) == 0) ? (path + 7) : path;

        snprintf(out, MS_GANONDORF_PATH_MAX, "__OTR__oot/%s", rest);
    }
    return out;
}

s32 MergeShip_Ganondorf_Enabled(void) {
    return CVarGetInteger(MERGESHIP_GANONDORF_CVAR, 0) != 0;
}


// His masks (Ganondorf mode): the Deku Mask's place is the Phantom Ganon mask, the Fierce Deity's Mask's is Ganon's.
// Worn, he is Phantom Ganon (his rig on Link's skeleton, his magic) or Beast Ganon (the boss's own skeleton and
// animations, the boss's swords); the same mask again takes him back.
static s32 sForm;
s32 gMsGanondorfPendingForm = -1;
MsGanondorfFx gMsGanondorfFx;
Vec3f gMsGanondorfHandPos;
Vec3f gMsGanondorfChestPos;
Vec3f gMsGanondorfSpearTip;

s32 MergeShip_Ganondorf_Form(void) {
    return sForm;
}

static void MergeShip_Ganondorf_ResetBeast(void);

void MergeShip_Ganondorf_SetForm(s32 form) {
    sForm = form;
    MergeShip_Ganondorf_ResetBeast();
}

// ---- the bosses' animations on Link's skeleton ------------------------------------------------------------------------
//
// tools/ganon_retarget.py, at run time: the rig puts his limb g on Link limb L as  LinkWorld[L] * C_g = BossWorld[g],
// so the boss's pose is the Link pose with  LinkWorld[L] = BossWorld[g] * C_g^-1  (rotations) for each Link limb
// carrying one of his limbs, and no rotation of its own for the rest (the control limbs, hat, collar, sheath; the
// root). Applied when he is drawn -- after the frame's animation tasks have loaded Link's own pose -- and kept in the
// joint table, so Anchor sends it and Link's next animation morphs from it.

// Link's limb parents (PLAYER_LIMB_*; 0: none)
static const u8 sLinkLimbParents[PLAYER_LIMB_MAX] = { 0, 0, 1, 2, 3, 4, 5, 3, 7, 8, 1, 10, 11, 10, 10, 14, 15, 10, 17, 18, 10, 10 };

#define MS_BOSS_LIMBS 25
static const char* sPoseAnim;
static f32 sPoseFrame;
static f32 sPoseWeight;
static s32 sPoseSetFrame = -1;
static s32 sPoseAppliedFrame = -1;
// the boss frame last posed (Phantom Ganon's spear limbs are drawn from it); room for the Iron Knuckle's limbs
#define MS_POSE_LIMBS MAX(MS_BOSS_LIMBS, MS_IK_LIMBS)
static Vec3s sPoseBoss[MS_POSE_LIMBS + 1];
// the axe's head where it was last drawn (his axe's hitbox rides it: MergeShipGanondorfPlayer.inc), its trail's points
Vec3f gMsGanondorfAxeHead;
Vec3f gMsGanondorfAxeTip;
Vec3f gMsGanondorfAxeBase;
u32 gMsGanondorfAxeHeadFrame;
static s32 sPoseSpear;
// The limbs' offsets this frame: the rig's, or (posed on the tower's or Phantom Ganon's own rig) where the boss's
// joints are (tools/ganon_retarget.py retarget_full). sPosedJointsFrame: the frame they were made for.
static Vec3f sPosedJoints[PLAYER_LIMB_MAX - 1];
static s32 sPosedJointsFrame = -1;

void MergeShip_Ganondorf_SetPose(const char* anim, f32 frame, f32 weight) {
    sPoseAnim = anim;
    sPoseFrame = frame;
    sPoseWeight = (anim != NULL) ? CLAMP(weight, 0.0f, 1.0f) : 0.0f;
    sPoseSetFrame = (gPlayState != NULL) ? (s32)gPlayState->gameplayFrames : 0;
}

typedef f32 MsMat3[3][3];

// Matrix_TranslateRotateZYX's rotation: Rz * Ry * Rx
static void MsGd_Rot(Vec3s* r, MsMat3 m) {
    f32 sx = Math_SinS(r->x), cx = Math_CosS(r->x);
    f32 sy = Math_SinS(r->y), cy = Math_CosS(r->y);
    f32 sz = Math_SinS(r->z), cz = Math_CosS(r->z);

    m[0][0] = cz * cy;
    m[0][1] = cz * sy * sx - sz * cx;
    m[0][2] = cz * sy * cx + sz * sx;
    m[1][0] = sz * cy;
    m[1][1] = sz * sy * sx + cz * cx;
    m[1][2] = sz * sy * cx - cz * sx;
    m[2][0] = -sy;
    m[2][1] = cy * sx;
    m[2][2] = cy * cx;
}

static void MsGd_Mul(MsMat3 a, MsMat3 b, MsMat3 out) {
    MsMat3 t;
    s32 i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            t[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
        }
    }
    memcpy(out, t, sizeof(t));
}

// out = a^T * b
static void MsGd_MulTA(MsMat3 a, MsMat3 b, MsMat3 out) {
    MsMat3 t;
    s32 i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            t[i][j] = a[0][i] * b[0][j] + a[1][i] * b[1][j] + a[2][i] * b[2][j];
        }
    }
    memcpy(out, t, sizeof(t));
}

static void MsGd_Identity(MsMat3 m) {
    memset(m, 0, sizeof(MsMat3));
    m[0][0] = m[1][1] = m[2][2] = 1.0f;
}

static s16 MsGd_ToBinang(f32 rad) {
    return (s16)(s32)(rad * (32768.0f / M_PIf));
}

static void MsGd_Euler(MsMat3 m, Vec3s* out) {
    f32 sy = CLAMP(-m[2][0], -1.0f, 1.0f);

    // At y = +-90 degrees (gimbal lock) x and z turn about the same axis: atan2 of the near-zero entries then loses
    // the turn (Phantom Ganon's charge put his upper body there, and drew it wrong). Rz Ry Rx with cos y = 0:
    // m01 = sin(x - z), m11 = cos(x - z) at +90; m01 = -sin(x + z), m11 = cos(x + z) at -90 -- all in x, z = 0.
    if (sy > 0.99995f) {
        out->x = MsGd_ToBinang(atan2f(m[0][1], m[1][1]));
        out->y = 0x4000;
        out->z = 0;
        return;
    }
    if (sy < -0.99995f) {
        out->x = MsGd_ToBinang(atan2f(-m[0][1], m[1][1]));
        out->y = -0x4000;
        out->z = 0;
        return;
    }
    out->x = MsGd_ToBinang(atan2f(m[2][1], m[2][2]));
    out->y = MsGd_ToBinang(asinf(sy));
    out->z = MsGd_ToBinang(atan2f(m[1][0], m[0][0]));
}

// ---- the Iron Knuckle's arms on him (tools/ik_retarget.py reach_arms) ----------------------------------------------------
//
// The user, 2026-10-17 ("his arms are way too narrow and close together", "the shoulders look awkward"): the Iron
// Knuckle's wide body angles its arms in to meet on the handle; those angles on his narrower shoulders cross his
// forearms. So each arm reaches instead: the hand where the Iron Knuckle's is (from the chest, at his size), the elbow
// solved on his own two bones, bending the way the Iron Knuckle's bends; the hand keeps its turn (the axe's).

static void MsGd_Col0(MsMat3 m, f32* out) {
    out[0] = m[0][0];
    out[1] = m[1][0];
    out[2] = m[2][0];
}

static f32 MsGd_Len3(const f32* v) {
    return sqrtf(SQ(v[0]) + SQ(v[1]) + SQ(v[2]));
}

// m turned the least so that its +X points along v.
static void MsGd_Align(MsMat3 m, const f32* v) {
    f32 x[3];
    f32 n[3];
    f32 axis[3];
    f32 lx;
    f32 lv;
    f32 sn;
    f32 cs;
    MsMat3 k;
    MsMat3 k2;
    MsMat3 q;
    s32 a;
    s32 b;

    MsGd_Col0(m, x);
    lx = MsGd_Len3(x);
    lv = MsGd_Len3(v);
    if ((lx < 1e-6f) || (lv < 1e-6f)) {
        return;
    }
    for (a = 0; a < 3; a++) {
        x[a] /= lx;
        n[a] = v[a] / lv;
    }
    axis[0] = x[1] * n[2] - x[2] * n[1];
    axis[1] = x[2] * n[0] - x[0] * n[2];
    axis[2] = x[0] * n[1] - x[1] * n[0];
    sn = MsGd_Len3(axis);
    cs = x[0] * n[0] + x[1] * n[1] + x[2] * n[2];
    if (sn < 1e-6f) {
        return;
    }
    for (a = 0; a < 3; a++) {
        axis[a] /= sn;
    }
    k[0][0] = 0.0f;
    k[0][1] = -axis[2];
    k[0][2] = axis[1];
    k[1][0] = axis[2];
    k[1][1] = 0.0f;
    k[1][2] = -axis[0];
    k[2][0] = -axis[1];
    k[2][1] = axis[0];
    k[2][2] = 0.0f;
    MsGd_Mul(k, k, k2);
    for (a = 0; a < 3; a++) {
        for (b = 0; b < 3; b++) {
            q[a][b] = ((a == b) ? 1.0f : 0.0f) + k[a][b] * sn + k2[a][b] * (1.0f - cs);
        }
    }
    MsGd_Mul(q, m, m);
}

// world += r * off
static void MsGd_AddRot(f32* world, MsMat3 r, const Vec3f* off) {
    world[0] += r[0][0] * off->x + r[0][1] * off->y + r[0][2] * off->z;
    world[1] += r[1][0] * off->x + r[1][1] * off->y + r[1][2] * off->z;
    world[2] += r[2][0] * off->x + r[2][1] * off->y + r[2][2] * off->z;
}

// Wider shoulders on him (the user, 2026-10-17: "more of the top bones like the Iron Knuckle" -- wider shoulders): his
// shoulder joints this far out from his chest while it holds the axe (the Iron Knuckle's are 2.3 times his).
#define MS_IK_SHOULDER_WIDEN CVarGetFloat("gMergeShip.GanondorfAxeShoulders", 1.5f)

// One arm's two bones solved so its wrist lands on `wrist` (or as near as it reaches), its elbow bending toward `pole`;
// out: where the wrist ended.
static void MsGd_ReachArm(MsMat3* linkWorld, f32 (*linkPos)[3], const Vec3f* joints, u8 ls, u8 la, u8 lh,
                          const f32* wrist, const f32* pole, f32* out) {
    f32 a = sqrtf(SQ(joints[la - 1].x) + SQ(joints[la - 1].y) + SQ(joints[la - 1].z));
    f32 b = sqrtf(SQ(joints[lh - 1].x) + SQ(joints[lh - 1].y) + SQ(joints[lh - 1].z));
    f32 d[3];
    f32 n[3];
    f32 p[3];
    f32 e[3];
    f32 seg[3];
    f32 dist;
    f32 distC;
    f32 dot = 0.0f;
    f32 lp;
    f32 cosA;
    f32 sinA;
    s32 c;

    memcpy(out, linkPos[lh], 3 * sizeof(f32));
    for (c = 0; c < 3; c++) {
        d[c] = wrist[c] - linkPos[ls][c];
    }
    dist = MsGd_Len3(d);
    if ((dist < 1e-3f) || (a < 1e-3f) || (b < 1e-3f)) {
        return;
    }
    distC = CLAMP(dist, fabsf(a - b) + 1e-3f, (a + b) * 0.999f);
    for (c = 0; c < 3; c++) {
        n[c] = d[c] / dist;
        dot += pole[c] * n[c];
    }
    for (c = 0; c < 3; c++) {
        p[c] = pole[c] - n[c] * dot;
    }
    lp = MsGd_Len3(p);
    if (lp < 1e-6f) {
        p[0] = n[2]; // (no bend to follow: out to the side)
        p[1] = 0.0f;
        p[2] = -n[0];
        lp = MsGd_Len3(p);
        if (lp < 1e-6f) {
            return;
        }
    }
    cosA = (a * a + distC * distC - b * b) / (2.0f * a * distC);
    sinA = sqrtf(MAX(0.0f, 1.0f - cosA * cosA));
    for (c = 0; c < 3; c++) {
        e[c] = linkPos[ls][c] + n[c] * a * cosA + (p[c] / lp) * a * sinA;
        seg[c] = e[c] - linkPos[ls][c];
        out[c] = linkPos[ls][c] + n[c] * distC;
    }
    MsGd_Align(linkWorld[ls], seg);
    for (c = 0; c < 3; c++) {
        seg[c] = out[c] - e[c];
    }
    MsGd_Align(linkWorld[la], seg);
    // (the hand keeps its world turn: linkWorld[lh] as it is)
}

// Both hands on the axe, as the Iron Knuckle's: his right fist where its right hand is (from the chest, at his size) --
// the handle through it (the axe drawn about his fist: MergeShip_Ganondorf_PostLimb) -- and his left fist where its left
// hand holds the shaft (from its right hand: on his axe, wherever his right hand got to).
static void MergeShip_Ganondorf_IkReach(MsMat3* linkWorld, MsMat3* bossWorld, const Vec3f* joints) {
    f32 linkPos[PLAYER_LIMB_MAX][3];
    f32 bossPos[MS_IK_LIMBS][3];
    f32 k = sGanondorfIkAxeScale; // its units in his (game units)
    f32 wrist[3];
    f32 pole[3];
    f32 fist[3];
    f32 reached[3];
    s32 i;
    s32 c;

    // his joints (game units, from his root) and the Iron Knuckle's (its units, from its root)
    linkPos[PLAYER_LIMB_ROOT][0] = linkPos[PLAYER_LIMB_ROOT][1] = linkPos[PLAYER_LIMB_ROOT][2] = 0.0f;
    for (i = PLAYER_LIMB_ROOT + 1; i < PLAYER_LIMB_MAX; i++) {
        u8 pl = sLinkLimbParents[i];

        memcpy(linkPos[i], linkPos[pl], sizeof(linkPos[i]));
        MsGd_AddRot(linkPos[i], linkWorld[pl], &joints[i - 1]);
    }
    for (i = 0; i < MS_IK_LIMBS; i++) {
        u8 pb = sGanondorfIkBossParents[i];

        if (pb == 255) {
            bossPos[i][0] = bossPos[i][1] = bossPos[i][2] = 0.0f;
            continue;
        }
        memcpy(bossPos[i], bossPos[pb], sizeof(bossPos[i]));
        MsGd_AddRot(bossPos[i], bossWorld[pb], &sGanondorfIkBossLimbPos[i]);
    }

    // (his fists turn as its hands -- the pairing -- and its hand spaces sit in his hands by sGanondorfIkAxeMtx's way:
    // its fist's middle on his. sGanondorfIkHandOrigin: where its hand limbs' origins are in his.)
    // the right arm: its hand's origin where the Iron Knuckle's right hand is (from the chest, at his size)
    fist[0] = fist[1] = fist[2] = 0.0f;
    MsGd_AddRot(fist, linkWorld[PLAYER_LIMB_RIGHT_HAND], &sGanondorfIkHandOrigin[0]);
    for (c = 0; c < 3; c++) {
        wrist[c] = linkPos[PLAYER_LIMB_UPPER_ROOT][c] + (bossPos[15][c] - bossPos[9][c]) * k - fist[c];
        pole[c] = bossPos[14][c] - bossPos[13][c]; // (its upper arm's way: tools/ik_sheets.py --pole upper)
    }
    MsGd_ReachArm(linkWorld, linkPos, joints, PLAYER_LIMB_RIGHT_SHOULDER, PLAYER_LIMB_RIGHT_FOREARM, PLAYER_LIMB_RIGHT_HAND, wrist,
                  pole, reached);
    // the left arm: its hand's origin where the Iron Knuckle's left hand is from its right (on the shaft when it holds it)
    {
        f32 lorigin[3] = { 0.0f, 0.0f, 0.0f };

        MsGd_AddRot(lorigin, linkWorld[PLAYER_LIMB_LEFT_HAND], &sGanondorfIkHandOrigin[1]);
        for (c = 0; c < 3; c++) {
            wrist[c] = reached[c] + fist[c] + (bossPos[20][c] - bossPos[15][c]) * k - lorigin[c];
            pole[c] = bossPos[19][c] - bossPos[18][c];
        }
        MsGd_ReachArm(linkWorld, linkPos, joints, PLAYER_LIMB_LEFT_SHOULDER, PLAYER_LIMB_LEFT_FOREARM, PLAYER_LIMB_LEFT_HAND,
                      wrist, pole, reached);
    }
}

// Poses `jointTable` (Link's, PLAYER_LIMB_MAX entries) as the boss animation's frame, blended by sPoseWeight. On the
// rig made from that boss (`positions`: the tower's for his animations, Phantom Ganon's for his), the joints are put
// where the boss's are too: rotations alone keep Link's upper body stacked on his hips, and a pose that bends the
// boss's spine double (Phantom Ganon's charge) then draws him upside down.
static void MergeShip_Ganondorf_ApplyPose(Vec3s* jointTable, s32 phantom, s32 positions, const Vec3f* rigJoints,
                                          f32 rigRootScale) {
    static MsMat3 sBossWorld[MS_POSE_LIMBS];
    static MsMat3 sTarget[PLAYER_LIMB_MAX];
    static MsMat3 sLinkWorld[PLAYER_LIMB_MAX];
    // an Iron Knuckle animation (his axe: tools/ik_retarget.py): its own skeleton paired with the tower's rig, rotations
    s32 ik = !phantom && (strstr(sPoseAnim, "object_ik/") != NULL);
    const MsGanondorfCarrier* carriers =
        ik ? sGanondorfIkCarriers : phantom ? sGanondorfPhantomCarriers : sGanondorfTowerCarriers;
    s32 carrierCount = ik        ? ARRAY_COUNT(sGanondorfIkCarriers)
                       : phantom ? ARRAY_COUNT(sGanondorfPhantomCarriers)
                                 : ARRAY_COUNT(sGanondorfTowerCarriers);
    const u8* bossParents =
        ik ? sGanondorfIkBossParents : phantom ? sGanondorfPhantomBossParents : sGanondorfTowerBossParents;
    f32 rootScale = ik ? sGanondorfIkRetargetRoot : phantom ? sGanondorfPhantomRetargetRoot : sGanondorfTowerRetargetRoot;
    s32 bossLimbs = ik ? MS_IK_LIMBS : MS_BOSS_LIMBS;
    u8 has[PLAYER_LIMB_MAX];
    s32 frame;
    s32 last;
    s32 i;

    if (ik) {
        positions = false;
    }
    last = Animation_GetLastFrame((void*)sPoseAnim);
    frame = CLAMP((s32)sPoseFrame, 0, MAX(last, 0));
    SkelAnime_GetFrameData((AnimationHeader*)sPoseAnim, frame, bossLimbs + 1, sPoseBoss);
    MergeShipTest_Log("POSEIN %s f=%d root %d %d %d", sPoseAnim, frame, sPoseBoss[0].x, sPoseBoss[0].y, sPoseBoss[0].z);
    sPoseSpear = phantom;

    for (i = 0; i < bossLimbs; i++) {
        MsMat3 r;

        MsGd_Rot(&sPoseBoss[i + 1], r);
        if (bossParents[i] == 255) {
            memcpy(sBossWorld[i], r, sizeof(MsMat3));
        } else {
            MsGd_Mul(sBossWorld[bossParents[i]], r, sBossWorld[i]);
        }
    }
    memset(has, 0, sizeof(has));
    for (i = 0; i < carrierCount; i++) {
        MsMat3 invC;

        memcpy(invC, carriers[i].invC, sizeof(MsMat3));
        MsGd_Mul(sBossWorld[carriers[i].bossLimb], invC, sTarget[carriers[i].linkLimb]);
        has[carriers[i].linkLimb] = true;
    }
    MsGd_Identity(sLinkWorld[PLAYER_LIMB_ROOT]);
    for (i = PLAYER_LIMB_ROOT; i < PLAYER_LIMB_MAX; i++) {
        if (i == PLAYER_LIMB_ROOT) {
        } else if (has[i]) {
            memcpy(sLinkWorld[i], sTarget[i], sizeof(MsMat3));
        } else {
            memcpy(sLinkWorld[i], sLinkWorld[sLinkLimbParents[i]], sizeof(MsMat3));
        }
    }
    if (ik) {
        // his shoulders wider, as the Iron Knuckle's (this frame's joints: the reach's and the draw's)
        f32 widen = 1.0f + (MS_IK_SHOULDER_WIDEN - 1.0f) * sPoseWeight;

        memcpy(sPosedJoints, rigJoints, sizeof(sPosedJoints));
        sPosedJoints[PLAYER_LIMB_LEFT_SHOULDER - 1].z *= widen;
        sPosedJoints[PLAYER_LIMB_RIGHT_SHOULDER - 1].z *= widen;
        sPosedJointsFrame = (gPlayState != NULL) ? (s32)gPlayState->gameplayFrames : 0;
        MergeShip_Ganondorf_IkReach(sLinkWorld, sBossWorld, sPosedJoints); // (its arms reach, on his shoulders)
    }
    for (i = PLAYER_LIMB_ROOT; i < PLAYER_LIMB_MAX; i++) {
        MsMat3 local;
        Vec3s posed;
        Vec3s* own = &jointTable[i];

        if ((i != PLAYER_LIMB_ROOT) && has[i]) {
            MsGd_MulTA(sLinkWorld[sLinkLimbParents[i]], sLinkWorld[i], local);
        } else if (ik && (i != PLAYER_LIMB_ROOT)) {
            // (his own, held as in Link's idle: his head's carriage on the Iron Knuckle's chest)
            own->x += (s16)((s16)(sGanondorfIkOwnJoints[i].x - own->x) * sPoseWeight);
            own->y += (s16)((s16)(sGanondorfIkOwnJoints[i].y - own->y) * sPoseWeight);
            own->z += (s16)((s16)(sGanondorfIkOwnJoints[i].z - own->z) * sPoseWeight);
            continue;
        } else {
            MsGd_Identity(local);
        }
        MsGd_Euler(local, &posed);
        MergeShipTest_Log("POSEOUT L%d %d %d %d w=%.2f", i, posed.x, posed.y, posed.z, sPoseWeight);
        own->x += (s16)((s16)(posed.x - own->x) * sPoseWeight);
        own->y += (s16)((s16)(posed.y - own->y) * sPoseWeight);
        own->z += (s16)((s16)(posed.z - own->z) * sPoseWeight);
    }
    if (positions) {
        // the boss's joint positions (his units): root at his root translation, each limb at its offset from its parent
        static Vec3f sBossPos[MS_BOSS_LIMBS];
        const Vec3f* limbPos = phantom ? sGanondorfPhantomBossLimbPos : sGanondorfTowerBossLimbPos;
        const u8* jointOn = phantom ? sGanondorfPhantomJointOn : sGanondorfTowerJointOn;
        // game units over rig units (the rig is drawn at its size: RootScale over the retarget's root scale)
        f32 toGame = rigRootScale / rootScale;
        Vec3f linkPos[PLAYER_LIMB_MAX];
        Vec3f rootPos;

        for (i = 0; i < MS_BOSS_LIMBS; i++) {
            Vec3f off;

            if (bossParents[i] == 255) {
                sBossPos[i].x = sPoseBoss[0].x;
                sBossPos[i].y = sPoseBoss[0].y;
                sBossPos[i].z = sPoseBoss[0].z;
                continue;
            }
            off = limbPos[i];
            MsMat3* pr = &sBossWorld[bossParents[i]];
            sBossPos[i].x = sBossPos[bossParents[i]].x + (*pr)[0][0] * off.x + (*pr)[0][1] * off.y + (*pr)[0][2] * off.z;
            sBossPos[i].y = sBossPos[bossParents[i]].y + (*pr)[1][0] * off.x + (*pr)[1][1] * off.y + (*pr)[1][2] * off.z;
            sBossPos[i].z = sBossPos[bossParents[i]].z + (*pr)[2][0] * off.x + (*pr)[2][1] * off.y + (*pr)[2][2] * off.z;
        }
        // Link's root (no rotation) so that his waist lands on the boss's pelvis (for the joints' offsets only)
        rootPos.x = sBossPos[jointOn[PLAYER_LIMB_WAIST]].x - rigJoints[PLAYER_LIMB_WAIST - 1].x / toGame;
        rootPos.y = sBossPos[jointOn[PLAYER_LIMB_WAIST]].y - rigJoints[PLAYER_LIMB_WAIST - 1].y / toGame;
        rootPos.z = sBossPos[jointOn[PLAYER_LIMB_WAIST]].z - rigJoints[PLAYER_LIMB_WAIST - 1].z / toGame;
        linkPos[PLAYER_LIMB_ROOT] = rootPos;
        for (i = PLAYER_LIMB_ROOT + 1; i < PLAYER_LIMB_MAX; i++) {
            u8 parentLimb = sLinkLimbParents[i];
            MsMat3* pr = &sLinkWorld[parentLimb];
            Vec3f own = rigJoints[i - 1];
            Vec3f off;

            own.x /= toGame;
            own.y /= toGame;
            own.z /= toGame;
            off = own;
            if (jointOn[i] != 255) {
                // on the boss's joint: the offset that puts it there, in the parent's frame
                Vec3f d;

                d.x = sBossPos[jointOn[i]].x - linkPos[parentLimb].x;
                d.y = sBossPos[jointOn[i]].y - linkPos[parentLimb].y;
                d.z = sBossPos[jointOn[i]].z - linkPos[parentLimb].z;
                off.x = (*pr)[0][0] * d.x + (*pr)[1][0] * d.y + (*pr)[2][0] * d.z;
                off.y = (*pr)[0][1] * d.x + (*pr)[1][1] * d.y + (*pr)[2][1] * d.z;
                off.z = (*pr)[0][2] * d.x + (*pr)[1][2] * d.y + (*pr)[2][2] * d.z;
            }
            linkPos[i].x = linkPos[parentLimb].x + (*pr)[0][0] * off.x + (*pr)[0][1] * off.y + (*pr)[0][2] * off.z;
            linkPos[i].y = linkPos[parentLimb].y + (*pr)[1][0] * off.x + (*pr)[1][1] * off.y + (*pr)[1][2] * off.z;
            linkPos[i].z = linkPos[parentLimb].z + (*pr)[2][0] * off.x + (*pr)[2][1] * off.y + (*pr)[2][2] * off.z;
            // (in game units, blended from the rig's own)
            sPosedJoints[i - 1].x = rigJoints[i - 1].x + (off.x * toGame - rigJoints[i - 1].x) * sPoseWeight;
            sPosedJoints[i - 1].y = rigJoints[i - 1].y + (off.y * toGame - rigJoints[i - 1].y) * sPoseWeight;
            sPosedJoints[i - 1].z = rigJoints[i - 1].z + (off.z * toGame - rigJoints[i - 1].z) * sPoseWeight;
        }
        sPosedJointsFrame = (gPlayState != NULL) ? (s32)gPlayState->gameplayFrames : 0;
        // the root keeps Link's own height (his standing): the boss's floating animations hold his hips at 0, which
        // would plant them in the floor; the joints above are placed relative to the hips, so they hold either way
        return;
    }

    // the root's translation. The boss's flying animations keep it at 0 (his pelvis limb carries the height): Link's
    // own then, plus their motion. His standing ones (the organ, StandIdle: ~7250 up) carry the whole height: theirs
    // then, in Link's units, in place of Link's.
    if (ABS(sPoseBoss[0].y) > 1000) {
        jointTable[0].x += (s16)(((sPoseBoss[0].x / rootScale) - jointTable[0].x) * sPoseWeight);
        jointTable[0].y += (s16)(((sPoseBoss[0].y / rootScale) - jointTable[0].y) * sPoseWeight);
        jointTable[0].z += (s16)(((sPoseBoss[0].z / rootScale) - jointTable[0].z) * sPoseWeight);
    } else {
        jointTable[0].x += (s16)((sPoseBoss[0].x / rootScale) * sPoseWeight);
        jointTable[0].y += (s16)((sPoseBoss[0].y / rootScale) * sPoseWeight);
        jointTable[0].z += (s16)((sPoseBoss[0].z / rootScale) * sPoseWeight);
    }
}

static s32 MergeShip_Ganondorf_FormOf(Player* player) {
    if (player->actor.id == ACTOR_PLAYER) {
        return sForm;
    }
    s32 v = MergeShipAnchor_DummyIsGanondorf(&player->actor); // (a remote player: his own, from his updates)
    return (v > 0) ? (v - 1) : MS_GANON_FORM_GANONDORF;
}

s32 MergeShip_Ganondorf_AnchorValue(void) {
    return MergeShip_Ganondorf_Enabled() ? (1 + sForm) : 0;
}

// A colour CVar ("<name>.Value", the menu's colour picker's): 0 unset (his own), else 0x01RRGGBB.
static u32 MergeShip_Ganondorf_CvarColor(const char* valueCvar) {
    Color_RGBA8 color = CVarGetColor(valueCvar, (Color_RGBA8){ 0, 0, 0, 0 });

    return (color.a == 0) ? 0 : (0x01000000 | (color.r << 16) | (color.g << 8) | color.b);
}

u32 MergeShip_Ganondorf_ArmorColor(void) {
    return MergeShip_Ganondorf_CvarColor("gMergeShip.GanondorfArmor.Value");
}

u32 MergeShip_Ganondorf_CapeColor(void) {
    return MergeShip_Ganondorf_CvarColor("gMergeShip.GanondorfCape.Value");
}




// The player, or a remote player in Anchor (a dummy, ACTOR_EN_OE2): drawn as Ganondorf when Ganondorf mode is on for
// them -- this game's option for the player, the remote player's own for a dummy -- and they are not transformed.
static s32 MergeShip_Ganondorf_IsPlayerGanondorf(Player* player) {
    if ((player == NULL) || (player->transformation != PLAYER_FORM_HUMAN)) {
        return false;
    }
    if (player->actor.id == ACTOR_PLAYER) {
        return MergeShip_Ganondorf_Enabled();
    }
    // a remote player in Anchor (a dummy, ANCHOR_DUMMY_ACTOR_ID: ACTOR_UNSET_13): his own option, from his updates
    return (player->actor.id == ACTOR_UNSET_13) && (MergeShipAnchor_DummyIsGanondorf(&player->actor) != 0);
}

// Link's skeleton with Ganondorf's proportions: every joint at his (the rig's offsets), the root's translation from
// Link's animations scaled to his height -- undoing the child's 0.64 (Player_OverrideLimbDrawGameplayCommon), so he is
// the same size at both ages. Link's own meshes (and the items his hand lists hold) are not drawn.
static s32 sOrganActive;
static void MergeShip_Ganondorf_DrawOrgan(PlayState* play);

static s32 MergeShip_Ganondorf_OverrideLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            Actor* thisx) {
    Player* player = (Player*)thisx;
    s32 ret;

    // (first person: Link's own bones, so his fists come where Link's hands do before the camera)
    if ((limbIndex != PLAYER_LIMB_ROOT) && !sDrawingFirstPerson) {
        *pos = ((gPlayState != NULL) && (sPosedJointsFrame == (s32)gPlayState->gameplayFrames))
                   ? sPosedJoints[limbIndex - 1]
                   : sJoints[limbIndex - 1];
    }
    ret = (sLinkOverride != NULL) ? sLinkOverride(play, limbIndex, dList, pos, rot, thisx) : false;
    if ((limbIndex == PLAYER_LIMB_LEFT_HAND) || (limbIndex == PLAYER_LIMB_RIGHT_HAND)) {
        s32 hand = (limbIndex == PLAYER_LIMB_RIGHT_HAND);

        sHandItem[hand] =
            ((*dList != NULL) && MergeShip_Ganondorf_HandHoldsItem(player, limbIndex) && !sOrganActive)
                ? MergeShip_Ganondorf_StripHandDL((const char*)*dList)
                : NULL;
    }
    if (limbIndex == PLAYER_LIMB_ROOT) {
        // Link's root translation (Player_OverrideLimbDrawGameplayCommon scaled it by his age's), at his height
        // (riding: Link's own seat -- his hips on the saddle)
        f32 base = ((player->stateFlags1 & PLAYER_STATE1_800000) || sDrawingFirstPerson) ? 1.0f : sRootScale;
        f32 age = (player->ageProperties != NULL) ? player->ageProperties->unk_08 : 1.0f;
        f32 xz = base;
        f32 y = base;

        if (age > 0.0f) {
            if (!(player->skelAnime.movementFlags & ANIM_FLAG_4) || (player->skelAnime.movementFlags & ANIM_FLAG_1)) {
                xz /= age;
            }
            if (!(player->skelAnime.movementFlags & ANIM_FLAG_4) ||
                (player->skelAnime.movementFlags & ANIM_FLAG_UPDATE_Y)) {
                y /= age;
            }
        }
        pos->x *= xz;
        pos->y *= y;
        pos->z *= xz;
    }
    *dList = NULL;
    return ret;
}

// At each Link limb (its matrix current): the matrices of the Ganondorf limbs riding it.
static void MergeShip_Ganondorf_PostLimb(PlayState* play, s32 limbIndex, Gfx** dList, Gfx** dList2, Vec3s* rot,
                                         Actor* thisx) {
    void* data = thisx;
    s32 i;

    for (i = 0; i < sPartCount; i++) {
        if (sParts[i].linkLimb == limbIndex) {
            Matrix_Push();
            if (sDrawingPhantom && sPoseSpear && (sParts[i].ganonLimb == 12)) {
                // Phantom Ganon's spear, turned in his hand as his animation turns it (limbs 11 and 12 on hand 10)
                s32 k;

                for (k = 0; k < sPartCount; k++) {
                    if (sParts[k].ganonLimb == 10) {
                        Matrix_Mult((MtxF*)&sParts[k].c, MTXMODE_APPLY);
                        break;
                    }
                }
                Matrix_TranslateRotateZYX((Vec3f*)&sGanondorfPhantomSpearPivot, &sPoseBoss[12]);
                Matrix_TranslateRotateZYX(&sZeroPos, &sPoseBoss[13]);
            } else {
                Matrix_Mult((MtxF*)&sParts[i].c, MTXMODE_APPLY);
            }
            Matrix_ToMtx(&sMtx[i]);
            if (((Player*)data)->actor.id == ACTOR_PLAYER) {
                // BossGanon_PostLimbDraw / BossGanondrof_PostLimbDraw: where his magic comes from
                static Vec3f sHandLight = { 1300.0f, 0.0f, 0.0f };
                static Vec3f sYoungHandLight = { 1000.0f, 0.0f, 0.0f };
                static Vec3f sSpearTip = { 0.0f, 0.0f, 6000.0f };
                static Vec3f sOrigin = { 0.0f, 0.0f, 0.0f };
                u8 g = sParts[i].ganonLimb;

                if (sDrawingPhantom) {
                    if (g == 12) {
                        Matrix_MultVec3f(&sSpearTip, &gMsGanondorfSpearTip);
                    } else if (g == 10) {
                        Matrix_MultVec3f(&sOrigin, &gMsGanondorfHandPos);
                    } else if (g == 13) {
                        Matrix_MultVec3f(&sOrigin, &gMsGanondorfChestPos);
                    }
                } else if (g == (sDrawingTower ? GANON_LIMB_RIGHT_HAND : 7)) {
                    Matrix_MultVec3f(sDrawingTower ? &sHandLight : &sYoungHandLight, &gMsGanondorfHandPos);
                } else if (g == 1) {
                    Matrix_MultVec3f(&sOrigin, &gMsGanondorfChestPos);
                }
            }
            if (sDrawingTower) {
                // BossGanon_PostLimbDraw's cape points, in his limbs' own space
                static Vec3f sLeftArmPoint = { -500.0f, 200.0f, -300.0f };
                static Vec3f sRightArmPoint = { -500.0f, 200.0f, 300.0f };
                static Vec3f sZero = { 0.0f, 0.0f, 0.0f };

                switch (sParts[i].ganonLimb) {
                    case GANON_LIMB_LEFT_UPPER_ARM:
                        Matrix_MultVec3f(&sLeftArmPoint, &sCapeLeftArm);
                        break;
                    case GANON_LIMB_RIGHT_UPPER_ARM:
                        Matrix_MultVec3f(&sRightArmPoint, &sCapeRightArm);
                        break;
                    case GANON_LIMB_LEFT_HAND:
                        Matrix_MultVec3f(&sZero, &sCapeLeftHand);
                        break;
                    case GANON_LIMB_RIGHT_HAND:
                        Matrix_MultVec3f(&sZero, &sCapeRightHand);
                        break;
                    default:
                        break;
                }
            }
            Matrix_Pop();
            sFilled[i] = true;
        }
    }
    if ((limbIndex == PLAYER_LIMB_LEFT_HAND) || (limbIndex == PLAYER_LIMB_RIGHT_HAND) ||
        ((limbIndex == PLAYER_LIMB_HEAD) && (((Player*)data)->currentMask != PLAYER_MASK_NONE))) {
        // His fist and head are bigger than Link's: what Link holds or wears is drawn bigger with them.
        Gfx* item = (limbIndex == PLAYER_LIMB_HEAD) ? NULL : sHandItem[limbIndex == PLAYER_LIMB_RIGHT_HAND];
        f32 scale = MS_GANONDORF_ITEM_SCALE;

        if ((limbIndex == PLAYER_LIMB_RIGHT_HAND) && !sDrawingFirstPerson && MergeShip_Ganondorf_AxeOut((Player*)data)) {
            // the Iron Knuckle's axe, as the Iron Knuckle holds it (its hand limb's space is his right hand's: the
            // pairing of tools/ik_retarget.py), at its size against him as against the Iron Knuckle
            Matrix_Push();
            Matrix_Mult((MtxF*)&sGanondorfIkAxeMtx, MTXMODE_APPLY); // (its hand's space in his fist)
            OPEN_DISPS(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, Matrix_Finalize(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            if (MergeShip_Ganondorf_IronKnuckleAxeDL() != NULL) {
                // (the Iron Knuckle draws it after the opaque setup -- its list leans on it: without, his limbs' render
                // state drew it see-through; the user, 2026-10-17)
                Gfx_SetupDL25_Opa(play->state.gfxCtx);
                gSPDisplayList(POLY_OPA_DISP++, MergeShip_Ganondorf_IronKnuckleAxeDL());
            }
            // its head's middle (the double blade: tools/ik_retarget.py, the axe list's far end), for its hitbox
            {
                static Vec3f sAxeHead = { 395.0f, -59.0f, -4062.0f };

                // its trail: from the head's top spike down past the blades (the axe list's far end)
                static Vec3f sAxeTip = { 395.0f, -59.0f, -5300.0f };
                static Vec3f sAxeBase = { 395.0f, -59.0f, -2900.0f };

                Matrix_MultVec3f(&sAxeHead, &gMsGanondorfAxeHead);
                Matrix_MultVec3f(&sAxeTip, &gMsGanondorfAxeTip);
                Matrix_MultVec3f(&sAxeBase, &gMsGanondorfAxeBase);
                gMsGanondorfAxeHeadFrame = play->gameplayFrames;
            }
            CLOSE_DISPS(play->state.gfxCtx);
            Matrix_Pop();
        }


        Matrix_Push();
        if (sDrawingFirstPerson) {
            scale = 1.0f; // (first person: the item where and as Link holds it)
        } else if (limbIndex != PLAYER_LIMB_HEAD) {
            Matrix_Translate(MS_GANONDORF_ITEM_OUT, 0.0f, 0.0f, MTXMODE_APPLY);
        } else {
            const MsGanondorfHeadFit* fit = MergeShip_Ganondorf_HeadFit((Player*)data);

            Matrix_Translate(fit->x, fit->y, 0.0f, MTXMODE_APPLY);
            Matrix_RotateZF(DEG_TO_RAD(fit->pitchDeg), MTXMODE_APPLY);
            scale = fit->scale;
        }
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        if (item != NULL) {
            OPEN_DISPS(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, Matrix_Finalize(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, item);
            CLOSE_DISPS(play->state.gfxCtx);
        }
        if (sLinkPost != NULL) {
            sLinkPost(play, limbIndex, dList, dList2, rot, thisx);
        }
        Matrix_Pop();
        return;
    }
    if (sLinkPost != NULL) {
        sLinkPost(play, limbIndex, dList, dList2, rot, thisx);
    }
}

// ---- Beast Ganon ----------------------------------------------------------------------------------------------------------
//
// The boss's own skeleton (object_ganon2 gGanonSkel) and animations, chosen from what the player is doing: his guard
// stance standing or shielding, his walk moving (faster with speed), his two sword swings in turn as an attack starts
// (MergeShip_Ganondorf_OnMeleeAttack), his recoil when hurt. Drawn at about twice Link's height.

#define MS_BEAST_SCALE 0.55f
#define MS_BEAST_LIMBS 47
static SkelAnime sBeast;
static Vec3s sBeastJoints[MS_BEAST_LIMBS];
static Vec3s sBeastMorph[MS_BEAST_LIMBS];
static s32 sBeastReady;
static const char* sBeastAnim;
static s32 sBeastSwingTimer;
static s32 sBeastSwingRight;
static s32 sBeastHurtTimer;
static u8 sBeastLastInvincibility;
static s32 sBeastControlled; // an ability plays his animation

static void MergeShip_Ganondorf_ResetBeast(void) {
    sBeastAnim = NULL;
    sBeastControlled = false;
    sBeastSwingTimer = 0;
    sBeastHurtTimer = 0;
}

void MergeShip_Ganondorf_BeastAnim(const char* anim, f32 speed, s32 loop, f32 morph) {
    if (!sBeastReady) {
        return;
    }
    if (anim == NULL) {
        sBeastControlled = false;
        return;
    }
    sBeastControlled = true;
    sBeastAnim = anim;
    Animation_Change(&sBeast, (AnimationHeader*)anim, speed, 0.0f, Animation_GetLastFrame((AnimationHeader*)anim),
                     loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, morph);
}

f32 MergeShip_Ganondorf_BeastFrame(void) {
    return sBeastReady ? sBeast.curFrame : 0.0f;
}

s32 MergeShip_Ganondorf_BeastOnFrame(f32 frame) {
    return sBeastReady && Animation_OnFrame(&sBeast, frame);
}

static void MergeShip_Ganondorf_BeastPlay(const char* anim, f32 speed, s32 loop) {
    if (anim == sBeastAnim) {
        sBeast.playSpeed = speed;
        return;
    }
    sBeastAnim = anim;
    Animation_Change(&sBeast, (AnimationHeader*)anim, speed, 0.0f, Animation_GetLastFrame((AnimationHeader*)anim),
                     loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -4.0f);
}

static void MergeShip_Ganondorf_UpdateBeast(PlayState* play, Player* player) {
    if (!sBeastReady) {
        SkelAnime_InitFlex(play, &sBeast, (FlexSkeletonHeader*)gGanonSkel, (AnimationHeader*)gGanonGuardIdleAnim,
                           sBeastJoints, sBeastMorph, MS_BEAST_LIMBS);
        sBeastAnim = gGanonGuardIdleAnim;
        sBeastReady = true;
    }
    // hurt: a new invincibility window
    if ((player->invincibilityTimer > 0) && (sBeastLastInvincibility == 0)) {
        sBeastHurtTimer = 12;
        MergeShip_OoTSfx_Actor(&player->actor, OOT_NA_SE_EN_MGANON_DAMAGE);
    }
    sBeastLastInvincibility = (player->invincibilityTimer > 0);

    if (sBeastControlled) {
        // an ability's animation (MergeShipGanondorfPlayer.inc)
    } else if (sBeastSwingTimer > 0) {
        sBeastSwingTimer--;
        MergeShip_Ganondorf_BeastPlay(sBeastSwingRight ? gGanonRightSwordSwingAnim : gGanonLeftSwordSwingAnim, 1.5f,
                                      false);
    } else if (sBeastHurtTimer > 0) {
        sBeastHurtTimer--;
        MergeShip_Ganondorf_BeastPlay(gGanonDamageAnim, 1.0f, false);
    } else if (player->actor.speed > 1.5f) {
        // (BossGanon2: speed = playSpeed * 3 at his size; drawn at MS_BEAST_SCALE, his stride is that much shorter)
        MergeShip_Ganondorf_BeastPlay(gGanonWalkAnim, CLAMP(player->actor.speed / (3.0f * MS_BEAST_SCALE), 0.8f, 2.5f), true);
    } else {
        MergeShip_Ganondorf_BeastPlay(gGanonGuardIdleAnim, 1.0f, true);
    }
    SkelAnime_Update(&sBeast);

    // func_80900344 / func_808FD4D4: his stride -- a stomp at frames 13 and 28 of the walk (the sound, a ring of dust,
    // the ground shaking), and now and then his growl
    if (sBeastAnim == gGanonWalkAnim) {
        if (Animation_OnFrame(&sBeast, 13.0f) || Animation_OnFrame(&sBeast, 28.0f)) {
            MergeShip_OoTSfx_Actor(&player->actor, OOT_NA_SE_EN_MGANON_WALK);
            Actor_SpawnFloorDustRing(play, &player->actor, &player->actor.world.pos, 14.0f, 3, 6.0f, 300, 10, true);
            Actor_RequestQuakeAndRumble(&player->actor, play, 1, 6);
        }
    }
    {
        static s16 sGrowlTimer = 60;

        if (--sGrowlTimer <= 0) {
            sGrowlTimer = (s16)Rand_ZeroFloat(80.0f) + 60;
            MergeShip_OoTSfx_Actor(&player->actor, OOT_NA_SE_EN_MGANON_UNARI);
        }
    }
}

// Beast Ganon's animations as Anchor sends them (BUG-0111, as SoH's): the one he plays, by its place here, and its frame.
static const char* sBeastSyncAnims[] = {
    gGanonGuardIdleAnim,       gGanonGuardWalkAnim, gGanonWalkAnim, gGanonRightSwordSwingAnim,
    gGanonLeftSwordSwingAnim,  gGanonDamageAnim,    gGanonRoarAnim, gGanonStunStartAnim,
    gGanonStunLoopAnim,
};

s32 MergeShip_Ganondorf_AnchorBeast(f32* frame) {
    s32 i;

    *frame = 0.0f;
    if (!MergeShip_Ganondorf_Enabled() || (sForm != MS_GANON_FORM_BEAST) || !sBeastReady) {
        return -1;
    }
    for (i = 0; i < ARRAY_COUNT(sBeastSyncAnims); i++) {
        if (sBeastAnim == sBeastSyncAnims[i]) {
            *frame = sBeast.curFrame;
            return i;
        }
    }
    return -1;
}

static void MergeShip_Ganondorf_DrawBeast(PlayState* play, Player* player) {
    static void* sEyes[] = { (void*)gGanonEyeOpenTex, (void*)gGanonEyeHalfTex, (void*)gGanonEyeClosedTex };
    static Vec3s sRemoteJoints[MS_BEAST_LIMBS];
    Vec3s* joints = sBeast.jointTable;

    if (player->actor.id != ACTOR_PLAYER) {
        // a remote Beast Ganon (Anchor): his own animation and frame, from his updates
        s32 index = -1;
        f32 frame = 0.0f;

        MergeShipAnchor_DummyGanondorfBeast(&player->actor, &index, &frame);
        if (!sBeastReady) {
            SkelAnime_InitFlex(play, &sBeast, (FlexSkeletonHeader*)gGanonSkel, (AnimationHeader*)gGanonGuardIdleAnim,
                               sBeastJoints, sBeastMorph, MS_BEAST_LIMBS);
            sBeastAnim = gGanonGuardIdleAnim;
            sBeastReady = true;
        }
        if ((index < 0) || (index >= ARRAY_COUNT(sBeastSyncAnims))) {
            index = 0;
        }
        SkelAnime_GetFrameData((AnimationHeader*)sBeastSyncAnims[index],
                               CLAMP((s32)frame, 0, (s32)Animation_GetLastFrame((void*)sBeastSyncAnims[index])),
                               sBeast.limbCount, sRemoteJoints);
        joints = sRemoteJoints;
    } else if (!sBeastReady) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyes[((play->gameplayFrames % 64) < 3) ? 2 : 0]);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    Matrix_Push();
    Matrix_Scale(MS_BEAST_SCALE, MS_BEAST_SCALE, MS_BEAST_SCALE, MTXMODE_APPLY);
    SkelAnime_DrawFlexOpa(play, sBeast.skeleton, joints, sBeast.dListCount, NULL, NULL, NULL);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

s32 MergeShip_Ganondorf_IsPlayer(Player* player) {
    return MergeShip_Ganondorf_IsPlayerGanondorf(player);
}

// ---- his masks --------------------------------------------------------------------------------------------------------
//
// Phantom Ganon's (his head limb's face plate, eyes, jewel and horns) and Ganon's face (the beast's head, jaw, snout,
// teeth, eyes, jewel and horns, on Beast Ganon's own skeleton in his guard stance), placed on Ganondorf's face in his head
// limb's space (tools: the previews of 2026-10-16) -- the same for the tower and the young Ganondorf.

static const MtxF sPhantomMaskOnHead = { {
    { 1.150000f, 0.000000f, 0.000000f, 0.000000f },
    { 0.000000f, 1.150000f, 0.000000f, 0.000000f },
    { 0.000000f, 0.000000f, 1.150000f, 0.000000f },
    { 150.000000f, 120.000000f, 0.000000f, 1.000000f },
} };
// (the guard stance's head limb, inverted, then on his face)
static const MtxF sBeastMaskOnHead = { {
    { 0.000000f, 0.000000f, 0.450000f, 0.000000f },
    { -0.075586f, -0.443606f, 0.000000f, 0.000000f },
    { 0.443606f, -0.075586f, 0.000000f, 0.000000f },
    { -918.459201f, 5337.429807f, 0.000000f, 1.000000f },
} };
static Vec3s sBeastMaskJoints[MS_BEAST_LIMBS];
static s32 sBeastMaskJointsReady;

// Beast Ganon's limbs (SkelAnime's, from 1) that make his face: jewel, snout, teeth, mouth, jaw; eyes, head, horns.
static s32 MergeShip_Ganondorf_BeastMaskLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                             void* data) {
    if (!(((limbIndex >= 16) && (limbIndex <= 21)) || ((limbIndex >= 31) && (limbIndex <= 34)))) {
        *dList = NULL;
    }
    return false;
}

s32 MergeShip_Ganondorf_DrawMask(PlayState* play, Player* player, s32 mask, s32 inHand) {
    static void* sEyes[] = { (void*)gGanonEyeOpenTex, (void*)gGanonEyeHalfTex, (void*)gGanonEyeClosedTex };
    const MsGanondorfPart* parts = MergeShip_AdultLink_TablesAdult() ? sGanondorfTowerParts
                                                                               : sGanondorfYoungParts;
    s32 count = MergeShip_AdultLink_TablesAdult() ? ARRAY_COUNT(sGanondorfTowerParts)
                                                             : ARRAY_COUNT(sGanondorfYoungParts);
    const MsGanondorfPart* head = NULL;
    s32 i;

    if (!MergeShip_Ganondorf_IsPlayerGanondorf(player)) {
        return false;
    }
    for (i = 0; i < count; i++) {
        if ((parts[i].linkLimb == PLAYER_LIMB_HEAD) && (strstr(parts[i].dList, "HeadDL") != NULL)) {
            head = &parts[i];
            break;
        }
    }
    if (head == NULL) {
        return true;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    // Link's head limb (on his face: the head fit undone; in his hand: the hand's own size), then his head limb's space
    if (inHand) {
        Matrix_Scale(1.0f / MS_GANONDORF_MASK_SCALE, 1.0f / MS_GANONDORF_MASK_SCALE, 1.0f / MS_GANONDORF_MASK_SCALE,
                     MTXMODE_APPLY);
    } else {
        const MsGanondorfHeadFit* fit = MergeShip_Ganondorf_HeadFit(player);

        Matrix_Scale(1.0f / fit->scale, 1.0f / fit->scale, 1.0f / fit->scale, MTXMODE_APPLY);
        Matrix_RotateZF(DEG_TO_RAD(-fit->pitchDeg), MTXMODE_APPLY);
        Matrix_Translate(-fit->x, -fit->y, 0.0f, MTXMODE_APPLY);
    }
    Matrix_Mult((MtxF*)&head->c, MTXMODE_APPLY);
    if (inHand) {
        // in his hand it is smaller, about its middle (his face's size there dwarfs the hand MM holds it in)
        f32 cx = (mask == PLAYER_MASK_DEKU) ? 1000.0f : 1200.0f;
        f32 cy = (mask == PLAYER_MASK_DEKU) ? -200.0f : -500.0f;

        Matrix_Translate(cx, cy, 0.0f, MTXMODE_APPLY);
        Matrix_Scale(0.6f, 0.6f, 0.6f, MTXMODE_APPLY);
        Matrix_Translate(-cx, -cy, 0.0f, MTXMODE_APPLY);
    }
    if (mask == PLAYER_MASK_DEKU) {
        Gfx* dl = MergeShip_Ganondorf_PhantomMaskDL();

        Matrix_Mult((MtxF*)&sPhantomMaskOnHead, MTXMODE_APPLY);
        if (dl != NULL) {
            Gfx_SetupDL25_Opa(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, Matrix_Finalize(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, dl);
        }
    } else {
        if (!sBeastReady) {
            SkelAnime_InitFlex(play, &sBeast, (FlexSkeletonHeader*)gGanonSkel, (AnimationHeader*)gGanonGuardIdleAnim,
                               sBeastJoints, sBeastMorph, MS_BEAST_LIMBS);
            sBeastAnim = gGanonGuardIdleAnim;
            sBeastReady = true;
        }
        if (!sBeastMaskJointsReady) {
            SkelAnime_GetFrameData((AnimationHeader*)gGanonGuardIdleAnim, 0, sBeast.limbCount, sBeastMaskJoints);
            sBeastMaskJointsReady = true;
        }
        Matrix_Mult((MtxF*)&sBeastMaskOnHead, MTXMODE_APPLY);
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyes[0]);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
        SkelAnime_DrawFlexOpa(play, sBeast.skeleton, sBeastMaskJoints, sBeast.dListCount,
                              MergeShip_Ganondorf_BeastMaskLimb, NULL, NULL);
    }
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

// BossGanon_Draw's light ball in his hand and BossGanon_DrawBigMagicCharge, with the bosses' effects
// (MsGanonMagic_DrawEffects).
static f32 sBigRand;
static f32 MergeShip_Ganondorf_BigRand(void) {
    // BossGanon_RandZeroOne's sequence, seeded per charge
    static s32 a, b, c;
    f32 r;

    if (sBigRand >= 0.0f) {
        a = (s32)sBigRand + 1;
        b = 0x71AC;
        c = 0x263A;
        sBigRand = -1.0f;
    }
    a = (a * 171) % 30269;
    b = (b * 172) % 30307;
    c = (c * 170) % 30323;
    r = (a / 30269.0f) + (b / 30307.0f) + (c / 30323.0f);
    while (r >= 1.0f) {
        r -= 1.0f;
    }
    return fabsf(r);
}

static void MergeShip_Ganondorf_DrawMagic(PlayState* play, Player* player) {
    MsGanondorfFx* fx = &gMsGanondorfFx;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frames = play->gameplayFrames;
    Vec3f charge;
    s32 i;

    MsGanonMagic_DrawEffects(play);

    OPEN_DISPS(gfxCtx);

    if (fx->handBallScale > 0.0f) {
        Gfx_SetupDL25_Xlu(gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
        if ((frames % 2) != 0) {
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);
        } else {
            gDPSetEnvColor(POLY_XLU_DISP++, 100, 255, 0, 0);
        }
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightBallMaterialDL);
        Matrix_Push();
        Matrix_Translate(gMsGanondorfHandPos.x, gMsGanondorfHandPos.y, gMsGanondorfHandPos.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(fx->handBallScale, fx->handBallScale, fx->handBallScale, MTXMODE_APPLY);
        Matrix_RotateZF(fx->handBallSpin, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfSquareDL);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 155, ((frames % 2) != 0) ? 100 : 80);
        Matrix_Translate(gMsGanondorfHandPos.x, player->actor.floorHeight, gMsGanondorfHandPos.z, MTXMODE_NEW);
        Matrix_Scale(fx->handBallScale * 0.75f, 1.0f, fx->handBallScale * 0.75f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightCoreDL);
        Matrix_Pop();
    }

    if (fx->bigCircle > 0.0f) {
        f32 yRot = BINANG_TO_RAD(player->actor.shape.rot.y);

        charge = gMsGanonMagicChargePos;
        Gfx_SetupDL25_Xlu(gfxCtx);
        Matrix_Push();
        // light flecks
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 170, (s8)fx->bigFleckAlpha);
        gDPSetEnvColor(POLY_XLU_DISP++, 200, 255, 0, 128);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, frames * -2, 0, 0x40, 0x40, 1, 0, frames * 0xA, 0x40, 0x40, -2, 0, 0,
                                      0xA));
        Matrix_Translate(charge.x, charge.y, charge.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(fx->bigFleck, fx->bigFleck, fx->bigFleck, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightFlecksDL);
        // background circle
        Matrix_Translate(charge.x, charge.y, charge.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(fx->bigCircle, fx->bigCircle, fx->bigCircle, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 0, 100, (s8)fx->bigCircleAlpha);
        gSPSegment(POLY_XLU_DISP++, 0x09,
                   (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x20, 0x20, 1, 0, frames * -4, 0x20, 0x20, 0, 0, 0, -4));
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfBigMagicBGCircleDL);
        // yellow dot
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 150, 170, 0, (s8)fx->bigCircleAlpha);
        gSPSegment(POLY_XLU_DISP++, 0x0A,
                   (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x20, 0x20, 1, frames * 2, frames * -0x14, 0x40, 0x40, 0, 0, 2,
                                      -0x14));
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfDotDL);
        // the light ball
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 100, 0);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightBallMaterialDL);
        Matrix_Translate(charge.x, charge.y, charge.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(fx->bigCore, fx->bigCore, fx->bigCore, MTXMODE_APPLY);
        Matrix_RotateZF((frames * 10.0f) / 1000.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfSquareDL);
        // the rays
        sBigRand = fx->bigSeed;
        Matrix_Translate(charge.x, charge.y, charge.z, MTXMODE_NEW);
        Matrix_RotateYF((frames * 10.0f) / 1000.0f, MTXMODE_APPLY);
        gDPSetEnvColor(POLY_XLU_DISP++, 200, 255, 0, 0);
        for (i = 0; i < fx->bigRays; i++) {
            f32 xzRot = (MergeShip_Ganondorf_BigRand() - 0.5f) * M_PIf * 1.5f;

            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (s8)fx->bigRayAlpha[i]);
            Matrix_Push();
            Matrix_RotateYF(xzRot + yRot, MTXMODE_APPLY);
            Matrix_RotateXFApply((MergeShip_Ganondorf_BigRand() - 0.5f) * M_PIf);
            Matrix_RotateZF(xzRot, MTXMODE_APPLY);
            Matrix_Translate(0.0f, 0.0f, 50.0f, MTXMODE_APPLY);
            Matrix_Scale(4.0f, 4.0f, 1.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightRayTriDL);
            Matrix_Pop();
        }
        Matrix_Pop();
    }

    CLOSE_DISPS(gfxCtx);
}

s32 MergeShip_Ganondorf_DrawPlayer(PlayState* play, void** skeleton, Vec3s* jointTable, s32 dListCount, s32 lod,
                                   void* overrideLimbDraw, void* postLimbDraw, Actor* actor) {
    Player* player = (Player*)actor;
    void* data = actor;
    s32 eyeIndex = 0;
    s32 firstPerson = (overrideLimbDraw == (void*)Player_OverrideLimbDrawGameplayFirstPerson);
    s32 adult;
    s32 i;

    if ((actor == NULL) || !MergeShip_Ganondorf_IsPlayerGanondorf(player)) {
        return false;
    }
    s32 form = MergeShip_Ganondorf_FormOf(player);

    adult = MergeShip_AdultLink_TablesAdult();
    sDrawingPhantom = (form == MS_GANON_FORM_PHANTOM);
    if ((player->actor.id == ACTOR_PLAYER) && (sPoseWeight > 0.0f) && (sPoseAnim != NULL) &&
        (sPoseSetFrame == (s32)play->gameplayFrames) && (sPoseAppliedFrame != (s32)play->gameplayFrames) &&
        (form != MS_GANON_FORM_BEAST)) {
        // this frame's ability pose, once (the joint table keeps it); joint positions too on the boss's own rig
        // (joint positions only for Phantom Ganon: his charge bends his spine double, and on rotations alone he hangs
        // upside down; the tower's animations stay on rotations -- their joints placed sank him and bent his arms)
        if (form == MS_GANON_FORM_PHANTOM) {
            MergeShip_Ganondorf_ApplyPose(jointTable, true, true, sGanondorfPhantomJoints, sGanondorfPhantomRootScale);
        } else if (adult) {
            MergeShip_Ganondorf_ApplyPose(jointTable, false, false, sGanondorfTowerJoints, sGanondorfTowerRootScale);
        } else {
            MergeShip_Ganondorf_ApplyPose(jointTable, false, false, sGanondorfYoungJoints, sGanondorfYoungRootScale);
        }
        sPoseAppliedFrame = play->gameplayFrames;
    } else if ((player->actor.id == ACTOR_PLAYER) && (sPoseSetFrame != (s32)play->gameplayFrames)) {
        sPoseSpear = false;
    }
    if (form == MS_GANON_FORM_PHANTOM) {
        sParts = sGanondorfPhantomParts;
        sPartPaths = sPhantomPaths;
        sPartCount = ARRAY_COUNT(sGanondorfPhantomParts);
        sJoints = sGanondorfPhantomJoints;
        sRootScale = sGanondorfPhantomRootScale;
        adult = false; // (no cape, no tower eyes)
    } else if (form == MS_GANON_FORM_BEAST) {
        // Link's skeleton, Ganondorf-sized and unseen, still places the hitboxes; Beast Ganon is drawn after it
        sParts = sGanondorfTowerParts;
        sPartPaths = sTowerPaths;
        sPartCount = 0;
        sJoints = sGanondorfTowerJoints;
        sRootScale = sGanondorfTowerRootScale;
        adult = false;
    } else if (adult) {
        sParts = sGanondorfTowerParts;
        sPartPaths = sTowerPaths;
        sPartCount = ARRAY_COUNT(sGanondorfTowerParts);
        sJoints = sGanondorfTowerJoints;
        sRootScale = sGanondorfTowerRootScale;
    } else {
        sParts = sGanondorfYoungParts;
        sPartPaths = sYoungPaths;
        sPartCount = ARRAY_COUNT(sGanondorfYoungParts);
        sJoints = sGanondorfYoungJoints;
        sRootScale = sGanondorfYoungRootScale;
    }
    sDrawingTower = adult;
    // (never 0: THA_AllocEnd masks a size of 0 to address 0, and every allocation after it lands below 0)
    sMtx = GRAPH_ALLOC(play->state.gfxCtx, MAX(sPartCount, 1) * sizeof(Mtx));
    memset(sFilled, 0, sizeof(sFilled));
    sHandItem[0] = sHandItem[1] = NULL;
    sLinkOverride = (OverrideLimbDrawFlex)overrideLimbDraw;
    sLinkPost = (PostLimbDrawFlex)postLimbDraw;
    sDrawingFirstPerson = firstPerson;

    SkelAnime_DrawFlexLod(play, skeleton, jointTable, dListCount, MergeShip_Ganondorf_OverrideLimb,
                          MergeShip_Ganondorf_PostLimb, data, lod);
    sLinkOverride = NULL;
    sLinkPost = NULL;
    if (adult && (sCape != NULL) && (player->actor.id == ACTOR_PLAYER)) {
        // BossGanon_Draw
        sCape->rightForearmPos = sCapeRightArm;
        sCape->leftForearmPos = sCapeLeftArm;
        sCape->rightShoulderPos = sCapeRightHand;
        sCape->leftShoulderPos = sCapeLeftHand;
        sCapePointsFrame = play->state.frames;
    }

    if (player->actor.id == ACTOR_PLAYER) {
        MergeShip_Ganondorf_DrawMagic(play, player);
        MergeShip_Ganondorf_DrawOrgan(play);
    }
    // First person, as Link's (Player_OverrideLimbDrawGameplayFirstPerson): aiming an item, or his light, his forearms
    // and fists (and what they hold); else nothing of him in front of the camera.
    sDrawingFirstPerson = false;
    if (firstPerson && ((form == MS_GANON_FORM_BEAST) ||
                        !((player->unk_AA5 == PLAYER_UNKAA5_3) || (gMsGanondorfFx.handBallScale > 0.0f)))) {
        return true;
    }
    if (form == MS_GANON_FORM_BEAST) {
        MergeShip_Ganondorf_DrawBeast(play, player);
        return true;
    }

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    if (form == MS_GANON_FORM_PHANTOM) {
        // BossGanondrof_Draw: segment 8 is his fading body's list (none), the environment his eyes' glow
        static Gfx sEmptyDL[] = { gsSPEndDisplayList() };

        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEmptyDL);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    } else if (adult) {
        // BossGanon_Draw
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gGanondorfNormalEyeTex);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)gGanondorfNormalEyeTex);
    } else {
        // EnViewer_Draw: his eyes blink with Link's
        const void* eyes = (eyeIndex == 1)                     ? gYoungGanondorfEyeHalfTex
                           : ((eyeIndex == 2) || (eyeIndex == 7)) ? gYoungGanondorfEyeClosedTex
                           : (eyeIndex == 6)                     ? gYoungGanondorfEyeLookingDownTex
                                                                 : gYoungGanondorfEyeOpenTex;

        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)eyes);
        gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)eyes);
    }
    gSPSegment(POLY_OPA_DISP++, 0x0D, (uintptr_t)sMtx);
    {
        // his colours (the menu's, or a remote player's): his lists with the leather and the cape's front recoloured
        u32 armor = (player->actor.id == ACTOR_PLAYER) ? MergeShip_Ganondorf_ArmorColor()
                                                        : MergeShipAnchor_DummyGanondorfColor(&player->actor, false);
        u32 cape = (player->actor.id == ACTOR_PLAYER) ? MergeShip_Ganondorf_CapeColor()
                                                       : MergeShipAnchor_DummyGanondorfColor(&player->actor, true);

        for (i = 0; i < sPartCount; i++) {
            if (sFilled[i] &&
                (!firstPerson || (sParts[i].linkLimb == PLAYER_LIMB_LEFT_FOREARM) ||
                 (sParts[i].linkLimb == PLAYER_LIMB_LEFT_HAND) || (sParts[i].linkLimb == PLAYER_LIMB_RIGHT_FOREARM) ||
                 (sParts[i].linkLimb == PLAYER_LIMB_RIGHT_HAND))) {
                Gfx* colored =
                    ((armor != 0) || (cape != 0)) ? MergeShip_Ganondorf_ColorDL(MergeShip_Ganondorf_PartDL(i), armor, cape) : NULL;

                gSPMatrix(POLY_OPA_DISP++, &sMtx[i], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, (colored != NULL) ? colored : (Gfx*)MergeShip_Ganondorf_PartDL(i));
            }
        }
    }

    // Blocking (R): his barrier -- BossGanon's light, a glowing disc in front of him.
    if ((player->actor.id == ACTOR_PLAYER) && gMsGanondorfFx.barrier && !firstPerson) {
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        Matrix_Push();
        Matrix_Translate(player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * 35.0f,
                         player->actor.world.pos.y + 50.0f,
                         player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * 35.0f, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(0.9f, 0.9f, 0.9f, MTXMODE_APPLY);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightBallMaterialDL);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 110);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 200, 0, 0);
        gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfSquareDL);
        Matrix_Pop();
    }

    // His eyes (BossGanon_PostLimbDraw: gGanondorfEyesDL on the jewel limb, translucent).
    if (adult && !firstPerson) {
        for (i = 0; i < sPartCount; i++) {
            if ((sParts[i].ganonLimb == GANON_LIMB_JEWEL) && sFilled[i]) {
                Gfx_SetupDL25_Xlu(play->state.gfxCtx);
                gSPMatrix(POLY_XLU_DISP++, &sMtx[i], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfEyesDL);
                break;
            }
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

// ---- his organ -----------------------------------------------------------------------------------------------------------
//
// BossGanon's intro: the organ at the room's origin, Ganondorf at (0, 112, -333) facing it, playing gGanondorfPlayOrganAnim.
// Here the organ is put down around him where he starts to play (at his size: the rig is the boss at 0.75), and the
// camera takes the wide view of him at it until he stops.

volatile u8 gMergeShipGanonOrgan;

#define MS_ORGAN_SCALE 0.75f
static PlayState* sOrganPlay;
static Vec3f sOrganOrigin;
static s16 sOrganYaw;
static s16 sOrganCam = -1;
static s16 sOrganRetCam = CAM_ID_MAIN;
static f32 sOrganFrame;
static s32 sOrganFrames; // frames since he sat down (the keyboard is brought to his hands at the third)
static Gfx sOrganEmptyDL[] = { gsSPEndDisplayList() };

// organ space -> world
static void MergeShip_Ganondorf_OrganPoint(f32 x, f32 y, f32 z, Vec3f* out) {
    f32 s = Math_SinS(sOrganYaw);
    f32 c = Math_CosS(sOrganYaw);

    x *= MS_ORGAN_SCALE;
    y *= MS_ORGAN_SCALE;
    z *= MS_ORGAN_SCALE;
    out->x = sOrganOrigin.x + x * c + z * s;
    out->y = sOrganOrigin.y + y;
    out->z = sOrganOrigin.z - x * s + z * c;
}

static void MergeShip_Ganondorf_StopOrgan(PlayState* play) {
    if ((sOrganCam > CAM_ID_MAIN) && (play == sOrganPlay) && (play != NULL)) {
        Play_ClearCamera(play, sOrganCam);
        Play_ChangeCameraStatus(play, sOrganRetCam, CAM_STATUS_ACTIVE);
    }
    sOrganCam = -1;
    sOrganActive = false;
    sOrganPlay = NULL;
}

static void MergeShip_Ganondorf_UpdateOrgan(PlayState* play, Player* player) {
    s32 ganondorf = MergeShip_Ganondorf_IsPlayerGanondorf(player) && (sForm == MS_GANON_FORM_GANONDORF);
    s32 playing = (player->stateFlags2 & PLAYER_STATE2_USING_OCARINA) && ganondorf;

    // his notes and his songs' playback (the fanfares) sound on the organ whenever he is the one playing
    gMergeShipGanonOrgan = ganondorf;
    Vec3f eye;
    Vec3f at;
    f32 last;

    if (sOrganActive && (sOrganPlay != play)) {
        // a new scene: its cameras are new too
        sOrganCam = -1;
        sOrganPlay = play;
        MergeShip_Ganondorf_StopOrgan(NULL);
    }
    if (playing && !sOrganActive) {
        sOrganActive = true;
        sOrganPlay = play;
        sOrganYaw = player->actor.shape.rot.y;
        sOrganFrame = 0.0f;
        sOrganFrames = 0;
        // he sits where BossGanon sits: the organ around that point
        sOrganOrigin = player->actor.world.pos;
        {
            Vec3f seat;

            MergeShip_Ganondorf_OrganPoint(0.0f, 112.0f, -333.0f, &seat);
            sOrganOrigin.x -= seat.x - sOrganOrigin.x;
            sOrganOrigin.y -= seat.y - sOrganOrigin.y;
            sOrganOrigin.z -= seat.z - sOrganOrigin.z;
        }
        // (a prerendered room keeps its fixed camera: a 3D one there shows the room's real geometry)
        if ((play->csCtx.state == CS_STATE_IDLE) &&
            !((play->roomCtx.curRoom.roomShape != NULL) && (play->roomCtx.curRoom.roomShape->base.type == ROOM_SHAPE_TYPE_IMAGE))) {
            sOrganRetCam = Play_GetActiveCamId(play);
            sOrganCam = Play_CreateSubCamera(play);
            if (sOrganCam > CAM_ID_MAIN) {
                Camera* sub = Play_GetCamera(play, sOrganCam);

                memcpy(sub, Play_GetCamera(play, sOrganRetCam), sizeof(Camera));
                sub->camId = sOrganCam;
                Play_ChangeCameraStatus(play, sOrganRetCam, CAM_STATUS_WAIT);
                Play_ChangeCameraStatus(play, sOrganCam, CAM_STATUS_ACTIVE);
                // (left as copied, unk_14C bit 0 re-applies the floor's scene camera every frame, and unk_14A bit 0
                // refuses a new setting: MergeShip_TransformCam_Start)
                sub->stateFlags &= ~(0x40 | 0x1);
                sub->behaviorFlags = 0;
                Camera_ChangeSetting(sub, CAM_SET_FREE0);
            }
        }
    } else if (!playing && sOrganActive) {
        MergeShip_Ganondorf_StopOrgan(play);
    }
    if (!sOrganActive) {
        return;
    }
    // he plays it as BossGanon does
    last = Animation_GetLastFrame((void*)gGanondorfPlayOrganAnim);
    sOrganFrame += 1.0f;
    if (sOrganFrame > last) {
        sOrganFrame -= last;
    }
    MergeShip_Ganondorf_SetPose(gGanondorfPlayOrganAnim, sOrganFrame, 1.0f);
    player->actor.shape.rot.y = sOrganYaw;
    // once he is posed, the keyboard is brought to his hands (BossGanon stands higher than the room's floor)
    if (++sOrganFrames == 3) {
        Vec3f keys;

        MergeShip_Ganondorf_OrganPoint(0.0f, 110.0f, -385.0f, &keys);
        sOrganOrigin.y += (gMsGanondorfHandPos.y - 4.0f) - keys.y;
    }
    // the camera on him at the organ: from behind his left shoulder, him, the keyboard and the pipes above
    if (sOrganCam > CAM_ID_MAIN) {
        MergeShip_Ganondorf_OrganPoint(-230.0f, 250.0f, -120.0f, &eye);
        MergeShip_Ganondorf_OrganPoint(0.0f, 185.0f, -370.0f, &at);
        Play_SetCameraAtEye(play, sOrganCam, &at, &eye);
    }
}

static void MergeShip_Ganondorf_DrawOrgan(PlayState* play) {
    Gfx* organ;

    if (!sOrganActive || (sOrganPlay != play) || ((organ = MergeShip_Ganondorf_OrganDL()) == NULL)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    // EnGanonOrgan_Draw: segments 8 and 9 hold the room's fade (none)
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sOrganEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sOrganEmptyDL);
    Matrix_Push();
    Matrix_Translate(sOrganOrigin.x, sOrganOrigin.y, sOrganOrigin.z, MTXMODE_NEW);
    Matrix_RotateYF(BINANG_TO_RAD(sOrganYaw), MTXMODE_APPLY);
    Matrix_Scale(MS_ORGAN_SCALE, MS_ORGAN_SCALE, MS_ORGAN_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_Finalize(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, organ);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

static OoTEnGanonMant* MergeShip_Ganondorf_FindCape(PlayState* play) {
    Actor* actor;
    s32 cat;

    if (sCape == NULL) {
        return NULL;
    }
    for (cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (actor = play->actorCtx.actorLists[cat].first; actor != NULL; actor = actor->next) {
            if ((actor == &sCape->actor) && (actor->id == ACTOR_OOT_EN_GANON_MANT)) {
                return sCape;
            }
        }
    }
    return NULL;
}

void MergeShip_Ganondorf_PlayerUpdate(PlayState* play, Player* player) {
    s32 want = (player->actor.id == ACTOR_PLAYER) && MergeShip_Ganondorf_IsPlayerGanondorf(player) &&
               MergeShip_AdultLink_TablesAdult();

    // his masks' icons in the Deku Mask's and the Fierce Deity's Mask's places, his hand in the swords' (his B is his
    // magic) -- as SoH's (AdultMask.hpp's archive)
    if (player->actor.id == ACTOR_PLAYER) {
        static const s32 sSwapped[] = { ITEM_MASK_DEKU,         ITEM_MASK_FIERCE_DEITY,      ITEM_SWORD_KOKIRI,
                                        ITEM_SWORD_RAZOR,       ITEM_SWORD_GILDED,           ITEM_OOT_SWORD_MASTER,
                                        ITEM_OOT_SWORD_BIGGORON, ITEM_OOT_SWORD_GIANTS_KNIFE, ITEM_OOT_SWORD_KOKIRI };
        static const char* sOurs[] = { "__OTR__mergeship/ganondorf/gPhantomGanonMaskIconTex",
                                       "__OTR__mergeship/ganondorf/gGanonMaskIconTex",
                                       "__OTR__mergeship/ganondorf/gGanondorfHandIconTex",
                                       "__OTR__mergeship/ganondorf/gGanondorfHandIconTex",
                                       "__OTR__mergeship/ganondorf/gGanondorfHandIconTex",
                                       "__OTR__mergeship/ganondorf/gGanondorfHandIconTex",
                                       "__OTR__mergeship/ganondorf/gIronKnuckleAxeIconTex",
                                       "__OTR__mergeship/ganondorf/gIronKnuckleAxeIconTex",
                                       "__OTR__mergeship/ganondorf/gGanondorfHandIconTex" };
        static TexturePtr sTheirs[ARRAY_COUNT(sSwapped)];
        static s32 sIconsSwapped;
        s32 swap = MergeShip_Ganondorf_Enabled() && MergeShip_AdultMask_ArtMounted();
        s32 i;

        if (swap != sIconsSwapped) {
            for (i = 0; i < ARRAY_COUNT(sSwapped); i++) {
                if (swap) {
                    sTheirs[i] = gItemIcons[sSwapped[i]];
                    gItemIcons[sSwapped[i]] = (TexturePtr)sOurs[i];
                } else {
                    gItemIcons[sSwapped[i]] = sTheirs[i];
                }
            }
            sIconsSwapped = swap;
            for (i = EQUIP_SLOT_B; i <= EQUIP_SLOT_C_RIGHT; i++) {
                Interface_LoadItemIconImpl(play, i);
            }
        }
    }
    // (BUG-0112: his masks are the Deku Mask and the Fierce Deity's Mask where they are found -- never given at the
    // start: given, they were real masks in the shared inventory, and Anchor gave them to everyone)
    if ((player->actor.id == ACTOR_PLAYER) && MergeShip_Ganondorf_Enabled()) {
    } else if (player->actor.id == ACTOR_PLAYER) {
        sForm = MS_GANON_FORM_GANONDORF;
    }
    if ((player->actor.id == ACTOR_PLAYER) && (sForm == MS_GANON_FORM_BEAST) &&
        MergeShip_Ganondorf_IsPlayerGanondorf(player)) {
        MergeShip_Ganondorf_UpdateBeast(play, player);
    }
    if (player->actor.id == ACTOR_PLAYER) {
        static u32 sEffectsFrame = 0xFFFFFFFF;

        // the bosses' effects his magic leaves (once a frame); his colours' copies kept in check
        if (sEffectsFrame != play->gameplayFrames) {
            sEffectsFrame = play->gameplayFrames;
            MsGanonMagic_UpdateEffects(play);
            MergeShip_Ganondorf_ColorFrameBegin();
        }
        // an ability sets its pose and its magic's look again each frame it runs
        if (!MergeShip_Ganondorf_Enabled()) {
            memset(&gMsGanondorfFx, 0, sizeof(gMsGanondorfFx));
        }
        MergeShip_Ganondorf_UpdateOrgan(play, player);
    }
    want = want && (sForm == MS_GANON_FORM_GANONDORF);

    sCape = MergeShip_Ganondorf_FindCape(play);
    if (!want) {
        if (sCape != NULL) {
            Actor_Kill(&sCape->actor);
            sCape = NULL;
        }
        return;
    }
    if (sCape == NULL) {
        // BossGanon_Init: a child actor of his; the miscellaneous category, not the boss one (a boss in the room
        // changes what the scene does).
        sCape = (OoTEnGanonMant*)Actor_SpawnAsChild(&play->actorCtx, &player->actor, play, ACTOR_OOT_EN_GANON_MANT,
                                                 player->actor.world.pos.x, player->actor.world.pos.y,
                                                 player->actor.world.pos.z, 0, 0, 0, 1);
        if (sCape == NULL) {
            return;
        }
        Actor_ChangeCategory(play, &play->actorCtx, &sCape->actor, ACTORCAT_MISC);
        // BossGanon_SetupIntroCutscene's settings for him standing
        sCape->backPush = -2.0f;
        sCape->backSwayMagnitude = 0.25f;
        sCape->sideSwayMagnitude = -1.0f;
        sCape->minDist = 0.0f;
        sCape->gravity = -3.0f;
        sCapePointsFrame = -1;
    }
    sCape->actor.world.pos = player->actor.world.pos;
    // (BUG-0132: riding, his own floor height goes stale -- he does not walk -- and downhill it stood above the cape
    // and shoved it straight up: on the horse, the horse's floor)
    sCape->minY = (((player->stateFlags1 & PLAYER_STATE1_800000) && (player->rideActor != NULL)) ? player->rideActor->floorHeight
                                                                                  : player->actor.floorHeight) +
                  2.0f;
}

// His guard with his cape (the user, 2026-10-16), as BossGanon_SetupBlock / BossGanon_Block move it: 0 up (it wraps
// over his arm; again at each thing turned back), 1 held, 2 let go (back to his shoulders), 3 his standing cape.
void MergeShip_Ganondorf_CapeBlock(s32 phase) {
    if (sCape == NULL) {
        return;
    }
    switch (phase) {
        case 0:
            sCape->attachLeftArmTimer = 10.0f;
            // fallthrough
        case 1:
            sCape->backPush = -9.0f;
            sCape->backSwayMagnitude = 0.25f;
            sCape->sideSwayMagnitude = -2.0f;
            sCape->minDist = 13.0f;
            break;
        case 2:
            sCape->attachShouldersTimer = 15.0f;
            sCape->sideSwayMagnitude = -13.0f;
            break;
        default:
            // BossGanon_SetupIntroCutscene's settings for him standing
            sCape->backPush = -2.0f;
            sCape->backSwayMagnitude = 0.25f;
            sCape->sideSwayMagnitude = -1.0f;
            sCape->minDist = 0.0f;
            break;
    }
}

// ---- his attacks --------------------------------------------------------------------------------------------------------
//
// The bosses' own (MergeShipGanondorfPlayer.inc and the magic actor, ovl_MS_Ganon_Magic): Link's sword never comes out.

void MergeShip_Ganondorf_OnMeleeAttack(PlayState* play, Player* player, s32 meleeWeaponAnimation) {
}

// Player_UseItem: his masks put on or taken off (true: the item was his mask, or one Beast Ganon cannot use).
s32 MergeShip_Ganondorf_StartTransform(PlayState* play, Player* player, s32 form, s32 mask); // the .inc

s32 MergeShip_Ganondorf_UseItem(PlayState* play, Player* player, s32 item) {
    s32 target;

    if ((player->actor.id != ACTOR_PLAYER) || !MergeShip_Ganondorf_IsPlayerGanondorf(player)) {
        return false;
    }
    target = (item == ITEM_MASK_DEKU) ? MS_GANON_FORM_PHANTOM : (item == ITEM_MASK_FIERCE_DEITY) ? MS_GANON_FORM_BEAST : -1;
    if (target < 0) {
        // no sword for him (his attacks are his own), and no Goron or Zora; Beast Ganon uses only bottles besides
        if (((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_GILDED)) || (item == ITEM_SWORD_GREAT_FAIRY) ||
            ITEM_IS_OOT_EQUIP_SWORD(item) || // (Ocarina of Time's swords: the user, 2026-10-17, "all swords need to")
            (item == ITEM_MASK_GORON) || (item == ITEM_MASK_ZORA)) {
            return true;
        }
        return (sForm == MS_GANON_FORM_BEAST) && (item != ITEM_NONE) &&
               !((item >= ITEM_BOTTLE) && (item <= ITEM_OBABA_DRINK));
    }
    // MM's mask transformation (MergeShipGanondorfPlayer.inc): his form changes at its flash
    MergeShip_Ganondorf_StartTransform(play, player, (sForm == target) ? MS_GANON_FORM_GANONDORF : target,
                                       (item == ITEM_MASK_DEKU) ? PLAYER_MASK_DEKU : PLAYER_MASK_FIERCE_DEITY);
    return true;
}

// Player_GetHeight: as in Hyrule (the user, 2026-10-17: "make the termina camera like oots, too far back for
// ganondorf"): SoH's camera frames him by Link's own heights (the adult's 68, the child's 44, whatever his form), so
// Termina's does too -- the height Link's would be.
f32 MergeShip_Ganondorf_Height(Player* player, f32 height) {
    return height;
}

// His axe (the user, 2026-10-17; SoH's MergeShip_Ganondorf_AxeOut): the Iron Knuckle's, in Ocarina of Time's Biggoron
// Sword's place -- in Termina its Biggoron Sword or Giant's Knife on B (Ocarina of Time's equipment, MergeShipOoTEquip.h).
s32 MergeShip_Ganondorf_AxeOut(Player* player) {
    s32 item;

    if ((player == NULL) || (player->actor.id != ACTOR_PLAYER) || !MergeShip_Ganondorf_IsPlayerGanondorf(player) ||
        (sForm != MS_GANON_FORM_GANONDORF) || !MergeShip_AdultLink_TablesAdult() || (gPlayState == NULL)) {
        return false;
    }
    // (MergeShipOoTEquip.h: the human's B sword -- MS_OOTEQ_SWORD_LONG, Biggoron's Sword or the Giant's Knife)
    item = Inventory_GetBtnBItem(gPlayState);
    return (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip.sword == MS_OOTEQ_SWORD_LONG) ||
           (item == ITEM_OOT_SWORD_BIGGORON) || (item == ITEM_OOT_SWORD_GIANTS_KNIFE);
}

f32 MergeShip_Ganondorf_PauseScale(Player* player) {
    if (!MergeShip_Ganondorf_IsPlayerGanondorf(player)) {
        return 1.0f;
    }
    if (sForm == MS_GANON_FORM_BEAST) {
        return 0.5f; // (Beast Ganon: about twice the tower Ganondorf's height)
    }
    // the rig draws both Ganondorfs 1.3 adult Links tall (tools/ganon_rig.py TARGET); the young Link's frame is for
    // his 44 against the adult's 68 (Player_GetHeight)
    return (MergeShip_AdultLink_TablesAdult() ? 1.0f : (44.0f / 68.0f)) / 1.3f;
}

// func_80837918: Link's swings carry no blade for him (none should start; his own attacks do the hitting).
s32 MergeShip_Ganondorf_NoMeleeDamage(Player* player) {
    return MergeShip_Ganondorf_IsPlayer(player);
}

// EnArrow_Draw: (his magic is its own actor now)


// ---- voice -----------------------------------------------------------------------------------------------------------

// Link's voice bank (NA_SE_VO_LI_*, by index, both ages) -> Ganondorf's voice (BossGanon), 0: silent.
static const u16 sGanondorfVoice[0x20] = {
    /* SWORD_N */ OOT_NA_SE_EN_GANON_AT_RETURN,
    /* SWORD_L */ OOT_NA_SE_EN_GANON_THROW,
    /* LASH */ OOT_NA_SE_EN_GANON_THROW,
    /* HANG */ OOT_NA_SE_EN_GANON_THROW,
    /* CLIMB_END */ OOT_NA_SE_EN_GANON_THROW,
    /* DAMAGE_S */ OOT_NA_SE_EN_GANON_DAMAGE1,
    /* FREEZE */ OOT_NA_SE_EN_GANON_DAMAGE2,
    /* FALL_S */ OOT_NA_SE_EN_GANON_DAMAGE1,
    /* FALL_L */ OOT_NA_SE_EN_GANON_DAMAGE2,
    /* BREATH_REST */ 0,
    /* BREATH_DRINK */ 0,
    /* DOWN */ OOT_NA_SE_EN_GANON_DOWN,
    /* TAKEN_AWAY */ OOT_NA_SE_EN_GANON_DOWN,
    /* HELD */ OOT_NA_SE_EN_GANON_DAMAGE2,
    /* SNEEZE */ 0,
    /* SWEAT */ 0,
    /* DRINK */ 0,
    /* RELAX */ 0,
    /* SWORD_PUTAWAY */ OOT_NA_SE_EN_GANON_LAUGH,
    /* GROAN */ OOT_NA_SE_EN_GANON_DAMAGE1,
    /* AUTO_JUMP */ OOT_NA_SE_EN_GANON_THROW,
    /* MAGIC_NALE */ OOT_NA_SE_EN_GANON_LAUGH,
    /* SURPRISE */ OOT_NA_SE_EN_GANON_DAMAGE1,
    /* MAGIC_FROL */ OOT_NA_SE_EN_GANON_LAUGH,
    /* PUSH */ OOT_NA_SE_EN_GANON_THROW,
    /* HOOKSHOT_HANG */ OOT_NA_SE_EN_GANON_THROW,
    /* LAND_DAMAGE_S */ OOT_NA_SE_EN_GANON_DAMAGE1,
    /* 0x1B */ 0,
    /* MAGIC_ATTACK */ OOT_NA_SE_EN_GANON_AT_RETURN,
    /* BL_DOWN */ OOT_NA_SE_EN_GANON_DEAD,
    /* DEMO_DAMAGE */ OOT_NA_SE_EN_GANON_DAMAGE2,
    /* ELECTRIC_SHOCK_LV */ OOT_NA_SE_EN_GANON_DAMAGE2,
};

s32 MergeShip_Ganondorf_PlayVoice(Actor* actor, u16 sfxId) {
    u16 ganon;

    if ((actor == NULL) || (actor->id != ACTOR_PLAYER) || !MergeShip_Ganondorf_IsPlayerGanondorf((Player*)actor) ||
        (sfxId < NA_SE_VO_LI_SWORD_N) || (sfxId >= NA_SE_VO_LI_SWORD_N + 2 * 0x20)) {
        return false;
    }
    ganon = sGanondorfVoice[(sfxId - NA_SE_VO_LI_SWORD_N) % 0x20];
    if ((ganon != 0) && (sForm == MS_GANON_FORM_PHANTOM)) {
        ganon = (ganon == OOT_NA_SE_EN_GANON_LAUGH)                                      ? OOT_NA_SE_EN_FANTOM_LAUGH
                : ((ganon == OOT_NA_SE_EN_GANON_DAMAGE1) || (ganon == OOT_NA_SE_EN_GANON_DAMAGE2)) ? OOT_NA_SE_EN_FANTOM_DAMAGE
                : ((ganon == OOT_NA_SE_EN_GANON_DOWN) || (ganon == OOT_NA_SE_EN_GANON_DEAD))     ? OOT_NA_SE_EN_FANTOM_DEAD
                                                                                        : OOT_NA_SE_EN_FANTOM_VOICE;
    } else if ((ganon != 0) && (sForm == MS_GANON_FORM_BEAST)) {
        ganon = ((ganon == OOT_NA_SE_EN_GANON_DAMAGE1) || (ganon == OOT_NA_SE_EN_GANON_DAMAGE2)) ? OOT_NA_SE_EN_MGANON_DAMAGE
                : ((ganon == OOT_NA_SE_EN_GANON_DOWN) || (ganon == OOT_NA_SE_EN_GANON_DEAD))   ? OOT_NA_SE_EN_MGANON_DEAD1
                : (ganon == OOT_NA_SE_EN_GANON_LAUGH)                                     ? OOT_NA_SE_EN_MGANON_ROAR
                                                                                      : OOT_NA_SE_EN_MGANON_BREATH;
    }
    if (ganon != 0) {
        MergeShip_OoTSfx_Actor(actor, ganon);
    }
    return true;
}


