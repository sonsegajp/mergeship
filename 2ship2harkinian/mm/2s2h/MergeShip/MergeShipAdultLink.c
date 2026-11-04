// MergeShip2 -- Ocarina of Time's adult Link as Majora's Mask's human (MergeShipAdultLink.h): the human's rows of the
// form-indexed body tables, the adult's measurements, and his equipment as Ocarina of Time draws it.
//
// References: Ocarina of Time's player in Shipwright (soh/src/code/z_player_lib.c: gPlayerSkelHeaders, sBootData,
// the sPlayer*DLs / sSheath*DLs / sFirstPerson*DLs tables, sEyeTextures, sMouthTextures, sTunicColors, sGauntletColors,
// sBootDListGroups, D_80126038..D_80126068, Player_DrawImpl, Player_OverrideLimbDrawGameplayDefault / FirstPerson;
// soh/src/overlays/actors/ovl_player_actor/z_player.c: sAgeProperties[LINK_AGE_ADULT], the KNOB_ANIM_ADULT door
// animations; z_actor.c Player_GetHeight) and Majora's Mask's in 2ship2harkinian (the same tables, form-indexed).

#include "2s2h/MergeShip/MergeShipAdultMask.h" // MergeShip2: the Adult Mask
#include "global.h"
#include "2s2h/MergeShip/MergeShipAdultLink.h"
#include "2s2h/MergeShip/MergeShipHost.h"
#include "2s2h/MergeShip/MergeShipAnchorColors.h"
#include "2s2h/MergeShip/MergeShipAssets.h"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipOoTItems.h" // MERGESHIP_OOT_FAIRY_OCARINA_HAND_DL (BUG-0031)
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h" // MergeShip2: DrawOnLimb's own child
#include "objects/object_link_child/object_link_child.h"
#include "objects/gameplay_keep/gameplay_keep.h" // gKokiriSwordDL, gRazorSwordDL
#include <libultraship/bridge/consolevariablebridge.h>
#include <mergeship/MaskFit.h> // MergeShip2: each MM mask on the adult's head, shared with Hyrule
#include <stdio.h>
#include <string.h>

void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction);

// ---- the tables whose human row is swapped (z_player.c, z_player_lib.c) ------------------------------------------------
extern PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX];
extern Gfx* gPlayerWaistDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerSheath12DLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerSheath13DLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerSheath14DLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerLeftHandTwoHandSwordDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerLeftHandOpenDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerLeftHandClosedDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerLeftHandOneHandSwordDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerRightHandOpenDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerRightHandClosedDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerRightHandBowDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerRightHandInstrumentDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerRightHandHookshotDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerLeftHandBottleDLs[2 * PLAYER_FORM_MAX];
extern Gfx* sPlayerFirstPersonLeftForearmDLs[PLAYER_FORM_MAX];
extern Gfx* sPlayerFirstPersonLeftHandDLs[PLAYER_FORM_MAX];
extern Gfx* sPlayerFirstPersonRightShoulderDLs[PLAYER_FORM_MAX];
extern Gfx* sPlayerFirstPersonRightHandDLs[PLAYER_FORM_MAX];
extern Gfx* sPlayerFirstPersonRightHandHookshotDLs[PLAYER_FORM_MAX];
extern TexturePtr sPlayerEyesTextures[PLAYER_FORM_MAX][PLAYER_EYES_MAX];
extern TexturePtr sPlayerMouthTextures[PLAYER_FORM_MAX][PLAYER_MOUTH_MAX];
extern Vec3f D_801C08C0[PLAYER_FORM_MAX];
extern f32 D_801C08FC[PLAYER_FORM_MAX];
extern f32 D_801C0910[PLAYER_FORM_MAX];
extern f32 D_801C0924[PLAYER_FORM_MAX];
extern f32 D_801C0938[PLAYER_FORM_MAX];
extern s32 sPlayerLod;
extern PlayerModelType sPlayerLeftHandType;
extern PlayerModelType sPlayerRightHandType;
void func_8012301C(Actor* thisx, PlayState* play2);

#define OOT_EQUIP (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip)

// ---- Ocarina of Time's adult (object_link_boy) ----------------------------------------------------------------------------
#define BOY(name) OOT_ASSET("objects/object_link_boy/" name)
#define BOY_DL(sym) static const ALIGN_ASSET(2) char sym[] = BOY(#sym)

static const ALIGN_ASSET(2) char gLinkAdultSkelPath[] = BOY("gLinkAdultSkel");
BOY_DL(gLinkAdultWaistNearDL);
BOY_DL(gLinkAdultWaistFarDL);
BOY_DL(gLinkAdultLeftHandNearDL);
BOY_DL(gLinkAdultLeftHandFarDL);
BOY_DL(gLinkAdultLeftHandClosedNearDL);
BOY_DL(gLinkAdultLeftHandClosedFarDL);
BOY_DL(gLinkAdultLeftHandHoldingMasterSwordNearDL);
BOY_DL(gLinkAdultLeftHandHoldingMasterSwordFarDL);
BOY_DL(gLinkAdultLeftHandHoldingBgsNearDL);
BOY_DL(gLinkAdultLeftHandHoldingBgsFarDL);
BOY_DL(gLinkAdultHandHoldingBrokenGiantsKnifeDL);
BOY_DL(gLinkAdultHandHoldingBrokenGiantsKnifeFarDL);
BOY_DL(gLinkAdultLeftHandOutNearDL);
BOY_DL(gLinkAdultRightHandNearDL);
BOY_DL(gLinkAdultRightHandFarDL);
BOY_DL(gLinkAdultRightHandClosedNearDL);
BOY_DL(gLinkAdultRightHandClosedFarDL);
BOY_DL(gLinkAdultRightHandHoldingBowNearDL);
BOY_DL(gLinkAdultRightHandHoldingBowFarDL);
BOY_DL(gLinkAdultRightHandHoldingOotNearDL);
BOY_DL(gLinkAdultRightHandHoldingOotFarDL);
BOY_DL(gLinkAdultRightHandHoldingHookshotNearDL);
BOY_DL(gLinkAdultRightHandHoldingHookshotFarDL);
BOY_DL(gLinkAdultRightHandHoldingHylianShieldNearDL);
BOY_DL(gLinkAdultRightHandHoldingHylianShieldFarDL);
BOY_DL(gLinkAdultRightHandHoldingMirrorShieldNearDL);
BOY_DL(gLinkAdultRightHandHoldingMirrorShieldFarDL);
BOY_DL(gLinkAdultRightHandHoldingBowFirstPersonDL);
BOY_DL(gLinkAdultRightArmOutNearDL);
BOY_DL(gLinkAdultRightHandOutNearDL);
BOY_DL(gLinkAdultRightShoulderNearDL);
BOY_DL(gLinkAdultLeftArmOutNearDL);
BOY_DL(gLinkAdultMasterSwordAndSheathNearDL);
BOY_DL(gLinkAdultMasterSwordAndSheathFarDL);
BOY_DL(gLinkAdultSheathNearDL);
BOY_DL(gLinkAdultSheathFarDL);
BOY_DL(gLinkAdultHylianShieldSwordAndSheathNearDL);
BOY_DL(gLinkAdultHylianShieldSwordAndSheathFarDL);
BOY_DL(gLinkAdultMirrorShieldSwordAndSheathNearDL);
BOY_DL(gLinkAdultMirrorShieldSwordAndSheathFarDL);
BOY_DL(gLinkAdultHylianShieldAndSheathNearDL);
BOY_DL(gLinkAdultHylianShieldAndSheathFarDL);
BOY_DL(gLinkAdultMirrorShieldAndSheathNearDL);
BOY_DL(gLinkAdultMirrorShieldAndSheathFarDL);
BOY_DL(gLinkAdultLeftIronBootDL);
BOY_DL(gLinkAdultRightIronBootDL);
BOY_DL(gLinkAdultLeftHoverBootDL);
BOY_DL(gLinkAdultRightHoverBootDL);
BOY_DL(gLinkAdultLeftGauntletPlate1DL);
BOY_DL(gLinkAdultLeftGauntletPlate2DL);
BOY_DL(gLinkAdultLeftGauntletPlate3DL);
BOY_DL(gLinkAdultRightGauntletPlate1DL);
BOY_DL(gLinkAdultRightGauntletPlate2DL);
BOY_DL(gLinkAdultRightGauntletPlate3DL);
BOY_DL(gLinkAdultEyesOpenTex);
BOY_DL(gLinkAdultEyesHalfTex);
BOY_DL(gLinkAdultEyesClosedfTex);
BOY_DL(gLinkAdultEyesRollLeftTex);
BOY_DL(gLinkAdultEyesRollRightTex);
BOY_DL(gLinkAdultEyesShockTex);
BOY_DL(gLinkAdultEyesUnk1Tex);
BOY_DL(gLinkAdultEyesUnk2Tex);
BOY_DL(gLinkAdultMouth1Tex);
BOY_DL(gLinkAdultMouth2Tex);
BOY_DL(gLinkAdultMouth3Tex);
BOY_DL(gLinkAdultMouth4Tex);

// ---- the swap ---------------------------------------------------------------------------------------------------------------

// Every DL table row swapped, and the adult's entry for it (MM rows: [2 * form] near, [2 * form + 1] far).
typedef struct {
    Gfx** slot;
    const char* adult;
    Gfx* young; // MM's, saved at the first swap
} MsAdultSlot;

#define HUMAN_NEAR (2 * PLAYER_FORM_HUMAN)
#define HUMAN_FAR (2 * PLAYER_FORM_HUMAN + 1)
#define ROWS2(table) MERGESHIP_ADULT_ASSERT_TABLE(table, 2 * PLAYER_FORM_MAX)
#define ROWS1(table) MERGESHIP_ADULT_ASSERT_TABLE(table, PLAYER_FORM_MAX)
// The swap writes the human's row of each table, as declared above with PLAYER_FORM_MAX rows (their definitions in
// z_player_lib.c / z_player.c use the same bound). Pinned here so a change to PLAYER_FORM_HUMAN or PLAYER_FORM_MAX, or to
// an LOD layout, fails at build time instead of writing past a row (the lesson of SoH's MergeShipForms.h). Nothing here
// grows PLAYER_FORM_MAX: the adult is a row swap, not a sixth form.
#define MERGESHIP_ADULT_ASSERT_TABLE(table, n) \
    _Static_assert(ARRAY_COUNT(table) == (n), #table " must have one row (or LOD pair) per PlayerTransformation")

ROWS2(gPlayerWaistDLs);
ROWS2(gPlayerSheath12DLs);
ROWS2(gPlayerSheath13DLs);
ROWS2(gPlayerSheath14DLs);
ROWS2(gPlayerLeftHandTwoHandSwordDLs);
ROWS2(gPlayerLeftHandOpenDLs);
ROWS2(gPlayerLeftHandClosedDLs);
ROWS2(gPlayerLeftHandOneHandSwordDLs);
ROWS2(gPlayerRightHandOpenDLs);
ROWS2(gPlayerRightHandClosedDLs);
ROWS2(gPlayerRightHandBowDLs);
ROWS2(gPlayerRightHandInstrumentDLs);
ROWS2(gPlayerRightHandHookshotDLs);
ROWS2(gPlayerLeftHandBottleDLs);
ROWS1(sPlayerFirstPersonLeftForearmDLs);
ROWS1(sPlayerFirstPersonLeftHandDLs);
ROWS1(sPlayerFirstPersonRightShoulderDLs);
ROWS1(sPlayerFirstPersonRightHandDLs);
ROWS1(sPlayerFirstPersonRightHandHookshotDLs);
ROWS1(sPlayerAgeProperties);
ROWS1(gPlayerSkeletons);
ROWS1(sPlayerEyesTextures);
ROWS1(sPlayerMouthTextures);
ROWS1(D_801C08C0);
ROWS1(D_801C08FC);
ROWS1(D_801C0910);
ROWS1(D_801C0924);
ROWS1(D_801C0938);
_Static_assert((PLAYER_FORM_HUMAN == 4) && (PLAYER_FORM_MAX == 5), "the human is MM's fifth and last form row");
_Static_assert(PLAYER_EYES_MAX == 8, "Ocarina of Time's sEyeTextures has 8 eyes per age");
_Static_assert(PLAYER_MOUTH_MAX == 4, "Ocarina of Time's sMouthTextures has 4 mouths per age");

// The hookshot / Megaton Hammer models' hands start at these commands (the cut MergeShipOoTItemsDraw.inc makes for the
// Longshot and the hammer, SoH AgeDependentEquipment.cpp): the adult holds MM's hookshot as the closed hand plus the
// hookshot, so that it looks the same whether or not that cut was made.
#define MS_ADULT_HOOKSHOT_HAND_START 84

static MsAdultSlot sSlots[] = {
    { &gPlayerWaistDLs[HUMAN_NEAR], gLinkAdultWaistNearDL },
    { &gPlayerWaistDLs[HUMAN_FAR], gLinkAdultWaistFarDL },
    // OoT sSheathDLs: the empty scabbard (MergeShip_AdultLink_OverrideLimb picks the sword and shield on it).
    { &gPlayerSheath12DLs[HUMAN_NEAR], gLinkAdultSheathNearDL },
    { &gPlayerSheath12DLs[HUMAN_FAR], gLinkAdultSheathFarDL },
    { &gPlayerSheath13DLs[HUMAN_NEAR], gLinkAdultSheathNearDL },
    { &gPlayerSheath13DLs[HUMAN_FAR], gLinkAdultSheathFarDL },
    { &gPlayerSheath14DLs[HUMAN_NEAR], gLinkAdultSheathNearDL },
    { &gPlayerSheath14DLs[HUMAN_FAR], gLinkAdultSheathFarDL },
    // OoT gPlayerLeftHandBgsDLs (MM's human holds the Great Fairy's Sword here; MergeShip_AdultLink_OverrideLimb).
    { &gPlayerLeftHandTwoHandSwordDLs[HUMAN_NEAR], gLinkAdultLeftHandHoldingBgsNearDL },
    { &gPlayerLeftHandTwoHandSwordDLs[HUMAN_FAR], gLinkAdultLeftHandHoldingBgsFarDL },
    { &gPlayerLeftHandOpenDLs[HUMAN_NEAR], gLinkAdultLeftHandNearDL },
    { &gPlayerLeftHandOpenDLs[HUMAN_FAR], gLinkAdultLeftHandFarDL },
    { &gPlayerLeftHandClosedDLs[HUMAN_NEAR], gLinkAdultLeftHandClosedNearDL },
    { &gPlayerLeftHandClosedDLs[HUMAN_FAR], gLinkAdultLeftHandClosedFarDL },
    { &gPlayerLeftHandOneHandSwordDLs[HUMAN_NEAR], gLinkAdultLeftHandHoldingMasterSwordNearDL },
    { &gPlayerLeftHandOneHandSwordDLs[HUMAN_FAR], gLinkAdultLeftHandHoldingMasterSwordFarDL },
    { &gPlayerRightHandOpenDLs[HUMAN_NEAR], gLinkAdultRightHandNearDL },
    { &gPlayerRightHandOpenDLs[HUMAN_FAR], gLinkAdultRightHandFarDL },
    { &gPlayerRightHandClosedDLs[HUMAN_NEAR], gLinkAdultRightHandClosedNearDL },
    { &gPlayerRightHandClosedDLs[HUMAN_FAR], gLinkAdultRightHandClosedFarDL },
    { &gPlayerRightHandBowDLs[HUMAN_NEAR], gLinkAdultRightHandHoldingBowNearDL },
    { &gPlayerRightHandBowDLs[HUMAN_FAR], gLinkAdultRightHandHoldingBowFarDL },
    // OoT sPlayerRightHandOcarinaDLs / sPlayerRightHandOotDLs: the adult holds the Ocarina of Time.
    { &gPlayerRightHandInstrumentDLs[HUMAN_NEAR], gLinkAdultRightHandHoldingOotNearDL },
    { &gPlayerRightHandInstrumentDLs[HUMAN_FAR], gLinkAdultRightHandHoldingOotFarDL },
    // OoT sPlayerRightHandHookshotDLs (near for both LODs; MergeShip_AdultLink_OverrideLimb adds the closed hand).
    { &gPlayerRightHandHookshotDLs[HUMAN_NEAR], gLinkAdultRightHandHoldingHookshotNearDL },
    { &gPlayerRightHandHookshotDLs[HUMAN_FAR], gLinkAdultRightHandHoldingHookshotNearDL },
    // OoT sPlayerLeftHandBottleDLs (MM draws the bottle itself in func_80128640).
    { &gPlayerLeftHandBottleDLs[HUMAN_NEAR], gLinkAdultLeftHandOutNearDL },
    { &gPlayerLeftHandBottleDLs[HUMAN_FAR], gLinkAdultLeftHandOutNearDL },
    // OoT sFirstPerson*DLs (OoT names them from the arm's other side).
    { &sPlayerFirstPersonLeftForearmDLs[PLAYER_FORM_HUMAN], gLinkAdultRightArmOutNearDL },
    { &sPlayerFirstPersonLeftHandDLs[PLAYER_FORM_HUMAN], gLinkAdultRightHandOutNearDL },
    { &sPlayerFirstPersonRightShoulderDLs[PLAYER_FORM_HUMAN], gLinkAdultRightShoulderNearDL },
    { &sPlayerFirstPersonRightHandDLs[PLAYER_FORM_HUMAN], gLinkAdultRightHandHoldingBowFirstPersonDL },
    { &sPlayerFirstPersonRightHandHookshotDLs[PLAYER_FORM_HUMAN], gLinkAdultRightHandHoldingHookshotFarDL },
};

// OoT sEyeTextures[LINK_AGE_ADULT] / sMouthTextures[LINK_AGE_ADULT], in the same order as MM's human's (both are
// Ocarina of Time's eye order; the animations index them).
static const char* const sAdultEyes[PLAYER_EYES_MAX] = {
    gLinkAdultEyesOpenTex,      gLinkAdultEyesHalfTex,  gLinkAdultEyesClosedfTex, gLinkAdultEyesRollLeftTex,
    gLinkAdultEyesRollRightTex, gLinkAdultEyesShockTex, gLinkAdultEyesUnk1Tex,    gLinkAdultEyesUnk2Tex,
};
static const char* const sAdultMouths[PLAYER_MOUTH_MAX] = {
    gLinkAdultMouth1Tex,
    gLinkAdultMouth2Tex,
    gLinkAdultMouth3Tex,
    gLinkAdultMouth4Tex,
};

// OoT D_80126038 / D_80126050 / D_80126058 / D_80126060 / D_80126068 [LINK_AGE_ADULT]: the leg IK (func_8008F87C).
static const Vec3f sAdultThighPos = { 1304.0f, 0.0f, 0.0f };
static const f32 sAdultShinLength = 1265.0f;
static const f32 sAdultLegLengthSq = SQ(13.04f);
static const f32 sAdultLegOffset = 10.019104f;
static const f32 sAdultFootHeight = 5.0f;

// OoT sBootData[PLAYER_BOOTS_KOKIRI] (adult), in MM's layout (MM adds REG(39), which is OoT's constant 1.2).
static s16 sAdultBootRegs[18] = {
    200,  // REG(19)
    1000, // REG(30)
    300,  // REG(32)
    700,  // REG(34)
    550,  // REG(35)
    270,  // REG(36)
    600,  // REG(37)
    350,  // REG(38)
    120,  // REG(39): OoT func_8084029C's 1.2f
    800,  // R_DECELERATE_RATE (OoT REG(43))
    600,  // R_RUN_SPEED_LIMIT (OoT REG(45))
    -100, // REG(68)
    600,  // REG(69)
    590,  // IREG(66)
    750,  // IREG(67)
    125,  // IREG(68)
    200,  // IREG(69)
    130,  // MREG(95)
};

static struct {
    u8 backedUp;
    u8 adult; // the rows hold the adult's
    PlayerAgeProperties youngAge;
    PlayerAgeProperties adultAge;
    FlexSkeletonHeader* youngSkel;
    TexturePtr youngEyes[PLAYER_EYES_MAX];
    TexturePtr youngMouths[PLAYER_MOUTH_MAX];
    Vec3f youngThighPos;
    f32 youngLeg[4];
} sAdult;

// OoT sAgeProperties[LINK_AGE_ADULT] in MM's PlayerAgeProperties. The animation fields are the Fierce Deity's row,
// which are Ocarina of Time's adult ones (and are z_player.c's own pointers, which it compares).
static void MergeShip_AdultLink_BuildAgeProperties(void) {
    static const Vec3s sUnk44 = { 9, 4671, 359 };
    static const Vec3s sUnk4A[4] = { { 8, 4694, 380 }, { 9, 6122, 359 }, { 8, 4694, 380 }, { 9, 6122, 359 } };
    static const Vec3s sUnk62[4] = { { 9, 6122, 359 }, { 9, 7693, 380 }, { 9, 6122, 359 }, { 9, 7693, 380 } };
    static const Vec3s sUnk7A[4] = { { 8, 4694, 380 }, { 9, 6122, 359 }, { -1592, 4694, 380 }, { -1591, 6122, 359 } };
    PlayerAgeProperties* p = &sAdult.adultAge;

    *p = sPlayerAgeProperties[PLAYER_FORM_FIERCE_DEITY];
    p->ceilingCheckHeight = 56.0f;
    p->shadowScale = 90.0f;
    p->unk_08 = 1.0f;
    p->unk_0C = 111.0f;
    p->unk_10 = 70.0f;
    p->unk_14 = 79.4f;
    p->unk_18 = 59.0f;
    p->unk_1C = 41.0f;
    p->unk_20 = 19.0f;
    p->unk_24 = 36.0f;
    p->unk_28 = 44.8f;
    p->unk_2C = 56.0f;
    p->unk_30 = 68.0f;
    p->unk_34 = 70.0f;
    p->wallCheckRadius = 18.0f;
    p->unk_3C = 15.0f;
    p->unk_40 = 70.0f;
    p->unk_44 = sUnk44;
    memcpy(p->unk_4A, sUnk4A, sizeof(sUnk4A));
    memcpy(p->unk_62, sUnk62, sizeof(sUnk62));
    memcpy(p->unk_7A, sUnk7A, sizeof(sUnk7A));
    // MM's voice bank 0 and footstep bank 0x80 are Ocarina of Time's adult Link (OoT: voiceSfxIdOffset 0,
    // surfaceSfxIdOffset 0x80); MM keeps them for the Fierce Deity.
    p->voiceSfxIdOffset = 0;
    p->surfaceSfxIdOffset = 0x80;
    // MM-only (interaction reach): the Fierce Deity's at the adult's size, which is the human's value.
    p->unk_98 = 22.0f;
    p->unk_9C = 29.4343f;
}

static void MergeShip_AdultLink_BackUp(void) {
    size_t i;

    if (sAdult.backedUp) {
        return;
    }
    sAdult.backedUp = true;
    for (i = 0; i < ARRAY_COUNT(sSlots); i++) {
        sSlots[i].young = *sSlots[i].slot;
    }
    sAdult.youngAge = sPlayerAgeProperties[PLAYER_FORM_HUMAN];
    sAdult.youngSkel = gPlayerSkeletons[PLAYER_FORM_HUMAN];
    memcpy(sAdult.youngEyes, sPlayerEyesTextures[PLAYER_FORM_HUMAN], sizeof(sAdult.youngEyes));
    memcpy(sAdult.youngMouths, sPlayerMouthTextures[PLAYER_FORM_HUMAN], sizeof(sAdult.youngMouths));
    sAdult.youngThighPos = D_801C08C0[PLAYER_FORM_HUMAN];
    sAdult.youngLeg[0] = D_801C08FC[PLAYER_FORM_HUMAN];
    sAdult.youngLeg[1] = D_801C0910[PLAYER_FORM_HUMAN];
    sAdult.youngLeg[2] = D_801C0924[PLAYER_FORM_HUMAN];
    sAdult.youngLeg[3] = D_801C0938[PLAYER_FORM_HUMAN];
    MergeShip_AdultLink_BuildAgeProperties();
}

// The rows only (no patch, no log): MergeShip_AdultLink_Apply, and MergeShip_AdultLink_UseTables for an Anchor ghost.
static void MergeShip_AdultLink_SwapTables(s32 adult) {
    size_t i;
    s32 j;

    MergeShip_AdultLink_BackUp();
    for (i = 0; i < ARRAY_COUNT(sSlots); i++) {
        *sSlots[i].slot = adult ? (Gfx*)sSlots[i].adult : sSlots[i].young;
    }
    sPlayerAgeProperties[PLAYER_FORM_HUMAN] = adult ? sAdult.adultAge : sAdult.youngAge;
    gPlayerSkeletons[PLAYER_FORM_HUMAN] = adult ? (FlexSkeletonHeader*)gLinkAdultSkelPath : sAdult.youngSkel;
    for (j = 0; j < PLAYER_EYES_MAX; j++) {
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][j] = adult ? (TexturePtr)sAdultEyes[j] : sAdult.youngEyes[j];
    }
    for (j = 0; j < PLAYER_MOUTH_MAX; j++) {
        sPlayerMouthTextures[PLAYER_FORM_HUMAN][j] = adult ? (TexturePtr)sAdultMouths[j] : sAdult.youngMouths[j];
    }
    D_801C08C0[PLAYER_FORM_HUMAN] = adult ? sAdultThighPos : sAdult.youngThighPos;
    D_801C08FC[PLAYER_FORM_HUMAN] = adult ? sAdultShinLength : sAdult.youngLeg[0];
    D_801C0910[PLAYER_FORM_HUMAN] = adult ? sAdultLegLengthSq : sAdult.youngLeg[1];
    D_801C0924[PLAYER_FORM_HUMAN] = adult ? sAdultLegOffset : sAdult.youngLeg[2];
    D_801C0938[PLAYER_FORM_HUMAN] = adult ? sAdultFootHeight : sAdult.youngLeg[3];
    sAdult.adult = adult;
}

static void MergeShip_AdultLink_PatchHookshotHand(void) {
    static u8 sPatched = false;

    if (sPatched) {
        return;
    }
    sPatched = true;
    // See MS_ADULT_HOOKSHOT_HAND_START.
    Gfx end[] = { gsSPEndDisplayList() };
    ResourceMgr_PatchGfxByName("oot/objects/object_link_boy/gLinkAdultRightHandHoldingHookshotNearDL",
                               "mergeShipHookshotOnly", MS_ADULT_HOOKSHOT_HAND_START, end[0]);
}

static void MergeShip_AdultLink_Apply(s32 adult) {
    MergeShip_AdultLink_SwapTables(adult);
    if (adult) {
        MergeShip_AdultLink_PatchHookshotHand();
    }
    MergeShipTest_Log("ADULT tables %s", adult ? "adult" : "young");
}

// An Anchor ghost is another player's human, adult or young whatever this player is: its build and draw take that
// age's rows (and put this player's back after). Returns the rows' age before, for the caller to restore.
s32 MergeShip_AdultLink_UseTables(s32 adult) {
    const s32 prev = sAdult.adult;

    adult = adult && MergeShip_AdultLink_Enabled();
    if (adult != prev) {
        MergeShip_AdultLink_SwapTables(adult);
        if (adult) {
            MergeShip_AdultLink_PatchHookshotHand();
        }
    }
    return prev;
}

// ---- state ------------------------------------------------------------------------------------------------------------------

s32 MergeShip_AdultLink_Enabled(void) {
    return CVarGetInteger(MERGESHIP_ADULT_LINK_CVAR, 1) && MergeShip_IsOcarinaMounted();
}

// The byte's bit 0 is the age; the Adult Mask keeps its own bits above it (MergeShipAdultMask.h).
u8 MergeShip_AdultLink_GetSaveAge(void) {
    return gSaveContext.save.shipSaveInfo.mergeShipHumanAge & 1;
}

void MergeShip_AdultLink_SetSaveAge(u8 age) {
    gSaveContext.save.shipSaveInfo.mergeShipHumanAge =
        (gSaveContext.save.shipSaveInfo.mergeShipHumanAge & ~1) |
        ((age == MERGESHIP_HUMAN_AGE_ADULT) ? MERGESHIP_HUMAN_AGE_ADULT : MERGESHIP_HUMAN_AGE_YOUNG);
}

s32 MergeShip_AdultLink_WantsAdult(void) {
    s32 debug = CVarGetInteger(MERGESHIP_ADULT_LINK_DEBUG_CVAR, 0);

    if (debug == 1) {
        MergeShip_AdultLink_SetSaveAge(MERGESHIP_HUMAN_AGE_ADULT);
    } else if (debug == 2) {
        MergeShip_AdultLink_SetSaveAge(MERGESHIP_HUMAN_AGE_YOUNG);
    }
    return MergeShip_AdultLink_Enabled() && (MergeShip_AdultLink_GetSaveAge() == MERGESHIP_HUMAN_AGE_ADULT);
}

s32 MergeShip_AdultLink_TablesAdult(void) {
    return sAdult.adult;
}

s32 MergeShip_AdultLink_IsActive(Player* player) {
    // The Anchor ghost (ACTOR_UNSET_13, Anchor.h ANCHOR_DUMMY_ACTOR_ID) is built from the same rows (skeleton, eyes,
    // display lists), so it is drawn as the adult too; else its face and sword take the young Link's setup.
    return sAdult.adult && (player != NULL) &&
           ((player->actor.id == ACTOR_PLAYER) || (player->actor.id == ACTOR_UNSET_13)) &&
           (player->transformation == PLAYER_FORM_HUMAN);
}

void MergeShip_AdultLink_Sync(void) {
    s32 adult;

    MergeShip_AdultMask_Sync(); // the Adult Mask's age change (MergeShipAdultMask.h), before the age is read
    adult = MergeShip_AdultLink_WantsAdult();

    if (adult != sAdult.adult) {
        MergeShip_AdultLink_Apply(adult);
    } else if (adult && (sPlayerEyesTextures[PLAYER_FORM_HUMAN][0] != (TexturePtr)sAdultEyes[0])) {
        // 2S2H's Play as Kafei (Modes/PlayAsKafei.cpp) writes the human's eyes and mouths back at every scene's end,
        // even while off: the adult's head then drew young Link's face with its own palette, black. The rows again.
        MergeShip_AdultLink_SwapTables(adult);
    }
}

s32 MergeShip_AdultLink_UpdateRebuild(PlayState* play) {
    Player* player;

    if ((play == NULL) || (play->actorCtx.actorLists[ACTORCAT_PLAYER].first == NULL)) {
        return false;
    }
    player = GET_PLAYER(play);
    // Not while a rebuild (or MM's own re-init after a transformation) is under way.
    if ((player->transformation != PLAYER_FORM_HUMAN) || (player->actor.update == func_8012301C) ||
        (player->actor.init != NULL)) {
        return false;
    }
    if (MergeShip_AdultLink_WantsAdult() == sAdult.adult) {
        return false;
    }
    // Only while Link is standing free (MM rebuilds the player the same way after a transformation cutscene).
    if ((player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_20000000 | PLAYER_STATE1_800000 |
                                PLAYER_STATE1_8000000 | PLAYER_STATE1_2000 | PLAYER_STATE1_4000)) ||
        (player->csAction != PLAYER_CSACTION_NONE) || !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF) ||
        (play->pauseCtx.state != 0)) {
        // (A scene change -- the host's arrival -- builds the new player at Player_Init with the right rows anyway.)
        return false;
    }
    MergeShipTest_Log("ADULT rebuild the player as %s", sAdult.adult ? "young" : "adult");
    player->actor.update = func_8012301C;
    player->actor.draw = NULL;
    player->av1.actionVar1 = 0;
    return true;
}

// ---- player -----------------------------------------------------------------------------------------------------------------

f32 MergeShip_AdultLink_Height(Player* player, f32 height) {
    // OoT Player_GetHeight: (LINK_IS_ADULT ? 68.0f : 44.0f) + the horse's 32; MM's human is the 44.
    return MergeShip_AdultLink_IsActive(player) ? (height + (68.0f - 44.0f)) : height;
}

// The adult's hands this frame, from their own limbs' matrices (MergeShip_AdultLink_PostLimbDraw). MM's own
// sPlayerGetItemRefPos reads bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND], a slot MM fills only when the limb drawn has a
// list (Player_PostLimbDrawGameplay) but counts by the skeleton's own (Player_OverrideLimbDrawGameplayCommon): with the
// adult's hands swapped for Ocarina of Time's the two can disagree, and the slot keeps an old position -- the held-up
// item then sits low, inside him.
static Vec3f sAdultHandPos[2]; // left, right

void MergeShip_AdultLink_GetItemRefPos(Player* player, Vec3f* refPos) {
    if (!MergeShip_AdultLink_IsActive(player)) {
        return;
    }
    // OoT Player_PostLimbDrawGameplay's sGetItemRefPos: the left hand alone when an exchange item is shown, else the
    // point between both hands.
    if (!(player->stateFlags1 & PLAYER_STATE1_400) && (player->getItemDrawIdPlusOne != (GID_NONE + 1)) &&
        (player->exchangeItemAction != PLAYER_IA_NONE)) {
        *refPos = sAdultHandPos[0];
    } else {
        refPos->x = (sAdultHandPos[0].x + sAdultHandPos[1].x) * 0.5f;
        refPos->y = (sAdultHandPos[0].y + sAdultHandPos[1].y) * 0.5f;
        refPos->z = (sAdultHandPos[0].z + sAdultHandPos[1].z) * 0.5f;
    }
}

s16* MergeShip_AdultLink_BootRegs(Player* player, s16* bootRegs) {
    // OoT Player_SetBootData: the adult's Kokiri Boots row (the child's is PLAYER_BOOTS_KOKIRI_CHILD, MM's human's).
    if (MergeShip_AdultLink_IsActive(player) && (player->currentBoots == PLAYER_BOOTS_HYLIAN)) {
        return sAdultBootRegs;
    }
    return bootRegs;
}

s32 MergeShip_AdultLink_KnobDoor(Player* player, s32 doorDirection, u8* doorAnimIndex) {
    if (!MergeShip_AdultLink_IsActive(player)) {
        return false;
    }
    // OoT KNOB_ANIM_ADULT_L / _R: the door's adult animation, which MM keeps as the Fierce Deity's.
    *doorAnimIndex = PLAYER_FORM_FIERCE_DEITY + ((doorDirection < 0) ? 0 : PLAYER_FORM_MAX);
    return true;
}

// ---- drawing ----------------------------------------------------------------------------------------------------------------

// OoT sTunicColors.
static const Color_RGB8 sAdultTunicColors[3] = { { 30, 105, 27 }, { 100, 20, 0 }, { 0, 60, 100 } };
// OoT sGauntletColors.
static const Color_RGB8 sAdultGauntletColors[2] = { { 255, 255, 255 }, { 254, 207, 15 } };

// The tunic's colour of the adult being drawn (MergeShip_AdultLink_DrawSetup), set again before each limb.
static Color_RGB8 sAdultTunicEnv = { 30, 105, 27 };

void MergeShip_AdultLink_DrawSetup(PlayState* play, Actor* actor, s32 playerForm) {
    Player* player = (Player*)actor;
    Color_RGB8 color;
    s32 tunic = 0;

    if ((playerForm != PLAYER_FORM_HUMAN) || !MergeShip_AdultLink_IsActive(player)) {
        return;
    }
    if (MergeShip_OoTEquip_Enabled() && (OOT_EQUIP.tunic <= 2)) {
        tunic = OOT_EQUIP.tunic;
    }
    color = sAdultTunicColors[tunic];
    // OoT: the Kokiri Tunic takes the cosmetic editor's colour (SoH Link.KokiriTunic; 2S2H Player.HumanTunic).
    if ((tunic == 0) && CVarGetInteger("gCosmetic.Player.HumanTunic.Changed", 0)) {
        Color_RGBA8 def = { color.r, color.g, color.b, 255 };
        Color_RGBA8 c = CVarGetColor("gCosmetic.Player.HumanTunic.Color", def);
        color.r = c.r;
        color.g = c.g;
        color.b = c.b;
    }
    // Anchor: every tunic takes the player's colour (SoH VB_APPLY_TUNIC_COLOR; MergeShipAnchorColors.cpp).
    MergeShip_AnchorColors_TunicColor(actor, &color);
    sAdultTunicEnv = color;

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, color.r, color.g, color.b, 0);
    CLOSE_DISPS(play->state.gfxCtx);

    // Which eyes and mouth the head draws (the black-face report, 2026-09-24): logged once a second by a test.
    if ((play->gameplayFrames % 60) == 0) {
        const char* eyes = (const char*)sPlayerEyesTextures[PLAYER_FORM_HUMAN][0];
#ifdef MERGESHIP_HOSTED
        MergeShipHostTest_Log("ADULT draw tables=%d eyes0=%s mouth0=%s skel=%s", sAdult.adult,
                          ((eyes != NULL) && (eyes[0] == '_')) ? eyes : "(not a path)",
                          ((const char*)sPlayerMouthTextures[PLAYER_FORM_HUMAN][0] != NULL) &&
                                  (((const char*)sPlayerMouthTextures[PLAYER_FORM_HUMAN][0])[0] == '_')
                              ? (const char*)sPlayerMouthTextures[PLAYER_FORM_HUMAN][0]
                              : "(not a path)",
                          ((const char*)gPlayerSkeletons[PLAYER_FORM_HUMAN] != NULL) &&
                                  (((const char*)gPlayerSkeletons[PLAYER_FORM_HUMAN])[0] == '_')
                              ? (const char*)gPlayerSkeletons[PLAYER_FORM_HUMAN]
                              : "(not a path)");
#endif
    }
}

// Fits for MM's own equipment, made for young Link, on the adult: the inverse of SoH's ScaleAdultEquipmentAsChild (0.8 in
// the hands; the scabbard translated 218,-100,62 and scaled 0.8). Tunable: gMergeShip.AdultFit.<name>.<X|Y|Z|S>.
// Hand: a young list with its fist (Ocarina of Time's Kokiri Sword). Left / Right: MM's item alone in the adult's own
// fist, Hyrule's measured fits for the same lists on the same adult hands (Shipwright MergeShipMMEquipmentDraw.inc
// sMSEqAdultFits), so MM's gear sits the same in both games. Hookshot: MM's Hookshot on Ocarina of Time's hookshot, as
// measured from both models' vertices (mm.o2r gLinkHumanHookshotDL, oot.o2r gLinkAdultRightHandHoldingHookshotNearDL):
// both barrels run along the hand's +Y, so no rotation -- MM's barrel axis moved onto OoT's (x 97, z 34), scaled to
// OoT's grip-to-muzzle length (1.85) with the hook at OoT's hook height (y 1640). Both hookshots then fill the same
// place in first person (the user, 2026-09-30: "why are the first person views for the hookshots different").
typedef enum {
    MS_ADULT_FIT_HAND,
    MS_ADULT_FIT_SHEATH,
    MS_ADULT_FIT_BGS_BACK, // BUG-0021: the Biggoron's Sword hand list onto the scabbard (MERGESHIP_BGS_ON_BACK_*)
    MS_ADULT_FIT_LEFT,
    MS_ADULT_FIT_RIGHT,
    MS_ADULT_FIT_HOOKSHOT,
    MS_ADULT_FIT_RIGHT_MIRROR, // MS_ADULT_FIT_RIGHT with each shield's centre on its OoT pair's (BUG-0002)
    MS_ADULT_FIT_RIGHT_HERO,
    MS_ADULT_FIT_SHEATH_MIRROR, // MS_ADULT_FIT_SHEATH for MM's shields on the back, lowered onto OoT's (BUG-0034)
    MS_ADULT_FIT_SHEATH_HERO,
    MS_ADULT_FIT_MAX
} MsAdultFit;
static const struct {
    const char* name;
    f32 v[4];
} sAdultFits[MS_ADULT_FIT_MAX] = {
    { "Hand", { 0.0f, 0.0f, 0.0f, 1.25f } },
    { "Sheath", { -272.5f, 125.0f, -77.5f, 1.25f } },
    { "BgsBack", { MERGESHIP_BGS_ON_BACK_X, MERGESHIP_BGS_ON_BACK_Y, MERGESHIP_BGS_ON_BACK_Z, 1.0f } },
    { "Left", { 0.0f, -15.0f, -60.0f, 1.45f } },
    { "Right", { 10.0f, 40.0f, 40.0f, 1.3f } },
    { "Hookshot", { 100.7f, 65.6f, -10.5f, 1.85f } },
    // Right, moved so the shield's centre is the Ocarina of Time shield's in the adult's hand (Hyrule's
    // MSEq_ComposeHand offsets, here folded in: 10 + 1.3 * -141.5, 40 + 1.3 * 12.3; 10 + 1.3 * 7.7, 40 + 1.3 * 20.8).
    { "RightMirror", { -174.0f, 56.0f, 40.0f, 1.3f } },
    { "RightHero", { 20.0f, 67.0f, 40.0f, 1.3f } },
    // Sheath, lowered (+X runs down the adult's back) so each MM shield sits where Ocarina of Time's Mirror Shield does
    // (BUG-0034, measured from behind: build/tests/mm_bug0034_back_shield).
    { "SheathMirror", { 327.5f, 125.0f, -77.5f, 1.25f } },
    { "SheathHero", { -72.5f, 125.0f, -77.5f, 1.25f } },
};

// BUG-0102 (MergeShip_AdultLink_HeroBowFPValues): MM's belly (-23, 588) onto Ocarina of Time's (10, 394) and MM's
// upper tip (1279, -441) onto Ocarina of Time's (2010, -309): turned 18.9 degrees, scaled 1.278 (compared on screen
// with Ocarina of Time's bow: build/tests/bug0102_fit_rot).
#define MS_HERO_BOW_FP_X 281.2f
#define MS_HERO_BOW_FP_Y -307.5f
#define MS_HERO_BOW_FP_Z -10.0f
#define MS_HERO_BOW_FP_S 1.278f
#define MS_HERO_BOW_FP_R 18.9f

static void MergeShip_AdultLink_FitValues(s32 fit, f32 v[4]) {
    static const char* sAxes[4] = { "X", "Y", "Z", "S" };
    char cvar[64];
    s32 i;

    for (i = 0; i < 4; i++) {
        snprintf(cvar, sizeof(cvar), "gMergeShip.AdultFit.%s.%s", sAdultFits[fit].name, sAxes[i]);
        v[i] = CVarGetFloat(cvar, sAdultFits[fit].v[i]);
    }
}

static Mtx* MergeShip_AdultLink_FitMtx(PlayState* play, s32 fit) {
    f32 v[4];
    Mtx* mtx;

    MergeShip_AdultLink_FitValues(fit, v);
    // Built directly (guMtxF2L), not through the matrix stack: this runs inside limb callbacks, where every Matrix_*
    // call is recorded for frame interpolation, which matches recorded ops by their place along the skeleton -- these
    // extra ones mid-skeleton threw every later limb into the wrong space at the user's refresh rate (his hands
    // "exploding", 2026-09-30; the Bunny Hood's bug, 2026-09-29). The fit is a constant: nothing to interpolate.
    // Translate then scale (as Matrix_Translate(NEW) + Matrix_Scale(APPLY)): the scale on the diagonal, the
    // translation in the last row.
    {
        float mf[4][4] = {
            { v[3], 0.0f, 0.0f, 0.0f },
            { 0.0f, v[3], 0.0f, 0.0f },
            { 0.0f, 0.0f, v[3], 0.0f },
            { v[0], v[1], v[2], 1.0f },
        };

        mtx = GRAPH_ALLOC(play->state.gfxCtx, sizeof(Mtx));
        guMtxF2L(mf, mtx);
    }
    return mtx;
}

// BUG-0102: MM's Hero's Bow aimed in first person, laid on Ocarina of Time's bow. MM's bow is shorter and bent deeper
// (mm.o2r gLinkHumanBowDL: belly 588, tips -441 at x -1322 / 1279; oot.o2r gLinkAdultBowBodyTex: belly 394, tips -258
// / -309 at x -2009 / 2010), so under the plain right-hand fit its upper limb ran 20 degrees off Ocarina of Time's and
// the bow sat to the left of where Hyrule's bow sits. Here the belly goes on Ocarina of Time's belly and the upper
// limb's tip on Ocarina of Time's tip: turned about Z, scaled, moved. Tunable:
// gMergeShip.AdultFit.HeroBowFP.<X|Y|Z|S|R> (R in degrees).
static void MergeShip_AdultLink_HeroBowFPValues(f32 v[5]) {
    static const char* sAxes[5] = { "X", "Y", "Z", "S", "R" };
    static const f32 sDefaults[5] = { MS_HERO_BOW_FP_X, MS_HERO_BOW_FP_Y, MS_HERO_BOW_FP_Z, MS_HERO_BOW_FP_S,
                                      MS_HERO_BOW_FP_R };
    char cvar[64];
    s32 i;

    for (i = 0; i < 5; i++) {
        snprintf(cvar, sizeof(cvar), "gMergeShip.AdultFit.HeroBowFP.%s", sAxes[i]);
        v[i] = CVarGetFloat(cvar, sDefaults[i]);
    }
}

static Mtx* MergeShip_AdultLink_HeroBowFPMtx(PlayState* play) {
    f32 v[5];
    f32 c;
    f32 s;
    Mtx* mtx;

    MergeShip_AdultLink_HeroBowFPValues(v);
    c = cosf(DEG_TO_RAD(v[4])) * v[3];
    s = sinf(DEG_TO_RAD(v[4])) * v[3];
    // Built directly, as MergeShip_AdultLink_FitMtx: scale, turn about Z (Matrix_RotateZF's sense), translate.
    {
        float mf[4][4] = {
            { c, s, 0.0f, 0.0f },
            { -s, c, 0.0f, 0.0f },
            { 0.0f, 0.0f, v[3], 0.0f },
            { v[0], v[1], v[2], 1.0f },
        };

        mtx = GRAPH_ALLOC(play->state.gfxCtx, sizeof(Mtx));
        guMtxF2L(mf, mtx);
    }
    return mtx;
}

// `base` as it is, then `fitted` and `fitted2` under a fit (MS_ADULT_FIT_MAX: no fit).
static Gfx* MergeShip_AdultLink_Compose2(PlayState* play, const void* base, const void* fitted, const void* fitted2,
                                         s32 fit) {
    Gfx* start = GRAPH_ALLOC(play->state.gfxCtx, 8 * sizeof(Gfx));
    Gfx* p = start;

    if (base != NULL) {
        gSPDisplayList(p++, (Gfx*)base);
    }
    if (fitted != NULL) {
        if (fit < MS_ADULT_FIT_MAX) {
            gSPMatrix(p++, MergeShip_AdultLink_FitMtx(play, fit), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        }
        gSPDisplayList(p++, (Gfx*)fitted);
        if (fitted2 != NULL) {
            gSPDisplayList(p++, (Gfx*)fitted2);
        }
        if (fit < MS_ADULT_FIT_MAX) {
            gSPPopMatrix(p++, G_MTX_MODELVIEW);
        }
    }
    gSPEndDisplayList(p++);
    return start;
}

static Gfx* MergeShip_AdultLink_Compose(PlayState* play, const void* base, const void* fitted, s32 fit) {
    return MergeShip_AdultLink_Compose2(play, base, fitted, NULL, fit);
}

static s32 MergeShip_AdultLink_IsPath(const Gfx* dList, const char* path) {
    // Only resource paths (a list built this frame is not one).
    return (dList != NULL) && (strncmp((const char*)dList, "__OTR__", 7) == 0) &&
           (strcmp((const char*)dList, path) == 0);
}

#define LOD(nearDL, farDL) ((sPlayerLod == 0) ? (Gfx*)(nearDL) : (Gfx*)(farDL))

typedef enum {
    MS_ADULT_SWORD_NONE,
    MS_ADULT_SWORD_MASTER, // Ocarina of Time's Master Sword
    MS_ADULT_SWORD_LONG,   // Ocarina of Time's Biggoron's Sword / Giant's Knife (no scabbard of its own)
    MS_ADULT_SWORD_MM,     // Majora's Mask's Kokiri / Razor / Gilded Sword
} MsAdultSword;

typedef enum {
    MS_ADULT_SHIELD_NONE,
    MS_ADULT_SHIELD_OOT_DEKU, // Ocarina of Time's adult cannot hold it (OoT sPlayerRightHandShieldDLs: a fist)
    MS_ADULT_SHIELD_OOT_HYLIAN,
    MS_ADULT_SHIELD_OOT_MIRROR,
    MS_ADULT_SHIELD_MM, // Majora's Mask's Hero's / Mirror Shield
} MsAdultShield;

// The human's B sword (the one in the scabbard).
static s32 MergeShip_AdultLink_BSword(void) {
    u8 bItem = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);

    if (MergeShip_OoTEquip_Enabled()) {
        if ((OOT_EQUIP.sword == MS_OOTEQ_SWORD_MASTER) && (bItem == ITEM_OOT_SWORD_MASTER)) {
            return MS_ADULT_SWORD_MASTER;
        }
        if ((OOT_EQUIP.sword == MS_OOTEQ_SWORD_LONG) && ITEM_IS_OOT_EQUIP_SWORD(bItem)) {
            return MS_ADULT_SWORD_LONG;
        }
    }
    return (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_NONE) ? MS_ADULT_SWORD_MM : MS_ADULT_SWORD_NONE;
}

static s32 MergeShip_AdultLink_Shield(Player* player) {
    if (player->currentShield == PLAYER_SHIELD_NONE) {
        return MS_ADULT_SHIELD_NONE;
    }
    if (MergeShip_OoTEquip_Enabled()) {
        switch (OOT_EQUIP.shield) {
            case MS_OOTEQ_SHIELD_DEKU:
                return MS_ADULT_SHIELD_OOT_DEKU;
            case MS_OOTEQ_SHIELD_HYLIAN:
                return MS_ADULT_SHIELD_OOT_HYLIAN;
            case MS_OOTEQ_SHIELD_MIRROR:
                return MS_ADULT_SHIELD_OOT_MIRROR;
            default:
                break;
        }
    }
    return MS_ADULT_SHIELD_MM;
}

// MM's young human's hands, as MM or the other MergeShip modules chose them, and the adult's hand for each.
static const struct {
    const char* young;
    const char* adultNear;
    const char* adultFar;
} sAdultHands[] = {
    { dgLinkHumanLeftHandOpenDL, gLinkAdultLeftHandNearDL, gLinkAdultLeftHandFarDL },
    { dgLinkHumanLeftHandClosedDL, gLinkAdultLeftHandClosedNearDL, gLinkAdultLeftHandClosedFarDL },
    { dgLinkHumanRightHandOpenDL, gLinkAdultRightHandNearDL, gLinkAdultRightHandFarDL },
    { dgLinkHumanRightHandClosedDL, gLinkAdultRightHandClosedNearDL, gLinkAdultRightHandClosedFarDL },
    { dgLinkHumanRightHandHoldingOcarinaDL, gLinkAdultRightHandHoldingOotNearDL, gLinkAdultRightHandHoldingOotFarDL },
    { dgLinkHumanRightHandHoldingBowDL, gLinkAdultRightHandHoldingBowNearDL, gLinkAdultRightHandHoldingBowFarDL },
    { dgLinkHumanLeftHandHoldBottleDL, gLinkAdultLeftHandOutNearDL, gLinkAdultLeftHandOutNearDL },
};

// Majora's Mask's sword in hand as MM chose it (gLinkHumanLeftHandHolding*DL: young Link's gLinkHumanLeftHandClosedDL
// followed by the sword): only the sword, in the adult's own fist, as Hyrule draws MM's swords on the adult
// (MSEq_ComposeHand / MSEq_ItemLists). False when the list is not one of them.
static s32 MergeShip_AdultLink_MMSwordInFist(PlayState* play, Gfx** dList) {
    static const struct {
        const char* held;
        const char* a;
        const char* b;
    } sSwords[] = {
        { dgLinkHumanLeftHandHoldingKokiriSwordDL, dgKokiriSwordDL, NULL },
        { dgLinkHumanLeftHandHoldingRazorSwordDL, dgRazorSwordDL, NULL },
        { dgLinkHumanLeftHandHoldingGildedSwordDL, dgLinkHumanGildedSwordHandleDL, dgLinkHumanGildedSwordBladeDL },
        { dgLinkHumanLeftHandHoldingGreatFairysSwordDL, dgLinkHumanGreatFairysSwordDL, NULL },
    };
    size_t i;

    for (i = 0; i < ARRAY_COUNT(sSwords); i++) {
        if (MergeShip_AdultLink_IsPath(*dList, sSwords[i].held)) {
            *dList = MergeShip_AdultLink_Compose2(
                play, LOD(gLinkAdultLeftHandClosedNearDL, gLinkAdultLeftHandClosedFarDL), sSwords[i].a, sSwords[i].b,
                MS_ADULT_FIT_LEFT);
            return true;
        }
    }
    return false;
}

static void MergeShip_AdultLink_LeftHand(PlayState* play, Player* player, Gfx** dList) {
    u8 held = player->heldItemId;

    if (sPlayerLeftHandType == PLAYER_MODELTYPE_LH_ONE_HAND_SWORD) {
        // MM: the B sword in hand (D_801C018C, or MergeShipOoTEquip's Ocarina of Time sword).
        if (held == ITEM_OOT_SWORD_MASTER) {
            *dList = LOD(gLinkAdultLeftHandHoldingMasterSwordNearDL, gLinkAdultLeftHandHoldingMasterSwordFarDL);
        } else if (held == ITEM_OOT_SWORD_KOKIRI) {
            // Ocarina of Time's Kokiri Sword (the child's fist and sword, MergeShipOoTEquipDraw.c) fitted to the adult.
            *dList = MergeShip_AdultLink_Compose(play, NULL, *dList, MS_ADULT_FIT_HAND);
        } else if (ITEM_IS_OOT_EQUIP_SWORD(held)) {
            *dList = (MergeShip_OoTEquip_HoldsBrokenKnife(player) || (held == ITEM_OOT_SWORD_BROKEN))
                         ? LOD(gLinkAdultHandHoldingBrokenGiantsKnifeDL, gLinkAdultHandHoldingBrokenGiantsKnifeFarDL)
                         : LOD(gLinkAdultLeftHandHoldingBgsNearDL, gLinkAdultLeftHandHoldingBgsFarDL);
        } else if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_NONE) {
            // Majora's Mask's own sword in the adult's fist.
            MergeShip_AdultLink_MMSwordInFist(play, dList);
        }
    } else if (sPlayerLeftHandType == PLAYER_MODELTYPE_LH_TWO_HAND_SWORD) {
        if (ITEM_IS_OOT_EQUIP_SWORD(held)) {
            *dList = (MergeShip_OoTEquip_HoldsBrokenKnife(player) || (held == ITEM_OOT_SWORD_BROKEN))
                         ? LOD(gLinkAdultHandHoldingBrokenGiantsKnifeDL, gLinkAdultHandHoldingBrokenGiantsKnifeFarDL)
                         : LOD(gLinkAdultLeftHandHoldingBgsNearDL, gLinkAdultLeftHandHoldingBgsFarDL);
        } else if (held == ITEM_SWORD_GREAT_FAIRY) {
            // MM's Great Fairy's Sword in the adult's fist.
            *dList = (Gfx*)gLinkHumanLeftHandHoldingGreatFairysSwordDL;
            MergeShip_AdultLink_MMSwordInFist(play, dList);
        }
    }
}

static void MergeShip_AdultLink_RightHand(PlayState* play, Player* player, Gfx** dList) {
    // BUG-0031: Hyrule's Fairy Ocarina (young Link's hand list, MergeShip_OoTItems_OverrideHandDL): the ocarina alone,
    // in the adult's own hand.
    if (MergeShip_AdultLink_IsPath(*dList, MERGESHIP_OOT_FAIRY_OCARINA_HAND_DL)) {
        static const ALIGN_ASSET(2) char sFairyOcarinaHandDL[] = MERGESHIP_OOT_FAIRY_OCARINA_HAND_DL;
        *dList = MergeShip_AdultLink_Compose(
            play, LOD(gLinkAdultRightHandClosedNearDL, gLinkAdultRightHandClosedFarDL),
            MergeShip_OoTEquip_RewriteDL(sFairyOcarinaHandDL, MS_OOTEQ_DL_STRIP_HAND), MS_ADULT_FIT_HAND);
        return;
    }
    if ((sPlayerRightHandType == PLAYER_MODELTYPE_RH_BOW) && MergeShip_OoTEquip_AdultHoldsHeroBow(player)) {
        // BUG-0059: MM's Hero's Bow alone (gLinkHumanBowDL; MM's hand list is young Link's fist and it) in the adult's
        // own fist, with Hyrule's fit for it (MSEq_ComposeHand, MSEQ_PART_RIGHT). His rows hold OoT's Fairy Bow.
        *dList = MergeShip_AdultLink_Compose(play, LOD(gLinkAdultRightHandClosedNearDL, gLinkAdultRightHandClosedFarDL),
                                             gLinkHumanBowDL, MS_ADULT_FIT_RIGHT);
        return;
    }
    if (sPlayerRightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
        // OoT sPlayerRightHandShieldDLs (adult column).
        switch (MergeShip_AdultLink_Shield(player)) {
            case MS_ADULT_SHIELD_OOT_HYLIAN:
                *dList = LOD(gLinkAdultRightHandHoldingHylianShieldNearDL, gLinkAdultRightHandHoldingHylianShieldFarDL);
                return;
            case MS_ADULT_SHIELD_OOT_MIRROR:
                *dList = LOD(gLinkAdultRightHandHoldingMirrorShieldNearDL, gLinkAdultRightHandHoldingMirrorShieldFarDL);
                return;
            case MS_ADULT_SHIELD_OOT_DEKU:
            case MS_ADULT_SHIELD_NONE:
                *dList = LOD(gLinkAdultRightHandClosedNearDL, gLinkAdultRightHandClosedFarDL);
                return;
            case MS_ADULT_SHIELD_MM:
            default:
                // MM's Hero's / Mirror Shield alone (MM's gLinkHumanRightHandHolding*ShieldDL is young Link's fist
                // and the shield) in the adult's own fist, as Hyrule draws it (MSEq_ComposeHand).
                *dList = MergeShip_AdultLink_Compose(play,
                                                     LOD(gLinkAdultRightHandClosedNearDL, gLinkAdultRightHandClosedFarDL),
                                                     (player->currentShield == PLAYER_SHIELD_MIRROR_SHIELD)
                                                         ? gLinkHumanMirrorShieldDL
                                                         : gLinkHumanHerosShieldDL,
                                                     (player->currentShield == PLAYER_SHIELD_MIRROR_SHIELD)
                                                         ? MS_ADULT_FIT_RIGHT_MIRROR
                                                         : MS_ADULT_FIT_RIGHT_HERO);
                return;
        }
    }
    if (sPlayerRightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT) {
        if (player->heldItemAction == PLAYER_IA_HOOKSHOT) {
            // Majora's Mask's own Hookshot, always MM's model, in the adult's own closed fist (OoT holds its hookshot
            // in gLinkAdultRightHandClosed*DL's fist: the hand part of gLinkAdultRightHandHoldingHookshot*DL). MM's
            // hand DL, gLinkHumanRightHandHoldingHookshotDL, is young Link's gLinkHumanRightHandClosedDL followed by
            // gLinkHumanHookshotDL: only the hookshot is drawn, fitted to the adult's hand -- as Hyrule's
            // MSEq_ComposeHand draws it on the adult (MergeShipMMEquipmentDraw.inc), with Hyrule's fit.
            *dList = MergeShip_AdultLink_Compose(
                play, LOD(gLinkAdultRightHandClosedNearDL, gLinkAdultRightHandClosedFarDL), gLinkHumanHookshotDL,
                MS_ADULT_FIT_HOOKSHOT);
        } else if (MergeShip_AdultLink_IsPath(*dList, gLinkAdultRightHandHoldingHookshotNearDL)) {
            // Ocarina of Time's ported Longshot: the hookshot without its hand (MS_ADULT_HOOKSHOT_HAND_START) in the
            // adult's own closed hand.
            *dList = MergeShip_AdultLink_Compose(play, LOD(gLinkAdultRightHandClosedNearDL, gLinkAdultRightHandClosedFarDL),
                                                 gLinkAdultRightHandHoldingHookshotNearDL, MS_ADULT_FIT_MAX);
        }
    }
}

static void MergeShip_AdultLink_Sheath(PlayState* play, Player* player, Gfx** dList) {
    // MM's sheath types: 12 sword sheathed / shield in hand, 13 both in hand, 14 sword sheathed / shield on the back,
    // 15 sword in hand / shield on the back (OoT 16, 17, 18, 19).
    s32 swordSheathed = (player->sheathType == PLAYER_MODELTYPE_SHEATH_12) ||
                        (player->sheathType == PLAYER_MODELTYPE_SHEATH_14);
    s32 shieldOnBack = (player->sheathType == PLAYER_MODELTYPE_SHEATH_14) ||
                       (player->sheathType == PLAYER_MODELTYPE_SHEATH_15);
    s32 sword = MergeShip_AdultLink_BSword();
    s32 shield = shieldOnBack ? MergeShip_AdultLink_Shield(player) : MS_ADULT_SHIELD_NONE;
    const void* shieldOnly = NULL;

    if (sword == MS_ADULT_SWORD_MM) {
        // Majora's Mask's sword and scabbard (MM chose them) fitted to the adult's back, and an Ocarina of Time shield
        // on it without the Master Sword's scabbard.
        if (shield == MS_ADULT_SHIELD_OOT_HYLIAN) {
            shieldOnly = MergeShip_OoTEquip_RewriteDL(gLinkAdultHylianShieldAndSheathNearDL, MS_OOTEQ_DL_STRIP_SHEATH);
        } else if (shield == MS_ADULT_SHIELD_OOT_MIRROR) {
            shieldOnly = MergeShip_OoTEquip_RewriteDL(gLinkAdultMirrorShieldAndSheathNearDL, MS_OOTEQ_DL_STRIP_SHEATH);
        }
        *dList = MergeShip_AdultLink_Compose(play, shieldOnly, *dList, MS_ADULT_FIT_SHEATH);
        return;
    }

    // OoT sSwordAndSheathDLs / sSheathDLs / sSheathWithSwordDLs / sSheathWithoutSwordDLs (adult column): the Master
    // Sword in its scabbard, or the empty scabbard (Biggoron's Sword has none of its own), with the shield on it.
    // BUG-0021: Biggoron's Sword put away is worn there too (below).
    // BUG-0062: MM keeps the sword "sheathed" while a two-handed one is in the hand; then the back is empty.
    const s32 bgsOnBack = swordSheathed && (sword == MS_ADULT_SWORD_LONG) && !MergeShip_OoTEquip_SwordInHand(player) &&
                          CVarGetInteger(MERGESHIP_BGS_ON_BACK_CVAR, 1);
    swordSheathed = swordSheathed && (sword == MS_ADULT_SWORD_MASTER);
    switch (shield) {
        case MS_ADULT_SHIELD_OOT_HYLIAN:
            *dList = swordSheathed ? LOD(gLinkAdultHylianShieldSwordAndSheathNearDL,
                                         gLinkAdultHylianShieldSwordAndSheathFarDL)
                                   : LOD(gLinkAdultHylianShieldAndSheathNearDL, gLinkAdultHylianShieldAndSheathFarDL);
            break;
        case MS_ADULT_SHIELD_OOT_MIRROR:
            *dList = swordSheathed ? LOD(gLinkAdultMirrorShieldSwordAndSheathNearDL,
                                         gLinkAdultMirrorShieldSwordAndSheathFarDL)
                                   : LOD(gLinkAdultMirrorShieldAndSheathNearDL, gLinkAdultMirrorShieldAndSheathFarDL);
            break;
        default:
            // No shield, the Deku Shield (the adult cannot carry it) or MM's shield (MergeShip_AdultLink_BackShieldDL).
            *dList = swordSheathed ? LOD(gLinkAdultMasterSwordAndSheathNearDL, gLinkAdultMasterSwordAndSheathFarDL)
                                   : LOD(gLinkAdultSheathNearDL, gLinkAdultSheathFarDL);
            break;
    }
    if (bgsOnBack) {
        // The sword alone on his back, without the Master Sword's scabbard (the user, 2026-10-07: "just remove the
        // sheath, keep it on his back"); the shield on it keeps its own lists without the scabbard's.
        const void* shieldOnly = NULL;

        if (shield == MS_ADULT_SHIELD_OOT_HYLIAN) {
            shieldOnly = MergeShip_OoTEquip_RewriteDL(gLinkAdultHylianShieldAndSheathNearDL, MS_OOTEQ_DL_STRIP_SHEATH);
        } else if (shield == MS_ADULT_SHIELD_OOT_MIRROR) {
            shieldOnly = MergeShip_OoTEquip_RewriteDL(gLinkAdultMirrorShieldAndSheathNearDL, MS_OOTEQ_DL_STRIP_SHEATH);
        }
        *dList = MergeShip_AdultLink_Compose(
            play, shieldOnly, MergeShip_OoTEquip_RewriteDL(gLinkAdultLeftHandHoldingBgsNearDL, MS_OOTEQ_DL_STRIP_HAND),
            MS_ADULT_FIT_BGS_BACK);
    }
}

void MergeShip_AdultLink_LimbTunicColor(PlayState* play, Player* player) {
    if (!MergeShip_AdultLink_IsActive(player)) {
        return;
    }
    // Ocarina of Time's adult lists take the tunic's colour from the environment colour, set once per draw in OoT.
    // Here MM's gear drawn on the limbs before (the Gilded Sword, the Mirror Shield ...) may leave another in it, and
    // the torso, drawn last, came out black: every limb gets the tunic's colour again.
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, sAdultTunicEnv.r, sAdultTunicEnv.g, sAdultTunicEnv.b, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

void MergeShip_AdultLink_OverrideLimb(PlayState* play, Player* player, s32 limbIndex, Gfx** dList) {
    size_t i;

    if (!MergeShip_AdultLink_IsActive(player)) {
        return;
    }

    switch (limbIndex) {
        case PLAYER_LIMB_LEFT_HAND:
            MergeShip_AdultLink_LeftHand(play, player, dList);
            break;
        case PLAYER_LIMB_RIGHT_HAND:
            MergeShip_AdultLink_RightHand(play, player, dList);
            break;
        case PLAYER_LIMB_SHEATH:
            MergeShip_AdultLink_Sheath(play, player, dList);
            return;
        default:
            return;
    }
    // A young human's hand chosen by a table the swap does not cover (MergeShip's item modules): the adult's.
    for (i = 0; i < ARRAY_COUNT(sAdultHands); i++) {
        if (MergeShip_AdultLink_IsPath(*dList, sAdultHands[i].young)) {
            *dList = LOD(sAdultHands[i].adultNear, sAdultHands[i].adultFar);
            break;
        }
    }
}

void MergeShip_AdultLink_OverrideLimbFirstPerson(PlayState* play, Player* player, s32 limbIndex, Gfx** dList) {
    if (!MergeShip_AdultLink_IsActive(player) || (player->unk_AA5 != PLAYER_UNKAA5_3)) {
        return;
    }
    // OoT Player_OverrideLimbDrawGameplayFirstPerson also draws the right forearm (sFirstPersonForearmDLs).
    if (limbIndex == PLAYER_LIMB_RIGHT_FOREARM) {
        *dList = (Gfx*)gLinkAdultLeftArmOutNearDL;
    } else if ((limbIndex == PLAYER_LIMB_RIGHT_HAND) && MergeShip_OoTEquip_AdultHoldsHeroBow(player)) {
        // BUG-0059: MM's Hero's Bow aimed in first person; BUG-0102: laid on Ocarina of Time's bow (Hyrule's first
        // person draws the same, MergeShipMMEquipmentDraw.inc).
        Gfx* p = GRAPH_ALLOC(play->state.gfxCtx, 5 * sizeof(Gfx));

        *dList = p;
        gSPDisplayList(p++, (Gfx*)gLinkAdultRightHandClosedNearDL);
        gSPMatrix(p++, MergeShip_AdultLink_HeroBowFPMtx(play), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        gSPDisplayList(p++, (Gfx*)gLinkHumanBowDL);
        gSPPopMatrix(p++, G_MTX_MODELVIEW);
        gSPEndDisplayList(p++);
    } else if ((limbIndex == PLAYER_LIMB_RIGHT_HAND) && (player->heldItemAction == PLAYER_IA_HOOKSHOT) &&
               Player_IsHoldingHookshot(player)) {
        // Majora's Mask's own Hookshot aimed in first person (the row swap above gave the human's row of
        // sPlayerFirstPersonRightHandHookshotDLs Ocarina of Time's hookshot, right only for the ported Longshot).
        // MM's own first-person model, object_link_child_DL_017B40 (MM sPlayerFirstPersonRightHandHookshotDLs): its
        // rounder first-person body, crown and handle, in the same place as gLinkHumanHookshotDL's, then young Link's
        // hand (from its texture object_link_child_Tex_019240 on), which is cut off -- in the adult's own closed fist
        // under the Hookshot fit, so MM's Hookshot looks in first person as it does in MM (the user, 2026-09-30), as
        // Hyrule's MergeShip_MMEquip_OverrideLimbFirstPerson draws it on the adult.
        Gfx* firstPerson =
            MergeShip_OoTEquip_CutDLBeforeTexture(object_link_child_DL_017B40, "object_link_child_Tex_019240");

        *dList = MergeShip_AdultLink_Compose(play, gLinkAdultRightHandClosedNearDL,
                                             (firstPerson != NULL) ? (const void*)firstPerson : gLinkHumanHookshotDL,
                                             MS_ADULT_FIT_HOOKSHOT);
    }
}

// BUG-0059: a point of MM's young-Link right-hand lists (MM D_801C0D98, the Hero's Bow's string) under the adult's
// right-hand fit, and the fit's scale.
f32 MergeShip_AdultLink_RightFitPoint(Vec3f* mmPoint, Vec3f* out) {
    f32 v[4];

    MergeShip_AdultLink_FitValues(MS_ADULT_FIT_RIGHT, v);
    out->x = (mmPoint->x * v[3]) + v[0];
    out->y = (mmPoint->y * v[3]) + v[1];
    out->z = (mmPoint->z * v[3]) + v[2];
    return v[3];
}

// MM D_801C0DD8 (the hand's hookshot point) and D_801C0DE4 (the hook), made for MM's Hookshot in young Link's hand: on
// the adult MM's Hookshot is fitted (MS_ADULT_FIT_HOOKSHOT), so its points are too -- as Hyrule's
// MergeShip_MMEquip_HookshotOffset. Anything else: `mmPoint` as it is.
Vec3f* MergeShip_AdultLink_HookshotPoint(Player* player, Vec3f* mmPoint) {
    static Vec3f sFitted;
    f32 v[4];

    if (!MergeShip_AdultLink_IsActive(player) || (player->heldItemAction != PLAYER_IA_HOOKSHOT)) {
        return mmPoint;
    }
    MergeShip_AdultLink_FitValues(MS_ADULT_FIT_HOOKSHOT, v);
    sFitted.x = (mmPoint->x * v[3]) + v[0];
    sFitted.y = (mmPoint->y * v[3]) + v[1];
    sFitted.z = (mmPoint->z * v[3]) + v[2];
    return &sFitted;
}

// ArmsHook_Draw: MM's hook (object_link_child_DL_01D960) at the adult's fit of MM's Hookshot (Hyrule's
// MergeShip_MMEquip_HookshotTipScale); 1 otherwise.
f32 MergeShip_AdultLink_HookshotTipScale(Player* player) {
    f32 v[4];

    if (!MergeShip_AdultLink_IsActive(player) || (player->heldItemAction != PLAYER_IA_HOOKSHOT)) {
        return 1.0f;
    }
    MergeShip_AdultLink_FitValues(MS_ADULT_FIT_HOOKSHOT, v);
    return v[3];
}

Gfx* MergeShip_AdultLink_BackShieldDL(PlayState* play, Player* player, Gfx* dList) {
    static Gfx sNothing[] = { gsSPEndDisplayList() };

    if (!MergeShip_AdultLink_IsActive(player)) {
        return dList;
    }
    if (MergeShip_AdultLink_Shield(player) == MS_ADULT_SHIELD_MM) {
        // MM's Hero's / Mirror Shield on the back (gPlayerShields), fitted as young Link's scabbard is.
        return MergeShip_AdultLink_Compose(play, NULL,
                                           (player->currentShield == PLAYER_SHIELD_MIRROR_SHIELD)
                                               ? gLinkHumanMirrorShieldWithMtxDL
                                               : gLinkHumanHerosShieldWithMtxDL,
                                           (player->currentShield == PLAYER_SHIELD_MIRROR_SHIELD)
                                               ? MS_ADULT_FIT_SHEATH_MIRROR
                                               : MS_ADULT_FIT_SHEATH_HERO);
    }
    // Ocarina of Time's shields are part of the scabbard's list (MergeShip_AdultLink_Sheath).
    return sNothing;
}

// A list that loads its limb's matrix from segment 0x0D (OoT draws the boots and gauntlet plates after the skeleton),
// drawn on this limb through segment 0x0E (MergeShip_OoTEquip_RewriteDL's MS_OOTEQ_DL_REMAP_0D).
static void MergeShip_AdultLink_DrawOnLimb(PlayState* play, const char* dl, const Color_RGB8* env) {
    OPEN_DISPS(play->state.gfxCtx);

    // Its own interpolation child, keyed by the list: drawn on some frames only (the boots and plates the adult has),
    // its recorded matrix would otherwise shift every later limb's (the Hyrule side's MS_INTERP_PUSH, for the same
    // reason).
    FrameInterpolation_RecordOpenChild(dl, 0);
    gSPSegment(POLY_OPA_DISP++, 0x0E, Matrix_Finalize(play->state.gfxCtx));
    FrameInterpolation_RecordCloseChild();
    if (env != NULL) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, env->r, env->g, env->b, 0);
    }
    gSPDisplayList(POLY_OPA_DISP++, MergeShip_OoTEquip_RewriteDL(dl, MS_OOTEQ_DL_REMAP_0D));
    // Segment 0x0E is MM's master display list (graph.c); later draws in this frame call into it.
    gSPSegment(POLY_OPA_DISP++, 0x0E, gGfxMasterDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void MergeShip_AdultLink_PostLimbDraw(PlayState* play, Player* player, s32 limbIndex) {
    const Color_RGB8* gauntlet;
    s32 strength;

    if (!MergeShip_AdultLink_IsActive(player)) {
        return;
    }
    if ((limbIndex == PLAYER_LIMB_LEFT_HAND) || (limbIndex == PLAYER_LIMB_RIGHT_HAND)) {
        Matrix_MultZero(&sAdultHandPos[(limbIndex == PLAYER_LIMB_LEFT_HAND) ? 0 : 1]); // the get-item's hands
    }
    if ((player->actor.scale.y < 0.0f) || !MergeShip_OoTEquip_Enabled()) {
        return;
    }
    // OoT Player_DrawImpl (adult): the gauntlet plates for the Silver / Golden Gauntlets (no Goron's Bracelet), the
    // Iron / Hover Boots (sBootDListGroups).
    strength = OOT_EQUIP.strength;
    gauntlet = (strength >= 3) ? &sAdultGauntletColors[1] : &sAdultGauntletColors[0];
    switch (limbIndex) {
        case PLAYER_LIMB_LEFT_FOOT:
        case PLAYER_LIMB_RIGHT_FOOT:
            if ((player->currentBoots == PLAYER_BOOTS_IRON) || (player->currentBoots == PLAYER_BOOTS_HOVER)) {
                s32 left = (limbIndex == PLAYER_LIMB_LEFT_FOOT);
                const char* dl = (player->currentBoots == PLAYER_BOOTS_IRON)
                                     ? (left ? gLinkAdultLeftIronBootDL : gLinkAdultRightIronBootDL)
                                     : (left ? gLinkAdultLeftHoverBootDL : gLinkAdultRightHoverBootDL);
                MergeShip_AdultLink_DrawOnLimb(play, dl, NULL);
            }
            break;
        case PLAYER_LIMB_LEFT_FOREARM:
            if (strength >= 2) {
                MergeShip_AdultLink_DrawOnLimb(play, gLinkAdultLeftGauntletPlate1DL, gauntlet);
            }
            break;
        case PLAYER_LIMB_RIGHT_FOREARM:
            if (strength >= 2) {
                MergeShip_AdultLink_DrawOnLimb(play, gLinkAdultRightGauntletPlate1DL, gauntlet);
            }
            break;
        case PLAYER_LIMB_LEFT_HAND:
            if (strength >= 2) {
                MergeShip_AdultLink_DrawOnLimb(play,
                                               (sPlayerLeftHandType == PLAYER_MODELTYPE_LH_OPEN)
                                                   ? gLinkAdultLeftGauntletPlate2DL
                                                   : gLinkAdultLeftGauntletPlate3DL,
                                               gauntlet);
            }
            break;
        case PLAYER_LIMB_RIGHT_HAND:
            if (strength >= 2) {
                MergeShip_AdultLink_DrawOnLimb(play,
                                               (sPlayerRightHandType == PLAYER_MODELTYPE_RH_OPEN)
                                                   ? gLinkAdultRightGauntletPlate2DL
                                                   : gLinkAdultRightGauntletPlate3DL,
                                               gauntlet);
            }
            break;
        default:
            break;
    }
}

// ---- the host ---------------------------------------------------------------------------------------------------------------

void MergeShip_AdultLink_SetArrivalAge(u8 ootAge) {
    MergeShip_AdultLink_SetSaveAge((ootAge == MERGESHIP_OOT_LINK_AGE_ADULT) ? MERGESHIP_HUMAN_AGE_ADULT
                                                                           : MERGESHIP_HUMAN_AGE_YOUNG);
    MergeShipTest_Log("ADULT arrival age %d -> human age %d", ootAge, MergeShip_AdultLink_GetSaveAge());
}

// ---- Majora's Mask's masks on the adult's head (include/mergeship/MaskFit.h) ------------------------------------------

Mtx* gMergeShipFlexMtx = NULL;

static float MergeShip_AdultLink_MaskFitCVar(const char* name, float defaultValue) {
    return CVarGetFloat(name, defaultValue);
}

s32 MergeShip_AdultLink_MaskFitBegin(PlayState* play, Player* player) {
    MsMaskFit fit;
    Mtx* list;
    s32 i;

    if ((player->transformation != PLAYER_FORM_HUMAN) || !MergeShip_AdultLink_TablesAdult() ||
        (gMergeShipFlexMtx == NULL)) {
        return false;
    }
    // The Child Mask (MergeShipAdultMask.h: the Adult Mask the adult puts on, drawn as the Fierce Deity's Mask) has
    // its own fit: it is modelled for the Skull Kid's head.
    fit = MsMaskFit_Adult(MergeShip_AdultMask_DrawingChild(player) ? MS_MASK_FIT_CHILD_MASK : player->currentMask,
                          MergeShip_AdultLink_MaskFitCVar);

    OPEN_DISPS(play->state.gfxCtx);
    // The head's matrix, fitted: the masks' moving parts (ears, hair, tears) read the matrix stack; the mask itself
    // reloads its head matrix from the skeleton's list (G_MTX 0x0D0001C0, entry 7: every entry, to be safe).
    Matrix_Push();
    Matrix_Translate(fit.x, fit.y, fit.z, MTXMODE_APPLY);
    if (fit.pitch != 0.0f) {
        // A backward tilt is a pitch about world X, which is the head's local Z.
        Matrix_RotateZS((s16)fit.pitch, MTXMODE_APPLY);
    }
    Matrix_Scale(fit.sx, fit.sy, fit.sz, MTXMODE_APPLY);
    list = GRAPH_ALLOC(play->state.gfxCtx, 8 * sizeof(Mtx));
    for (i = 0; i < 8; i++) {
        Matrix_ToMtx(&list[i]);
    }
    gSPSegment(POLY_OPA_DISP++, 0x0D, list);
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

void MergeShip_AdultLink_MaskFitEnd(PlayState* play, s32 fitted) {
    if (!fitted) {
        return;
    }
    Matrix_Pop();
    OPEN_DISPS(play->state.gfxCtx);
    // The limbs drawn after the head load their matrices from the skeleton's own list.
    gSPSegment(POLY_OPA_DISP++, 0x0D, gMergeShipFlexMtx);
    CLOSE_DISPS(play->state.gfxCtx);
}
